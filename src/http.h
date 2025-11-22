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

#include "auth.h"
#include "cgi.h"
#include "content_negotiator.h"
#include "filter.h"
#include "log.h"
#include "middleware.h"
#include "mime.h"
#include "socket.h"
#include "token.h"

// Structure to represent a byte range
struct ByteRange {
    long long start; // -1 means unspecified
    long long end;   // -1 means unspecified
    bool is_suffix;  // true for suffix ranges like -500 (last 500 bytes)
};

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
    std::string readChunkedBody();
    void writeChunkedData(std::string_view data);
    void writeChunkedEnd();
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

    // Range request support
    std::vector<ByteRange> parseRangeHeader(const std::string& range_header);
    bool validateRange(const ByteRange& range, long long file_size, long long& start,
                       long long& end);
    void sendPartialContent(std::string_view filename, const std::vector<ByteRange>& ranges,
                            long long file_size, std::string_view content_type, bool keep_alive);
    void sendMultipartRanges(std::string_view filename, const std::vector<ByteRange>& ranges,
                             long long file_size, std::string_view content_type, bool keep_alive);
    std::string formatHttpDate(time_t time);

    // Authentication support
    bool checkAuthentication(const std::string& path, const std::string& method,
                             const std::map<std::string, std::string>& headermap, bool keep_alive);

    std::string lastHeader; // Store last sent header for testing
    int test_requests = 0;  // Exit after N requests (0 = run forever)
    int request_count = 0;  // Current request count

    // Worker pool management
    std::vector<pid_t> worker_pids_;
    int max_requests_per_worker_ = 1000; // Worker restarts after this many requests
    void worker_loop();
    void monitor_workers();
    void cleanup_workers();

    // Middleware chain
    std::unique_ptr<MiddlewareChain> middleware_chain;

    // Cookie storage for response
    std::vector<std::string> response_cookies;

    // Content negotiation
    ContentNegotiator content_negotiator;

    // Authentication
    Auth auth;

    // Rate limiting
    // Note: In fork-per-connection mode, each child process has its own rate_limit_map_,
    // so rate limiting is less effective for parallel requests from the same IP.
    // Rate limiting works best with:
    // 1. Sequential requests (typical browser behavior)
    // 2. Worker pool mode where workers share state within a process
    // For distributed/shared rate limiting, consider using Redis or similar
    struct RateLimitInfo {
        std::vector<time_t> request_times;
        time_t blocked_until = 0; // Time when client will be unblocked
    };
    std::map<std::string, RateLimitInfo> rate_limit_map_;
    int rate_limit_max_requests_ = 100;  // Max requests per window
    int rate_limit_window_seconds_ = 60; // Time window in seconds
    int rate_limit_block_seconds_ = 60;  // How long to block after limit exceeded
    bool rate_limiting_enabled_ = false; // Disabled by default, opt-in

    // Maintenance mode
    bool maintenance_mode_ = false;
    std::string maintenance_message_ = "Server is temporarily unavailable for maintenance";

    // Rate limiting methods
    bool checkRateLimit(const std::string& client_ip, bool keep_alive);
    void cleanupRateLimitMap();

  public:
    Http(); // Constructor to initialize middleware

    void sendHeader(int code, int size, std::string_view file_type = "text/plain",
                    bool keep_alive = false, const std::vector<std::string>& extra_headers = {});
    void sendRedirect(int code, const std::string& location, bool keep_alive = false);
    void sendOptionsHeader(bool keep_alive = false);
    std::string getHeader(bool use_timeout = false);
    void start(int server_port, int read_timeout = 30, int write_timeout = 30, int num_workers = 0);
    bool parseHeader(std::string_view header);
    void setTestMode(int requests) { test_requests = requests; }
    void setMaxRequestsPerWorker(int max_requests) { max_requests_per_worker_ = max_requests; }

    // Middleware configuration
    void setMiddlewareChain(std::unique_ptr<MiddlewareChain> chain) {
        middleware_chain = std::move(chain);
    }

    // Set up default middleware chain
    void setupDefaultMiddleware();

    // Rate limiting configuration
    void setRateLimitEnabled(bool enabled) { rate_limiting_enabled_ = enabled; }
    void setRateLimitMaxRequests(int max_requests) { rate_limit_max_requests_ = max_requests; }
    void setRateLimitWindow(int seconds) { rate_limit_window_seconds_ = seconds; }
    void setRateLimitBlockDuration(int seconds) { rate_limit_block_seconds_ = seconds; }

    // Maintenance mode configuration
    void setMaintenanceMode(bool enabled) { maintenance_mode_ = enabled; }
    void setMaintenanceMessage(const std::string& message) { maintenance_message_ = message; }

    std::unique_ptr<Socket> sock;
};

#endif /* !SHELOB_HTTP_H */
