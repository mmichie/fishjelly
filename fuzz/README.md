# Fuzzing the Fishjelly Web Server

This directory contains various fuzzing harnesses for testing the robustness and security of the fishjelly web server.

## Fuzzing Approaches

### 1. LibFuzzer (Recommended)

LibFuzzer is an in-process, coverage-guided fuzzer. We provide several targets:

- **fuzz_http_parser**: Fuzzes the HTTP request parser
- **fuzz_middleware**: Fuzzes the middleware chain
- **fuzz_network**: Network-based fuzzing (sends actual network requests)

#### Building LibFuzzer Targets

```bash
# Install LLVM/Clang if not already installed
brew install llvm  # macOS

# Build the fuzzers
./build_fuzzers.sh

# Or manually:
clang++ -g -fsanitize=fuzzer,address,undefined \
    -I../src fuzz_http_parser.cc \
    ../builddir/src/libfishjelly.a \
    -lboost_system -lboost_coroutine \
    -o fuzz_http_parser
```

#### Running LibFuzzer

```bash
# Basic fuzzing
./build/fuzz_http_parser corpus/

# With specific options
./build/fuzz_http_parser -max_len=1024 -timeout=10 corpus/

# Minimize corpus
./build/fuzz_http_parser -merge=1 corpus_minimized corpus/

# Run with dictionary
./build/fuzz_http_parser -dict=http.dict corpus/
```

### 2. AFL++ (Advanced)

AFL++ is a fork-based fuzzer with many advanced features:

```bash
# Install AFL++
brew install aflplusplus  # macOS
# or
git clone https://github.com/AFLplusplus/AFLplusplus
cd AFLplusplus && make && sudo make install

# Build with AFL++
AFL_USE_ASAN=1 afl-clang++ -g afl_http_server.cc -o afl_http_server

# Run AFL++
afl-fuzz -i corpus -o findings -- ./afl_http_server
```

### 3. Property-Based Testing

The property test harness tests invariants that should always hold:

```bash
# Build
clang++ -g property_test.cc ../builddir/src/libfishjelly.a \
    -lboost_system -lboost_coroutine -o property_test

# Run
./property_test
```

## Corpus

The `corpus/` directory contains seed inputs for the fuzzers:

- `valid_get.txt` - Valid GET request
- `valid_post.txt` - Valid POST request  
- `malformed_headers.txt` - Headers with various malformations
- `path_traversal.txt` - Path traversal attempts
- `http_methods.txt` - Various HTTP methods
- `edge_cases.txt` - Edge cases and unusual inputs

## Security Findings

When the fuzzer finds a crash or security issue:

1. The input will be saved in the `findings/` directory
2. Minimize the test case: `./fuzz_http_parser -minimize_crash=1 crash-input`
3. Debug with: `lldb ./fuzz_http_parser crash-input`
4. Fix the issue and add the input to the corpus for regression testing

## Continuous Fuzzing

For continuous fuzzing, consider:

1. **OSS-Fuzz Integration**: Google's continuous fuzzing service
2. **Local Setup**: Run fuzzers in tmux/screen sessions
3. **CI Integration**: Add fuzzing to your CI pipeline

Example continuous fuzzing script:

```bash
#!/bin/bash
# continuous_fuzz.sh

while true; do
    timeout 3600 ./build/fuzz_http_parser corpus/
    timeout 3600 ./build/fuzz_middleware corpus/
    
    # Minimize corpus periodically
    ./build/fuzz_http_parser -merge=1 corpus_min corpus/
    mv corpus_min corpus
done
```

## Best Practices

1. **Start with a good corpus**: Include valid and edge-case inputs
2. **Use sanitizers**: Always build with ASan, UBSan, and MSan
3. **Monitor coverage**: Use `-print_coverage=1` to track code coverage
4. **Minimize crashes**: Always minimize crash inputs before debugging
5. **Fix root causes**: Don't just patch the symptom

## Common Issues Found by Fuzzing

- Buffer overflows in header parsing
- Integer overflows in Content-Length handling
- Path traversal vulnerabilities
- Denial of service through resource exhaustion
- Use-after-free in connection handling
- Race conditions in threaded code

## HTTP Dictionary

Create an HTTP dictionary file for better fuzzing:

```
# http.dict
"GET"
"POST"
"HEAD"
"OPTIONS"
"HTTP/1.0"
"HTTP/1.1"
"Host:"
"Content-Length:"
"Content-Type:"
"User-Agent:"
"Accept:"
"Connection:"
"Keep-Alive"
"close"
"\r\n"
"/../"
"%2e%2e%2f"
"localhost"
"127.0.0.1"
```

## Integration with Development

1. Run fuzzing before releases
2. Add crash inputs to regression tests
3. Monitor fuzzing metrics (coverage, executions/sec)
4. Document security issues found and fixed