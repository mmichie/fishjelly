#include "compression_middleware.h"
#include <algorithm>
#include <sstream>

void CompressionMiddleware::process(RequestContext& ctx, std::function<void()> next) {
    // Check if client accepts compression
    bool accepts_gzip = false;
    auto accept_encoding = ctx.headers.find("Accept-Encoding");
    if (accept_encoding != ctx.headers.end()) {
        accepts_gzip = accept_encoding->second.find("gzip") != std::string::npos;
    }
    
    // Call next middleware
    next();
    
    // Only compress successful responses
    if (!ctx.response_sent && ctx.status_code == 200 && accepts_gzip) {
        // Check if content type is compressible
        if (compressible_types.find(ctx.content_type) != compressible_types.end()) {
            // Check if response is large enough to benefit from compression
            if (ctx.response_body.size() >= min_size) {
                // NOTE: Actual gzip compression would be implemented here
                // For this example, we just add the header to indicate support
                
                // In a real implementation:
                // 1. Compress ctx.response_body using zlib
                // 2. Replace ctx.response_body with compressed data
                // 3. Add Content-Encoding header
                
                // For now, just add a comment in the response
                std::stringstream msg;
                msg << "<!-- Compression middleware: Would compress " 
                    << ctx.response_body.size() << " bytes -->\n";
                ctx.response_body = msg.str() + ctx.response_body;
                
                // Would add: ctx.response_headers["Content-Encoding"] = "gzip";
            }
        }
    }
}