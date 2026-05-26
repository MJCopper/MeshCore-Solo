#### Display Screenshot Tool

> **⚠️ Note:** The screenshot feature requires the `ENABLE_SCREENSHOT` build flag to be enabled. Add `-D ENABLE_SCREENSHOT` to your PlatformIO build flags, then recompile and flash the firmware.

The firmware supports capturing the current display contents and transmitting
it over USB serial. This is useful for debugging, remote monitoring, or
creating documentation.

**Important:** When the device is connected to the companion app, the USB
serial port is not available for communication. To use the screenshot tool,
ensure the device is not connected to the companion app.

**Usage:**

1. Build and flash firmware with `-D ENABLE_SCREENSHOT` build flag enabled

Example for the OLED dual firmware:

```
PLATFORMIO_BUILD_FLAGS="-D ENABLE_SCREENSHOT" pio run -e WioTrackerL1_companion_dual -t upload
```

2. Connect the device to your computer via USB (ensure no companion app connection)
3. Install dependencies and run the screenshot tool. To manage python and python dependencies, use uv: https://docs.astral.sh/uv/

   ```sh
   cd tools
   uv run tools/screenshot.py
   ```

   Options:
   - `--port PORT` — Serial port to use (default: auto-detect)
   - `--scale SCALE` — Upscale factor for the output image (1=no upscale, 2=2x, 3=3x, 4=4x, etc.; default: 1)

4. In the tool's interactive menu, press **S** to capture a screenshot
5. The tool will save the screenshot as a PNG file in `tools/pngs/` with a timestamp-based filename

**How it works:**
- The tool sends the `CMD_GET_SCREENSHOT` command (66) to the device
- The device responds with `RESP_CODE_SCREENSHOT` (29) containing the framebuffer data
- The framebuffer is transmitted in chunks (128×64 display = 1024 bytes, split across multiple frames)
- The tool reassembles the chunks and converts them to a PNG image
