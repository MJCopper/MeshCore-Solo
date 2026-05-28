#!/usr/bin/env python3
"""
Screenshot tool for Wio L1 Tracker Pro (OLED SH1106 and e-ink GxEPD2).
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

# display_type values (byte [5] in response frame)
DISPLAY_TYPE_OLED = 0   # page-based, column-major (SH1106/SSD1306)
DISPLAY_TYPE_EINK = 1   # row-major, MSB-first, 1=white/0=black (GxEPD2)

# No panel-specific constants needed — the firmware now sends GxEPD2's own reported
# dimensions (which use WIDTH_VISIBLE, not the full physical WIDTH), so the header
# already carries the correct visible height and width for any panel/rotation.


def parse_args():
    parser = argparse.ArgumentParser(
        description="Capture screenshots from Wio L1 Tracker Pro (OLED or e-ink)"
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
    """Receive screenshot data (may be split across multiple frames).

    Frame format: RESP_CODE_SCREENSHOT, width, height, chunk_idx,
                  total_chunks, display_type, [chunk_data...]
    """
    start_time = time.time()
    buffer_data = bytearray()
    width = None
    height = None
    display_type = None
    total_chunks = None
    received_chunks = 0

    while time.time() - start_time < timeout:
        frame = read_frame(ser)
        if frame is None:
            continue

        if len(frame) < 6:
            print(f"Error: Frame too short: {len(frame)} bytes")
            return None

        resp_code = frame[0]

        if resp_code == RESP_CODE_SCREENSHOT:
            w            = frame[1]
            h            = frame[2]
            chunk_idx    = frame[3]
            num_chunks   = frame[4]
            disp_type    = frame[5]
            chunk_data   = frame[6:]

            if chunk_idx == 0:
                width         = w
                height        = h
                display_type  = disp_type
                total_chunks  = num_chunks
                buffer_data   = bytearray()
                received_chunks = 0

            if width is None or width != w or height != h:
                print(f"Error: Inconsistent dimensions in chunk {chunk_idx}")
                return None

            if total_chunks is None or total_chunks != num_chunks:
                print(f"Error: Inconsistent chunk count in chunk {chunk_idx}")
                return None

            buffer_data.extend(chunk_data)
            received_chunks += 1

            if received_chunks >= total_chunks:
                return bytes(buffer_data), width, height, display_type

        elif resp_code == RESP_CODE_ERR:
            print("Error: Device returned error")
            return None

        else:
            continue

    print(
        f"Error: Timeout waiting for screenshot (received {received_chunks}/{total_chunks or '?'} chunks)"
    )
    return None


def oled_buffer_to_image(buffer, width, height):
    """Decode SH1106/SSD1306 framebuffer (page-based, column-major).
    Each byte covers 8 vertical pixels; bit 0 = top pixel of the byte.
    """
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
                        pixels[col, page + bit] = 1 if (byte_val & (1 << bit)) else 0
    return image.convert("RGB")


def eink_buffer_to_image(buffer, log_width, log_height):
    """Decode GxEPD2 framebuffer for any panel/rotation.

    The firmware sends GxEPD2's own width()/height() in the header (using WIDTH_VISIBLE,
    not the full physical WIDTH).  Rotation is inferred from the aspect ratio:
      portrait  (log_height > log_width)  → rotation 0: direct mapping
      landscape (log_width  > log_height) → rotation 1: phys_x = WIDTH_VISIBLE-1-ly
      square    (log_width == log_height) → assumed rotation 1

    Buffer layout (physical panel coordinates, rotation-independent):
      row-major, stride bytes per row, MSB-first
      byte_idx = phys_x // 8 + phys_y * stride
      bit      = 7 - phys_x % 8
      1 = white (paper), 0 = black (ink)

    Physical HEIGHT = max(log_width, log_height) for any rotation on a non-square panel.
    stride = buffer_size // HEIGHT  (no panel-specific constants needed).
    """
    if log_width == 0 or log_height == 0:
        return Image.new("RGB", (1, 1), (255, 255, 255))

    # Physical panel HEIGHT is always the longer dimension.
    phys_height = max(log_width, log_height)
    phys_stride = len(buffer) // phys_height    # bytes per physical row (e.g. 128/8 = 16)

    image  = Image.new("RGB", (log_width, log_height), (255, 255, 255))
    pixels = image.load()

    portrait = log_height > log_width           # rotation=0 vs rotation=1

    for ly in range(log_height):
        for lx in range(log_width):
            if portrait:
                # rotation=0: logical (lx,ly) maps directly to physical (lx,ly)
                phys_x = lx
                phys_y = ly
            else:
                # rotation=1: logical (lx,ly) → physical (WIDTH_VISIBLE-1-ly, lx)
                # WIDTH_VISIBLE = log_height (from screenshotHeight() in firmware)
                phys_x = log_height - 1 - ly
                phys_y = lx

            byte_idx = phys_x // 8 + phys_y * phys_stride
            bit      = 7 - phys_x % 8
            if byte_idx < len(buffer):
                raw   = (buffer[byte_idx] >> bit) & 1
                color = (255, 255, 255) if raw else (0, 0, 0)
                pixels[lx, ly] = color

    return image


def buffer_to_image(buffer, width, height, display_type, scale=1):
    """Convert framebuffer to PIL Image with a 3-pixel black frame."""
    if display_type == DISPLAY_TYPE_EINK:
        image_rgb = eink_buffer_to_image(buffer, width, height)
    else:
        image_rgb = oled_buffer_to_image(buffer, width, height)

    # Add 3-pixel black frame
    FRAME_WIDTH = 3
    fw = image_rgb.width  + FRAME_WIDTH * 2
    fh = image_rgb.height + FRAME_WIDTH * 2
    final_image = Image.new("RGB", (fw, fh), (0, 0, 0))
    final_image.paste(image_rgb, (FRAME_WIDTH, FRAME_WIDTH))

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
        print("Screenshot Tool for Wio L1 Tracker Pro (OLED + e-ink)")
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
                    buffer_data, width, height, display_type = result
                    type_str = "e-ink" if display_type == DISPLAY_TYPE_EINK else "OLED"
                    print(
                        f"Received framebuffer: {width}x{height} {type_str}, {len(buffer_data)} bytes"
                    )

                    try:
                        image = buffer_to_image(
                            buffer_data, width, height, display_type, scale=args.scale
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
