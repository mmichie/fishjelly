#ifndef SHELOB_HTTP_H
#define SHELOB_HTTP_H 1

#include <map>
#include <signal.h>
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
    void printDate(void);
    void printServer(void);
    void printContentType(string type);
    void printContentLength(int size);
    void printConnectionType(bool keep_alive = false);
    string sanitizeFilename(string filename);
    void sendFile(string filename);
    void processHeadRequest(map<string, string> headermap);
    void processGetRequest(map<string, string> headermap, string request_line,
                           bool keep_alive);
    void processPostRequest(map<string, string> headermap);

  public:
    void sendHeader(int code, int size, string file_type = "text/plain",
                    bool keep_alive = false);
    string getHeader();
    void start(int server_port);
    bool parseHeader(string header);

    Socket *sock;
};

#endif /* !SHELOB_HTTP_H */
