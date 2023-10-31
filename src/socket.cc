// socket.cc
#include "socket.h"
#include <iostream>  // for std::cerr, std::cout
#include <stdexcept> // for std::runtime_error
#include <unistd.h>

/**
 * Safely closes the socket and its associated file pointer.
 */
void Socket::closeSocket() {
    if (DEBUG) {
        std::cout << "Closing socket" << std::endl;
    }

    if (fclose(socket_fp) != 0) {
        std::cerr << "Failed to close socket file pointer" << std::endl;
    }
}

/**
 * Accepts a client connection and updates the internal state of the socket.
 */
void Socket::acceptClient() {
    sin_size = sizeof(struct sockaddr_in);

    while (true) {
        accept_fd =
            accept(socket_fd, reinterpret_cast<struct sockaddr *>(&client),
                   reinterpret_cast<socklen_t *>(&sin_size));

        if (accept_fd == -1) {
            if (errno == EINTR) {
                continue; // Restart accept
            } else {
                throw std::runtime_error("Failed to accept client");
            }
        } else {
            break;
        }
    }

    if (DEBUG) {
        std::cout << "Client accepted from " << inet_ntoa(client.sin_addr)
                  << "... my pid is " << getpid() << std::endl;
    }

    socket_fp = fdopen(accept_fd, "r");
    if (socket_fp == nullptr) {
        throw std::runtime_error("Failed to open file descriptor");
    }
}

/**
 * Sends a string to the connected client.
 */
void Socket::writeLine(const std::string &line) {
    if (send(accept_fd, line.data(), line.size(), 0) == -1) {
        throw std::runtime_error("Failed to send data");
    }
}

/**
 * Reads a line from the socket into the provided buffer.
 */
bool Socket::readLine(std::string *buffer) {
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
 * Initializes and binds the server socket to the given port.
 */
void Socket::serverBind(int server_port) {
    int yes = 1; // For setsockopt() SO_REUSEADDR

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        throw std::runtime_error("Failed to create socket");
    }

    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) ==
        -1) {
        throw std::runtime_error("Failed to set socket options");
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);
    server.sin_addr.s_addr = INADDR_ANY;

    if (::bind(socket_fd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        throw std::runtime_error("Failed to bind socket");
    }

    if (listen(socket_fd, NUM_CLIENTS_TO_QUEUE) == -1) {
        throw std::runtime_error("Failed to listen on socket");
    }
}
