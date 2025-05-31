#!/bin/bash
# Convenience script to run common tests

echo "Fishjelly Test Runner"
echo "===================="
echo ""

# Check if server is running
if ! nc -z localhost 8080 2>/dev/null; then
    echo "Warning: Server doesn't appear to be running on port 8080"
    echo "Start it with: ./builddir/src/shelob -p 8080"
    echo ""
fi

# Simple menu
echo "Available test suites:"
echo "1) HTTP/1.1 Compliance Tests"
echo "2) POST Request Tests" 
echo "3) Keep-Alive Tests"
echo "4) All Tests"
echo ""
echo -n "Select test suite (1-4): "
read choice

case $choice in
    1)
        echo "Running HTTP/1.1 compliance tests..."
        python3 scripts/testing/test_http11_compliance.py
        ;;
    2)
        echo "Running POST tests..."
        ./scripts/testing/test_post.sh
        ;;
    3)
        echo "Running keep-alive tests..."
        ./scripts/testing/test_keepalive_timeout.sh
        ;;
    4)
        echo "Running all tests..."
        echo "HTTP/1.1 Compliance:"
        python3 scripts/testing/test_http11_compliance.py
        echo -e "\n\nPOST Tests:"
        ./scripts/testing/test_post.sh
        echo -e "\n\nKeep-Alive Tests:"
        ./scripts/testing/test_keepalive_timeout.sh
        ;;
    *)
        echo "Invalid choice"
        exit 1
        ;;
esac