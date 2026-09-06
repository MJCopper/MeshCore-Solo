# MeshCore Zen Companion Firmware

Zen extends the official [MeshCore](https://github.com/meshcore-dev/MeshCore)
companion firmware with a standalone messaging interface for the Seeed Wio
Tracker L1.

Current release: **Zen v2.27**, based on **MeshCore v1.17.1**.

## Supported hardware

| Device | Display | Release file |
| ------ | ------- | ------------ |
| Wio Tracker L1 OLED | 128 × 64 SSD1306/SH1106 | `zen-<version>-WioTrackerL1.uf2` |
| Wio Tracker L1 E-ink | 250 × 122 GxEPD2 | `zen-<version>-WioTrackerL1Eink.uf2` |

Both builds support BLE and USB serial. Firmware is available from the
[releases page](https://github.com/MJCopper/MeshCore-Zen/releases).

## Features

- Direct, channel and room messaging with full transcripts and delivery state.
- Predictive T9 and ABC text entry with Australianised word completion.
- UTF-8 text, monochrome emoji display and four insertable chat emoji.
- Clock, unread shortcut, GPS time sync and timed GPS operation.
- Editable Favourites Dial for four contacts.
- Automatic CardKB support through the Grove I2C port.
- PIN-protected Child Mode and scheduled Quiet Time.
- On-device radio, advert, notification and home-page settings.
- Node discovery, companion repeater mode, ringtone editor and diagnostics.
- Remote repeater and room administration from the Node List action menu.
- Sensors carousel with on-demand remote telemetry.

See [FEATURES.md](./FEATURES.md) for the compact feature summary.

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
- [UI developer guide](./docs/design/zen_ui_framework.md)

Protocol references remain under [`docs/`](./docs/).

## Screenshot capture

Connect by USB serial and send `S`. Disconnect the companion app first because
USB serial is suspended during an active BLE connection.

## Development

Zen tracks upstream MeshCore. After cloning, enable the repository's protected
README merge driver once:

```sh
git config merge.ours.driver true
```

Contributions should follow the existing code style and remain focused.

Thanks to [vanous](https://github.com/vanous),
[marczykm](https://github.com/marczykm), and the upstream
[MeshCore contributors](https://github.com/meshcore-dev/MeshCore/graphs/contributors).
