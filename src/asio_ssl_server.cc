#include "asio_ssl_server.h"
#include "asio_socket_adapter.h"
#include "http.h"
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <chrono>
#include <iostream>

using namespace boost::asio::experimental::awaitable_operators;

AsioSSLServer::AsioSSLServer(int port, SSLContext& ssl_context, int test_requests)
    : ssl_context_(ssl_context.get_context()),
      acceptor_(io_context_, tcp::endpoint(tcp::v4(), port)), signals_(io_context_, SIGINT, SIGTERM),
      port_(port), test_requests_(test_requests) {

    if (!ssl_context.is_configured()) {
        throw std::runtime_error("SSL context not properly configured. Load certificate and key first.");
    }

    std::cout << "Starting ASIO SSL server on port " << port_ << " process ID: " << getpid()
              << std::endl;
    std::cout << "  Certificate: " << ssl_context.get_cert_file() << std::endl;
    std::cout << "  Private Key: " << ssl_context.get_key_file() << std::endl;

    if (test_requests_ > 0) {
        std::cout << "Test mode: Will exit after " << test_requests_ << " requests" << std::endl;
    }

    // Set up signal handlers
    signals_.async_wait([this](std::error_code /*ec*/, int /*signo*/) { stop(); });
}

AsioSSLServer::~AsioSSLServer() { stop(); }

void AsioSSLServer::run() {
    // Start accepting connections
    asio::co_spawn(io_context_, listener(), asio::detached);

    // Run the I/O context
    io_context_.run();

    if (test_requests_ > 0) {
        std::cout << "SSL Server shutdown complete." << std::endl;
    }
}

void AsioSSLServer::stop() {
    if (!stopping_) {
        stopping_ = true;
        acceptor_.close();
        io_context_.stop();
    }
}

asio::awaitable<void> AsioSSLServer::listener() {
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

asio::awaitable<void> AsioSSLServer::handle_connection(ssl_socket socket) {
    try {
        // Get client endpoint for logging (before handshake)
        auto client_endpoint = socket.lowest_layer().remote_endpoint();

        // Perform SSL handshake with timeout
        asio::steady_timer handshake_timer(socket.get_executor());
        handshake_timer.expires_after(std::chrono::seconds(SSL_HANDSHAKE_TIMEOUT_SEC));

        auto handshake_result =
            co_await (socket.async_handshake(ssl::stream_base::server,
                                             asio::as_tuple(asio::use_awaitable)) ||
                      handshake_timer.async_wait(asio::as_tuple(asio::use_awaitable)));

        if (handshake_result.index() == 1) {
            // Handshake timeout
            std::cerr << "SSL handshake timeout from " << client_endpoint << std::endl;
            co_return;
        }

        auto [handshake_ec] = std::get<0>(handshake_result);
        if (handshake_ec) {
            std::cerr << "SSL handshake failed: " << handshake_ec.message() << std::endl;
            co_return;
        }

        // Create socket adapter (Note: We'll need to modify this for SSL)
        // For now, we'll work with the socket directly similar to AsioServer
        // TODO: Create SSL-aware socket adapter

        // Create HTTP handler
        Http http;

        // First request doesn't use timeout
        std::string header = co_await read_http_request(socket, false);
        if (header.empty()) {
            co_return;
        }

        // For SSL we need to create a custom adapter that works with SSL streams
        // For now, we'll use a simpler approach similar to the fork model
        // Parse and handle the request
        // Note: This is simplified - full integration with Http class requires
        // an SSL-aware socket adapter

        // Send a basic HTTPS response for testing
        std::string response = "HTTP/1.1 200 OK\r\n"
                              "Content-Type: text/html\r\n"
                              "Content-Length: 85\r\n"
                              "Connection: close\r\n"
                              "\r\n"
                              "<html><body><h1>HTTPS Works!</h1><p>SSL/TLS connection "
                              "established.</p></body></html>";

        co_await write_response(socket, response);

        // Gracefully shutdown SSL
        boost::system::error_code shutdown_ec;
        co_await socket.async_shutdown(asio::redirect_error(asio::use_awaitable, shutdown_ec));

        // Increment request count for test mode
        int count = ++request_count_;
        if (test_requests_ > 0 && count >= test_requests_) {
            std::cout << "Test mode: Exiting after " << count << " requests" << std::endl;
            stop();
        }

    } catch (const std::exception& e) {
        // Connection error - this is normal when clients disconnect
        // SSL errors are also common (e.g., client doesn't support TLS version)
    }
}

asio::awaitable<std::string> AsioSSLServer::read_http_request(ssl_socket& socket, bool use_timeout) {
    try {
        std::string request;
        asio::streambuf buffer;

        if (use_timeout) {
            // Set up timeout
            asio::steady_timer timer(socket.get_executor());
            timer.expires_after(std::chrono::seconds(KEEPALIVE_TIMEOUT_SEC));

            // Race between read and timeout
            auto result = co_await (asio::async_read_until(socket, buffer, "\r\n\r\n",
                                                           asio::as_tuple(asio::use_awaitable)) ||
                                    timer.async_wait(asio::as_tuple(asio::use_awaitable)));

            if (result.index() == 1) {
                // Timeout occurred
                co_return "";
            }

            auto [ec, bytes_transferred] = std::get<0>(result);
            if (ec || bytes_transferred == 0) {
                co_return "";
            }
        } else {
            // No timeout for first request
            auto [ec, bytes_transferred] =
                co_await asio::async_read_until(socket, buffer, "\r\n\r\n",
                                                asio::as_tuple(asio::use_awaitable));
            if (ec || bytes_transferred == 0) {
                co_return "";
            }
        }

        // Convert buffer to string
        std::istream is(&buffer);
        std::string line;
        while (std::getline(is, line) && line != "\r") {
            request += line + "\n";
        }
        request += "\n"; // Add final newline

        co_return request;

    } catch (const std::exception& e) {
        co_return "";
    }
}

asio::awaitable<void> AsioSSLServer::write_response(ssl_socket& socket, const std::string& response) {
    try {
        co_await asio::async_write(socket, asio::buffer(response), asio::use_awaitable);
    } catch (const std::exception& e) {
        // Write error - client may have disconnected
    }
}
