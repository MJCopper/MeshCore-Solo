# MeshCore Zen Companion Firmware

Zen extends the official [MeshCore](https://github.com/meshcore-dev/MeshCore)
companion firmware with a standalone messaging interface for the Seeed Wio
Tracker L1.

Current release: **Zen v1.32.33**, based on **MeshCore v1.17.1**.

## Supported hardware

| Device | Display | Release file |
| ------ | ------- | ------------ |
| Wio Tracker L1 OLED | 128 × 64 SSD1306/SH1106 | `WioTrackerL1_Zen_OLED.<version>.uf2` |
| Wio Tracker L1 E-ink | 250 × 122 GxEPD2 | `WioTrackerL1_Zen_E-INK.<version>.uf2` |

Both builds support BLE and USB serial. Firmware is available from the
[releases page](https://github.com/MJCopper/MeshCore-Zen/releases).

## Features

- Direct, channel and room messaging with full transcripts and delivery state.
- Route-aware retries, manual resend and unread-message shortcuts.
- Predictive T9 and ABC text entry with Australianised word completion.
- UTF-8 text, monochrome emoji display and four insertable chat emoji.
- Clock, unread shortcut, GPS time sync and timed GPS operation.
- Starred contacts, rooms and channels, plus an editable four-contact speed dial.
- Automatic CardKB support through the Grove I2C port.
- PIN-protected Child Mode and scheduled Quiet Time.
- On-device radio, advert, notification and home-page settings.
- Low-battery protection, emergency mode and learned runtime estimation.
- Node discovery, companion repeater mode, ringtone editor and RAM event log.
- Remote repeater, room and sensor administration from the Node List action menu.
- Sensors carousel with on-demand remote telemetry.

See [FEATURES.md](./FEATURES.md) for the complete Zen feature summary.

> [!WARNING]
> Zen is a personal project and beta-test software. It may contain faults or
> behave unexpectedly. Back up your device and use this firmware at your own
> risk.

## Flashing

1. Download the correct `.uf2` from the releases page.
2. Press Reset twice to open the bootloader drive.
3. Copy the `.uf2` to that drive.

BLE takes priority over USB serial. Disconnect BLE before using a USB companion
connection.

> [!WARNING]
> When changing from another firmware, back up your device, erase flash with the
> [MeshCore Flasher](https://meshcore.io/flasher), then install Zen.

## Guides

- [Messages](./docs/zen_features/message_screen/message_screen.md)
- [Settings](./docs/zen_features/settings_screen/settings_screen.md)
- [Clock](./docs/zen_features/clock_screen/clock_screen.md)
- [Favourites Dial](./docs/zen_features/favourites_dial/favourites_dial.md)
- [CardKB](./docs/zen_features/cardkb/cardkb.md)
- [Child Mode](./docs/zen_features/child_mode/child_mode.md)
- [Quiet Time](./docs/zen_features/quiet_time/quiet_time.md)
- [Tools](./docs/zen_features/tools_screen/tools_screen.md)
- [Build Zen](./docs/building_zen.md)
- [UI developer guide](./docs/design/zen_ui_framework.md)

Protocol references remain under [`docs/`](./docs/).

## Development

See [Building Zen](./docs/building_zen.md) for local builds and tests. Zen tracks
upstream MeshCore. After cloning, enable the repository's protected README merge
driver once:

```sh
git config merge.ours.driver true
```

Contributions should follow the existing code style and remain focused.

Thanks to [vanous](https://github.com/vanous),
[marczykm](https://github.com/marczykm), and the upstream
[MeshCore contributors](https://github.com/meshcore-dev/MeshCore/graphs/contributors).
