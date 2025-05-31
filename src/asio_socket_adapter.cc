#include "asio_socket_adapter.h"
#include <iostream>
#include <cstring>

AsioSocketAdapter::AsioSocketAdapter(tcp::socket* asio_socket, const tcp::endpoint& client_endpoint)
    : Socket(0)  // Pass dummy port to base class
    , asio_socket_(asio_socket) {
    
    (void)asio_socket_; // Currently unused but kept for future use
    
    // Set up client address for logging
    client = {};
    client.sin_family = AF_INET;
    client.sin_port = htons(client_endpoint.port());
    
    // Convert boost address to sockaddr_in
    if (client_endpoint.address().is_v4()) {
        auto addr = client_endpoint.address().to_v4();
        auto bytes = addr.to_bytes();
        memcpy(&client.sin_addr.s_addr, bytes.data(), 4);
    }
    
    // Set accept_fd_ to a dummy value to indicate socket is "open"
    accept_fd_ = 1;
}

void AsioSocketAdapter::write_line(std::string_view line) {
    response_buffer_ << line;
    // The base class adds newline, but we want to preserve exact output
    if (!line.empty() && line.back() != '\n') {
        response_buffer_ << '\n';
    }
}

bool AsioSocketAdapter::read_line(std::string* buffer) {
    if (request_pos_ >= request_data_.size()) {
        return false;
    }
    
    // Read until newline or end of data
    buffer->clear();
    while (request_pos_ < request_data_.size()) {
        char c = request_data_[request_pos_++];
        buffer->push_back(c);
        if (c == '\n') {
            break;
        }
    }
    
    return !buffer->empty();
}

int AsioSocketAdapter::write_raw(const char* data, size_t size) {
    response_buffer_.write(data, size);
    return size;  // Always successful in buffer mode
}

ssize_t AsioSocketAdapter::read_raw(char* buffer, size_t size) {
    if (request_pos_ >= request_data_.size()) {
        return 0;  // No more data
    }
    
    // Read up to 'size' bytes from request_data_
    size_t available = request_data_.size() - request_pos_;
    size_t to_read = std::min(size, available);
    
    if (to_read > 0) {
        memcpy(buffer, request_data_.data() + request_pos_, to_read);
        request_pos_ += to_read;
    }
    
    return to_read;
}