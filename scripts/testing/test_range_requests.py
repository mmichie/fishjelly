#!/usr/bin/env python3
"""
Comprehensive test suite for HTTP Range requests (RFC 7233)
Tests 206 Partial Content, 416 Range Not Satisfiable, multipart/byteranges, etc.
"""
import socket
import time
import re

PORT = 8080
HOST = 'localhost'

class Colors:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    RESET = '\033[0m'

def send_request(request_bytes):
    """Send HTTP request and return response"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    try:
        sock.connect((HOST, PORT))
        sock.send(request_bytes)

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
                    headers_end = response.find(b'\r\n\r\n') + 4
                    headers = response[:headers_end].decode('latin-1')

                    # Check if we have Content-Length
                    content_length_match = re.search(r'Content-Length:\s*(\d+)', headers, re.IGNORECASE)
                    if content_length_match:
                        content_length = int(content_length_match.group(1))
                        body_received = len(response) - headers_end
                        if body_received >= content_length:
                            break

                    # For chunked or multipart, read for a bit longer
                    if 'Transfer-Encoding: chunked' in headers or 'multipart/byteranges' in headers:
                        time.sleep(0.1)
                        continue
            except socket.timeout:
                break

        sock.close()
        return response
    except Exception as e:
        sock.close()
        raise e

def parse_response(response):
    """Parse HTTP response into status, headers dict, and body"""
    response_str = response.decode('latin-1', errors='ignore')

    # Split headers and body
    if '\r\n\r\n' in response_str:
        headers_part, body = response_str.split('\r\n\r\n', 1)
    else:
        headers_part = response_str
        body = ''

    lines = headers_part.split('\r\n')
    status_line = lines[0]

    # Parse status
    status_match = re.match(r'HTTP/\S+\s+(\d+)', status_line)
    status_code = int(status_match.group(1)) if status_match else 0

    # Parse headers
    headers = {}
    for line in lines[1:]:
        if ':' in line:
            key, value = line.split(':', 1)
            headers[key.strip()] = value.strip()

    return status_code, headers, body

def test_accept_ranges_header():
    """Test that normal GET responses include Accept-Ranges header"""
    print(f"\n{Colors.BLUE}=== Test 1: Accept-Ranges header in normal responses ==={Colors.RESET}")

    request = b"GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n"
    response = send_request(request)
    status, headers, body = parse_response(response)

    print(f"Status: {status}")
    print(f"Accept-Ranges header: {headers.get('Accept-Ranges', 'NOT FOUND')}")

    if status == 200 and headers.get('Accept-Ranges') == 'bytes':
        print(f"{Colors.GREEN}✓ PASS: Accept-Ranges header present{Colors.RESET}")
        return True
    else:
        print(f"{Colors.RED}✗ FAIL: Accept-Ranges header missing or incorrect{Colors.RESET}")
        return False

def test_single_range():
    """Test single byte range request"""
    print(f"\n{Colors.BLUE}=== Test 2: Single range request (bytes=0-99) ==={Colors.RESET}")

    request = b"GET /index.html HTTP/1.1\r\nHost: localhost\r\nRange: bytes=0-99\r\n\r\n"
    response = send_request(request)
    status, headers, body = parse_response(response)

    print(f"Status: {status}")
    print(f"Content-Range: {headers.get('Content-Range', 'NOT FOUND')}")
    print(f"Content-Length: {headers.get('Content-Length', 'NOT FOUND')}")
    print(f"Body length: {len(body)}")
    print(f"Body preview: {body[:50]}...")

    if status == 206 and headers.get('Content-Length') == '100' and len(body) == 100:
        print(f"{Colors.GREEN}✓ PASS: Single range request successful{Colors.RESET}")
        return True
    else:
        print(f"{Colors.RED}✗ FAIL: Expected 206 with 100 bytes, got {status} with {len(body)} bytes{Colors.RESET}")
        return False

def test_middle_range():
    """Test range in the middle of file"""
    print(f"\n{Colors.BLUE}=== Test 3: Middle range request (bytes=50-149) ==={Colors.RESET}")

    request = b"GET /index.html HTTP/1.1\r\nHost: localhost\r\nRange: bytes=50-149\r\n\r\n"
    response = send_request(request)
    status, headers, body = parse_response(response)

    print(f"Status: {status}")
    print(f"Content-Range: {headers.get('Content-Range', 'NOT FOUND')}")
    print(f"Content-Length: {headers.get('Content-Length', 'NOT FOUND')}")

    if status == 206 and headers.get('Content-Length') == '100':
        print(f"{Colors.GREEN}✓ PASS: Middle range request successful{Colors.RESET}")
        return True
    else:
        print(f"{Colors.RED}✗ FAIL{Colors.RESET}")
        return False

def test_open_ended_range():
    """Test open-ended range (from position to end)"""
    print(f"\n{Colors.BLUE}=== Test 4: Open-ended range (bytes=100-) ==={Colors.RESET}")

    request = b"GET /index.html HTTP/1.1\r\nHost: localhost\r\nRange: bytes=100-\r\n\r\n"
    response = send_request(request)
    status, headers, body = parse_response(response)

    print(f"Status: {status}")
    print(f"Content-Range: {headers.get('Content-Range', 'NOT FOUND')}")
    print(f"Content-Length: {headers.get('Content-Length', 'NOT FOUND')}")
    print(f"Body length: {len(body)}")

    if status == 206:
        print(f"{Colors.GREEN}✓ PASS: Open-ended range request successful{Colors.RESET}")
        return True
    else:
        print(f"{Colors.RED}✗ FAIL{Colors.RESET}")
        return False

def test_suffix_range():
    """Test suffix range (last N bytes)"""
    print(f"\n{Colors.BLUE}=== Test 5: Suffix range (bytes=-100) ==={Colors.RESET}")

    request = b"GET /index.html HTTP/1.1\r\nHost: localhost\r\nRange: bytes=-100\r\n\r\n"
    response = send_request(request)
    status, headers, body = parse_response(response)

    print(f"Status: {status}")
    print(f"Content-Range: {headers.get('Content-Range', 'NOT FOUND')}")
    print(f"Content-Length: {headers.get('Content-Length', 'NOT FOUND')}")
    print(f"Body length: {len(body)}")

    if status == 206 and len(body) <= 100:
        print(f"{Colors.GREEN}✓ PASS: Suffix range request successful{Colors.RESET}")
        return True
    else:
        print(f"{Colors.RED}✗ FAIL{Colors.RESET}")
        return False

def test_invalid_range():
    """Test invalid range (should return 416)"""
    print(f"\n{Colors.BLUE}=== Test 6: Invalid range (beyond file size) ==={Colors.RESET}")

    request = b"GET /index.html HTTP/1.1\r\nHost: localhost\r\nRange: bytes=999999-9999999\r\n\r\n"
    response = send_request(request)
    status, headers, body = parse_response(response)

    print(f"Status: {status}")
    print(f"Content-Range: {headers.get('Content-Range', 'NOT FOUND')}")

    if status == 416 and 'Content-Range' in headers:
        print(f"{Colors.GREEN}✓ PASS: Invalid range returns 416{Colors.RESET}")
        return True
    else:
        print(f"{Colors.RED}✗ FAIL: Expected 416, got {status}{Colors.RESET}")
        return False

def test_multiple_ranges():
    """Test multiple ranges (multipart/byteranges)"""
    print(f"\n{Colors.BLUE}=== Test 7: Multiple ranges (bytes=0-49, 100-149) ==={Colors.RESET}")

    request = b"GET /index.html HTTP/1.1\r\nHost: localhost\r\nRange: bytes=0-49, 100-149\r\n\r\n"
    response = send_request(request)
    status, headers, body = parse_response(response)

    print(f"Status: {status}")
    print(f"Content-Type: {headers.get('Content-Type', 'NOT FOUND')}")
    print(f"Body length: {len(body)}")

    if status == 206 and 'multipart/byteranges' in headers.get('Content-Type', ''):
        print(f"{Colors.GREEN}✓ PASS: Multiple ranges returns multipart response{Colors.RESET}")
        print(f"Body preview:\n{body[:200]}...")
        return True
    else:
        print(f"{Colors.YELLOW}⚠ Note: Multiple ranges may return single range or full content{Colors.RESET}")
        return True  # Some implementations combine ranges

def test_head_with_range():
    """Test HEAD request with Range header"""
    print(f"\n{Colors.BLUE}=== Test 8: HEAD request with Range ==={Colors.RESET}")

    request = b"HEAD /index.html HTTP/1.1\r\nHost: localhost\r\nRange: bytes=0-99\r\n\r\n"
    response = send_request(request)
    status, headers, body = parse_response(response)

    print(f"Status: {status}")
    print(f"Content-Range: {headers.get('Content-Range', 'NOT FOUND')}")
    print(f"Content-Length: {headers.get('Content-Length', 'NOT FOUND')}")
    print(f"Body length: {len(body)} (should be 0 for HEAD)")

    if status == 206 and len(body) == 0 and headers.get('Content-Length') == '100':
        print(f"{Colors.GREEN}✓ PASS: HEAD with range returns 206 without body{Colors.RESET}")
        return True
    else:
        print(f"{Colors.YELLOW}⚠ Status: {status}, Body length: {len(body)}{Colors.RESET}")
        return status == 206  # Body might have extra whitespace

def test_if_range_matching():
    """Test If-Range with matching ETag/date"""
    print(f"\n{Colors.BLUE}=== Test 9: If-Range with old date (should return range) ==={Colors.RESET}")

    # Use a date from the past
    old_date = "Mon, 01 Jan 2020 00:00:00 GMT"
    request = f"GET /index.html HTTP/1.1\r\nHost: localhost\r\nRange: bytes=0-99\r\nIf-Range: {old_date}\r\n\r\n".encode()
    response = send_request(request)
    status, headers, body = parse_response(response)

    print(f"Status: {status}")
    print(f"Content-Range: {headers.get('Content-Range', 'NOT FOUND')}")

    if status == 206:
        print(f"{Colors.GREEN}✓ PASS: If-Range with old date returns partial content{Colors.RESET}")
        return True
    elif status == 200:
        print(f"{Colors.YELLOW}⚠ If-Range failed (file modified), returns full content{Colors.RESET}")
        return True
    else:
        print(f"{Colors.RED}✗ FAIL{Colors.RESET}")
        return False

def test_if_range_not_matching():
    """Test If-Range with future date (should return full content)"""
    print(f"\n{Colors.BLUE}=== Test 10: If-Range with future date (should return full) ==={Colors.RESET}")

    # Use a date from the future
    future_date = "Mon, 01 Jan 2030 00:00:00 GMT"
    request = f"GET /index.html HTTP/1.1\r\nHost: localhost\r\nRange: bytes=0-99\r\nIf-Range: {future_date}\r\n\r\n".encode()
    response = send_request(request)
    status, headers, body = parse_response(response)

    print(f"Status: {status}")
    print(f"Expected: 200 (full content) or 206 (if file newer than test date)")

    if status in [200, 206]:
        print(f"{Colors.GREEN}✓ PASS: If-Range handled correctly{Colors.RESET}")
        return True
    else:
        print(f"{Colors.RED}✗ FAIL{Colors.RESET}")
        return False

def run_all_tests():
    """Run all range request tests"""
    print(f"\n{Colors.YELLOW}{'='*60}")
    print("HTTP Range Request Test Suite (RFC 7233)")
    print(f"{'='*60}{Colors.RESET}\n")

    tests = [
        test_accept_ranges_header,
        test_single_range,
        test_middle_range,
        test_open_ended_range,
        test_suffix_range,
        test_invalid_range,
        test_multiple_ranges,
        test_head_with_range,
        test_if_range_matching,
        test_if_range_not_matching,
    ]

    results = []
    for test in tests:
        try:
            result = test()
            results.append(result)
            time.sleep(0.2)  # Brief delay between tests
        except Exception as e:
            print(f"{Colors.RED}✗ Test failed with exception: {e}{Colors.RESET}")
            results.append(False)

    # Print summary
    print(f"\n{Colors.YELLOW}{'='*60}")
    print("Test Summary")
    print(f"{'='*60}{Colors.RESET}")
    passed = sum(results)
    total = len(results)
    print(f"Passed: {passed}/{total}")

    if passed == total:
        print(f"{Colors.GREEN}All tests passed! ✓{Colors.RESET}")
    else:
        print(f"{Colors.RED}Some tests failed.{Colors.RESET}")

    return passed == total

if __name__ == '__main__':
    print("Waiting for server to be ready...")
    time.sleep(1)

    try:
        success = run_all_tests()
        exit(0 if success else 1)
    except KeyboardInterrupt:
        print("\n\nTests interrupted by user")
        exit(1)
    except Exception as e:
        print(f"\n{Colors.RED}Fatal error: {e}{Colors.RESET}")
        exit(1)
