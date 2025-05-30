#!/bin/bash
# Test script for HTTP version handling

echo "=== Testing HTTP Version Handling ==="

# Start server in test mode for 5 requests
echo "Starting server for 5 requests..."
./builddir/src/shelob -p 8080 -t 5 &
SERVER_PID=$!
sleep 1

# Test 1: HTTP/1.1 with Host header should succeed
echo -e "\nTest 1: HTTP/1.1 with Host header"
echo -e "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n" | nc localhost 8080 -w 2 | head -20

# Test 2: HTTP/1.0 without Host header should succeed
echo -e "\nTest 2: HTTP/1.0 without Host header"
echo -e "GET / HTTP/1.0\r\n\r\n" | nc localhost 8080 -w 2 | head -20

# Test 3: Invalid HTTP version should return 505
echo -e "\nTest 3: HTTP/2.0 should return 505"
echo -e "GET / HTTP/2.0\r\nHost: localhost\r\n\r\n" | nc localhost 8080 -w 2 | head -20

# Test 4: Unknown method should return 405
echo -e "\nTest 4: PATCH method should return 405"
echo -e "PATCH / HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8080 -w 2 | head -20

# Test 5: HTTP/1.1 default connection should be keep-alive
echo -e "\nTest 5: HTTP/1.1 default connection"
echo -e "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8080 -w 2 | head -20

# Wait for server to exit
wait $SERVER_PID
echo -e "\nTests complete."