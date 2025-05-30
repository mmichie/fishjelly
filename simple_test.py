#!/usr/bin/env python3
import socket
import time

def test_http11_no_host():
    """Test HTTP/1.1 request without Host header - should fail with 400"""
    print("\n=== Testing HTTP/1.1 without Host header ===")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', 8080))
    
    request = b"GET / HTTP/1.1\r\n\r\n"
    print(f"Request:\n{request.decode()}")
    
    sock.send(request)
    response = sock.recv(4096)
    print(f"Response:\n{response.decode()}")
    sock.close()

def test_http11_with_host():
    """Test HTTP/1.1 request with Host header - should succeed"""
    print("\n=== Testing HTTP/1.1 with Host header ===")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', 8080))
    
    request = b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
    print(f"Request:\n{request.decode()}")
    
    sock.send(request)
    response = sock.recv(4096)
    print(f"Response:\n{response.decode()}")
    sock.close()

def test_options():
    """Test OPTIONS method"""
    print("\n=== Testing OPTIONS method ===")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', 8080))
    
    request = b"OPTIONS * HTTP/1.1\r\nHost: localhost\r\n\r\n"
    print(f"Request:\n{request.decode()}")
    
    sock.send(request)
    response = sock.recv(4096)
    print(f"Response:\n{response.decode()}")
    sock.close()

if __name__ == '__main__':
    time.sleep(1)  # Give server time to start
    
    try:
        test_http11_no_host()
    except Exception as e:
        print(f"Error: {e}")
        
    time.sleep(0.5)
        
    try:
        test_http11_with_host()
    except Exception as e:
        print(f"Error: {e}")
        
    time.sleep(0.5)
        
    try:
        test_options()
    except Exception as e:
        print(f"Error: {e}")