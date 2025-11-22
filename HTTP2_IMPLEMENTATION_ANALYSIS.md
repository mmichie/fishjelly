# HTTP/2 Implementation Analysis for Fishjelly

## Executive Summary

Adding HTTP/2 support to fishjelly represents a **major architectural undertaking** with significant complexity. HTTP/2 is fundamentally different from HTTP/1.1, introducing binary framing, stream multiplexing, header compression (HPACK), and bidirectional flow control. This analysis examines three implementation approaches and provides detailed recommendations.

**Key Findings:**
- HTTP/2 protocol complexity is substantial (~20,000 LOC for reference implementation)
- Current fishjelly codebase: ~3,700 LOC
- Recommended approach: Integration with nghttp2 library
- Estimated effort: 3-6 weeks for basic integration, 8-12 weeks for production-ready implementation
- Implementation complexity: **High**

---

## Table of Contents

1. [Protocol Complexity Analysis](#protocol-complexity-analysis)
2. [Current Architecture Assessment](#current-architecture-assessment)
3. [Implementation Approaches](#implementation-approaches)
4. [Detailed Implementation Plan](#detailed-implementation-plan)
5. [Challenges and Risk Factors](#challenges-and-risk-factors)
6. [Effort Estimation](#effort-estimation)
7. [Recommendations](#recommendations)

---

## Protocol Complexity Analysis

### 1. Binary Framing Layer

**HTTP/1.1 (Current):**
- Text-based protocol with line-delimited headers
- Simple parsing: split on `\r\n`, parse key-value pairs
- Human-readable and debuggable with tools like `telnet`

**HTTP/2 (Required):**
- Binary frame protocol with 9-byte fixed header per frame
- Frame structure: Length (24-bit) | Type (8-bit) | Flags (8-bit) | Stream ID (31-bit) | Payload
- 10 frame types must be supported:
  - DATA (0x00) - Message payload
  - HEADERS (0x01) - Field blocks
  - PRIORITY (0x02) - Stream priority (deprecated but required for compatibility)
  - RST_STREAM (0x03) - Stream termination
  - SETTINGS (0x04) - Connection configuration
  - PUSH_PROMISE (0x05) - Server push initiation
  - PING (0x06) - Connection health check
  - GOAWAY (0x07) - Graceful shutdown
  - WINDOW_UPDATE (0x08) - Flow control
  - CONTINUATION (0x09) - Header block continuation
- Unknown frame types must be ignored (extensibility)
- Connection preface: Client sends magic string `"PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"` + SETTINGS frame

**Complexity Impact:** Complete rewrite of request/response parsing required. Current line-oriented parsing (http.cc:348-400) is incompatible.

### 2. Stream Multiplexing

**HTTP/1.1 (Current):**
```
Connection → Process one request → Send response → Process next request (keep-alive)
```
- Sequential request processing
- One active request per connection
- Simple state: keep-alive boolean
- Connection management: 5-second timeout between requests

**HTTP/2 (Required):**
```
Connection → Manage N concurrent streams (each with independent state)
Stream 1: HEADERS → DATA → DATA → END_STREAM
Stream 3: HEADERS → DATA → END_STREAM
Stream 5: HEADERS → PUSH_PROMISE → DATA → END_STREAM
```
- Concurrent stream processing on single connection
- Stream state machine (RFC 9113 §5.1):
  - idle → open → half-closed (local/remote) → closed
- Stream ID management:
  - Client-initiated: odd numbers (1, 3, 5, ...)
  - Server-initiated: even numbers (2, 4, 6, ...)
  - Stream 0: Reserved for connection-level frames
  - Must be strictly increasing (cannot reuse)
- Stream concurrency limit: SETTINGS_MAX_CONCURRENT_STREAMS (default 100)
- Stream priority and dependencies (deprecated in RFC 9113, but must parse for compatibility)

**Complexity Impact:** Requires per-stream state tracking, concurrent request handling, stream lifecycle management. Current single-request-at-a-time model (asio_server.cc:102-126) insufficient.

### 3. HPACK Header Compression

**HTTP/1.1 (Current):**
- Plain text headers: `Host: example.com\r\n`
- Simple key-value parsing
- Stored as `std::map<std::string, std::string>`

**HTTP/2 (Required):**
- HPACK (RFC 7541) compression with:
  - **Static table:** 61 predefined entries (index 1-61)
  - **Dynamic table:** FIFO buffer with configurable size
  - **Huffman encoding:** Variable-length string compression
  - **Integer encoding:** Variable-length with N-bit prefix
- Header representation types:
  - Indexed: Reference to static/dynamic table
  - Literal with indexing: Add to dynamic table
  - Literal without indexing: Do not add to table
  - Literal never indexed: Security-sensitive headers
- Dynamic table management:
  - Entry size = name_len + value_len + 32 bytes overhead
  - Eviction when size limit exceeded
  - SETTINGS_HEADER_TABLE_SIZE negotiation
- Stateful compression across entire connection
- Decompression errors (COMPRESSION_ERROR) terminate connection

**Complexity Impact:** Requires HPACK encoder/decoder implementation (~4,000 LOC in nghttp2). Stateful compression adds connection-level state. Parsing errors become connection errors, not recoverable.

### 4. Flow Control

**HTTP/1.1 (Current):**
- TCP-level flow control only
- No application-level control
- Simple: read request, send entire response

**HTTP/2 (Required):**
- **Two-level flow control:**
  - Connection-level (stream ID 0)
  - Per-stream level
- Initial window size: 65,535 bytes (both levels)
- WINDOW_UPDATE frames adjust sender's window
- Only DATA frames consume flow control credit
- Rules:
  - Cannot send DATA if window = 0
  - Receiving DATA beyond window → stream error
  - Must track windows for all active streams + connection
- SETTINGS_MAX_FRAME_SIZE: 16,384 to 16,777,215 bytes (default 16,384)

**Complexity Impact:** Requires window tracking per stream + connection, backpressure handling, frame splitting for large payloads.

### 5. Additional Features

**Connection Initialization:**
- TLS with ALPN (Application-Layer Protocol Negotiation)
  - Must advertise and accept "h2" identifier
  - Must reject "h2c" on TLS connections
- Cleartext with prior knowledge (optional)
- SETTINGS frame exchange and acknowledgment

**Error Handling:**
- Connection errors: GOAWAY + close TCP connection
- Stream errors: RST_STREAM + continue connection
- Error codes: PROTOCOL_ERROR, INTERNAL_ERROR, FLOW_CONTROL_ERROR, SETTINGS_TIMEOUT, STREAM_CLOSED, FRAME_SIZE_ERROR, REFUSED_STREAM, CANCEL, COMPRESSION_ERROR, CONNECT_ERROR, ENHANCE_YOUR_CALM, INADEQUATE_SECURITY, HTTP_1_1_REQUIRED

**Server Push (Optional):**
- Server initiates request via PUSH_PROMISE
- Client can refuse with RST_STREAM
- Requires careful resource prediction

**Priority (Deprecated but must support):**
- Stream dependencies and weights
- Priority frames and header fields
- RFC 9113 deprecates RFC 7540 priority scheme
- Still must parse for compatibility

---

## Current Architecture Assessment

### Codebase Metrics

**Fishjelly (Current):**
- 21 C++ implementation files
- 23 header files
- ~3,100 lines of C++ code
- ~600 lines of header code
- **Total: ~3,700 LOC**

**nghttp2 Core Library (Reference):**
- 26 C implementation files
- 27 header files
- ~18,000 lines of C code
- ~2,400 lines of header code
- **Total: ~20,000 LOC** (just the core library)

**Impact:** Native HTTP/2 implementation would more than quintuple codebase size.

### Architecture Analysis

**Current HTTP/1.1 Flow (src/asio_server.cc, src/http.cc):**

```
1. AsioServer::listener() accepts TCP connection
2. AsioServer::handle_connection() spawns coroutine
3. WebSocket upgrade check (before HTTP processing)
4. Loop while keep_alive:
   a. read_http_request() reads headers until "\r\n\r\n"
   b. Create AsioSocketAdapter wrapping socket
   c. Http::parseHeader() processes request
   d. Http dispatches to processGetRequest(), processPostRequest(), etc.
   e. Response written via Socket interface
5. Connection closed or continues (keep-alive)
```

**Key Architectural Characteristics:**

✅ **Strengths for HTTP/2:**
- Async I/O with C++20 coroutines (`co_await`, `asio::awaitable`)
- Clean socket abstraction (Socket interface → AsioSocketAdapter)
- Separation of protocol handling (Http) from transport (AsioServer)
- Middleware architecture (request/response chain)
- Existing SSL/TLS support via AsioSSLServer

❌ **Challenges for HTTP/2:**
- Sequential request processing (one request at a time per connection)
- Line-oriented parsing (`async_read_until "\r\n\r\n"`)
- No stream management or multiplexing
- Response buffering in memory (std::stringstream)
- No flow control beyond TCP
- Stateless per-request processing

### Component Compatibility

| Component | HTTP/1.1 Role | HTTP/2 Compatibility |
|-----------|---------------|---------------------|
| **AsioServer** | Connection acceptor, request reader | ✅ Can reuse connection handling, ❌ request reading incompatible |
| **AsioSocketAdapter** | Socket → Socket interface adapter | ❌ Assumes text-based protocol |
| **Http** | Request parser and dispatcher | ❌ Complete rewrite needed for binary framing |
| **Middleware** | Request/response chain | ✅ Concept reusable, ❌ API incompatible (needs stream context) |
| **Token** | Text tokenizer | ❌ Not applicable to binary frames |
| **Mime** | Content-Type detection | ✅ Fully reusable |
| **Log** | Access logging | ✅ Reusable with modifications (add stream ID) |
| **Auth** | Basic/Digest auth | ✅ Reusable (headers still present, just compressed) |
| **Filter** | Content modification | ✅ Reusable |
| **WebSocketHandler** | WebSocket via Boost.Beast | ❌ WebSocket over HTTP/2 uses different framing |

---

## Implementation Approaches

### Approach 1: Integration with nghttp2-asio (RECOMMENDED)

**Description:** Use nghttp2's C++ wrapper library (libnghttp2_asio) which provides high-level HTTP/2 server API built on Boost.ASIO.

**Architecture:**
```
Client → TLS with ALPN → Protocol Detection
                              ↓
                    ┌─────────┴──────────┐
                    ↓                    ↓
            HTTP/2 Handler         HTTP/1.1 Handler
            (nghttp2-asio)         (existing Http class)
                    ↓                    ↓
              Shared Application Logic
              (file serving, auth, middleware, logging)
```

**Pros:**
- ✅ Mature, battle-tested library (8,487+ commits)
- ✅ High-level API designed for ease of use
- ✅ Built on Boost.ASIO (matches existing architecture)
- ✅ Handles all protocol complexity (framing, HPACK, flow control)
- ✅ RFC 9113 compliant
- ✅ Active maintenance and security updates
- ✅ Server push support built-in
- ✅ Can share application logic between HTTP/1.1 and HTTP/2

**Cons:**
- ❌ Callback-based API (not coroutine-based)
- ❌ Separate io_service coordination required
- ❌ Additional dependency (libnghttp2)
- ❌ API differences require abstraction layer
- ❌ Library was deprecated in nghttp2 mainline (moved to separate repo)
- ❌ Less control over low-level behavior

**Implementation Effort:** 3-6 weeks

**Code Example (from nghttp2 docs):**
```cpp
#include <nghttp2/asio_http2_server.h>

using namespace nghttp2::asio_http2;
using namespace nghttp2::asio_http2::server;

int main() {
    boost::system::error_code ec;
    http2 server;

    server.handle("/", [](const request &req, const response &res) {
        res.write_head(200);
        res.end("hello, world\n");
    });

    if (server.listen_and_serve(ec, "localhost", "3000")) {
        std::cerr << "error: " << ec.message() << std::endl;
    }
}
```

### Approach 2: Integration with nghttp2 C Library

**Description:** Use core nghttp2 C library directly, implementing custom event loop integration and session management.

**Architecture:**
```
AsioServer → Custom HTTP/2 Session Manager → nghttp2 C API
                                                ↓
                                    Frame callbacks
                                    Stream callbacks
                                    Data providers
```

**Pros:**
- ✅ Full control over integration
- ✅ Can implement coroutine-based API
- ✅ Smaller dependency footprint
- ✅ Direct integration with existing ASIO event loop
- ✅ No separate io_service needed
- ✅ Better performance potential

**Cons:**
- ❌ Significantly more complex implementation
- ❌ Callback hell (nghttp2 uses extensive callbacks)
- ❌ Must manually handle session lifecycle
- ❌ Must manually integrate with ASIO
- ❌ More opportunities for bugs
- ❌ Requires deep understanding of nghttp2 internals
- ❌ Higher maintenance burden

**Implementation Effort:** 8-12 weeks

**Code Snippet (callback setup):**
```cpp
nghttp2_session_callbacks *callbacks;
nghttp2_session_callbacks_new(&callbacks);
nghttp2_session_callbacks_set_send_callback(callbacks, send_callback);
nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, on_frame_recv_callback);
nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, on_stream_close_callback);
nghttp2_session_callbacks_set_on_header_callback(callbacks, on_header_callback);
nghttp2_session_callbacks_set_on_begin_headers_callback(callbacks, on_begin_headers_callback);
// ... 15+ more callbacks ...

nghttp2_session_server_new(&session, callbacks, user_data);
```

### Approach 3: Implement from Scratch

**Description:** Implement HTTP/2 protocol directly without external libraries.

**Architecture:**
```
HTTP/2 Frame Parser → Stream Manager → HPACK Encoder/Decoder → Flow Control
                                                ↓
                                        Application Logic
```

**Pros:**
- ✅ Complete control over implementation
- ✅ No external dependencies
- ✅ Can optimize for specific use cases
- ✅ Perfect integration with existing architecture
- ✅ Educational value

**Cons:**
- ❌ **MASSIVE implementation effort** (20,000+ LOC)
- ❌ High risk of bugs and security vulnerabilities
- ❌ Protocol compliance challenges
- ❌ HPACK alone is ~4,000 LOC
- ❌ Flow control complexity
- ❌ Stream state machine complexity
- ❌ Years to reach production quality
- ❌ Ongoing maintenance of protocol implementation
- ❌ No interoperability testing initially

**Implementation Effort:** 6-12 months (conservative estimate)

**NOT RECOMMENDED** unless there are exceptional requirements.

---

## Detailed Implementation Plan

### Recommended: Approach 1 (nghttp2-asio Integration)

This plan focuses on the recommended approach using nghttp2-asio library.

#### Phase 1: Foundation and Setup (Week 1)

**1.1 Dependency Integration**
- Add nghttp2 to build system (meson.build)
- Configure with `--enable-asio-lib` flag
- Update Meson to find nghttp2_asio library
- Add pkg-config checks for libnghttp2_asio
- Update CLAUDE.md with build instructions

**1.2 Protocol Detection**
- Implement ALPN configuration in AsioSSLServer
- Add "h2" protocol identifier to TLS context
- Modify connection handling to detect negotiated protocol
- Branch to appropriate handler based on ALPN result

**1.3 Basic Structure**
```cpp
// New files to create:
src/http2_server.h/cc          // HTTP/2 server wrapper
src/http2_handler.h/cc         // Request handler adapter
src/protocol_detector.h/cc     // ALPN-based detection
```

**Deliverable:** Build system configured, protocol detection working

#### Phase 2: Core HTTP/2 Integration (Weeks 2-3)

**2.1 HTTP/2 Server Wrapper**
```cpp
class Http2Server {
public:
    Http2Server(boost::asio::io_service& io_service,
                const std::string& address,
                const std::string& port);

    void start();
    void stop();

private:
    nghttp2::asio_http2::server::http2 server_;
    void configure_routes();
    void handle_request(const request& req, const response& res);
};
```

**2.2 Request Handler Adapter**
- Convert nghttp2 request/response objects to internal format
- Adapt callback-based API to work with existing middleware
- Handle URI parsing and method extraction
- Map HTTP/2 pseudo-headers (`:method`, `:path`, `:authority`, `:scheme`)

**2.3 Response Generation**
- Adapt existing response generation to nghttp2 API
- Convert headers to HTTP/2 format
- Handle response writing via `res.write_head()` and `res.end()`
- Implement chunked responses if needed

**Deliverable:** Basic HTTP/2 requests working (GET, HEAD)

#### Phase 3: Feature Parity (Weeks 3-4)

**3.1 HTTP Method Support**
- GET and HEAD (already implemented)
- POST with body reading
- PUT with body reading
- DELETE
- OPTIONS (CORS support)

**3.2 Static File Serving**
- Use nghttp2's `file_generator()` utility
- Integrate with existing Mime type detection
- Handle Range requests (if supported by nghttp2)
- Error page generation

**3.3 Authentication and Authorization**
- Adapt Basic authentication
- Adapt Digest authentication (if still relevant)
- Session/cookie handling

**3.4 Content Negotiation**
- Accept header processing
- Compression (may be redundant with HTTP/2 header compression)
- Content-Type selection

**Deliverable:** HTTP/2 feature parity with HTTP/1.1

#### Phase 4: HTTP/2-Specific Features (Week 5)

**4.1 Server Push (Optional but Recommended)**
```cpp
void handle_html_request(const request& req, const response& res) {
    // Push CSS
    auto push = res.push(boost::system::error_code(), "GET", "/style.css");
    push->write_head(200);
    push->end(read_file("/style.css"));

    // Send HTML
    res.write_head(200);
    res.end(read_file("/index.html"));
}
```

**4.2 Stream Priority Handling**
- Configure if nghttp2-asio exposes priority API
- Test with browser developer tools

**4.3 Protocol-Specific Logging**
- Add stream ID to access logs
- Log HTTP/2-specific events (push promises, stream resets)
- Performance metrics (concurrent streams)

**Deliverable:** HTTP/2-specific features implemented

#### Phase 5: Integration and Testing (Week 6)

**5.1 Dual-Protocol Support**
- Run HTTP/1.1 and HTTP/2 servers simultaneously
- Different ports or ALPN-based routing
- Shared application logic (file serving, auth, etc.)

**5.2 Configuration**
- Command-line option: `--http2` or `--protocol=http2`
- SSL certificate configuration for ALPN
- Performance tuning (max concurrent streams, frame size)

**5.3 Testing**
```bash
# HTTP/2 with curl
curl --http2 https://localhost:8443/

# HTTP/2 with nghttp client
nghttp -v https://localhost:8443/

# Load testing with h2load
h2load -n 1000 -c 10 https://localhost:8443/
```

**5.4 Browser Testing**
- Chrome DevTools Network tab (Protocol column)
- Firefox Developer Tools
- Safari Web Inspector
- Verify server push works
- Test multiplexing (multiple concurrent requests)

**Deliverable:** Production-ready HTTP/2 support

#### Phase 6: Documentation and Deployment (Ongoing)

**6.1 Documentation**
- Update README.md with HTTP/2 information
- Update CLAUDE.md with HTTP/2 build instructions
- Document configuration options
- Create HTTP/2 examples

**6.2 Performance Benchmarking**
- Compare HTTP/1.1 vs HTTP/2 performance
- Benchmark with h2load
- Profile CPU and memory usage
- Optimize hot paths

**Deliverable:** Complete documentation and benchmarks

---

## Challenges and Risk Factors

### Technical Challenges

**1. Callback vs Coroutine Impedance Mismatch**
- nghttp2-asio uses callbacks: `server.handle("/", [](const request& req, const response& res) { ... })`
- Fishjelly uses coroutines: `co_await read_http_request()`
- **Mitigation:** Create adapter layer that converts between paradigms
- **Risk Level:** Medium

**2. Separate I/O Service Coordination**
- nghttp2-asio creates its own `io_service` internally
- Fishjelly uses a single `io_context`
- **Mitigation:** Run both event loops, or investigate using same io_service
- **Risk Level:** Medium

**3. API Abstraction Layer**
- Two different request/response APIs (HTTP/1.1 and HTTP/2)
- Need unified interface for application logic
- **Mitigation:** Create abstraction that both can implement
```cpp
class HttpRequest {
    virtual std::string method() const = 0;
    virtual std::string uri() const = 0;
    virtual std::string header(const std::string& name) const = 0;
    // ...
};
```
- **Risk Level:** Medium

**4. Middleware Adaptation**
- Current middleware expects RequestContext with specific structure
- HTTP/2 has different lifecycle (streams vs connections)
- **Mitigation:** Refactor middleware interface to be protocol-agnostic
- **Risk Level:** Low-Medium

**5. WebSocket over HTTP/2**
- Current WebSocket uses HTTP/1.1 Upgrade
- HTTP/2 uses different mechanism (RFC 8441)
- **Mitigation:** Phase 2 concern, can maintain HTTP/1.1 WebSocket initially
- **Risk Level:** Low (can be deferred)

### Library-Specific Risks

**1. nghttp2-asio Deprecation**
- Original library deprecated from main nghttp2 repo
- Moved to separate repository: https://github.com/nghttp2/nghttp2-asio
- **Mitigation:** Monitor new repo, evaluate maintenance status
- **Risk Level:** Medium-High
- **Alternative:** Fall back to Approach 2 (C library) if necessary

**2. Limited Coroutine Support**
- nghttp2-asio predates C++20 coroutines
- May not integrate smoothly with existing coroutine code
- **Mitigation:** Investigate bridging callbacks to coroutines
```cpp
boost::asio::awaitable<void> handle_http2_request(...) {
    // Bridge nghttp2 callback to coroutine
}
```
- **Risk Level:** Medium

**3. Performance Overhead**
- High-level API may have overhead
- Multiple event loops may cause context switching
- **Mitigation:** Benchmark and profile, optimize if needed
- **Risk Level:** Low

### Operational Challenges

**1. Debugging Complexity**
- Binary protocol not human-readable
- Multiple concurrent streams harder to debug
- **Mitigation:** Use specialized tools (wireshark with HTTP/2 dissector, nghttp client with verbose mode)
- **Risk Level:** Medium

**2. Backward Compatibility**
- Must maintain HTTP/1.1 support
- Clients may not support HTTP/2
- **Mitigation:** Dual-stack implementation, ALPN negotiation
- **Risk Level:** Low

**3. TLS Configuration**
- ALPN requires proper TLS setup
- Cipher suite restrictions (RFC 9113 §9.2.2)
- **Mitigation:** Follow RFC recommendations, test with ssllabs.com
- **Risk Level:** Low

---

## Effort Estimation

### Approach 1: nghttp2-asio Integration (Recommended)

| Phase | Task | Effort | Dependencies |
|-------|------|--------|--------------|
| 1 | Foundation and Setup | 5-8 days | None |
| 2 | Core HTTP/2 Integration | 8-10 days | Phase 1 |
| 3 | Feature Parity | 8-10 days | Phase 2 |
| 4 | HTTP/2-Specific Features | 5-7 days | Phase 3 |
| 5 | Integration and Testing | 5-8 days | Phase 4 |
| 6 | Documentation | 2-3 days | Phase 5 |
| | **Total (Optimistic)** | **4-6 weeks** | |
| | **Total (Realistic)** | **6-8 weeks** | |

**Assumptions:**
- Experienced C++ developer
- Familiar with Boost.ASIO
- Access to testing infrastructure
- No major blockers or library issues

### Approach 2: nghttp2 C Library Integration

| Phase | Effort |
|-------|--------|
| Session management and callbacks | 2-3 weeks |
| ASIO integration | 1-2 weeks |
| Request/response handling | 2-3 weeks |
| Testing and debugging | 2-3 weeks |
| **Total** | **8-12 weeks** |

### Approach 3: Implement from Scratch

| Component | Effort |
|-----------|--------|
| Binary frame parser | 3-4 weeks |
| HPACK implementation | 4-6 weeks |
| Stream state machine | 3-4 weeks |
| Flow control | 2-3 weeks |
| Connection management | 2-3 weeks |
| Testing and compliance | 4-6 weeks |
| **Total** | **6-12 months** |

**NOT RECOMMENDED**

---

## Recommendations

### Primary Recommendation: Approach 1 with Contingency

**Implement nghttp2-asio integration with fallback plan:**

1. **Start with nghttp2-asio (Approach 1)**
   - Fastest time to working implementation
   - Proven, battle-tested library
   - Good starting point even if it needs replacement

2. **Monitor deprecation status**
   - Track https://github.com/nghttp2/nghttp2-asio
   - Check commit activity and issue responses
   - Watch for security updates

3. **Prepare contingency (Approach 2)**
   - If nghttp2-asio becomes unmaintained
   - If performance is insufficient
   - If API limitations become blockers
   - Fall back to C library integration
   - Already familiar with nghttp2 ecosystem

4. **Never pursue Approach 3**
   - Unless this becomes a protocol research project
   - Or there are extremely specific requirements
   - Cost/benefit ratio is prohibitive

### Implementation Strategy

**Phase 1: Proof of Concept (1 week)**
- Get nghttp2-asio building and linked
- Implement ALPN detection
- Handle one simple GET request via HTTP/2
- **Go/No-Go Decision Point:** If major blockers, reassess approach

**Phase 2: MVP (2-3 weeks)**
- Basic request routing
- Static file serving
- Authentication
- Logging
- **Go/No-Go Decision Point:** Evaluate performance and library stability

**Phase 3: Production (2-3 weeks)**
- Full feature parity
- Server push
- Comprehensive testing
- Documentation

**Phase 4: Optimization (Ongoing)**
- Performance tuning
- Memory optimization
- Load testing
- Bug fixes

### Architecture Recommendations

1. **Maintain HTTP/1.1 Support**
   - Don't remove or degrade existing functionality
   - HTTP/2 should be additive
   - Some clients don't support HTTP/2

2. **Protocol Abstraction Layer**
   - Create common interface for both protocols
   - Share application logic (auth, file serving, middleware)
   - Isolate protocol-specific code

3. **Configuration Flexibility**
   - Allow running HTTP/1.1-only (existing behavior)
   - Allow running HTTP/2-only (new mode)
   - Allow dual-stack (both protocols)

4. **Monitoring and Metrics**
   - Log protocol version in access logs
   - Track HTTP/2-specific metrics (streams, push promises)
   - Monitor performance differences

### Success Criteria

**Must Have:**
- ✅ Successful ALPN negotiation
- ✅ Basic request/response cycle (GET, POST, PUT, DELETE)
- ✅ Static file serving
- ✅ Authentication working
- ✅ No regressions in HTTP/1.1 support
- ✅ Browser compatibility (Chrome, Firefox, Safari)

**Should Have:**
- ✅ Server push implementation
- ✅ Performance equal or better than HTTP/1.1
- ✅ Comprehensive test coverage
- ✅ Production-ready error handling

**Nice to Have:**
- ✅ HTTP/2 server push for optimized page loads
- ✅ Advanced stream priority handling
- ✅ WebSocket over HTTP/2 (RFC 8441)

---

## Conclusion

HTTP/2 implementation is a **substantial but achievable** undertaking for fishjelly. The protocol's complexity—binary framing, multiplexing, HPACK, flow control—represents approximately 5x the current codebase size if implemented from scratch.

**The recommended path forward:**
1. Use nghttp2-asio library for rapid implementation (4-6 weeks)
2. Monitor library maintenance status
3. Maintain HTTP/1.1 support for compatibility
4. Focus on HTTP/2 benefits: multiplexing, server push, header compression
5. Be prepared to shift to C library if needed (contingency plan)

**Expected outcomes:**
- Improved performance for modern browsers
- Better resource utilization (multiplexing)
- Reduced latency (server push)
- Future-proofing (HTTP/2 is now standard)

**Key success factor:** Proper abstraction between protocol handling and application logic, enabling both protocols to coexist and share functionality.

---

## References

- RFC 9113: HTTP/2 (https://www.rfc-editor.org/rfc/rfc9113.html)
- RFC 7541: HPACK Header Compression (https://datatracker.ietf.org/doc/html/rfc7541)
- nghttp2 Library (https://nghttp2.org/)
- nghttp2-asio Repository (https://github.com/nghttp2/nghttp2-asio)
- High Performance Browser Networking - HTTP/2 (https://hpbn.co/http2/)

---

*Analysis Date: November 22, 2025*
*Fishjelly Version: Based on current master branch (commit 93ae162)*
