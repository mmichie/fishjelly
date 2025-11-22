#include "security_middleware.h"
#include "http.h"
#include <algorithm>
#include <cctype>

/**
 * URL decode a string (handles %XX encoding)
 */
std::string SecurityMiddleware::url_decode(const std::string& str) {
    std::string result;
    result.reserve(str.length());

    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            // Check if next two characters are valid hex digits
            if (std::isxdigit(str[i + 1]) && std::isxdigit(str[i + 2])) {
                std::string hex = str.substr(i + 1, 2);
                try {
                    int value = std::stoi(hex, nullptr, 16);
                    result += static_cast<char>(value);
                    i += 2; // Skip the two hex digits
                } catch (...) {
                    // Invalid hex sequence, keep the % character
                    result += str[i];
                }
            } else {
                // Not valid hex after %, keep the % character
                result += str[i];
            }
        } else {
            result += str[i];
        }
    }

    return result;
}

/**
 * Recursively URL decode until no more encoding is found
 * This prevents double/triple encoding bypasses like %252f
 */
std::string SecurityMiddleware::url_decode_recursive(const std::string& str) {
    std::string current = str;
    std::string decoded;
    int max_iterations = 10; // Prevent infinite loops
    int iterations = 0;

    // Keep decoding until the string stops changing
    do {
        decoded = url_decode(current);
        if (decoded == current || iterations++ >= max_iterations) {
            break;
        }
        current = decoded;
    } while (true);

    return current;
}

/**
 * Sanitize and validate a file path to prevent directory traversal
 * Returns the sanitized path if valid, empty string if invalid
 */
std::string SecurityMiddleware::sanitize_path(const std::string& path,
                                              const std::filesystem::path& base_dir) {
    try {
        // 1. URL decode the path (handles multiple encodings)
        std::string decoded = url_decode_recursive(path);

        // 2. Remove null bytes (prevents null byte injection)
        decoded.erase(std::remove(decoded.begin(), decoded.end(), '\0'), decoded.end());

        // 3. Additional checks for suspicious patterns before canonicalization
        // Check for backslashes (Windows-style paths, suspicious on Unix)
        if (decoded.find('\\') != std::string::npos) {
            return ""; // Block paths with backslashes
        }

        // Check for multiple dots followed by slashes (various evasion attempts)
        if (decoded.find("..") != std::string::npos) {
            // This is a strong indicator of traversal attempt
            // We'll still do full canonical validation below, but this catches obvious cases
        }

        // 4. Remove any leading slashes to make it relative
        while (!decoded.empty() && decoded[0] == '/') {
            decoded = decoded.substr(1);
        }

        // 5. If empty after removing slashes, use current directory
        if (decoded.empty()) {
            decoded = ".";
        }

        // 6. Normalize multiple slashes to single slashes
        size_t pos = 0;
        while ((pos = decoded.find("//", pos)) != std::string::npos) {
            decoded.replace(pos, 2, "/");
        }

        // 7. Construct the full path and canonicalize it
        std::filesystem::path requested_path = base_dir / decoded;
        std::filesystem::path canonical_path = std::filesystem::weakly_canonical(requested_path);

        // 8. Get the canonical absolute path of the base directory
        std::filesystem::path canonical_base = std::filesystem::weakly_canonical(base_dir);

        // 9. Check if the canonical path is still within the base directory
        // by verifying that canonical_base is a prefix of canonical_path
        auto [base_end, path_end] = std::mismatch(canonical_base.begin(), canonical_base.end(),
                                                  canonical_path.begin(), canonical_path.end());

        // If we didn't consume all of canonical_base, the path escapes
        if (base_end != canonical_base.end()) {
            return ""; // Path traversal detected
        }

        // 10. Return the sanitized path (relative to base_dir)
        return canonical_path.string();

    } catch (const std::filesystem::filesystem_error&) {
        // Any filesystem error means the path is invalid
        return "";
    } catch (...) {
        // Any other error means the path is invalid
        return "";
    }
}

void SecurityMiddleware::process(RequestContext& ctx, std::function<void()> next) {
    // Check for blocked paths (before sanitization to catch raw attempts)
    for (const auto& blocked : blocked_paths) {
        if (ctx.path.find(blocked) == 0) { // starts_with equivalent
            ctx.status_code = 403;
            ctx.response_body = "<html><body>403 Forbidden</body></html>";
            ctx.should_continue = false;
            return;
        }
    }

    // Sanitize the path and check for traversal attempts
    std::string sanitized = sanitize_path(ctx.path, std::filesystem::path("htdocs"));
    if (sanitized.empty()) {
        // Path traversal or invalid path detected
        ctx.status_code = 400;
        ctx.response_body = "<html><body>400 Bad Request - Invalid Path</body></html>";
        ctx.should_continue = false;
        return;
    }

    // Update the context with the sanitized path for use by downstream handlers
    // Note: We store the full canonical path, downstream handlers can use it directly
    ctx.sanitized_file_path = sanitized;

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