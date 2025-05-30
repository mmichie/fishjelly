#!/bin/bash
# Test script for Host header validation

echo "=== Testing Host Header Validation ==="

# Start server in test mode for 2 requests
echo "Starting server for 2 requests..."
./builddir/src/shelob -p 8080 -t 2 &
SERVER_PID=$!
sleep 1

# Test 1: HTTP/1.1 without Host header (should get 400)
echo -e "\nTest 1: HTTP/1.1 without Host header"
echo -e "GET / HTTP/1.1\r\n\r\n" | nc localhost 8080 | head -1

# Test 2: HTTP/1.1 with Host header (should get 200)
echo -e "\nTest 2: HTTP/1.1 with Host header"
echo -e "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8080 | head -1

# Wait for server to exit
wait $SERVER_PID
echo -e "\nTests complete."