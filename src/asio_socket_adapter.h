#ifndef ASIO_SOCKET_ADAPTER_H
#define ASIO_SOCKET_ADAPTER_H

#include "socket.h"
#include <boost/asio.hpp>
#include <sstream>
#include <memory>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// Adapter class that makes ASIO socket look like the old Socket interface
class AsioSocketAdapter : public Socket {
public:
    AsioSocketAdapter(tcp::socket* asio_socket, const tcp::endpoint& client_endpoint);
    
    // Override Socket methods to write to string buffer instead
    void write_line(std::string_view line) override;
    bool read_line(std::string* buffer) override;
    ssize_t read_raw(char* buffer, size_t size) override;
    int write_raw(const char* data, size_t size) override;
    
    // Get accumulated response
    std::string getResponse() const { return response_buffer_.str(); }
    
    // Set request data for reading
    void setRequestData(const std::string& data) { 
        request_data_ = data; 
        request_pos_ = 0;
    }
    
    // Add raw data to response (for sendFile)
    void write_rawData(const char* data, size_t size) {
        response_buffer_.write(data, size);
    }
    
private:
    tcp::socket* asio_socket_;  // Not owned
    std::stringstream response_buffer_;
    std::string request_data_;
    size_t request_pos_ = 0;
    
    // Disable copy/move since we don't own the socket
    AsioSocketAdapter(const AsioSocketAdapter&) = delete;
    AsioSocketAdapter& operator=(const AsioSocketAdapter&) = delete;
};

#endif // ASIO_SOCKET_ADAPTER_H