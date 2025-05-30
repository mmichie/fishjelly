# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

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

Fishjelly is a lightweight HTTP web server written in C++17 using a traditional Unix fork-per-connection model. The codebase follows object-oriented design principles with clear separation of concerns.

### Core Components

1. **Http (http.h/cc)**: Central HTTP protocol handler that processes GET, HEAD, and POST requests. This is the main integration point that coordinates all other components.

2. **Socket (socket.h/cc)**: Manages TCP socket operations including server binding, client connections, and network I/O. Implements the fork-based concurrent connection model.

3. **Webserver (webserver.h/cc)**: Application entry point handling command-line parsing, daemon mode, signal management (SIGCHLD, SIGINT), and PID file operations.

### Component Relationships

- `Webserver` creates and manages the main `Socket` instance
- `Socket` accepts connections and forks child processes
- Each child process creates an `Http` instance to handle the request
- `Http` uses:
  - `Mime` for content type detection
  - `CGI` for script execution
  - `Log` for access logging
  - `Filter` for content modification
  - `Token` for parsing headers

### Runtime Structure

The server operates from the `base/` directory which contains:
- `htdocs/`: Web root for static files
- `logs/`: Access log location
- `mime.types`: MIME type configuration
- `fishjelly.pid`: PID file when running as daemon

### Key Design Patterns

- Fork-per-connection for concurrency (no threading)
- Signal handling for proper child process cleanup
- File-based configuration (mime.types)
- Apache-style access logging
- Traditional CGI support with environment variable setup

## Development Notes

- The binary is named "shelob" (historical name)
- Uses Meson build system
- Embeds Git commit hash during compilation
- Strict compilation flags: `-Wall -Wextra -Werror`
- C++17 standard with optimization flags
- Code formatting: 4-space indentation (see .clang-format)