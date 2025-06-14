#ifndef ASIO_SERVER_H
#define ASIO_SERVER_H

#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <memory>
#include <string>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class AsioServer {
  public:
    AsioServer(int port, int test_requests = 0);
    ~AsioServer();

    void run();
    void stop();

  private:
    // Coroutine to accept connections
    asio::awaitable<void> listener();

    // Coroutine to handle a single connection
    asio::awaitable<void> handle_connection(tcp::socket socket);

    // Coroutine to read HTTP request with timeout
    asio::awaitable<std::string> read_http_request(tcp::socket& socket, bool use_timeout = false);

    // Write response to socket
    asio::awaitable<void> write_response(tcp::socket& socket, const std::string& response);

    asio::io_context io_context_;
    tcp::acceptor acceptor_;
    asio::signal_set signals_;

    int port_;
    int test_requests_;
    std::atomic<int> request_count_{0};
    bool stopping_{false};

    static constexpr int KEEPALIVE_TIMEOUT_SEC = 5;
};

#endif // ASIO_SERVER_H