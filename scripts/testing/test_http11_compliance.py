#!/usr/bin/env python3
"""
Basic HTTP/1.1 compliance test suite for fishjelly web server
Tests core HTTP/1.1 requirements based on RFC 2616
"""

import socket
import time
import sys
import subprocess
import os

class HTTPComplianceTest:
    def __init__(self, host='localhost', port=8080):
        self.host = host
        self.port = port
        self.passed = 0
        self.failed = 0
        self.server_process = None
        
    def start_server(self):
        """Start the fishjelly server"""
        print("Starting fishjelly server...")
        # The server will change to 'base' directory itself
        self.server_process = subprocess.Popen(
            ['./builddir/src/shelob', '-p', str(self.port)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        time.sleep(3)  # Give server time to start
        
    def stop_server(self):
        """Stop the fishjelly server"""
        if self.server_process:
            print("\nStopping server...")
            self.server_process.terminate()
            self.server_process.wait()
    
    def send_request(self, request):
        """Send raw HTTP request and return response"""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)
            sock.connect((self.host, self.port))
            sock.send(request.encode())
            
            response = b''
            complete = False
            while True:
                data = sock.recv(1024)
                if not data:
                    break
                response += data
                # Check if we've received the full headers
                if b'\r\n\r\n' in response:
                    # For HEAD/OPTIONS, we're done
                    if b'HEAD' in request.encode() or b'OPTIONS' in request.encode():
                        break
                    # For GET/POST, check Content-Length
                    headers = response.split(b'\r\n\r\n')[0].decode()
                    if 'Content-Length:' in headers:
                        for line in headers.split('\r\n'):
                            if line.startswith('Content-Length:'):
                                content_length = int(line.split()[1])
                                header_end = response.find(b'\r\n\r\n') + 4
                                if len(response) >= header_end + content_length:
                                    complete = True
                                    break
                    elif 'Transfer-Encoding: chunked' not in headers:
                        # No Content-Length and not chunked, assume no body
                        complete = True
                    
                    if complete:
                        break
            
            sock.close()
            return response.decode('utf-8', errors='ignore')
        except Exception as e:
            return f"ERROR: {str(e)}"
    
    def test(self, name, request, expected_in_response=None, expected_not_in_response=None, expected_status=None):
        """Run a single test"""
        print(f"\n[TEST] {name}")
        print(f"Request:\n{request.strip()}")
        
        response = self.send_request(request)
        print(f"Response:\n{response[:200]}...")
        
        passed = True
        
        if expected_status and not response.startswith(f"HTTP/1.1 {expected_status}"):
            print(f"❌ Expected status {expected_status}, got: {response.split()[1] if len(response.split()) > 1 else 'ERROR'}")
            passed = False
            
        if expected_in_response:
            # Check if this is an OR condition (for status codes like 405/501)
            if any(exp in ["405", "501", "304", "200"] for exp in expected_in_response) and len(expected_in_response) > 1:
                # This is an OR condition - at least one should be present
                found_any = False
                for expected in expected_in_response:
                    if expected in response:
                        found_any = True
                        break
                if not found_any:
                    print(f"❌ Expected one of {expected_in_response} in response")
                    passed = False
            else:
                # Normal AND condition - all must be present
                for expected in expected_in_response:
                    if expected not in response:
                        print(f"❌ Expected '{expected}' in response")
                        passed = False
                    
        if expected_not_in_response:
            for not_expected in expected_not_in_response:
                if not_expected in response:
                    print(f"❌ Did not expect '{not_expected}' in response")
                    passed = False
        
        if passed:
            print("✅ PASSED")
            self.passed += 1
        else:
            print("❌ FAILED")
            self.failed += 1
            
        return passed
    
    def run_tests(self):
        """Run all compliance tests"""
        print("=" * 60)
        print("HTTP/1.1 Compliance Test Suite")
        print("=" * 60)
        
        # Test 1: Host header is required for HTTP/1.1
        self.test(
            "HTTP/1.1 request without Host header should fail",
            "GET / HTTP/1.1\r\n\r\n",
            expected_status="400"
        )
        
        # Test 2: Host header present
        self.test(
            "HTTP/1.1 request with Host header should succeed",
            "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n",
            expected_status="200",
            expected_in_response=["HTTP/1.1 200"]
        )
        
        # Test 3: HTTP/1.0 request should work without Host
        self.test(
            "HTTP/1.0 request without Host header should succeed",
            "GET / HTTP/1.0\r\n\r\n",
            expected_in_response=["200"]
        )
        
        # Test 4: OPTIONS method support (required)
        self.test(
            "OPTIONS method should be supported",
            "OPTIONS * HTTP/1.1\r\nHost: localhost\r\n\r\n",
            expected_status="200",
            expected_in_response=["Allow:"]
        )
        
        # Test 5: HEAD method (required)
        self.test(
            "HEAD method should return headers only",
            "HEAD /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n",
            expected_status="200",
            expected_not_in_response=["<html>", "<body>"]
        )
        
        # Test 6: Unknown method should return 501 or 405
        self.test(
            "Unknown method should return proper error",
            "PATCH / HTTP/1.1\r\nHost: localhost\r\n\r\n",
            expected_in_response=["501", "405"]
        )
        
        # Test 7: Invalid HTTP version
        self.test(
            "Invalid HTTP version should return 505",
            "GET / HTTP/2.0\r\nHost: localhost\r\n\r\n",
            expected_status="505"
        )
        
        # Test 8: Connection handling for HTTP/1.1
        self.test(
            "HTTP/1.1 default connection should be keep-alive",
            "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n",
            expected_in_response=["Connection: keep-alive"]
        )
        
        # Test 9: Connection close
        self.test(
            "Connection: close should be honored",
            "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n",
            expected_in_response=["Connection: close"]
        )
        
        # Test 10: If-Modified-Since (conditional GET)
        self.test(
            "If-Modified-Since should be supported",
            "GET /index.html HTTP/1.1\r\nHost: localhost\r\nIf-Modified-Since: Wed, 01 Jan 2025 00:00:00 GMT\r\n\r\n",
            expected_in_response=["304", "200"]  # Either is acceptable depending on file date
        )
        
        # Test 11: POST with Content-Length
        self.test(
            "POST without Content-Length should return 411",
            "POST /test HTTP/1.1\r\nHost: localhost\r\n\r\n",
            expected_status="411"
        )
        
        # Test 12: Required headers in response
        self.test(
            "Response must include Date header",
            "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n",
            expected_in_response=["Date:", "Server:"]
        )
        
        print("\n" + "=" * 60)
        print(f"Test Results: {self.passed} passed, {self.failed} failed")
        print("=" * 60)
        
        return self.failed == 0

def main():
    # Check if server binary exists
    if not os.path.exists('builddir/src/shelob'):
        print("Error: builddir/src/shelob not found. Please build the project first.")
        sys.exit(1)
        
    tester = HTTPComplianceTest()
    
    try:
        tester.start_server()
        success = tester.run_tests()
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        print("\nTest interrupted")
        sys.exit(1)
    finally:
        tester.stop_server()

if __name__ == '__main__':
    main()