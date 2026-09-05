# Display screenshot tool

> **⚠️ Note:** The screenshot feature requires the `ENABLE_SCREENSHOT` build flag to be enabled. Add `-D ENABLE_SCREENSHOT` to your PlatformIO build flags, then recompile and flash the firmware.

The firmware supports capturing the current display contents and transmitting
it over USB serial. This is useful for debugging, remote monitoring, or
creating documentation.

When the device is connected to the companion app, the USB
serial port is not available for communication. To use the screenshot tool,
ensure the device is not connected to the companion app.

## Usage

1. Build and flash firmware with `-D ENABLE_SCREENSHOT` build flag enabled

Example for the OLED dual firmware:

```
PLATFORMIO_BUILD_FLAGS="-D ENABLE_SCREENSHOT" pio run -e WioTrackerL1_companion_dual -t upload
```

2. Connect the device to your computer via USB (ensure no companion app connection)
3. Install dependencies and run the tool with [uv](https://docs.astral.sh/uv/):

   ```sh
   cd tools
   uv run tools/screenshot.py
   ```

   Options are `--port PORT` (auto-detected by default) and `--scale SCALE`
   (default `1`).

4. In the tool's interactive menu, press **S** to capture a screenshot
5. The tool will save the screenshot as a PNG file in `tools/pngs/` with a timestamp-based filename

The tool requests the framebuffer, reassembles its chunks and saves a PNG.
