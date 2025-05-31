#!/usr/bin/env python3
"""Test POST request handling in fishjelly web server."""

import requests
import json
import sys

# Server configuration
HOST = "localhost"
PORT = 8080
BASE_URL = f"http://{HOST}:{PORT}"

def test_simple_post():
    """Test a simple POST request with form data."""
    print("Testing simple POST with form data...")
    data = {"name": "John Doe", "email": "john@example.com"}
    response = requests.post(f"{BASE_URL}/test", data=data)
    print(f"Status: {response.status_code}")
    print(f"Response: {response.text[:200]}...")
    print()

def test_json_post():
    """Test POST request with JSON data."""
    print("Testing POST with JSON data...")
    json_data = {"user": "alice", "items": ["apple", "banana", "cherry"]}
    headers = {"Content-Type": "application/json"}
    response = requests.post(f"{BASE_URL}/api/data", 
                           data=json.dumps(json_data), 
                           headers=headers)
    print(f"Status: {response.status_code}")
    print(f"Response: {response.text[:200]}...")
    print()

def test_empty_post():
    """Test POST request with no body."""
    print("Testing POST with empty body...")
    response = requests.post(f"{BASE_URL}/empty")
    print(f"Status: {response.status_code}")
    print(f"Response: {response.text[:200]}...")
    print()

def test_large_post():
    """Test POST request with large body."""
    print("Testing POST with large body (1MB)...")
    large_data = "x" * (1024 * 1024)  # 1MB of data
    response = requests.post(f"{BASE_URL}/upload", data=large_data)
    print(f"Status: {response.status_code}")
    print(f"Response: {response.text[:200]}...")
    print()

def test_no_content_length():
    """Test POST without Content-Length header (should fail)."""
    print("Testing POST without Content-Length...")
    # This is tricky with requests as it auto-adds Content-Length
    # We'll use a raw socket connection for this test
    import socket
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect((HOST, PORT))
        request = b"POST /test HTTP/1.1\r\nHost: localhost\r\n\r\n"
        sock.send(request)
        response = sock.recv(1024).decode()
        print(f"Response: {response}")
    finally:
        sock.close()
    print()

def test_too_large_post():
    """Test POST request exceeding size limit."""
    print("Testing POST exceeding size limit (11MB)...")
    try:
        huge_data = "x" * (11 * 1024 * 1024)  # 11MB of data
        response = requests.post(f"{BASE_URL}/upload", data=huge_data)
        print(f"Status: {response.status_code}")
        print(f"Response: {response.text[:200]}...")
    except Exception as e:
        print(f"Error: {e}")
    print()

if __name__ == "__main__":
    print(f"Testing POST functionality on {BASE_URL}")
    print("Make sure the server is running on port {PORT}")
    print("-" * 50)
    
    try:
        test_simple_post()
        test_json_post()
        test_empty_post()
        test_large_post()
        test_no_content_length()
        test_too_large_post()
        
        print("All tests completed!")
    except requests.exceptions.ConnectionError:
        print(f"Error: Could not connect to server at {BASE_URL}")
        print("Make sure the server is running: ./builddir/src/shelob -p 8080")
        sys.exit(1)
    except Exception as e:
        print(f"Unexpected error: {e}")
        sys.exit(1)