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
void Http::start(int server_port) {
    assert(server_port > 0 && server_port <= 65535);

    bool keep_alive = false;
    int pid;
    sock = std::make_unique<Socket>(server_port);

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
            keep_alive = parseHeader(getHeader(false));
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
        headermap.find("POST") == headermap.end() && headermap.find("OPTIONS") == headermap.end()) {
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
            headerStream << "Allow: GET, HEAD, POST, OPTIONS\r\n";
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
        } else if (headermap.find("OPTIONS") != headermap.end()) {
            processOptionsRequest(headermap, keep_alive);
        }
    }

    return keep_alive; // Return keep_alive status for connection handling
}

void Http::processPostRequest(const std::map<std::string, std::string>& headermap,
                              bool keep_alive) {
    // Check for Content-Length header (required for POST)
    auto content_length_it = headermap.find("Content-Length");
    if (content_length_it == headermap.end()) {
        if (DEBUG) {
            std::cout << "POST request without Content-Length header" << std::endl;
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
            std::cout << "Invalid Content-Length value: " << content_length_it->second << std::endl;
        }
        if (sock) {
            sendHeader(400, 0, "text/html", keep_alive);
            sock->write_line("<html><body>400 Bad Request - Invalid Content-Length</body></html>");
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
                sendHeader(400, 0, "text/html", keep_alive);
                sock->write_line(
                    "<html><body>400 Bad Request - Incomplete POST body</body></html>");
            }
            return;
        }
    }

    // Convert to string for processing
    std::string body_str(post_body.begin(), post_body.end());

    if (DEBUG) {
        std::cout << "POST body (" << content_length << " bytes): " << body_str << std::endl;
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
        cgi_headers["CONTENT_LENGTH"] = std::to_string(content_length);
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
    response += "<p>Content-Length: " + std::to_string(content_length) + "</p>\n";
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

    // Log using singleton
    Log& log = Log::getInstance();
    log.openLogFile("logs/access_log");
    auto referer_it = headermap.find("Referer");
    auto user_agent_it = headermap.find("User-Agent");
    log.writeLogLine(inet_ntoa(sock->client.sin_addr), "HEAD " + filename, 200, size,
                     referer_it != headermap.end() ? referer_it->second : "",
                     user_agent_it != headermap.end() ? user_agent_it->second : "");

    // TODO: Optimize Mime to be a singleton
    Mime mime;
    mime.readMimeConfig("mime.types");

    // Send header
    sendHeader(200, size, mime.getMimeFromExtension(filename), keep_alive);
}

void Http::processGetRequest(const std::map<std::string, std::string>& headermap,
                             std::string_view request_line, bool keep_alive) {
    auto it = headermap.find("GET");
    if (it == headermap.end())
        return;

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

    if (file_extension == ".sh") {
        Cgi cgi;
        cgi.executeCGI(filename, sock->accept_fd_, headermap);
        sock->close_socket();
        return;
    }

    // Log using singleton
    Log& log = Log::getInstance();
    log.openLogFile("logs/access_log");
    auto referer_it = headermap.find("Referer");
    auto user_agent_it = headermap.find("User-Agent");
    log.writeLogLine(inet_ntoa(sock->client.sin_addr), std::string(request_line), 200, size,
                     referer_it != headermap.end() ? referer_it->second : "",
                     user_agent_it != headermap.end() ? user_agent_it->second : "");

    // TODO: optimize Mime to be a singleton
    Mime mime;
    mime.readMimeConfig("mime.types");

    /* BUG: There is a possible bug where the file size changes between sending
       the header and when the file is actually sent through the socket.
    */
    sendHeader(200, size, mime.getMimeFromExtension(filename), keep_alive, extra_headers);

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
    case 304:
        headerStream << "HTTP/1.1 304 Not Modified\r\n";
        break;
    case 400:
        headerStream << "HTTP/1.1 400 Bad Request\r\n";
        break;
    case 404:
        headerStream << "HTTP/1.1 404 Not Found\r\n";
        break;
    case 406:
        headerStream << "HTTP/1.1 406 Not Acceptable\r\n";
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
