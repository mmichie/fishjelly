#include "asio_http_connection.h"
#include <iostream>

AsioHttpConnection::AsioHttpConnection(tcp::socket socket)
    : socket_(std::move(socket)), response_buffer_{} {
}

asio::awaitable<bool> AsioHttpConnection::process_request(const std::string& header) {
    // Clear response buffer
    response_buffer_.str("");
    response_buffer_.clear();
    
    // Create a custom Socket wrapper that writes to our buffer instead of a real socket
    // For now, we'll need to refactor the HTTP handler to support this
    // TODO: Refactor Http class to use an abstract output interface
    
    // Process the header
    bool keep_alive = http_handler_.parseHeader(header);
    
    // Send the accumulated response
    co_await send_response();
    
    co_return keep_alive;
}

asio::awaitable<void> AsioHttpConnection::send_response() {
    std::string response = response_buffer_.str();
    if (!response.empty()) {
        co_await asio::async_write(socket_, asio::buffer(response), asio::use_awaitable);
    }
}

std::string AsioHttpConnection::get_client_address() const {
    try {
        auto endpoint = socket_.remote_endpoint();
        return endpoint.address().to_string();
    } catch (...) {
        return "unknown";
    }
}

void AsioHttpConnection::capture_http_output(const std::string& data) {
    response_buffer_ << data;
}