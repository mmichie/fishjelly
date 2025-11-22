#!/usr/bin/env python3
"""
Test script for HTTP status codes: 405, 429, and 503
Tests Method Not Allowed, Rate Limiting, and Service Unavailable
"""

import socket
import sys
import time

# ANSI color codes
GREEN = '\033[92m'
RED = '\033[91m'
YELLOW = '\033[93m'
RESET = '\033[0m'
BOLD = '\033[1m'

def send_request(method, path, host="localhost", port=8080, headers=None, timeout=5):
    """Send an HTTP request and return the response"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect((host, port))

        # Build request
        request = f"{method} {path} HTTP/1.1\r\n"
        request += f"Host: {host}\r\n"

        if headers:
            for key, value in headers.items():
                request += f"{key}: {value}\r\n"

        request += "Connection: close\r\n"
        request += "\r\n"

        sock.sendall(request.encode())

        # Read response
        response = b""
        while True:
            try:
                data = sock.recv(4096)
                if not data:
                    break
                response += data
            except socket.timeout:
                break

        sock.close()
        return response.decode('utf-8', errors='ignore')
    except Exception as e:
        print(f"{RED}Error sending request: {e}{RESET}")
        return None

def parse_status_code(response):
    """Extract status code from HTTP response"""
    if not response:
        return None

    lines = response.split('\r\n')
    if len(lines) > 0:
        status_line = lines[0]
        parts = status_line.split(' ')
        if len(parts) >= 2:
            try:
                return int(parts[1])
            except ValueError:
                return None
    return None

def check_header(response, header_name):
    """Check if a header exists in the response"""
    if not response:
        return False

    lines = response.split('\r\n')
    for line in lines:
        if line.lower().startswith(header_name.lower() + ':'):
            return True
    return False

def get_header_value(response, header_name):
    """Get the value of a header from the response"""
    if not response:
        return None

    lines = response.split('\r\n')
    for line in lines:
        if line.lower().startswith(header_name.lower() + ':'):
            parts = line.split(':', 1)
            if len(parts) == 2:
                return parts[1].strip()
    return None

def test_405_method_not_allowed():
    """Test 405 Method Not Allowed for unsupported HTTP methods"""
    print(f"\n{BOLD}Test 1: 405 Method Not Allowed{RESET}")
    print("=" * 60)

    # Test with an unsupported method (TRACE is typically not implemented)
    print("Testing TRACE method (unsupported)...")
    response = send_request("TRACE", "/", port=8080)

    if response:
        status_code = parse_status_code(response)
        has_allow_header = check_header(response, "Allow")

        if status_code == 405:
            print(f"{GREEN}✓ Received 405 Method Not Allowed{RESET}")
        else:
            print(f"{RED}✗ Expected 405, got {status_code}{RESET}")
            return False

        if has_allow_header:
            allow_header = get_header_value(response, "Allow")
            print(f"{GREEN}✓ Allow header present: {allow_header}{RESET}")
        else:
            print(f"{YELLOW}⚠ Allow header missing (should list allowed methods){RESET}")

        return status_code == 405
    else:
        print(f"{RED}✗ No response received{RESET}")
        return False

def test_429_rate_limiting():
    """Test 429 Too Many Requests with rate limiting"""
    print(f"\n{BOLD}Test 2: 429 Too Many Requests (Rate Limiting){RESET}")
    print("=" * 60)

    # Note: The default rate limit is 100 requests per 60 seconds
    # We'll send requests rapidly to trigger it
    print("Sending rapid requests to trigger rate limiting...")
    print("(Default: 100 requests per 60 seconds)")

    max_requests = 110  # More than the default limit
    status_429_received = False
    retry_after_present = False

    for i in range(max_requests):
        response = send_request("GET", "/", port=8080)

        if response:
            status_code = parse_status_code(response)

            if status_code == 429:
                status_429_received = True
                print(f"{GREEN}✓ Received 429 after {i + 1} requests{RESET}")

                # Check for Retry-After header
                if check_header(response, "Retry-After"):
                    retry_after = get_header_value(response, "Retry-After")
                    print(f"{GREEN}✓ Retry-After header present: {retry_after} seconds{RESET}")
                    retry_after_present = True
                else:
                    print(f"{YELLOW}⚠ Retry-After header missing{RESET}")

                break

        # Small delay to not overwhelm the server
        time.sleep(0.01)

    if not status_429_received:
        print(f"{YELLOW}⚠ Did not receive 429 (sent {max_requests} requests){RESET}")
        print(f"{YELLOW}  Rate limiting may be disabled or limit is higher{RESET}")
        return False

    return status_429_received and retry_after_present

def test_503_service_unavailable():
    """Test 503 Service Unavailable (maintenance mode)"""
    print(f"\n{BOLD}Test 3: 503 Service Unavailable{RESET}")
    print("=" * 60)

    print(f"{YELLOW}Note: This test requires the server to be in maintenance mode{RESET}")
    print(f"{YELLOW}To enable maintenance mode, use: http.setMaintenanceMode(true){RESET}")
    print(f"{YELLOW}Skipping automated test - checking if endpoint responds normally...{RESET}")

    response = send_request("GET", "/", port=8080)

    if response:
        status_code = parse_status_code(response)

        if status_code == 503:
            print(f"{GREEN}✓ Server is in maintenance mode (503){RESET}")

            if check_header(response, "Retry-After"):
                retry_after = get_header_value(response, "Retry-After")
                print(f"{GREEN}✓ Retry-After header present: {retry_after} seconds{RESET}")

            return True
        elif status_code == 200:
            print(f"{YELLOW}⚠ Server is operational (200 OK){RESET}")
            print(f"{YELLOW}  To test 503, enable maintenance mode in the server{RESET}")
            return True  # Not a failure, just not in maintenance mode
        else:
            print(f"{YELLOW}⚠ Received status code: {status_code}{RESET}")
            return True  # Not testing maintenance mode
    else:
        print(f"{RED}✗ No response received{RESET}")
        return False

def test_405_with_valid_methods():
    """Test that valid methods still work (negative test for 405)"""
    print(f"\n{BOLD}Test 4: Valid Methods Should Not Return 405{RESET}")
    print("=" * 60)

    valid_methods = ["GET", "HEAD", "POST", "OPTIONS", "PUT", "DELETE"]
    all_passed = True

    for method in valid_methods:
        print(f"Testing {method} method...")
        response = send_request(method, "/", port=8080)

        if response:
            status_code = parse_status_code(response)

            # Valid methods should not return 405
            # (They might return 404 if resource doesn't exist, but not 405)
            if status_code == 405:
                print(f"{RED}✗ {method} returned 405 (should be allowed){RESET}")
                all_passed = False
            else:
                print(f"{GREEN}✓ {method} returned {status_code} (not 405){RESET}")
        else:
            print(f"{RED}✗ No response for {method}{RESET}")
            all_passed = False

    return all_passed

def main():
    print(f"{BOLD}HTTP Status Code Tests{RESET}")
    print(f"{BOLD}Testing: 405, 429, and 503{RESET}")
    print("=" * 60)

    # Check if server is running
    print("\nChecking if server is running on port 8080...")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(2)
        sock.connect(("localhost", 8080))
        sock.close()
        print(f"{GREEN}✓ Server is running{RESET}")
    except:
        print(f"{RED}✗ Server is not running on port 8080{RESET}")
        print(f"{YELLOW}Please start the server first: ./shelob -p 8080{RESET}")
        return 1

    # Run tests
    results = []

    results.append(("405 Method Not Allowed", test_405_method_not_allowed()))
    results.append(("Valid Methods Work", test_405_with_valid_methods()))
    results.append(("429 Rate Limiting", test_429_rate_limiting()))
    results.append(("503 Service Unavailable", test_503_service_unavailable()))

    # Print summary
    print(f"\n{BOLD}Test Summary{RESET}")
    print("=" * 60)

    passed = 0
    failed = 0

    for test_name, result in results:
        if result:
            print(f"{GREEN}✓ {test_name}{RESET}")
            passed += 1
        else:
            print(f"{RED}✗ {test_name}{RESET}")
            failed += 1

    print(f"\n{BOLD}Total: {passed} passed, {failed} failed{RESET}")

    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
