# ASIO Migration Status

## Overview
Started migration from fork-per-connection model to modern ASIO-based architecture with C++20 coroutines.

## Completed
- ✅ Added Boost.Asio dependencies to build system
- ✅ Created AsioServer class with coroutine-based accept loop
- ✅ Implemented async HTTP request reading with timeout support
- ✅ Added --asio/-a command line flag to enable ASIO mode
- ✅ Basic ASIO server responds to requests (test response)
- ✅ Proper signal handling (SIGINT, SIGTERM)
- ✅ Test mode support (exit after N requests)

## Architecture Benefits
1. **Better Keep-Alive Support**: Coroutines make persistent connections natural
2. **Improved Scalability**: Single process handles many connections efficiently
3. **Timeout Handling**: Built-in timeout support for keep-alive connections
4. **Modern C++**: Uses C++20 coroutines for clean async code
5. **Gradual Migration**: Runs alongside existing fork-based server

## Current Status
The ASIO server infrastructure is in place and responding to requests with a test message.
The server properly handles:
- Connection acceptance
- HTTP request reading (with \r\n\r\n detection)
- Keep-alive timeouts (5 seconds)
- Graceful shutdown
- Test mode

## Next Steps
1. Integrate full HTTP handler with ASIO server
2. Create output abstraction to bridge HTTP handler with ASIO sockets
3. Implement proper keep-alive connection handling
4. Add support for concurrent request processing
5. Performance testing and optimization

## Usage
```bash
# Run server in ASIO mode
./builddir/src/shelob -p 8080 -a

# Run in ASIO mode with test mode (exit after 10 requests)
./builddir/src/shelob -p 8080 -a -t 10

# Run in traditional fork mode (default)
./builddir/src/shelob -p 8080
```