#!/usr/bin/env python3
"""
Benchmark tool for comparing fork vs ASIO server models
"""

import subprocess
import time
import requests
import concurrent.futures
import psutil
import statistics
import json
import os
import signal
from datetime import datetime

class ServerBenchmark:
    def __init__(self, port=8080):
        self.port = port
        self.server_process = None
        self.results = {}
        
    def start_server(self, use_asio=False, server_path="./builddir/src/shelob"):
        """Start the server with specified mode"""
        cmd = [server_path, "-p", str(self.port)]
        if use_asio:
            cmd.append("-a")
            
        print(f"Starting server: {' '.join(cmd)}")
        self.server_process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        time.sleep(2)  # Give server time to start
        
    def stop_server(self):
        """Stop the server"""
        if self.server_process:
            self.server_process.terminate()
            self.server_process.wait()
            
    def measure_throughput(self, duration=10, concurrent_requests=10):
        """Measure requests per second"""
        print(f"Measuring throughput for {duration} seconds with {concurrent_requests} concurrent requests...")
        
        start_time = time.time()
        request_count = 0
        errors = 0
        
        def make_request():
            nonlocal request_count, errors
            try:
                response = requests.get(f"http://localhost:{self.port}/index.html", 
                                      headers={"Connection": "close"})
                if response.status_code == 200:
                    request_count += 1
                else:
                    errors += 1
            except Exception:
                errors += 1
        
        with concurrent.futures.ThreadPoolExecutor(max_workers=concurrent_requests) as executor:
            futures = []
            while time.time() - start_time < duration:
                # Submit new requests
                for _ in range(concurrent_requests - len(futures)):
                    futures.append(executor.submit(make_request))
                
                # Clean up completed futures
                futures = [f for f in futures if not f.done()]
                time.sleep(0.01)  # Small delay to prevent CPU spinning
        
        elapsed = time.time() - start_time
        throughput = request_count / elapsed
        
        return {
            "requests_per_second": throughput,
            "total_requests": request_count,
            "errors": errors,
            "duration": elapsed
        }
    
    def measure_latency(self, num_requests=100):
        """Measure response time statistics"""
        print(f"Measuring latency for {num_requests} requests...")
        
        latencies = []
        errors = 0
        
        for _ in range(num_requests):
            start = time.time()
            try:
                response = requests.get(f"http://localhost:{self.port}/index.html",
                                      headers={"Connection": "close"})
                latency = (time.time() - start) * 1000  # Convert to ms
                if response.status_code == 200:
                    latencies.append(latency)
                else:
                    errors += 1
            except Exception:
                errors += 1
        
        if latencies:
            return {
                "mean_ms": statistics.mean(latencies),
                "median_ms": statistics.median(latencies),
                "min_ms": min(latencies),
                "max_ms": max(latencies),
                "p95_ms": statistics.quantiles(latencies, n=20)[18],  # 95th percentile
                "p99_ms": statistics.quantiles(latencies, n=100)[98],  # 99th percentile
                "stdev_ms": statistics.stdev(latencies) if len(latencies) > 1 else 0,
                "errors": errors
            }
        else:
            return {"error": "No successful requests"}
    
    def measure_concurrent_connections(self, max_concurrent=100):
        """Test how many concurrent connections can be handled"""
        print(f"Testing concurrent connections (up to {max_concurrent})...")
        
        successful = 0
        
        def test_connection():
            try:
                response = requests.get(f"http://localhost:{self.port}/index.html",
                                      timeout=5)
                return response.status_code == 200
            except:
                return False
        
        with concurrent.futures.ThreadPoolExecutor(max_workers=max_concurrent) as executor:
            futures = [executor.submit(test_connection) for _ in range(max_concurrent)]
            for future in concurrent.futures.as_completed(futures):
                if future.result():
                    successful += 1
        
        return {
            "max_attempted": max_concurrent,
            "successful": successful,
            "success_rate": successful / max_concurrent
        }
    
    def measure_resource_usage(self, duration=10):
        """Measure CPU and memory usage during load"""
        print(f"Measuring resource usage for {duration} seconds...")
        
        if not self.server_process:
            return {"error": "Server not running"}
        
        try:
            process = psutil.Process(self.server_process.pid)
        except:
            return {"error": "Cannot access process"}
        
        cpu_samples = []
        memory_samples = []
        
        # Generate load while measuring
        def generate_load():
            while time.time() - start_time < duration:
                try:
                    requests.get(f"http://localhost:{self.port}/index.html",
                               headers={"Connection": "close"})
                except:
                    pass
        
        # Start load generation in background
        with concurrent.futures.ThreadPoolExecutor(max_workers=5) as executor:
            start_time = time.time()
            load_futures = [executor.submit(generate_load) for _ in range(5)]
            
            # Sample resource usage
            while time.time() - start_time < duration:
                try:
                    cpu_samples.append(process.cpu_percent(interval=0.1))
                    memory_samples.append(process.memory_info().rss / 1024 / 1024)  # MB
                except:
                    break
                time.sleep(0.5)
        
        if cpu_samples and memory_samples:
            return {
                "cpu_percent_avg": statistics.mean(cpu_samples),
                "cpu_percent_max": max(cpu_samples),
                "memory_mb_avg": statistics.mean(memory_samples),
                "memory_mb_max": max(memory_samples),
                "samples": len(cpu_samples)
            }
        else:
            return {"error": "No samples collected"}
    
    def run_benchmark(self, server_mode, server_path="./builddir/src/shelob"):
        """Run complete benchmark suite"""
        print(f"\n{'='*60}")
        print(f"Benchmarking {server_mode} mode")
        print(f"{'='*60}")
        
        results = {"mode": server_mode, "timestamp": datetime.now().isoformat()}
        
        try:
            # Start server
            self.start_server(use_asio=(server_mode == "ASIO"), server_path=server_path)
            
            # Run benchmarks
            results["throughput"] = self.measure_throughput(duration=10, concurrent_requests=10)
            results["latency"] = self.measure_latency(num_requests=100)
            results["concurrent"] = self.measure_concurrent_connections(max_concurrent=50)
            results["resources"] = self.measure_resource_usage(duration=10)
            
        finally:
            self.stop_server()
        
        return results

def main():
    benchmark = ServerBenchmark(port=8080)
    
    # Check if we're in the right directory
    if not os.path.exists("builddir/src/shelob"):
        print("Error: builddir/src/shelob not found. Please run from project root.")
        return
    
    results = {}
    
    # Benchmark current version with both modes
    print("\nBenchmarking current version...")
    
    # Fork mode
    results["current_fork"] = benchmark.run_benchmark("Fork")
    
    # ASIO mode
    results["current_asio"] = benchmark.run_benchmark("ASIO")
    
    # Save results
    with open("benchmark_results.json", "w") as f:
        json.dump(results, f, indent=2)
    
    # Print comparison
    print(f"\n{'='*60}")
    print("BENCHMARK RESULTS SUMMARY")
    print(f"{'='*60}\n")
    
    print(f"{'Metric':<30} {'Fork':<20} {'ASIO':<20} {'Improvement':<15}")
    print("-" * 85)
    
    # Throughput comparison
    fork_rps = results["current_fork"]["throughput"]["requests_per_second"]
    asio_rps = results["current_asio"]["throughput"]["requests_per_second"]
    improvement = ((asio_rps - fork_rps) / fork_rps) * 100
    print(f"{'Throughput (req/s)':<30} {fork_rps:<20.1f} {asio_rps:<20.1f} {improvement:+.1f}%")
    
    # Latency comparison
    fork_lat = results["current_fork"]["latency"]["mean_ms"]
    asio_lat = results["current_asio"]["latency"]["mean_ms"]
    improvement = ((fork_lat - asio_lat) / fork_lat) * 100
    print(f"{'Mean Latency (ms)':<30} {fork_lat:<20.2f} {asio_lat:<20.2f} {improvement:+.1f}%")
    
    # P95 Latency
    fork_p95 = results["current_fork"]["latency"]["p95_ms"]
    asio_p95 = results["current_asio"]["latency"]["p95_ms"]
    improvement = ((fork_p95 - asio_p95) / fork_p95) * 100
    print(f"{'P95 Latency (ms)':<30} {fork_p95:<20.2f} {asio_p95:<20.2f} {improvement:+.1f}%")
    
    # Concurrent connections
    fork_conn = results["current_fork"]["concurrent"]["successful"]
    asio_conn = results["current_asio"]["concurrent"]["successful"]
    improvement = ((asio_conn - fork_conn) / fork_conn) * 100 if fork_conn > 0 else 0
    print(f"{'Concurrent Connections':<30} {fork_conn:<20} {asio_conn:<20} {improvement:+.1f}%")
    
    # CPU usage
    fork_cpu = results["current_fork"]["resources"]["cpu_percent_avg"]
    asio_cpu = results["current_asio"]["resources"]["cpu_percent_avg"]
    improvement = ((fork_cpu - asio_cpu) / fork_cpu) * 100
    print(f"{'CPU Usage (%)':<30} {fork_cpu:<20.1f} {asio_cpu:<20.1f} {improvement:+.1f}%")
    
    # Memory usage
    fork_mem = results["current_fork"]["resources"]["memory_mb_avg"]
    asio_mem = results["current_asio"]["resources"]["memory_mb_avg"]
    improvement = ((fork_mem - asio_mem) / fork_mem) * 100
    print(f"{'Memory Usage (MB)':<30} {fork_mem:<20.1f} {asio_mem:<20.1f} {improvement:+.1f}%")
    
    print(f"\n{'='*60}")
    print("Detailed results saved to benchmark_results.json")

if __name__ == "__main__":
    main()