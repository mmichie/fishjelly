/**
 * Property-based testing for HTTP server
 * Tests invariants that should always hold
 */

#include "../src/http.h"
#include "../src/socket.h"
#include "../src/middleware.h"
#include <random>
#include <iostream>
#include <cassert>

class TestSocket : public Socket {
public:
    std::string output;
    std::string input;
    size_t input_pos = 0;
    
    bool readLine(std::string* line) override {
        if (input_pos >= input.size()) return false;
        
        size_t newline = input.find('\n', input_pos);
        if (newline == std::string::npos) {
            *line = input.substr(input_pos);
            input_pos = input.size();
        } else {
            *line = input.substr(input_pos, newline - input_pos + 1);
            input_pos = newline + 1;
        }
        return true;
    }
    
    bool readLineWithTimeout(std::string* line, int timeout) override {
        return readLine(line);
    }
    
    void writeLine(std::string_view line) override {
        output += line;
        output += "\n";
    }
    
    int writeRaw(const void* buffer, size_t length) override {
        output.append(static_cast<const char*>(buffer), length);
        return length;
    }
    
    void closeSocket() override {}
    void closeClient() override {}
};

// Property: Server should never crash on any input
bool test_no_crash(const std::string& input) {
    try {
        Http http;
        auto test_sock = std::make_unique<TestSocket>();
        test_sock->input = input;
        http.sock = std::move(test_sock);
        
        http.parseHeader(input);
        return true;
    } catch (...) {
        // Exceptions are OK, crashes are not
        return true;
    }
}

// Property: Valid HTTP/1.1 without Host should always return 400
bool test_http11_host_required(const std::string& method, const std::string& path) {
    std::string request = method + " " + path + " HTTP/1.1\r\n\r\n";
    
    Http http;
    auto test_sock = std::make_unique<TestSocket>();
    test_sock->input = request;
    auto* sock_ptr = test_sock.get();
    http.sock = std::move(test_sock);
    
    http.parseHeader(request);
    
    // Check that 400 was sent
    return sock_ptr->output.find("400 Bad Request") != std::string::npos;
}

// Property: Middleware chain should preserve request integrity
bool test_middleware_preserves_request() {
    RequestContext ctx;
    ctx.method = "GET";
    ctx.path = "/test";
    ctx.response_body = "Original content";
    
    std::string original_method = ctx.method;
    std::string original_path = ctx.path;
    
    MiddlewareChain chain;
    chain.use([](RequestContext& ctx, auto next) {
        // Middleware should not change method/path unless intended
        next();
    });
    
    chain.execute(ctx);
    
    return ctx.method == original_method && ctx.path == original_path;
}

// Random string generator for fuzzing
std::string generate_random_string(std::mt19937& gen, size_t max_length) {
    std::uniform_int_distribution<> length_dist(0, max_length);
    std::uniform_int_distribution<> char_dist(0, 255);
    
    size_t length = length_dist(gen);
    std::string result;
    result.reserve(length);
    
    for (size_t i = 0; i < length; i++) {
        result.push_back(static_cast<char>(char_dist(gen)));
    }
    
    return result;
}

int main() {
    std::mt19937 gen(std::random_device{}());
    const int NUM_TESTS = 10000;
    
    std::cout << "Running property-based tests...\n";
    
    // Test 1: No crashes
    std::cout << "Testing crash resistance: ";
    for (int i = 0; i < NUM_TESTS; i++) {
        std::string random_input = generate_random_string(gen, 1024);
        assert(test_no_crash(random_input));
        if (i % 1000 == 0) std::cout << ".";
    }
    std::cout << " PASSED\n";
    
    // Test 2: HTTP/1.1 Host requirement
    std::cout << "Testing HTTP/1.1 Host requirement: ";
    std::vector<std::string> methods = {"GET", "POST", "HEAD", "OPTIONS", "PUT", "DELETE"};
    for (int i = 0; i < 1000; i++) {
        std::uniform_int_distribution<> method_dist(0, methods.size() - 1);
        std::string method = methods[method_dist(gen)];
        std::string path = "/" + generate_random_string(gen, 50);
        
        // Remove newlines from path
        path.erase(std::remove(path.begin(), path.end(), '\n'), path.end());
        path.erase(std::remove(path.begin(), path.end(), '\r'), path.end());
        
        assert(test_http11_host_required(method, path));
        if (i % 100 == 0) std::cout << ".";
    }
    std::cout << " PASSED\n";
    
    // Test 3: Middleware integrity
    std::cout << "Testing middleware integrity: ";
    for (int i = 0; i < 1000; i++) {
        assert(test_middleware_preserves_request());
        if (i % 100 == 0) std::cout << ".";
    }
    std::cout << " PASSED\n";
    
    std::cout << "\nAll property tests passed!\n";
    return 0;
}