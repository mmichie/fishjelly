#!/bin/bash

echo "=== Quick Version Comparison ==="
echo

# Save current branch
CURRENT_BRANCH=$(git branch --show-current)

# Test old version
echo "1. Testing old fork-only version (dd7b65d)..."
git checkout dd7b65d -q 2>/dev/null

# Build old version
echo "   Building..."
meson setup builddir_old 2>/dev/null
meson compile -C builddir_old

# Quick test
echo "   Running quick benchmark..."
./builddir_old/src/shelob -p 8080 &
OLD_PID=$!
sleep 2

# Simple throughput test using curl
OLD_RESULT=$(
    start=$(date +%s)
    for i in {1..1000}; do
        curl -s http://localhost:8080/index.html >/dev/null &
    done
    wait
    end=$(date +%s)
    echo "scale=2; 1000 / ($end - $start)" | bc
)

kill $OLD_PID 2>/dev/null
wait $OLD_PID 2>/dev/null

# Return to current version
echo -e "\n2. Testing current ASIO version..."
git checkout $CURRENT_BRANCH -q

# Test current ASIO version
./builddir/src/shelob -p 8080 -a &
NEW_PID=$!
sleep 2

# Simple throughput test
NEW_RESULT=$(
    start=$(date +%s)
    for i in {1..1000}; do
        curl -s http://localhost:8080/index.html >/dev/null &
    done
    wait
    end=$(date +%s)
    echo "scale=2; 1000 / ($end - $start)" | bc
)

kill $NEW_PID 2>/dev/null
wait $NEW_PID 2>/dev/null

# Results
echo -e "\n=== RESULTS ==="
echo "Old Fork version: ~$OLD_RESULT requests/second"
echo "New ASIO version: ~$NEW_RESULT requests/second"

# Cleanup
rm -rf builddir_old