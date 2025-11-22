#!/usr/bin/env -S uv run --quiet --script
# /// script
# dependencies = ["h2>=4.1.0"]
# ///
"""
Test script for HTTP/2 Rapid Reset (CVE-2023-44487) protection

This script simulates the rapid reset attack by:
1. Opening an HTTP/2 connection
2. Creating multiple streams
3. Immediately sending RST_STREAM for each
4. Repeating until the server terminates the connection
"""

import socket
import ssl
import sys
import time

try:
    from h2.connection import H2Connection
    from h2.events import (
        RemoteSettingsChanged,
        SettingsAcknowledged,
        StreamEnded,
        StreamReset,
        ConnectionTerminated,
    )
    from h2.errors import ErrorCodes
except ImportError:
    print("Error: h2 library not found. Install with: pip3 install h2")
    sys.exit(1)


def test_rapid_reset_protection():
    """Test that rapid reset attacks are detected and blocked"""
    print("=" * 70)
    print("HTTP/2 Rapid Reset (CVE-2023-44487) Protection Test")
    print("=" * 70)

    # Configuration
    host = "localhost"
    port = 8443
    max_resets = 150  # Server limit is 100, we'll try to exceed it

    # Create SSL context (disable cert verification for self-signed certs)
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    ctx.set_alpn_protocols(["h2"])

    # Connect
    print(f"\n[+] Connecting to {host}:{port}...")
    sock = socket.create_connection((host, port))
    sock = ctx.wrap_socket(sock, server_hostname=host)

    # Verify HTTP/2 via ALPN
    negotiated_protocol = sock.selected_alpn_protocol()
    if negotiated_protocol != "h2":
        print(f"[-] ALPN negotiation failed. Got: {negotiated_protocol}")
        return False

    print(f"[+] ALPN negotiated: {negotiated_protocol}")

    # Initialize H2 connection
    h2_conn = H2Connection()
    h2_conn.initiate_connection()
    sock.sendall(h2_conn.data_to_send())

    # Wait for server settings
    print("[+] Waiting for server SETTINGS...")
    data = sock.recv(65535)
    events = h2_conn.receive_data(data)

    for event in events:
        if isinstance(event, RemoteSettingsChanged):
            print(f"[+] Received server SETTINGS: {event.changed_settings}")

    # Send settings acknowledgement
    sock.sendall(h2_conn.data_to_send())

    # Perform rapid reset attack
    print(f"\n[+] Starting rapid reset attack ({max_resets} resets)...")
    resets_sent = 0
    connection_terminated = False

    for i in range(1, max_resets + 1):
        stream_id = i * 2 + 1  # Client streams are odd

        # Send HEADERS frame to open stream
        h2_conn.send_headers(
            stream_id=stream_id,
            headers=[
                (":method", "GET"),
                (":scheme", "https"),
                (":authority", f"{host}:{port}"),
                (":path", "/"),
            ],
        )

        # Immediately send RST_STREAM
        h2_conn.reset_stream(stream_id, error_code=ErrorCodes.CANCEL)
        resets_sent += 1

        # Send data
        try:
            sock.sendall(h2_conn.data_to_send())
        except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
            print(f"\n[+] Connection closed after {resets_sent} resets")
            connection_terminated = True
            break

        # Check for response every 10 resets
        if i % 10 == 0:
            try:
                sock.settimeout(0.1)
                data = sock.recv(65535)
                if data:
                    events = h2_conn.receive_data(data)
                    for event in events:
                        if isinstance(event, ConnectionTerminated):
                            print(f"\n[+] Server sent GOAWAY after {resets_sent} resets")
                            print(f"    Error code: {event.error_code}")
                            print(f"    Last stream: {event.last_stream_id}")
                            if event.additional_data:
                                print(f"    Message: {event.additional_data.decode('utf-8', errors='ignore')}")
                            connection_terminated = True
                            break
                sock.settimeout(None)
            except socket.timeout:
                pass

            if connection_terminated:
                break

            print(f"    Sent {resets_sent} resets...", end="\r")

    # Clean up
    sock.close()

    # Evaluate results
    print("\n")
    print("=" * 70)
    print("Test Results:")
    print("=" * 70)

    if connection_terminated and resets_sent <= 110:
        print(f"[✓] PASS: Server detected rapid reset attack")
        print(f"    Connection terminated after {resets_sent} resets")
        print(f"    (Expected: ~100 resets, got: {resets_sent})")
        return True
    elif connection_terminated:
        print(f"[!] WARNING: Connection terminated, but after {resets_sent} resets")
        print(f"    Expected termination around 100 resets")
        return True
    else:
        print(f"[-] FAIL: Server did not detect rapid reset attack")
        print(f"    Sent {resets_sent} resets without server response")
        return False


def test_normal_operation():
    """Test that normal HTTP/2 operation still works"""
    print("\n" + "=" * 70)
    print("Normal Operation Test (verify false positives)")
    print("=" * 70)

    host = "localhost"
    port = 8443

    # Create SSL context
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    ctx.set_alpn_protocols(["h2"])

    # Connect
    print(f"\n[+] Connecting to {host}:{port}...")
    sock = socket.create_connection((host, port))
    sock = ctx.wrap_socket(sock, server_hostname=host)

    # Initialize H2 connection
    h2_conn = H2Connection()
    h2_conn.initiate_connection()
    sock.sendall(h2_conn.data_to_send())

    # Wait for server settings
    data = sock.recv(65535)
    h2_conn.receive_data(data)
    sock.sendall(h2_conn.data_to_send())

    # Send 5 normal requests (well below threshold)
    print("[+] Sending 5 normal requests...")
    for i in range(5):
        stream_id = i * 2 + 1

        h2_conn.send_headers(
            stream_id=stream_id,
            headers=[
                (":method", "GET"),
                (":scheme", "https"),
                (":authority", f"{host}:{port}"),
                (":path", "/index.html"),
            ],
            end_stream=True,
        )
        sock.sendall(h2_conn.data_to_send())

        # Wait for response
        sock.settimeout(2.0)
        try:
            data = sock.recv(65535)
            events = h2_conn.receive_data(data)
            print(f"    Request {i+1}: OK")
        except socket.timeout:
            print(f"    Request {i+1}: Timeout")
            return False

    sock.close()

    print("\n[✓] PASS: Normal requests work correctly")
    return True


if __name__ == "__main__":
    print("\nHTTP/2 Rapid Reset Attack Protection Test Suite")
    print("This tests CVE-2023-44487 mitigation\n")

    # Kill any existing server
    import subprocess
    subprocess.run(["pkill", "-f", "shelob.*--http2"], stderr=subprocess.DEVNULL)
    time.sleep(1)

    # Start server
    print("[+] Starting HTTP/2 server...")
    server_process = subprocess.Popen(
        ["../builddir/src/shelob", "--ssl", "--http2", "--ssl-port", "8443"],
        cwd="/Users/mim/src/fishjelly/base",
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    time.sleep(2)

    try:
        # Run tests
        test1_passed = test_normal_operation()
        test2_passed = test_rapid_reset_protection()

        # Summary
        print("\n" + "=" * 70)
        print("Test Summary:")
        print("=" * 70)
        print(f"Normal operation: {'✓ PASS' if test1_passed else '✗ FAIL'}")
        print(f"Rapid reset protection: {'✓ PASS' if test2_passed else '✗ FAIL'}")

        if test1_passed and test2_passed:
            print("\n[✓] All tests passed! HTTP/2 Rapid Reset protection is working.")
            sys.exit(0)
        else:
            print("\n[✗] Some tests failed!")
            sys.exit(1)

    finally:
        # Clean up
        print("\n[+] Cleaning up...")
        server_process.terminate()
        server_process.wait()
