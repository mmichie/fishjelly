#ifndef SHELOB_LOGGING_MIDDLEWARE_H
#define SHELOB_LOGGING_MIDDLEWARE_H 1

#include "middleware.h"
#include <chrono>

/**
 * Request logging middleware
 * Logs requests with timing information
 */
class LoggingMiddleware : public Middleware {
public:
    void process(RequestContext& ctx, std::function<void()> next) override;
};

#endif /* !SHELOB_LOGGING_MIDDLEWARE_H */