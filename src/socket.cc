// socket.cc
#include "socket.h"
#include <iostream>  // for std::cerr, std::cout
#include <stdexcept> // for std::runtime_error
#include <unistd.h>
#include <poll.h>

/**
 * SocketException class for specialized socket error handling.
 */
class SocketException : public std::runtime_error {
  public:
    explicit SocketException(const std::string& message) : std::runtime_error(message) {}
};

/**
 * Centralized error handler for the Socket class.
 */
void Socket::handle_error(std::string_view message) {
    std::cerr << message << std::endl;
    throw SocketException(std::string(message));
}

/**
 * Safely closes the socket and its associated file pointer.
 */
void Socket::close_socket() {
    if (DEBUG) {
        std::cout << "Closing socket" << std::endl;
    }

    if (socket_fp_ && fclose(socket_fp_) != 0) {
        handle_error("Failed to close socket file pointer");
    }

    if (socket_fd_ != -1) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
}

/**
 * Closes only the client connection, not the server socket.
 * Used by the parent process after forking.
 */
void Socket::close_client() {
    if (DEBUG) {
        std::cout << "Closing client connection" << std::endl;
    }

    // Only close the client file descriptor in the parent
    if (accept_fd_ != -1) {
        close(accept_fd_);
        accept_fd_ = -1;
    }

    // Don't close socket_fp_ in parent as it was created in child's address space
    socket_fp_ = nullptr;
}

/**
 * Accepts a client connection and updates the internal state of the socket.
 */
void Socket::accept_client() {
    sin_size_ = sizeof(struct sockaddr_in);

    while (true) {
        accept_fd_ = accept(socket_fd_, reinterpret_cast<struct sockaddr*>(&client),
                           reinterpret_cast<socklen_t*>(&sin_size_));

        if (accept_fd_ == -1) {
            if (errno == EINTR) {
                continue; // Restart accept
            } else {
                handle_error("Failed to accept client");
            }
        } else {
            break;
        }
    }

    if (DEBUG) {
        std::cout << "Client accepted from " << inet_ntoa(client.sin_addr) << "... my pid is "
                  << getpid() << std::endl;
    }

    socket_fp_ = fdopen(accept_fd_, "r");
    if (socket_fp_ == nullptr) {
        handle_error("Failed to open file descriptor");
    }
}

/**
 * Sends a string to the connected client.
 */
void Socket::write_line(std::string_view line) {
    if (accept_fd_ == -1) {
        return; // No client connected
    }

    if (send(accept_fd_, line.data(), line.size(), 0) == -1) {
        handle_error("Failed to send data");
    }
}

/**
 * Writes raw data to the socket.
 */
int Socket::write_raw(const char* data, size_t size) {
    if (accept_fd_ < 0) {
        return -1;
    }
    return send(accept_fd_, data, size, 0);
}

/**
 * Reads raw data from the socket.
 */
ssize_t Socket::read_raw(char* buffer, size_t size) {
    if (accept_fd_ < 0) {
        return -1;
    }
    return recv(accept_fd_, buffer, size, 0);
}

/**
 * Reads a line from the socket into the provided buffer.
 */
bool Socket::read_line(std::string* buffer) {
    if (!socket_fp_) {
        return false;
    }

    char c = fgetc(socket_fp_);

    while (c != '\n' && c != EOF && c != '\r') {
        buffer->push_back(c);
        c = fgetc(socket_fp_);
    }

    // Handle CRLF properly
    if (c == '\r') {
        buffer->push_back(c);
        char next = fgetc(socket_fp_);
        if (next == '\n') {
            buffer->push_back(next);
        } else if (next != EOF) {
            // Put it back if it's not '\n'
            ungetc(next, socket_fp_);
        }
    } else if (c == '\n') {
        buffer->push_back(c);
    }

    return c != EOF;
}

/**
 * Reads a line from the socket with a timeout.
 */
bool Socket::read_line_with_timeout(std::string* buffer, int timeout_seconds) {
    if (!socket_fp_ || accept_fd_ < 0) {
        return false;
    }

    // Use poll to check if data is available
    struct pollfd pfd;
    pfd.fd = accept_fd_;
    pfd.events = POLLIN;
    pfd.revents = 0;

    // Convert seconds to milliseconds
    int timeout_ms = timeout_seconds * 1000;
    
    // Wait for data or timeout
    int ret = poll(&pfd, 1, timeout_ms);
    
    if (ret < 0) {
        // Error occurred
        if (DEBUG) {
            std::cerr << "Poll error: " << strerror(errno) << std::endl;
        }
        return false;
    } else if (ret == 0) {
        // Timeout occurred
        if (DEBUG) {
            std::cout << "Read timeout after " << timeout_seconds << " seconds" << std::endl;
        }
        return false;
    }
    
    // Data is available, use regular read_line
    return read_line(buffer);
}

/**
 * Sets socket options.
 */
void Socket::set_socket_options() {
    const int yes = 1; // For setsockopt() SO_REUSEADDR
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        handle_error("Failed to set socket options");
    }
}

/**
 * Binds the server socket.
 */
void Socket::bind_socket(int server_port) {
    server_.sin_family = AF_INET;
    server_.sin_port = htons(server_port);
    server_.sin_addr.s_addr = INADDR_ANY;

    if (::bind(socket_fd_, (struct sockaddr*)&server_, sizeof(server_)) == -1) {
        handle_error("Failed to bind socket");
    }
}

/**
 * Initializes and binds the server socket to the given port.
 */
void Socket::server_bind(int server_port) {
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ == -1) {
        handle_error("Failed to create socket");
    }

    set_socket_options();
    bind_socket(server_port);

    if (listen(socket_fd_, NUM_CLIENTS_TO_QUEUE) == -1) {
        handle_error("Failed to listen on socket");
    }
}
