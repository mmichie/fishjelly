#ifndef ASIO_HTTP_CONNECTION_H
#define ASIO_HTTP_CONNECTION_H

#include "http.h"
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <memory>
#include <sstream>
#include <string>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class AsioHttpConnection : public std::enable_shared_from_this<AsioHttpConnection> {
  public:
    explicit AsioHttpConnection(tcp::socket socket);

    // Process a single HTTP request/response cycle
    asio::awaitable<bool> process_request(const std::string& header);

    // Get client address for logging
    std::string get_client_address() const;

  private:
    tcp::socket socket_;
    Http http_handler_;
    std::stringstream response_buffer_;

    // Bridge functions to connect HTTP handler to ASIO socket
    asio::awaitable<void> send_response();
    void capture_http_output(const std::string& data);
};

#endif // ASIO_HTTP_CONNECTION_H