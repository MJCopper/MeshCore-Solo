# Diagnostics

[Back to README](../../../README.md)

Open **Tools › Diagnostics** and use Left/Right to change tabs.

- **Live** shows radio, mesh and queue counters. Hold Enter to reset them.
- **Battery** shows voltage, curve percentage and learned remaining runtime.
- **System** shows device and firmware information.
- **Events** contains the 16 newest RAM-only warnings and errors.
- **Font** provides a glyph viewer for display testing.

On Events, use Up/Down to select an entry and Enter to read its complete text.
Consecutive duplicates are combined. Hold Enter to clear the log; reboot also
clears it. Events are never written to flash. Repeated background failures are
rate-limited to avoid excessive display wakeups and noise.
