#include "websocket_handler.h"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/http.hpp>
#include <iostream>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

asio::awaitable<void> WebSocketHandler::handle_session(tcp::socket socket,
                                                       const std::string& http_request) {
    try {
        // Create WebSocket stream from TCP socket
        websocket::stream<tcp::socket> ws(std::move(socket));

        // Set timeout options
        ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));

        // Set a decorator to change the Server header
        ws.set_option(websocket::stream_base::decorator([](websocket::response_type& res) {
            res.set(beast::http::field::server, "Fishjelly/0.6 WebSocket");
        }));

        // Parse the HTTP request
        beast::http::request_parser<beast::http::string_body> parser;
        beast::error_code ec;
        parser.put(asio::buffer(http_request), ec);

        if (ec) {
            std::cerr << "WebSocket handshake parse error: " << ec.message() << std::endl;
            co_return;
        }

        // Accept the WebSocket handshake using the parsed request
        co_await ws.async_accept(parser.get(), asio::use_awaitable);

        std::cout << "WebSocket connection established" << std::endl;

        // Run the echo loop
        co_await echo_loop(ws);

    } catch (const std::exception& e) {
        std::cerr << "WebSocket error: " << e.what() << std::endl;
    }
}

asio::awaitable<void> WebSocketHandler::echo_loop(websocket::stream<tcp::socket>& ws) {
    try {
        beast::flat_buffer buffer;

        while (true) {
            // Read a message
            co_await ws.async_read(buffer, asio::use_awaitable);

            // Check if connection is closing
            if (ws.is_message_done()) {
                // Echo the message back
                ws.text(ws.got_text());
                co_await ws.async_write(buffer.data(), asio::use_awaitable);
                buffer.consume(buffer.size());
            }
        }
    } catch (const beast::system_error& se) {
        // Check if it's a normal close
        if (se.code() != websocket::error::closed) {
            std::cerr << "WebSocket echo error: " << se.what() << std::endl;
        }
    }
}
