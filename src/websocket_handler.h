#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <memory>
#include <string>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

/**
 * WebSocket handler using Boost.Beast
 *
 * Handles WebSocket connections with:
 * - Automatic upgrade from HTTP
 * - Echo server functionality
 * - Ping/pong keep-alive
 * - Graceful close handling
 */
class WebSocketHandler {
  public:
    /**
     * Handle a WebSocket session
     *
     * @param socket TCP socket to upgrade to WebSocket
     * @param http_request The HTTP upgrade request (already read)
     * @return awaitable coroutine
     */
    static asio::awaitable<void> handle_session(tcp::socket socket,
                                                const std::string& http_request);

  private:
    /**
     * Process WebSocket messages (echo server)
     *
     * @param ws WebSocket stream
     * @return awaitable coroutine
     */
    static asio::awaitable<void> echo_loop(websocket::stream<tcp::socket>& ws);
};
