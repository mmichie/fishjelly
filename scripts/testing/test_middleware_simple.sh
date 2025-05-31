#!/bin/bash

echo "=== Simple Middleware Test ==="
echo

# Build
echo "Building project..."
cd /Users/mim/src/fishjelly
meson compile -C builddir

# Create test files
echo "Creating test files..."
cat > base/htdocs/test.shtml << 'EOF'
<html>
<head><title>Test SHTML</title></head>
<body>
<h1>Test Page</h1>
<p>This should have a footer added by middleware.</p>
</body>
</html>
EOF

# Start server in ASIO mode for middleware support
echo "Starting server in ASIO mode..."
./builddir/src/shelob -p 8080 -a &
SERVER_PID=$!
sleep 2

# Test regular HTML file
echo "1. Testing regular HTML file (no middleware modification):"
curl -s "http://localhost:8080/index.html"
echo
echo

# Test SHTML file
echo "2. Testing SHTML file (should have footer):"
curl -s "http://localhost:8080/test.shtml"
echo
echo

# Kill server
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

# Cleanup
rm -f base/htdocs/test.shtml

echo "=== Test complete ===" 