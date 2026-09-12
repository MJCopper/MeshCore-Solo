# Battery and Low Power Mode

[Back to README](../../../README.md)

Zen treats 4.12 V as full and 3.3 V as empty using a non-linear single-cell
Li-ion/LiPo curve. The device shuts down at or below 3.3 V unless externally
powered.

At 20% or below, Zen gives one short **Low Battery** alert and repeats it hourly
while on battery. Quiet Time suppresses the sound. The alert does not create an
unread message.

## Low Power mode

After three consecutive samples at 5% or below, Zen suspends the radio, GPS and
Bluetooth, stops automatic retries, and uses minimum OLED brightness with a
five-second display timeout. CardKB remains usable while the display is awake.
External power leaves Low Power mode and restores the saved settings.

## Emergency communications

Low Power adds **Low Power Emergency** at the end of the carousel. Press Enter
and confirm to enable radio and messaging for ten minutes. GPS and Bluetooth
remain off initially but can be enabled temporarily from their home pages. The
Emergency page shows remaining time and all three statuses. Press Enter there
again to end the window early.

## Remaining-time estimate

**Tools › Diagnostics › Battery** shows filtered voltage, percentage and time
remaining to 3.3 V. It starts with a five-day full-charge model and gradually
learns from normal discharge, so the result is an estimate rather than a
guaranteed runtime.

