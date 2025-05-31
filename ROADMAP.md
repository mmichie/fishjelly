# Fishjelly Development Roadmap

## Phase 1: Core HTTP/1.1 Compliance (Completed ✓)
- [x] Fix server crash after handling requests (fork model issue)

## Phase 2: HTTP/1.1 Required Features (High Priority)
- [ ] Implement Host header validation (return 400 for HTTP/1.1 without Host)
- [ ] Add HTTP version parsing and proper handling
- [ ] Implement OPTIONS method (mandatory for HTTP/1.1)
- [ ] Add missing critical status codes:
  - [ ] 400 Bad Request
  - [ ] 405 Method Not Allowed  
  - [ ] 411 Length Required
  - [ ] 505 HTTP Version Not Supported
- [ ] Fix Connection header handling (keep-alive for 1.1, close for 1.0)

## Phase 3: Modern Architecture - ASIO Migration (High Priority)
- [ ] Add ASIO dependency to meson.build
- [ ] Create AsioServer class to replace fork-based model
- [ ] Migrate HTTP handling to coroutine-based design
- [ ] Benefits:
  - Cross-platform support (Linux/macOS/Windows)
  - Better performance and scalability
  - Cleaner async code with coroutines
  - Built-in SSL/TLS support

## Phase 4: Essential HTTP/1.1 Features (Medium Priority)
- [ ] Implement If-Modified-Since and 304 Not Modified
- [ ] Add proper POST request body handling with Content-Length
- [ ] Implement chunked transfer encoding support
- [ ] Add basic content negotiation (Accept headers)
- [ ] Implement PUT and DELETE methods
- [ ] Add timeout handling for slow clients
- [ ] Implement connection pooling for keep-alive

## Phase 5: Advanced Features (Low Priority)
- [ ] Add Range request support (206 Partial Content)
- [ ] Implement authentication (Basic and Digest)
- [ ] Add SSL/TLS support using ASIO SSL
- [ ] WebSocket support
- [ ] HTTP/2 support

## Architecture Notes

### Current Architecture (Fork-based)
- Simple but outdated
- One process per connection
- High overhead, doesn't scale

### Target Architecture (ASIO Coroutines)
```cpp
asio::awaitable<void> handle_request(tcp::socket socket) {
    // Clean, linear async code
    auto request = co_await read_request(socket);
    auto response = process_request(request);
    co_await write_response(socket, response);
}
```

### Migration Strategy
1. Add ASIO alongside existing code
2. Create new AsioServer class
3. Gradually move functionality
4. Remove old fork-based code
5. Leverage ASIO features (SSL, timers, etc.)

## Testing
- HTTP/1.1 compliance test suite exists (`scripts/testing/test_http11_compliance.py`)
- Currently: 1/12 tests passing
- Goal: Full compliance with HTTP/1.1 spec

## Timeline Estimate
- Phase 2: 2-3 days
- Phase 3: 3-4 days (ASIO migration)
- Phase 4: 3-4 days
- Phase 5: 5+ days

Total: ~2-3 weeks for full HTTP/1.1 compliance with modern architecture