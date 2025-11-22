#!/usr/bin/env python3
"""
WebSocket Echo Server Test

Tests the WebSocket functionality of Fishjelly server.
Requires: pip install websockets
"""

import asyncio
import websockets
import sys


async def test_echo():
    """Test WebSocket echo functionality"""
    uri = "ws://localhost:8080/"

    try:
        print(f"Connecting to {uri}...")
        async with websockets.connect(uri) as websocket:
            print("✓ WebSocket connection established")

            # Test 1: Simple text message
            test_message = "Hello, WebSocket!"
            print(f"\nSending: {test_message}")
            await websocket.send(test_message)

            response = await websocket.recv()
            print(f"Received: {response}")

            if response == test_message:
                print("✓ Echo test passed")
            else:
                print(f"✗ Echo test failed: expected '{test_message}', got '{response}'")
                return False

            # Test 2: Multiple messages
            messages = ["Message 1", "Message 2", "Message 3"]
            print("\nTesting multiple messages...")
            for msg in messages:
                await websocket.send(msg)
                response = await websocket.recv()
                if response == msg:
                    print(f"✓ {msg} echoed correctly")
                else:
                    print(f"✗ Failed: expected '{msg}', got '{response}'")
                    return False

            # Test 3: Longer message
            long_message = "A" * 1000
            print(f"\nSending long message ({len(long_message)} bytes)...")
            await websocket.send(long_message)
            response = await websocket.recv()

            if response == long_message:
                print("✓ Long message test passed")
            else:
                print(f"✗ Long message test failed")
                return False

            print("\n✓ All WebSocket tests passed!")
            return True

    except ConnectionRefusedError:
        print("✗ Connection refused. Is the server running on port 8080?")
        return False
    except Exception as e:
        print(f"✗ Error: {e}")
        return False


if __name__ == "__main__":
    try:
        result = asyncio.run(test_echo())
        sys.exit(0 if result else 1)
    except KeyboardInterrupt:
        print("\nTest interrupted")
        sys.exit(1)
