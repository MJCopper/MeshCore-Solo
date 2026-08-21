# MeshCore Zen Companion Firmware

A fork of the official [MeshCore](https://github.com/meshcore-dev/MeshCore) companion radio firmware with extended features and UI enhancements for the Seeed Wio Tracker L1.

Join the discussion on the official MeshCore Discord: https://discord.gg/sdhYArU2jr

Zen firmware thread: https://discord.com/channels/1495203904898728149/1505294337884553447

---

## Supported Devices

| Device | Display | Firmware file |
| ------ | ------- | ------------- |
| Seeed Wio Tracker L1 (OLED) | SSD1306 / SH1106 128 × 64 | `solo-<version>-WioTrackerL1.uf2` |
| Seeed Wio Tracker L1 (E-ink) | GxEPD2 250 × 122 | `solo-<version>-WioTrackerL1Eink.uf2` |

All firmware files are published on the [releases page](https://github.com/MarekZegare4/MeshCore-Solo/releases). Each binary supports both BLE and USB serial — there are no separate BLE/USB builds.

<!-- **Enclosures (Wio Tracker L1)**
- [E-ink case](https://www.printables.com/model/1420534-seeed-wio-tracker-l1-e-ink-enclosure)
- [OLED case](https://www.printables.com/model/1380791-meshpack-seeed-l1-oled) -->

---

## Feature highlights

- Native Unicode message rendering with an EN-US on-screen keyboard and Hold-Enter access to common European Latin accents

- Sensor support for dashboard fields, telemetry, and message placeholders without a standalone Sensors page

- [Messages Screen](./docs/solo_features/message_screen/message_screen.md) — view and send messages, open message details, reply with quick messages or custom text, configure per-channel notifications, and add/edit/delete channels on-device

- [Favourites Dial](./docs/solo_features/favourites_dial/favourites_dial.md) — pin four contacts for quick access from the home screen

- [Settings Screen](./docs/solo_features/settings_screen/settings_screen.md) — configure display, sound, home page order, radio and system settings

- [CardKB](./docs/solo_features/cardkb/cardkb.md) — external Grove keyboard input with boot-only detection and low-power polling

- [Clock Screen](./docs/solo_features/clock_screen/clock_screen.md) — view time, date and the total unread message count

- [Screen Lock](./docs/solo_features/screen_lock/screen_lock.md) — lock the device to prevent accidental keypresses, with a lock screen showing time and sensor data

- [Child Mode](./docs/solo_features/child_mode/child_mode.md) — PIN-protected restricted interface with favourite-only messaging, optional pages, and disabled Bluetooth/USB companion access

- [Quiet Time](./docs/solo_features/quiet_time/quiet_time.md) — silence incoming notification presentation on a daily local-time schedule while retaining messages and unread counts

- [Tools Screen](./docs/solo_features/tools_screen/tools_screen.md) — nearby nodes, ringtone editor, diagnostics, and optional repeater mode

- **Battery saving (radio)** — two optional, independent toggles under Settings › Radio:
  - **Auto pwr** — Adaptive Power Control: trims actual TX power on strong links (from ACK SNR) and ramps back up to the configured ceiling on weak/lost links; the home screen shows the live power

### E-ink Display (Wio Tracker L1)

The e-ink variant targets the Wio Tracker L1 fitted with a 2.13″ GxEPD2 panel (250 × 122 px). All screens have been adapted for the e-ink panel:

- **Adaptive layout** — every screen reflows correctly in both landscape (250 × 122) and portrait (122 × 250) orientations
- **Display rotation** — configurable in Settings › Display; applied immediately and persisted across reboots
- **Joystick rotation** — independent of display rotation; useful for custom enclosures
- **Full refresh interval** — configurable in Settings › Display; reduces ghosting on long sessions
- **Clock seconds suppressed by default** — seconds are hidden to reduce per-second panel refreshes and extend display lifetime; re-enable in Settings › Display

---

## Flashing

1. Download the `.uf2` file for your device from the [releases page](https://github.com/MarekZegare4/MeshCore-Solo/releases)
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
| [Messages Screen](./docs/solo_features/message_screen/message_screen.md)   | Sending messages, context menus, replies, and notification overrides |
| [Favourites Dial](./docs/solo_features/favourites_dial/favourites_dial.md) | Pinned contacts grid, unread badges, pin/unpin                        |
| [Clock Screen](./docs/solo_features/clock_screen/clock_screen.md)          | Clock page, date and total unread message count                        |
| [Settings Screen](./docs/solo_features/settings_screen/settings_screen.md) | All settings sections with values and interactions                    |
| [CardKB](./docs/solo_features/cardkb/cardkb.md)                             | External keyboard controls, status and low-power polling               |
| [Screen Lock](./docs/solo_features/screen_lock/screen_lock.md)             | Lock/unlock sequence, lock screen, auto-lock                          |
| [Child Mode](./docs/solo_features/child_mode/child_mode.md)                 | Parent PIN, allowed conversations and pages, transport restrictions, recovery |
| [Quiet Time](./docs/solo_features/quiet_time/quiet_time.md)                 | Daily local-time notification schedule with retained unread state |
| [Tools Screen](./docs/solo_features/tools_screen/tools_screen.md)          | Nearby nodes, ringtone editor, diagnostics, and repeater mode |
| [Zen UI framework](./docs/design/solo_ui_framework.md)                     | **Developer guide** — the reusable building blocks (screens, lists, popups, mini-icons, geo/persistence helpers) and how to add a new feature |

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

## Zen Tools

Zen builds include screenshot capture without requiring special build flags.

### [Zen Tools Web App](https://marekzegare4.github.io/Solo-tools/) — no install required

Open the link in a browser with Web Serial support (Chromium-based) and click **Connect device**. The web app supports:

- **Screenshot** — capture the current display contents as a PNG
Send the **S** key after connecting to capture the current screen.

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
