#ifndef SHELOB_COMPRESSION_MIDDLEWARE_H
#define SHELOB_COMPRESSION_MIDDLEWARE_H 1

#include "middleware.h"
#include <set>

/**
 * Compression middleware (example - not fully implemented)
 * Would compress responses based on Accept-Encoding
 */
class CompressionMiddleware : public Middleware {
private:
    size_t min_size;  // Minimum size to compress
    std::set<std::string> compressible_types;
    
public:
    CompressionMiddleware(size_t min_bytes = 1024) : min_size(min_bytes) {
        compressible_types = {
            "text/html",
            "text/css",
            "text/javascript",
            "application/javascript",
            "application/json",
            "text/plain",
            "text/xml",
            "application/xml"
        };
    }
    
    void process(RequestContext& ctx, std::function<void()> next) override;
};

#endif /* !SHELOB_COMPRESSION_MIDDLEWARE_H */