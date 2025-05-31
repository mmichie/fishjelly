#!/bin/bash

# Benchmark script using wrk
# Requires: brew install wrk

echo "=== Fishjelly Server Benchmark ==="
echo "Comparing Fork vs ASIO models"
echo

# Check for wrk
if ! command -v wrk &> /dev/null; then
    echo "Error: wrk not found. Please install with: brew install wrk"
    exit 1
fi

# Configuration
PORT=8080
DURATION=30s
THREADS=4
CONNECTIONS=100
URL="http://localhost:$PORT/index.html"

# Results file
RESULTS_FILE="wrk_benchmark_results.txt"
echo "Benchmark Results - $(date)" > $RESULTS_FILE
echo "==============================" >> $RESULTS_FILE

# Function to run benchmark
run_benchmark() {
    local mode=$1
    local server_cmd=$2
    
    echo -e "\n### Benchmarking $mode mode ###"
    echo -e "\n### $mode Mode ###" >> $RESULTS_FILE
    
    # Start server
    echo "Starting server..."
    $server_cmd &
    SERVER_PID=$!
    sleep 2
    
    # Check if server started
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo "Error: Server failed to start"
        return 1
    fi
    
    # Run wrk benchmark
    echo "Running benchmark (duration: $DURATION)..."
    
    # Basic throughput test
    echo -e "\n1. Throughput Test (${CONNECTIONS} connections)" | tee -a $RESULTS_FILE
    wrk -t$THREADS -c$CONNECTIONS -d$DURATION --latency $URL | tee -a $RESULTS_FILE
    
    # High connection test
    echo -e "\n2. High Connection Test (400 connections)" | tee -a $RESULTS_FILE
    wrk -t$THREADS -c400 -d10s --latency $URL | tee -a $RESULTS_FILE
    
    # Sustained load test
    echo -e "\n3. Sustained Load Test (10 connections, 60s)" | tee -a $RESULTS_FILE
    wrk -t2 -c10 -d60s --latency $URL | tee -a $RESULTS_FILE
    
    # Stop server
    kill $SERVER_PID 2>/dev/null
    wait $SERVER_PID 2>/dev/null
    
    echo "Benchmark complete for $mode mode"
}

# Build the server
echo "Building server..."
cd /Users/mim/src/fishjelly
meson compile -C builddir

# Run benchmarks
run_benchmark "Fork" "./builddir/src/shelob -p $PORT"
run_benchmark "ASIO" "./builddir/src/shelob -p $PORT -a"

# Parse and compare results
echo -e "\n\n=== COMPARISON SUMMARY ===" | tee -a $RESULTS_FILE

# Extract key metrics using awk
echo -e "\nExtracting metrics..."

# Function to extract metric from wrk output
extract_metric() {
    local mode=$1
    local test=$2
    local metric=$3
    
    grep -A20 "$test" $RESULTS_FILE | grep -A15 "$mode Mode" | grep "$metric" | head -1
}

# Compare throughput
echo -e "\nThroughput Comparison:" | tee -a $RESULTS_FILE
echo "Fork mode:" | tee -a $RESULTS_FILE
extract_metric "Fork" "Throughput Test" "Requests/sec" | tee -a $RESULTS_FILE
echo "ASIO mode:" | tee -a $RESULTS_FILE
extract_metric "ASIO" "Throughput Test" "Requests/sec" | tee -a $RESULTS_FILE

# Compare latency
echo -e "\nLatency Comparison:" | tee -a $RESULTS_FILE
echo "Fork mode:" | tee -a $RESULTS_FILE
extract_metric "Fork" "Throughput Test" "Latency" | tee -a $RESULTS_FILE
echo "ASIO mode:" | tee -a $RESULTS_FILE
extract_metric "ASIO" "Throughput Test" "Latency" | tee -a $RESULTS_FILE

echo -e "\nDetailed results saved to: $RESULTS_FILE"

# Create a simple comparison chart
echo -e "\n=== Quick Visual Comparison ==="
echo "Metric                Fork        ASIO        Winner"
echo "------                ----        ----        ------"

# This would need more sophisticated parsing, but gives the idea
# In practice, you'd parse the actual numbers and compare