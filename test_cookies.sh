#!/bin/bash

echo "=== Test 1: Initial visit to cookie demo (no cookies) ==="
curl -i http://localhost:8080/cookie-demo

echo -e "\n\n=== Test 2: Second visit (with visit_count cookie) ==="
curl -i -H "Cookie: visit_count=1" http://localhost:8080/cookie-demo

echo -e "\n\n=== Test 3: Visit with multiple cookies ==="
curl -i -H "Cookie: visit_count=5; user=john; theme=dark" http://localhost:8080/cookie-demo

echo -e "\n\n=== Test 4: Setting a cookie ==="
curl -i http://localhost:8080/set-cookie?name=favorite&value=chocolate

echo -e "\n\n=== Test 5: Clearing cookies ==="
curl -i http://localhost:8080/clear-cookies

echo -e "\n\n=== Test 6: Testing cookie parsing edge cases ==="
curl -i -H 'Cookie: test1=value1; test2="quoted value"; test3=value3' http://localhost:8080/cookie-demo

echo -e "\n"