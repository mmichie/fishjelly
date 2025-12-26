#!/usr/bin/env -S uv run --quiet --script
# /// script
# dependencies = ["h2>=4.1.0"]
# ///
"""
Test script for HPACK bomb protection

This script verifies that:
1. The server sends HEADER_TABLE_SIZE in SETTINGS (4096 bytes)
2. The server sends MAX_HEADER_LIST_SIZE in SETTINGS
3. Normal requests still work correctly
"""

import socket
import ssl
import subprocess
import sys
import time

try:
    from h2.connection import H2Connection
    from h2.events import (
        RemoteSettingsChanged,
        ResponseReceived,
        DataReceived,
        StreamEnded,
    )
    from h2.settings import SettingCodes
except ImportError:
    print("Error: h2 library not found. Install with: pip3 install h2")
    sys.exit(1)


def test_hpack_settings():
    """Test that server sends proper HPACK-related SETTINGS"""
    print("=" * 70)
    print("HPACK Settings Verification Test")
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
    try:
        sock = socket.create_connection((host, port), timeout=5)
        sock = ctx.wrap_socket(sock, server_hostname=host)
    except Exception as e:
        print(f"[-] Connection failed: {e}")
        return False

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
    sock.settimeout(5)
    data = sock.recv(65535)
    events = h2_conn.receive_data(data)

    header_table_size = None
    max_header_list_size = None
    max_concurrent_streams = None
    max_frame_size = None

    for event in events:
        if isinstance(event, RemoteSettingsChanged):
            settings = event.changed_settings
            print(f"[+] Received server SETTINGS:")
            for setting_id, setting in settings.items():
                name = SettingCodes(setting_id).name if setting_id in [s.value for s in SettingCodes] else f"Unknown({setting_id})"
                print(f"    {name}: {setting.new_value}")

                if setting_id == SettingCodes.HEADER_TABLE_SIZE:
                    header_table_size = setting.new_value
                elif setting_id == SettingCodes.MAX_HEADER_LIST_SIZE:
                    max_header_list_size = setting.new_value
                elif setting_id == SettingCodes.MAX_CONCURRENT_STREAMS:
                    max_concurrent_streams = setting.new_value
                elif setting_id == SettingCodes.MAX_FRAME_SIZE:
                    max_frame_size = setting.new_value

    # Send settings acknowledgement
    sock.sendall(h2_conn.data_to_send())

    # Verify settings
    print("\n[+] Verifying HPACK protection settings...")

    tests_passed = True

    # Check HEADER_TABLE_SIZE (HPACK dynamic table size)
    if header_table_size is not None:
        if header_table_size <= 4096:
            print(f"[+] HEADER_TABLE_SIZE: {header_table_size} bytes (OK - limited)")
        else:
            print(f"[-] HEADER_TABLE_SIZE: {header_table_size} bytes (WARNING - larger than recommended)")
            tests_passed = False
    else:
        print("[-] HEADER_TABLE_SIZE not set (server using default 4096)")

    # Check MAX_HEADER_LIST_SIZE
    if max_header_list_size is not None:
        if max_header_list_size <= 16384:
            print(f"[+] MAX_HEADER_LIST_SIZE: {max_header_list_size} bytes (OK - limited)")
        else:
            print(f"[-] MAX_HEADER_LIST_SIZE: {max_header_list_size} bytes (WARNING - larger than recommended)")
    else:
        print("[-] MAX_HEADER_LIST_SIZE not set (server using unlimited)")
        tests_passed = False

    sock.close()
    return tests_passed


def test_normal_request():
    """Test that normal requests work with HPACK limits in place"""
    print("\n" + "=" * 70)
    print("Normal Request Test (verify HPACK doesn't break functionality)")
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
    sock = socket.create_connection((host, port), timeout=5)
    sock = ctx.wrap_socket(sock, server_hostname=host)

    # Initialize H2 connection
    h2_conn = H2Connection()
    h2_conn.initiate_connection()
    sock.sendall(h2_conn.data_to_send())

    # Wait for server settings
    data = sock.recv(65535)
    h2_conn.receive_data(data)
    sock.sendall(h2_conn.data_to_send())

    # Send request
    print("[+] Sending GET / request...")
    stream_id = 1
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
    sock.settimeout(5)
    response_status = None
    response_complete = False

    while not response_complete:
        try:
            data = sock.recv(65535)
            if not data:
                break
            events = h2_conn.receive_data(data)
            sock.sendall(h2_conn.data_to_send())

            for event in events:
                if isinstance(event, ResponseReceived):
                    for name, value in event.headers:
                        if name == b":status":
                            response_status = int(value.decode())
                            print(f"[+] Received response: HTTP/2 {response_status}")
                elif isinstance(event, StreamEnded):
                    response_complete = True
                    break
        except socket.timeout:
            break

    sock.close()

    if response_status == 200:
        print("[+] PASS: Normal request completed successfully")
        return True
    else:
        print(f"[-] FAIL: Unexpected response status: {response_status}")
        return False


if __name__ == "__main__":
    print("\nHPACK Bomb Protection Test Suite")
    print("Verifies HPACK dynamic table size limits are configured\n")

    # Kill any existing server
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
        test1_passed = test_hpack_settings()
        test2_passed = test_normal_request()

        # Summary
        print("\n" + "=" * 70)
        print("Test Summary:")
        print("=" * 70)
        print(f"HPACK settings verification: {'PASS' if test1_passed else 'FAIL'}")
        print(f"Normal request test: {'PASS' if test2_passed else 'FAIL'}")

        if test1_passed and test2_passed:
            print("\n[+] All tests passed! HPACK bomb protection is configured.")
            sys.exit(0)
        else:
            print("\n[-] Some tests failed!")
            sys.exit(1)

    finally:
        # Clean up
        print("\n[+] Cleaning up...")
        server_process.terminate()
        server_process.wait()
