#!/usr/bin/env python3

import subprocess
import time
from datetime import datetime
from typing import List


def capture_image(filename: str) -> None:
    """
    Capture an image using libcamera-still command with best quality settings.
    
    Args:
        filename: The output filename for the captured image
    """
    # Build command - using best quality settings
    cmd: List[str] = [
        "libcamera-still",
        "-n",              # No preview
        "-t", "1",         # Capture immediately (1ms delay)
        "--width", "4056",
        "--height", "3040",
        "-e", "png",       # PNG format for best quality
        "-o", filename,
        "--immediate"      # Skip AWB/AE convergence for faster capture
    ]
    
    # Execute command
    print(f"execute cmd: {' '.join(cmd)}")
    subprocess.run(cmd, check=True)


def calculate_time() -> None:
    """Calculate execution time for image capture."""
    # Generate timestamp for filename
    timestamp: str = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename: str = f"{timestamp}.png"
    
    # Record start time
    start_time: float = time.time()
    start_datetime: str = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
    print(f"start_time: {start_datetime}")

    # Capture the image
    capture_image(filename)

    # Record end time
    end_time: float = time.time()
    end_datetime: str = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
    print(f"end_time: {end_datetime}")

    # Calculate time range
    time_range: float = end_time - start_time
    print(f"time_range: {time_range:.3f} seconds")
    print(f"Image saved as: {filename}")


if __name__ == "__main__":
    calculate_time()