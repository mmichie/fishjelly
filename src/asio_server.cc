#include "asio_server.h"
#include "asio_socket_adapter.h"
#include "http.h"
#include "websocket_handler.h"
#include <algorithm>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <chrono>
#include <iostream>

using namespace boost::asio::experimental::awaitable_operators;

AsioServer::AsioServer(int port, int test_requests)
    : acceptor_(io_context_, tcp::endpoint(tcp::v4(), port)),
      signals_(io_context_, SIGINT, SIGTERM), port_(port), test_requests_(test_requests) {
    std::cout << "Starting ASIO server on port " << port_ << " process ID: " << getpid()
              << std::endl;

    if (test_requests_ > 0) {
        std::cout << "Test mode: Will exit after " << test_requests_ << " requests" << std::endl;
    }

    // Set up signal handlers
    signals_.async_wait([this](std::error_code /*ec*/, int /*signo*/) { stop(); });
}

AsioServer::~AsioServer() { stop(); }

void AsioServer::run() {
    // Start accepting connections
    asio::co_spawn(io_context_, listener(), asio::detached);

    // Run the I/O context
    io_context_.run();

    if (test_requests_ > 0) {
        std::cout << "Server shutdown complete." << std::endl;
    }
}

void AsioServer::stop() {
    if (!stopping_) {
        stopping_ = true;
        acceptor_.close();
        io_context_.stop();
    }
}

asio::awaitable<void> AsioServer::listener() {
    try {
        while (!stopping_) {
            auto socket = co_await acceptor_.async_accept(asio::use_awaitable);

            // Handle each connection concurrently
            asio::co_spawn(io_context_, handle_connection(std::move(socket)), asio::detached);
        }
    } catch (const std::exception& e) {
        if (!stopping_) {
            std::cerr << "Accept error: " << e.what() << std::endl;
        }
    }
}

asio::awaitable<void> AsioServer::handle_connection(tcp::socket socket) {
    try {
        // Get client endpoint for logging
        auto client_endpoint = socket.remote_endpoint();

        // First request doesn't use timeout
        std::string header = co_await read_http_request(socket, false);
        if (header.empty()) {
            co_return;
        }

        // Check if this is a WebSocket upgrade request
        if (is_websocket_upgrade(header)) {
            std::cout << "WebSocket upgrade detected from " << client_endpoint << std::endl;
            co_await WebSocketHandler::handle_session(std::move(socket), header);
            co_return;
        }

        // Process as regular HTTP
        // Create socket adapter
        AsioSocketAdapter socket_adapter(&socket, client_endpoint);

        // Create HTTP handler with the adapter
        Http http;
        http.sock = std::unique_ptr<Socket>(&socket_adapter);

        // Provide the request data to the adapter
        socket_adapter.setRequestData(header);

        // Process the first request
        bool keep_alive = http.parseHeader(header);

        // Send the response
        std::string response = socket_adapter.getResponse();
        if (!response.empty()) {
            co_await write_response(socket, response);
        }

        // Handle keep-alive
        while (keep_alive && !stopping_) {
            // Create new adapter for next request
            AsioSocketAdapter next_adapter(&socket, client_endpoint);
            http.sock.release(); // Release the previous adapter
            http.sock = std::unique_ptr<Socket>(&next_adapter);

            // Subsequent requests use timeout
            header = co_await read_http_request(socket, true);
            if (header.empty()) {
                http.sock.release(); // Don't delete stack object
                break;               // Timeout or connection closed
            }

            // Provide the request data to the adapter
            next_adapter.setRequestData(header);

            keep_alive = http.parseHeader(header);

            // Send the response
            response = next_adapter.getResponse();
            if (!response.empty()) {
                co_await write_response(socket, response);
            }
        }

        // Don't delete the adapter if we didn't break out of the loop
        if (http.sock) {
            http.sock.release();
        }

        // Increment request count for test mode
        int count = ++request_count_;
        if (test_requests_ > 0 && count >= test_requests_) {
            std::cout << "Test mode: Exiting after " << count << " requests" << std::endl;
            stop();
        }

    } catch (const std::exception& e) {
        // Connection error - this is normal when clients disconnect
    }
}

asio::awaitable<std::string> AsioServer::read_http_request(tcp::socket& socket, bool use_timeout) {
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
                socket.cancel();
                co_return "";
            }

            // Check for read error
            auto [ec, bytes] = std::get<0>(result);
            if (ec) {
                co_return "";
            }
        } else {
            // No timeout
            co_await asio::async_read_until(socket, buffer, "\r\n\r\n", asio::use_awaitable);
        }

        // Convert buffer to string
        std::istream stream(&buffer);
        std::string line;
        while (std::getline(stream, line)) {
            request += line + "\n";
            if (line == "\r" || line.empty()) {
                break;
            }
        }

        co_return request;

    } catch (const std::exception& e) {
        co_return "";
    }
}

asio::awaitable<void> AsioServer::write_response(tcp::socket& socket, const std::string& response) {
    try {
        co_await asio::async_write(socket, asio::buffer(response), asio::use_awaitable);
    } catch (const std::exception& e) {
        // Write error - client may have disconnected
    }
}

bool AsioServer::is_websocket_upgrade(const std::string& header) {
    // Convert header to lowercase for case-insensitive comparison
    std::string lower_header = header;
    std::transform(lower_header.begin(), lower_header.end(), lower_header.begin(), ::tolower);

    // Check for "upgrade: websocket" and "connection: upgrade"
    return (lower_header.find("upgrade: websocket") != std::string::npos &&
            lower_header.find("connection:") != std::string::npos &&
            lower_header.find("upgrade") != std::string::npos);
}