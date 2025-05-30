#ifndef SHELOB_HTTP_H
#define SHELOB_HTTP_H 1

#include <map>
#include <memory>
#include <signal.h>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <vector>

#include "global.h"

#include "cgi.h"
#include "filter.h"
#include "log.h"
#include "mime.h"
#include "socket.h"
#include "token.h"

class Http {
  private:
    void printDate();
    void printServer();
    void printContentType(std::string_view type);
    void printContentLength(int size);
    void printConnectionType(bool keep_alive = false);
    std::string sanitizeFilename(std::string_view filename);
    void sendFile(std::string_view filename);
    void processHeadRequest(const std::map<std::string, std::string>& headermap);
    void processGetRequest(const std::map<std::string, std::string>& headermap,
                           std::string_view request_line, bool keep_alive);
    void processPostRequest(const std::map<std::string, std::string>& headermap);
    
    std::string lastHeader; // Store last sent header for testing

  public:
    void sendHeader(int code, int size, std::string_view file_type = "text/plain",
                    bool keep_alive = false);
    std::string getHeader();
    void start(int server_port);
    bool parseHeader(std::string_view header);

    std::unique_ptr<Socket> sock;
};

#endif /* !SHELOB_HTTP_H */
