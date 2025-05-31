/**
 * LibFuzzer harness for middleware chain
 * Tests middleware robustness with malformed inputs
 */

#include "../src/middleware.h"
#include "../src/footer_middleware.h"
#include "../src/security_middleware.h"
#include "../src/logging_middleware.h"
#include "../src/compression_middleware.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <memory>
#include <sstream>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) return 0;  // Need at least some data
    
    // Parse fuzzer input into request components
    std::string input(reinterpret_cast<const char*>(data), size);
    std::istringstream stream(input);
    
    // Create request context with fuzzed data
    RequestContext ctx;
    
    // First byte determines method
    switch (data[0] % 4) {
        case 0: ctx.method = "GET"; break;
        case 1: ctx.method = "POST"; break;
        case 2: ctx.method = "HEAD"; break;
        case 3: ctx.method = "OPTIONS"; break;
    }
    
    // Extract path (up to first newline or 256 chars)
    size_t path_end = 1;
    while (path_end < size && path_end < 257 && data[path_end] != '\n') {
        path_end++;
    }
    ctx.path = std::string(reinterpret_cast<const char*>(data + 1), path_end - 1);
    
    // Rest is response body
    if (path_end < size) {
        ctx.response_body = std::string(reinterpret_cast<const char*>(data + path_end + 1), 
                                       size - path_end - 1);
    }
    
    // Add some headers
    ctx.headers["Host"] = "fuzz.test";
    ctx.headers["User-Agent"] = "Fuzzer/1.0";
    
    // Create and execute middleware chain
    MiddlewareChain chain;
    chain.use(std::make_shared<SecurityMiddleware>())
         .use(std::make_shared<LoggingMiddleware>())
         .use(std::make_shared<CompressionMiddleware>())
         .use(std::make_shared<FooterMiddleware>());
    
    try {
        chain.execute(ctx);
    } catch (...) {
        // Catch exceptions but let real crashes through
    }
    
    return 0;
}