# Middleware Chain Implementation

This implementation brings Go's Alice-style middleware chains to the fishjelly web server.

## Overview

The middleware system allows you to chain request/response handlers in a composable way, similar to Go's popular Alice middleware chain library.

## Architecture

### Core Components

1. **RequestContext** - Contains all request and response data
2. **Middleware** - Base class for middleware components
3. **MiddlewareChain** - Builder for chaining middleware
4. **MiddlewareFunc** - Function type for lambda middleware

### Example Usage

```cpp
// Create an HTTP handler
Http http;

// Method 1: Use default middleware
http.setupDefaultMiddleware();

// Method 2: Build custom chain
auto chain = std::make_unique<MiddlewareChain>();
chain->use(std::make_shared<SecurityMiddleware>())
     ->use(std::make_shared<LoggingMiddleware>())
     ->use(std::make_shared<FooterMiddleware>());
http.setMiddlewareChain(std::move(chain));

// Method 3: Use lambda middleware
auto chain = std::make_unique<MiddlewareChain>();
chain->use([](RequestContext& ctx, auto next) {
    // Pre-processing
    ctx.response_headers["X-Custom"] = "Header";
    
    next();  // Call next middleware
    
    // Post-processing
    std::cout << "Request completed: " << ctx.path << std::endl;
});
```

## Built-in Middleware

### 1. SecurityMiddleware
- Blocks dangerous paths (/.env, /.git, etc.)
- Adds security headers (X-Frame-Options, etc.)
- Prevents path traversal attacks

### 2. LoggingMiddleware
- Logs requests with timing information
- Supports verbose mode with headers

### 3. FooterMiddleware
- Adds footer to .shtml files
- Replaces the old Filter class

### 4. CompressionMiddleware
- Placeholder for gzip compression
- Checks Accept-Encoding headers

## Creating Custom Middleware

### Class-based Middleware

```cpp
class RateLimitMiddleware : public Middleware {
private:
    std::map<std::string, int> request_counts;
    int max_requests_per_ip;
    
public:
    RateLimitMiddleware(int max_requests = 100) 
        : max_requests_per_ip(max_requests) {}
    
    void process(RequestContext& ctx, std::function<void()> next) override {
        std::string client_ip = ctx.headers["X-Forwarded-For"];
        
        if (++request_counts[client_ip] > max_requests_per_ip) {
            ctx.status_code = 429;
            ctx.response_body = "Too Many Requests";
            ctx.should_continue = false;
            return;
        }
        
        next();
    }
};
```

### Lambda Middleware

```cpp
// CORS middleware
chain->use([](RequestContext& ctx, auto next) {
    if (ctx.method == "OPTIONS") {
        ctx.status_code = 200;
        ctx.response_headers["Access-Control-Allow-Origin"] = "*";
        ctx.response_headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE";
        ctx.response_sent = true;
        return;
    }
    
    ctx.response_headers["Access-Control-Allow-Origin"] = "*";
    next();
});

// Request ID middleware
chain->use([](RequestContext& ctx, auto next) {
    static std::atomic<uint64_t> request_id{0};
    ctx.response_headers["X-Request-ID"] = std::to_string(++request_id);
    next();
});
```

## Integration Points

Currently, middleware is integrated in:
- `processGetRequest()` when a file is served
- Can be extended to other request types

Future integration points:
- CGI requests
- Error responses
- Static file serving

## Benefits

1. **Composability** - Mix and match middleware as needed
2. **Reusability** - Write once, use in multiple chains
3. **Testability** - Each middleware can be unit tested
4. **Separation of Concerns** - Keep HTTP logic clean
5. **Flexibility** - Use classes, lambdas, or functions

## Comparison with Go's Alice

Go's Alice:
```go
chain := alice.New(Logging, Security, Compression)
http.Handle("/", chain.Then(myHandler))
```

Fishjelly's implementation:
```cpp
auto chain = std::make_unique<MiddlewareChain>();
chain->use(std::make_shared<LoggingMiddleware>())
     ->use(std::make_shared<SecurityMiddleware>())
     ->use(std::make_shared<CompressionMiddleware>());
http.setMiddlewareChain(std::move(chain));
```

## Future Enhancements

1. **Route-specific middleware** - Apply middleware to specific paths
2. **Conditional middleware** - Apply based on request properties
3. **Error handling middleware** - Centralized error handling
4. **WebSocket middleware** - For future WebSocket support
5. **Metrics middleware** - Prometheus-style metrics

## Testing

To test the middleware functionality:

```bash
# Build the project
meson compile -C builddir

# Run with default middleware (in fork mode)
./builddir/src/shelob -p 8080

# The middleware is currently only active when using sendFileWithMiddleware()
# Full integration with ASIO mode is pending
```