#!/usr/bin/env python3
"""
Simple benchmark comparing fork vs ASIO using only standard library
"""

import subprocess
import time
import urllib.request
import urllib.error
import concurrent.futures
import statistics
import json
import os
import signal
from datetime import datetime

class SimpleBenchmark:
    def __init__(self, port=8080):
        self.port = port
        self.server_process = None
        
    def start_server(self, use_asio=False):
        """Start the server"""
        cmd = ["./builddir/src/shelob", "-p", str(self.port)]
        if use_asio:
            cmd.append("-a")
            
        print(f"Starting server: {' '.join(cmd)}")
        self.server_process = subprocess.Popen(
            cmd,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
        time.sleep(2)
        
    def stop_server(self):
        """Stop the server"""
        if self.server_process:
            self.server_process.terminate()
            try:
                self.server_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.server_process.kill()
                
    def make_request(self):
        """Make a single HTTP request"""
        try:
            start = time.time()
            with urllib.request.urlopen(f"http://localhost:{self.port}/index.html", timeout=5) as response:
                response.read()
                elapsed = time.time() - start
                return True, elapsed
        except Exception:
            return False, 0
            
    def benchmark_throughput(self, duration=10, workers=10):
        """Measure requests per second"""
        print(f"Measuring throughput for {duration} seconds...")
        
        request_count = 0
        errors = 0
        start_time = time.time()
        
        def worker():
            nonlocal request_count, errors
            while time.time() - start_time < duration:
                success, _ = self.make_request()
                if success:
                    request_count += 1
                else:
                    errors += 1
                    
        # Run workers
        with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as executor:
            futures = [executor.submit(worker) for _ in range(workers)]
            concurrent.futures.wait(futures)
            
        elapsed = time.time() - start_time
        throughput = request_count / elapsed
        
        return {
            "requests_per_second": throughput,
            "total_requests": request_count,
            "errors": errors,
            "duration": elapsed
        }
        
    def benchmark_latency(self, num_requests=100):
        """Measure response times"""
        print(f"Measuring latency for {num_requests} requests...")
        
        latencies = []
        errors = 0
        
        for i in range(num_requests):
            success, elapsed = self.make_request()
            if success:
                latencies.append(elapsed * 1000)  # Convert to ms
            else:
                errors += 1
                
        if latencies:
            latencies.sort()
            return {
                "mean_ms": statistics.mean(latencies),
                "median_ms": statistics.median(latencies),
                "min_ms": min(latencies),
                "max_ms": max(latencies),
                "p95_ms": latencies[int(len(latencies) * 0.95)],
                "p99_ms": latencies[int(len(latencies) * 0.99)],
                "errors": errors
            }
        else:
            return {"error": "No successful requests"}
            
    def benchmark_concurrent(self, max_concurrent=50):
        """Test concurrent connections"""
        print(f"Testing {max_concurrent} concurrent connections...")
        
        def test_connection():
            success, _ = self.make_request()
            return success
            
        with concurrent.futures.ThreadPoolExecutor(max_workers=max_concurrent) as executor:
            futures = [executor.submit(test_connection) for _ in range(max_concurrent)]
            results = [f.result() for f in concurrent.futures.wait(futures)[0]]
            
        successful = sum(results)
        return {
            "attempted": max_concurrent,
            "successful": successful,
            "failed": max_concurrent - successful,
            "success_rate": successful / max_concurrent * 100
        }
        
    def run_full_benchmark(self, mode_name, use_asio=False):
        """Run complete benchmark suite"""
        print(f"\n{'='*60}")
        print(f"Benchmarking {mode_name}")
        print(f"{'='*60}")
        
        results = {
            "mode": mode_name,
            "timestamp": datetime.now().isoformat()
        }
        
        try:
            self.start_server(use_asio)
            
            # Run benchmarks
            results["throughput"] = self.benchmark_throughput(duration=10, workers=10)
            results["latency"] = self.benchmark_latency(num_requests=100)
            results["concurrent"] = self.benchmark_concurrent(max_concurrent=50)
            
        finally:
            self.stop_server()
            
        return results

def main():
    # Check if we're in the right directory
    if not os.path.exists("builddir/src/shelob"):
        print("Error: Please run from the fishjelly project root directory")
        return
        
    benchmark = SimpleBenchmark(port=8080)
    
    # Run benchmarks
    fork_results = benchmark.run_full_benchmark("Fork Mode", use_asio=False)
    asio_results = benchmark.run_full_benchmark("ASIO Mode", use_asio=True)
    
    # Save results
    results = {
        "fork": fork_results,
        "asio": asio_results
    }
    
    with open("benchmark_results.json", "w") as f:
        json.dump(results, f, indent=2)
        
    # Print comparison
    print(f"\n{'='*80}")
    print("BENCHMARK RESULTS COMPARISON")
    print(f"{'='*80}\n")
    
    # Create comparison table
    print(f"{'Metric':<35} {'Fork Mode':<20} {'ASIO Mode':<20} {'Difference':<15}")
    print("-" * 90)
    
    # Throughput
    fork_rps = fork_results["throughput"]["requests_per_second"]
    asio_rps = asio_results["throughput"]["requests_per_second"]
    diff = ((asio_rps - fork_rps) / fork_rps) * 100
    print(f"{'Throughput (requests/sec)':<35} {fork_rps:<20.1f} {asio_rps:<20.1f} {diff:+.1f}%")
    
    # Total requests
    fork_total = fork_results["throughput"]["total_requests"]
    asio_total = asio_results["throughput"]["total_requests"]
    print(f"{'Total Requests (10s)':<35} {fork_total:<20} {asio_total:<20} {asio_total-fork_total:+}")
    
    # Errors
    fork_errors = fork_results["throughput"]["errors"]
    asio_errors = asio_results["throughput"]["errors"]
    print(f"{'Errors':<35} {fork_errors:<20} {asio_errors:<20} {asio_errors-fork_errors:+}")
    
    # Latency
    print(f"\n{'Latency Metrics':<35}")
    print("-" * 90)
    
    fork_mean = fork_results["latency"]["mean_ms"]
    asio_mean = asio_results["latency"]["mean_ms"]
    diff = ((fork_mean - asio_mean) / fork_mean) * 100
    print(f"{'  Mean (ms)':<35} {fork_mean:<20.2f} {asio_mean:<20.2f} {diff:+.1f}%")
    
    fork_median = fork_results["latency"]["median_ms"]
    asio_median = asio_results["latency"]["median_ms"]
    diff = ((fork_median - asio_median) / fork_median) * 100
    print(f"{'  Median (ms)':<35} {fork_median:<20.2f} {asio_median:<20.2f} {diff:+.1f}%")
    
    fork_p95 = fork_results["latency"]["p95_ms"]
    asio_p95 = asio_results["latency"]["p95_ms"]
    diff = ((fork_p95 - asio_p95) / fork_p95) * 100
    print(f"{'  95th Percentile (ms)':<35} {fork_p95:<20.2f} {asio_p95:<20.2f} {diff:+.1f}%")
    
    fork_p99 = fork_results["latency"]["p99_ms"]
    asio_p99 = asio_results["latency"]["p99_ms"]
    diff = ((fork_p99 - asio_p99) / fork_p99) * 100
    print(f"{'  99th Percentile (ms)':<35} {fork_p99:<20.2f} {asio_p99:<20.2f} {diff:+.1f}%")
    
    # Concurrent connections
    print(f"\n{'Concurrent Connections':<35}")
    print("-" * 90)
    
    fork_success = fork_results["concurrent"]["successful"]
    asio_success = asio_results["concurrent"]["successful"]
    diff = ((asio_success - fork_success) / fork_success) * 100 if fork_success > 0 else 0
    print(f"{'  Successful (out of 50)':<35} {fork_success:<20} {asio_success:<20} {diff:+.1f}%")
    
    fork_rate = fork_results["concurrent"]["success_rate"]
    asio_rate = asio_results["concurrent"]["success_rate"]
    print(f"{'  Success Rate (%)':<35} {fork_rate:<20.1f} {asio_rate:<20.1f} {asio_rate-fork_rate:+.1f}")
    
    print(f"\n{'='*80}")
    print("Detailed results saved to benchmark_results.json")
    
    # Summary
    print(f"\n{'SUMMARY':^80}")
    print(f"{'='*80}")
    
    if asio_rps > fork_rps:
        print(f"✓ ASIO mode is {((asio_rps - fork_rps) / fork_rps) * 100:.1f}% faster in throughput")
    else:
        print(f"✗ Fork mode is {((fork_rps - asio_rps) / asio_rps) * 100:.1f}% faster in throughput")
        
    if asio_mean < fork_mean:
        print(f"✓ ASIO mode has {((fork_mean - asio_mean) / fork_mean) * 100:.1f}% lower mean latency")
    else:
        print(f"✗ Fork mode has {((asio_mean - fork_mean) / asio_mean) * 100:.1f}% lower mean latency")
        
    if asio_success > fork_success:
        print(f"✓ ASIO mode handles {((asio_success - fork_success) / fork_success) * 100:.1f}% more concurrent connections")
    else:
        print(f"✗ Fork mode handles {((fork_success - asio_success) / asio_success) * 100:.1f}% more concurrent connections")

if __name__ == "__main__":
    main()