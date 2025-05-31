#!/bin/bash

# Compare current version (with ASIO) against old fork-only version

echo "=== Version Comparison Benchmark ==="
echo

# Save current state
CURRENT_BRANCH=$(git branch --show-current)
CURRENT_COMMIT=$(git rev-parse HEAD)

# Configuration
OLD_COMMIT="dd7b65d"  # Before ASIO implementation
BENCHMARK_DIR="benchmark_comparison"
PORT=8080

# Create benchmark directory
mkdir -p $BENCHMARK_DIR

# Function to build and benchmark a version
benchmark_version() {
    local version_name=$1
    local commit=$2
    local use_asio=$3
    
    echo -e "\n### Benchmarking $version_name (commit: $commit) ###"
    
    # Checkout version
    if [ "$commit" != "current" ]; then
        echo "Checking out $commit..."
        git checkout $commit -q
    fi
    
    # Clean and build
    echo "Building..."
    meson setup builddir --wipe 2>/dev/null || meson setup builddir
    meson compile -C builddir
    
    # Start server
    if [ "$use_asio" = "true" ]; then
        ./builddir/src/shelob -p $PORT -a &
    else
        ./builddir/src/shelob -p $PORT &
    fi
    SERVER_PID=$!
    sleep 2
    
    # Run tests
    OUTPUT_FILE="$BENCHMARK_DIR/${version_name}_results.txt"
    
    echo "Running tests..."
    
    # 1. Simple throughput test with ab (Apache Bench)
    if command -v ab &> /dev/null; then
        echo -e "\n--- Apache Bench Test ---" > $OUTPUT_FILE
        ab -n 10000 -c 10 -k http://localhost:$PORT/index.html 2>&1 | grep -E "Requests per second:|Time per request:|Transfer rate:" >> $OUTPUT_FILE
    fi
    
    # 2. Concurrent connection test
    echo -e "\n--- Concurrent Connection Test ---" >> $OUTPUT_FILE
    python3 -c "
import concurrent.futures
import requests
import time

def test_request():
    try:
        r = requests.get('http://localhost:$PORT/index.html', timeout=5)
        return r.status_code == 200
    except:
        return False

start = time.time()
with concurrent.futures.ThreadPoolExecutor(max_workers=50) as executor:
    futures = [executor.submit(test_request) for _ in range(1000)]
    results = [f.result() for f in futures]
    
elapsed = time.time() - start
successful = sum(results)
print(f'Requests: 1000')
print(f'Successful: {successful}')
print(f'Time: {elapsed:.2f}s')
print(f'Req/sec: {1000/elapsed:.2f}')
" >> $OUTPUT_FILE
    
    # 3. Memory usage test
    echo -e "\n--- Resource Usage ---" >> $OUTPUT_FILE
    ps aux | grep shelob | grep -v grep | awk '{print "Memory (RSS): " $6 " KB"}' >> $OUTPUT_FILE
    
    # Stop server
    kill $SERVER_PID 2>/dev/null
    wait $SERVER_PID 2>/dev/null
    
    echo "Results saved to $OUTPUT_FILE"
}

# Benchmark old version (fork only)
benchmark_version "old_fork" $OLD_COMMIT "false"

# Return to current version
echo -e "\nReturning to current version..."
git checkout $CURRENT_BRANCH -q

# Benchmark current version - fork mode
benchmark_version "current_fork" "current" "false"

# Benchmark current version - ASIO mode
benchmark_version "current_asio" "current" "true"

# Generate comparison report
echo -e "\n\n=== COMPARISON REPORT ==="
echo "========================="

# Function to extract metric
extract_metric() {
    local file=$1
    local pattern=$2
    grep "$pattern" "$BENCHMARK_DIR/$file" | head -1
}

echo -e "\nThroughput (Requests/second):"
echo "Old Fork:     $(extract_metric "old_fork_results.txt" "Requests per second:")"
echo "Current Fork: $(extract_metric "current_fork_results.txt" "Requests per second:")"
echo "Current ASIO: $(extract_metric "current_asio_results.txt" "Requests per second:")"

echo -e "\nConcurrent Requests (from Python test):"
echo "Old Fork:"
extract_metric "old_fork_results.txt" "Req/sec:"
echo "Current Fork:"
extract_metric "current_fork_results.txt" "Req/sec:"
echo "Current ASIO:"
extract_metric "current_asio_results.txt" "Req/sec:"

echo -e "\nMemory Usage:"
echo "Old Fork:     $(extract_metric "old_fork_results.txt" "Memory")"
echo "Current Fork: $(extract_metric "current_fork_results.txt" "Memory")"
echo "Current ASIO: $(extract_metric "current_asio_results.txt" "Memory")"

echo -e "\nDetailed results in: $BENCHMARK_DIR/"

# Cleanup - return to original commit
git checkout $CURRENT_COMMIT -q 2>/dev/null