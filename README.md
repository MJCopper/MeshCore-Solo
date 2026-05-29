# Seeed Wio Tracker L1 - MeshCore "Solo" firmware

This branch extends the official MeshCore companion radio firmware for the **Seeed Wio Tracker L1**. Provides support for both the original OLED and the e-ink variant, with additional features and UI enhancements.

Join the discussion on the offical MeshCore discord: https://discord.gg/sdhYArU2jr

<img src="./img/radios.jpeg">

**Enclosures**

- [E-ink case](https://www.printables.com/model/1420534-seeed-wio-tracker-l1-e-ink-enclosure)
- [Oled case](https://www.printables.com/model/1380791-meshpack-seeed-l1-oled)

<!-- ### E-ink Display

The e-ink variant targets the Wio Tracker L1 fitted with a 2.13″ GxEPD2 panel (250 × 122 px). All screens have been adapted for the e-ink panel:

- **Adaptive layout** — every screen reflows correctly in both landscape (250 × 122) and portrait (122 × 250) orientations
- **Display rotation** — configurable in Settings › Display; applied immediately and persisted across reboots
- **Scaled navigation dots** — dot indicators on the home screen scale with line height so they remain visible at all orientations
- **Clock seconds suppressed by default** — seconds are hidden to reduce per-second panel refreshes and extend display lifetime; re-enable in Settings › Display -->

---

## Feature highlights

- Extended language support with native Unicode rendering and input [Lemon font](https://github.com/cmvnd/fonts) alongside the original ASCII mode (Default font with transliteration)

- Enabled sensor screens with support for all onboard sensors (temperature, humidity, pressure, luminosity, CO₂) and GPS data

- [Messages Screen](./docs/solo_features/message_screen/message_screen.md) — view and send messages, open message details, reply with quick messages or custom text, per-channel notification and melody overrides

- [Favourites Dial](./docs/solo_features/favourites_dial/favourites_dial.md) — pin up to six contacts for quick access from the home screen

- [Settings Screen](./docs/solo_features/settings_screen/settings_screen.md) — configure display, sound, home page order, radio and system settings

- [Clock Screen](./docs/solo_features/clock_screen/clock_screen.md) — view time and date plus up to three configurable sensor values

- [Screen Lock](./docs/solo_features/screen_lock/screen_lock.md) — lock the device to prevent accidental keypresses, with a lock screen showing time and sensor data

- [Tools Screen](./docs/solo_features/tools_screen/tools_screen.md) — GPS trail recording and export, ringtone editor, auto-reply bot, and more

## Firmware Variants

Two firmware builds are published with each release:

| Variant   | Display                   | File                                    |
| --------- | ------------------------- | --------------------------------------- |
| **OLED**  | SSD1306 / SH1106 128 × 64 | `WioTrackerL1_companion_dual_*.uf2`     |
| **E-ink** | GxEPD2 250 × 122          | `WioTrackerL1Eink_companion_dual_*.uf2` |

Both variants are built from a single codebase and share the same feature set. The `dual` in the filename means a single binary supports both BLE and USB serial — there are no separate BLE/USB builds.

> [!IMPORTANT]
> BLE connection has priority over USB serial. When a BLE connection is active, the USB protocol is suspended. When connecting to the companion app via USB, ensure to disconnect from BLE first or disable BLE directly from the device to avoid confusion.

### Flashing

1. Download the appropriate `.uf2` file for your display variant from the [releases page](https://github.com/MarekZegare4/MeshCore/releases)
2. Press reset twice quickly to enter bootloader mode (solid amber LED) - the device should appear as a mass storage drive on your computer
3. Copy the `.uf2` file to the drive to flash the firmware

Updating to newer firmware versions usually does not require erasing flash unless the release notes explicitly state otherwise. To retain your settings and data, avoid performing a factory reset after flashing unless you need to start fresh or encounter problems.

> [!WARNING]
> When migrating from official/custom firmware, backup your data and **perform factory reset** to prevent conflicts with existing settings and data:
>
> 1.  Open device settings in the companion app and download data backup
> 2.  Go to [MeshCore Flasher](https://meshcore.io/flasher) website
> 3.  Select "Wio Tracker L1 Pro"
> 4.  Choose any of the firmware variant
> 5.  Perform "Erase flash"

---

## Documentation

### This fork

| Document | Description |
|----------|-------------|
| [Messages Screen](./docs/solo_features/message_screen/message_screen.md) | Sending messages, context menus, reply, Notif/Melody overrides |
| [Favourites Dial](./docs/solo_features/favourites_dial/favourites_dial.md) | Pinned contacts grid, unread badges, pin/unpin |
| [Clock Screen](./docs/solo_features/clock_screen/clock_screen.md) | Clock page, date, configurable data fields |
| [Settings Screen](./docs/solo_features/settings_screen/settings_screen.md) | All settings sections with values and interactions |
| [Screen Lock](./docs/solo_features/screen_lock/screen_lock.md) | Lock/unlock sequence, lock screen, auto-lock |
| [Tools Screen](./docs/solo_features/tools_screen/tools_screen.md) | GPS trail, nearby nodes, ringtone editor, auto-reply bot, auto-advert |

### Upstream MeshCore

| Document | Description |
|----------|-------------|
| [FAQ](./docs/faq.md) | Frequently asked questions |
| [CLI Commands](./docs/cli_commands.md) | Commands for repeaters, room servers and sensors |
| [Terminal Chat CLI](./docs/terminal_chat_cli.md) | Commands for the terminal chat client |
| [Companion Protocol](./docs/companion_protocol.md) | Serial/BLE frame protocol between device and app |
| [Packet Format](./docs/packet_format.md) | LoRa packet structure |
| [QR Codes](./docs/qr_codes.md) | Channel and contact QR code formats |

---

## Development

This fork tracks the upstream [MeshCore](https://github.com/ripplebiz/MeshCore) repository. To prevent upstream changes from overwriting this README during merges, `README.md` is protected via `.gitattributes`. After cloning, run once:

```sh
git config merge.ours.driver true
```

### Contributing

Contributions are welcome! Fork the repository, make your changes, and submit a pull request. Please ensure your code adheres to the existing style and includes comments where necessary.

#### Display Screenshot Tool

> [!NOTE]
> The screenshot feature requires the `ENABLE_SCREENSHOT` build flag to be enabled. Add `-D ENABLE_SCREENSHOT` to your PlatformIO build flags, then recompile and flash the firmware.

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

<!--
**How it works:**
- The tool sends the `CMD_GET_SCREENSHOT` command (66) to the device
- The device responds with `RESP_CODE_SCREENSHOT` (29) containing the framebuffer data
- The framebuffer is transmitted in chunks (128×64 display = 1024 bytes, split across multiple frames)
- The tool reassembles the chunks and converts them to a PNG image -->
