#!/usr/bin/env python3
"""Test real keep-alive functionality"""

import socket
import time

def test_keepalive():
    print("Testing real keep-alive functionality...")
    
    # Connect to server
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    sock.connect(('localhost', 8080))
    
    # First request (keep-alive)
    print("\n1. Sending first request (keep-alive)...")
    request1 = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
    sock.send(request1.encode())
    
    # Read response with exact Content-Length
    response1 = b''
    while True:
        data = sock.recv(1024)
        if not data:
            break
        response1 += data
        if b'\r\n\r\n' in response1:
            headers_end = response1.find(b'\r\n\r\n')
            headers = response1[:headers_end].decode()
            if 'Content-Length:' in headers:
                for line in headers.split('\r\n'):
                    if line.startswith('Content-Length:'):
                        content_length = int(line.split()[1])
                        body_start = headers_end + 4
                        if len(response1) >= body_start + content_length:
                            break
    
    print("Response 1:", response1.decode()[:200])
    assert b'Connection: keep-alive' in response1
    
    # Second request on same connection
    print("\n2. Sending second request on same connection...")
    time.sleep(0.5)  # Small delay
    request2 = "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n"
    sock.send(request2.encode())
    
    # Read second response
    response2 = b''
    while True:
        data = sock.recv(1024)
        if not data:
            break
        response2 += data
        if b'\r\n\r\n' in response2:
            headers_end = response2.find(b'\r\n\r\n')
            headers = response2[:headers_end].decode()
            if 'Content-Length:' in headers:
                for line in headers.split('\r\n'):
                    if line.startswith('Content-Length:'):
                        content_length = int(line.split()[1])
                        body_start = headers_end + 4
                        if len(response2) >= body_start + content_length:
                            break
    
    print("Response 2:", response2.decode()[:200])
    assert b'Connection: keep-alive' in response2
    
    # Third request with Connection: close
    print("\n3. Sending third request with Connection: close...")
    request3 = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
    sock.send(request3.encode())
    
    # Read final response (should close after this)
    response3 = b''
    while True:
        data = sock.recv(1024)
        if not data:
            break
        response3 += data
    
    print("Response 3:", response3.decode()[:200])
    assert b'Connection: close' in response3
    
    sock.close()
    print("\n✅ Keep-alive test passed! Connection reused successfully.")

if __name__ == "__main__":
    test_keepalive()