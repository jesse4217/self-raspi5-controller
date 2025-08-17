#!/usr/bin/env python3
"""
HQ Camera Client - Raspberry Pi camera client with server registration.
Automatically registers with server and handles camera operations.
"""

import socket
import subprocess
import threading
import time
import os
import sys
from datetime import datetime
from typing import List, Optional
import signal

# Import our modules
from camera_protocol import CameraProtocol, MessageType, ProtocolConfig, connect_with_retry
from file_manager import ImageFileManager
from s3_uploader import get_default_s3_uploader


class CameraClient:
    """HQ Camera client for Raspberry Pi."""
    
    def __init__(self, server_host: str, server_port: int = 8888, client_port: int = 8889):
        self.server_host = server_host
        self.server_port = server_port
        self.client_port = client_port
        self.hostname = socket.gethostname()
        
        self.protocol = CameraProtocol()
        self.file_manager = ImageFileManager()
        self.s3_uploader = get_default_s3_uploader()
        
        self.running = False
        self.client_socket = None
        self.registration_thread = None
        self.heartbeat_thread = None
        
        print(f"Camera Client [{self.hostname}] initialized")
        print(f"   Server: {server_host}:{server_port}")
        print(f"   Client port: {client_port}")
    
    def capture_image(self, timestamp: str) -> dict:
        """Execute libcamera-still to capture an image."""
        filename = f"{self.hostname}_{timestamp}.jpg"
        
        # Build command - optimized for Pi Zero
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
            print(f"[{self.hostname}] Capturing: {filename}")
            start_time = datetime.now()
            
            # Execute capture command
            result = subprocess.run(cmd, check=True, timeout=15, 
                                  capture_output=False, text=True)
            
            end_time = datetime.now()
            duration = (end_time - start_time).total_seconds()
            
            # Verify file was created
            if os.path.exists(filename):
                file_size = os.path.getsize(filename)
                print(f"[{self.hostname}] Capture completed in {duration:.2f}s ({file_size} bytes)")
                return {
                    'status': 'SUCCESS',
                    'message': f"Captured {filename}",
                    'data': {
                        'filename': filename,
                        'duration': duration,
                        'size_bytes': file_size
                    }
                }
            else:
                return {
                    'status': 'ERROR',
                    'message': 'Image file not created'
                }
                
        except subprocess.TimeoutExpired:
            return {
                'status': 'ERROR',
                'message': 'Capture timeout after 15s'
            }
        except subprocess.CalledProcessError as e:
            return {
                'status': 'ERROR', 
                'message': f'Capture failed with code {e.returncode}'
            }
        except Exception as e:
            return {
                'status': 'ERROR',
                'message': str(e)
            }
    
    def list_images(self) -> dict:
        """List all captured images."""
        try:
            image_info = self.file_manager.get_image_info()
            print(f"[{self.hostname}] Found {image_info['count']} images ({image_info['total_size_mb']:.2f} MB)")
            
            return {
                'status': 'SUCCESS',
                'message': f"Found {image_info['count']} images",
                'data': image_info
            }
        except Exception as e:
            return {
                'status': 'ERROR',
                'message': str(e)
            }
    
    def upload_to_s3(self, bucket_name: str) -> dict:
        """Upload all images to S3."""
        try:
            # Check S3 credentials
            if not self.s3_uploader.check_credentials():
                return {
                    'status': 'ERROR',
                    'message': 'No valid AWS credentials found'
                }
            
            # Get list of images
            images = self.file_manager.list_images()
            if not images:
                return {
                    'status': 'SUCCESS',
                    'message': 'No images to upload',
                    'data': {'uploaded_count': 0, 'errors': []}
                }
            
            print(f"[{self.hostname}] Uploading {len(images)} images to {bucket_name}")
            
            # Upload with hostname prefix
            success_count, errors = self.s3_uploader.upload_images(
                images, bucket_name, hostname_prefix=self.hostname
            )
            
            print(f"[{self.hostname}] Uploaded {success_count}/{len(images)} images")
            
            return {
                'status': 'SUCCESS',
                'message': f"Uploaded {success_count} images",
                'data': {
                    'uploaded_count': success_count,
                    'total_count': len(images),
                    'errors': errors
                }
            }
            
        except Exception as e:
            return {
                'status': 'ERROR',
                'message': str(e)
            }
    
    def delete_images(self) -> dict:
        """Delete all captured images."""
        try:
            deleted_count, errors = self.file_manager.delete_all_images()
            
            print(f"[{self.hostname}] Deleted {deleted_count} images")
            if errors:
                print(f"   WARNING: {len(errors)} errors occurred")
            
            return {
                'status': 'SUCCESS',
                'message': f"Deleted {deleted_count} images",
                'data': {
                    'deleted_count': deleted_count,
                    'errors': errors
                }
            }
            
        except Exception as e:
            return {
                'status': 'ERROR',
                'message': str(e)
            }
    
    def handle_command(self, conn: socket.socket, message) -> None:
        """Handle incoming command from server."""
        try:
            response_data = {'status': 'ERROR', 'message': 'Unknown command'}
            
            if message.type == MessageType.CAPTURE:
                timestamp = message.data.get('timestamp', datetime.now().strftime("%Y%m%d_%H%M%S"))
                response_data = self.capture_image(timestamp)
                
            elif message.type == MessageType.LIST_IMAGES:
                response_data = self.list_images()
                
            elif message.type == MessageType.UPLOAD_S3:
                bucket_name = message.data.get('bucket_name')
                if bucket_name:
                    response_data = self.upload_to_s3(bucket_name)
                else:
                    response_data = {'status': 'ERROR', 'message': 'No bucket name provided'}
                    
            elif message.type == MessageType.DELETE_IMAGES:
                response_data = self.delete_images()
            
            # Send response
            response = self.protocol.create_response_message(
                response_data['status'],
                response_data['message'],
                response_data.get('data', {})
            )
            self.protocol.send_message(conn, response)
            
        except Exception as e:
            print(f"ERROR [{self.hostname}] Error handling command: {e}")
            error_response = self.protocol.create_error_message(str(e))
            self.protocol.send_message(conn, error_response)
    
    def register_with_server(self) -> bool:
        """Register this client with the server."""
        try:
            sock = connect_with_retry(self.server_host, self.server_port, retries=3)
            if not sock:
                return False
            
            # Send registration message
            register_msg = self.protocol.create_register_message(
                self.hostname, 
                {'client_port': self.client_port}
            )
            
            if not self.protocol.send_message(sock, register_msg):
                sock.close()
                return False
            
            # Wait for acknowledgment
            response = self.protocol.receive_message(sock)
            sock.close()
            
            if response and response.type == MessageType.RESPONSE:
                if response.data.get('status') == 'SUCCESS':
                    print(f"[{self.hostname}] Registered with server")
                    return True
            
            print(f"ERROR [{self.hostname}] Registration failed")
            return False
            
        except Exception as e:
            print(f"ERROR [{self.hostname}] Registration error: {e}")
            return False
    
    def send_heartbeat(self):
        """Send periodic heartbeat to server."""
        while self.running:
            try:
                time.sleep(30)  # Send heartbeat every 30 seconds
                if not self.running:
                    break
                
                sock = connect_with_retry(self.server_host, self.server_port, retries=1, timeout=3)
                if sock:
                    heartbeat_msg = self.protocol.create_heartbeat_message()
                    heartbeat_msg.data['hostname'] = self.hostname
                    
                    self.protocol.send_message(sock, heartbeat_msg)
                    response = self.protocol.receive_message(sock)
                    sock.close()
                    
                    if response and response.data.get('status') == 'SUCCESS':
                        print(f"[{self.hostname}] Heartbeat sent")
                    
            except Exception as e:
                print(f"WARNING [{self.hostname}] Heartbeat failed: {e}")
    
    def start_command_server(self):
        """Start server to listen for commands from main server."""
        try:
            self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.client_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.client_socket.bind(("0.0.0.0", self.client_port))
            self.client_socket.listen(5)
            
            print(f"[{self.hostname}] Command server listening on port {self.client_port}")
            
            while self.running:
                try:
                    conn, addr = self.client_socket.accept()
                    print(f"[{self.hostname}] Command from {addr[0]}")
                    
                    # Receive and handle command
                    message = self.protocol.receive_message(conn)
                    if message:
                        self.handle_command(conn, message)
                    
                    conn.close()
                    
                except socket.error:
                    if self.running:
                        print(f"ERROR [{self.hostname}] Command server error")
                    break
                    
        except Exception as e:
            print(f"ERROR [{self.hostname}] Failed to start command server: {e}")
    
    def run(self):
        """Run the client."""
        self.running = True
        
        # Ensure image directory exists
        if not self.file_manager.ensure_directory_exists():
            print(f"ERROR [{self.hostname}] Failed to create image directory")
            return
        
        # Register with server
        print(f"[{self.hostname}] Registering with server...")
        for attempt in range(5):
            if self.register_with_server():
                break
            print(f"[{self.hostname}] Registration attempt {attempt + 1}/5 failed, retrying...")
            time.sleep(5)
        else:
            print(f"ERROR [{self.hostname}] Failed to register with server after 5 attempts")
            return
        
        try:
            # Start heartbeat thread
            self.heartbeat_thread = threading.Thread(target=self.send_heartbeat, daemon=True)
            self.heartbeat_thread.start()
            
            # Start command server (blocking)
            self.start_command_server()
            
        except KeyboardInterrupt:
            print(f"\n[{self.hostname}] Client interrupted by user")
        finally:
            self.shutdown()
    
    def shutdown(self):
        """Shutdown client gracefully."""
        print(f"[{self.hostname}] Shutting down...")
        self.running = False
        
        if self.client_socket:
            self.client_socket.close()
        
        print(f"[{self.hostname}] Client shutdown complete")


def signal_handler(signum, frame):
    """Handle system signals for graceful shutdown."""
    print(f"\nReceived signal {signum}")
    sys.exit(0)


def main():
    """Main entry point."""
    if len(sys.argv) < 2:
        print("Usage: python new_hq_camera_client.py <server_ip> [server_port] [client_port]")
        print("Example: python new_hq_camera_client.py 192.168.1.100")
        print("Example: python new_hq_camera_client.py 192.168.1.100 8888 8889")
        sys.exit(1)
    
    server_host = sys.argv[1]
    server_port = int(sys.argv[2]) if len(sys.argv) > 2 else 8888
    client_port = int(sys.argv[3]) if len(sys.argv) > 3 else 8889
    
    # Set up signal handlers
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    client = CameraClient(server_host, server_port, client_port)
    client.run()


if __name__ == "__main__":
    main()