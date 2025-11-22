/**
 * Rate Limiting Demo
 * Demonstrates the 429 Too Many Requests functionality with aggressive limits
 */

#include "http.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    int port = 8080;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [-p PORT]" << std::endl;
            std::cout << "  -p, --port PORT    Listen on PORT (default: 8080)" << std::endl;
            std::cout << "\nRate Limiting Configuration:" << std::endl;
            std::cout << "  Max Requests: 10 per 10 seconds" << std::endl;
            std::cout << "  Block Duration: 30 seconds" << std::endl;
            return 0;
        }
    }

    std::cout << "===========================================\n";
    std::cout << "  Rate Limiting Demo Server\n";
    std::cout << "===========================================\n";
    std::cout << "Starting server on port " << port << "\n";
    std::cout << "\nRate Limiting Settings:\n";
    std::cout << "  - Max Requests: 10 per 10 seconds\n";
    std::cout << "  - Block Duration: 30 seconds after limit exceeded\n";
    std::cout << "\nTo test rate limiting:\n";
    std::cout << "  for i in {1..15}; do curl -s http://localhost:" << port
              << "/ -o /dev/null -w 'Request $i: HTTP %{http_code}\\n'; done\n";
    std::cout << "===========================================\n\n";

    Http http;

    // Enable rate limiting with aggressive limits for demo
    http.setRateLimitEnabled(true);
    http.setRateLimitMaxRequests(10);   // Only 10 requests
    http.setRateLimitWindow(10);        // Per 10 seconds
    http.setRateLimitBlockDuration(30); // Block for 30 seconds

    http.start(port, 30, 30, 0);

    return 0;
}
