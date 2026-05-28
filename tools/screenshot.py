#!/usr/bin/env python3
"""
Screenshot tool for Wio L1 Tracker Pro with SH1106 display.
Connects via USB serial using mesh companion protocol, captures framebuffer, saves as PNG.

Usage:
    python screenshot.py [--port PORT] [--scale SCALE]

Options:
    --port PORT    Serial port to use (default: auto-detect)
    --scale SCALE  Upscale factor: 1=no upscale, 2=2x, 3=3x, 4=4x, etc. (default: 1)
"""

import argparse
import serial
import serial.tools.list_ports
from datetime import datetime
from PIL import Image, ImageDraw
import sys
import time
import os

# Mesh protocol constants
FRAME_START_OUT = 0x3C  # '<' - used when sending TO device
FRAME_START_IN = 0x3E  # '>' - used when receiving FROM device
CMD_GET_SCREENSHOT = 66
RESP_CODE_SCREENSHOT = 29
RESP_CODE_ERR = 1
DISPLAY_WIDTH = 128
DISPLAY_HEIGHT = 64
BUFFER_SIZE = (DISPLAY_WIDTH * DISPLAY_HEIGHT) // 8  # 1024 bytes


def parse_args():
    parser = argparse.ArgumentParser(
        description="Capture screenshots from Wio L1 Tracker Pro with SH1106 display"
    )
    parser.add_argument(
        "--port", "-p", help="Serial port to use (default: auto-detect)"
    )
    parser.add_argument(
        "--scale",
        "-s",
        type=int,
        default=1,
        help="Upscale factor: 1=no upscale, 2=2x, 3=3x, 4=4x, etc. (default: 1)",
    )
    return parser.parse_args()


def find_wio_port():
    """Find Wio Tracker L1 serial port"""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        desc = str(port.description).lower()
        if "wio" in desc or "seeed" in desc:
            return port.device

    # Fallback: check common port names
    for port in ports:
        if "ACM" in port.device or port.device.startswith("COM"):
            return port.device
    return None


def read_frame(ser):
    """Read a complete frame from the mesh protocol (from device)"""
    header = ser.read(3)
    if len(header) != 3:
        return None

    if header[0] != FRAME_START_IN:
        print(
            f"Error: Invalid frame start byte: {header[0]:02x} (expected {FRAME_START_IN:02x})"
        )
        return None

    frame_len = header[1] | (header[2] << 8)

    if frame_len > 0:
        data = bytearray()
        while len(data) < frame_len:
            chunk = ser.read(frame_len - len(data))
            if not chunk:
                print(f"Error: Timeout waiting for frame data")
                return None
            data.extend(chunk)
        return bytes(data)
    else:
        return b""


def send_command(ser, command, data=b""):
    """Send a command frame using mesh protocol (to device)"""
    frame_len = len(data) + 1
    header = bytes([FRAME_START_OUT, frame_len & 0xFF, (frame_len >> 8) & 0xFF])
    frame = header + bytes([command]) + data
    ser.write(frame)
    ser.flush()
    return frame_len + 3


def receive_screenshot(ser, timeout=5):
    """Receive screenshot data (may be split across multiple frames)"""
    start_time = time.time()
    buffer_data = bytearray()
    width = None
    height = None
    total_chunks = None
    received_chunks = 0

    while time.time() - start_time < timeout:
        frame = read_frame(ser)
        if frame is None:
            continue
        if len(frame) < 1:
            continue

        resp_code = frame[0]

        if resp_code == RESP_CODE_ERR:
            err = frame[1] if len(frame) > 1 else 0xFF
            print(f"Error: Device returned error code 0x{err:02x}")
            return None

        if resp_code != RESP_CODE_SCREENSHOT:
            continue

        if len(frame) < 5:
            print(f"Error: Screenshot frame too short: {len(frame)} bytes (need >= 5)")
            return None

        w = frame[1]
        h = frame[2]
        chunk_idx = frame[3]
        num_chunks = frame[4]
        chunk_data = frame[5:]

        if chunk_idx == 0:
            width = w
            height = h
            total_chunks = num_chunks
            buffer_data = bytearray()
            received_chunks = 0

        if width is None:
            print(f"Error: Missed chunk 0 (first chunk seen: {chunk_idx})")
            return None
        if width != w or height != h:
            print(f"Error: Inconsistent dimensions in chunk {chunk_idx}")
            return None
        if total_chunks != num_chunks:
            print(f"Error: Inconsistent chunk count in chunk {chunk_idx}")
            return None

        buffer_data.extend(chunk_data)
        received_chunks += 1

        if received_chunks >= total_chunks:
            expected_size = (width * height) // 8
            if len(buffer_data) != expected_size:
                print(f"Error: Expected {expected_size} bytes, got {len(buffer_data)}")
                return None
            return bytes(buffer_data), width, height

    print(
        f"Error: Timeout waiting for screenshot (received {received_chunks}/{total_chunks or '?'} chunks)"
    )
    return None


def buffer_to_image(buffer, width, height, scale=1):
    """Convert SH1106/SH1106 framebuffer to PIL Image with white text on black background and black frame"""
    image = Image.new("1", (width, height), 0)
    pixels = image.load()

    bytes_per_page = width

    for page in range(0, height, 8):
        page_start = (page // 8) * bytes_per_page
        for col in range(width):
            byte_idx = page_start + col
            if byte_idx < len(buffer):
                byte_val = buffer[byte_idx]
                for bit in range(8):
                    if page + bit < height:
                        pixel_value = 1 if (byte_val & (1 << bit)) else 0
                        pixels[col, page + bit] = pixel_value

    # Convert to RGB: white text on black background (no inversion)
    image_rgb = image.convert("RGB")

    # Add 3-pixel black frame around the image
    FRAME_WIDTH = 3
    new_width = width + FRAME_WIDTH * 2
    new_height = height + FRAME_WIDTH * 2
    final_image = Image.new("RGB", (new_width, new_height), (0, 0, 0))

    # Paste the display content in the center
    final_image.paste(image_rgb, (FRAME_WIDTH, FRAME_WIDTH))

    # Upscale if requested
    if scale > 1:
        final_image = final_image.resize(
            (final_image.width * scale, final_image.height * scale),
            Image.Resampling.NEAREST,
        )

    return final_image


def generate_filename():
    """Generate timestamp filename in local time with seconds"""
    now = datetime.now()
    os.makedirs("tools/pngs", exist_ok=True)
    return os.path.join("tools/pngs", now.strftime("screenshot_%Y-%m-%d_%H-%M-%S.png"))


def main():
    args = parse_args()

    port = args.port
    if not port:
        port = find_wio_port()

    if not port:
        print("Error: Could not find Wio Tracker L1 serial port")
        print("\nAvailable ports:")
        for p in serial.tools.list_ports.comports():
            print(f"  {p.device}: {p.description}")

        port = input("\nEnter serial port manually: ")

    print(f"\nConnecting to {port} at 115200 baud (scale: {args.scale}x)...")

    try:
        ser = serial.Serial(port, 115200, timeout=3)
        print("Connected!\n")
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
        sys.exit(1)

    try:
        print("Screenshot Tool for Wio L1 Tracker Pro")
        print("Press 'S' to capture screenshot, 'Q' to quit\n")

        while True:
            try:
                key = input("> ").strip().upper()
            except (KeyboardInterrupt, EOFError):
                print("\nExiting...")
                break

            if key == "Q":
                print("Exiting...")
                break

            if key == "S":
                print("Requesting screenshot...")

                ser.reset_input_buffer()
                send_command(ser, CMD_GET_SCREENSHOT)
                time.sleep(0.1)

                result = receive_screenshot(ser)
                if result:
                    buffer_data, width, height = result
                    print(
                        f"Received framebuffer: {width}x{height}, {len(buffer_data)} bytes"
                    )

                    try:
                        image = buffer_to_image(
                            buffer_data, width, height, scale=args.scale
                        )
                        filename = generate_filename()
                        image.save(filename)
                        print(f"Saved as {filename} ({image.width}x{image.height})")
                    except Exception as e:
                        print(f"Error saving image: {e}")
                else:
                    print("Error: Failed to receive screenshot")
            else:
                print("Unknown command. Press 'S' for screenshot, 'Q' to quit")

    except KeyboardInterrupt:
        print("\nExiting...")

    finally:
        ser.close()
        print("Connection closed.")


if __name__ == "__main__":
    main()
