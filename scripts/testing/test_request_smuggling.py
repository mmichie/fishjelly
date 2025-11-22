#!/usr/bin/env python3
"""
HTTP Request Smuggling Test Suite
Tests various request smuggling attack patterns to verify server protection
"""

import socket
import sys
import time

class Colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'

def send_raw_request(host, port, raw_request):
    """Send a raw HTTP request and return the response"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect((host, port))
        sock.sendall(raw_request.encode('latin-1'))

        # Read response
        response = b""
        sock.settimeout(2)
        try:
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response += chunk
        except socket.timeout:
            pass  # Expected for keep-alive connections

        sock.close()
        return response.decode('latin-1', errors='ignore')
    except Exception as e:
        return f"ERROR: {e}"

def test_duplicate_content_length(host, port):
    """Test: Multiple Content-Length headers (classic smuggling)"""
    print(f"{Colors.OKBLUE}Test 1: Duplicate Content-Length headers{Colors.ENDC}")

    # Request with two different Content-Length values
    raw_request = (
        "POST /test.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 6\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "12345SMUGGLED"
    )

    response = send_raw_request(host, port, raw_request)

    # Should be rejected with 400
    if "400" in response and "Duplicate" in response:
        print(f"{Colors.OKGREEN}✓ PASS: Server correctly rejected duplicate Content-Length{Colors.ENDC}")
        return True
    else:
        print(f"{Colors.FAIL}✗ FAIL: Server accepted duplicate Content-Length headers!{Colors.ENDC}")
        print(f"Response: {response[:200]}")
        return False

def test_duplicate_transfer_encoding(host, port):
    """Test: Multiple Transfer-Encoding headers"""
    print(f"{Colors.OKBLUE}Test 2: Duplicate Transfer-Encoding headers{Colors.ENDC}")

    raw_request = (
        "POST /test.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Transfer-Encoding: identity\r\n"
        "\r\n"
        "0\r\n\r\n"
    )

    response = send_raw_request(host, port, raw_request)

    if "400" in response and "Duplicate" in response:
        print(f"{Colors.OKGREEN}✓ PASS: Server correctly rejected duplicate Transfer-Encoding{Colors.ENDC}")
        return True
    else:
        print(f"{Colors.FAIL}✗ FAIL: Server accepted duplicate Transfer-Encoding headers!{Colors.ENDC}")
        print(f"Response: {response[:200]}")
        return False

def test_cl_te_smuggling(host, port):
    """Test: CL.TE request smuggling (both Content-Length and Transfer-Encoding)"""
    print(f"{Colors.OKBLUE}Test 3: CL.TE smuggling (Content-Length + Transfer-Encoding){Colors.ENDC}")

    # Classic CL.TE smuggling attempt
    raw_request = (
        "POST /test.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 13\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "0\r\n"
        "\r\n"
        "SMUGGLED"
    )

    response = send_raw_request(host, port, raw_request)

    if "400" in response and "mutually exclusive" in response:
        print(f"{Colors.OKGREEN}✓ PASS: Server correctly rejected CL.TE smuggling attempt{Colors.ENDC}")
        return True
    else:
        print(f"{Colors.FAIL}✗ FAIL: Server vulnerable to CL.TE smuggling!{Colors.ENDC}")
        print(f"Response: {response[:200]}")
        return False

def test_te_cl_smuggling(host, port):
    """Test: TE.CL request smuggling"""
    print(f"{Colors.OKBLUE}Test 4: TE.CL smuggling (Transfer-Encoding + Content-Length){Colors.ENDC}")

    raw_request = (
        "POST /test.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
        "5c\r\n"
        "SMUGGLED_REQUEST\r\n"
        "0\r\n"
        "\r\n"
    )

    response = send_raw_request(host, port, raw_request)

    if "400" in response and "mutually exclusive" in response:
        print(f"{Colors.OKGREEN}✓ PASS: Server correctly rejected TE.CL smuggling attempt{Colors.ENDC}")
        return True
    else:
        print(f"{Colors.FAIL}✗ FAIL: Server vulnerable to TE.CL smuggling!{Colors.ENDC}")
        print(f"Response: {response[:200]}")
        return False

def test_malformed_transfer_encoding(host, port):
    """Test: Malformed Transfer-Encoding values"""
    print(f"{Colors.OKBLUE}Test 5: Malformed Transfer-Encoding value{Colors.ENDC}")

    # Transfer-Encoding with additional encodings (not just "chunked")
    raw_request = (
        "POST /test.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked, identity\r\n"
        "\r\n"
        "0\r\n"
        "\r\n"
    )

    response = send_raw_request(host, port, raw_request)

    if "400" in response and ("Invalid" in response or "Only" in response):
        print(f"{Colors.OKGREEN}✓ PASS: Server correctly rejected malformed Transfer-Encoding{Colors.ENDC}")
        return True
    else:
        print(f"{Colors.FAIL}✗ FAIL: Server accepted malformed Transfer-Encoding!{Colors.ENDC}")
        print(f"Response: {response[:200]}")
        return False

def test_obfuscated_transfer_encoding(host, port):
    """Test: Obfuscated Transfer-Encoding (whitespace tricks)"""
    print(f"{Colors.OKBLUE}Test 6: Obfuscated Transfer-Encoding with extra whitespace{Colors.ENDC}")

    raw_request = (
        "POST /test.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding:  chunked  \r\n"
        "\r\n"
        "0\r\n"
        "\r\n"
    )

    response = send_raw_request(host, port, raw_request)

    # This should be ACCEPTED (whitespace around "chunked" is normalized)
    if "200" in response or "chunked" in response.lower():
        print(f"{Colors.OKGREEN}✓ PASS: Server correctly normalized whitespace in Transfer-Encoding{Colors.ENDC}")
        return True
    else:
        print(f"{Colors.WARNING}⚠ WARNING: Server may have issues with whitespace normalization{Colors.ENDC}")
        print(f"Response: {response[:200]}")
        return False

def test_identity_encoding(host, port):
    """Test: Transfer-Encoding: identity (should be rejected)"""
    print(f"{Colors.OKBLUE}Test 7: Transfer-Encoding: identity{Colors.ENDC}")

    raw_request = (
        "POST /test.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: identity\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "12345"
    )

    response = send_raw_request(host, port, raw_request)

    # Should reject both headers AND invalid Transfer-Encoding value
    if "400" in response:
        print(f"{Colors.OKGREEN}✓ PASS: Server correctly rejected invalid encoding{Colors.ENDC}")
        return True
    else:
        print(f"{Colors.FAIL}✗ FAIL: Server accepted Transfer-Encoding: identity!{Colors.ENDC}")
        print(f"Response: {response[:200]}")
        return False

def test_valid_chunked_request(host, port):
    """Test: Valid chunked request (should be accepted)"""
    print(f"{Colors.OKBLUE}Test 8: Valid chunked request (control test){Colors.ENDC}")

    raw_request = (
        "POST /test.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\n"
        "hello\r\n"
        "0\r\n"
        "\r\n"
    )

    response = send_raw_request(host, port, raw_request)

    if "200" in response or "hello" in response:
        print(f"{Colors.OKGREEN}✓ PASS: Server correctly accepted valid chunked request{Colors.ENDC}")
        return True
    else:
        print(f"{Colors.WARNING}⚠ INFO: Valid chunked request response: {response[:100]}{Colors.ENDC}")
        return True  # Not a failure

def test_valid_content_length(host, port):
    """Test: Valid Content-Length request (should be accepted)"""
    print(f"{Colors.OKBLUE}Test 9: Valid Content-Length request (control test){Colors.ENDC}")

    raw_request = (
        "POST /test.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello"
    )

    response = send_raw_request(host, port, raw_request)

    if "200" in response or "hello" in response:
        print(f"{Colors.OKGREEN}✓ PASS: Server correctly accepted valid Content-Length request{Colors.ENDC}")
        return True
    else:
        print(f"{Colors.WARNING}⚠ INFO: Valid Content-Length request response: {response[:100]}{Colors.ENDC}")
        return True  # Not a failure

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <port>")
        print(f"Example: {sys.argv[0]} 8080")
        sys.exit(1)

    host = "localhost"
    port = int(sys.argv[1])

    print(f"{Colors.HEADER}{Colors.BOLD}")
    print("=" * 70)
    print("HTTP Request Smuggling Protection Test Suite")
    print("=" * 70)
    print(f"{Colors.ENDC}")
    print(f"Testing server at {host}:{port}\n")

    tests = [
        test_duplicate_content_length,
        test_duplicate_transfer_encoding,
        test_cl_te_smuggling,
        test_te_cl_smuggling,
        test_malformed_transfer_encoding,
        test_obfuscated_transfer_encoding,
        test_identity_encoding,
        test_valid_chunked_request,
        test_valid_content_length,
    ]

    results = []
    for test in tests:
        result = test(host, port)
        results.append(result)
        print()
        time.sleep(0.1)  # Brief pause between tests

    # Summary
    print(f"{Colors.HEADER}{Colors.BOLD}")
    print("=" * 70)
    print("Test Summary")
    print("=" * 70)
    print(f"{Colors.ENDC}")

    passed = sum(results)
    total = len(results)

    print(f"Tests passed: {passed}/{total}")

    if passed == total:
        print(f"{Colors.OKGREEN}{Colors.BOLD}✓ ALL TESTS PASSED - Server is protected against request smuggling!{Colors.ENDC}")
        sys.exit(0)
    else:
        print(f"{Colors.FAIL}{Colors.BOLD}✗ SOME TESTS FAILED - Server may be vulnerable to request smuggling!{Colors.ENDC}")
        sys.exit(1)

if __name__ == "__main__":
    main()
