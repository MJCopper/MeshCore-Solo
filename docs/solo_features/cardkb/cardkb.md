## CardKB

[Go back](../../../README.md)

### Overview

The Wio Tracker L1 Solo firmware supports the M5Stack CardKB connected directly
to the Grove I2C port. The keyboard is detected automatically at address `0x5F`;
there is no setting required to enable it and the Tracker controls remain
available.

---

### Connecting

1. Switch off the Wio Tracker L1.
2. Connect CardKB to the Grove port with a four-wire Grove cable.
3. Switch the Tracker on.

Detection occurs once during startup. If CardKB is connected later, or is
disconnected while running, restart the Tracker to detect it again.

To minimise battery use, CardKB is polled at most once every 20 ms and only while
the display is on. Wake the display with a Tracker button before using the
keyboard. Polling stops after three consecutive I2C read failures.

CardKB shares the external Grove I2C bus with supported sensors and uses address
`0x5F`. Multiple devices require a suitable I2C hub or splitter and distinct
addresses.

---

### Controls

| CardKB key | Action |
| ---------- | ------ |
| Arrow keys | Navigate menus, lists, pages and the on-screen keyboard |
| Enter | Operate the selected item or on-screen keyboard key |
| Fn + Enter | Submit the current text field |
| Esc | Cancel / Back |
| Backspace | Delete the previous character |
| Fn + Tab | Open the Hold Enter context action |
| Fn + letter | Open the accent choices for that Latin letter |
| Fn + Esc | Lock or unlock after the display has been woken |
| Shift / Sym | Enter uppercase letters and symbols using CardKB's own modes |

Printable CardKB characters are inserted directly in both ABC and T9 text
editors. The joystick and on-screen keyboard can still be used during the same
edit.

---

### Settings and status

Under **Settings › Keyboard**:

| Setting | Values | Description |
| ------- | ------ | ----------- |
| CardKB | Found / Missing | Current boot detection or connection state |
| Ext. KB | Full / Compact | Show the full on-screen grid or a compact external-keyboard status view |

The CardKB status is informational and is not stored as a preference.

---

### Troubleshooting

- `Missing` after connecting CardKB: restart the Tracker; detection is boot-only.
- `Missing` after it was working: three consecutive reads failed; check the cable
  and restart the Tracker.
- CardKB does not wake the display: this is intentional to avoid continuous
  screen-off I2C polling; use a Tracker button first.
- Disconnect other Grove devices temporarily to rule out wiring, power, hub or
  address conflicts.
