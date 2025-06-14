#!/bin/bash

echo "=== Testing POST with application/x-www-form-urlencoded ==="
curl -X POST http://localhost:8080/submit \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "username=john_doe&email=john%40example.com&message=Hello+World%21&age=25"

echo -e "\n\n=== Testing POST with multipart/form-data ==="
curl -X POST http://localhost:8080/upload \
  -F "username=jane_doe" \
  -F "email=jane@example.com" \
  -F "bio=Software developer" \
  -F "interests=Python, C++, Web"

echo -e "\n\n=== Testing POST with JSON ==="
curl -X POST http://localhost:8080/api/test \
  -H "Content-Type: application/json" \
  -d '{"name":"Test API","version":"1.0","features":["auth","logging"]}'

echo -e "\n\n=== Testing POST with plain text ==="
curl -X POST http://localhost:8080/text \
  -H "Content-Type: text/plain" \
  -d "This is a plain text message."

echo -e "\n\n=== Testing empty POST ==="
curl -X POST http://localhost:8080/empty \
  -H "Content-Length: 0"

echo -e "\n\n=== Testing special characters in URL encoding ==="
curl -X POST http://localhost:8080/special \
  -d "special=Hello%2BWorld&percent=100%25&space=Hello+World"

echo -e "\n"