#!/usr/bin/env python3

import socket
import threading
import time
from datetime import datetime
from typing import List, Tuple

# Server configuration
SERVER_HOST = "0.0.0.0"  # Listen on all interfaces
SERVER_PORT = 8888
BUFFER_SIZE = 1024

# Client addresses (using direct IP addresses)
CLIENTS = [
    ("192.168.100.126", 8889),  # PiZero1
    ("192.168.100.127", 8889),  # PiZero2
]


def send_capture_command(client_addr: Tuple[str, int], command: str) -> None:
    """Send capture command to a client."""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.settimeout(5)
            sock.connect(client_addr)
            sock.sendall(command.encode())
            response = sock.recv(BUFFER_SIZE)
            print(f"[{client_addr[0]}] Response: {response.decode()}")
    except Exception as e:
        print(f"[{client_addr[0]}] Error: {e}")


def broadcast_capture() -> None:
    """Send capture command to all clients simultaneously."""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    command = f"CAPTURE:{timestamp}"

    print(f"\n[Server] Broadcasting capture command at {timestamp}")

    # Create threads for parallel sending
    threads: List[threading.Thread] = []
    for client in CLIENTS:
        thread = threading.Thread(target=send_capture_command, args=(client, command))
        threads.append(thread)
        thread.start()

    # Wait for all threads to complete
    for thread in threads:
        thread.join()

    print("[Server] Broadcast complete\n")


def main() -> None:
    """Main server loop."""
    print(f"server started on {SERVER_HOST}:{SERVER_PORT}")
    print(f"configured clients: {CLIENTS}")
    print("\nCommands:")
    print("  'c' or 'capture' - Trigger capture on all clients")
    print("  'q' or 'quit' - Exit server")
    print("-" * 50)

    while True:
        cmd = input("\nEnter command: ").strip().lower()

        if cmd in ["c", "capture"]:
            broadcast_capture()
        elif cmd in ["q", "quit"]:
            print("Server shutting down...")
            break
        else:
            print("Unknown command. Use 'c' for capture or 'q' to quit.")


if __name__ == "__main__":
    main()

