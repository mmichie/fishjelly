#!/bin/bash
# Test POST request handling in fishjelly

echo "Testing POST request handling..."
echo "================================"

# Test 1: Simple POST with data
echo "Test 1: Simple POST with form data"
curl -X POST -d "name=John&email=john@example.com" http://localhost:8080/test
echo -e "\n"

# Test 2: POST with JSON
echo "Test 2: POST with JSON data"
curl -X POST -H "Content-Type: application/json" \
     -d '{"user":"alice","message":"Hello World"}' \
     http://localhost:8080/api/test
echo -e "\n"

# Test 3: Empty POST
echo "Test 3: POST with empty body"
curl -X POST http://localhost:8080/empty
echo -e "\n"

# Test 4: POST without Content-Length (using nc)
echo "Test 4: POST without Content-Length (should return 411)"
echo -e "POST /test HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8080
echo -e "\n"

echo "Tests completed!"