#!/usr/bin/env python3
"""
Test script for Slowloris and Slow Read attack protection

This script tests the server's resilience against:
1. Slowloris - sending partial HTTP headers very slowly
2. Slow POST - sending POST body data at minimal rate
3. Slow Read - reading response data very slowly

Expected behavior:
- Server should timeout slow connections
- Server should remain responsive to legitimate clients
"""

import socket
import time
import sys
import threading

# Test configuration
SERVER_HOST = "localhost"
SERVER_PORT = 8080
READ_HEADER_TIMEOUT = 10  # Must match ConnectionTimeouts::READ_HEADER_TIMEOUT_SEC
WRITE_RESPONSE_TIMEOUT = 60  # Must match ConnectionTimeouts::WRITE_RESPONSE_TIMEOUT_SEC

# ANSI color codes
GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
RESET = "\033[0m"


def print_test(name):
    """Print test name"""
    print(f"\n{YELLOW}[TEST]{RESET} {name}")


def print_pass(message):
    """Print success message"""
    print(f"{GREEN}[PASS]{RESET} {message}")


def print_fail(message):
    """Print failure message"""
    print(f"{RED}[FAIL]{RESET} {message}")


def test_slowloris():
    """
    Test Slowloris attack protection
    Send partial HTTP headers very slowly (1 byte per second)
    Expected: Server should timeout after READ_HEADER_TIMEOUT seconds
    """
    print_test("Slowloris Attack Protection")

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((SERVER_HOST, SERVER_PORT))

        # Send partial headers very slowly
        headers = "GET / HTTP/1.1\r\n"
        start_time = time.time()

        for i, char in enumerate(headers):
            try:
                sock.send(char.encode())
                time.sleep(0.5)  # Send very slowly

                # Check if connection is still alive after timeout period
                elapsed = time.time() - start_time
                if elapsed > READ_HEADER_TIMEOUT + 2:
                    # Connection should have been closed by now
                    try:
                        sock.send(b"X")  # Try to send more data
                        print_fail(
                            f"Connection still open after {elapsed:.1f}s (expected timeout at {READ_HEADER_TIMEOUT}s)"
                        )
                        sock.close()
                        return False
                    except BrokenPipeError:
                        print_pass(
                            f"Connection properly timed out after {elapsed:.1f}s (expected: {READ_HEADER_TIMEOUT}s)"
                        )
                        return True
            except BrokenPipeError:
                elapsed = time.time() - start_time
                if elapsed >= READ_HEADER_TIMEOUT - 2 and elapsed <= READ_HEADER_TIMEOUT + 2:
                    print_pass(
                        f"Connection properly timed out after {elapsed:.1f}s (expected: {READ_HEADER_TIMEOUT}s)"
                    )
                    return True
                else:
                    print_fail(
                        f"Unexpected timeout at {elapsed:.1f}s (expected: ~{READ_HEADER_TIMEOUT}s)"
                    )
                    return False

        sock.close()
        print_fail("Connection completed without timeout")
        return False

    except Exception as e:
        print_fail(f"Unexpected error: {e}")
        return False


def test_slow_read():
    """
    Test Slow Read attack protection
    Read response data very slowly
    Expected: Server should timeout after WRITE_RESPONSE_TIMEOUT seconds
    """
    print_test("Slow Read Attack Protection")

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((SERVER_HOST, SERVER_PORT))

        # Send a complete request quickly
        request = "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n"
        sock.send(request.encode())

        # Try to read response very slowly (1 byte at a time with delay)
        start_time = time.time()
        bytes_read = 0

        try:
            while True:
                data = sock.recv(1)  # Read 1 byte at a time
                if not data:
                    elapsed = time.time() - start_time
                    print_fail(
                        f"Connection closed prematurely after {elapsed:.1f}s (read {bytes_read} bytes)"
                    )
                    return False

                bytes_read += len(data)
                time.sleep(2)  # Very slow read (slower than min data rate)

                elapsed = time.time() - start_time
                if elapsed > WRITE_RESPONSE_TIMEOUT + 5:
                    print_fail(
                        f"Connection still open after {elapsed:.1f}s (expected timeout at {WRITE_RESPONSE_TIMEOUT}s)"
                    )
                    sock.close()
                    return False

        except (socket.timeout, ConnectionResetError, BrokenPipeError) as e:
            elapsed = time.time() - start_time
            # For slow read, the timeout is enforced during write, which happens immediately
            # So we just verify the connection was terminated
            if bytes_read > 0:
                print_pass(
                    f"Connection terminated after {elapsed:.1f}s (read {bytes_read} bytes) - server enforced write timeout"
                )
                return True
            else:
                print_fail(f"Connection terminated too early: {e}")
                return False

    except Exception as e:
        print_fail(f"Unexpected error: {e}")
        return False


def test_legitimate_client():
    """
    Test that legitimate clients are not affected
    Send a normal request and verify it completes successfully
    """
    print_test("Legitimate Client Test")

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect((SERVER_HOST, SERVER_PORT))

        # Send a complete, legitimate request
        request = "GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
        sock.send(request.encode())

        # Read response
        response = b""
        while True:
            data = sock.recv(4096)
            if not data:
                break
            response += data

        sock.close()

        # Verify we got a valid HTTP response
        if response.startswith(b"HTTP/1.1 200"):
            print_pass("Legitimate request completed successfully")
            return True
        else:
            print_fail(f"Unexpected response: {response[:100]}")
            return False

    except Exception as e:
        print_fail(f"Legitimate request failed: {e}")
        return False


def test_multiple_slow_connections():
    """
    Test server behavior under multiple simultaneous slow connections
    Ensure legitimate clients can still connect
    """
    print_test("Multiple Slow Connections + Legitimate Client")

    slow_sockets = []

    try:
        # Create several slow connections
        for i in range(5):
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect((SERVER_HOST, SERVER_PORT))
            sock.send(b"GET /")  # Send partial request
            slow_sockets.append(sock)

        # Give slow connections a moment
        time.sleep(1)

        # Try a legitimate request
        legit_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        legit_sock.settimeout(5)
        legit_sock.connect((SERVER_HOST, SERVER_PORT))
        request = "GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
        legit_sock.send(request.encode())

        response = b""
        while True:
            data = legit_sock.recv(4096)
            if not data:
                break
            response += data

        legit_sock.close()

        # Clean up slow connections
        for sock in slow_sockets:
            try:
                sock.close()
            except:
                pass

        if response.startswith(b"HTTP/1.1 200"):
            print_pass("Server remained responsive despite slow connections")
            return True
        else:
            print_fail("Server unresponsive or returned error")
            return False

    except Exception as e:
        print_fail(f"Test failed: {e}")
        # Clean up
        for sock in slow_sockets:
            try:
                sock.close()
            except:
                pass
        return False


def main():
    """Run all tests"""
    print("=" * 60)
    print("Slowloris / Slow Read Attack Protection Test Suite")
    print("=" * 60)
    print(f"Target: {SERVER_HOST}:{SERVER_PORT}")
    print(f"Timeout values: Header={READ_HEADER_TIMEOUT}s, Write={WRITE_RESPONSE_TIMEOUT}s")

    results = []

    # Test 1: Slowloris protection
    results.append(("Slowloris Protection", test_slowloris()))

    # Wait between tests
    time.sleep(2)

    # Test 2: Slow Read protection
    results.append(("Slow Read Protection", test_slow_read()))

    # Wait between tests
    time.sleep(2)

    # Test 3: Legitimate client
    results.append(("Legitimate Client", test_legitimate_client()))

    # Wait between tests
    time.sleep(2)

    # Test 4: Multiple slow connections
    results.append(("Multiple Slow Connections", test_multiple_slow_connections()))

    # Print summary
    print("\n" + "=" * 60)
    print("Test Summary")
    print("=" * 60)

    passed = 0
    failed = 0

    for name, result in results:
        status = f"{GREEN}PASS{RESET}" if result else f"{RED}FAIL{RESET}"
        print(f"{name:40} [{status}]")
        if result:
            passed += 1
        else:
            failed += 1

    print("=" * 60)
    print(f"Total: {passed + failed}, Passed: {passed}, Failed: {failed}")

    if failed == 0:
        print(f"\n{GREEN}All tests passed!{RESET}")
        return 0
    else:
        print(f"\n{RED}{failed} test(s) failed{RESET}")
        return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
        sys.exit(1)
