#include "security_middleware.h"
#include "http.h"

void SecurityMiddleware::process(RequestContext& ctx, std::function<void()> next) {
    // Check for blocked paths
    for (const auto& blocked : blocked_paths) {
        if (ctx.path.find(blocked) == 0) {  // starts_with equivalent
            ctx.status_code = 403;
            ctx.response_body = "<html><body>403 Forbidden</body></html>";
            ctx.should_continue = false;
            return;
        }
    }
    
    // Check for path traversal attempts
    if (ctx.path.find("../") != std::string::npos || 
        ctx.path.find("..\\") != std::string::npos) {
        ctx.status_code = 400;
        ctx.response_body = "<html><body>400 Bad Request</body></html>";
        ctx.should_continue = false;
        return;
    }
    
    // Add security headers to response
    if (add_security_headers) {
        ctx.response_headers["X-Content-Type-Options"] = "nosniff";
        ctx.response_headers["X-Frame-Options"] = "DENY";
        ctx.response_headers["X-XSS-Protection"] = "1; mode=block";
        ctx.response_headers["Referrer-Policy"] = "strict-origin-when-cross-origin";
    }
    
    // Continue to next middleware
    next();
}