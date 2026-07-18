## Child Mode

[Go back](../../../README.md)

### Overview

Child Mode provides a PIN-protected, restricted interface for a device given to a child. It keeps the normal standalone messaging experience while hiding configuration pages, disabling companion access, and limiting which conversations are available.

> [!IMPORTANT]
> Child Mode is a practical user-interface lock, not tamper-proof security. Anyone with physical access to the bootloader can erase or replace the firmware.

---

### Preparing the device

Configure the device before enabling Child Mode:

1. Add and favourite the contacts, rooms, and channels the child may use. Contacts pinned to the Favourites Dial can also be opened directly.
2. Configure GPS, location-sharing ACLs, radio settings, sensors, and other parent-controlled features.
3. Open **Settings › Child Mode** and choose **Set PIN**.
4. Enter the six-digit PIN twice. The active digit is shown with a white background and black text.
5. Choose which optional home pages the child may see.
6. Set **Enabled** to **ON**, acknowledge the recovery warning, then leave Settings to lock the device.

The PIN is not displayed on the Settings screen after it is saved.

> [!WARNING]
> There is no forgotten-PIN recovery menu. If you forget the PIN, the device must be **ERASED & REFLASHED**. Erasing also removes the device identity, contacts, channels, messages, and settings, so keep a current backup.

---

### Settings and defaults

| Setting    | Default | Notes                                                              |
| ---------- | ------- | ------------------------------------------------------------------ |
| Enabled    | Off     | Enables the restrictions after the warning is accepted             |
| Set PIN    | —       | Sets or replaces the six-digit parent PIN; it must be entered twice |
| Recent     | Off     | Shows the Recent activity page                                      |
| Favourites | On      | Shows the Favourites Dial                                           |
| Map        | Off     | Shows a read-only map                                               |
| Sensors    | Off     | Shows sensor readings                                               |
| Shutdown   | Off     | Allows the child to open the Shutdown page                          |

The Clock, Messages, and Settings entries remain available. Opening Settings while locked presents the parent PIN screen.

---

### Restrictions while locked

| Area                 | Behaviour                                                                                               |
| -------------------- | ------------------------------------------------------------------------------------------------------- |
| Messages             | Lists only upstream-starred contacts and rooms, plus favourited channels                                |
| Favourites Dial      | Opens the contacts already pinned to its six slots; adding, moving, and removing pins is blocked        |
| Message context menus | Contact, room, and channel editing actions are blocked                                                  |
| Radio                | Page hidden; the configured radio settings continue to operate                                          |
| Bluetooth / USB      | Both companion protocol transports are disabled; USB power and charging are unaffected                  |
| Advert               | Manual Advert page hidden; received adverts continue to be processed without advert sound or vibration  |
| GPS and mapping      | GPS controls are hidden; configured GPS, telemetry, and ACL behaviour continues; optional Map is read-only |
| Tools                | Page hidden, including GPS and location-sharing configuration                                           |
| Sensors              | Optional read-only page                                                                                 |
| Shutdown             | Hidden unless explicitly enabled                                                                        |

Child Mode does not delete or rewrite the hidden settings. A parent can unlock Settings and adjust them normally.

---

### Parent access and relocking

Select **Settings** and enter the six-digit PIN to start a parent session. This temporarily restores the full home-page set and enables Bluetooth and USB companion access.

The device returns to the restricted interface when the parent leaves Settings, the display sleeps, or the device restarts. Disabling Child Mode leaves Bluetooth and USB enabled; it does not restore an earlier disabled state.

---

### Child Mode and Screen Lock

[Screen Lock](../screen_lock/screen_lock.md) prevents accidental keypresses and uses a button sequence to unlock. Child Mode controls which pages and conversations are available and requires a parent PIN. The two features can be used together.
