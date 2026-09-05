# Child Mode

[Back to README](../../../README.md)

Child Mode provides a PIN-protected interface with favourite-only messaging.
It is a UI lock, not protection against someone who can erase or replace the
firmware.

## Setup

1. Configure the device and favourite the permitted contacts and rooms.
2. Favourite any permitted private channels.
3. Open **Settings › Child Mode**, set and confirm a six-digit PIN.
4. Choose whether Channels and the Favourites page are visible.
5. Enable Child Mode, accept the warning, then leave Settings.

| Setting | Default | Behaviour |
| ------- | ------- | --------- |
| Enabled | Off | Enables restrictions after confirmation |
| Set PIN | — | Sets or replaces the hidden six-digit PIN |
| Channels | Off | Shows favourited private channels only |
| Favourites | On | Shows the Favourites Dial |

While locked:

- Messages lists only favourited contacts and rooms, plus enabled favourited
  private channels.
- Favourite, contact and channel editing is blocked.
- Other messages are stored but do not alert, wake the display or count as
  child-visible unread messages.
- Bluetooth and USB companion access are disabled.
- Parent-controlled Settings, Tools, Radio, GPS and Advert pages are hidden.
- Mesh routing and acknowledgements continue normally.

Opening Settings asks for the PIN. A successful entry restores parent access
until Settings is closed, the display sleeps or the device restarts.

> [!WARNING]
> If you forget the PIN, the device must be **ERASED & REFLASHED**. Erasing also
> removes identity, contacts, channels, messages and settings.
