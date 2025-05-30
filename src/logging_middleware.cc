#include "logging_middleware.h"
#include <iostream>
#include <iomanip>
#include <sstream>

void LoggingMiddleware::process(RequestContext& ctx, std::function<void()> next) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Call next middleware
    next();
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Log the request
    std::stringstream log_entry;
    log_entry << "[" << std::chrono::system_clock::now().time_since_epoch().count() << "] "
              << ctx.method << " " << ctx.path << " " << ctx.version << " "
              << "-> " << ctx.status_code << " "
              << "(" << duration.count() << "μs)";
    
    // Add request headers if verbose logging
    if (std::getenv("VERBOSE_LOG")) {
        log_entry << " Headers: {";
        for (const auto& [key, value] : ctx.headers) {
            log_entry << key << ": " << value << ", ";
        }
        log_entry << "}";
    }
    
    std::cout << log_entry.str() << std::endl;
}