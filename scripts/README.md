# Fishjelly Scripts

This directory contains various scripts for testing, development, and maintenance of the Fishjelly web server.

## Directory Structure

### testing/
Test scripts for validating server functionality:
- `test_host_header.sh` - Tests HTTP/1.1 Host header requirements
- `test_http11_compliance.py` - Comprehensive HTTP/1.1 compliance test suite
- `test_http11_compliance_fixed.py` - Updated compliance tests
- `test_http_versions.sh` - Tests HTTP version handling
- `test_if_modified_since.sh` - Tests conditional GET requests
- `test_keepalive_issue.sh` - Tests for keep-alive connection issues
- `test_keepalive_timeout.sh` - Tests keep-alive timeout behavior
- `test_middleware.sh` - Tests middleware functionality
- `test_middleware_simple.sh` - Simple middleware tests
- `test_options.sh` - Tests OPTIONS method handling
- `test_post.py` - Comprehensive POST request tests (Python)
- `test_post.sh` - POST request tests (shell)
- `test_post_411.sh` - Tests POST without Content-Length
- `test_real_keepalive.py` - Real-world keep-alive testing
- `simple_test.py` - Basic server functionality tests

### development/
Development and code quality tools:
- `run_clang_tidy.sh` - Runs clang-tidy for static analysis

### benchmarking/ (../benchmark/)
Performance testing tools:
- `benchmark.py` - Main benchmarking script
- `simple_benchmark.py` - Simple performance tests
- `compare_versions.sh` - Compare performance between versions
- `quick_version_compare.sh` - Quick version comparison
- `wrk_benchmark.sh` - Benchmark using wrk tool

### fuzzing/ (../fuzz/)
Security and robustness testing:
- `build_fuzzers.sh` - Build fuzzing test binaries
- `run_basic_fuzz.sh` - Run basic fuzzing tests

## Usage

Most scripts can be run directly. Ensure the server is running before executing test scripts:

```bash
# Start the server
./builddir/src/shelob -p 8080

# Run a test script
./scripts/testing/test_post.sh
```

For Python scripts, you may need to install dependencies:
```bash
pip install requests
```