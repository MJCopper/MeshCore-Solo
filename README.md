# MeshCore Solo Companion Firmware

A fork of the official [MeshCore](https://github.com/meshcore-dev/MeshCore) companion radio firmware with extended features and UI enhancements, targeting a growing set of supported devices.

Join the discussion on the official MeshCore Discord: https://discord.gg/sdhYArU2jr

Solo firmware thread: https://discord.com/channels/1495203904898728149/1505294337884553447

---

## Supported Devices

| Device | Display | Firmware file |
| ------ | ------- | ------------- |
| Seeed Wio Tracker L1 (OLED) | SSD1306 / SH1106 128 × 64 | `solo-<version>-WioTrackerL1.uf2` |
| Seeed Wio Tracker L1 (E-ink) | GxEPD2 250 × 122 | `solo-<version>-WioTrackerL1Eink.uf2` |
| GAT562 30S Mesh Kit | SSD1306 128 × 64 | `solo-<version>-GAT562-30S-Mesh-Kit.uf2` |

All firmware files are published on the [releases page](https://github.com/MarekZegare4/MeshCore-Solo/releases). Each binary supports both BLE and USB serial — there are no separate BLE/USB builds.

<!-- **Enclosures (Wio Tracker L1)**
- [E-ink case](https://www.printables.com/model/1420534-seeed-wio-tracker-l1-e-ink-enclosure)
- [OLED case](https://www.printables.com/model/1380791-meshpack-seeed-l1-oled) -->

---

## Feature highlights

- Extended language support with native Unicode rendering and input ([Lemon font](https://github.com/cmvnd/fonts)) alongside the original ASCII mode (Default font with transliteration)

- Enabled sensor screens with support for onboard sensors (temperature, humidity, pressure, luminosity, CO₂) and GPS data

- **GPS navigation** — a full navigation suite that needs no extra hardware (details in the [Tools Screen](./docs/solo_features/tools_screen/tools_screen.md) docs):

  - **Waypoints** — mark a spot (car, camp, water…) with a short label, see it on the trail map, and get live bearing + distance back to it; the list always offers a one-tap backtrack to where your trail started
  - **GPS compass** — heading derived from course-over-ground (no magnetometer needed), shown as a clear scrolling heading tape with a large degrees + cardinal readout
  - **Navigate to anything** — a saved waypoint, a node straight from Nearby Nodes, or a location someone shares with you in a message
  - **Share & save locations** — send a waypoint to a contact or channel; on the other end, navigate to or save any shared location with one menu
  - **GPS trail** — background route recording with an auto-fit map (waypoints + live position), summary stats, and [GPX export](#solo-tools)
  - **Metric or imperial** — one global Units setting drives every distance and speed across the UI

- [Messages Screen](./docs/solo_features/message_screen/message_screen.md) — view and send messages, open message details, reply with quick messages or custom text, navigate to / save locations shared in a message, per-channel notification and melody overrides

- [Favourites Dial](./docs/solo_features/favourites_dial/favourites_dial.md) — pin up to six contacts for quick access from the home screen

- [Settings Screen](./docs/solo_features/settings_screen/settings_screen.md) — configure display, sound, home page order, radio and system settings

- [Clock Screen](./docs/solo_features/clock_screen/clock_screen.md) — view time and date plus up to three configurable data fields

- [Screen Lock](./docs/solo_features/screen_lock/screen_lock.md) — lock the device to prevent accidental keypresses, with a lock screen showing time and sensor data

- [Tools Screen](./docs/solo_features/tools_screen/tools_screen.md) — GPS trail & waypoints, compass, nearby nodes (with ping & navigate), ringtone editor, auto-reply bot, auto-advert, diagnostics, repeater

- **Battery saving (radio)** — two optional, independent toggles under Settings › Radio:
  - **Pwr save** — hardware duty-cycle receive (SX126x `SetRxDutyCycle`): the radio cycles RX↔sleep on its own and wakes on a preamble, cutting average RX current with only a little added receive latency
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
| [Messages Screen](./docs/solo_features/message_screen/message_screen.md)   | Sending messages, context menus, reply, navigate to / save shared locations, Notif/Melody overrides |
| [Favourites Dial](./docs/solo_features/favourites_dial/favourites_dial.md) | Pinned contacts grid, unread badges, pin/unpin                        |
| [Clock Screen](./docs/solo_features/clock_screen/clock_screen.md)          | Clock page, date, configurable data fields                            |
| [Settings Screen](./docs/solo_features/settings_screen/settings_screen.md) | All settings sections with values and interactions                    |
| [Screen Lock](./docs/solo_features/screen_lock/screen_lock.md)             | Lock/unlock sequence, lock screen, auto-lock                          |
| [Tools Screen](./docs/solo_features/tools_screen/tools_screen.md)          | GPS trail & waypoints, compass, navigation, nearby nodes, ringtone editor, auto-reply bot, auto-advert, diagnostics, repeater |

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

## Solo Tools

All solo builds include screenshot and GPX trail export support out of the box — no special build flags required.

### [Solo Tools Web App](https://marekzegare4.github.io/Solo-tools/) — no install required

Open the link in a browser with Web Serial support (Chromium-based) and click **Connect device**. The web app supports:

- **Screenshot** — capture the current display contents as a PNG
- **GPX export** — stream the GPS trail and download a timestamped `.gpx` file

Trigger the relevant action on the device (**Tools › Trail › Hold Enter** for GPX export, **S** key for screenshot) after connecting.

> Disconnect from the companion app before connecting via USB — USB serial is suspended while a BLE connection is active.

> If the companion app is connected over BLE the GPX export is safe (USB receive is ignored). If the app is on USB, disconnect it first — the raw stream will otherwise disrupt the app's frame protocol.

---

## Development

This fork tracks the upstream [MeshCore](https://github.com/meshcore-dev/MeshCore) repository. To prevent upstream changes from overwriting this README during merges, `README.md` is protected via `.gitattributes`. After cloning, run once:

```sh
git config merge.ours.driver true
```

### Contributing

Contributions are welcome. Fork the repository, make your changes, and open a pull request. Please follow the existing code style and keep changes focused.
