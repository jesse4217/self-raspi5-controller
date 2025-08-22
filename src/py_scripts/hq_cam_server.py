#!/usr/bin/env python3
"""
HQ Camera Server - Controls multiple Raspberry Pi camera clients.
Provides dynamic client discovery and menu-driven operations.
"""

import socket
import threading
import time
import os
from datetime import datetime
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
import sys

# Import our modules
from camera_protocol import CameraProtocol, MessageType, ProtocolConfig
from s3_uploader import get_default_s3_uploader


@dataclass
class ClientInfo:
    """Information about a connected client."""
    hostname: str
    address: str
    port: int
    last_seen: datetime
    status: str = "connected"
    
    def __str__(self):
        return f"{self.hostname} ({self.address}:{self.port}) - {self.status}"


class CameraServer:
    """HQ Camera server for managing multiple Pi clients."""
    
    def __init__(self, host: str = "0.0.0.0", port: int = 8888):
        self.host = host
        self.port = port
        self.clients: Dict[str, ClientInfo] = {}  # hostname -> ClientInfo
        self.client_addresses: Dict[str, Tuple[str, int]] = {}  # hostname -> (ip, port)
        self.protocol = CameraProtocol()
        self.running = False
        self.server_socket = None
        
        # Server discovery thread
        self.discovery_thread = None
        self.status_lock = threading.Lock()
        
        print(f"Camera Server initialized on {host}:{port}")
    
    def start_discovery_server(self):
        """Start the discovery server to accept client registrations."""
        try:
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.bind((self.host, self.port))
            self.server_socket.listen(10)
            self.running = True
            
            print(f"Discovery server listening on {self.host}:{self.port}")
            print("Waiting for client registrations...")
            
            while self.running:
                try:
                    conn, addr = self.server_socket.accept()
                    # Handle each client connection in a separate thread
                    client_thread = threading.Thread(
                        target=self.handle_client_connection,
                        args=(conn, addr),
                        daemon=True
                    )
                    client_thread.start()
                    
                except socket.error:
                    if self.running:
                        print("Server socket error occurred")
                    break
                    
        except Exception as e:
            print(f"Failed to start discovery server: {e}")
    
    def handle_client_connection(self, conn: socket.socket, addr: Tuple[str, int]):
        """Handle a client connection for registration or heartbeat."""
        try:
            # Set timeout for client communication
            conn.settimeout(self.protocol.config.TIMEOUT)
            
            # Receive registration message
            message = self.protocol.receive_message(conn)
            if not message:
                return
            
            if message.type == MessageType.REGISTER:
                hostname = message.data.get('hostname', f'unknown_{addr[0]}')
                client_port = message.data.get('client_port', 8889)
                
                with self.status_lock:
                    # Store client info
                    self.clients[hostname] = ClientInfo(
                        hostname=hostname,
                        address=addr[0],
                        port=client_port,
                        last_seen=datetime.now()
                    )
                    self.client_addresses[hostname] = (addr[0], client_port)
                
                # Send acknowledgment
                response = self.protocol.create_response_message("SUCCESS", "Registered successfully")
                self.protocol.send_message(conn, response)
                
                print(f"Client registered: {hostname} at {addr[0]}:{client_port}")
                
            elif message.type == MessageType.HEARTBEAT:
                hostname = message.data.get('hostname')
                if hostname and hostname in self.clients:
                    with self.status_lock:
                        self.clients[hostname].last_seen = datetime.now()
                        self.clients[hostname].status = "connected"
                    
                    # Send heartbeat response
                    response = self.protocol.create_response_message("SUCCESS", "Heartbeat received")
                    self.protocol.send_message(conn, response)
                    
        except Exception as e:
            print(f"Error handling client {addr}: {e}")
        finally:
            conn.close()
    
    def send_command_to_client(self, hostname: str, message) -> Optional[Dict]:
        """Send a command to a specific client and get response."""
        if hostname not in self.client_addresses:
            return {'status': 'ERROR', 'message': f'Client {hostname} not found'}
        
        ip, port = self.client_addresses[hostname]
        
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                sock.settimeout(self.protocol.config.TIMEOUT)
                sock.connect((ip, port))
                
                # Send command
                if not self.protocol.send_message(sock, message):
                    return {'status': 'ERROR', 'message': 'Failed to send command'}
                
                # Receive response
                response = self.protocol.receive_message(sock)
                if response:
                    return response.data
                else:
                    return {'status': 'ERROR', 'message': 'No response received'}
                    
        except Exception as e:
            with self.status_lock:
                if hostname in self.clients:
                    self.clients[hostname].status = "disconnected"
            return {'status': 'ERROR', 'message': str(e)}
    
    def broadcast_command(self, message) -> Dict[str, Dict]:
        """Send command to all clients and collect responses."""
        results = {}
        threads = []
        responses = {}
        
        def send_to_client(hostname):
            responses[hostname] = self.send_command_to_client(hostname, message)
        
        # Send to all clients in parallel
        for hostname in list(self.clients.keys()):
            thread = threading.Thread(target=send_to_client, args=(hostname,))
            threads.append(thread)
            thread.start()
        
        # Wait for all responses
        for thread in threads:
            thread.join()
        
        return responses
    
    def capture_images(self):
        """Command all clients to capture images."""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        capture_msg = self.protocol.create_capture_message(timestamp)
        
        print(f"\nBroadcasting capture command at {timestamp}")
        print("-" * 50)
        
        responses = self.broadcast_command(capture_msg)
        
        for hostname, response in responses.items():
            if response['status'] == 'SUCCESS':
                print(f"SUCCESS {hostname}: {response.get('message', 'Capture successful')}")
            else:
                print(f"ERROR {hostname}: {response.get('message', 'Capture failed')}")
        
        print("-" * 50)
    
    def list_client_images(self):
        """Get image list and count from all clients."""
        list_msg = self.protocol.create_list_images_message()
        
        print(f"\nListing images from all clients")
        print("-" * 60)
        
        responses = self.broadcast_command(list_msg)
        
        total_images = 0
        for hostname, response in responses.items():
            if response['status'] == 'SUCCESS':
                data = response.get('data', {})
                count = data.get('count', 0)
                total_size_mb = data.get('total_size_mb', 0)
                files = data.get('files', [])
                
                print(f"Client {hostname}:")
                print(f"   Images: {count} files ({total_size_mb:.2f} MB)")
                
                if files:
                    print("   Files:")
                    for file_info in files[:5]:  # Show first 5 files
                        print(f"     - {file_info['name']} ({file_info['size_mb']:.2f} MB)")
                    if len(files) > 5:
                        print(f"     ... and {len(files) - 5} more files")
                
                total_images += count
            else:
                print(f"ERROR {hostname}: {response.get('message', 'Failed to list images')}")
            print()
        
        print(f"Total images across all clients: {total_images}")
        print("-" * 60)
    
    def upload_to_s3(self):
        """Command all clients to upload images to S3."""
        # Create bucket name with timestamp
        timestamp = datetime.now().strftime("%Y-%m%d-%H%M")
        bucket_name = f"camera-captures-{timestamp}"
        
        upload_msg = self.protocol.create_upload_s3_message(bucket_name)
        
        print(f"\nUploading images to S3 bucket: {bucket_name}")
        print("-" * 60)
        
        responses = self.broadcast_command(upload_msg)
        
        total_uploaded = 0
        for hostname, response in responses.items():
            if response['status'] == 'SUCCESS':
                data = response.get('data', {})
                uploaded = data.get('uploaded_count', 0)
                total_uploaded += uploaded
                print(f"SUCCESS {hostname}: Uploaded {uploaded} images")
                
                errors = data.get('errors', [])
                if errors:
                    print(f"   WARNING: {len(errors)} errors")
                    for error in errors[:3]:  # Show first 3 errors
                        print(f"      - {error}")
            else:
                print(f"ERROR {hostname}: {response.get('message', 'Upload failed')}")
        
        print(f"\nTotal images uploaded: {total_uploaded}")
        print("-" * 60)
    
    def delete_client_images(self):
        """Command all clients to delete their images after confirmation."""
        # Get confirmation
        confirmation = input("\nWARNING: Are you sure you want to delete ALL images from ALL clients? (Y/n): ")
        if confirmation.lower() not in ['y', 'yes', '']:
            print("Delete operation cancelled.")
            return
        
        delete_msg = self.protocol.create_delete_images_message()
        
        print(f"\nDeleting all images from clients")
        print("-" * 50)
        
        responses = self.broadcast_command(delete_msg)
        
        total_deleted = 0
        for hostname, response in responses.items():
            if response['status'] == 'SUCCESS':
                data = response.get('data', {})
                deleted = data.get('deleted_count', 0)
                total_deleted += deleted
                print(f"SUCCESS {hostname}: Deleted {deleted} images")
                
                errors = data.get('errors', [])
                if errors:
                    print(f"   WARNING: {len(errors)} errors")
            else:
                print(f"ERROR {hostname}: {response.get('message', 'Delete failed')}")
        
        print(f"\nTotal images deleted: {total_deleted}")
        print("-" * 50)
    
    def show_client_status(self):
        """Display current client status."""
        with self.status_lock:
            if not self.clients:
                print("\nNo clients connected")
                return
            
            print(f"\nConnected Clients ({len(self.clients)}):")
            print("-" * 60)
            
            current_time = datetime.now()
            for hostname, client in self.clients.items():
                time_diff = (current_time - client.last_seen).total_seconds()
                
                # Update status based on last seen time
                if time_diff > 60:  # More than 1 minute
                    client.status = "disconnected"
                elif time_diff > 30:  # More than 30 seconds
                    client.status = "timeout"
                else:
                    client.status = "connected"
                
                status_prefix = {
                    "connected": "[ONLINE]",
                    "timeout": "[TIMEOUT]", 
                    "disconnected": "[OFFLINE]"
                }.get(client.status, "[UNKNOWN]")
                
                print(f"{status_prefix} {client.hostname}")
                print(f"   Address: {client.address}:{client.port}")
                print(f"   Status: {client.status}")
                print(f"   Last seen: {time_diff:.0f}s ago")
                print()
    
    def show_menu(self):
        """Display the command menu."""
        print("\n" + "="*60)
        print("HQ Camera Server Control Panel")
        print("="*60)
        print("Commands:")
        print("  [1] Capture images on all clients")
        print("  [2] List images from all clients") 
        print("  [9] Upload images to S3")
        print("  [0] Delete all images (with confirmation)")
        print("  [s] Show client status")
        print("  [h] Show this menu")
        print("  [q] Quit server")
        print("-"*60)
    
    def run(self):
        """Run the server main loop."""
        # Start discovery server in background thread
        self.discovery_thread = threading.Thread(target=self.start_discovery_server, daemon=True)
        self.discovery_thread.start()
        
        # Show initial menu and status
        self.show_menu()
        self.show_client_status()
        
        try:
            while True:
                command = input("\nEnter command: ").strip().lower()
                
                if command == '1':
                    self.capture_images()
                elif command == '2':
                    self.list_client_images()
                elif command == '9':
                    self.upload_to_s3()
                elif command == '0':
                    self.delete_client_images()
                elif command == 's':
                    self.show_client_status()
                elif command == 'h':
                    self.show_menu()
                elif command in ['q', 'quit']:
                    print("\nShutting down server...")
                    break
                else:
                    print("Unknown command. Press 'h' for help.")
        
        except KeyboardInterrupt:
            print("\n\nServer interrupted by user")
        finally:
            self.shutdown()
    
    def shutdown(self):
        """Shutdown the server gracefully."""
        self.running = False
        if self.server_socket:
            self.server_socket.close()
        print("Server shutdown complete")


def main():
    """Main entry point."""
    # Check for custom port
    port = 8888
    if len(sys.argv) > 1:
        try:
            port = int(sys.argv[1])
        except ValueError:
            print("Invalid port number. Using default 8888.")
    
    server = CameraServer(port=port)
    server.run()


if __name__ == "__main__":
    main()