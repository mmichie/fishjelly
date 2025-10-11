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
- [ ] Implement authentication (Basic and Digest)
- [ ] Add SSL/TLS support using ASIO SSL
- [ ] WebSocket support
- [ ] HTTP/2 support

## Architecture Notes

### Dual Architecture Support
- Fork-based (default): Simple, battle-tested, compatible
- ASIO Coroutines (--asio flag): Modern, scalable, async

### ASIO Architecture (Implemented)
```cpp
asio::awaitable<void> handle_request(tcp::socket socket) {
    // Clean, linear async code
    auto request = co_await read_request(socket);
    auto response = process_request(request);
    co_await write_response(socket, response);
}
```

### Implementation Approach (Completed)
1. ✓ Added ASIO alongside existing code
2. ✓ Created AsioServer class with coroutines
3. ✓ Both models coexist for flexibility
4. Fork-based code retained for compatibility
5. ASIO features available when using --asio mode

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