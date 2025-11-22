#ifndef SHELOB_AUTH_H
#define SHELOB_AUTH_H 1

#include <chrono>
#include <map>
#include <string>
#include <string_view>

/**
 * Authentication manager for HTTP Basic and Digest authentication
 * RFC 7617 (Basic) and RFC 7616 (Digest)
 *
 * Security: Uses argon2id for password hashing (OWASP recommendation)
 * Passwords are never stored in plaintext, only as salted hashes
 */
class Auth {
  private:
    // User credentials storage (username -> password_hash)
    // NOTE: Values are argon2id hashes, NOT plaintext passwords
    std::map<std::string, std::string> users_;

    // Protected paths (path -> realm)
    std::map<std::string, std::string> protected_paths_;

    // Active nonces for Digest auth (nonce -> timestamp)
    std::map<std::string, std::chrono::system_clock::time_point> nonces_;

    // Nonce timeout in seconds
    int nonce_timeout_ = 300; // 5 minutes

    // Base64 encoding/decoding
    std::string base64_encode(const std::string& input);
    std::string base64_decode(const std::string& input);

    // MD5 hashing for Digest auth (legacy - only for Digest auth protocol)
    std::string md5_hash(const std::string& input);

    // Secure password hashing (argon2id via libsodium)
    std::string hash_password(const std::string& password);
    bool verify_password(const std::string& password, const std::string& hash);

    // Nonce management
    std::string generate_nonce();
    bool validate_nonce(const std::string& nonce);
    void cleanup_expired_nonces();

    // Parse Authorization header
    std::map<std::string, std::string> parse_basic_auth(const std::string& auth_header);
    std::map<std::string, std::string> parse_digest_auth(const std::string& auth_header);

  public:
    Auth();

    // User management
    void add_user(const std::string& username, const std::string& password);
    void remove_user(const std::string& username);
    bool user_exists(const std::string& username);

    // Protected paths
    void add_protected_path(const std::string& path, const std::string& realm = "Protected Area");
    void remove_protected_path(const std::string& path);
    bool is_protected(const std::string& path, std::string& realm);

    // Authentication validation
    bool validate_basic_auth(const std::string& auth_header);
    bool validate_digest_auth(const std::string& auth_header, const std::string& method,
                              const std::string& uri);

    // Generate authentication challenges
    std::string generate_basic_challenge(const std::string& realm);
    std::string generate_digest_challenge(const std::string& realm);

    // Configuration
    void set_nonce_timeout(int seconds) { nonce_timeout_ = seconds; }
    void load_users_from_file(const std::string& filename);
};

#endif /* !SHELOB_AUTH_H */
