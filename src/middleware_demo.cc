/**
 * Demonstration of the new middleware chain functionality
 * This shows how to use the chainable middleware like Go's Alice
 */

#include "http.h"
#include "socket.h"
#include "middleware.h"
#include "footer_middleware.h"
#include "logging_middleware.h"
#include "security_middleware.h"
#include "compression_middleware.h"
#include <iostream>
#include <memory>

// Custom middleware example - adds a custom header
class CustomHeaderMiddleware : public Middleware {
private:
    std::string header_name;
    std::string header_value;
    
public:
    CustomHeaderMiddleware(const std::string& name, const std::string& value) 
        : header_name(name), header_value(value) {}
    
    void process(RequestContext& ctx, std::function<void()> next) override {
        // Add custom header before processing
        ctx.response_headers[header_name] = header_value;
        
        // Call next middleware
        next();
        
        // Could also modify response after processing
    }
};

// Example authentication middleware
class BasicAuthMiddleware : public Middleware {
private:
    std::string realm;
    std::map<std::string, std::string> users;  // username -> password
    
public:
    BasicAuthMiddleware(const std::string& auth_realm) : realm(auth_realm) {
        // Add some test users
        users["admin"] = "secret";
        users["user"] = "password";
    }
    
    void process(RequestContext& ctx, std::function<void()> next) override {
        // Check for Authorization header
        auto auth_it = ctx.headers.find("Authorization");
        if (auth_it == ctx.headers.end()) {
            // Request authentication
            ctx.status_code = 401;
            ctx.response_headers["WWW-Authenticate"] = "Basic realm=\"" + realm + "\"";
            ctx.response_body = "<html><body>401 Unauthorized</body></html>";
            ctx.should_continue = false;
            return;
        }
        
        // In real implementation, would decode base64 and check credentials
        // For demo, just continue
        next();
    }
};

int main() {
    std::cout << "=== Middleware Chain Demo ===" << std::endl;
    std::cout << std::endl;
    
    // Create HTTP handler
    Http http;
    
    // Example 1: Default middleware chain
    std::cout << "1. Setting up default middleware chain:" << std::endl;
    http.setupDefaultMiddleware();
    std::cout << "   - Security middleware (blocks dangerous paths)" << std::endl;
    std::cout << "   - Logging middleware (logs all requests)" << std::endl;
    std::cout << "   - Compression middleware (adds compression support)" << std::endl;
    std::cout << "   - Footer middleware (adds footer to .shtml files)" << std::endl;
    std::cout << std::endl;
    
    // Example 2: Custom middleware chain
    std::cout << "2. Creating custom middleware chain:" << std::endl;
    auto custom_chain = std::make_unique<MiddlewareChain>();
    
    // Add middleware in order (like Go's Alice)
    custom_chain->use(std::make_shared<LoggingMiddleware>())
                .use(std::make_shared<CustomHeaderMiddleware>("X-Powered-By", "Fishjelly/0.6"))
                .use(std::make_shared<SecurityMiddleware>())
                .use(std::make_shared<FooterMiddleware>());
    
    http.setMiddlewareChain(std::move(custom_chain));
    std::cout << "   Custom chain created!" << std::endl;
    std::cout << std::endl;
    
    // Example 3: Authentication middleware
    std::cout << "3. Adding authentication to specific paths:" << std::endl;
    auto auth_chain = std::make_unique<MiddlewareChain>();
    
    // Create a path-specific middleware
    auth_chain->use([](RequestContext& ctx, std::function<void()> next) {
        // Only apply auth to /admin paths
        if (ctx.path.find("/admin") == 0) {
            BasicAuthMiddleware auth("Admin Area");
            auth.process(ctx, next);
        } else {
            next();
        }
    });
    
    // Add other middleware
    auth_chain->use(std::make_shared<LoggingMiddleware>())
              .use(std::make_shared<SecurityMiddleware>());
    
    std::cout << "   Auth middleware added for /admin paths" << std::endl;
    std::cout << std::endl;
    
    // Example 4: Lambda middleware
    std::cout << "4. Using lambda functions as middleware:" << std::endl;
    auto lambda_chain = std::make_unique<MiddlewareChain>();
    
    // Add timing middleware
    lambda_chain->use([](RequestContext& ctx, std::function<void()> next) {
        auto start = std::chrono::high_resolution_clock::now();
        next();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        ctx.response_headers["X-Response-Time"] = std::to_string(duration.count()) + "us";
    });
    
    // Add CORS middleware
    lambda_chain->use([](RequestContext& ctx, std::function<void()> next) {
        ctx.response_headers["Access-Control-Allow-Origin"] = "*";
        ctx.response_headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
        next();
    });
    
    std::cout << "   - Timing middleware (adds response time header)" << std::endl;
    std::cout << "   - CORS middleware (adds CORS headers)" << std::endl;
    std::cout << std::endl;
    
    std::cout << "=== Middleware Benefits ===" << std::endl;
    std::cout << "1. Composable - combine middleware in any order" << std::endl;
    std::cout << "2. Reusable - use same middleware in multiple chains" << std::endl;
    std::cout << "3. Testable - each middleware can be tested independently" << std::endl;
    std::cout << "4. Flexible - use classes, lambdas, or functions" << std::endl;
    std::cout << "5. Clean - separates concerns from main HTTP logic" << std::endl;
    
    return 0;
}