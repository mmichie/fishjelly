#!/bin/bash
# Test script for keep-alive timeout functionality

echo "=== Testing Keep-Alive Timeout ==="

# Start server in debug and test mode
echo "Starting server in debug mode..."
./builddir/src/shelob -p 8080 -d -t 2 &
SERVER_PID=$!
sleep 1

# Test 1: Send request with keep-alive, then wait for timeout
echo -e "\nTest 1: Keep-alive request followed by timeout"
(
    echo -e "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
    # Wait for response
    sleep 1
    # Now wait 6 seconds to trigger timeout (timeout is 5 seconds)
    echo "Waiting for timeout..."
    sleep 6
) | nc localhost 8080 -w 10 &

NC_PID=$!

# Give it time to timeout
sleep 8

# Kill nc if still running
kill $NC_PID 2>/dev/null

# Test 2: Multiple requests on same keep-alive connection
echo -e "\nTest 2: Multiple requests on keep-alive connection"
(
    echo -e "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
    sleep 0.5
    echo -e "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n"
) | nc localhost 8080 -w 5

# Wait for server to exit
wait $SERVER_PID
echo -e "\nTests complete."