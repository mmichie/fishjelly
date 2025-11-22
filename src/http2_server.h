#ifndef HTTP2_SERVER_H
#define HTTP2_SERVER_H

#ifdef HAVE_NGHTTP2

#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/ssl.hpp>
#include <map>
#include <memory>
#include <nghttp2/nghttp2.h>
#include <string>
#include <vector>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using ssl_socket = asio::ssl::stream<tcp::socket>;

/**
 * HTTP/2 connection session
 * Manages a single HTTP/2 connection with multiple streams
 */
class Http2Session {
  public:
    Http2Session(ssl_socket socket);
    ~Http2Session();

    asio::awaitable<void> start();

  private:
    // nghttp2 callbacks
    static ssize_t send_callback(nghttp2_session* session, const uint8_t* data, size_t length,
                                 int flags, void* user_data);

    static int on_frame_recv_callback(nghttp2_session* session, const nghttp2_frame* frame,
                                      void* user_data);

    static int on_stream_close_callback(nghttp2_session* session, int32_t stream_id,
                                        uint32_t error_code, void* user_data);

    static int on_header_callback(nghttp2_session* session, const nghttp2_frame* frame,
                                  const uint8_t* name, size_t namelen, const uint8_t* value,
                                  size_t valuelen, uint8_t flags, void* user_data);

    static int on_begin_headers_callback(nghttp2_session* session, const nghttp2_frame* frame,
                                         void* user_data);

    static int on_data_chunk_recv_callback(nghttp2_session* session, uint8_t flags,
                                           int32_t stream_id, const uint8_t* data, size_t len,
                                           void* user_data);

    // Process a stream request
    void process_request(int32_t stream_id);

    // Send response
    void send_response(int32_t stream_id, int status, const std::string& content_type,
                       const std::string& body);

    // Send error response
    void send_error(int32_t stream_id, int status, const std::string& message);

    ssl_socket socket_;
    nghttp2_session* session_;

    // Stream data
    struct StreamData {
        std::string method;
        std::string path;
        std::string authority;
        std::string scheme;
        std::map<std::string, std::string> headers;
        std::vector<uint8_t> request_body;
        std::string response_body;    // Store response body
        size_t bytes_sent = 0;        // Track bytes sent
        size_t header_count = 0;      // Track number of headers
        size_t total_header_size = 0; // Track total header size (names + values)
    };

    std::map<int32_t, StreamData> streams_;
};

/**
 * HTTP/2 server using nghttp2 C library
 * Provides HTTP/2 support with multiplexing and HPACK compression
 */
class Http2Server {
  public:
    /**
     * Constructor for HTTP/2 server
     * @param port Port to listen on (typically 443 for HTTPS)
     * @param use_tls Whether to use TLS (required for HTTP/2 over TLS)
     * @param cert_path Path to SSL certificate file (required if use_tls=true)
     * @param key_path Path to SSL private key file (required if use_tls=true)
     */
    Http2Server(int port, bool use_tls = true, const std::string& cert_path = "",
                const std::string& key_path = "");

    ~Http2Server();

    /**
     * Run the server (blocks until stopped)
     */
    void run();

    /**
     * Stop the server gracefully
     */
    void stop();

  private:
    // Coroutine to accept connections
    asio::awaitable<void> listener();

    // Handle a single HTTP/2 connection
    asio::awaitable<void> handle_connection(ssl_socket socket);

    asio::io_context io_context_;
    asio::ssl::context ssl_context_;
    tcp::acceptor acceptor_;
    asio::signal_set signals_;

    int port_;
    bool use_tls_;
    std::string cert_path_;
    std::string key_path_;
    std::atomic<bool> stopping_{false};
};

#endif // HAVE_NGHTTP2

#endif // HTTP2_SERVER_H
