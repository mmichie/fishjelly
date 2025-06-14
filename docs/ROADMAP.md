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
- [x] **POST Body Processing** - ✅ Complete POST body handling with form parsing
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

## Reverse Proxy & Load Balancer Features (Traefik-like)

- [ ] **Dynamic Service Discovery** - Auto-discover backends from Docker/Kubernetes
- [ ] **Docker Integration** - Watch Docker API for container changes
- [ ] **Kubernetes Ingress Controller** - Native K8s ingress support
- [ ] **Automatic HTTPS** - Let's Encrypt integration with ACME
- [ ] **Load Balancing Algorithms** - Round-robin, least connections, weighted
- [ ] **Health Checks** - Active and passive backend health monitoring
- [ ] **Circuit Breaker** - Automatic failover for unhealthy backends
- [ ] **Retry Logic** - Configurable retry policies
- [ ] **Request/Response Modification** - Headers, path rewriting
- [ ] **Service Mesh Integration** - Consul, etcd service discovery
- [ ] **Metrics & Tracing** - Prometheus metrics, OpenTelemetry support
- [ ] **WebSocket Proxying** - Full duplex connection proxying
- [ ] **gRPC Support** - HTTP/2 based RPC proxying
- [ ] **Canary Deployments** - Traffic splitting and A/B testing
- [ ] **Rate Limiting per Service** - Different limits per backend
- [ ] **Authentication Middleware** - OAuth2, JWT, Basic Auth forwarding
- [ ] **Access Control** - IP whitelisting, API key validation
- [ ] **Dynamic Configuration** - Hot reload from file, API, or service discovery
- [ ] **Dashboard UI** - Web interface for monitoring and configuration
- [ ] **Multi-protocol Support** - TCP, UDP proxy capabilities

## Nice to Have (Enhanced Features)

- [ ] **HTTP/3 QUIC Support** - Latest protocol version
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

- [x] **Fix TODO: POST body processing** (http.cc:518) - ✅ Completed
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

- [ ] **Rust Components** - Memory-safe modules for critical paths
- [ ] **WASM Plugin System** - WebAssembly plugins for extending functionality
- [ ] **Edge Computing Mode** - Lightweight deployment for edge locations
- [ ] **AI/ML Integration** - Smart routing, anomaly detection, predictive scaling
- [ ] **Multi-cluster Support** - Federated proxy across multiple clusters
- [ ] **GitOps Integration** - Config from Git repositories
- [ ] **Serverless Backend Support** - Route to Lambda/Cloud Functions

---

**Note**: Items are roughly ordered by priority within each section. Check off items as they are completed.