#include "auth.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <openssl/md5.h>
#include <random>
#include <sstream>

/**
 * Constructor
 */
Auth::Auth() {
    // Initialize with empty user list
}

/**
 * Base64 encoding
 */
std::string Auth::base64_encode(const std::string& input) {
    static const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                      "abcdefghijklmnopqrstuvwxyz"
                                      "0123456789+/";

    std::string output;
    int val = 0;
    int valb = -6;

    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            output.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6) {
        output.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }

    while (output.size() % 4) {
        output.push_back('=');
    }

    return output;
}

/**
 * Base64 decoding
 */
std::string Auth::base64_decode(const std::string& input) {
    static const unsigned char base64_table[256] = {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62,
        64, 64, 64, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64, 64, 0,
        1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
        23, 24, 25, 64, 64, 64, 64, 64, 64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
        39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64};

    std::string output;
    int val = 0;
    int valb = -8;

    for (unsigned char c : input) {
        if (base64_table[c] == 64)
            break;
        val = (val << 6) + base64_table[c];
        valb += 6;
        if (valb >= 0) {
            output.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    return output;
}

/**
 * MD5 hash using OpenSSL
 */
std::string Auth::md5_hash(const std::string& input) {
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(input.c_str()), input.length(), digest);

    std::ostringstream ss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }

    return ss.str();
}

/**
 * Generate a random nonce for Digest authentication
 */
std::string Auth::generate_nonce() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::ostringstream ss;
    ss << std::hex;
    for (int i = 0; i < 32; i++) {
        ss << dis(gen);
    }

    std::string nonce = ss.str();
    nonces_[nonce] = std::chrono::system_clock::now();

    return nonce;
}

/**
 * Validate a nonce (check if it exists and hasn't expired)
 */
bool Auth::validate_nonce(const std::string& nonce) {
    cleanup_expired_nonces();

    auto it = nonces_.find(nonce);
    if (it == nonces_.end()) {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();

    return age <= nonce_timeout_;
}

/**
 * Clean up expired nonces
 */
void Auth::cleanup_expired_nonces() {
    auto now = std::chrono::system_clock::now();

    for (auto it = nonces_.begin(); it != nonces_.end();) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
        if (age > nonce_timeout_) {
            it = nonces_.erase(it);
        } else {
            ++it;
        }
    }
}

/**
 * Parse Basic Authorization header
 * Format: "Basic base64(username:password)"
 */
std::map<std::string, std::string> Auth::parse_basic_auth(const std::string& auth_header) {
    std::map<std::string, std::string> result;

    // Check if it starts with "Basic "
    if (auth_header.find("Basic ") != 0) {
        return result;
    }

    // Extract and decode the base64 part
    std::string encoded = auth_header.substr(6);
    std::string decoded = base64_decode(encoded);

    // Split username:password
    size_t colon_pos = decoded.find(':');
    if (colon_pos == std::string::npos) {
        return result;
    }

    result["username"] = decoded.substr(0, colon_pos);
    result["password"] = decoded.substr(colon_pos + 1);

    return result;
}

/**
 * Parse Digest Authorization header
 * Format: "Digest username="...", realm="...", nonce="...", uri="...", response="...", ..."
 */
std::map<std::string, std::string> Auth::parse_digest_auth(const std::string& auth_header) {
    std::map<std::string, std::string> result;

    // Check if it starts with "Digest "
    if (auth_header.find("Digest ") != 0) {
        return result;
    }

    std::string params = auth_header.substr(7);

    // Parse key=value pairs
    size_t pos = 0;
    while (pos < params.length()) {
        // Find key
        size_t eq_pos = params.find('=', pos);
        if (eq_pos == std::string::npos)
            break;

        std::string key = params.substr(pos, eq_pos - pos);

        // Trim whitespace from key
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);

        // Find value (quoted or unquoted)
        pos = eq_pos + 1;
        std::string value;

        // Skip whitespace
        while (pos < params.length() && std::isspace(params[pos]))
            pos++;

        if (pos < params.length() && params[pos] == '"') {
            // Quoted value
            pos++; // Skip opening quote
            size_t end_quote = params.find('"', pos);
            if (end_quote == std::string::npos)
                break;
            value = params.substr(pos, end_quote - pos);
            pos = end_quote + 1;
        } else {
            // Unquoted value
            size_t comma_pos = params.find(',', pos);
            if (comma_pos == std::string::npos) {
                value = params.substr(pos);
                pos = params.length();
            } else {
                value = params.substr(pos, comma_pos - pos);
                pos = comma_pos;
            }
            // Trim whitespace from value
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
        }

        result[key] = value;

        // Skip comma and whitespace
        while (pos < params.length() && (params[pos] == ',' || std::isspace(params[pos])))
            pos++;
    }

    return result;
}

/**
 * Add a user
 */
void Auth::add_user(const std::string& username, const std::string& password) {
    users_[username] = password;
}

/**
 * Remove a user
 */
void Auth::remove_user(const std::string& username) { users_.erase(username); }

/**
 * Check if user exists
 */
bool Auth::user_exists(const std::string& username) {
    return users_.find(username) != users_.end();
}

/**
 * Add a protected path
 */
void Auth::add_protected_path(const std::string& path, const std::string& realm) {
    protected_paths_[path] = realm;
}

/**
 * Remove a protected path
 */
void Auth::remove_protected_path(const std::string& path) { protected_paths_.erase(path); }

/**
 * Check if path is protected and get its realm
 */
bool Auth::is_protected(const std::string& path, std::string& realm) {
    // Check exact match first
    auto it = protected_paths_.find(path);
    if (it != protected_paths_.end()) {
        realm = it->second;
        return true;
    }

    // Check if path starts with any protected prefix
    for (const auto& [protected_path, protected_realm] : protected_paths_) {
        if (path.find(protected_path) == 0) {
            realm = protected_realm;
            return true;
        }
    }

    return false;
}

/**
 * Validate Basic authentication
 */
bool Auth::validate_basic_auth(const std::string& auth_header) {
    auto params = parse_basic_auth(auth_header);

    if (params.empty()) {
        return false;
    }

    auto username_it = params.find("username");
    auto password_it = params.find("password");

    if (username_it == params.end() || password_it == params.end()) {
        return false;
    }

    auto user_it = users_.find(username_it->second);
    if (user_it == users_.end()) {
        return false;
    }

    return user_it->second == password_it->second;
}

/**
 * Validate Digest authentication
 */
bool Auth::validate_digest_auth(const std::string& auth_header, const std::string& method,
                                const std::string& uri) {
    auto params = parse_digest_auth(auth_header);

    if (params.empty()) {
        return false;
    }

    // Get required parameters
    auto username_it = params.find("username");
    auto nonce_it = params.find("nonce");
    auto response_it = params.find("response");
    auto uri_it = params.find("uri");

    if (username_it == params.end() || nonce_it == params.end() || response_it == params.end() ||
        uri_it == params.end()) {
        return false;
    }

    // Validate that the URI in the Authorization header matches the request URI
    if (uri_it->second != uri) {
        return false;
    }

    // Validate nonce
    if (!validate_nonce(nonce_it->second)) {
        return false;
    }

    // Check if user exists
    auto user_it = users_.find(username_it->second);
    if (user_it == users_.end()) {
        return false;
    }

    // Calculate expected response
    // HA1 = MD5(username:realm:password)
    // HA2 = MD5(method:uri)
    // response = MD5(HA1:nonce:HA2)

    std::string realm = params.count("realm") ? params["realm"] : "Protected Area";
    std::string ha1 = md5_hash(username_it->second + ":" + realm + ":" + user_it->second);
    std::string ha2 = md5_hash(method + ":" + uri_it->second);
    std::string expected_response = md5_hash(ha1 + ":" + nonce_it->second + ":" + ha2);

    return expected_response == response_it->second;
}

/**
 * Generate Basic authentication challenge
 */
std::string Auth::generate_basic_challenge(const std::string& realm) {
    return "Basic realm=\"" + realm + "\"";
}

/**
 * Generate Digest authentication challenge
 */
std::string Auth::generate_digest_challenge(const std::string& realm) {
    std::string nonce = generate_nonce();
    return "Digest realm=\"" + realm + "\", nonce=\"" + nonce + "\", algorithm=MD5, qop=\"auth\"";
}

/**
 * Load users from file
 * Format: username:password (one per line)
 */
void Auth::load_users_from_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) {
            continue;
        }

        std::string username = line.substr(0, colon_pos);
        std::string password = line.substr(colon_pos + 1);

        // Trim whitespace
        username.erase(0, username.find_first_not_of(" \t"));
        username.erase(username.find_last_not_of(" \t\r\n") + 1);
        password.erase(0, password.find_first_not_of(" \t"));
        password.erase(password.find_last_not_of(" \t\r\n") + 1);

        add_user(username, password);
    }
}
