#include "http.h"
#include "compression_middleware.h"
#include "footer_middleware.h"
#include "logging_middleware.h"
#include "security_middleware.h"
#include <array>
#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

/**
 * Constructor - Initialize without middleware by default
 */
Http::Http() {
    // Don't initialize middleware by default to maintain compatibility
    // Middleware can be set up explicitly with setupDefaultMiddleware()

    // Configure authentication users (hardcoded for demo)
    auth.add_user("admin", "secret123");
    auth.add_user("testuser", "password");
    auth.add_user("demo", "demo");

    // Configure protected paths
    auth.add_protected_path("/secure", "Secure Area");
    auth.add_protected_path("/admin", "Admin Area");
}

/**
 * Set up default middleware chain
 */
void Http::setupDefaultMiddleware() {
    // Create default middleware chain
    middleware_chain = std::make_unique<MiddlewareChain>();

    // Add middleware in order of execution
    // 1. Security checks first
    middleware_chain->use(std::make_shared<SecurityMiddleware>());

    // 2. Logging
    middleware_chain->use(std::make_shared<LoggingMiddleware>());

    // 3. Compression (after content is generated)
    middleware_chain->use(std::make_shared<CompressionMiddleware>());

    // 4. Footer addition (for .shtml files)
    middleware_chain->use(std::make_shared<FooterMiddleware>());
}

/**
 * Parse HTTP date format (RFC 2616)
 * Example: "Wed, 01 Jan 2025 00:00:00 GMT"
 */
time_t Http::parseHttpDate(const std::string& date_str) {
    struct tm tm = {};
    // Try to parse the date string
    if (strptime(date_str.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm) == nullptr) {
        return 0; // Return epoch on parse failure
    }
    // Set DST flag to -1 to let mktime determine it
    tm.tm_isdst = -1;

    // Save current timezone
    char* tz = getenv("TZ");
    std::string old_tz;
    if (tz)
        old_tz = tz;

    // Set timezone to UTC
    setenv("TZ", "UTC", 1);
    tzset();

    // Convert to time_t
    time_t result = mktime(&tm);

    // Restore original timezone
    if (old_tz.empty()) {
        unsetenv("TZ");
    } else {
        setenv("TZ", old_tz.c_str(), 1);
    }
    tzset();

    return result;
}

/**
 * Check if a file has been modified since the given time
 */
bool Http::isModifiedSince(const std::string& filename, time_t since_time) {
    struct stat file_stat;
    if (stat(filename.c_str(), &file_stat) != 0) {
        return true; // If we can't stat the file, assume it's modified
    }
    return file_stat.st_mtime > since_time;
}

/**
 * Write a RFC 2616 compliant Date header to the client.
 */
void Http::printDate() {
    if (!sock)
        return;

    std::array<char, 50> buf;
    time_t ltime = time(nullptr);
    struct tm* today = gmtime(&ltime);

    // Date: Fri, 16 Jul 2004 15:37:18 GMT
    strftime(buf.data(), buf.size(), "%a, %d %b %Y %H:%M:%S GMT", today);

    sock->write_line(std::format("Date: {}\r\n", buf.data()));
}

/**
 * Write a RFC 2616 compliant Server header to the client.
 */
void Http::printServer() {
    if (sock)
        sock->write_line("Server: SHELOB/0.5 (Unix)\r\n");
}

/**
 * Write a RFC 2616 compliant ContentType header to the client.
 */
void Http::printContentType(std::string_view type) {
    if (sock)
        sock->write_line(std::format("Content-Type: {}\r\n", type));
}

/**
 * Write a RFC 2616 compliant ContentLength header to the client.
 */
void Http::printContentLength(int size) {
    assert(size >= 0);
    if (sock)
        sock->write_line(std::format("Content-Length: {}\r\n", size));
}

/**
 * Write a RFC 2616 compliant ConnectionType header to the client.
 */
void Http::printConnectionType(bool keep_alive) {
    if (!sock)
        return;

    if (keep_alive)
        sock->write_line("Connection: keep-alive\r\n");
    else
        sock->write_line("Connection: close\r\n");
}

/**
 * Starts the web server, listening on server_port.
 */
void Http::start(int server_port, int read_timeout, int write_timeout, int num_workers) {
    assert(server_port > 0 && server_port <= 65535);

    sock = std::make_unique<Socket>(server_port);

    // Configure timeouts
    sock->set_read_timeout(read_timeout);
    sock->set_write_timeout(write_timeout);

    // Use worker pool if num_workers > 0, otherwise use traditional fork model
    if (num_workers > 0) {
        // Pre-fork worker pool model
        if (DEBUG) {
            std::cout << "Starting worker pool with " << num_workers << " workers" << std::endl;
        }

        // Create worker processes
        for (int i = 0; i < num_workers; i++) {
            pid_t pid = fork();
            if (pid < 0) {
                perror("Fork worker");
                cleanup_workers();
                exit(1);
            } else if (pid == 0) {
                // Worker process
                worker_loop();
                exit(0);
            } else {
                // Parent: track worker PID
                worker_pids_.push_back(pid);
                if (DEBUG) {
                    std::cout << "Created worker " << i << " with PID " << pid << std::endl;
                }
            }
        }

        // Parent monitors and manages workers
        monitor_workers();
    } else {
        // Traditional fork-per-connection model
        bool keep_alive = false;
        int pid;

        // Loop to handle clients
        while (1) {
            sock->accept_client();

            pid = fork();
            if (pid < 0) {
                perror("Fork");
                exit(1);
            }

            /* Child */
            if (pid == 0) {
                // First request doesn't use timeout
                std::string header_str = getHeader(false);
                keep_alive = parseHeader(header_str);
                while (keep_alive) {
                    // Subsequent requests use timeout for keep-alive
                    std::string header = getHeader(true);
                    if (header.empty()) {
                        // Timeout or connection closed
                        break;
                    }
                    keep_alive = parseHeader(header);
                }

                sock->close_socket();
                exit(0);
            }

            /* Parent */
            else {
                sock->close_client(); // Only close client connection, not server socket
                request_count++;

                // Exit after N requests in test mode
                if (test_requests > 0 && request_count >= test_requests) {
                    std::cout << "Test mode: Exiting after " << request_count << " requests"
                              << std::endl;
                    break;
                }
            }
        }

        // Print completion message for test mode
        if (test_requests > 0) {
            std::cout << "Server shutdown complete." << std::endl;
        }
    }
}

std::string Http::sanitizeFilename(std::string_view filename) {
    std::filesystem::path path;

    // Remove leading '/' from filename
    if (!filename.empty() && filename[0] == '/') {
        filename.remove_prefix(1);
    }

    // Default page
    if (filename.empty()) {
        path = "htdocs/index.html";
    } else {
        // Remove any trailing newlines
        auto pos = filename.find('\n');
        if (pos != std::string_view::npos) {
            filename = filename.substr(0, pos);
        }
        path = std::filesystem::path("htdocs") / filename;

        // If the path is a directory, append index.html
        if (std::filesystem::is_directory(path)) {
            path = path / "index.html";
        }
    }

    return path.string();
}

/**
 * Sends a file down an open socket.
 */
void Http::sendFile(std::string_view filename) {
    // Open file
    std::ifstream file(filename.data(), std::ios::in | std::ios::binary);
    if (!file) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    // Find the extension using filesystem
    std::filesystem::path filepath(filename);
    std::string file_extension = filepath.extension().string();

    // Determine File Size
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read entire file into vector
    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        std::cerr << "Error reading file!" << std::endl;
        return;
    }

    // Text manipulate contents of .shtml
    std::string filtered;
    if (file_extension == ".shtml" || file_extension == ".shtm") {
        // Add the footer
        std::string s_buffer(buffer.begin(), buffer.end());
        Filter filter;
        filtered = filter.addFooter(s_buffer);

        // Send filtered content
        if (sock->write_raw(filtered.data(), filtered.length()) == -1) {
            perror("send");
        }
    } else {
        // Send raw buffer
        if (sock->write_raw(buffer.data(), buffer.size()) == -1) {
            perror("send");
        }
    }
}

void Http::sendFileWithMiddleware(std::string_view filename, const std::string& method,
                                  const std::string& path, const std::string& version,
                                  const std::map<std::string, std::string>& headermap) {
    // Create request context
    RequestContext ctx;
    ctx.method = method;
    ctx.path = path;
    ctx.version = version;
    ctx.headers = headermap;
    ctx.http_handler = this;

    // Open file and read content
    std::ifstream file(filename.data(), std::ios::in | std::ios::binary);
    if (!file) {
        ctx.status_code = 404;
        ctx.response_body =
            "<html><head><title>404</title></head><body>404 not found</body></html>";
        ctx.content_type = "text/html";
    } else {
        // Determine File Size
        file.seekg(0, std::ios::end);
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);

        // Read entire file
        std::vector<char> buffer(size);
        if (!file.read(buffer.data(), size)) {
            ctx.status_code = 500;
            ctx.response_body = "<html><body>500 Internal Server Error</body></html>";
            ctx.content_type = "text/html";
        } else {
            ctx.status_code = 200;
            ctx.response_body = std::string(buffer.begin(), buffer.end());

            // Determine content type
            Mime mime;
            mime.readMimeConfig("mime.types");
            ctx.content_type = mime.getMimeFromExtension(filename);
        }
    }

    // Execute middleware chain
    if (middleware_chain) {
        middleware_chain->execute(ctx);
    }

    // Send response if not already sent by middleware
    if (!ctx.response_sent) {
        // Send custom headers added by middleware
        for (const auto& [key, value] : ctx.response_headers) {
            sock->write_line(key + ": " + value);
        }

        // Send the response body
        if (sock->write_raw(ctx.response_body.data(), ctx.response_body.length()) == -1) {
            perror("send");
        }
    }
}

bool Http::parseHeader(std::string_view header) {
    std::string request_line;

    std::vector<std::string> tokens, tokentmp;
    std::map<std::string, std::string> headermap;

    bool keep_alive = false;

    unsigned int i;

    // Handle empty header (connection closed)
    if (header.empty()) {
        if (DEBUG) {
            std::cout << "Empty header received - connection closed by client" << std::endl;
        }
        return false;
    }

    Token token;
    /* Seperate the client request headers by newline */
    token.tokenize(header, tokens, "\n");

    if (tokens.empty()) {
        return false;
    }

    /* The first line of the client request should always be GET, HEAD, POST,
     * etc */
    request_line = tokens[0];
    // Remove trailing \r if present
    if (!request_line.empty() && request_line.back() == '\r') {
        request_line.pop_back();
    }

    token.tokenize(request_line, tokentmp, " ");

    if (tokentmp.size() < 3) {
        // Only send 400 if we actually have a malformed request line (not empty)
        if (!request_line.empty() && sock) {
            sendHeader(400, 0, "text/html", false);
            sock->write_line("<html><body>400 Bad Request - Malformed request line</body></html>");
        }
        return false;
    }

    // Parse method, URI, and HTTP version
    std::string method = tokentmp[0];
    std::string uri = tokentmp[1];
    std::string http_version = tokentmp[2];

    // Store method and URI in headermap for backward compatibility
    headermap[method] = uri;

    // Validate HTTP version
    if (http_version != "HTTP/1.0" && http_version != "HTTP/1.1") {
        if (DEBUG) {
            std::cout << "Unsupported HTTP version: " << http_version << std::endl;
        }
        if (sock) {
            // Send 505 HTTP Version Not Supported
            sendHeader(505, 0, "text/html", false);
            sock->write_line("<html><body>505 HTTP Version Not Supported</body></html>");
        }
        return false;
    }

    // Clear response cookies from previous request
    response_cookies.clear();

    /* Seperate each request header with the name and value and insert into a
     * hash map */
    for (i = 1; i < tokens.size(); i++) {
        // Skip empty lines
        if (tokens[i].empty() || tokens[i] == "\r") {
            continue;
        }

        std::string::size_type pos = tokens[i].find(':');
        if (pos != std::string::npos) {
            std::string name = tokens[i].substr(0, pos);
            // Skip the colon and any spaces after it
            std::string::size_type value_start = pos + 1;
            while (value_start < tokens[i].length() && tokens[i][value_start] == ' ') {
                value_start++;
            }

            std::string value = tokens[i].substr(value_start);
            // Remove trailing \r if present
            if (!value.empty() && value.back() == '\r') {
                value.pop_back();
            }

            headermap[name] = value;
        }
    }

    /* Print all pairs of the header hash map to console */
    if (DEBUG) {
        std::cout << "Method: " << method << ", URI: " << uri << ", HTTP Version: " << http_version
                  << std::endl;
        std::cout << "Headers:" << std::endl;
        for (const auto& [key, value] : headermap) {
            if (key != method) { // Don't print method as header
                std::cout << "  " << key << ": " << value << std::endl;
            }
        }
    }

    // HTTP/1.1 requires Host header
    if (http_version == "HTTP/1.1" && headermap.find("Host") == headermap.end()) {
        if (DEBUG) {
            std::cout << "Host header not found for HTTP/1.1 request" << std::endl;
        }
        if (sock) {
            sendHeader(400, 0, "text/html", false);
            sock->write_line(
                "<html><body>400 Bad Request - HTTP/1.1 requires Host header</body></html>");
        }
        return false;
    }

    // Handle Connection header based on HTTP version
    // HTTP/1.0 defaults to close, HTTP/1.1 defaults to keep-alive
    if (http_version == "HTTP/1.0") {
        keep_alive = (headermap["Connection"] == "keep-alive");
    } else { // HTTP/1.1
        keep_alive = (headermap["Connection"] != "close");
    }

    // Check if we have a valid request method
    if (headermap.find("GET") == headermap.end() && headermap.find("HEAD") == headermap.end() &&
        headermap.find("POST") == headermap.end() && headermap.find("OPTIONS") == headermap.end() &&
        headermap.find("PUT") == headermap.end() && headermap.find("DELETE") == headermap.end()) {
        if (DEBUG) {
            std::cout << "No valid request method found in headermap" << std::endl;
        }
        if (sock) {
            // Send 405 Method Not Allowed with Allow header
            std::ostringstream headerStream;
            headerStream << "HTTP/1.1 405 Method Not Allowed\r\n";

            // Generate date header
            std::array<char, 50> buf;
            time_t ltime = time(nullptr);
            struct tm* today = gmtime(&ltime);
            strftime(buf.data(), buf.size(), "%a, %d %b %Y %H:%M:%S GMT", today);
            headerStream << "Date: " << buf.data() << "\r\n";

            headerStream << "Server: SHELOB/0.5 (Unix)\r\n";
            headerStream << "Allow: GET, HEAD, POST, OPTIONS, PUT, DELETE\r\n";
            headerStream << "Content-Length: 0\r\n";
            headerStream << "Connection: " << (keep_alive ? "keep-alive" : "close") << "\r\n";
            headerStream << "\r\n";

            sock->write_line(headerStream.str());
        }
        return false;
    }

    if (sock) {
        if (headermap.find("GET") != headermap.end()) {
            processGetRequest(headermap, request_line, keep_alive);
        } else if (headermap.find("HEAD") != headermap.end()) {
            processHeadRequest(headermap, keep_alive);
        } else if (headermap.find("POST") != headermap.end()) {
            processPostRequest(headermap, keep_alive);
        } else if (headermap.find("PUT") != headermap.end()) {
            processPutRequest(headermap, request_line, keep_alive);
        } else if (headermap.find("DELETE") != headermap.end()) {
            processDeleteRequest(headermap, request_line, keep_alive);
        } else if (headermap.find("OPTIONS") != headermap.end()) {
            processOptionsRequest(headermap, keep_alive);
        }
    }

    return keep_alive; // Return keep_alive status for connection handling
}

void Http::processPostRequest(const std::map<std::string, std::string>& headermap,
                              bool keep_alive) {
    // Check for Transfer-Encoding header
    auto transfer_encoding_it = headermap.find("Transfer-Encoding");
    bool is_chunked = false;
    if (transfer_encoding_it != headermap.end() &&
        transfer_encoding_it->second.find("chunked") != std::string::npos) {
        is_chunked = true;
    }

    std::string body_str;

    if (is_chunked) {
        // Read chunked body
        body_str = readChunkedBody();
        // Note: empty body is valid for chunked encoding
        // readChunkedBody() sends error responses if needed
    } else {
        // Check for Content-Length header (required for POST when not chunked)
        auto content_length_it = headermap.find("Content-Length");
        if (content_length_it == headermap.end()) {
            if (DEBUG) {
                std::cout << "POST request without Content-Length or Transfer-Encoding header"
                          << std::endl;
            }
            if (sock) {
                sendHeader(411, 0, "text/html", keep_alive);
                sock->write_line("<html><body>411 Length Required</body></html>");
            }
            return;
        }

        // Parse Content-Length
        int content_length = 0;
        try {
            content_length = std::stoi(content_length_it->second);
        } catch (const std::exception& e) {
            if (DEBUG) {
                std::cout << "Invalid Content-Length value: " << content_length_it->second
                          << std::endl;
            }
            if (sock) {
                sendHeader(400, 0, "text/html", keep_alive);
                sock->write_line(
                    "<html><body>400 Bad Request - Invalid Content-Length</body></html>");
            }
            return;
        }

        // Check for negative or excessively large content length
        const int MAX_POST_SIZE = 10 * 1024 * 1024; // 10MB max
        if (content_length < 0 || content_length > MAX_POST_SIZE) {
            if (DEBUG) {
                std::cout << "Invalid Content-Length: " << content_length << std::endl;
            }
            if (sock) {
                sendHeader(413, 0, "text/html", keep_alive);
                sock->write_line("<html><body>413 Request Entity Too Large</body></html>");
            }
            return;
        }

        // Read the POST body
        std::vector<char> post_body(content_length);
        if (content_length > 0) {
            ssize_t bytes_read = sock->read_raw(post_body.data(), content_length);
            if (bytes_read != content_length) {
                if (DEBUG) {
                    std::cout << "Failed to read complete POST body. Expected: " << content_length
                              << ", Read: " << bytes_read << std::endl;
                }
                if (sock) {
                    // Check if the error was a timeout
                    if (bytes_read < 0 && sock->is_timeout_error()) {
                        sendHeader(408, 0, "text/html", false);
                        sock->write_line("<html><body>408 Request Timeout - Client too "
                                         "slow sending body</body></html>");
                    } else {
                        sendHeader(400, 0, "text/html", keep_alive);
                        sock->write_line(
                            "<html><body>400 Bad Request - Incomplete POST body</body></html>");
                    }
                }
                return;
            }
        }

        // Convert to string for processing
        body_str = std::string(post_body.begin(), post_body.end());
    }

    if (DEBUG) {
        std::cout << "POST body (" << body_str.length() << " bytes): " << body_str << std::endl;
    }

    // Get the requested URI
    auto post_it = headermap.find("POST");
    if (post_it == headermap.end()) {
        return;
    }
    std::string uri = post_it->second;

    // Parse Content-Type
    std::string content_type;
    auto content_type_it = headermap.find("Content-Type");
    if (content_type_it != headermap.end()) {
        content_type = content_type_it->second;
    }

    // Parse the POST data based on content type
    std::map<std::string, std::string> post_params;

    if (content_type.find("application/x-www-form-urlencoded") != std::string::npos) {
        post_params = parseFormUrlEncoded(body_str);
    } else if (content_type.find("multipart/form-data") != std::string::npos) {
        std::string boundary = getBoundaryFromContentType(content_type);
        post_params = parseMultipartFormData(body_str, boundary);
    }
    // For other content types (like application/json), we keep the raw body

    // Check if this is a CGI request
    std::string filename = "htdocs" + uri;
    filename = sanitizeFilename(filename);

    // Check if file exists and is executable (CGI script)
    struct stat file_stat;
    if (stat(filename.c_str(), &file_stat) == 0 && (file_stat.st_mode & S_IXUSR)) {
        // This is a CGI script - we need to set up the environment and execute it
        // Create a new headermap with POST data for CGI
        std::map<std::string, std::string> cgi_headers = headermap;
        cgi_headers["REQUEST_METHOD"] = "POST";
        cgi_headers["CONTENT_LENGTH"] = std::to_string(body_str.length());
        if (!content_type.empty()) {
            cgi_headers["CONTENT_TYPE"] = content_type;
        }

        // For CGI, we need to pipe the POST body to stdin
        // This requires modifying the CGI handler to accept POST data
        // For now, we'll send a response indicating CGI POST is not yet fully implemented
        std::string response = "<html><body><h1>501 Not Implemented</h1>\n";
        response += "<p>POST to CGI scripts is not yet fully implemented.</p>\n";
        response += "</body></html>";

        sendHeader(501, response.length(), "text/html", keep_alive);
        sock->write_line(response);

        // Log the request
        Log& log = Log::getInstance();
        log.openLogFile("logs/access_log");
        auto referer_it = headermap.find("Referer");
        auto user_agent_it = headermap.find("User-Agent");
        log.writeLogLine(inet_ntoa(sock->client.sin_addr), "POST " + uri, 501, response.length(),
                         referer_it != headermap.end() ? referer_it->second : "",
                         user_agent_it != headermap.end() ? user_agent_it->second : "");
        return;
    }

    // For now, return a response showing the parsed POST data
    std::string response = "<html><body><h1>POST Request Received</h1>\n";
    response += "<p>URI: " + uri + "</p>\n";
    response += "<p>Content-Length: " + std::to_string(body_str.length()) + "</p>\n";
    response += "<p>Content-Type: " + content_type + "</p>\n";

    if (!post_params.empty()) {
        response += "<h2>Parsed Form Data:</h2>\n<table border='1'>\n";
        response += "<tr><th>Field</th><th>Value</th></tr>\n";
        for (const auto& [key, value] : post_params) {
            response += "<tr><td>" + key + "</td><td>" + value + "</td></tr>\n";
        }
        response += "</table>\n";
    } else {
        response += "<h2>Raw Body:</h2><pre>" + body_str + "</pre>\n";
    }

    response += "</body></html>";

    // Send response
    sendHeader(200, response.length(), "text/html", keep_alive);
    sock->write_line(response);

    // Log the request
    Log& log = Log::getInstance();
    log.openLogFile("logs/access_log");
    auto referer_it = headermap.find("Referer");
    auto user_agent_it = headermap.find("User-Agent");
    log.writeLogLine(inet_ntoa(sock->client.sin_addr), "POST " + uri, 200, response.length(),
                     referer_it != headermap.end() ? referer_it->second : "",
                     user_agent_it != headermap.end() ? user_agent_it->second : "");
}

/**
 * Handle HTTP PUT request - create or update a resource
 */
void Http::processPutRequest(const std::map<std::string, std::string>& headermap,
                             std::string_view request_line, bool keep_alive) {
    auto put_it = headermap.find("PUT");
    if (put_it == headermap.end()) {
        return;
    }

    std::string uri = put_it->second;
    std::string filename = sanitizeFilename(uri);

    // Check for Transfer-Encoding header
    auto transfer_encoding_it = headermap.find("Transfer-Encoding");
    bool is_chunked = false;
    if (transfer_encoding_it != headermap.end() &&
        transfer_encoding_it->second.find("chunked") != std::string::npos) {
        is_chunked = true;
    }

    std::string body;

    if (is_chunked) {
        // Read chunked body
        body = readChunkedBody();
        // Note: empty body is valid for chunked encoding
        // readChunkedBody() sends error responses if needed
    } else {
        // Check if Content-Length is present
        auto content_length_it = headermap.find("Content-Length");
        if (content_length_it == headermap.end()) {
            sendHeader(411, 0, "text/plain", keep_alive);
            return;
        }

        // Read the request body
        int content_length = std::stoi(content_length_it->second);
        if (content_length < 0 || content_length > 10 * 1024 * 1024) { // 10MB limit
            sendHeader(413, 0, "text/plain", keep_alive);
            return;
        }

        if (content_length > 0) {
            std::vector<char> buffer(content_length);
            ssize_t bytes_read = sock->read_raw(buffer.data(), content_length);
            if (bytes_read < content_length) {
                // Check if the error was a timeout
                if (bytes_read < 0 && sock->is_timeout_error()) {
                    sendHeader(408, 0, "text/plain", false);
                    sock->write_line("408 Request Timeout - Client too slow sending body\n");
                } else {
                    sendHeader(400, 0, "text/plain", keep_alive);
                    sock->write_line("400 Bad Request - Incomplete body\n");
                }
                return;
            }
            body = std::string(buffer.data(), content_length);
        }
    }

    // Check if file exists to determine response code
    bool file_exists = std::filesystem::exists(filename);

    // Write content to file
    std::ofstream outfile(filename, std::ios::binary | std::ios::trunc);
    if (!outfile.is_open()) {
        // Cannot write file - return 500 Internal Server Error
        std::string error_msg = "<html><head><title>500 Internal Server Error</title></head>"
                                "<body><h1>500 Internal Server Error</h1>"
                                "<p>Could not write to the specified resource.</p></body></html>";
        sendHeader(500, error_msg.length(), "text/html", keep_alive);
        sock->write_line(error_msg);
        return;
    }

    outfile.write(body.c_str(), body.length());
    outfile.close();

    // Return 201 Created if new file, 200 OK if updated
    int status_code = file_exists ? 200 : 201;

    if (status_code == 201) {
        // Include Location header for newly created resource
        std::vector<std::string> headers = {"Location: " + uri};
        sendHeader(201, 0, "text/plain", keep_alive, headers);
    } else {
        sendHeader(200, 0, "text/plain", keep_alive);
    }

    // Log the request
    Log& log = Log::getInstance();
    log.openLogFile("logs/access_log");
    auto referer_it = headermap.find("Referer");
    auto user_agent_it = headermap.find("User-Agent");
    log.writeLogLine(inet_ntoa(sock->client.sin_addr), std::string(request_line), status_code, 0,
                     referer_it != headermap.end() ? referer_it->second : "",
                     user_agent_it != headermap.end() ? user_agent_it->second : "");
}

/**
 * Handle HTTP DELETE request - delete a resource
 */
void Http::processDeleteRequest(const std::map<std::string, std::string>& headermap,
                                std::string_view request_line, bool keep_alive) {
    auto delete_it = headermap.find("DELETE");
    if (delete_it == headermap.end()) {
        return;
    }

    std::string uri = delete_it->second;
    std::string filename = sanitizeFilename(uri);

    // Check if file exists
    if (!std::filesystem::exists(filename)) {
        std::string error_msg = "<html><head><title>404 Not Found</title></head>"
                                "<body><h1>404 Not Found</h1>"
                                "<p>The requested resource does not exist.</p></body></html>";
        sendHeader(404, error_msg.length(), "text/html", keep_alive);
        sock->write_line(error_msg);

        // Log the request
        Log& log = Log::getInstance();
        log.openLogFile("logs/access_log");
        auto referer_it = headermap.find("Referer");
        auto user_agent_it = headermap.find("User-Agent");
        log.writeLogLine(inet_ntoa(sock->client.sin_addr), std::string(request_line), 404, 0,
                         referer_it != headermap.end() ? referer_it->second : "",
                         user_agent_it != headermap.end() ? user_agent_it->second : "");
        return;
    }

    // Delete the file
    std::error_code ec;
    bool removed = std::filesystem::remove(filename, ec);

    if (!removed || ec) {
        // Could not delete file - return 500 Internal Server Error
        std::string error_msg = "<html><head><title>500 Internal Server Error</title></head>"
                                "<body><h1>500 Internal Server Error</h1>"
                                "<p>Could not delete the specified resource.</p></body></html>";
        sendHeader(500, error_msg.length(), "text/html", keep_alive);
        sock->write_line(error_msg);
        return;
    }

    // Return 204 No Content on successful deletion
    sendHeader(204, 0, "text/plain", keep_alive);

    // Log the request
    Log& log = Log::getInstance();
    log.openLogFile("logs/access_log");
    auto referer_it = headermap.find("Referer");
    auto user_agent_it = headermap.find("User-Agent");
    log.writeLogLine(inet_ntoa(sock->client.sin_addr), std::string(request_line), 204, 0,
                     referer_it != headermap.end() ? referer_it->second : "",
                     user_agent_it != headermap.end() ? user_agent_it->second : "");
}

/**
 * Read a chunked request body according to RFC 7230
 * Returns the complete body with chunks decoded
 */
std::string Http::readChunkedBody() {
    std::string body;
    std::string line;

    while (true) {
        // Read chunk size line (hex number followed by CRLF)
        if (!sock->read_line(&line)) {
            break;
        }

        // Parse chunk size (ignore any chunk extensions after semicolon)
        size_t semicolon_pos = line.find(';');
        std::string size_str =
            semicolon_pos != std::string::npos ? line.substr(0, semicolon_pos) : line;

        // Remove whitespace
        size_str.erase(0, size_str.find_first_not_of(" \t"));
        size_str.erase(size_str.find_last_not_of(" \t\r\n") + 1);

        // Convert hex string to integer
        size_t chunk_size;
        try {
            chunk_size = std::stoull(size_str, nullptr, 16);
        } catch (...) {
            // Invalid chunk size - send 400 Bad Request
            std::string error_msg = "400 Bad Request - Invalid chunk size\n";
            sendHeader(400, error_msg.length(), "text/plain");
            sock->write_line(error_msg);
            return "";
        }

        // Chunk size of 0 indicates end of chunks
        if (chunk_size == 0) {
            // Read trailing headers (if any) until empty line
            while (true) {
                if (!sock->read_line(&line)) {
                    break;
                }
                if (line.empty() || line == "\r\n" || line == "\n") {
                    break;
                }
            }
            break;
        }

        // Read chunk data
        std::vector<char> chunk_data(chunk_size);
        ssize_t bytes_read = sock->read_raw(chunk_data.data(), chunk_size);
        if (bytes_read < static_cast<ssize_t>(chunk_size)) {
            // Incomplete chunk - send 400 Bad Request
            std::string error_msg = "400 Bad Request - Incomplete chunk data\n";
            sendHeader(400, error_msg.length(), "text/plain");
            sock->write_line(error_msg);
            return "";
        }

        body.append(chunk_data.data(), chunk_size);

        // Read trailing CRLF after chunk data
        sock->read_line(&line);
    }

    return body;
}

/**
 * Write data as a chunked chunk
 */
void Http::writeChunkedData(std::string_view data) {
    if (data.empty()) {
        return;
    }

    // Write chunk size in hex followed by CRLF
    std::string chunk_header = std::format("{:x}\r\n", data.size());
    sock->write_raw(chunk_header.c_str(), chunk_header.length());

    // Write chunk data
    sock->write_raw(data.data(), data.size());

    // Write trailing CRLF
    sock->write_raw("\r\n", 2);
}

/**
 * Write the final 0-sized chunk to end chunked encoding
 */
void Http::writeChunkedEnd() {
    // Send 0-sized chunk followed by CRLF
    sock->write_raw("0\r\n\r\n", 5);
}

/**
 * Parse application/x-www-form-urlencoded data
 */
std::map<std::string, std::string> Http::parseFormUrlEncoded(const std::string& body) {
    std::map<std::string, std::string> params;
    std::string key, value;
    bool parsing_key = true;

    for (size_t i = 0; i < body.length(); ++i) {
        char c = body[i];

        if (c == '=') {
            parsing_key = false;
        } else if (c == '&') {
            // URL decode and store the key-value pair
            if (!key.empty()) {
                // Simple URL decoding (replace + with space, decode %XX)
                std::string decoded_key = key;
                std::string decoded_value = value;

                // Replace + with space
                size_t pos = 0;
                while ((pos = decoded_key.find('+', pos)) != std::string::npos) {
                    decoded_key[pos] = ' ';
                    pos++;
                }
                pos = 0;
                while ((pos = decoded_value.find('+', pos)) != std::string::npos) {
                    decoded_value[pos] = ' ';
                    pos++;
                }

                // Decode %XX sequences
                auto url_decode = [](std::string& str) {
                    size_t pos = 0;
                    while ((pos = str.find('%', pos)) != std::string::npos) {
                        if (pos + 2 < str.length()) {
                            std::string hex = str.substr(pos + 1, 2);
                            try {
                                int value = std::stoi(hex, nullptr, 16);
                                str.replace(pos, 3, 1, static_cast<char>(value));
                            } catch (...) {
                                // Invalid hex sequence, skip
                                pos++;
                                continue;
                            }
                        }
                        pos++;
                    }
                };

                url_decode(decoded_key);
                url_decode(decoded_value);

                params[decoded_key] = decoded_value;
            }

            // Reset for next pair
            key.clear();
            value.clear();
            parsing_key = true;
        } else {
            if (parsing_key) {
                key += c;
            } else {
                value += c;
            }
        }
    }

    // Don't forget the last pair
    if (!key.empty()) {
        // URL decode the last pair
        std::string decoded_key = key;
        std::string decoded_value = value;

        // Replace + with space
        size_t pos = 0;
        while ((pos = decoded_key.find('+', pos)) != std::string::npos) {
            decoded_key[pos] = ' ';
            pos++;
        }
        pos = 0;
        while ((pos = decoded_value.find('+', pos)) != std::string::npos) {
            decoded_value[pos] = ' ';
            pos++;
        }

        // Decode %XX sequences
        auto url_decode = [](std::string& str) {
            size_t pos = 0;
            while ((pos = str.find('%', pos)) != std::string::npos) {
                if (pos + 2 < str.length()) {
                    std::string hex = str.substr(pos + 1, 2);
                    try {
                        int value = std::stoi(hex, nullptr, 16);
                        str.replace(pos, 3, 1, static_cast<char>(value));
                    } catch (...) {
                        // Invalid hex sequence, skip
                        pos++;
                        continue;
                    }
                }
                pos++;
            }
        };

        url_decode(decoded_key);
        url_decode(decoded_value);

        params[decoded_key] = decoded_value;
    }

    return params;
}

/**
 * Extract boundary from Content-Type header
 */
std::string Http::getBoundaryFromContentType(const std::string& content_type) {
    size_t boundary_pos = content_type.find("boundary=");
    if (boundary_pos == std::string::npos) {
        return "";
    }

    std::string boundary = content_type.substr(boundary_pos + 9);

    // Remove quotes if present
    if (!boundary.empty() && boundary.front() == '"') {
        boundary = boundary.substr(1);
    }
    if (!boundary.empty() && boundary.back() == '"') {
        boundary.pop_back();
    }

    return boundary;
}

/**
 * Parse multipart/form-data
 * This is a simplified parser that handles basic text fields
 */
std::map<std::string, std::string> Http::parseMultipartFormData(const std::string& body,
                                                                const std::string& boundary) {
    std::map<std::string, std::string> params;

    if (boundary.empty()) {
        return params;
    }

    std::string delimiter = "--" + boundary;
    std::string end_delimiter = "--" + boundary + "--";

    size_t pos = 0;
    size_t end_pos = body.find(end_delimiter);

    while ((pos = body.find(delimiter, pos)) != std::string::npos && pos < end_pos) {
        pos += delimiter.length();

        // Skip CRLF after boundary
        if (pos + 2 <= body.length() && body.substr(pos, 2) == "\r\n") {
            pos += 2;
        }

        // Find headers end
        size_t headers_end = body.find("\r\n\r\n", pos);
        if (headers_end == std::string::npos) {
            break;
        }

        // Parse headers
        std::string headers = body.substr(pos, headers_end - pos);
        std::string name;

        // Extract name from Content-Disposition header
        size_t name_pos = headers.find("name=\"");
        if (name_pos != std::string::npos) {
            name_pos += 6;
            size_t name_end = headers.find("\"", name_pos);
            if (name_end != std::string::npos) {
                name = headers.substr(name_pos, name_end - name_pos);
            }
        }

        // Move to content start
        pos = headers_end + 4;

        // Find next boundary
        size_t content_end = body.find("\r\n" + delimiter, pos);
        if (content_end == std::string::npos) {
            break;
        }

        // Extract content
        std::string content = body.substr(pos, content_end - pos);

        if (!name.empty()) {
            params[name] = content;
        }

        pos = content_end + 2;
    }

    return params;
}

/**
 * Parse cookies from Cookie header
 * @param cookie_header The value of the Cookie header
 * @return Map of cookie name to value
 */
std::map<std::string, std::string> Http::parseCookies(const std::string& cookie_header) {
    std::map<std::string, std::string> cookies;

    if (cookie_header.empty()) {
        return cookies;
    }

    // Split by semicolon and optional space
    size_t start = 0;
    size_t end = 0;

    while (start < cookie_header.length()) {
        // Skip whitespace
        while (start < cookie_header.length() && std::isspace(cookie_header[start])) {
            start++;
        }

        // Find next semicolon
        end = cookie_header.find(';', start);
        if (end == std::string::npos) {
            end = cookie_header.length();
        }

        // Extract cookie pair
        std::string cookie_pair = cookie_header.substr(start, end - start);

        // Find equals sign
        size_t equals_pos = cookie_pair.find('=');
        if (equals_pos != std::string::npos) {
            std::string name = cookie_pair.substr(0, equals_pos);
            std::string value = cookie_pair.substr(equals_pos + 1);

            // Trim whitespace from name and value
            name.erase(0, name.find_first_not_of(" \t"));
            name.erase(name.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // Remove quotes if present
            if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.length() - 2);
            }

            cookies[name] = value;
        }

        start = end + 1;
    }

    return cookies;
}

/**
 * Set a cookie to be sent in the response
 * @param name Cookie name
 * @param value Cookie value
 * @param path Cookie path (default "/")
 * @param max_age Max age in seconds (-1 for session cookie)
 * @param secure Whether cookie should only be sent over HTTPS
 * @param http_only Whether cookie should be inaccessible to JavaScript
 * @param same_site SameSite attribute ("Strict", "Lax", "None", or empty)
 */
void Http::setCookie(const std::string& name, const std::string& value, const std::string& path,
                     int max_age, bool secure, bool http_only, const std::string& same_site) {
    std::string cookie = name + "=" + value;

    if (!path.empty()) {
        cookie += "; Path=" + path;
    }

    if (max_age >= 0) {
        cookie += "; Max-Age=" + std::to_string(max_age);
    }

    if (secure) {
        cookie += "; Secure";
    }

    if (http_only) {
        cookie += "; HttpOnly";
    }

    if (!same_site.empty()) {
        cookie += "; SameSite=" + same_site;
    }

    response_cookies.push_back(cookie);
}

/**
 * Processes an HTTP OPTIONS request.
 * Returns allowed methods for the requested resource.
 * @param headermap A map containing parsed HTTP headers.
 * @param keep_alive Whether to keep the connection alive.
 */
void Http::processOptionsRequest(const std::map<std::string, std::string>& headermap,
                                 bool keep_alive) {
    auto it = headermap.find("OPTIONS");
    if (it == headermap.end())
        return;

    std::string uri = it->second;

    // For OPTIONS *, return server-wide capabilities
    // For specific URIs, return methods allowed for that resource

    // Send 200 OK with Allow header
    sendOptionsHeader(keep_alive);
}

/**
 * Processes an HTTP HEAD request.
 * @param headermap A map containing parsed HTTP headers.
 * @param keep_alive Whether to keep the connection alive.
 */
void Http::processHeadRequest(const std::map<std::string, std::string>& headermap,
                              bool keep_alive) {
    auto it = headermap.find("HEAD");
    if (it == headermap.end())
        return;

    std::string filename = sanitizeFilename(it->second);
    std::filesystem::path filepath(filename);
    std::string file_extension = filepath.extension().string();

    // Open file
    std::ifstream file(filename, std::ios::in | std::ios::binary);

    // Can't find the file, send 404 header
    if (!file.is_open()) {
        sendHeader(404, 0, "text/html", false);
        return;
    }

    // Check If-Modified-Since header
    auto if_modified_since_it = headermap.find("If-Modified-Since");
    if (if_modified_since_it != headermap.end()) {
        time_t since_time = parseHttpDate(if_modified_since_it->second);
        if (since_time > 0 && !isModifiedSince(filename, since_time)) {
            // File has not been modified, send 304
            sendHeader(304, 0, "", keep_alive);
            return;
        }
    }

    // Determine File Size
    file.seekg(0, std::ios::end);
    auto size = file.tellg();

    // Get MIME type
    Mime mime;
    mime.readMimeConfig("mime.types");
    std::string content_type = mime.getMimeFromExtension(filename);

    // Check for Range header
    auto range_it = headermap.find("Range");
    if (range_it != headermap.end()) {
        // If-Range support
        bool honor_range = true;
        auto if_range_it = headermap.find("If-Range");
        if (if_range_it != headermap.end()) {
            if (if_range_it->second.find("GMT") != std::string::npos) {
                time_t if_range_time = parseHttpDate(if_range_it->second);
                struct stat file_stat;
                if (stat(filename.c_str(), &file_stat) == 0) {
                    if (file_stat.st_mtime > if_range_time) {
                        honor_range = false;
                    }
                }
            } else {
                honor_range = false;
            }
        }

        if (honor_range) {
            std::vector<ByteRange> ranges = parseRangeHeader(range_it->second);
            if (ranges.size() == 1) {
                // For HEAD, we only handle single range requests
                long long start, end;
                if (validateRange(ranges[0], size, start, end)) {
                    long long content_length = end - start + 1;

                    // Log the range request
                    Log& log = Log::getInstance();
                    log.openLogFile("logs/access_log");
                    auto referer_it = headermap.find("Referer");
                    auto user_agent_it = headermap.find("User-Agent");
                    log.writeLogLine(inet_ntoa(sock->client.sin_addr), "HEAD " + filename, 206,
                                     content_length,
                                     referer_it != headermap.end() ? referer_it->second : "",
                                     user_agent_it != headermap.end() ? user_agent_it->second : "");

                    // Send 206 header with Content-Range
                    std::ostringstream headerStream;
                    headerStream << "HTTP/1.1 206 Partial Content\r\n";

                    std::array<char, 50> buf;
                    time_t ltime = time(nullptr);
                    struct tm* today = gmtime(&ltime);
                    strftime(buf.data(), buf.size(), "%a, %d %b %Y %H:%M:%S GMT", today);
                    headerStream << "Date: " << buf.data() << "\r\n";

                    headerStream << "Server: SHELOB/0.5 (Unix)\r\n";
                    headerStream << "Content-Type: " << content_type << "\r\n";
                    headerStream << "Content-Range: bytes " << start << "-" << end << "/" << size
                                 << "\r\n";
                    headerStream << "Content-Length: " << content_length << "\r\n";
                    headerStream << "Accept-Ranges: bytes\r\n";
                    headerStream << "Connection: " << (keep_alive ? "keep-alive" : "close")
                                 << "\r\n";
                    headerStream << "\r\n";

                    if (sock) {
                        sock->write_line(headerStream.str());
                    }
                    return;
                }
            } else if (ranges.size() > 1) {
                // Multiple ranges - would need multipart, just return 200 for HEAD
                // (Most clients don't use multiple ranges with HEAD anyway)
            }
        }
    }

    // No Range header or single range failed - send normal response
    // Log using singleton
    Log& log = Log::getInstance();
    log.openLogFile("logs/access_log");
    auto referer_it = headermap.find("Referer");
    auto user_agent_it = headermap.find("User-Agent");
    log.writeLogLine(inet_ntoa(sock->client.sin_addr), "HEAD " + filename, 200, size,
                     referer_it != headermap.end() ? referer_it->second : "",
                     user_agent_it != headermap.end() ? user_agent_it->second : "");

    // Send header
    sendHeader(200, size, content_type, keep_alive);
}

void Http::processGetRequest(const std::map<std::string, std::string>& headermap,
                             std::string_view request_line, bool keep_alive) {
    auto it = headermap.find("GET");

    if (it == headermap.end()) {
        return;
    }

    // Check authentication first
    if (!checkAuthentication(it->second, "GET", headermap, keep_alive)) {
        return; // Authentication failed, 401 already sent
    }

    // Handle cookie demo
    if (it->second == "/cookie-demo") {
        // Parse cookies from request
        std::map<std::string, std::string> cookies;
        auto cookie_it = headermap.find("Cookie");
        if (cookie_it != headermap.end()) {
            cookies = parseCookies(cookie_it->second);
        }

        // Generate response
        std::string response = "<html><head><title>Cookie Demo</title></head><body>\n";
        response += "<h1>Cookie Demo</h1>\n";

        // Check if we have a visit count cookie
        int visit_count = 1;
        auto count_it = cookies.find("visit_count");
        if (count_it != cookies.end()) {
            try {
                visit_count = std::stoi(count_it->second) + 1;
            } catch (...) {
                visit_count = 1;
            }
        }

        response += "<p>Visit count: " + std::to_string(visit_count) + "</p>\n";

        // Display all cookies
        response += "<h2>Current Cookies:</h2>\n";
        if (cookies.empty()) {
            response += "<p>No cookies set</p>\n";
        } else {
            response += "<ul>\n";
            for (const auto& [name, value] : cookies) {
                response += "<li>" + name + " = " + value + "</li>\n";
            }
            response += "</ul>\n";
        }

        response += "<h2>Actions:</h2>\n";
        response += "<ul>\n";
        response += "<li><a href='/cookie-demo'>Refresh (increment visit count)</a></li>\n";
        response +=
            "<li><a href='/set-cookie?name=user&value=john'>Set user=john cookie</a></li>\n";
        response +=
            "<li><a href='/set-cookie?name=theme&value=dark'>Set theme=dark cookie</a></li>\n";
        response += "<li><a href='/clear-cookies'>Clear all cookies</a></li>\n";
        response += "</ul>\n";
        response += "</body></html>";

        // Set visit count cookie
        setCookie("visit_count", std::to_string(visit_count), "/", 3600); // 1 hour

        // Send response
        sendHeader(200, response.length(), "text/html", keep_alive);
        sock->write_line(response);

        // Log request
        Log& log = Log::getInstance();
        log.openLogFile("logs/access_log");
        auto referer_it = headermap.find("Referer");
        auto user_agent_it = headermap.find("User-Agent");
        log.writeLogLine(inet_ntoa(sock->client.sin_addr), std::string(request_line), 200,
                         response.length(), referer_it != headermap.end() ? referer_it->second : "",
                         user_agent_it != headermap.end() ? user_agent_it->second : "");
        return;
    }

    // Handle set-cookie endpoint
    if (it->second.find("/set-cookie") == 0) {
        std::string query = it->second.substr(11); // Remove "/set-cookie"
        std::string cookie_name = "test";
        std::string cookie_value = "value";

        // Parse query parameters (simple implementation)
        if (query.length() > 1 && query[0] == '?') {
            query = query.substr(1);
            size_t name_pos = query.find("name=");
            size_t value_pos = query.find("value=");

            if (name_pos != std::string::npos) {
                size_t end = query.find('&', name_pos);
                cookie_name =
                    query.substr(name_pos + 5, end != std::string::npos ? end - (name_pos + 5)
                                                                        : std::string::npos);
            }

            if (value_pos != std::string::npos) {
                size_t end = query.find('&', value_pos);
                cookie_value =
                    query.substr(value_pos + 6, end != std::string::npos ? end - (value_pos + 6)
                                                                         : std::string::npos);
            }
        }

        // Set the cookie
        setCookie(cookie_name, cookie_value, "/", 3600); // 1 hour

        // Redirect back to demo
        std::string response =
            "<html><head><meta http-equiv='refresh' content='0;url=/cookie-demo'></head>";
        response += "<body>Setting cookie and redirecting...</body></html>";

        sendHeader(200, response.length(), "text/html", keep_alive);
        sock->write_line(response);

        // Log request
        Log& log = Log::getInstance();
        log.openLogFile("logs/access_log");
        auto referer_it = headermap.find("Referer");
        auto user_agent_it = headermap.find("User-Agent");
        log.writeLogLine(inet_ntoa(sock->client.sin_addr), std::string(request_line), 200,
                         response.length(), referer_it != headermap.end() ? referer_it->second : "",
                         user_agent_it != headermap.end() ? user_agent_it->second : "");
        return;
    }

    // Handle clear-cookies endpoint
    if (it->second == "/clear-cookies") {
        // Set all cookies to expire
        setCookie("visit_count", "", "/", 0);
        setCookie("user", "", "/", 0);
        setCookie("theme", "", "/", 0);

        // Redirect back to demo
        std::string response =
            "<html><head><meta http-equiv='refresh' content='0;url=/cookie-demo'></head>";
        response += "<body>Clearing cookies and redirecting...</body></html>";

        sendHeader(200, response.length(), "text/html", keep_alive);
        sock->write_line(response);

        // Log request
        Log& log = Log::getInstance();
        log.openLogFile("logs/access_log");
        auto referer_it = headermap.find("Referer");
        auto user_agent_it = headermap.find("User-Agent");
        log.writeLogLine(inet_ntoa(sock->client.sin_addr), std::string(request_line), 200,
                         response.length(), referer_it != headermap.end() ? referer_it->second : "",
                         user_agent_it != headermap.end() ? user_agent_it->second : "");
        return;
    }

    std::string filename = sanitizeFilename(it->second);
    std::filesystem::path filepath(filename);
    std::string file_extension = filepath.extension().string();

    // Try content negotiation if Accept header is present
    std::vector<std::string> extra_headers;
    auto accept_it = headermap.find("Accept");

    if (accept_it != headermap.end()) {
        // Remove extension from path to find base path for variants
        std::string base_path = filename;
        if (!file_extension.empty() && file_extension.length() < filename.length()) {
            base_path = filename.substr(0, filename.length() - file_extension.length());
        }

        // Try to find variants for the base path
        auto variants = content_negotiator.findVariants(base_path);

        if (!variants.empty()) {
            // Variants exist, try content negotiation
            std::string best_match =
                content_negotiator.selectBestMatch(base_path, accept_it->second);

            if (!best_match.empty()) {
                // Found an acceptable variant
                filename = best_match;
                filepath = std::filesystem::path(filename);
                file_extension = filepath.extension().string();
                extra_headers.push_back("Vary: Accept");
            } else {
                // No acceptable variant - return 406 Not Acceptable
                std::string error_msg = "<html><head><title>406 Not Acceptable</title></head>"
                                        "<body><h1>406 Not Acceptable</h1>"
                                        "<p>The requested resource is not available in a format "
                                        "acceptable to your client.</p></body></html>";
                sendHeader(406, error_msg.length(), "text/html", keep_alive);
                sock->write_line(error_msg);
                return;
            }
        }
    }

    // Open file
    std::ifstream file(filename, std::ios::in | std::ios::binary);

    // can't find file, 404 it
    if (!file.is_open()) {
        sendHeader(404, 0, "text/html", false);
        sock->write_line("<html><head><title>404</title></head><body>404 not "
                         "found</body></html>");
        // sock->close_socket();
        return;
    }

    // Check If-Modified-Since header
    auto if_modified_since_it = headermap.find("If-Modified-Since");
    if (if_modified_since_it != headermap.end()) {
        time_t since_time = parseHttpDate(if_modified_since_it->second);
        if (since_time > 0 && !isModifiedSince(filename, since_time)) {
            // File has not been modified, send 304
            sendHeader(304, 0, "", keep_alive);
            return;
        }
    }

    // Determine File Size
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (file_extension == ".sh") {
        Cgi cgi;
        cgi.executeCGI(filename, sock->accept_fd_, headermap);
        sock->close_socket();
        return;
    }

    // Get MIME type
    Mime mime;
    mime.readMimeConfig("mime.types");
    std::string content_type = mime.getMimeFromExtension(filename);

    // Check for Range header
    auto range_it = headermap.find("Range");
    if (range_it != headermap.end()) {
        // If-Range support: only honor Range if If-Range conditions match
        bool honor_range = true;
        auto if_range_it = headermap.find("If-Range");
        if (if_range_it != headermap.end()) {
            // If-Range can be either an ETag or a date
            // For simplicity, we'll check if it's a date (contains spaces/GMT)
            if (if_range_it->second.find("GMT") != std::string::npos) {
                // It's a date - check if file was modified
                time_t if_range_time = parseHttpDate(if_range_it->second);
                struct stat file_stat;
                if (stat(filename.c_str(), &file_stat) == 0) {
                    if (file_stat.st_mtime > if_range_time) {
                        honor_range = false; // File modified, return full content
                    }
                }
            }
            // For ETags, we'd need to implement ETag generation and comparison
            // For now, we'll assume ETag matching fails and return full content
            else {
                honor_range = false;
            }
        }

        if (honor_range) {
            // Parse and handle Range request
            std::vector<ByteRange> ranges = parseRangeHeader(range_it->second);
            if (!ranges.empty()) {
                // Log the range request
                Log& log = Log::getInstance();
                log.openLogFile("logs/access_log");
                auto referer_it = headermap.find("Referer");
                auto user_agent_it = headermap.find("User-Agent");
                log.writeLogLine(inet_ntoa(sock->client.sin_addr), std::string(request_line), 206,
                                 0, referer_it != headermap.end() ? referer_it->second : "",
                                 user_agent_it != headermap.end() ? user_agent_it->second : "");

                // Send partial content
                sendPartialContent(filename, ranges, size, content_type, keep_alive);
                return;
            }
        }
    }

    // No Range header or If-Range failed - send full file
    // Log using singleton
    Log& log = Log::getInstance();
    log.openLogFile("logs/access_log");
    auto referer_it = headermap.find("Referer");
    auto user_agent_it = headermap.find("User-Agent");
    log.writeLogLine(inet_ntoa(sock->client.sin_addr), std::string(request_line), 200, size,
                     referer_it != headermap.end() ? referer_it->second : "",
                     user_agent_it != headermap.end() ? user_agent_it->second : "");

    /* BUG: There is a possible bug where the file size changes between sending
       the header and when the file is actually sent through the socket.
    */
    sendHeader(200, size, content_type, keep_alive, extra_headers);

    // Use middleware if available, otherwise use legacy sendFile
    if (middleware_chain) {
        sendFileWithMiddleware(filename, "GET", it->second, "HTTP/1.1", headermap);
    } else {
        sendFile(filename);
    }
}

/**
 * Receive the client's request headers
 */
std::string Http::getHeader(bool use_timeout) {
    // For testing, return the last header sent
    if (!sock)
        return lastHeader;

    std::string clientBuffer;
    std::string line;

    // For keep-alive connections, use a timeout
    const int KEEPALIVE_TIMEOUT = 5; // 5 seconds timeout

    // Read headers line by line until we get an empty line
    bool read_success;
    if (use_timeout) {
        read_success = sock->read_line_with_timeout(&line, KEEPALIVE_TIMEOUT);
    } else {
        read_success = sock->read_line(&line);
    }

    while (read_success) {
        if (DEBUG) {
            std::cout << "DEBUG getHeader: Read line [" << line << "]" << std::endl;
        }
        clientBuffer += line;

        // Check for end of headers (empty line)
        if (line == "\n" || line == "\r\n") {
            break;
        }

        // Also check if we've accumulated the end pattern
        if (clientBuffer.find("\r\n\r\n") != std::string::npos ||
            clientBuffer.find("\n\n") != std::string::npos) {
            break;
        }

        line.clear();
        if (use_timeout) {
            read_success = sock->read_line_with_timeout(&line, KEEPALIVE_TIMEOUT);
        } else {
            read_success = sock->read_line(&line);
        }
    }

    return clientBuffer;
}

/**
 * Send HTTP headers to the client
 */
void Http::sendHeader(int code, int size, std::string_view file_type, bool keep_alive,
                      const std::vector<std::string>& extra_headers) {
    assert(code > 99 && code < 600);
    assert(size >= 0);

    std::ostringstream headerStream;

    switch (code) {
    case 200:
        headerStream << "HTTP/1.1 200 OK\r\n";
        break;
    case 201:
        headerStream << "HTTP/1.1 201 Created\r\n";
        break;
    case 204:
        headerStream << "HTTP/1.1 204 No Content\r\n";
        break;
    case 301:
        headerStream << "HTTP/1.1 301 Moved Permanently\r\n";
        break;
    case 302:
        headerStream << "HTTP/1.1 302 Found\r\n";
        break;
    case 303:
        headerStream << "HTTP/1.1 303 See Other\r\n";
        break;
    case 304:
        headerStream << "HTTP/1.1 304 Not Modified\r\n";
        break;
    case 307:
        headerStream << "HTTP/1.1 307 Temporary Redirect\r\n";
        break;
    case 308:
        headerStream << "HTTP/1.1 308 Permanent Redirect\r\n";
        break;
    case 206:
        headerStream << "HTTP/1.1 206 Partial Content\r\n";
        break;
    case 400:
        headerStream << "HTTP/1.1 400 Bad Request\r\n";
        break;
    case 401:
        headerStream << "HTTP/1.1 401 Unauthorized\r\n";
        break;
    case 403:
        headerStream << "HTTP/1.1 403 Forbidden\r\n";
        break;
    case 416:
        headerStream << "HTTP/1.1 416 Range Not Satisfiable\r\n";
        break;
    case 404:
        headerStream << "HTTP/1.1 404 Not Found\r\n";
        break;
    case 406:
        headerStream << "HTTP/1.1 406 Not Acceptable\r\n";
        break;
    case 408:
        headerStream << "HTTP/1.1 408 Request Timeout\r\n";
        break;
    case 411:
        headerStream << "HTTP/1.1 411 Length Required\r\n";
        break;
    case 413:
        headerStream << "HTTP/1.1 413 Request Entity Too Large\r\n";
        break;
    case 501:
        headerStream << "HTTP/1.1 501 Not Implemented\r\n";
        break;
    case 505:
        headerStream << "HTTP/1.1 505 HTTP Version Not Supported\r\n";
        break;
    default:
        std::cerr << "Wrong HTTP CODE!" << std::endl;
        headerStream << "HTTP/1.1 500 Internal Server Error\r\n";
    }

    // Generate date header
    std::array<char, 50> buf;
    time_t ltime = time(nullptr);
    struct tm* today = gmtime(&ltime);
    strftime(buf.data(), buf.size(), "%a, %d %b %Y %H:%M:%S GMT", today);
    headerStream << "Date: " << buf.data() << "\r\n";

    headerStream << "Server: SHELOB/0.5 (Unix)\r\n";

    if (size != 0)
        headerStream << "Content-Length: " << size << "\r\n";

    headerStream << "Connection: " << (keep_alive ? "keep-alive" : "close") << "\r\n";
    headerStream << "Content-Type: " << file_type << "\r\n";

    // Add Accept-Ranges header for 200 OK responses
    if (code == 200) {
        headerStream << "Accept-Ranges: bytes\r\n";
    }

    // Add Set-Cookie headers
    for (const auto& cookie : response_cookies) {
        headerStream << "Set-Cookie: " << cookie << "\r\n";
    }

    // Add extra headers
    for (const auto& header : extra_headers) {
        headerStream << header << "\r\n";
    }

    headerStream << "\r\n";

    lastHeader = headerStream.str();

    if (sock) {
        sock->write_line(lastHeader);
    }
}

/**
 * Send HTTP redirect response with Location header
 */
void Http::sendRedirect(int code, const std::string& location, bool keep_alive) {
    // Validate that code is a redirect status code
    if (code < 300 || code >= 400) {
        std::cerr << "Warning: sendRedirect called with non-3xx status code: " << code << std::endl;
    }

    // Create extra headers with Location
    std::vector<std::string> headers = {"Location: " + location};

    // Send minimal HTML body for clients that don't follow redirects
    std::string body = std::format("<!DOCTYPE html>\n"
                                   "<html>\n"
                                   "<head><title>Redirect</title></head>\n"
                                   "<body><p>Redirecting to <a href=\"{}\">{}</a></p></body>\n"
                                   "</html>\n",
                                   location, location);

    sendHeader(code, body.length(), "text/html", keep_alive, headers);

    if (sock) {
        sock->write_line(body);
    }
}

/**
 * Send HTTP OPTIONS response headers to the client
 */
void Http::sendOptionsHeader(bool keep_alive) {
    std::ostringstream headerStream;

    // Status line
    headerStream << "HTTP/1.1 200 OK\r\n";

    // Generate date header
    std::array<char, 50> buf;
    time_t ltime = time(nullptr);
    struct tm* today = gmtime(&ltime);
    strftime(buf.data(), buf.size(), "%a, %d %b %Y %H:%M:%S GMT", today);
    headerStream << "Date: " << buf.data() << "\r\n";

    // Server header
    headerStream << "Server: SHELOB/0.5 (Unix)\r\n";

    // Allow header - list supported methods
    headerStream << "Allow: GET, HEAD, POST, OPTIONS\r\n";

    // Content-Length must be 0 for OPTIONS
    headerStream << "Content-Length: 0\r\n";

    // Connection header
    headerStream << "Connection: " << (keep_alive ? "keep-alive" : "close") << "\r\n";

    // Empty line to end headers
    headerStream << "\r\n";

    lastHeader = headerStream.str();

    if (sock) {
        sock->write_line(lastHeader);
    }
}

/**
 * Format time_t as HTTP date string
 */
std::string Http::formatHttpDate(time_t time) {
    std::array<char, 50> buf;
    struct tm* gmt = gmtime(&time);
    strftime(buf.data(), buf.size(), "%a, %d %b %Y %H:%M:%S GMT", gmt);
    return std::string(buf.data());
}

/**
 * Check authentication for a path
 * Returns true if authentication passed or path is not protected
 * Returns false and sends 401 response if authentication failed
 */
bool Http::checkAuthentication(const std::string& path, const std::string& method,
                               const std::map<std::string, std::string>& headermap,
                               bool keep_alive) {
    // Check if this path is protected
    std::string realm;
    if (!auth.is_protected(path, realm)) {
        return true; // Not protected, allow access
    }

    // Path is protected - check for Authorization header
    auto auth_header_it = headermap.find("Authorization");
    if (auth_header_it == headermap.end()) {
        // No auth header - send 401 challenge
        std::string challenge = auth.generate_basic_challenge(realm);

        std::vector<std::string> extra_headers;
        extra_headers.push_back("WWW-Authenticate: " + challenge);

        sendHeader(401, 0, "text/html", keep_alive, extra_headers);
        if (sock) {
            sock->write_line("<html><body><h1>401 Unauthorized</h1>"
                             "<p>This resource requires authentication.</p></body></html>");
        }
        return false;
    }

    // We have an auth header - validate it
    const std::string& auth_header = auth_header_it->second;

    bool authenticated = false;

    // Try Basic auth first
    if (auth_header.find("Basic ") == 0) {
        authenticated = auth.validate_basic_auth(auth_header);
    }
    // Try Digest auth
    else if (auth_header.find("Digest ") == 0) {
        authenticated = auth.validate_digest_auth(auth_header, method, path);
    }

    if (!authenticated) {
        // Authentication failed - send 401
        std::string challenge = auth.generate_basic_challenge(realm);

        std::vector<std::string> extra_headers;
        extra_headers.push_back("WWW-Authenticate: " + challenge);

        sendHeader(401, 0, "text/html", keep_alive, extra_headers);
        if (sock) {
            sock->write_line("<html><body><h1>401 Unauthorized</h1>"
                             "<p>Invalid credentials.</p></body></html>");
        }
        return false;
    }

    return true; // Authentication passed
}

/**
 * Parse Range header and return vector of ranges
 * Supports formats like:
 * - bytes=0-499 (single range)
 * - bytes=0-499,1000-1499 (multiple ranges)
 * - bytes=0- (from start to end)
 * - bytes=-500 (last 500 bytes - suffix range)
 * - bytes=500- (from 500 to end)
 */
std::vector<ByteRange> Http::parseRangeHeader(const std::string& range_header) {
    std::vector<ByteRange> ranges;

    // Check if header starts with "bytes="
    if (range_header.find("bytes=") != 0) {
        return ranges; // Invalid format
    }

    std::string ranges_str = range_header.substr(6); // Skip "bytes="

    // Split by comma to handle multiple ranges
    size_t pos = 0;
    while (pos < ranges_str.length()) {
        size_t comma_pos = ranges_str.find(',', pos);
        std::string range_spec = ranges_str.substr(pos, comma_pos - pos);

        // Trim whitespace
        range_spec.erase(0, range_spec.find_first_not_of(" \t"));
        range_spec.erase(range_spec.find_last_not_of(" \t\r\n") + 1);

        ByteRange range;
        range.start = -1;
        range.end = -1;
        range.is_suffix = false;

        size_t dash_pos = range_spec.find('-');
        if (dash_pos == std::string::npos) {
            // Invalid range format
            pos = (comma_pos == std::string::npos) ? ranges_str.length() : comma_pos + 1;
            continue;
        }

        std::string start_str = range_spec.substr(0, dash_pos);
        std::string end_str = range_spec.substr(dash_pos + 1);

        // Trim
        start_str.erase(0, start_str.find_first_not_of(" \t"));
        start_str.erase(start_str.find_last_not_of(" \t") + 1);
        end_str.erase(0, end_str.find_first_not_of(" \t"));
        end_str.erase(end_str.find_last_not_of(" \t") + 1);

        try {
            if (start_str.empty() && !end_str.empty()) {
                // Suffix range: -500 means last 500 bytes
                range.is_suffix = true;
                range.end = std::stoll(end_str);
            } else if (!start_str.empty()) {
                range.start = std::stoll(start_str);
                if (!end_str.empty()) {
                    range.end = std::stoll(end_str);
                }
                // else: start- format (end stays -1)
            }

            ranges.push_back(range);
        } catch (...) {
            // Invalid number format, skip this range
        }

        pos = (comma_pos == std::string::npos) ? ranges_str.length() : comma_pos + 1;
    }

    return ranges;
}

/**
 * Validate a range against file size and compute actual byte positions
 * Returns true if range is valid, false otherwise
 * Outputs actual start and end positions (inclusive)
 */
bool Http::validateRange(const ByteRange& range, long long file_size, long long& start,
                         long long& end) {
    if (range.is_suffix) {
        // Suffix range: -500 means last 500 bytes
        if (range.end <= 0) {
            return false;
        }
        start = std::max(0LL, file_size - range.end);
        end = file_size - 1;
        return true;
    }

    // Normal range
    if (range.start < 0) {
        return false; // Invalid
    }

    start = range.start;

    // If start is beyond file size, range is invalid
    if (start >= file_size) {
        return false;
    }

    // Compute end
    if (range.end < 0) {
        // Open-ended range: start-
        end = file_size - 1;
    } else {
        end = std::min(range.end, file_size - 1);
    }

    // End must be >= start
    if (end < start) {
        return false;
    }

    return true;
}

/**
 * Send partial content response (206)
 * Handles both single and multiple ranges
 */
void Http::sendPartialContent(std::string_view filename, const std::vector<ByteRange>& ranges,
                              long long file_size, std::string_view content_type, bool keep_alive) {
    // Validate all ranges first
    std::vector<std::pair<long long, long long>> valid_ranges;
    for (const auto& range : ranges) {
        long long start, end;
        if (validateRange(range, file_size, start, end)) {
            valid_ranges.push_back({start, end});
        }
    }

    if (valid_ranges.empty()) {
        // All ranges are invalid - send 416
        std::ostringstream headerStream;
        headerStream << "HTTP/1.1 416 Range Not Satisfiable\r\n";

        std::array<char, 50> buf;
        time_t ltime = time(nullptr);
        struct tm* today = gmtime(&ltime);
        strftime(buf.data(), buf.size(), "%a, %d %b %Y %H:%M:%S GMT", today);
        headerStream << "Date: " << buf.data() << "\r\n";

        headerStream << "Server: SHELOB/0.5 (Unix)\r\n";
        headerStream << "Content-Range: bytes */" << file_size << "\r\n";
        headerStream << "Content-Length: 0\r\n";
        headerStream << "Connection: " << (keep_alive ? "keep-alive" : "close") << "\r\n";
        headerStream << "\r\n";

        if (sock) {
            sock->write_line(headerStream.str());
        }
        return;
    }

    if (valid_ranges.size() == 1) {
        // Single range - send simple 206 response
        long long start = valid_ranges[0].first;
        long long end = valid_ranges[0].second;
        long long content_length = end - start + 1;

        std::ostringstream headerStream;
        headerStream << "HTTP/1.1 206 Partial Content\r\n";

        std::array<char, 50> buf;
        time_t ltime = time(nullptr);
        struct tm* today = gmtime(&ltime);
        strftime(buf.data(), buf.size(), "%a, %d %b %Y %H:%M:%S GMT", today);
        headerStream << "Date: " << buf.data() << "\r\n";

        headerStream << "Server: SHELOB/0.5 (Unix)\r\n";
        headerStream << "Content-Type: " << content_type << "\r\n";
        headerStream << "Content-Range: bytes " << start << "-" << end << "/" << file_size
                     << "\r\n";
        headerStream << "Content-Length: " << content_length << "\r\n";
        headerStream << "Accept-Ranges: bytes\r\n";
        headerStream << "Connection: " << (keep_alive ? "keep-alive" : "close") << "\r\n";
        headerStream << "\r\n";

        if (sock) {
            sock->write_line(headerStream.str());

            // Send the requested byte range
            std::ifstream file(filename.data(), std::ios::in | std::ios::binary);
            if (file) {
                file.seekg(start);
                std::vector<char> buffer(content_length);
                if (file.read(buffer.data(), content_length)) {
                    sock->write_raw(buffer.data(), content_length);
                }
            }
        }
    } else {
        // Multiple ranges - use multipart/byteranges
        sendMultipartRanges(filename, ranges, file_size, content_type, keep_alive);
    }
}

/**
 * Send multipart/byteranges response for multiple ranges
 */
void Http::sendMultipartRanges(std::string_view filename, const std::vector<ByteRange>& ranges,
                               long long file_size, std::string_view content_type,
                               bool keep_alive) {
    // Generate boundary
    std::string boundary = "SHELOB_MULTIPART_BOUNDARY";

    // Validate all ranges and prepare data
    std::vector<std::pair<long long, long long>> valid_ranges;
    for (const auto& range : ranges) {
        long long start, end;
        if (validateRange(range, file_size, start, end)) {
            valid_ranges.push_back({start, end});
        }
    }

    if (valid_ranges.empty()) {
        // All ranges invalid - send 416
        sendPartialContent(filename, ranges, file_size, content_type, keep_alive);
        return;
    }

    // Build the multipart body
    std::ostringstream body;
    std::ifstream file(filename.data(), std::ios::in | std::ios::binary);
    if (!file) {
        sendHeader(404, 0, "text/html", keep_alive);
        if (sock) {
            sock->write_line("<html><body>404 Not Found</body></html>");
        }
        return;
    }

    for (const auto& [start, end] : valid_ranges) {
        long long content_length = end - start + 1;

        // Boundary
        body << "\r\n--" << boundary << "\r\n";
        body << "Content-Type: " << content_type << "\r\n";
        body << "Content-Range: bytes " << start << "-" << end << "/" << file_size << "\r\n";
        body << "\r\n";

        // Read and append data
        file.seekg(start);
        std::vector<char> buffer(content_length);
        if (file.read(buffer.data(), content_length)) {
            body.write(buffer.data(), content_length);
        }
    }

    // Final boundary
    body << "\r\n--" << boundary << "--\r\n";

    std::string body_str = body.str();

    // Send headers
    std::ostringstream headerStream;
    headerStream << "HTTP/1.1 206 Partial Content\r\n";

    std::array<char, 50> buf;
    time_t ltime = time(nullptr);
    struct tm* today = gmtime(&ltime);
    strftime(buf.data(), buf.size(), "%a, %d %b %Y %H:%M:%S GMT", today);
    headerStream << "Date: " << buf.data() << "\r\n";

    headerStream << "Server: SHELOB/0.5 (Unix)\r\n";
    headerStream << "Content-Type: multipart/byteranges; boundary=" << boundary << "\r\n";
    headerStream << "Content-Length: " << body_str.length() << "\r\n";
    headerStream << "Accept-Ranges: bytes\r\n";
    headerStream << "Connection: " << (keep_alive ? "keep-alive" : "close") << "\r\n";
    headerStream << "\r\n";

    if (sock) {
        sock->write_line(headerStream.str());
        sock->write_raw(body_str.data(), body_str.length());
    }
}

/**
 * Worker loop - handles connections in worker process
 */
void Http::worker_loop() {
    int requests_handled = 0;
    bool keep_alive = false;

    if (DEBUG) {
        std::cout << "Worker " << getpid() << " started" << std::endl;
    }

    while (requests_handled < max_requests_per_worker_) {
        // Accept connection (workers compete for this - kernel handles locking)
        sock->accept_client();

        if (DEBUG) {
            std::cout << "Worker " << getpid() << " accepted connection" << std::endl;
        }

        // Handle keep-alive loop
        keep_alive = parseHeader(getHeader(false));
        requests_handled++;

        while (keep_alive && requests_handled < max_requests_per_worker_) {
            std::string header = getHeader(true);
            if (header.empty()) {
                break;
            }
            keep_alive = parseHeader(header);
            requests_handled++;
        }

        sock->close_client();
    }

    if (DEBUG) {
        std::cout << "Worker " << getpid() << " exiting after " << requests_handled << " requests"
                  << std::endl;
    }
}

/**
 * Monitor and manage worker processes
 */
void Http::monitor_workers() {
    while (true) {
        int status;
        pid_t pid = wait(&status);

        if (pid > 0) {
            if (DEBUG) {
                std::cout << "Worker " << pid << " exited with status " << status << std::endl;
            }

            // Spawn replacement worker
            pid_t new_pid = fork();
            if (new_pid < 0) {
                perror("Fork replacement worker");
                continue;
            } else if (new_pid == 0) {
                // New worker process
                worker_loop();
                exit(0);
            } else {
                // Parent: replace PID in tracking vector
                for (auto& wpid : worker_pids_) {
                    if (wpid == pid) {
                        wpid = new_pid;
                        if (DEBUG) {
                            std::cout << "Replaced worker " << pid << " with " << new_pid
                                      << std::endl;
                        }
                        break;
                    }
                }
            }
        }
    }
}

/**
 * Cleanup all worker processes
 */
void Http::cleanup_workers() {
    for (pid_t pid : worker_pids_) {
        kill(pid, SIGTERM);
    }
    worker_pids_.clear();
}
