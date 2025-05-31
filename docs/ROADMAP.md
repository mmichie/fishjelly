# Fishjelly Web Server Roadmap

This document outlines planned features and improvements for the Fishjelly web server.

## Critical Priority (Security & Stability)

- [ ] **HTTPS/TLS Support** - SSL/TLS implementation for secure connections
- [ ] **Request Size Limits** - Prevent memory exhaustion attacks
- [ ] **Rate Limiting** - Basic DoS protection with configurable limits
- [ ] **Proper Timeout Handling** - Complete timeout implementation for all operations
- [ ] **Robust Input Validation** - Enhanced header and URI validation

## High Priority (Core Functionality)

- [ ] **Configuration File System** - YAML/TOML config file support
- [ ] **Real gzip/deflate Compression** - Implement actual compression in middleware
- [ ] **POST Body Processing** - Complete POST body handling (marked TODO in code)
- [ ] **HTTP Range Requests** - Support for partial content (206 responses)
- [ ] **Chunked Transfer Encoding** - For streaming responses
- [ ] **Connection Limits** - Max connections per IP
- [ ] **Python WSGI Support** - Integration with Python web applications

## Medium Priority (Modern Standards)

- [ ] **HTTP/2 Support** - Multiplexing, server push, header compression
- [ ] **WebSocket Support** - Upgrade mechanism for real-time applications
- [ ] **Virtual Hosts** - Multi-domain hosting support
- [ ] **ETag Support** - Better caching with entity tags
- [ ] **URL Rewriting/Routing** - Flexible request handling rules
- [ ] **FastCGI Support** - For PHP and other languages
- [ ] **Caching Headers** - Complete cache-control implementation

## Nice to Have (Enhanced Features)

- [ ] **HTTP/3 QUIC Support** - Latest protocol version
- [ ] **Reverse Proxy Mode** - Proxy requests to backend servers
- [ ] **Load Balancing** - Distribute requests across backends
- [ ] **Built-in Monitoring** - /metrics endpoint for Prometheus
- [ ] **Hot Configuration Reload** - Zero-downtime config changes
- [ ] **Directory Listing** - Auto-generated directory indexes with templates
- [ ] **Basic Authentication** - Built-in HTTP basic auth
- [ ] **IP-based Access Control** - Allow/deny rules by IP
- [ ] **Custom Error Pages** - Configurable error pages per status code
- [ ] **Server Status Page** - Real-time server statistics
- [ ] **Server-Sent Events (SSE)** - For real-time data streaming
- [ ] **Module System** - Dynamic loading of features
- [ ] **GraphQL Support** - GraphQL endpoint handling

## Code Quality & Performance

- [ ] **Fix TODO: POST body processing** (http.cc:518)
- [ ] **Fix TODO: Singleton optimization for Log class** (http.cc:581, 643)
- [ ] **Fix TODO: Singleton optimization for Mime class** (http.cc:591, 653)
- [ ] **Fix TODO: Abstract output interface for Http class** (asio_http_connection.cc:15)
- [ ] **Fix BUG: File size race condition** (http.cc:657)
- [ ] **Complete clang-tidy compliance** - Fix remaining linting issues
- [ ] **Performance benchmarking suite** - Automated performance testing
- [ ] **Memory pool allocator** - Reduce allocation overhead

## Documentation & Testing

- [ ] **API Documentation** - Complete API reference
- [ ] **Deployment Guide** - Production deployment best practices
- [ ] **Security Hardening Guide** - Security configuration guide
- [ ] **Integration Tests** - Comprehensive test suite
- [ ] **Fuzzing Infrastructure** - Continuous fuzzing for security
- [ ] **Performance Tuning Guide** - Optimization documentation

## Future Considerations

- [ ] **Rust Rewrite** - Consider memory-safe language for core components
- [ ] **Container-Native Features** - Kubernetes health checks, graceful shutdown
- [ ] **Edge Computing Support** - Lightweight mode for edge deployments
- [ ] **AI/ML Integration** - Smart caching, anomaly detection

---

**Note**: Items are roughly ordered by priority within each section. Check off items as they are completed.