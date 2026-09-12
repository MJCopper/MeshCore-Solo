# Radio, GPS, Bluetooth and Adverts

[Back to README](../../../README.md)

## Radio

The Radio home page shows frequency, spreading factor, bandwidth, coding rate,
TX power and noise floor. Configure these under **Settings › Radio**. A preset
changes frequency, bandwidth, spreading factor and coding rate together.
All communicating nodes must use compatible radio settings.

The toolbar signal indicator estimates recent repeater-link quality from the
SNR of routed traffic and repeater adverts. Direct companion traffic is ignored.
Three bars represent high link margin, two medium and one low; a cross means no
fresh measurement or that the radio is unavailable. The value is averaged in
RAM and expires after two hours.

When the user wakes the display with Back and the last measurement is over 30
minutes old, Zen performs one silent repeater-only discovery. Notification and
alarm wakes do not trigger it, and unsuccessful scans are limited to one attempt
per 30 minutes. Discovery is authoritative: the strongest response becomes the
new reading, or the indicator changes to a cross after the eight-second window
when no repeater answers. Manual Discover Repeaters scans behave the same way.
See the [Signal Indicator guide](../signal_indicator/signal_indicator.md) for
measurement sources, thresholds, timing and troubleshooting.

## GPS

The GPS home page shows mode, receiver state, satellites, position and altitude.
Press Enter to cycle through Off, Continuous, 2 min, 5 min, 15 min, 30 min,
1 h, 3 h and 6 h. Timed modes obtain a stable fix, sleep the receiver, then
repeat after the selected interval. The same mode can be staged under
**Settings › System** and is applied on exit.

At boot, Zen can temporarily use GPS to set the clock even when its saved mode
is Off. See the [Clock guide](../clock_screen/clock_screen.md).

## Bluetooth

Press Enter on the Bluetooth home page to enable or disable BLE. The pairing PIN
appears while waiting for a connection. Set the saved startup state under
**Settings › System**. BLE takes priority over USB serial.

## Adverts and location privacy

Press Enter on the Advert home page to send a manual advert. Under
**Settings › Advert**, Auto Advert offers Off, 1 hour, 3 hours and 6 hours;
GPS Details offers Hide or Share. GPS Details applies to manual, automatic and
companion-triggered self adverts. The advert toolbar icon remains visible for
five seconds after an advert is queued.
