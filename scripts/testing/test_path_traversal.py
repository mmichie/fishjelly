#!/usr/bin/env python3
"""
Test path traversal vulnerability fixes
Tests various attack vectors to ensure they are all blocked
"""

import requests
import sys

# Test server URL
BASE_URL = "http://localhost:8080"

# Path traversal attack vectors from the roadmap
test_cases = [
    {
        "name": "Basic path traversal (../)",
        "path": "/../../../etc/passwd",
        "should_fail": True
    },
    {
        "name": "Windows-style traversal (..\\)",
        "path": "/..\\..\\..\\etc\\passwd",
        "should_fail": True
    },
    {
        "name": "URL encoded traversal (..%2f)",
        "path": "/..%2f..%2f..%2fetc/passwd",
        "should_fail": True
    },
    {
        "name": "Double encoded traversal (..%252f)",
        "path": "/..%252f..%252f..%252fetc/passwd",
        "should_fail": True
    },
    {
        "name": "Overlong UTF-8 (..%c0%af)",
        "path": "/..%c0%af..%c0%af..%c0%afetc/passwd",
        "should_fail": True
    },
    {
        "name": "Dot-dot-slash-slash (....//)",
        "path": "/....//....//....//etc/passwd",
        "should_fail": True
    },
    {
        "name": "Null byte injection (%00)",
        "path": "/../../../etc/passwd%00.html",
        "should_fail": True
    },
    {
        "name": "Mixed encoding",
        "path": "/..%2f..%5c..%2fetc/passwd",
        "should_fail": True
    },
    {
        "name": "Triple encoding",
        "path": "/..%25252f..%25252fetc/passwd",
        "should_fail": True
    },
    {
        "name": "Valid path (index.html)",
        "path": "/index.html",
        "should_fail": False
    },
    {
        "name": "Valid path (root)",
        "path": "/",
        "should_fail": False
    }
]

def test_path_traversal():
    """Test all path traversal attack vectors"""
    print("=" * 70)
    print("Path Traversal Security Test")
    print("=" * 70)

    passed = 0
    failed = 0

    for test in test_cases:
        try:
            url = BASE_URL + test["path"]
            response = requests.get(url, timeout=5, allow_redirects=False)

            # Check if the response matches expectations
            if test["should_fail"]:
                # Attack should be blocked with 400 or 403
                if response.status_code in [400, 403]:
                    print(f"✓ PASS: {test['name']}")
                    print(f"  Path: {test['path']}")
                    print(f"  Status: {response.status_code} (blocked as expected)")
                    passed += 1
                else:
                    print(f"✗ FAIL: {test['name']}")
                    print(f"  Path: {test['path']}")
                    print(f"  Status: {response.status_code} (SHOULD HAVE BEEN BLOCKED!)")
                    print(f"  Response snippet: {response.text[:100]}")
                    failed += 1
            else:
                # Valid request should succeed
                if response.status_code == 200:
                    print(f"✓ PASS: {test['name']}")
                    print(f"  Path: {test['path']}")
                    print(f"  Status: {response.status_code} (allowed as expected)")
                    passed += 1
                else:
                    print(f"✗ FAIL: {test['name']}")
                    print(f"  Path: {test['path']}")
                    print(f"  Status: {response.status_code} (should have been allowed)")
                    failed += 1

            print()

        except Exception as e:
            print(f"✗ ERROR: {test['name']}")
            print(f"  Path: {test['path']}")
            print(f"  Error: {e}")
            print()
            failed += 1

    print("=" * 70)
    print(f"Results: {passed} passed, {failed} failed out of {len(test_cases)} tests")
    print("=" * 70)

    return failed == 0

if __name__ == "__main__":
    success = test_path_traversal()
    sys.exit(0 if success else 1)
