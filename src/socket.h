#ifndef SHELOB_SOCKET_H
#define SHELOB_SOCKET_H 1

#include <cerrno>
#include <cstdio>
#include <iostream>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "global.h"

class Socket {
  private:
    int socket_fd_{-1}, sin_size_;
    struct sockaddr_in server_;
    static constexpr int NUM_CLIENTS_TO_QUEUE = 10;
    void server_bind(int server_port);
    FILE* socket_fp_{nullptr};

  public:
    int accept_fd_{-1}, pid_;

    struct sockaddr_in client;

    void accept_client();
    virtual bool read_line(std::string* buffer);
    bool read_line_with_timeout(std::string* buffer, int timeout_seconds);
    virtual void write_line(std::string_view line);
    virtual int write_raw(const char* data, size_t size); // Returns bytes written or -1 on error
    void handle_error(std::string_view message);
    void set_socket_options();
    void bind_socket(int server_port);

    void close_socket();
    void close_client(); // Close only the client connection

    // Constructor
    explicit Socket(int server_port) : socket_fd_{-1}, socket_fp_{nullptr}, accept_fd_{-1} {
        // Bind the port
        if (server_port > 0) {
            server_bind(server_port);
        }
    }
    
    // Virtual destructor
    virtual ~Socket() = default;
};

#endif /* !SHELOB_SOCKET_H */
