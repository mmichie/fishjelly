#ifndef HTTP_OUTPUT_INTERFACE_H
#define HTTP_OUTPUT_INTERFACE_H

#include <string>
#include <string_view>

// Abstract interface for HTTP output
class HttpOutputInterface {
public:
    virtual ~HttpOutputInterface() = default;
    
    // Write a line of text (adds newline if not present)
    virtual void writeLine(std::string_view line) = 0;
    
    // Write raw data
    virtual void writeData(const char* data, size_t size) = 0;
    
    // Get the underlying file descriptor (for CGI, returns -1 if not applicable)
    virtual int getFileDescriptor() const { return -1; }
    
    // Get client address for logging
    virtual std::string getClientAddress() const = 0;
};

#endif // HTTP_OUTPUT_INTERFACE_H