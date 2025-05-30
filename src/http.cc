#include "http.h"
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include "footer_middleware.h"
#include "logging_middleware.h"
#include "security_middleware.h"
#include "compression_middleware.h"

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
    if (tz) old_tz = tz;
    
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

    sock->writeLine(std::format("Date: {}\r\n", buf.data()));
}

/**
 * Write a RFC 2616 compliant Server header to the client.
 */
void Http::printServer() {
    if (sock)
        sock->writeLine("Server: SHELOB/0.5 (Unix)\r\n");
}

/**
 * Write a RFC 2616 compliant ContentType header to the client.
 */
void Http::printContentType(std::string_view type) {
    if (sock)
        sock->writeLine(std::format("Content-Type: {}\r\n", type));
}

/**
 * Write a RFC 2616 compliant ContentLength header to the client.
 */
void Http::printContentLength(int size) {
    assert(size >= 0);
    if (sock)
        sock->writeLine(std::format("Content-Length: {}\r\n", size));
}

/**
 * Write a RFC 2616 compliant ConnectionType header to the client.
 */
void Http::printConnectionType(bool keep_alive) {
    if (!sock)
        return;

    if (keep_alive)
        sock->writeLine("Connection: keep-alive\r\n");
    else
        sock->writeLine("Connection: close\r\n");
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
        sock->acceptClient();

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

            sock->closeSocket();
            exit(0);
        }

        /* Parent */
        else {
            sock->closeClient(); // Only close client connection, not server socket
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
        if (sock->writeRaw(filtered.data(), filtered.length()) == -1) {
            perror("send");
        }
    } else {
        // Send raw buffer
        if (sock->writeRaw(buffer.data(), buffer.size()) == -1) {
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
        ctx.response_body = "<html><head><title>404</title></head><body>404 not found</body></html>";
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
            sock->writeLine(key + ": " + value);
        }
        
        // Send the response body
        if (sock->writeRaw(ctx.response_body.data(), ctx.response_body.length()) == -1) {
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
            sock->writeLine("<html><body>400 Bad Request - Malformed request line</body></html>");
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
            sock->writeLine("<html><body>505 HTTP Version Not Supported</body></html>");
        }
        return false;
    }

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
            sock->writeLine(
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
            
            sock->writeLine(headerStream.str());
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

void Http::processPostRequest(const std::map<std::string, std::string>& headermap, bool keep_alive) {
    // Check for Content-Length header (required for POST)
    auto content_length_it = headermap.find("Content-Length");
    if (content_length_it == headermap.end()) {
        if (DEBUG) {
            std::cout << "POST request without Content-Length header" << std::endl;
        }
        if (sock) {
            sendHeader(411, 0, "text/html", keep_alive);
            sock->writeLine("<html><body>411 Length Required</body></html>");
        }
        return;
    }
    
    // TODO: Actually read and process the POST body
    if (sock)
        sock->writeLine("yeah right d00d\n");
}

/**
 * Processes an HTTP OPTIONS request.
 * Returns allowed methods for the requested resource.
 * @param headermap A map containing parsed HTTP headers.
 * @param keep_alive Whether to keep the connection alive.
 */
void Http::processOptionsRequest(const std::map<std::string, std::string>& headermap, bool keep_alive) {
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
void Http::processHeadRequest(const std::map<std::string, std::string>& headermap, bool keep_alive) {
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

    // TODO: Optimize Log to be a singleton
    Log log;
    log.openLogFile("logs/access_log");
    auto referer_it = headermap.find("Referer");
    auto user_agent_it = headermap.find("User-Agent");
    log.writeLogLine(inet_ntoa(sock->client.sin_addr), "HEAD " + filename, 200, size,
                     referer_it != headermap.end() ? referer_it->second : "",
                     user_agent_it != headermap.end() ? user_agent_it->second : "");
    log.closeLogFile();

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

    std::string filename = sanitizeFilename(it->second);
    std::filesystem::path filepath(filename);
    std::string file_extension = filepath.extension().string();

    // Open file
    std::ifstream file(filename, std::ios::in | std::ios::binary);

    // can't find file, 404 it
    if (!file.is_open()) {
        sendHeader(404, 0, "text/html", false);
        sock->writeLine("<html><head><title>404</title></head><body>404 not "
                        "found</body></html>");
        // sock->closeSocket();
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
        cgi.executeCGI(filename, sock->accept_fd, headermap);
        sock->closeSocket();
        return;
    }

    // TODO: optimize Log to be a singleton
    Log log;
    log.openLogFile("logs/access_log");
    auto referer_it = headermap.find("Referer");
    auto user_agent_it = headermap.find("User-Agent");
    log.writeLogLine(inet_ntoa(sock->client.sin_addr), std::string(request_line), 200, size,
                     referer_it != headermap.end() ? referer_it->second : "",
                     user_agent_it != headermap.end() ? user_agent_it->second : "");
    log.closeLogFile();

    // TODO: optimize Mime to be a singleton
    Mime mime;
    mime.readMimeConfig("mime.types");

    /* BUG: There is a possible bug where the file size changes between sending
       the header and when the file is actually sent through the socket.
    */
    sendHeader(200, size, mime.getMimeFromExtension(filename), keep_alive);
    
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
        read_success = sock->readLineWithTimeout(&line, KEEPALIVE_TIMEOUT);
    } else {
        read_success = sock->readLine(&line);
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
            read_success = sock->readLineWithTimeout(&line, KEEPALIVE_TIMEOUT);
        } else {
            read_success = sock->readLine(&line);
        }
    }

    return clientBuffer;
}

/**
 * Send HTTP headers to the client
 */
void Http::sendHeader(int code, int size, std::string_view file_type, bool keep_alive) {
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
    case 411:
        headerStream << "HTTP/1.1 411 Length Required\r\n";
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
    headerStream << "\r\n";

    lastHeader = headerStream.str();

    if (sock) {
        sock->writeLine(lastHeader);
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
        sock->writeLine(lastHeader);
    }
}
