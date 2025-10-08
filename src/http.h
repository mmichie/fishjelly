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
#include "content_negotiator.h"
#include "filter.h"
#include "log.h"
#include "middleware.h"
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
    void sendFileWithMiddleware(std::string_view filename, const std::string& method,
                                const std::string& path, const std::string& version,
                                const std::map<std::string, std::string>& headermap);
    void processHeadRequest(const std::map<std::string, std::string>& headermap, bool keep_alive);
    void processGetRequest(const std::map<std::string, std::string>& headermap,
                           std::string_view request_line, bool keep_alive);
    void processPostRequest(const std::map<std::string, std::string>& headermap, bool keep_alive);
    void processPutRequest(const std::map<std::string, std::string>& headermap,
                           std::string_view request_line, bool keep_alive);
    void processDeleteRequest(const std::map<std::string, std::string>& headermap,
                              std::string_view request_line, bool keep_alive);
    std::map<std::string, std::string> parseFormUrlEncoded(const std::string& body);
    std::map<std::string, std::string> parseMultipartFormData(const std::string& body,
                                                              const std::string& boundary);
    std::string getBoundaryFromContentType(const std::string& content_type);
    std::map<std::string, std::string> parseCookies(const std::string& cookie_header);
    void setCookie(const std::string& name, const std::string& value, const std::string& path = "/",
                   int max_age = -1, bool secure = false, bool http_only = false,
                   const std::string& same_site = "");
    void processOptionsRequest(const std::map<std::string, std::string>& headermap,
                               bool keep_alive);
    time_t parseHttpDate(const std::string& date_str);
    bool isModifiedSince(const std::string& filename, time_t since_time);

    std::string lastHeader; // Store last sent header for testing
    int test_requests = 0;  // Exit after N requests (0 = run forever)
    int request_count = 0;  // Current request count

    // Middleware chain
    std::unique_ptr<MiddlewareChain> middleware_chain;

    // Cookie storage for response
    std::vector<std::string> response_cookies;

    // Content negotiation
    ContentNegotiator content_negotiator;

  public:
    Http(); // Constructor to initialize middleware

    void sendHeader(int code, int size, std::string_view file_type = "text/plain",
                    bool keep_alive = false, const std::vector<std::string>& extra_headers = {});
    void sendOptionsHeader(bool keep_alive = false);
    std::string getHeader(bool use_timeout = false);
    void start(int server_port);
    bool parseHeader(std::string_view header);
    void setTestMode(int requests) { test_requests = requests; }

    // Middleware configuration
    void setMiddlewareChain(std::unique_ptr<MiddlewareChain> chain) {
        middleware_chain = std::move(chain);
    }

    // Set up default middleware chain
    void setupDefaultMiddleware();

    std::unique_ptr<Socket> sock;
};

#endif /* !SHELOB_HTTP_H */
