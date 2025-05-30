#include "http.h"
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>

/**
 * Write a RFC 2616 compliant Date header to the client.
 */
void Http::printDate() {
    std::array<char, 50> buf;
    time_t ltime = time(nullptr);
    struct tm *today = gmtime(&ltime);

    // Date: Fri, 16 Jul 2004 15:37:18 GMT
    strftime(buf.data(), buf.size(), "%a, %d %b %Y %H:%M:%S GMT", today);

    sock->writeLine(std::format("Date: {}\r\n", buf.data()));
}

/**
 * Write a RFC 2616 compliant Server header to the client.
 */
void Http::printServer() {
    sock->writeLine("Server: SHELOB/0.5 (Unix)\r\n");
}

/**
 * Write a RFC 2616 compliant ContentType header to the client.
 */
void Http::printContentType(std::string_view type) {
    sock->writeLine(std::format("Content-Type: {}\r\n", type));
}

/**
 * Write a RFC 2616 compliant ContentLength header to the client.
 */
void Http::printContentLength(int size) {
    assert(size >= 0);
    sock->writeLine(std::format("Content-Length: {}\r\n", size));
}

/**
 * Write a RFC 2616 compliant ConnectionType header to the client.
 */
void Http::printConnectionType(bool keep_alive) {
    if (keep_alive)
        sock->writeLine("Connection: Keep-Alive\n");
    else
        sock->writeLine("Connection: close\n");
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

            keep_alive = parseHeader(getHeader());
            while (keep_alive) {
                keep_alive = parseHeader(getHeader());
            }

            sock->closeSocket();
            exit(0);
        }

        /* Parent */
        else {
            sock->closeSocket();
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
        if (send(sock->accept_fd, filtered.data(), filtered.length(), 0) == -1) {
            perror("send");
        }
    } else {
        // Send raw buffer
        if (send(sock->accept_fd, buffer.data(), buffer.size(), 0) == -1) {
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

    Token token;
    /* Seperate the client request headers by newline */
    token.tokenize(header, tokens, "\n");

    /* The first line of the client request should always be GET, HEAD, POST,
     * etc */
    request_line = tokens[0];
    token.tokenize(tokens[0], tokentmp, " ");
    headermap[tokentmp[0]] = tokentmp[1];

    /* Seperate each request header with the name and value and insert into a
     * hash map */
    for (i = 1; i < tokens.size(); i++) {
        std::string::size_type pos = tokens[i].find(':');
        if (pos != std::string::npos) {
            std::string name = tokens[i].substr(0, pos);
            std::string value = tokens[i].substr(pos + 2, tokens[i].length() - pos);

            headermap[name] = value;
        }
    }

    /* Print all pairs of the header hash map to console */
    if (DEBUG) {
        for (const auto& [key, value] : headermap) {
            std::cout << key << " : " << value << std::endl;
        }
    }

    if (headermap["Connection"] == "keep-alive")
        keep_alive = true;
    else
        keep_alive = false;

    if (headermap.size() <= 0) {
        return false;
    }

    if (headermap.find("GET") != headermap.end()) {
        processGetRequest(headermap, request_line, keep_alive);
    } else if (headermap.find("HEAD") != headermap.end()) {
        processHeadRequest(headermap);
    } else if (headermap.find("POST") != headermap.end()) {
        processPostRequest(headermap);
    }

    return keep_alive;
}

void Http::processPostRequest(const std::map<std::string, std::string>& headermap) {
    (void)headermap; // Suppress unused parameter warning
    sock->writeLine("yeah right d00d\n");
}

/**
 * Processes an HTTP HEAD request.
 * @param headermap A map containing parsed HTTP headers.
 */
void Http::processHeadRequest(const std::map<std::string, std::string>& headermap) {
    auto it = headermap.find("HEAD");
    if (it == headermap.end()) return;
    
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

    // Determine File Size
    file.seekg(0, std::ios::end);
    auto size = file.tellg();

    // TODO: Optimize Log to be a singleton
    Log log;
    log.openLogFile("logs/access_log");
    auto referer_it = headermap.find("Referer");
    auto user_agent_it = headermap.find("User-Agent");
    log.writeLogLine(inet_ntoa(sock->client.sin_addr), "HEAD " + filename, 200,
                     size, 
                     referer_it != headermap.end() ? referer_it->second : "",
                     user_agent_it != headermap.end() ? user_agent_it->second : "");
    log.closeLogFile();

    // TODO: Optimize Mime to be a singleton
    Mime mime;
    mime.readMimeConfig("mime.types");

    // Send header
    sendHeader(200, size, mime.getMimeFromExtension(filename), false);
}

void Http::processGetRequest(const std::map<std::string, std::string>& headermap, 
                             std::string_view request_line,
                             bool keep_alive) {
    auto it = headermap.find("GET");
    if (it == headermap.end()) return;
    
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
    log.writeLogLine(inet_ntoa(sock->client.sin_addr), 
                     std::string(request_line), 200, size,
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
    sendFile(filename);
}

/**
 * Receive the client's request headers
 */
std::string Http::getHeader() {
    std::string clientBuffer;

    // read until EOF, break when done with header
    while ((sock->readLine(&clientBuffer))) {
        if (clientBuffer.find("\n\n") != std::string::npos)
            break;
    }

    return clientBuffer;
}

/**
 * Send HTTP headers to the client
 */
void Http::sendHeader(int code, int size, std::string_view file_type, bool keep_alive) {
    assert(code > 99 && code < 600);
    assert(size >= 0);

    switch (code) {
    case 200:
        sock->writeLine("HTTP/1.1 200 OK\r\n");
        break;
    case 404:
        sock->writeLine("HTTP/1.1 404 Not Found\r\n");
        break;
    default:
        std::cerr << "Wrong HTTP CODE!" << std::endl;
    }

    printDate();
    printServer();

    if (size != 0)
        printContentLength(size);

    printConnectionType(keep_alive);
    printContentType(file_type);

    sock->writeLine("\n");
}
