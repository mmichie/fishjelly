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
    int socket_fd, sin_size;
    struct sockaddr_in server;
    const static int NUM_CLIENTS_TO_QUEUE = 10;
    void serverBind(int server_port);
    FILE* socket_fp;

  public:
    int accept_fd, pid;

    struct sockaddr_in client;

    void acceptClient();
    bool readLine(std::string* buffer);
    void writeLine(std::string_view line);
    void handleError(std::string_view message);
    void setSocketOptions();
    void bindSocket(int server_port);

    void closeSocket();

    // Constructor
    Socket(int server_port) : socket_fd(-1), socket_fp(nullptr), accept_fd(-1) {
        // Bind the port
        serverBind(server_port);
    }
};

#endif /* !SHELOB_SOCKET_H */
