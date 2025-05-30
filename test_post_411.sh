#!/bin/bash
# Test script for POST 411 Length Required

echo "=== Testing POST 411 Length Required ==="

# Start server in test mode for 2 requests
echo "Starting server for 2 requests..."
./builddir/src/shelob -p 8080 -t 2 &
SERVER_PID=$!
sleep 1

# Test 1: POST without Content-Length should return 411
echo -e "\nTest 1: POST without Content-Length"
echo -e "POST /test HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8080 -w 2 | head -20

# Test 2: POST with Content-Length should work (even if we don't process the body properly yet)
echo -e "\nTest 2: POST with Content-Length"
echo -e "POST /test HTTP/1.1\r\nHost: localhost\r\nContent-Length: 13\r\n\r\nHello, World!" | nc localhost 8080 -w 2 | head -20

# Wait for server to exit
wait $SERVER_PID
echo -e "\nTests complete."