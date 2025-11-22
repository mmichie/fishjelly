#include "http2_server.h"

#ifdef HAVE_NGHTTP2

#include "log.h"
#include "mime.h"
#include "security_middleware.h"
#include <boost/asio/use_awaitable.hpp>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

// ============================================================================
// Http2Session Implementation
// ============================================================================

Http2Session::Http2Session(ssl_socket socket) : socket_(std::move(socket)), session_(nullptr) {}

Http2Session::~Http2Session() {
    if (session_) {
        nghttp2_session_del(session_);
    }
}

// Send callback - called by nghttp2 to send data
ssize_t Http2Session::send_callback(nghttp2_session* session, const uint8_t* data, size_t length,
                                    int flags, void* user_data) {
    (void)session;
    (void)flags;

    auto* self = static_cast<Http2Session*>(user_data);

    try {
        boost::system::error_code ec;
        size_t written = asio::write(self->socket_, asio::buffer(data, length), ec);

        if (ec) {
            return NGHTTP2_ERR_CALLBACK_FAILURE;
        }

        return written;
    } catch (...) {
        return NGHTTP2_ERR_CALLBACK_FAILURE;
    }
}

// Frame received callback
int Http2Session::on_frame_recv_callback(nghttp2_session* session, const nghttp2_frame* frame,
                                         void* user_data) {
    (void)session;
    auto* self = static_cast<Http2Session*>(user_data);

    switch (frame->hd.type) {
    case NGHTTP2_HEADERS:
        // If this is end of headers, process the request
        if (frame->hd.flags & NGHTTP2_FLAG_END_HEADERS) {
            self->process_request(frame->hd.stream_id);
        }
        break;

    case NGHTTP2_DATA:
        // Data frame received
        break;

    default:
        break;
    }

    return 0;
}

// Stream close callback
int Http2Session::on_stream_close_callback(nghttp2_session* session, int32_t stream_id,
                                           uint32_t error_code, void* user_data) {
    (void)session;
    (void)error_code;

    auto* self = static_cast<Http2Session*>(user_data);

    // Clean up stream data
    self->streams_.erase(stream_id);

    return 0;
}

// Header callback - called for each header field
int Http2Session::on_header_callback(nghttp2_session* session, const nghttp2_frame* frame,
                                     const uint8_t* name, size_t namelen, const uint8_t* value,
                                     size_t valuelen, uint8_t flags, void* user_data) {
    (void)session;
    (void)flags;

    auto* self = static_cast<Http2Session*>(user_data);

    if (frame->hd.type != NGHTTP2_HEADERS || frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
        return 0;
    }

    int32_t stream_id = frame->hd.stream_id;
    std::string header_name(reinterpret_cast<const char*>(name), namelen);
    std::string header_value(reinterpret_cast<const char*>(value), valuelen);

    auto& stream = self->streams_[stream_id];

    // Handle HTTP/2 pseudo-headers
    if (header_name == ":method") {
        stream.method = header_value;
    } else if (header_name == ":path") {
        stream.path = header_value;
    } else if (header_name == ":authority") {
        stream.authority = header_value;
    } else if (header_name == ":scheme") {
        stream.scheme = header_value;
    } else {
        // Regular header
        stream.headers[header_name] = header_value;
    }

    return 0;
}

// Begin headers callback
int Http2Session::on_begin_headers_callback(nghttp2_session* session, const nghttp2_frame* frame,
                                            void* user_data) {
    (void)session;

    auto* self = static_cast<Http2Session*>(user_data);

    if (frame->hd.type != NGHTTP2_HEADERS || frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
        return 0;
    }

    // Initialize stream data
    int32_t stream_id = frame->hd.stream_id;
    self->streams_[stream_id] = StreamData{};

    return 0;
}

// Data chunk received callback
int Http2Session::on_data_chunk_recv_callback(nghttp2_session* session, uint8_t flags,
                                              int32_t stream_id, const uint8_t* data, size_t len,
                                              void* user_data) {
    (void)session;
    (void)flags;

    auto* self = static_cast<Http2Session*>(user_data);

    // Append data to stream's request body
    auto it = self->streams_.find(stream_id);
    if (it != self->streams_.end()) {
        it->second.request_body.insert(it->second.request_body.end(), data, data + len);
    }

    return 0;
}

void Http2Session::process_request(int32_t stream_id) {
    auto it = streams_.find(stream_id);
    if (it == streams_.end()) {
        send_error(stream_id, 500, "Internal Server Error");
        return;
    }

    const auto& stream = it->second;

    std::string path = stream.path;

    // Default to index.html for directory requests
    if (path.empty() || path == "/") {
        path = "/index.html";
    }

    // Security: Sanitize path to prevent directory traversal
    // This handles URL encoding, null bytes, and ensures path stays within htdocs
    std::string file_path =
        SecurityMiddleware::sanitize_path(path, std::filesystem::path("htdocs"));
    if (file_path.empty()) {
        send_error(stream_id, 400, "Bad Request - Invalid Path");
        return;
    }

    // Check if file exists
    if (!std::filesystem::exists(file_path)) {
        send_error(stream_id, 404, "Not Found");
        return;
    }

    // Check if it's a directory
    if (std::filesystem::is_directory(file_path)) {
        std::string index_path = file_path + "/index.html";
        if (std::filesystem::exists(index_path)) {
            file_path = index_path;
        } else {
            send_error(stream_id, 403, "Directory listing not allowed");
            return;
        }
    }

    // Get MIME type
    Mime& mime = Mime::getInstance();
    std::string content_type = mime.getMimeFromExtension(file_path);

    // Read file
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        send_error(stream_id, 500, "Failed to read file");
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Send response
    send_response(stream_id, 200, content_type, content);

    // Log access
    try {
        auto client_ip = socket_.lowest_layer().remote_endpoint().address().to_string();
        std::string request = stream.method + " " + stream.path + " HTTP/2";
        Log& logger = Log::getInstance();
        logger.writeLogLine(client_ip, request, 200, content.size(), "-", "-");
    } catch (...) {
        // Ignore logging errors
    }
}

void Http2Session::send_response(int32_t stream_id, int status, const std::string& content_type,
                                 const std::string& body) {
    // Store response body in stream data
    auto it = streams_.find(stream_id);
    if (it == streams_.end()) {
        return;
    }
    it->second.response_body = body;
    it->second.bytes_sent = 0;

    // Prepare response headers with persistent strings
    static const char status_200[] = "200";
    static const char status_404[] = "404";
    static const char status_403[] = "403";
    static const char status_500[] = "500";

    const char* status_str = status_200;
    if (status == 404)
        status_str = status_404;
    else if (status == 403)
        status_str = status_403;
    else if (status == 500)
        status_str = status_500;

    std::vector<nghttp2_nv> nva;

    auto make_nv = [](const char* name, const char* value) -> nghttp2_nv {
        return {const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(name)),
                const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(value)), std::strlen(name),
                std::strlen(value), NGHTTP2_NV_FLAG_NONE};
    };

    nva.push_back(make_nv(":status", status_str));
    nva.push_back(make_nv("content-type", content_type.c_str()));

    // Create data provider for response body
    nghttp2_data_provider data_prd;
    data_prd.source.fd = stream_id;
    data_prd.read_callback = [](nghttp2_session* session, int32_t stream_id, uint8_t* buf,
                                size_t length, uint32_t* data_flags, nghttp2_data_source* source,
                                void* user_data) -> ssize_t {
        (void)session;
        (void)source;

        auto* self = static_cast<Http2Session*>(user_data);
        auto it = self->streams_.find(stream_id);
        if (it == self->streams_.end()) {
            return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
        }

        const auto& response_body = it->second.response_body;
        size_t& bytes_sent = it->second.bytes_sent;

        size_t remaining = response_body.size() - bytes_sent;
        size_t to_copy = std::min(length, remaining);

        if (to_copy > 0) {
            std::memcpy(buf, response_body.data() + bytes_sent, to_copy);
            bytes_sent += to_copy;
        }

        if (bytes_sent >= response_body.size()) {
            *data_flags |= NGHTTP2_DATA_FLAG_EOF;
        }

        return to_copy;
    };

    // Submit response
    nghttp2_submit_response(session_, stream_id, nva.data(), nva.size(), &data_prd);
    nghttp2_session_send(session_);
}

void Http2Session::send_error(int32_t stream_id, int status, const std::string& message) {
    std::string body =
        "<html><body><h1>" + std::to_string(status) + " " + message + "</h1></body></html>";
    send_response(stream_id, status, "text/html", body);
}

asio::awaitable<void> Http2Session::start() {
    try {
        // Initialize nghttp2 session callbacks
        nghttp2_session_callbacks* callbacks;
        nghttp2_session_callbacks_new(&callbacks);

        nghttp2_session_callbacks_set_send_callback(callbacks, send_callback);
        nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, on_frame_recv_callback);
        nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, on_stream_close_callback);
        nghttp2_session_callbacks_set_on_header_callback(callbacks, on_header_callback);
        nghttp2_session_callbacks_set_on_begin_headers_callback(callbacks,
                                                                on_begin_headers_callback);
        nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks,
                                                                  on_data_chunk_recv_callback);

        // Create server session
        nghttp2_session_server_new(&session_, callbacks, this);

        nghttp2_session_callbacks_del(callbacks);

        // Send initial SETTINGS frame
        nghttp2_settings_entry settings[] = {{NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100}};

        nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, settings,
                                sizeof(settings) / sizeof(settings[0]));
        nghttp2_session_send(session_);

        // Main HTTP/2 processing loop
        std::vector<uint8_t> buffer(8192);

        while (true) {
            // Read from socket
            auto [ec, bytes_read] = co_await socket_.async_read_some(
                asio::buffer(buffer), asio::as_tuple(asio::use_awaitable));

            if (ec) {
                break;
            }

            // Process received data
            ssize_t read_len = nghttp2_session_mem_recv(session_, buffer.data(), bytes_read);

            if (read_len < 0) {
                std::cerr << "nghttp2_session_mem_recv error: " << nghttp2_strerror(read_len)
                          << std::endl;
                break;
            }

            // Send any pending data
            int rv = nghttp2_session_send(session_);
            if (rv != 0) {
                std::cerr << "nghttp2_session_send error: " << nghttp2_strerror(rv) << std::endl;
                break;
            }

            // Check if session wants to terminate
            if (nghttp2_session_want_read(session_) == 0 &&
                nghttp2_session_want_write(session_) == 0) {
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "HTTP/2 session error: " << e.what() << std::endl;
    }

    co_return;
}

// ============================================================================
// Http2Server Implementation
// ============================================================================

Http2Server::Http2Server(int port, bool use_tls, const std::string& cert_path,
                         const std::string& key_path)
    : ssl_context_(asio::ssl::context::tlsv12_server),
      acceptor_(io_context_, tcp::endpoint(tcp::v4(), port)),
      signals_(io_context_, SIGINT, SIGTERM), port_(port), use_tls_(use_tls), cert_path_(cert_path),
      key_path_(key_path) {
    std::cout << "Starting HTTP/2 server on port " << port_ << std::endl;

    if (use_tls_) {
        // Configure SSL context
        ssl_context_.use_certificate_chain_file(cert_path_);
        ssl_context_.use_private_key_file(key_path_, asio::ssl::context::pem);

        // Configure ALPN to advertise "h2"
        SSL_CTX_set_alpn_select_cb(
            ssl_context_.native_handle(),
            [](SSL* ssl, const unsigned char** out, unsigned char* outlen, const unsigned char* in,
               unsigned int inlen, void* arg) -> int {
                (void)ssl;
                (void)arg;

                // Look for "h2" in client's ALPN list
                const unsigned char h2[] = {2, 'h', '2'};

                for (unsigned int i = 0; i < inlen;) {
                    unsigned char len = in[i];
                    if (i + 1 + len > inlen)
                        break;

                    if (len == 2 && std::memcmp(&in[i], h2, 3) == 0) {
                        *out = &in[i + 1];
                        *outlen = len;
                        return SSL_TLSEXT_ERR_OK;
                    }

                    i += 1 + len;
                }

                return SSL_TLSEXT_ERR_NOACK;
            },
            nullptr);
    }

    // Set up signal handlers
    signals_.async_wait([this](const boost::system::error_code& /*ec*/, int /*signo*/) { stop(); });
}

Http2Server::~Http2Server() { stop(); }

void Http2Server::run() {
    // Start accepting connections
    asio::co_spawn(io_context_, listener(), asio::detached);

    // Run the I/O context
    io_context_.run();

    std::cout << "HTTP/2 server shutdown complete." << std::endl;
}

void Http2Server::stop() {
    if (!stopping_.exchange(true)) {
        acceptor_.close();
        io_context_.stop();
    }
}

asio::awaitable<void> Http2Server::listener() {
    try {
        while (!stopping_) {
            // Accept TCP connection
            auto socket = co_await acceptor_.async_accept(asio::use_awaitable);

            // Wrap in SSL stream
            ssl_socket ssl_sock(std::move(socket), ssl_context_);

            // Handle each connection concurrently
            asio::co_spawn(io_context_, handle_connection(std::move(ssl_sock)), asio::detached);
        }
    } catch (const std::exception& e) {
        if (!stopping_) {
            std::cerr << "Accept error: " << e.what() << std::endl;
        }
    }
}

asio::awaitable<void> Http2Server::handle_connection(ssl_socket socket) {
    try {
        // Perform SSL handshake
        co_await socket.async_handshake(asio::ssl::stream_base::server, asio::use_awaitable);

        // Create HTTP/2 session
        auto session = std::make_shared<Http2Session>(std::move(socket));

        // Start HTTP/2 processing
        co_await session->start();

    } catch (const std::exception& e) {
        std::cerr << "Connection error: " << e.what() << std::endl;
    }

    co_return;
}

#endif // HAVE_NGHTTP2
