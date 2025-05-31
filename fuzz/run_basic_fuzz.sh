#!/bin/bash

echo "=== Basic Fuzzing Test ==="
echo

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Build the project first
echo "Building fishjelly..."
cd .. && meson compile -C builddir
cd fuzz

# Try to build a simple fuzzer
echo -e "\n${YELLOW}Building simple HTTP parser fuzzer...${NC}"

# Create a simplified fuzzer that doesn't need all dependencies
cat > simple_fuzz.cc << 'EOF'
#include <cstdint>
#include <cstddef>
#include <string>
#include <iostream>

// Simple HTTP request parser for demonstration
bool parse_http_request(const uint8_t* data, size_t size) {
    if (size == 0) return false;
    
    std::string request(reinterpret_cast<const char*>(data), size);
    
    // Look for method
    size_t space = request.find(' ');
    if (space == std::string::npos) return false;
    
    std::string method = request.substr(0, space);
    
    // Look for path
    size_t space2 = request.find(' ', space + 1);
    if (space2 == std::string::npos) return false;
    
    std::string path = request.substr(space + 1, space2 - space - 1);
    
    // Check for path traversal
    if (path.find("../") != std::string::npos) {
        std::cerr << "Path traversal detected!" << std::endl;
        return false;
    }
    
    // Check method
    if (method != "GET" && method != "POST" && method != "HEAD" && 
        method != "OPTIONS" && method != "PUT" && method != "DELETE") {
        std::cerr << "Unknown method: " << method << std::endl;
        return false;
    }
    
    return true;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    parse_http_request(data, size);
    return 0;
}
EOF

# Try to build with libFuzzer
if command -v clang++ &> /dev/null; then
    echo -e "${GREEN}Found clang++, building fuzzer...${NC}"
    clang++ -g -fsanitize=fuzzer,address simple_fuzz.cc -o simple_fuzzer 2>/dev/null
    
    if [ -f simple_fuzzer ]; then
        echo -e "${GREEN}Fuzzer built successfully!${NC}"
        
        # Run for a short time
        echo -e "\n${YELLOW}Running fuzzer for 10 seconds...${NC}"
        timeout 10 ./simple_fuzzer -max_total_time=10 corpus/ 2>&1 | grep -E "(NEW|pulse|DONE)"
        
        echo -e "\n${GREEN}Fuzzing completed!${NC}"
    else
        echo -e "${RED}Failed to build fuzzer with libFuzzer${NC}"
    fi
else
    echo -e "${RED}clang++ not found. Please install LLVM/Clang for fuzzing support.${NC}"
    echo "On macOS: brew install llvm"
    echo "On Linux: sudo apt-get install clang"
fi

# Clean up
rm -f simple_fuzz.cc simple_fuzzer

echo -e "\n${YELLOW}For full fuzzing capabilities:${NC}"
echo "1. Install clang with libFuzzer support"
echo "2. Run ./build_fuzzers.sh to build all fuzz targets"
echo "3. See README.md for detailed instructions"