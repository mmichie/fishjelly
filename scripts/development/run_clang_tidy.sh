#!/bin/bash

# Run clang-tidy on the fishjelly codebase

echo "=== Running clang-tidy on fishjelly ==="
echo

# Check if clang-tidy is installed
if ! command -v clang-tidy &> /dev/null; then
    echo "Error: clang-tidy not found. Please install with:"
    echo "  macOS: brew install llvm"
    echo "  Linux: sudo apt-get install clang-tidy"
    exit 1
fi

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Create compile_commands.json if it doesn't exist
if [ ! -f "builddir/compile_commands.json" ]; then
    echo "Generating compile_commands.json..."
    meson setup builddir || meson configure builddir
fi

# Source files to check
SOURCE_FILES=$(find src -name "*.cc" -o -name "*.h" | grep -v "mktime.c")

# Run clang-tidy
echo "Checking files..."
echo

# Option 1: Run with fixes (commented out by default)
# clang-tidy -p builddir --fix $SOURCE_FILES

# Option 2: Run checks only
ERRORS=0
for file in $SOURCE_FILES; do
    echo -e "${YELLOW}Checking $file...${NC}"
    
    # Run clang-tidy and capture output
    OUTPUT=$(clang-tidy -p builddir "$file" 2>&1)
    
    # Check if there were any warnings/errors
    if echo "$OUTPUT" | grep -q "warning:"; then
        echo -e "${RED}Issues found in $file:${NC}"
        echo "$OUTPUT" | grep -A 2 "warning:"
        ((ERRORS++))
    else
        echo -e "${GREEN}✓ No issues${NC}"
    fi
    echo
done

# Summary
echo "=== Summary ==="
if [ $ERRORS -eq 0 ]; then
    echo -e "${GREEN}All files passed clang-tidy checks!${NC}"
    exit 0
else
    echo -e "${RED}Found issues in $ERRORS files${NC}"
    echo
    echo "To automatically fix some issues, run:"
    echo "  clang-tidy -p builddir --fix src/*.cc src/*.h"
    exit 1
fi