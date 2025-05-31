/**
 * LibFuzzer harness for HTTP parser
 * Build with: clang++ -g -fsanitize=fuzzer,address fuzz_http_parser.cc
 */

#include "../src/http.h"
#include "../src/socket.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <memory>

// Mock socket for fuzzing
class FuzzSocket : public Socket {
public:
    std::string response_data;
    
    FuzzSocket() : Socket() {}
    
    bool readLine(std::string* line) override {
        // Not used in fuzzing
        return false;
    }
    
    bool readLineWithTimeout(std::string* line, int timeout_seconds) override {
        // Not used in fuzzing
        return false;
    }
    
    void writeLine(std::string_view line) override {
        response_data += std::string(line) + "\n";
    }
    
    int writeRaw(const void* buffer, size_t length) override {
        response_data.append(static_cast<const char*>(buffer), length);
        return length;
    }
    
    void closeSocket() override {
        // No-op for fuzzing
    }
    
    void closeClient() override {
        // No-op for fuzzing
    }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Convert fuzzer input to string
    std::string input(reinterpret_cast<const char*>(data), size);
    
    // Create HTTP handler with mock socket
    Http http;
    auto fuzz_sock = std::make_unique<FuzzSocket>();
    http.sock = std::move(fuzz_sock);
    
    // Test the parser
    try {
        http.parseHeader(input);
    } catch (...) {
        // Catch any exceptions to prevent crashes from being reported as bugs
        // Real crashes (segfaults, etc.) will still be caught by the fuzzer
    }
    
    return 0;
}