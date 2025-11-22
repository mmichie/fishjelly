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
- [ ] WebSocket support
- [ ] HTTP/2 support

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

The next logical steps for the project are:
1. ✓ **Fixed keep-alive timeout issues** - All HTTP/1.1 tests now pass
2. ✓ **Phase 4 features** - Chunked encoding, PUT/DELETE methods, content negotiation - Complete!
3. ✓ **Range request support (RFC 7233)** - 206 Partial Content, multipart/byteranges - Complete!
4. **Phase 5 Advanced Features:**
   - **SSL/TLS support** - Leverage ASIO SSL capabilities
   - **Authentication** - Basic and Digest authentication
   - **WebSocket support** - Enable real-time bidirectional communication
   - **HTTP/2** - Modern protocol features

## Completed Phases Summary
- ✅ Phase 1: Core stability
- ✅ Phase 2: HTTP/1.1 required features
- ✅ Phase 3: ASIO migration (dual architecture)
- ✅ Phase 4: Essential features (all features complete)
- ⏳ Phase 5: Advanced features (pending)