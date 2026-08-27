# MeshCore Zen Companion Firmware

A fork of the official [MeshCore](https://github.com/meshcore-dev/MeshCore) companion radio firmware with extended features and UI enhancements for the Seeed Wio Tracker L1.

Current Zen release: **v2.26**, based on **MeshCore v1.17.1**.

Join the discussion on the official MeshCore Discord: https://discord.gg/sdhYArU2jr

Zen firmware thread: https://discord.com/channels/1495203904898728149/1505294337884553447

---

## Supported Devices

| Device | Display | Firmware file |
| ------ | ------- | ------------- |
| Seeed Wio Tracker L1 (OLED) | SSD1306 / SH1106 128 × 64 | `zen-<version>-WioTrackerL1.uf2` |
| Seeed Wio Tracker L1 (E-ink) | GxEPD2 250 × 122 | `zen-<version>-WioTrackerL1Eink.uf2` |

All firmware files are published on the [releases page](https://github.com/MJCopper/MeshCore-Zen/releases). Each binary supports both BLE and USB serial — there are no separate BLE/USB builds.

<!-- **Enclosures (Wio Tracker L1)**
- [E-ink case](https://www.printables.com/model/1420534-seeed-wio-tracker-l1-e-ink-enclosure)
- [OLED case](https://www.printables.com/model/1380791-meshpack-seeed-l1-oled) -->

---

## Feature highlights

- Native UTF-8 text and emoji rendering, including compact monochrome fallbacks for common emoji

- Predictive T9 and ABC on-screen keyboards, a 4,000-word Australianised completion dictionary, and four insertable chat emoji

- Sensor support for dashboard fields, telemetry and message placeholders

- [Messages Screen](./docs/zen_features/message_screen/message_screen.md) — full-message transcripts, replies, quick messages, delivery status, manual resend, notification controls, and on-device channel management

- [Favourites Dial](./docs/zen_features/favourites_dial/favourites_dial.md) — pin four contacts for quick access from the home screen

- [Settings Screen](./docs/zen_features/settings_screen/settings_screen.md) — configure display, sound, home-page order, radio, GPS, Bluetooth, adverts and messaging

- [CardKB](./docs/zen_features/cardkb/cardkb.md) — external Grove keyboard input with boot-only detection and screen-aware low-power polling

- [Clock Screen](./docs/zen_features/clock_screen/clock_screen.md) — fixed first page with time synchronisation, date, unread count and latest-conversation shortcut

- [Screen Lock](./docs/zen_features/screen_lock/screen_lock.md) — prevent accidental input while retaining a compact time, date and unread display

- [Child Mode](./docs/zen_features/child_mode/child_mode.md) — PIN-protected favourite-only messaging with optional private channels and disabled companion access

- [Quiet Time](./docs/zen_features/quiet_time/quiet_time.md) — silence notification sounds on a daily local-time schedule while visual alerts, vibration and unread counts continue

- [Tools Screen](./docs/zen_features/tools_screen/tools_screen.md) — nearby nodes, companion repeater, ringtone editor and diagnostics

- GPS time synchronisation at boot, timed low-power GPS modes, advert location privacy, hard-coded delivery retry policy, and automatic return to Clock after five idle minutes

### E-ink Display (Wio Tracker L1)

The e-ink variant targets the Wio Tracker L1 fitted with a 2.13″ GxEPD2 panel (250 × 122 px). All screens have been adapted for the e-ink panel:

- **Adaptive layout** — every screen reflows correctly in both landscape (250 × 122) and portrait (122 × 250) orientations
- **Display rotation** — configurable in Settings › Display; applied immediately and persisted across reboots
- **Joystick rotation** — independent of display rotation; useful for custom enclosures
- **Full refresh interval** — configurable in Settings › Display; reduces ghosting on long sessions
- **Clock seconds suppressed by default** — seconds are hidden to reduce per-second panel refreshes and extend display lifetime; re-enable in Settings › Display

---

## Flashing

1. Download the `.uf2` file for your device from the [releases page](https://github.com/MJCopper/MeshCore-Zen/releases)
2. Press reset twice quickly to enter bootloader mode — the device should appear as a mass storage drive on your computer
3. Copy the `.uf2` file to the drive to flash the firmware

> [!IMPORTANT]
> BLE connection has priority over USB serial. When a BLE connection is active, the USB protocol is suspended. When connecting to the companion app via USB, ensure to disconnect from BLE first or disable BLE directly from the device to avoid confusion.

Updating to a newer version usually does not require erasing flash unless the release notes explicitly state otherwise.

> [!WARNING]
> When migrating from official or other custom firmware, backup your data and **perform a factory reset** to prevent conflicts with existing settings:
>
> 1. Open device settings in the companion app and download a data backup
> 2. Go to [MeshCore Flasher](https://meshcore.io/flasher), select your device, and perform **Erase flash** before flashing

---

## Documentation

### This fork

| Document                                                                   | Description                                                           |
| -------------------------------------------------------------------------- | --------------------------------------------------------------------- |
| [Messages Screen](./docs/zen_features/message_screen/message_screen.md)   | Transcripts, sending, replies, delivery state, resend and context menus |
| [Favourites Dial](./docs/zen_features/favourites_dial/favourites_dial.md) | Pinned contacts, unread badges and editing                             |
| [Clock Screen](./docs/zen_features/clock_screen/clock_screen.md)          | Clock, time sync, date, unread count and conversation shortcut         |
| [Settings Screen](./docs/zen_features/settings_screen/settings_screen.md) | Current settings sections, values and save-on-exit behaviour           |
| [CardKB](./docs/zen_features/cardkb/cardkb.md)                            | External keyboard controls, status and low-power polling               |
| [Screen Lock](./docs/zen_features/screen_lock/screen_lock.md)             | Lock/unlock sequence, lock screen and auto-lock                        |
| [Child Mode](./docs/zen_features/child_mode/child_mode.md)                | Parent PIN, allowed conversations, transport restrictions and recovery |
| [Quiet Time](./docs/zen_features/quiet_time/quiet_time.md)                | Daily local-time sound suppression with retained notifications         |
| [Tools Screen](./docs/zen_features/tools_screen/tools_screen.md)          | Nearby nodes, repeater, ringtone editor and diagnostics                |
| [Zen UI framework](./docs/design/zen_ui_framework.md)                     | Developer guide to the reusable UI, policy and persistence modules     |

### Upstream MeshCore

| Document                                           | Description                                      |
| -------------------------------------------------- | ------------------------------------------------ |
| [FAQ](./docs/faq.md)                               | Frequently asked questions                       |
| [CLI Commands](./docs/cli_commands.md)             | Commands for repeaters, room servers and sensors |
| [Terminal Chat CLI](./docs/terminal_chat_cli.md)   | Commands for the terminal chat client            |
| [Companion Protocol](./docs/companion_protocol.md) | Serial/BLE frame protocol between device and app |
| [Packet Format](./docs/packet_format.md)           | LoRa packet structure                            |
| [QR Codes](./docs/qr_codes.md)                     | Channel and contact QR code formats              |

---

## Screenshot capture

Zen builds include screenshot capture without special build flags. Connect over
USB serial and send the **S** key to capture the current screen.

> Disconnect from the companion app before connecting via USB — USB serial is suspended while a BLE connection is active.


---

## Development

This fork tracks the upstream [MeshCore](https://github.com/meshcore-dev/MeshCore) repository. To prevent upstream changes from overwriting this README during merges, `README.md` is protected via `.gitattributes`. After cloning, run once:

```sh
git config merge.ours.driver true
```

### Contributing

Contributions are welcome. Fork the repository, make your changes, and open a pull request. Please follow the existing code style and keep changes focused.

---

## Contributors

Big thanks to the people who contributed to this fork:

- [vanous](https://github.com/vanous)
- [marczykm](https://github.com/marczykm)

Built on upstream [MeshCore](https://github.com/meshcore-dev/MeshCore) and its [community](https://github.com/meshcore-dev/MeshCore/graphs/contributors).
