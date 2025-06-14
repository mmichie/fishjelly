# Makefile wrapper for Meson build system
# This provides familiar make commands that delegate to Meson/Ninja

# Default build directory
BUILDDIR := builddir

# Detect if we're in the build directory
CURRENT_DIR := $(shell pwd)
IN_BUILDDIR := $(shell if [ -f build.ninja ]; then echo 1; fi)

# Default target
.PHONY: all
all: build

# Setup build directory if it doesn't exist
.PHONY: setup
setup:
	@if [ ! -d $(BUILDDIR) ]; then \
		echo "Setting up build directory..."; \
		meson setup $(BUILDDIR); \
	fi

# Build the project
.PHONY: build
build: setup
	@echo "Building project..."
	@cd $(BUILDDIR) && meson compile

# Clean build artifacts but keep build directory
.PHONY: clean
clean:
	@if [ -d $(BUILDDIR) ]; then \
		echo "Cleaning build artifacts..."; \
		cd $(BUILDDIR) && meson compile --clean; \
	else \
		echo "Build directory not found, nothing to clean"; \
	fi

# Clean everything including build directory
.PHONY: distclean
distclean:
	@echo "Removing build directory..."
	@rm -rf $(BUILDDIR)

# Rebuild from scratch
.PHONY: rebuild
rebuild: distclean build

# Install the binary
.PHONY: install
install: build
	@echo "Installing..."
	@cd $(BUILDDIR) && meson install

# Run the server (example with default port)
.PHONY: run
run: build
	@echo "Starting server on port 8080..."
	@echo "Press Ctrl+C to stop the server"
	@exec ./$(BUILDDIR)/src/shelob -p 8080

# Run the server in daemon mode
.PHONY: run-daemon
run-daemon: build
	@echo "Starting server in daemon mode on port 8080..."
	@./$(BUILDDIR)/src/shelob -p 8080 -d

# Format source code
.PHONY: format
format:
	@echo "Formatting C++ source files..."
	@if command -v clang-format >/dev/null 2>&1; then \
		find src tests -name "*.cc" -o -name "*.h" | xargs clang-format -i && \
		echo "Formatting complete."; \
	else \
		echo "Error: clang-format not found. Please install it."; \
		exit 1; \
	fi

# Alias for format
.PHONY: fmt
fmt: format

# Check formatting without modifying files
.PHONY: format-check
format-check:
	@echo "Checking C++ source formatting..."
	@if command -v clang-format >/dev/null 2>&1; then \
		find src tests -name "*.cc" -o -name "*.h" | xargs clang-format --dry-run --Werror && \
		echo "✓ Code formatting is correct"; \
	else \
		echo "Warning: clang-format not found, skipping format check"; \
	fi

# Run pre-commit checks (compile, test, format)
.PHONY: pre-commit
pre-commit: build test format-check
	@echo "✓ All pre-commit checks passed!"

# Install git pre-commit hook
.PHONY: install-hooks
install-hooks:
	@echo "Installing git pre-commit hook..."
	@echo '#!/bin/bash' > .git/hooks/pre-commit
	@echo 'make pre-commit' >> .git/hooks/pre-commit
	@chmod +x .git/hooks/pre-commit
	@echo "✓ Pre-commit hook installed"
	@echo "To skip the hook, use: git commit --no-verify"

# Show help
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all         - Build the project (default)"
	@echo "  setup       - Setup the build directory"
	@echo "  build       - Build the project"
	@echo "  clean       - Clean build artifacts"
	@echo "  distclean   - Remove entire build directory"
	@echo "  rebuild     - Clean and rebuild from scratch"
	@echo "  install     - Install the binary"
	@echo "  run         - Run the server on port 8080"
	@echo "  run-daemon  - Run the server in daemon mode"
	@echo "  test        - Run tests"
	@echo "  format/fmt  - Format all source files"
	@echo "  format-check- Check formatting without modifying"
	@echo "  pre-commit  - Run all pre-commit checks"
	@echo "  install-hooks - Install git pre-commit hook"
	@echo "  help        - Show this help message"

# Run tests
.PHONY: test
test: build
	@echo "Running tests..."
	@cd $(BUILDDIR) && meson test

# Run tests with verbose output
.PHONY: test-verbose
test-verbose: build
	@echo "Running tests (verbose)..."
	@cd $(BUILDDIR) && meson test -v

# Generate test coverage report
.PHONY: coverage
coverage:
	@echo "Configuring for coverage..."
	@meson setup $(BUILDDIR) --buildtype=debug -Db_coverage=true --reconfigure
	@cd $(BUILDDIR) && meson compile && meson test
	@cd $(BUILDDIR) && ninja coverage
	@echo "Coverage report generated in $(BUILDDIR)/meson-logs/coveragereport/"

# Configure build type
.PHONY: debug
debug:
	@echo "Configuring debug build..."
	@meson setup $(BUILDDIR) --buildtype=debug --reconfigure

.PHONY: release
release:
	@echo "Configuring release build..."
	@meson setup $(BUILDDIR) --buildtype=release --reconfigure

# Show build configuration
.PHONY: info
info:
	@if [ -d $(BUILDDIR) ]; then \
		echo "Build configuration:"; \
		cd $(BUILDDIR) && meson configure; \
	else \
		echo "Build directory not found. Run 'make setup' first."; \
	fi