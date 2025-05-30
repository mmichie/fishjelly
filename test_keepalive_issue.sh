#!/bin/bash
# Test to demonstrate the keep-alive issue

echo "=== Testing Keep-Alive Issue ==="

# Start ASIO server
./builddir/src/shelob -p 8080 -a -t 2 &
SERVER_PID=$!
sleep 1

echo -e "\nTest 1: Request with Connection: close (should work)"
echo -e "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n" | nc localhost 8080 -w 2

echo -e "\nTest 2: Request without Connection: close (will hang)"
echo "Sending request..."
# This will hang because server keeps connection alive but nc doesn't send more data
timeout 3 bash -c 'echo -e "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8080'
echo "Request timed out as expected"

kill $SERVER_PID 2>/dev/null
echo -e "\nDone."