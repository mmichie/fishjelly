#include "http.h"
#include <cstring>

/**
 * Write a RFC 2616 compliant Date header to the client.
 */
void Http::printDate(void) {
    ostringstream date;

    char buf[50];
    time_t ltime;
    struct tm *today;
    ltime = time(NULL);
    today = gmtime(&ltime);

    // Date: Fri, 16 Jul 2004 15:37:18 GMT
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", today);

    date << "Date: " << buf << "\r\n";

    sock->writeLine(date.str());
}

/**
 * Write a RFC 2616 compliant Server header to the client.
 */
void Http::printServer(void) {
    // sock->writeLine("Server: Shelob - Server for HTTP enviroment and Logging
    // Outgoing Bits (Unix)\n");
    sock->writeLine("Server: SHELOB/0.5 (Unix)\r\n");
}

/**
 * Write a RFC 2616 compliant ContentType header to the client.
 */
void Http::printContentType(string type) {
    ostringstream typebuffer;
    typebuffer << "Content-Type: " << type << "\r\n";
    sock->writeLine(typebuffer.str());
}

/**
 * Write a RFC 2616 compliant ContentLength header to the client.
 */
void Http::printContentLength(int size) {
    assert(size >= 0);

    ostringstream clbuffer;
    clbuffer << "Content-Length: " << size << "\r\n";

    sock->writeLine(clbuffer.str());
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
    sock = new Socket(server_port);

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
            // delete sock;
            exit(0);
        }

        /* Parent */
        else {
            sock->closeSocket();
        }
    }

    delete sock;
}

string Http::sanitizeFilename(string filename) {
    // Remove leading '/' from filename
    filename = filename.substr(1, filename.size()).c_str();

    // Default page
    if (filename == "")
        filename = "index.html";

    filename.insert(0, "htdocs/");

    // Remove any trailing newlines
    std::string::size_type pos = filename.find('\n');
    if (pos != std::string::npos)
        filename.erase(pos);

    return filename;
}

/**
 * Sends a file down an open socket.
 */
void Http::sendFile(string filename) {
    // Open file
    ifstream file(filename.c_str(), ios::in | ios::binary);

    // Find the extension (assumes the extension is whatever follows the last
    // '.')
    string file_extension =
        filename.substr(filename.rfind("."), filename.length());

    // Determine File Size
    unsigned long size;

    file.seekg(0, ios::end);
    size = file.tellg();

    // Allocate buffer large enough to read entire file
    char *buffer;
    buffer = new char[size + 1];
    if (buffer == NULL) {
        cerr << "Error allocating buffer!" << endl;
        file.close();
        return;
    }

    // Go to beginning of file and read it in
    file.seekg(0, ios::beg);
    file.read(buffer, size);

    if (file.gcount() != static_cast<streamsize>(size)) {
        cerr << "Error with read!" << endl;
        file.close();
        return;
    }

    // Text manipulate contents of .shtml
    string filtered;
    if (file_extension == ".shtml" || file_extension == ".shtm") {
        buffer[size] = 0;
        // Add the footer
        string s_buffer(buffer);
        Filter filter;

        filtered = filter.addFooter(s_buffer);
        size = filtered.length();
    }

    // Something of a hack FIXME if time permits
    //	sock->writeLine(buffer);
    if (file_extension == ".shtml" || file_extension == ".shtm") {
        if (send(sock->accept_fd, filtered.data(), size, 0) == -1) {
            perror("writeLine");
            // return (-1);
        }
    } else {
        if (send(sock->accept_fd, buffer, size, 0) == -1) {
            perror("writeLine");
            // return (-1);
        }
    }

    // cleanup
    file.close();
    delete[] buffer;
}

bool Http::parseHeader(string header) {
    string request_line;

    vector<string> tokens, tokentmp;
    map<string, string> headermap;

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

    string name, value;
    /* Seperate each request header with the name and value and insert into a
     * hash map */
    for (i = 1; i < tokens.size(); i++) {
        std::string::size_type pos = tokens[i].find(':');
        if (pos != std::string::npos) {
            name = tokens[i].substr(0, pos);
            value = tokens[i].substr(pos + 2, tokens[i].length() - pos);

            headermap[name] = value;
        }
    }

    /* Print all pairs of the header hash map to console */
    if (DEBUG) {
        map<string, string>::iterator iter;
        for (iter = headermap.begin(); iter != headermap.end(); iter++) {
            cout << iter->first << " : " << iter->second << endl;
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

void Http::processPostRequest(map<string, string> headermap) {
    (void)headermap; // Suppress unused parameter warning
    sock->writeLine("yeah right d00d\n");
}

void Http::processHeadRequest(map<string, string> headermap) {
    (void)headermap; // Suppress unused parameter warning
    sock->writeLine("HAR HAR !\n");
}

void Http::processGetRequest(map<string, string> headermap, string request_line,
                             bool keep_alive) {
    string filename = headermap["GET"];
    string file_extension;

    filename = sanitizeFilename(filename);

    // Open file
    ifstream file(filename.c_str(), ios::in | ios::binary);

    // Find the extension (assumes the extension is whatever follows the last
    // '.')
    file_extension = filename.substr(filename.rfind("."), filename.length());

    // can't find file, 404 it
    if (!file.is_open()) {
        sendHeader(404, 0, "text/html", false);
        sock->writeLine("<html><head><title>404</title></head><body>404 not "
                        "found</body></html>");
        // sock->closeSocket();
        return;
    }

    // Determine File Size
    unsigned long size;

    file.seekg(0, ios::end);
    size = file.tellg();

    if (file_extension == ".sh") {
        Cgi cgi;
        cgi.executeCGI(filename, sock->accept_fd, headermap);
        sock->closeSocket();
        return;
    }

    // TODO: optimize Log to be a singleton
    Log log;
    log.openLogFile("logs/access_log");
    log.writeLogLine(inet_ntoa(sock->client.sin_addr), request_line, 200, size,
                     headermap["Referer"], headermap["User-Agent"]);
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
string Http::getHeader() {
    string clientBuffer;

    // read until EOF, break when done with header
    while ((sock->readLine(&clientBuffer))) {
        if (strstr(clientBuffer.c_str(), "\n\n") != NULL)
            break;
    }

    return clientBuffer;
}

/**
 * Send HTTP headers to the client
 */
void Http::sendHeader(int code, int size, string file_type, bool keep_alive) {
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
        cerr << "Wrong HTTP CODE!" << endl;
    }

    printDate();
    printServer();

    if (size != 0)
        printContentLength(size);

    printConnectionType(keep_alive);
    printContentType(file_type);

    sock->writeLine("\n");
}
