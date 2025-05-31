#!/bin/bash

echo "Building fuzz targets..."

# Check for required tools
if ! command -v clang++ &> /dev/null; then
    echo "Error: clang++ not found. Please install LLVM/Clang."
    exit 1
fi

# Create output directory
mkdir -p build

# Build libFuzzer targets
echo "Building libFuzzer targets..."

# HTTP Parser fuzzer
echo "  - Building fuzz_http_parser..."
clang++ -g -fsanitize=fuzzer,address,undefined \
    -I../src \
    fuzz_http_parser.cc \
    ../builddir/src/libfishjelly.a \
    -lboost_system -lboost_coroutine \
    -o build/fuzz_http_parser \
    2>/dev/null || echo "    Failed to build (might need to adjust paths)"

# Middleware fuzzer
echo "  - Building fuzz_middleware..."
clang++ -g -fsanitize=fuzzer,address,undefined \
    -I../src \
    fuzz_middleware.cc \
    ../builddir/src/libfishjelly.a \
    -lboost_system -lboost_coroutine \
    -o build/fuzz_middleware \
    2>/dev/null || echo "    Failed to build (might need to adjust paths)"

# Network fuzzer
echo "  - Building fuzz_network..."
clang++ -g -fsanitize=fuzzer,address,undefined \
    fuzz_network.cc \
    -o build/fuzz_network \
    2>/dev/null || echo "    Failed to build"

# AFL++ harness (if AFL++ is installed)
if command -v afl-clang++ &> /dev/null; then
    echo -e "\nBuilding AFL++ targets..."
    echo "  - Building afl_http_server..."
    AFL_USE_ASAN=1 afl-clang++ -g \
        -I../src \
        afl_http_server.cc \
        ../builddir/src/libfishjelly.a \
        -lboost_system -lboost_coroutine \
        -o build/afl_http_server \
        2>/dev/null || echo "    Failed to build (might need AFL++ and adjusted paths)"
else
    echo -e "\nSkipping AFL++ targets (AFL++ not found)"
fi

echo -e "\nFuzz targets built in ./build/"
echo ""
echo "To run libFuzzer:"
echo "  ./build/fuzz_http_parser corpus/"
echo "  ./build/fuzz_middleware corpus/"
echo ""
echo "To run AFL++ (if built):"
echo "  afl-fuzz -i corpus -o findings -- ./build/afl_http_server"