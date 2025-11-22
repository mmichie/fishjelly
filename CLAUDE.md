# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Using Make wrapper
```bash
make              # Build the project
make clean        # Clean build artifacts
make distclean    # Remove entire build directory
make rebuild      # Clean and rebuild from scratch
make run          # Run server on port 8080
make run-daemon   # Run server in daemon mode
make help         # Show all available targets
```

### Using Meson directly
```bash
# Configure (first time)
meson setup builddir

# Build
cd builddir
meson compile

# Clean build
meson compile --clean
meson compile

# Run the server (example on port 8080)
./shelob -p 8080
```

## Architecture Overview

Fishjelly is a lightweight HTTP web server written in C++23 using Boost.ASIO for async I/O with coroutines. The codebase follows object-oriented design principles with clear separation of concerns.

### Core Components

1. **Http (http.h/cc)**: Central HTTP protocol handler that processes GET, HEAD, POST, PUT, DELETE, and OPTIONS requests. Coordinates all other components for request processing.

2. **Socket (socket.h)**: Abstract interface defining socket operations (read_line, write_line, read_raw, write_raw). Implemented by AsioSocketAdapter for ASIO-based connections.

3. **AsioServer / AsioSSLServer (asio_server.h/cc, asio_ssl_server.h/cc)**: ASIO-based async servers using C++20 coroutines. Handle HTTP and HTTPS connections respectively with async I/O and connection management.

4. **AsioSocketAdapter (asio_socket_adapter.h/cc)**: Adapts ASIO sockets to the Socket interface, providing buffered I/O for HTTP protocol handling.

5. **WebSocketHandler (websocket_handler.h/cc)**: Handles WebSocket connections using Boost.Beast. Detects HTTP Upgrade requests and manages WebSocket sessions with echo functionality.

6. **Webserver (webserver.h/cc)**: Application entry point handling command-line parsing, daemon mode, signal management (SIGINT), and PID file operations.

### Component Relationships

- `Webserver` creates either `AsioServer` or `AsioSSLServer` based on SSL option
- Server accepts connections and spawns coroutines to handle each request
- Each connection creates an `AsioSocketAdapter` implementing the `Socket` interface
- `Http` instance processes requests using the `Socket` interface
- `Http` uses:
  - `Mime` for content type detection
  - `Log` for access logging
  - `Filter` for content modification
  - `Token` for parsing headers
  - `Auth` for authentication (Basic and Digest)
  - `ContentNegotiator` for content type negotiation
  - Middleware chain for request/response processing

### Runtime Structure

The server operates from the `base/` directory which contains:
- `htdocs/`: Web root for static files
- `logs/`: Access log location
- `mime.types`: MIME type configuration
- `ssl/`: SSL certificates and keys (for HTTPS mode)
- `fishjelly.pid`: PID file when running as daemon

### Key Design Patterns

- Async I/O with C++20 coroutines (awaitable/co_await)
- Abstract Socket interface for protocol handling
- Middleware chain pattern for request/response processing
- SSL/TLS support with modern cipher suites (TLS 1.2/1.3)
- WebSocket support via Boost.Beast (RFC 6455)
- File-based configuration (mime.types)
- Apache-style access logging

## Development Notes

- The binary is named "shelob" (historical name)
- Uses Meson build system
- Embeds Git commit hash during compilation
- Strict compilation flags: `-Wall -Wextra -Werror`
- C++23 standard with optimization flags
- Code formatting: 4-space indentation (see .clang-format)
- Modern C++ features:
  - C++20 coroutines (`co_await`, `asio::awaitable`)
  - `std::unique_ptr` and `std::shared_ptr` for ownership
  - `std::string_view` for read-only string parameters
  - `std::filesystem` for path operations
  - `std::vector` for dynamic buffers
  - `std::format` for string formatting (C++23)
  - Range-based for loops
  - Structured bindings
- Boost.ASIO for async networking:
  - TCP socket operations
  - SSL/TLS streams
  - Coroutine-based async I/O
  - Timer-based connection management