#!/bin/bash
# Test script for If-Modified-Since functionality

echo "=== Testing If-Modified-Since ==="

# Start server in test mode for 3 requests
echo "Starting server for 3 requests..."
./builddir/src/shelob -p 8080 -t 3 &
SERVER_PID=$!
sleep 1

# Test 1: Request with old date should return 200
echo -e "\nTest 1: If-Modified-Since with old date (should return 200)"
echo -e "GET /index.html HTTP/1.1\r\nHost: localhost\r\nIf-Modified-Since: Wed, 01 Jan 2020 00:00:00 GMT\r\nConnection: close\r\n\r\n" | nc localhost 8080 -w 2 | head -20

# Test 2: Request with future date should return 304
echo -e "\nTest 2: If-Modified-Since with future date (should return 304)"
echo -e "GET /index.html HTTP/1.1\r\nHost: localhost\r\nIf-Modified-Since: Wed, 01 Jan 2030 00:00:00 GMT\r\nConnection: close\r\n\r\n" | nc localhost 8080 -w 2 | head -20

# Test 3: HEAD request with future date should return 304
echo -e "\nTest 3: HEAD with If-Modified-Since future date (should return 304)"
echo -e "HEAD /index.html HTTP/1.1\r\nHost: localhost\r\nIf-Modified-Since: Wed, 01 Jan 2030 00:00:00 GMT\r\nConnection: close\r\n\r\n" | nc localhost 8080 -w 2 | head -20

# Wait for server to exit
wait $SERVER_PID
echo -e "\nTests complete."