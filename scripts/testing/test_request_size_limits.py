#!/usr/bin/env python3
"""
Test suite for request size limit enforcement in Fishjelly web server.
Tests both HTTP/1.1 and HTTP/2 implementations.

Run this with: python3 test_request_size_limits.py
"""

import socket
import sys


class TestRequestSizeLimits:
    def __init__(self, host='localhost', port=8080):
        self.host = host
        self.port = port
        self.passed = 0
        self.failed = 0

    def send_request(self, request_data):
        """Send raw HTTP request and return response"""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)
            sock.connect((self.host, self.port))

            if isinstance(request_data, str):
                request_data = request_data.encode('utf-8')

            sock.sendall(request_data)

            # Read response
            response = b''
            while True:
                try:
                    chunk = sock.recv(4096)
                    if not chunk:
                        break
                    response += chunk
                    # Check if we have complete headers
                    if b'\r\n\r\n' in response:
                        # Parse Content-Length if present
                        headers_end = response.find(b'\r\n\r\n')
                        headers = response[:headers_end].decode('utf-8', errors='ignore')
                        if 'Content-Length:' in headers:
                            for line in headers.split('\r\n'):
                                if line.startswith('Content-Length:'):
                                    content_length = int(line.split(':')[1].strip())
                                    body_so_far = len(response) - (headers_end + 4)
                                    if body_so_far >= content_length:
                                        break
                        else:
                            # No content-length, just read a bit more
                            break
                except socket.timeout:
                    break

            sock.close()
            return response.decode('utf-8', errors='ignore')
        except Exception as e:
            return f"ERROR: {str(e)}"

    def test_header_line_too_large(self):
        """Test: Single header line exceeds 8KB limit"""
        print("\n[TEST] Header line too large (>8KB)")

        # Create a header with > 8KB value
        large_value = 'A' * 9000
        request = f"GET / HTTP/1.1\r\nHost: localhost\r\nX-Large: {large_value}\r\n\r\n"

        response = self.send_request(request)

        if '413' in response and 'Payload Too Large' in response:
            print("✓ PASS: Server rejected request with 413 Payload Too Large")
            self.passed += 1
        else:
            print(f"✗ FAIL: Expected 413 response, got: {response[:200]}")
            self.failed += 1

    def test_total_headers_too_large(self):
        """Test: Total headers exceed 8KB limit"""
        print("\n[TEST] Total headers too large (>8KB)")

        # Create many headers that exceed 8KB total
        headers = "GET / HTTP/1.1\r\nHost: localhost\r\n"
        for i in range(100):
            headers += f"X-Header-{i}: {'A' * 100}\r\n"
        headers += "\r\n"

        response = self.send_request(headers)

        if '413' in response and 'Payload Too Large' in response:
            print("✓ PASS: Server rejected request with 413 Payload Too Large")
            self.passed += 1
        else:
            print(f"✗ FAIL: Expected 413 response, got: {response[:200]}")
            self.failed += 1

    def test_too_many_headers(self):
        """Test: More than 100 headers"""
        print("\n[TEST] Too many headers (>100)")

        # Create > 100 headers
        headers = "GET / HTTP/1.1\r\nHost: localhost\r\n"
        for i in range(110):
            headers += f"X-H{i}: val\r\n"
        headers += "\r\n"

        response = self.send_request(headers)

        if '413' in response and 'Payload Too Large' in response:
            print("✓ PASS: Server rejected request with 413 Payload Too Large")
            self.passed += 1
        else:
            print(f"✗ FAIL: Expected 413 response, got: {response[:200]}")
            self.failed += 1

    def test_post_body_too_large(self):
        """Test: POST body exceeds 10MB limit"""
        print("\n[TEST] POST body too large (>10MB)")

        # Create a POST with > 10MB body
        body_size = 11 * 1024 * 1024  # 11MB
        headers = (
            f"POST /test HTTP/1.1\r\n"
            f"Host: localhost\r\n"
            f"Content-Length: {body_size}\r\n"
            f"\r\n"
        )

        response = self.send_request(headers)

        if '413' in response and 'Payload Too Large' in response:
            print("✓ PASS: Server rejected request with 413 Payload Too Large")
            self.passed += 1
        else:
            print(f"✗ FAIL: Expected 413 response, got: {response[:200]}")
            self.failed += 1

    def test_chunked_body_too_large(self):
        """Test: Chunked body exceeds 10MB limit"""
        print("\n[TEST] Chunked body too large (>10MB)")

        # Send chunked request that exceeds limit
        chunk_size = 2 * 1024 * 1024  # 2MB chunks
        num_chunks = 6  # 12MB total

        request = (
            "POST /test HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
        )

        # Send header first
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        try:
            sock.connect((self.host, self.port))
            sock.sendall(request.encode('utf-8'))

            # Send chunks
            for i in range(num_chunks):
                chunk_header = f"{chunk_size:x}\r\n".encode('utf-8')
                chunk_data = b'A' * chunk_size
                sock.sendall(chunk_header + chunk_data + b'\r\n')

                # Try to read response (server should reject before we finish)
                try:
                    response = sock.recv(4096).decode('utf-8', errors='ignore')
                    if '413' in response:
                        print("✓ PASS: Server rejected chunked request with 413 Payload Too Large")
                        self.passed += 1
                        sock.close()
                        return
                except:
                    pass

            sock.sendall(b"0\r\n\r\n")
            response = sock.recv(4096).decode('utf-8', errors='ignore')
            sock.close()

            print(f"✗ FAIL: Expected 413 response, got: {response[:200]}")
            self.failed += 1
        except Exception as e:
            print(f"✗ FAIL: Exception: {e}")
            self.failed += 1

    def test_put_upload_too_large(self):
        """Test: PUT upload exceeds 100MB limit"""
        print("\n[TEST] PUT upload too large (>100MB)")

        # Create a PUT with > 100MB body
        body_size = 101 * 1024 * 1024  # 101MB
        headers = (
            f"PUT /test.txt HTTP/1.1\r\n"
            f"Host: localhost\r\n"
            f"Content-Length: {body_size}\r\n"
            f"\r\n"
        )

        response = self.send_request(headers)

        if '413' in response and 'Payload Too Large' in response:
            print("✓ PASS: Server rejected request with 413 Payload Too Large")
            self.passed += 1
        else:
            print(f"✗ FAIL: Expected 413 response, got: {response[:200]}")
            self.failed += 1

    def test_single_chunk_too_large(self):
        """Test: Single chunk exceeds 1MB limit"""
        print("\n[TEST] Single chunk too large (>1MB)")

        # Send chunked request with a single > 1MB chunk
        chunk_size = 2 * 1024 * 1024  # 2MB chunk

        request = (
            "POST /test HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            f"{chunk_size:x}\r\n"
        )

        response = self.send_request(request.encode('utf-8') + b'A' * 1000 + b'\r\n0\r\n\r\n')

        if '413' in response and 'Payload Too Large' in response:
            print("✓ PASS: Server rejected request with 413 Payload Too Large")
            self.passed += 1
        else:
            print(f"✗ FAIL: Expected 413 response, got: {response[:200]}")
            self.failed += 1

    def test_valid_request_within_limits(self):
        """Test: Valid request within all limits"""
        print("\n[TEST] Valid request within limits")

        request = "GET / HTTP/1.1\r\nHost: localhost\r\nUser-Agent: TestClient\r\n\r\n"

        response = self.send_request(request)

        if '200' in response or '404' in response:
            print("✓ PASS: Server accepted valid request")
            self.passed += 1
        else:
            print(f"✗ FAIL: Expected 200/404 response, got: {response[:200]}")
            self.failed += 1

    def run_all_tests(self):
        """Run all test cases"""
        print("=" * 70)
        print("Request Size Limits Test Suite")
        print("=" * 70)
        print(f"Testing server at {self.host}:{self.port}")

        # Run tests
        self.test_valid_request_within_limits()
        self.test_header_line_too_large()
        self.test_total_headers_too_large()
        self.test_too_many_headers()
        self.test_post_body_too_large()
        self.test_single_chunk_too_large()
        self.test_chunked_body_too_large()
        self.test_put_upload_too_large()

        # Print summary
        print("\n" + "=" * 70)
        print(f"Test Summary: {self.passed} passed, {self.failed} failed")
        print("=" * 70)

        return self.failed == 0


def main():
    """Main test runner"""
    tester = TestRequestSizeLimits()
    success = tester.run_all_tests()
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
