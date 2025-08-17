#!/usr/bin/env python3
"""
File management module for camera system.
Handles image file operations like listing, counting, and deletion.
"""

import os
import glob
from typing import List, Dict, Tuple
from pathlib import Path


class ImageFileManager:
    """Manages image files for camera clients."""
    
    def __init__(self, base_dir: str = "."):
        self.base_dir = Path(base_dir)
        self.image_extensions = ['.jpg', '.jpeg', '.png', '.bmp', '.tiff']
    
    def list_images(self) -> List[str]:
        """List all image files in the directory."""
        images = []
        for ext in self.image_extensions:
            pattern = f"*{ext}"
            images.extend(glob.glob(str(self.base_dir / pattern)))
            # Also check uppercase extensions
            pattern = f"*{ext.upper()}"
            images.extend(glob.glob(str(self.base_dir / pattern)))
        
        # Return just filenames, not full paths
        return [os.path.basename(img) for img in images]
    
    def count_images(self) -> int:
        """Count total number of image files."""
        return len(self.list_images())
    
    def get_image_info(self) -> Dict[str, any]:
        """Get detailed information about images."""
        images = self.list_images()
        total_size = 0
        
        file_info = []
        for img in images:
            file_path = self.base_dir / img
            if file_path.exists():
                size = file_path.stat().st_size
                total_size += size
                file_info.append({
                    'name': img,
                    'size': size,
                    'size_mb': round(size / (1024 * 1024), 2)
                })
        
        return {
            'count': len(images),
            'total_size_bytes': total_size,
            'total_size_mb': round(total_size / (1024 * 1024), 2),
            'files': file_info
        }
    
    def delete_all_images(self) -> Tuple[int, List[str]]:
        """Delete all image files. Returns (count_deleted, errors)."""
        images = self.list_images()
        deleted_count = 0
        errors = []
        
        for img in images:
            try:
                file_path = self.base_dir / img
                if file_path.exists():
                    file_path.unlink()
                    deleted_count += 1
            except Exception as e:
                errors.append(f"Failed to delete {img}: {str(e)}")
        
        return deleted_count, errors
    
    def delete_specific_images(self, filenames: List[str]) -> Tuple[int, List[str]]:
        """Delete specific image files. Returns (count_deleted, errors)."""
        deleted_count = 0
        errors = []
        
        for filename in filenames:
            try:
                file_path = self.base_dir / filename
                if file_path.exists():
                    file_path.unlink()
                    deleted_count += 1
                else:
                    errors.append(f"File not found: {filename}")
            except Exception as e:
                errors.append(f"Failed to delete {filename}: {str(e)}")
        
        return deleted_count, errors
    
    def ensure_directory_exists(self) -> bool:
        """Ensure the base directory exists."""
        try:
            self.base_dir.mkdir(parents=True, exist_ok=True)
            return True
        except Exception as e:
            print(f"Failed to create directory {self.base_dir}: {e}")
            return False