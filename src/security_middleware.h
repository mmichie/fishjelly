#ifndef SHELOB_SECURITY_MIDDLEWARE_H
#define SHELOB_SECURITY_MIDDLEWARE_H 1

#include "middleware.h"
#include <filesystem>
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

    /**
     * URL decode a string (handles %XX encoding)
     * Returns the decoded string
     */
    static std::string url_decode(const std::string& str);

    /**
     * Recursively URL decode until no more encoding is found
     * This prevents double/triple encoding bypasses
     */
    static std::string url_decode_recursive(const std::string& str);

  public:
    /**
     * Sanitize and validate a file path to prevent directory traversal
     * Returns the sanitized path if valid, empty string if invalid
     * This is public so other components (like Http2Server) can use it
     */
    static std::string sanitize_path(const std::string& path,
                                     const std::filesystem::path& base_dir);
    SecurityMiddleware(bool add_headers = true) : add_security_headers(add_headers) {
        // Block some common attack paths
        blocked_paths = {"/.env",         "/.git",  "/.htaccess", "/wp-admin",
                         "/wp-login.php", "/admin", "/.ssh"};
    }

    void process(RequestContext& ctx, std::function<void()> next) override;
};

#endif /* !SHELOB_SECURITY_MIDDLEWARE_H */