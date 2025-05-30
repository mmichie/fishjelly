// socket.cc
#include "socket.h"
#include <iostream>  // for std::cerr, std::cout
#include <stdexcept> // for std::runtime_error
#include <unistd.h>

/**
 * SocketException class for specialized socket error handling.
 */
class SocketException : public std::runtime_error {
  public:
    SocketException(const std::string& message) : std::runtime_error(message) {}
};

/**
 * Centralized error handler for the Socket class.
 */
void Socket::handleError(std::string_view message) {
    std::cerr << message << std::endl;
    throw SocketException(std::string(message));
}

/**
 * Safely closes the socket and its associated file pointer.
 */
void Socket::closeSocket() {
    if (DEBUG) {
        std::cout << "Closing socket" << std::endl;
    }

    if (fclose(socket_fp) != 0) {
        handleError("Failed to close socket file pointer");
    }
}

/**
 * Accepts a client connection and updates the internal state of the socket.
 */
void Socket::acceptClient() {
    sin_size = sizeof(struct sockaddr_in);

    while (true) {
        accept_fd = accept(socket_fd, reinterpret_cast<struct sockaddr*>(&client),
                           reinterpret_cast<socklen_t*>(&sin_size));

        if (accept_fd == -1) {
            if (errno == EINTR) {
                continue; // Restart accept
            } else {
                handleError("Failed to accept client");
            }
        } else {
            break;
        }
    }

    if (DEBUG) {
        std::cout << "Client accepted from " << inet_ntoa(client.sin_addr) << "... my pid is "
                  << getpid() << std::endl;
    }

    socket_fp = fdopen(accept_fd, "r");
    if (socket_fp == nullptr) {
        handleError("Failed to open file descriptor");
    }
}

/**
 * Sends a string to the connected client.
 */
void Socket::writeLine(std::string_view line) {
    if (send(accept_fd, line.data(), line.size(), 0) == -1) {
        handleError("Failed to send data");
    }
}

/**
 * Reads a line from the socket into the provided buffer.
 */
bool Socket::readLine(std::string* buffer) {
    char c = fgetc(socket_fp);

    while (c != '\n' && c != EOF && c != '\r') {
        buffer->push_back(c);
        c = fgetc(socket_fp);
    }

    if (c == '\n') {
        buffer->push_back(c);
    }

    return c != EOF;
}

/**
 * Sets socket options.
 */
void Socket::setSocketOptions() {
    int yes = 1; // For setsockopt() SO_REUSEADDR
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        handleError("Failed to set socket options");
    }
}

/**
 * Binds the server socket.
 */
void Socket::bindSocket(int server_port) {
    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);
    server.sin_addr.s_addr = INADDR_ANY;

    if (::bind(socket_fd, (struct sockaddr*)&server, sizeof(server)) == -1) {
        handleError("Failed to bind socket");
    }
}

/**
 * Initializes and binds the server socket to the given port.
 */
void Socket::serverBind(int server_port) {
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        handleError("Failed to create socket");
    }

    setSocketOptions();
    bindSocket(server_port);

    if (listen(socket_fd, NUM_CLIENTS_TO_QUEUE) == -1) {
        handleError("Failed to listen on socket");
    }
}
