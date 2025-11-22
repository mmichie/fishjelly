#ifndef SHELOB_MIDDLEWARE_H
#define SHELOB_MIDDLEWARE_H 1

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// Forward declaration
class Http;

/**
 * Request context passed through middleware chain
 */
struct RequestContext {
    // Request details
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;

    // Security-sanitized file path (set by SecurityMiddleware)
    std::string sanitized_file_path;

    // Response details (can be modified by middleware)
    int status_code = 200;
    std::map<std::string, std::string> response_headers;
    std::string response_body;
    std::string content_type = "text/html";

    // Control flow
    bool should_continue = true; // Set to false to stop chain
    bool response_sent = false;  // Set to true if middleware sent response

    // Reference to HTTP handler for sending responses
    Http* http_handler = nullptr;
};

/**
 * Middleware function type
 * Takes a context and next handler, processes request/response
 */
using MiddlewareFunc = std::function<void(RequestContext&, std::function<void()>)>;

/**
 * Base class for middleware
 */
class Middleware {
  public:
    virtual ~Middleware() = default;

    /**
     * Process the request/response
     * @param ctx Request context
     * @param next Function to call next middleware in chain
     */
    virtual void process(RequestContext& ctx, std::function<void()> next) = 0;

    /**
     * Convert to middleware function for chaining
     */
    MiddlewareFunc toFunc() {
        return
            [this](RequestContext& ctx, std::function<void()> next) { this->process(ctx, next); };
    }
};

/**
 * Middleware chain builder (like Alice in Go)
 */
class MiddlewareChain {
  private:
    std::vector<MiddlewareFunc> middlewares;

  public:
    /**
     * Add middleware function to chain
     */
    MiddlewareChain& use(MiddlewareFunc middleware) {
        middlewares.push_back(middleware);
        return *this;
    }

    /**
     * Add middleware object to chain
     */
    MiddlewareChain& use(std::shared_ptr<Middleware> middleware) {
        middlewares.push_back(middleware->toFunc());
        return *this;
    }

    /**
     * Execute the middleware chain
     */
    void execute(RequestContext& ctx) { executeIndex(ctx, 0); }

  private:
    void executeIndex(RequestContext& ctx, size_t index) {
        if (!ctx.should_continue || ctx.response_sent) {
            return;
        }

        if (index >= middlewares.size()) {
            return; // End of chain
        }

        auto next = [this, &ctx, index]() { executeIndex(ctx, index + 1); };

        middlewares[index](ctx, next);
    }
};

#endif /* !SHELOB_MIDDLEWARE_H */