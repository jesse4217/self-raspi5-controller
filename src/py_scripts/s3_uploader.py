#!/usr/bin/env python3
"""
S3 upload module for camera system.
Handles uploading images to AWS S3 buckets.
"""

import os
import boto3
from typing import List, Dict, Tuple, Optional
from pathlib import Path
from botocore.exceptions import ClientError, NoCredentialsError


class S3Uploader:
    """Handles S3 upload operations for image files."""
    
    def __init__(self, aws_access_key: str = None, aws_secret_key: str = None, region: str = 'us-east-1'):
        """
        Initialize S3 uploader.
        If keys are None, will use AWS credentials from environment or AWS config.
        """
        try:
            if aws_access_key and aws_secret_key:
                self.s3_client = boto3.client(
                    's3',
                    aws_access_key_id=aws_access_key,
                    aws_secret_access_key=aws_secret_key,
                    region_name=region
                )
            else:
                # Use default credential chain (env vars, ~/.aws/credentials, IAM roles, etc.)
                self.s3_client = boto3.client('s3', region_name=region)
            
            self.region = region
            self.credentials_available = True
            
        except NoCredentialsError:
            print("Warning: No AWS credentials found. S3 operations will fail.")
            self.s3_client = None
            self.credentials_available = False
    
    def check_credentials(self) -> bool:
        """Check if AWS credentials are available and valid."""
        if not self.credentials_available:
            return False
        
        try:
            # Try to list buckets to verify credentials
            self.s3_client.list_buckets()
            return True
        except Exception as e:
            print(f"AWS credentials check failed: {e}")
            return False
    
    def create_bucket(self, bucket_name: str) -> bool:
        """Create S3 bucket if it doesn't exist."""
        if not self.credentials_available:
            print("Error: No AWS credentials available")
            return False
        
        try:
            # Check if bucket already exists
            try:
                self.s3_client.head_bucket(Bucket=bucket_name)
                print(f"Bucket {bucket_name} already exists")
                return True
            except ClientError as e:
                error_code = e.response['Error']['Code']
                if error_code != '404':
                    print(f"Error checking bucket: {e}")
                    return False
            
            # Create bucket
            if self.region == 'us-east-1':
                # us-east-1 doesn't need LocationConstraint
                self.s3_client.create_bucket(Bucket=bucket_name)
            else:
                self.s3_client.create_bucket(
                    Bucket=bucket_name,
                    CreateBucketConfiguration={'LocationConstraint': self.region}
                )
            
            print(f"Created bucket: {bucket_name}")
            return True
            
        except ClientError as e:
            error_code = e.response['Error']['Code']
            if error_code == 'BucketAlreadyOwnedByYou':
                print(f"Bucket {bucket_name} already owned by you")
                return True
            else:
                print(f"Failed to create bucket {bucket_name}: {e}")
                return False
        except Exception as e:
            print(f"Unexpected error creating bucket: {e}")
            return False
    
    def upload_file(self, local_file_path: str, bucket_name: str, s3_key: str = None) -> bool:
        """Upload a single file to S3."""
        if not self.credentials_available:
            print("Error: No AWS credentials available")
            return False
        
        try:
            file_path = Path(local_file_path)
            if not file_path.exists():
                print(f"File not found: {local_file_path}")
                return False
            
            # Use filename as S3 key if not provided
            if s3_key is None:
                s3_key = file_path.name
            
            self.s3_client.upload_file(str(file_path), bucket_name, s3_key)
            print(f"Uploaded {file_path.name} to s3://{bucket_name}/{s3_key}")
            return True
            
        except ClientError as e:
            print(f"Failed to upload {local_file_path}: {e}")
            return False
        except Exception as e:
            print(f"Unexpected error uploading file: {e}")
            return False
    
    def upload_images(self, image_files: List[str], bucket_name: str, 
                     hostname_prefix: str = None, base_dir: str = ".") -> Tuple[int, List[str]]:
        """
        Upload multiple image files to S3.
        Returns (success_count, error_list).
        """
        if not self.credentials_available:
            return 0, ["No AWS credentials available"]
        
        # Ensure bucket exists
        if not self.create_bucket(bucket_name):
            return 0, [f"Failed to create/access bucket {bucket_name}"]
        
        success_count = 0
        errors = []
        base_path = Path(base_dir)
        
        for image_file in image_files:
            try:
                local_path = base_path / image_file
                
                # Create S3 key with hostname prefix if provided
                if hostname_prefix:
                    s3_key = f"{hostname_prefix}/{image_file}"
                else:
                    s3_key = image_file
                
                if self.upload_file(str(local_path), bucket_name, s3_key):
                    success_count += 1
                else:
                    errors.append(f"Failed to upload {image_file}")
                    
            except Exception as e:
                errors.append(f"Error uploading {image_file}: {str(e)}")
        
        return success_count, errors
    
    def list_bucket_contents(self, bucket_name: str, prefix: str = None) -> Optional[List[str]]:
        """List contents of S3 bucket."""
        if not self.credentials_available:
            return None
        
        try:
            kwargs = {'Bucket': bucket_name}
            if prefix:
                kwargs['Prefix'] = prefix
            
            response = self.s3_client.list_objects_v2(**kwargs)
            
            if 'Contents' in response:
                return [obj['Key'] for obj in response['Contents']]
            else:
                return []
                
        except ClientError as e:
            print(f"Failed to list bucket contents: {e}")
            return None
        except Exception as e:
            print(f"Unexpected error listing bucket: {e}")
            return None


def get_default_s3_uploader() -> S3Uploader:
    """Get S3 uploader with default settings (uses environment credentials)."""
    return S3Uploader()