# Getting Started

[Back to README](../README.md)

## Before installing

Zen supports the Seeed Wio Tracker L1 with either the OLED or E-ink display.
Back up the identity, contacts, channels and messages with a companion app before
changing firmware. Follow the [flashing instructions](../README.md#flashing)
and choose the UF2 matching the display.

## First setup

1. Set the device name, timezone and units under **Settings › System**.
2. Select the local mesh preset under **Settings › Radio**.
3. Enable Bluetooth if a phone or computer will be used as a companion.
4. Send an advert from the Advert home page.
5. Leave GPS enabled until it obtains a fix if location sharing is required.

The clock shows `SYNC TIME` until valid time arrives from GPS or a connected
companion. Radio and messaging continue while time is unsynchronised.

## Navigation

- **Left/Right** moves between home pages or changes a setting value.
- **Up/Down** moves through lists and scrolls transcripts.
- **Enter** opens or confirms the selected item.
- **Hold Enter** opens available actions or quick replies.
- **Back** cancels or returns to Clock from a home page.

Back is the only button that wakes a sleeping display. A non-Clock home page
returns to Clock after five minutes without input.

## Companion connection

Enable Bluetooth from its home page or under **Settings › System**. While the
device is waiting to pair, the Bluetooth page displays its PIN. BLE takes
priority over USB serial, so disconnect BLE before using USB.

Settings are normally staged while editing and written only when leaving the
section with a changed value.

