#!/usr/bin/env python3

import socket
import subprocess
import threading
from datetime import datetime
from typing import List

# Client configuration
CLIENT_HOST = '0.0.0.0'  # Listen on all interfaces
CLIENT_PORT = 8889
BUFFER_SIZE = 1024
CLIENT_NAME = 'pizero2'


def capture_image(timestamp: str) -> str:
    """Execute libcamera-still to capture an image."""
    filename = f"{CLIENT_NAME}_{timestamp}.jpg"
    
    # Build command - using fastest settings for Pi Zero
    cmd: List[str] = [
        "libcamera-still",
        "-n",              # No preview
        "-t", "1",         # Capture immediately
        "--width", "1920",
        "--height", "1080",
        "-e", "jpg",       # JPEG for faster processing
        "-q", "75",        # Lower quality for speed
        "-o", filename
    ]
    
    try:
        print(f"[{CLIENT_NAME}] Capturing image: {filename}")
        start_time = datetime.now()
        # Don't capture output - let libcamera write directly to console
        result = subprocess.run(cmd, check=True, timeout=15)
        end_time = datetime.now()
        duration = (end_time - start_time).total_seconds()
        print(f"[{CLIENT_NAME}] Capture completed in {duration:.2f}s")
        return f"SUCCESS:{filename}:{duration:.2f}s"
    except subprocess.TimeoutExpired:
        return f"ERROR:Capture timeout after 15s"
    except subprocess.CalledProcessError as e:
        return f"ERROR:Capture failed with code {e.returncode}"
    except Exception as e:
        return f"ERROR:{str(e)}"


def handle_client(conn: socket.socket, addr: tuple) -> None:
    """Handle incoming connection from server."""
    try:
        data = conn.recv(BUFFER_SIZE)
        if not data:
            return
        
        command = data.decode().strip()
        print(f"[{CLIENT_NAME}] Received command: {command} from {addr[0]}")
        
        if command.startswith("CAPTURE:"):
            timestamp = command.split(":", 1)[1]
            response = capture_image(timestamp)
            conn.sendall(response.encode())
        else:
            conn.sendall(b"ERROR:Unknown command")
    
    except Exception as e:
        print(f"[{CLIENT_NAME}] Error handling connection: {e}")
        conn.sendall(f"ERROR:{str(e)}".encode())
    finally:
        conn.close()


def main() -> None:
    """Main client loop."""
    print(f"[{CLIENT_NAME}] Client started on {CLIENT_HOST}:{CLIENT_PORT}")
    print(f"[{CLIENT_NAME}] Waiting for commands...")
    
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_sock:
        server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_sock.bind((CLIENT_HOST, CLIENT_PORT))
        server_sock.listen(5)
        
        while True:
            try:
                conn, addr = server_sock.accept()
                print(f"[{CLIENT_NAME}] Connection from {addr[0]}:{addr[1]}")
                
                # Handle each connection in a new thread
                thread = threading.Thread(target=handle_client, args=(conn, addr))
                thread.daemon = True
                thread.start()
                
            except KeyboardInterrupt:
                print(f"\n[{CLIENT_NAME}] Shutting down...")
                break
            except Exception as e:
                print(f"[{CLIENT_NAME}] Server error: {e}")


if __name__ == "__main__":
    main()