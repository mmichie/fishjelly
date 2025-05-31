#!/bin/bash

echo "=== Testing Middleware Chain ==="
echo

# Start server with middleware logging
echo "Starting server with middleware logging..."
cd /Users/mim/src/fishjelly
./builddir/src/shelob -p 8080 -a &
SERVER_PID=$!
sleep 2

# Test 1: Security middleware blocks .env file
echo "1. Testing security middleware (blocked path)..."
curl -i "http://localhost:8080/.env" 2>/dev/null | head -10
echo

# Test 2: Normal request with logging
echo "2. Testing normal request with logging..."
curl -i "http://localhost:8080/index.html" 2>/dev/null | head -15
echo

# Test 3: Request .shtml file to test footer middleware
echo "3. Creating test.shtml file..."
cat > base/htdocs/test.shtml << 'EOF'
<html>
<head><title>Test SHTML</title></head>
<body>
<h1>Middleware Test Page</h1>
<p>This is a test page for middleware.</p>
</body>
</html>
EOF

echo "Requesting .shtml file (should have footer added)..."
curl "http://localhost:8080/test.shtml" 2>/dev/null
echo
echo

# Test 4: Check security headers
echo "4. Testing security headers..."
curl -I "http://localhost:8080/index.html" 2>/dev/null | grep -E "X-Content-Type|X-Frame|X-XSS|Referrer-Policy"
echo

# Test 5: Test compression middleware comment
echo "5. Creating large HTML file for compression test..."
cat > base/htdocs/large.html << 'EOF'
<html>
<head><title>Large File</title></head>
<body>
EOF

# Add 2KB of content
for i in {1..50}; do
    echo "<p>This is paragraph $i with some content to make the file larger for compression testing.</p>" >> base/htdocs/large.html
done

echo "</body></html>" >> base/htdocs/large.html

echo "Requesting large file (should have compression comment)..."
curl "http://localhost:8080/large.html" 2>/dev/null | head -5
echo

# Kill server
echo "Stopping server..."
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null

# Cleanup
rm -f base/htdocs/test.shtml base/htdocs/large.html

echo "=== Middleware test complete ===" 