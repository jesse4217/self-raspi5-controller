#!/usr/bin/env python3
"""
Camera protocol module for server-client communication.
Handles message formatting, parsing, and protocol constants.
"""

import json
import socket
from typing import Dict, Any, Optional, Tuple
from dataclasses import dataclass
from enum import Enum


class MessageType(Enum):
    """Message types for camera protocol."""
    REGISTER = "REGISTER"
    CAPTURE = "CAPTURE"
    LIST_IMAGES = "LIST_IMAGES"
    UPLOAD_S3 = "UPLOAD_S3"
    DELETE_IMAGES = "DELETE_IMAGES"
    RESPONSE = "RESPONSE"
    ERROR = "ERROR"
    HEARTBEAT = "HEARTBEAT"


@dataclass
class ProtocolConfig:
    """Protocol configuration constants."""
    BUFFER_SIZE: int = 4096
    TIMEOUT: int = 10
    HEARTBEAT_INTERVAL: int = 30
    MAX_RETRIES: int = 3


class CameraMessage:
    """Camera protocol message handler."""
    
    def __init__(self, msg_type: MessageType, data: Dict[str, Any] = None):
        self.type = msg_type
        self.data = data or {}
        self.timestamp = None
    
    def to_json(self) -> str:
        """Convert message to JSON string."""
        return json.dumps({
            'type': self.type.value,
            'data': self.data,
            'timestamp': self.timestamp
        })
    
    @classmethod
    def from_json(cls, json_str: str) -> 'CameraMessage':
        """Create message from JSON string."""
        try:
            parsed = json.loads(json_str)
            msg_type = MessageType(parsed['type'])
            msg = cls(msg_type, parsed.get('data', {}))
            msg.timestamp = parsed.get('timestamp')
            return msg
        except (json.JSONDecodeError, KeyError, ValueError) as e:
            raise ValueError(f"Invalid message format: {e}")


class CameraProtocol:
    """Camera protocol communication handler."""
    
    def __init__(self, config: ProtocolConfig = None):
        self.config = config or ProtocolConfig()
    
    def send_message(self, sock: socket.socket, message: CameraMessage) -> bool:
        """Send a message through socket."""
        try:
            json_data = message.to_json()
            # Send length first, then data
            length = len(json_data.encode())
            sock.sendall(length.to_bytes(4, byteorder='big'))
            sock.sendall(json_data.encode())
            return True
        except Exception as e:
            print(f"Failed to send message: {e}")
            return False
    
    def receive_message(self, sock: socket.socket) -> Optional[CameraMessage]:
        """Receive a message from socket."""
        try:
            # Receive length first
            length_bytes = self._receive_exact(sock, 4)
            if not length_bytes:
                return None
            
            length = int.from_bytes(length_bytes, byteorder='big')
            if length > self.config.BUFFER_SIZE:
                raise ValueError(f"Message too large: {length} bytes")
            
            # Receive actual message
            json_bytes = self._receive_exact(sock, length)
            if not json_bytes:
                return None
            
            json_str = json_bytes.decode()
            return CameraMessage.from_json(json_str)
        
        except Exception as e:
            print(f"Failed to receive message: {e}")
            return None
    
    def _receive_exact(self, sock: socket.socket, size: int) -> Optional[bytes]:
        """Receive exactly 'size' bytes from socket."""
        data = b''
        while len(data) < size:
            chunk = sock.recv(size - len(data))
            if not chunk:
                return None
            data += chunk
        return data
    
    def create_register_message(self, hostname: str, client_info: Dict[str, Any] = None) -> CameraMessage:
        """Create client registration message."""
        data = {
            'hostname': hostname,
            'info': client_info or {}
        }
        return CameraMessage(MessageType.REGISTER, data)
    
    def create_capture_message(self, timestamp: str) -> CameraMessage:
        """Create capture command message."""
        return CameraMessage(MessageType.CAPTURE, {'timestamp': timestamp})
    
    def create_list_images_message(self) -> CameraMessage:
        """Create list images command message."""
        return CameraMessage(MessageType.LIST_IMAGES)
    
    def create_upload_s3_message(self, bucket_name: str) -> CameraMessage:
        """Create S3 upload command message."""
        return CameraMessage(MessageType.UPLOAD_S3, {'bucket_name': bucket_name})
    
    def create_delete_images_message(self) -> CameraMessage:
        """Create delete images command message."""
        return CameraMessage(MessageType.DELETE_IMAGES)
    
    def create_response_message(self, status: str, message: str = "", data: Dict[str, Any] = None) -> CameraMessage:
        """Create response message."""
        response_data = {
            'status': status,
            'message': message,
            'data': data or {}
        }
        return CameraMessage(MessageType.RESPONSE, response_data)
    
    def create_error_message(self, error: str) -> CameraMessage:
        """Create error message."""
        return CameraMessage(MessageType.ERROR, {'error': error})
    
    def create_heartbeat_message(self) -> CameraMessage:
        """Create heartbeat message."""
        return CameraMessage(MessageType.HEARTBEAT)


def connect_with_retry(host: str, port: int, retries: int = 3, timeout: int = 5) -> Optional[socket.socket]:
    """Connect to server with retry logic."""
    for attempt in range(retries):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(timeout)
            sock.connect((host, port))
            return sock
        except Exception as e:
            print(f"Connection attempt {attempt + 1} failed: {e}")
            if sock:
                sock.close()
            if attempt < retries - 1:
                import time
                time.sleep(1)
    return None