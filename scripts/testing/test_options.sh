#!/bin/bash
# Test script for OPTIONS method

echo "=== Testing OPTIONS Method ==="

# Start server in test mode for 2 requests
echo "Starting server for 2 requests..."
./builddir/src/shelob -p 8080 -t 2 &
SERVER_PID=$!
sleep 1

# Test 1: OPTIONS * (server-wide capabilities)
echo -e "\nTest 1: OPTIONS * HTTP/1.1"
echo -e "OPTIONS * HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8080 | head -10

# Test 2: OPTIONS for specific resource
echo -e "\nTest 2: OPTIONS /index.html HTTP/1.1"  
echo -e "OPTIONS /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8080 | head -10

# Wait for server to exit
wait $SERVER_PID
echo -e "\nTests complete."