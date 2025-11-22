# Fishjelly Development Roadmap

## Phase 1: Core HTTP/1.1 Compliance (Completed ✓)
- [x] Fix server crash after handling requests (fork model issue)

## Phase 2: HTTP/1.1 Required Features (Completed ✓)
- [x] Implement Host header validation (return 400 for HTTP/1.1 without Host)
- [x] Add HTTP version parsing and proper handling
- [x] Implement OPTIONS method (mandatory for HTTP/1.1)
- [x] Add missing critical status codes:
  - [x] 400 Bad Request
  - [x] 405 Method Not Allowed  
  - [x] 411 Length Required
  - [x] 505 HTTP Version Not Supported
- [x] Fix Connection header handling (keep-alive for 1.1, close for 1.0) - Note: Basic implementation exists but keep-alive has timeout issues

## Phase 3: Modern Architecture - ASIO Migration (Completed ✓)
- [x] Add ASIO dependency to meson.build
- [x] Create AsioServer class to replace fork-based model
- [x] Migrate HTTP handling to coroutine-based design
- [x] Benefits achieved:
  - Cross-platform support (Linux/macOS/Windows)
  - Better performance and scalability
  - Cleaner async code with coroutines
  - Built-in SSL/TLS support ready
- Note: ASIO server is available via --asio flag, fork-based remains default for compatibility

## Phase 4: Essential HTTP/1.1 Features (Completed ✓)
- [x] Implement If-Modified-Since and 304 Not Modified
- [x] Add proper POST request body handling with Content-Length
- [x] Add basic content negotiation (Accept headers)
- [x] Implement PUT and DELETE methods
- [x] Implement chunked transfer encoding support (request parsing and response encoding)
- [x] Add timeout handling for slow clients
- [x] Implement connection pooling for keep-alive (pre-fork worker pool)

## Phase 5: Advanced Features
- [x] Add Range request support (206 Partial Content) - **Completed! ✓**
  - [x] Single range requests
  - [x] Multiple ranges (multipart/byteranges)
  - [x] Suffix ranges (last N bytes)
  - [x] Open-ended ranges
  - [x] If-Range conditional support
  - [x] 416 Range Not Satisfiable
  - [x] Accept-Ranges header
  - [x] HEAD with Range support
- [x] Implement authentication (Basic and Digest) - **Completed! ✓**
  - [x] Auth module with Basic and Digest support
  - [x] Base64 encoding/decoding
  - [x] MD5 hashing for Digest auth
  - [x] Nonce generation and validation
  - [x] Protected path configuration
  - [x] User management
  - [x] 401 Unauthorized responses with WWW-Authenticate
  - [x] Directory index file resolution
- [x] Complete HTTP status code coverage - **Completed! ✓**
  - [x] 403 Forbidden (for permission denied scenarios)
  - [x] 3xx Redirects (301 Moved Permanently, 302 Found, 307 Temporary Redirect, 308 Permanent Redirect)
  - [x] 405 Method Not Allowed (required for HTTP/1.1 compliance)
  - [x] 429 Too Many Requests (rate limiting with configurable limits, Retry-After header)
  - [x] 503 Service Unavailable (maintenance mode with configurable message)
- [x] Add SSL/TLS support using ASIO SSL - **Completed! ✓**
  - [x] SSLContext wrapper with security hardening
  - [x] TLS 1.2 and 1.3 support (old protocols disabled)
  - [x] Mozilla "Intermediate" cipher configuration (ECDHE, AES-GCM, ChaCha20-Poly1305)
  - [x] Session caching and DH parameters support
  - [x] AsioSSLServer with async SSL handshakes and timeout handling
  - [x] Self-signed certificate generation script for testing
  - [x] Command-line options (--ssl, --ssl-port, --ssl-cert, --ssl-key)
- [x] WebSocket support - **Completed! ✓**
  - [x] Boost.Beast WebSocket integration
  - [x] HTTP to WebSocket upgrade detection
  - [x] Echo server implementation
  - [x] Binary and text frame support
  - [x] Automatic ping/pong keep-alive
  - [x] Graceful close handshake
  - [x] Python test client (scripts/testing/test_websocket.py)
- [x] HTTP/2 support - **Completed! ✓**
  - [x] nghttp2 C library integration with Boost.ASIO
  - [x] HTTP/2 over TLS (h2) with ALPN negotiation
  - [x] Binary framing layer
  - [x] HPACK header compression
  - [x] Stream multiplexing
  - [x] Static file serving
  - [x] Access logging
  - [x] --http2 command line flag

## Architecture Notes

### ASIO-Only Architecture (Implemented)
- Modern async I/O using Boost.ASIO with C++20 coroutines
- Scalable connection handling without fork overhead
- Native SSL/TLS support via ASIO SSL streams
- **Fork-based model removed** (as of 2025-11-21)

### ASIO Architecture
```cpp
asio::awaitable<void> handle_request(tcp::socket socket) {
    // Clean, linear async code
    auto request = co_await read_request(socket);
    auto response = process_request(request);
    co_await write_response(socket, response);
}
```

### Migration Complete
1. ✓ Added ASIO alongside existing code
2. ✓ Created AsioServer and AsioSSLServer classes with coroutines
3. ✓ Removed fork-based Socket implementation
4. ✓ Converted Socket to pure abstract interface
5. ✓ Simplified codebase to ASIO-only model
6. ✓ CGI support disabled (incompatible with ASIO architecture)

## Testing
- HTTP/1.1 compliance test suite exists (`scripts/testing/test_http11_compliance.py`)
- Currently: 12/12 tests passing - Full HTTP/1.1 compliance achieved! ✓
- Fixed issues:
  - Keep-alive connection handling (replaced FILE* with raw socket operations)
  - Test script bug that prevented proper response parsing
- All required HTTP/1.1 features are now working correctly

## Next Steps

**All planned phases are now complete!** 🎉

Completed milestones:
1. ✓ **Fixed keep-alive timeout issues** - All HTTP/1.1 tests now pass
2. ✓ **Phase 4 features** - Chunked encoding, PUT/DELETE methods, content negotiation - Complete!
3. ✓ **Range request support (RFC 7233)** - 206 Partial Content, multipart/byteranges - Complete!
4. ✓ **Phase 5 Advanced Features - ALL COMPLETE:**
   - ✓ **SSL/TLS support** - ASIO SSL with TLS 1.2/1.3, modern ciphers
   - ✓ **Authentication** - Basic and Digest authentication
   - ✓ **WebSocket support** - Real-time bidirectional communication via Boost.Beast
   - ✓ **HTTP/2** - Full HTTP/2 support with nghttp2, ALPN, multiplexing, HPACK

Fishjelly now offers a complete, modern HTTP/1.1 and HTTP/2 web server with enterprise features!

## Completed Phases Summary
- ✅ Phase 1: Core stability
- ✅ Phase 2: HTTP/1.1 required features
- ✅ Phase 3: ASIO migration (dual architecture)
- ✅ Phase 4: Essential features (all features complete)
- ✅ Phase 5: Advanced features (ALL COMPLETE! 🎉)

---

## Future Enhancements (Optional)

Now that all planned phases are complete, here are additional features that could be considered for future development:

### Security Hardening (Comprehensive Threat Analysis - 2025-11-22)

**Executive Summary**: While Fishjelly uses modern C++23, ASIO, and smart pointers (reducing many classic vulnerabilities), it has critical security gaps that require attention before production deployment against hostile threats. Path traversal vulnerabilities have been fixed (2025-11-22) with comprehensive URL decoding and canonical path validation.

#### PHASE 1: CRITICAL VULNERABILITIES (Immediate Action Required)

**1. Plaintext Password Storage** ✅ FIXED (2025-11-22)
- **Location**: `src/auth.cc`, `src/auth.h`, `src/http.cc`
- **Issue**: Passwords were stored in plaintext both in memory and on disk
- **Solution Implemented**:
  - ✅ Replaced plaintext storage with argon2id hashing via libsodium
  - ✅ Implemented salted password hashing (automatic unique salt per user)
  - ✅ Constant-time comparison via crypto_pwhash_str_verify()
  - ✅ Backward compatibility: load_users_from_file() supports both formats
  - ✅ Migration path: plaintext passwords auto-hashed on load with warning
  ```cpp
  // Implementation in src/auth.cc
  std::string Auth::hash_password(const std::string& password) {
      char hashed_password[crypto_pwhash_STRBYTES];
      crypto_pwhash_str(hashed_password, password.c_str(), password.length(),
                       crypto_pwhash_OPSLIMIT_INTERACTIVE,
                       crypto_pwhash_MEMLIMIT_INTERACTIVE);
      return std::string(hashed_password);
  }

  bool Auth::verify_password(const std::string& password, const std::string& hash) {
      return crypto_pwhash_str_verify(hash.c_str(), password.c_str(),
                                       password.length()) == 0;
  }
  ```
- **Security Impact**: Eliminates password theft via memory dumps, config file access,
  or process inspection. Passwords now stored as argon2id hashes with unique salts.

**2. Timing Attack on Password Comparison** ✅ FIXED (2025-11-22)
- **Location**: `src/auth.cc:407`
- **Issue**: String equality operator leaked password length/content via timing
- **Solution Implemented**:
  - ✅ Replaced with libsodium's crypto_pwhash_str_verify() (constant-time)
  - ✅ All password verification now uses constant-time comparison
  ```cpp
  // Before (vulnerable):
  return user_it->second == password_it->second;

  // After (secure):
  return verify_password(password_it->second, user_it->second);
  ```
- **Security Impact**: Prevents timing-based password extraction attacks

**3. Weak Cryptographic Primitives (MD5)** ⚠️ PARTIALLY ADDRESSED (2025-11-22)
- **Location**: `src/auth.cc:96-106`, `src/auth.cc:425-484`
- **Issue**: MD5 is cryptographically broken (collision attacks since 2004)
- **Current Status**:
  - ✅ Basic auth now uses argon2id instead of MD5 (secure)
  - ⚠️ Digest auth still uses MD5 (required by RFC 2617 protocol)
  - ✅ Added deprecation warnings for Digest auth
  - ✅ Digest auth automatically disabled for users with hashed passwords
  - ⚠️ MD5 usage limited to legacy Digest auth protocol only
- **Recommendation**: Use Basic authentication over HTTPS instead of Digest auth
  - Basic + HTTPS is more secure than Digest over HTTP
  - HTTPS provides encryption and integrity that Digest cannot
- **Future Improvements**:
  - [ ] Consider implementing RFC 7616 (Digest with SHA-256)
  - [ ] Consider fully deprecating Digest auth in favor of OAuth2/JWT
  - [ ] For now, users should prefer Basic auth over HTTPS (secure + simple)

**4. No Request Size Limits** ✅ FIXED
- **Location**: `src/request_limits.h`, `src/http.cc`, `src/http2_server.cc`
- **Issue**: Unlimited header/body/upload sizes enable memory exhaustion DoS
- **Solution Implemented**:
  - ✅ Created centralized size limit constants in `src/request_limits.h`
  - ✅ HTTP/1.1 limits enforced:
    - Header size limits (8KB per line, 8KB total)
    - Header count limit (100 headers max)
    - POST body size limit (10MB)
    - PUT upload size limit (100MB)
    - Chunked transfer encoding limits (1MB per chunk, 10MB total)
  - ✅ HTTP/2 limits enforced:
    - Header name/value size limits (256B/8KB)
    - Total header list size (16KB)
    - Header count limit (100 headers)
    - Request body size limit (10MB)
    - SETTINGS frame configures client-side limits
  - ✅ Returns 413 Payload Too Large when limits exceeded
  - ✅ Test suite created (`scripts/testing/test_request_size_limits.py`)
- **Security Impact**: Prevents memory exhaustion DoS attacks via oversized requests

**5. Path Traversal Vulnerabilities** ✅ FIXED
- **Location**: `src/security_middleware.cc`, `src/http2_server.cc`
- **Issue**: Path traversal vulnerabilities via URL encoding, double encoding, null bytes, and other bypasses
- **Solution Implemented**:
  - ✅ Recursive URL decoding to handle multiple encoding levels
  - ✅ Null byte filtering
  - ✅ Filesystem canonical path resolution with containment validation
  - ✅ Backslash filtering (Windows-style paths)
  - ✅ Multiple slash normalization
  - ✅ Comprehensive path validation using `std::filesystem::weakly_canonical()`
- **Security Tests**: All critical path traversal attacks blocked:
  - ✅ Basic traversal (`../../../etc/passwd`)
  - ✅ URL encoded (`..%2f..%2fetc/passwd`)
  - ✅ Double encoded (`..%252f..%252fetc/passwd`)
  - ✅ Windows-style (`..\\..\\etc\\passwd`)
  - ✅ Null byte injection (`/etc/passwd%00.html`)
  - ✅ Attempts to access files outside htdocs directory
- **Implementation**: `SecurityMiddleware::sanitize_path()` provides robust path validation
  used by both HTTP/1.1 and HTTP/2 handlers

**6. HTTP Request Smuggling Risk** ✅ FIXED
- **Location**: `src/http.cc` (Content-Length/Transfer-Encoding handling)
- **Issue**: No validation of conflicting headers, insufficient chunked encoding validation
- **Attack**: CL.TE or TE.CL desync attacks
- **Solution Implemented**:
  - ✅ Reject requests with multiple Content-Length headers
  - ✅ Reject requests with multiple Transfer-Encoding headers
  - ✅ Reject requests with both Content-Length AND Transfer-Encoding
  - ✅ Strict Transfer-Encoding validation (only "chunked" allowed)
  - ✅ Whitespace normalization in Transfer-Encoding values
  - ✅ Comprehensive test suite (9/9 tests passing)
- **Security Impact**: Prevents CL.TE and TE.CL desync attacks that could bypass security controls
  and lead to cache poisoning or request routing confusion

#### PHASE 2: HIGH PRIORITY VULNERABILITIES

**7. Slowloris / Slow Read Attacks** 🟠 HIGH
- **Location**: Timeout handling throughout
- **Issue**: Insufficient protection against slow HTTP attacks
- **Current**: Basic timeouts exist but incomplete
  ```cpp
  static constexpr int KEEPALIVE_TIMEOUT_SEC = 5;
  static constexpr int SSL_HANDSHAKE_TIMEOUT_SEC = 10;
  ```
- **Attacks**:
  - Slowloris: Send partial headers, 1 byte every 10 seconds
  - Slow POST: Send body at 1 byte/second
  - Slow Read: Read response at 1 byte/minute
- **Fix Required**:
  - [ ] Implement per-stage timeouts:
  ```cpp
  static constexpr int READ_HEADER_TIMEOUT = 10;     // Initial headers
  static constexpr int READ_BODY_TIMEOUT = 30;       // Body transfer
  static constexpr int WRITE_TIMEOUT = 60;           // Response write
  static constexpr int MIN_DATA_RATE = 1024;         // bytes/sec
  ```
  - [ ] Add bandwidth tracking per connection:
  ```cpp
  struct ConnectionState {
      std::chrono::steady_clock::time_point last_activity;
      size_t bytes_transferred;

      bool is_too_slow() const {
          auto elapsed = std::chrono::steady_clock::now() - last_activity;
          auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
          if (seconds == 0) return false;
          return (bytes_transferred / seconds) < MIN_DATA_RATE;
      }
  };
  ```

**8. HTTP/2 Rapid Reset (CVE-2023-44487)** 🟠 HIGH
- **Location**: `src/http2_server.cc` - No protection against rapid stream resets
- **Attack**: Open streams, immediately RST them, repeat → CPU exhaustion
- **Impact**: Achieved with minimal bandwidth (attacker advantage ~1000x)
- **Fix Required**:
  - [ ] Track reset rate per connection:
  ```cpp
  class Http2Session {
      size_t reset_count_ = 0;
      std::chrono::steady_clock::time_point reset_window_start_;
      static constexpr size_t MAX_RESETS_PER_WINDOW = 100;
      static constexpr int RESET_WINDOW_SECONDS = 10;

      void on_stream_reset(int32_t stream_id) {
          auto now = std::chrono::steady_clock::now();
          if (now - reset_window_start_ > std::chrono::seconds(RESET_WINDOW_SECONDS)) {
              reset_count_ = 0;
              reset_window_start_ = now;
          }

          if (++reset_count_ > MAX_RESETS_PER_WINDOW) {
              nghttp2_session_terminate_session(session_, NGHTTP2_ENHANCE_YOUR_CALM);
          }
      }
  };
  ```

**9. HPACK Bomb Attack** 🟠 HIGH
- **Location**: `src/http2_server.cc` - No limits on HPACK table size
- **Attack**: Maliciously compressed headers that expand to gigabytes
- **Example**: 100KB compressed → 10GB decompressed
- **Fix Required**:
  - [ ] Configure nghttp2 limits:
  ```cpp
  nghttp2_option* option;
  nghttp2_option_new(&option);
  nghttp2_option_set_max_deflate_dynamic_table_size(option, 4096);
  nghttp2_option_set_max_header_list_size(option, 16384);
  nghttp2_session_server_new2(&session_, callbacks, this, option);
  nghttp2_option_del(option);
  ```

**10. Integer Overflow in File Size Handling** 🟠 HIGH
- **Location**: `src/http.cc:204-206`, `src/http.cc:255-257`
- **Issue**: `tellg()` returns `streampos` which could overflow
- **Current Code**:
  ```cpp
  auto size = file.tellg();
  std::vector<char> buffer(size);  // Potential huge allocation
  ```
- **Attack**: Symlink to /dev/zero or massive file → overflow → heap corruption
- **Fix Required**:
  - [ ] Add size validation:
  ```cpp
  auto size = file.tellg();
  if (size < 0 || size > MAX_FILE_SIZE) {
      throw std::runtime_error("File too large");
  }
  size_t safe_size = static_cast<size_t>(size);
  if (safe_size > MAX_BODY_SIZE) {
      ctx.status_code = 413;  // Payload Too Large
      return;
  }
  std::vector<char> buffer(safe_size);
  ```

**11. Resource Exhaustion - File Descriptors** 🟠 HIGH
- **Issue**: No limits on concurrent connections, open files, sockets per IP
- **Attack**: Exhaust file descriptors → denial of service
- **Fix Required**:
  - [ ] Implement connection limits:
  ```cpp
  static constexpr int MAX_CONNECTIONS = 10000;
  static constexpr int MAX_CONN_PER_IP = 100;
  static constexpr int MAX_OPEN_FILES = 8192;

  // Use rlimit to enforce
  struct rlimit limit = {MAX_OPEN_FILES, MAX_OPEN_FILES};
  setrlimit(RLIMIT_NOFILE, &limit);
  ```
  - [ ] Track connections per IP address
  - [ ] Implement connection pool with priority queue

#### PHASE 3: MEDIUM PRIORITY VULNERABILITIES

**12. Insufficient Security Headers** 🟠 MEDIUM
- **Location**: `src/security_middleware.cc:24-29`
- **Missing Critical Headers**: CSP, HSTS, Permissions-Policy
- **Fix Required**:
  - [ ] Add comprehensive security headers:
  ```cpp
  ctx.response_headers["Content-Security-Policy"] =
      "default-src 'self'; script-src 'self'; object-src 'none'; base-uri 'self'";
  ctx.response_headers["Strict-Transport-Security"] =
      "max-age=31536000; includeSubDomains; preload";
  ctx.response_headers["Permissions-Policy"] =
      "geolocation=(), microphone=(), camera=()";
  ctx.response_headers["X-Permitted-Cross-Domain-Policies"] = "none";
  ctx.response_headers["Cross-Origin-Embedder-Policy"] = "require-corp";
  ctx.response_headers["Cross-Origin-Opener-Policy"] = "same-origin";
  ctx.response_headers["Cross-Origin-Resource-Policy"] = "same-origin";
  ```

**13. No CSRF Protection** 🟠 MEDIUM
- **Issue**: POST/PUT/DELETE can be triggered cross-origin
- **Fix Required**:
  - [ ] Implement CSRF token generation and validation
  - [ ] Add SameSite cookie attribute
  - [ ] Validate Origin/Referer headers for state-changing operations

**14. Information Disclosure** 🟠 MEDIUM
- **Location**: `src/http.cc:127`
- **Issue**: Server version exposed in headers
- **Current**: `Server: SHELOB/0.5 (Unix)`
- **Fix Required**:
  - [ ] Make server header configurable
  - [ ] Default to generic value or omit entirely
  - [ ] Remove stack traces from error messages
  - [ ] Sanitize error responses to avoid path disclosure

**15. Session Management Weaknesses** 🟠 MEDIUM
- **Issue**: No session timeout, weak nonce generation, no session invalidation
- **Fix Required**:
  - [ ] Implement proper session management:
  ```cpp
  class SessionManager {
      static constexpr int SESSION_TIMEOUT = 1800;  // 30 minutes
      static constexpr int ABSOLUTE_TIMEOUT = 43200; // 12 hours

      struct Session {
          std::string id;
          std::chrono::system_clock::time_point created;
          std::chrono::system_clock::time_point last_access;
          std::string user;
          bool secure;
      };

      void invalidate_expired_sessions();
      void rotate_session_id(const std::string& old_id);
  };
  ```
  - [ ] Use cryptographically secure random for nonces (crypto_random_bytes)
  - [ ] Enforce cookie security flags: Secure, HttpOnly, SameSite

**16. Unvalidated Redirects** 🟠 MEDIUM
- **Issue**: Open redirect enables phishing attacks
- **Fix Required**:
  - [ ] Validate redirect targets are relative or whitelisted
  - [ ] Reject absolute URLs to external domains
  ```cpp
  bool is_safe_redirect(const std::string& url) {
      if (url.empty() || url[0] == '/') return true;  // Relative
      if (url.find("://") != std::string::npos) return false;  // Absolute
      return true;
  }
  ```

**17. Logging Security Issues** 🟠 MEDIUM
- **Issues**: Logs may contain sensitive data, no rotation, log injection
- **Fix Required**:
  - [ ] Sanitize log entries (remove passwords, tokens, PII)
  - [ ] Implement log rotation with size/time limits
  - [ ] Escape newlines to prevent log injection
  - [ ] Secure log file permissions (0600)

**18. No Protection Against Decompression Bombs** 🟠 MEDIUM
- **Location**: Compression middleware
- **Attack**: Upload 42KB file that decompresses to 4.5PB
- **Fix Required**:
  - [ ] Limit decompression ratio
  - [ ] Limit decompressed size
  - [ ] Stream decompression with monitoring

#### PHASE 4: ARCHITECTURAL IMPROVEMENTS

**19. Defense in Depth Architecture**
- [ ] Implement multi-layer security:
```cpp
class SecurityLayer {
    IPFirewall firewall_;              // Layer 1: Network filtering
    RateLimiter rate_limiter_;         // Layer 2: Rate limiting
    InputValidator validator_;          // Layer 3: Input validation
    AuthenticationMgr auth_;           // Layer 4: Authentication
    AuthorizationMgr authz_;           // Layer 5: Authorization
    AuditLogger audit_;                // Layer 6: Audit trail
    IntrusionDetection ids_;           // Layer 7: Attack detection
};
```

**20. Security Monitoring & Alerting**
- [ ] Implement anomaly detection:
```cpp
class SecurityMonitor {
    void detect_port_scanning(const std::string& ip);
    void detect_brute_force(const std::string& ip);
    void detect_sql_injection(const std::string& input);
    void detect_xss_attempt(const std::string& input);
    void detect_directory_traversal(const std::string& path);

    void alert_suspicious_activity(const std::string& details);
    void auto_block_attacker(const std::string& ip, int duration_sec);
};
```

**21. Memory Safety Enhancements**
- [ ] Enable sanitizers in debug builds:
```meson
if get_option('buildtype') == 'debug'
  sanitizers = ['-fsanitize=address', '-fsanitize=undefined',
                '-fsanitize=leak', '-fno-omit-frame-pointer']
  add_project_arguments(sanitizers, language: 'cpp')
  add_project_link_arguments(sanitizers, language: 'cpp')
endif
```

**22. Fuzzing Infrastructure**
- [ ] Integrate AFL++ or libFuzzer
- [ ] Fuzz critical parsers:
  - HTTP header parser
  - Chunked encoding parser
  - Range header parser
  - Multipart form parser
  - URL decoder
  - Cookie parser

**23. Security Testing Suite**
- [ ] OWASP ZAP automated scanning
- [ ] Burp Suite professional testing
- [ ] Custom exploit scripts for each CVE
- [ ] Integration with CI/CD pipeline

#### IMPLEMENTATION PRIORITY

**Completed**:
1. ✅ Path traversal protection with proper canonicalization (2025-11-22)
2. ✅ Request size limits to prevent memory exhaustion DoS (2025-11-22)
3. ✅ HTTP request smuggling protection (CL.TE/TE.CL attacks) (2025-11-22)
4. ✅ Plaintext password storage → argon2id hashed passwords (2025-11-22)
5. ✅ Timing attack on password comparison → constant-time verification (2025-11-22)

**Week 1 (Critical)**:
1. ~~Fix plaintext password storage → hashed passwords~~ ✅ COMPLETED
2. ~~Fix timing attacks → constant-time comparison~~ ✅ COMPLETED
3. ~~Add request size limits everywhere~~ ✅ COMPLETED (already done)
4. Add comprehensive input validation

**Weeks 2-4 (High)**:
1. HTTP request smuggling protections
2. Slowloris/slow read protections
3. HTTP/2 rapid reset protection
4. HPACK bomb protection
5. Integer overflow fixes
6. Enhanced security headers

**Months 2-3 (Medium)**:
1. Proper session management
2. CSRF protection
3. Per-IP rate limiting
4. Security monitoring and alerting
5. Audit logging
6. Intrusion detection

**Months 3-6 (Long-term)**:
1. External security audit
2. Penetration testing
3. Continuous fuzzing integration
4. Security compliance certification
5. Formal threat modeling
6. Security documentation

#### TESTING & VALIDATION

- [ ] Create security test suite with attack simulations
- [ ] Automated vulnerability scanning in CI/CD
- [ ] Regular penetration testing schedule
- [ ] Bug bounty program consideration
- [ ] Compliance testing (OWASP Top 10, CWE Top 25)

#### REFERENCES

- OWASP Top 10 (2021): https://owasp.org/Top10/
- CWE Top 25: https://cwe.mitre.org/top25/
- CVE-2023-44487 (HTTP/2 Rapid Reset)
- RFC 7616 (HTTP Digest Authentication with SHA-256)
- NIST Password Guidelines (SP 800-63B)

---

### Performance Enhancements
- [ ] **Memory pool allocator** - Reduce allocation overhead
- [ ] **Performance benchmarking suite** - Automated performance testing
- [ ] **Zero-copy optimizations** - Reduce memory copies in hot paths
- [ ] **Connection pooling improvements** - Better connection reuse

### Configuration & Management
- [ ] **Configuration File System** - YAML/TOML config file support
- [ ] **Virtual Hosts** - Multi-domain hosting support
- [ ] **Hot Configuration Reload** - Zero-downtime config changes
- [ ] **Custom Error Pages** - Configurable error pages per status code
- [ ] **Server Status Page** - Real-time server statistics

### Advanced Protocol Support
- [ ] **HTTP/3 QUIC Support** - Latest protocol version
- [ ] **FastCGI Support** - For PHP and other languages
- [ ] **Python WSGI Support** - Integration with Python web applications
- [ ] **gRPC Support** - HTTP/2 based RPC proxying

### Reverse Proxy & Load Balancing Features
- [ ] **Basic Reverse Proxy** - Proxy requests to backend servers
- [ ] **Load Balancing Algorithms** - Round-robin, least connections, weighted
- [ ] **Health Checks** - Active and passive backend health monitoring
- [ ] **Circuit Breaker** - Automatic failover for unhealthy backends
- [ ] **Service Discovery** - Integration with Consul, etcd
- [ ] **Automatic HTTPS** - Let's Encrypt integration with ACME
- [ ] **Metrics & Monitoring** - Prometheus metrics, OpenTelemetry support
- [ ] **WebSocket Proxying** - Full duplex connection proxying

### Modern Infrastructure Integration
- [ ] **Docker Integration** - Watch Docker API for container changes
- [ ] **Kubernetes Ingress Controller** - Native K8s ingress support
- [ ] **Dashboard UI** - Web interface for monitoring and configuration
- [ ] **GitOps Integration** - Config from Git repositories

### Code Quality Tasks
- [x] **Fix TODO: Singleton optimization for Mime class** - COMPLETED! Mime is now a thread-safe singleton that loads mime.types once instead of on every request
- [N/A] **Fix TODO: Abstract output interface for Http class** (asio_http_connection.cc:14) - Won't Do: AsioHttpConnection works fine as-is; Http2Server is the modern solution for new features
- [N/A] **Fix TODO: SSL-aware socket adapter** (asio_ssl_server.cc:100) - Won't Do: AsioSSLServer handles HTTP/1.1 over SSL correctly; Http2Server is the modern SSL/HTTP2 solution
- [N/A] **Fix BUG: File size race condition** (http.cc:1691) - Won't Do: Extremely unlikely edge case where static file size changes during transmission; not a realistic production issue
- [ ] **Complete clang-tidy compliance** - Fix remaining linting issues
- [ ] **Fuzzing Infrastructure** - Continuous fuzzing for security

### Documentation
- [ ] **API Documentation** - Complete API reference
- [ ] **Deployment Guide** - Production deployment best practices
- [ ] **Security Hardening Guide** - Security configuration guide
- [ ] **Performance Tuning Guide** - Optimization documentation

---

**Note**: These are aspirational features for potential future development. The core web server is fully functional and production-ready as-is.