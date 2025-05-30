#ifndef SHELOB_SECURITY_MIDDLEWARE_H
#define SHELOB_SECURITY_MIDDLEWARE_H 1

#include "middleware.h"
#include <set>
#include <string>

/**
 * Security middleware
 * Adds security headers and basic protection
 */
class SecurityMiddleware : public Middleware {
private:
    std::set<std::string> blocked_paths;
    bool add_security_headers;
    
public:
    SecurityMiddleware(bool add_headers = true) : add_security_headers(add_headers) {
        // Block some common attack paths
        blocked_paths = {
            "/.env",
            "/.git",
            "/.htaccess",
            "/wp-admin",
            "/wp-login.php",
            "/admin",
            "/.ssh"
        };
    }
    
    void process(RequestContext& ctx, std::function<void()> next) override;
};

#endif /* !SHELOB_SECURITY_MIDDLEWARE_H */