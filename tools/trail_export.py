#!/usr/bin/env python3
"""
Capture a GPX track from a Wio L1 Tracker (Solo firmware) over USB serial.

Usage:
    uv run tools/trail_export.py [--port PORT] [--out OUT]

The device prints raw GPX 1.1 XML to the USB-CDC serial port when the user
selects Tools › Trail › Hold Enter › Export (live) or Export (saved).
This script waits for the `<?xml` start, captures bytes until `</gpx>`,
and writes them to a timestamped file in `tools/gpx/`.

> Tip: disconnect the companion app first if you are using it over USB —
> the raw GPX stream will otherwise look like garbage frames to the app.
> If the app is connected over BLE the export is safe; USB receive is
> ignored while BLE is connected.
"""

import argparse
import os
import sys
import time
from datetime import datetime

import serial
import serial.tools.list_ports

GPX_START = b"<?xml"
GPX_END   = b"</gpx>"
DEFAULT_TIMEOUT_S = 30


def parse_args():
    parser = argparse.ArgumentParser(
        description="Capture a GPX trail dump from the Wio Tracker L1 over USB serial"
    )
    parser.add_argument("--port", "-p", help="Serial port (default: auto-detect)")
    parser.add_argument(
        "--out", "-o",
        help="Output file path (default: tools/gpx/trail_<timestamp>.gpx)",
    )
    parser.add_argument(
        "--timeout", "-t", type=int, default=DEFAULT_TIMEOUT_S,
        help=f"Seconds to wait for the GPX header before giving up (default: {DEFAULT_TIMEOUT_S})",
    )
    return parser.parse_args()


def find_wio_port():
    """Look for a Wio/Seeed CDC interface; fall back to a generic ACM/COM port."""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        desc = str(port.description).lower()
        if "wio" in desc or "seeed" in desc:
            return port.device
    for port in ports:
        if "ACM" in port.device or port.device.startswith("COM"):
            return port.device
    return None


def default_out_path():
    os.makedirs("tools/gpx", exist_ok=True)
    stamp = datetime.now().strftime("trail_%Y-%m-%d_%H-%M-%S.gpx")
    return os.path.join("tools/gpx", stamp)


def wait_for_header(ser, timeout_s):
    """Drain bytes until we see the GPX start marker. Returns the buffered
    chunk that *includes* `<?xml` (or None on timeout)."""
    deadline = time.time() + timeout_s
    window = bytearray()
    while time.time() < deadline:
        chunk = ser.read(256)
        if chunk:
            window += chunk
            idx = window.find(GPX_START)
            if idx >= 0:
                return bytes(window[idx:])
            # Keep only the last len(GPX_START) - 1 bytes — anything earlier
            # cannot be the start of a marker we haven't matched yet.
            if len(window) > len(GPX_START):
                window = bytearray(window[-(len(GPX_START)):])
        else:
            # short idle — keep waiting
            time.sleep(0.01)
    return None


def stream_until_footer(ser, initial, out_file):
    """Write bytes to out_file until we see `</gpx>`. Returns total written."""
    total = 0
    out_file.write(initial)
    total += len(initial)
    print(f"Receiving... ({total} B)", end="", flush=True)

    # Sliding tail keeps us from missing a marker split across reads.
    tail_keep = len(GPX_END)
    tail = bytearray(initial[-tail_keep:])
    if GPX_END in tail:
        idx = bytes(tail).find(GPX_END)
        # already complete (unlikely on first chunk, but be safe)
        return total

    last_report = total
    while True:
        chunk = ser.read(256)
        if not chunk:
            # Device went quiet — bail rather than hang forever
            if total > last_report:
                print(f"\rReceived {total} B (device idle)        ")
            else:
                print("\nDevice went quiet before </gpx> — partial file kept.")
            return total
        out_file.write(chunk)
        total += len(chunk)
        tail += chunk
        if len(tail) > tail_keep * 4:
            tail = bytearray(tail[-tail_keep * 4:])
        if GPX_END in tail:
            return total
        if total - last_report >= 1024:
            print(f"\rReceiving... ({total} B)", end="", flush=True)
            last_report = total


def main():
    args = parse_args()

    port = args.port or find_wio_port()
    if not port:
        print("Error: no serial port found.")
        print("\nAvailable ports:")
        for p in serial.tools.list_ports.comports():
            print(f"  {p.device}: {p.description}")
        port = input("\nEnter serial port manually: ").strip()
        if not port:
            sys.exit(1)

    out_path = args.out or default_out_path()

    print(f"Connecting to {port} at 115200 baud...")
    try:
        ser = serial.Serial(port, 115200, timeout=0.2)
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
        sys.exit(1)

    print("Connected.")
    print()
    print("On the device: Tools › Trail › Hold Enter ›")
    print("    Export (live)   — current RAM trail")
    print("    Export (saved)  — /trail file from flash")
    print()
    print(f"Waiting for GPX stream (up to {args.timeout}s)...")

    initial = wait_for_header(ser, args.timeout)
    if initial is None:
        print(f"Timed out after {args.timeout}s — no <?xml seen.")
        ser.close()
        sys.exit(2)

    with open(out_path, "wb") as f:
        total = stream_until_footer(ser, initial, f)

    ser.close()
    print(f"\nSaved {total} bytes to {out_path}")


if __name__ == "__main__":
    main()
