#ifndef ASIO_SSL_SERVER_H
#define ASIO_SSL_SERVER_H

#include "connection_timeouts.h"
#include "ssl_context.h"
#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <memory>
#include <string>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using ssl_socket = boost::asio::ssl::stream<tcp::socket>;

/**
 * ASIO-based HTTPS server with SSL/TLS support
 * Uses coroutines for async I/O and SSL streams for encryption
 */
class AsioSSLServer {
  public:
    /**
     * Constructor
     * @param port Port to listen on (typically 443)
     * @param ssl_context SSL context with loaded certificates
     * @param test_requests Number of requests before shutdown (0 = run forever)
     */
    AsioSSLServer(int port, SSLContext& ssl_context, int test_requests = 0);
    ~AsioSSLServer();

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

    // Coroutine to handle a single SSL connection
    asio::awaitable<void> handle_connection(ssl_socket socket);

    // Coroutine to read HTTP request with timeout (over SSL)
    asio::awaitable<std::string> read_http_request(ssl_socket& socket, bool use_timeout = false);

    // Write response to SSL socket
    asio::awaitable<void> write_response(ssl_socket& socket, const std::string& response);

    // Write response with timeout protection (for slow read attack prevention)
    asio::awaitable<bool> write_response_with_timeout(ssl_socket& socket,
                                                      const std::string& response);

    asio::io_context io_context_;
    ssl::context& ssl_context_;
    tcp::acceptor acceptor_;
    asio::signal_set signals_;

    int port_;
    int test_requests_;
    std::atomic<int> request_count_{0};
    bool stopping_{false};
};

#endif // ASIO_SSL_SERVER_H
