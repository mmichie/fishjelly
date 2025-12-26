#include "http2_server.h"

#ifdef HAVE_NGHTTP2

#include "connection_timeouts.h"
#include "log.h"
#include "mime.h"
#include "request_limits.h"
#include "security_middleware.h"
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace boost::asio::experimental::awaitable_operators;

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

    case NGHTTP2_RST_STREAM: {
        // HTTP/2 Rapid Reset (CVE-2023-44487) protection
        // Track RST_STREAM frames to detect rapid reset attacks
        auto now = std::chrono::steady_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(now - self->reset_window_start_);

        // Reset window if elapsed
        if (elapsed.count() >= RESET_WINDOW_SECONDS) {
            self->reset_count_ = 0;
            self->reset_window_start_ = now;
        }

        // Increment reset count
        self->reset_count_++;

        std::cerr << "RST_STREAM received on stream " << frame->hd.stream_id
                  << " (count: " << self->reset_count_ << " in " << elapsed.count() << "s)"
                  << std::endl;

        // If too many resets in window, terminate connection
        if (self->reset_count_ > MAX_RESETS_PER_WINDOW) {
            std::cerr << "HTTP/2 Rapid Reset attack detected: " << self->reset_count_
                      << " resets in " << elapsed.count() << "s - terminating connection"
                      << std::endl;

            // Terminate session with ENHANCE_YOUR_CALM error code
            nghttp2_submit_goaway(
                session, NGHTTP2_FLAG_NONE, nghttp2_session_get_last_proc_stream_id(session),
                NGHTTP2_ENHANCE_YOUR_CALM, reinterpret_cast<const uint8_t*>("Too many resets"), 15);

            // Return error to stop processing
            return NGHTTP2_ERR_CALLBACK_FAILURE;
        }
        break;
    }

    default:
        break;
    }

    return 0;
}

// Stream close callback
int Http2Session::on_stream_close_callback(nghttp2_session* session, int32_t stream_id,
                                           uint32_t error_code, void* user_data) {
    auto* self = static_cast<Http2Session*>(user_data);

    // HTTP/2 Rapid Reset (CVE-2023-44487) protection
    // Track reset frames to detect rapid reset attacks
    if (error_code != NGHTTP2_NO_ERROR) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(now - self->reset_window_start_);

        // Reset window if elapsed
        if (elapsed.count() >= RESET_WINDOW_SECONDS) {
            self->reset_count_ = 0;
            self->reset_window_start_ = now;
        }

        // Increment reset count
        self->reset_count_++;

        // If too many resets in window, terminate connection
        if (self->reset_count_ > MAX_RESETS_PER_WINDOW) {
            std::cerr << "HTTP/2 Rapid Reset attack detected: " << self->reset_count_
                      << " resets in " << elapsed.count() << "s - terminating connection"
                      << std::endl;

            // Terminate session with ENHANCE_YOUR_CALM error code
            nghttp2_submit_goaway(
                session, NGHTTP2_FLAG_NONE, nghttp2_session_get_last_proc_stream_id(session),
                NGHTTP2_ENHANCE_YOUR_CALM, reinterpret_cast<const uint8_t*>("Too many resets"), 15);

            // Clean up stream data
            self->streams_.erase(stream_id);

            // Return error to stop processing
            return NGHTTP2_ERR_CALLBACK_FAILURE;
        }
    }

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

    // Check individual header name size limit
    if (namelen > RequestLimits::MAX_HEADER_NAME_SIZE) {
        std::cerr << "Header name too large: " << namelen << " bytes" << std::endl;
        nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, stream_id, NGHTTP2_ENHANCE_YOUR_CALM);
        return 0;
    }

    // Check individual header value size limit
    if (valuelen > RequestLimits::MAX_HEADER_VALUE_SIZE) {
        std::cerr << "Header value too large: " << valuelen << " bytes" << std::endl;
        nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, stream_id, NGHTTP2_ENHANCE_YOUR_CALM);
        return 0;
    }

    // Update total header size
    stream.total_header_size += namelen + valuelen;
    if (stream.total_header_size > RequestLimits::MAX_HEADER_LIST_SIZE) {
        std::cerr << "Total header size too large: " << stream.total_header_size << " bytes"
                  << std::endl;
        nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, stream_id, NGHTTP2_ENHANCE_YOUR_CALM);
        return 0;
    }

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
        // Count regular headers (pseudo-headers don't count toward limit)
        stream.header_count++;
        if (stream.header_count > RequestLimits::MAX_HEADERS_COUNT) {
            std::cerr << "Too many headers: " << stream.header_count << std::endl;
            nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, stream_id,
                                      NGHTTP2_ENHANCE_YOUR_CALM);
            return 0;
        }

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
    (void)flags;

    auto* self = static_cast<Http2Session*>(user_data);

    // Append data to stream's request body
    auto it = self->streams_.find(stream_id);
    if (it != self->streams_.end()) {
        // Check if adding this data would exceed the body size limit
        if (it->second.request_body.size() + len > RequestLimits::MAX_BODY_SIZE) {
            std::cerr << "Request body too large: " << (it->second.request_body.size() + len)
                      << " bytes" << std::endl;
            // Reset stream with ENHANCE_YOUR_CALM error code
            nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, stream_id,
                                      NGHTTP2_ENHANCE_YOUR_CALM);
            return 0;
        }

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

        // Create nghttp2 options for HPACK bomb protection
        nghttp2_option* option;
        nghttp2_option_new(&option);

        // Limit the HPACK encoder's dynamic table size to prevent memory exhaustion
        // This protects against HPACK bomb attacks where maliciously compressed
        // headers expand to gigabytes of memory
        nghttp2_option_set_max_deflate_dynamic_table_size(option,
                                                          RequestLimits::MAX_HPACK_TABLE_SIZE);

        // Create server session with options
        nghttp2_session_server_new2(&session_, callbacks, this, option);

        nghttp2_option_del(option);
        nghttp2_session_callbacks_del(callbacks);

        // Send initial SETTINGS frame with comprehensive limits
        // These settings inform the client of our limits
        nghttp2_settings_entry settings[] = {
            // Limit concurrent streams to prevent resource exhaustion
            {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, RequestLimits::MAX_STREAMS_PER_CONN},
            // Set max frame size (RFC 7540 initial value is 16384)
            {NGHTTP2_SETTINGS_MAX_FRAME_SIZE, RequestLimits::MAX_FRAME_SIZE},
            // Set max header list size to prevent header DoS
            {NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE, RequestLimits::MAX_HEADER_LIST_SIZE},
            // Limit HPACK dynamic table size to prevent HPACK bomb attacks
            // This tells the client how large our decoder table is
            {NGHTTP2_SETTINGS_HEADER_TABLE_SIZE, RequestLimits::MAX_HPACK_TABLE_SIZE}};

        nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, settings,
                                sizeof(settings) / sizeof(settings[0]));
        nghttp2_session_send(session_);

        // Main HTTP/2 processing loop with timeout protection
        std::vector<uint8_t> buffer(8192);

        while (true) {
            // Read from socket with timeout (protects against slow HTTP/2 attacks)
            asio::steady_timer timer(socket_.get_executor());
            timer.expires_after(std::chrono::seconds(ConnectionTimeouts::READ_HEADER_TIMEOUT_SEC));

            auto result = co_await (socket_.async_read_some(asio::buffer(buffer),
                                                            asio::as_tuple(asio::use_awaitable)) ||
                                    timer.async_wait(asio::as_tuple(asio::use_awaitable)));

            if (result.index() == 1) {
                // Timeout occurred - terminate connection
                std::cerr << "HTTP/2 read timeout - possible slow attack" << std::endl;
                break;
            }

            auto [ec, bytes_read] = std::get<0>(result);
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
        // Perform SSL handshake with timeout
        asio::steady_timer handshake_timer(socket.get_executor());
        handshake_timer.expires_after(
            std::chrono::seconds(ConnectionTimeouts::SSL_HANDSHAKE_TIMEOUT_SEC));

        auto handshake_result =
            co_await (socket.async_handshake(asio::ssl::stream_base::server,
                                             asio::as_tuple(asio::use_awaitable)) ||
                      handshake_timer.async_wait(asio::as_tuple(asio::use_awaitable)));

        if (handshake_result.index() == 1) {
            // Handshake timeout
            std::cerr << "HTTP/2 SSL handshake timeout" << std::endl;
            co_return;
        }

        auto [handshake_ec] = std::get<0>(handshake_result);
        if (handshake_ec) {
            std::cerr << "HTTP/2 SSL handshake failed: " << handshake_ec.message() << std::endl;
            co_return;
        }

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
