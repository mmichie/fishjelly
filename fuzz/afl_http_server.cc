/**
 * AFL++ harness for fuzzing the HTTP server
 * Build with: afl-clang++ -g afl_http_server.cc -o afl_http_server
 * Run with: afl-fuzz -i corpus -o findings -- ./afl_http_server
 */

#include "../src/http.h"
#include "../src/socket.h"
#include "../src/mime.h"
#include "../src/log.h"
#include <iostream>
#include <string>
#include <sstream>
#include <memory>
#include <unistd.h>

// Mock socket that reads from stdin for AFL
class AFLSocket : public Socket {
public:
    std::istringstream input_stream;
    std::ostringstream output_stream;
    bool has_data = true;
    
    AFLSocket(const std::string& input) : Socket(), input_stream(input) {}
    
    bool readLine(std::string* line) override {
        if (std::getline(input_stream, *line)) {
            *line += "\n";
            return true;
        }
        return false;
    }
    
    bool readLineWithTimeout(std::string* line, int timeout_seconds) override {
        return readLine(line);
    }
    
    void writeLine(std::string_view line) override {
        output_stream << line << "\n";
    }
    
    int writeRaw(const void* buffer, size_t length) override {
        output_stream.write(static_cast<const char*>(buffer), length);
        return length;
    }
    
    void closeSocket() override {
        has_data = false;
    }
    
    void closeClient() override {
        has_data = false;
    }
};

int main(int argc, char** argv) {
#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
#endif

    // Read input from stdin
    std::string input;
    std::string line;
    while (std::getline(std::cin, line)) {
        input += line + "\n";
    }

#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_FUZZ_TESTCASE_LEN = input.length();
    __AFL_FUZZ_TESTCASE_BUF = (unsigned char*)input.data();
#endif

    while (__AFL_LOOP(1000)) {
        // Create HTTP handler with AFL socket
        Http http;
        auto afl_sock = std::make_unique<AFLSocket>(input);
        http.sock = std::move(afl_sock);
        
        // Enable middleware for more code coverage
        http.setupDefaultMiddleware();
        
        try {
            // Get headers from input
            std::string headers = http.getHeader(false);
            if (!headers.empty()) {
                // Parse the headers
                http.parseHeader(headers);
            }
        } catch (...) {
            // Catch exceptions but let crashes through
        }
    }
    
    return 0;
}