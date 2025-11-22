#ifndef SHELOB_SOCKET_H
#define SHELOB_SOCKET_H 1

#include <cstdio>
#include <string>
#include <string_view>

#include <netinet/in.h>

/**
 * Socket interface for HTTP I/O operations
 * This is a pure abstract base class implemented by AsioSocketAdapter
 */
class Socket {
  public:
    // Client information (set by implementations)
    struct sockaddr_in client;

    /**
     * Virtual destructor for proper cleanup
     */
    virtual ~Socket() = default;

    /**
     * Read a line from the socket
     * @param buffer Pointer to string buffer for the line
     * @return true if successful, false on error/EOF
     */
    virtual bool read_line(std::string* buffer) = 0;

    /**
     * Read raw bytes from the socket
     * @param buffer Buffer to store the data
     * @param size Number of bytes to read
     * @return Number of bytes read, or -1 on error
     */
    virtual ssize_t read_raw(char* buffer, size_t size) = 0;

    /**
     * Write a line to the socket
     * @param line String view of data to write
     */
    virtual void write_line(std::string_view line) = 0;

    /**
     * Write raw bytes to the socket
     * @param data Pointer to data to write
     * @param size Number of bytes to write
     * @return Number of bytes written, or -1 on error
     */
    virtual int write_raw(const char* data, size_t size) = 0;

  protected:
    /**
     * Protected constructor - only implementations can instantiate
     */
    Socket() = default;
};

#endif /* !SHELOB_SOCKET_H */
