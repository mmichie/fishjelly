#!/usr/bin/env python3
"""Enhanced test script for POST request handling with different content types"""

import requests
import json
import time
from urllib.parse import urlencode

BASE_URL = "http://localhost:8080"

def test_form_urlencoded():
    """Test application/x-www-form-urlencoded POST"""
    print("\n=== Testing application/x-www-form-urlencoded ===")
    
    data = {
        'username': 'john_doe',
        'email': 'john@example.com',
        'message': 'Hello World! This is a test message with special chars: & = ?',
        'age': '25'
    }
    
    response = requests.post(
        f"{BASE_URL}/submit",
        data=data,
        headers={'Content-Type': 'application/x-www-form-urlencoded'}
    )
    
    print(f"Status: {response.status_code}")
    print(f"Response:\n{response.text[:500]}...")

def test_multipart_form():
    """Test multipart/form-data POST"""
    print("\n=== Testing multipart/form-data ===")
    
    data = {
        'username': 'jane_doe',
        'email': 'jane@example.com',
        'bio': 'Software developer\nLoves coding!',
        'interests': 'Python, C++, Web Development'
    }
    
    # requests automatically sets Content-Type with boundary for multipart
    response = requests.post(
        f"{BASE_URL}/upload",
        files={'dummy': ('', '')},  # This forces multipart encoding
        data=data
    )
    
    print(f"Status: {response.status_code}")
    print(f"Response:\n{response.text[:500]}...")

def test_json_post():
    """Test application/json POST (raw body)"""
    print("\n=== Testing application/json ===")
    
    data = {
        'name': 'Test API',
        'version': '1.0',
        'features': ['auth', 'logging', 'caching'],
        'config': {
            'debug': True,
            'port': 8080
        }
    }
    
    response = requests.post(
        f"{BASE_URL}/api/test",
        json=data
    )
    
    print(f"Status: {response.status_code}")
    print(f"Response:\n{response.text[:500]}...")

def test_plain_text_post():
    """Test plain text POST"""
    print("\n=== Testing plain text POST ===")
    
    data = "This is a plain text message.\nIt has multiple lines.\nAnd some special characters: !@#$%^&*()"
    
    response = requests.post(
        f"{BASE_URL}/text",
        data=data,
        headers={'Content-Type': 'text/plain'}
    )
    
    print(f"Status: {response.status_code}")
    print(f"Response:\n{response.text[:500]}...")

def test_large_post():
    """Test large POST body (near limit)"""
    print("\n=== Testing large POST body ===")
    
    # Create a 5MB string
    large_data = 'x' * (5 * 1024 * 1024)
    
    response = requests.post(
        f"{BASE_URL}/large",
        data={'large_field': large_data},
        headers={'Content-Type': 'application/x-www-form-urlencoded'}
    )
    
    print(f"Status: {response.status_code}")
    print(f"Response length: {len(response.text)} bytes")

def test_empty_post():
    """Test empty POST body"""
    print("\n=== Testing empty POST body ===")
    
    response = requests.post(
        f"{BASE_URL}/empty",
        data='',
        headers={'Content-Length': '0'}
    )
    
    print(f"Status: {response.status_code}")
    print(f"Response:\n{response.text[:500]}...")

def test_url_encoded_special_chars():
    """Test URL encoding with special characters"""
    print("\n=== Testing URL encoding special characters ===")
    
    data = {
        'special': 'Hello+World',  # Plus sign
        'percent': '100%',         # Percent sign
        'ampersand': 'A&B',        # Ampersand
        'equals': 'x=y',           # Equals
        'space': 'Hello World',    # Space
        'unicode': 'Café ☕',       # Unicode
    }
    
    response = requests.post(
        f"{BASE_URL}/special",
        data=data
    )
    
    print(f"Status: {response.status_code}")
    print(f"Response:\n{response.text}")

if __name__ == "__main__":
    print("Starting enhanced POST tests...")
    print(f"Make sure the server is running on {BASE_URL}")
    
    time.sleep(1)
    
    try:
        test_form_urlencoded()
        test_multipart_form()
        test_json_post()
        test_plain_text_post()
        test_empty_post()
        test_url_encoded_special_chars()
        # Uncomment to test large POST (takes time)
        # test_large_post()
        
        print("\n✅ All tests completed!")
        
    except requests.exceptions.ConnectionError:
        print(f"\n❌ Error: Could not connect to server at {BASE_URL}")
        print("Make sure the server is running: ./shelob -p 8080")
    except Exception as e:
        print(f"\n❌ Error: {e}")