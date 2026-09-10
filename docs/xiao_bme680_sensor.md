# XIAO nRF52840 BME680 Sensor

The `Xiao_nrf52_bme680_sensor` firmware turns a Seeed Studio XIAO nRF52840 and
supported SX1262 radio into a dedicated MeshCore environmental sensor. Connect
the BME680 over I2C using SDA `D7` and SCL `D6`. The default address is `0x76`.

The node exposes temperature, relative humidity, barometric pressure, calculated
altitude and a relative air-quality score through standard MeshCore telemetry
requests. It samples once per minute and does not forward mesh packets.

Calculated altitude has a fixed `+100 m` calibration offset for the installation
at Hillvue. Atmospheric pressure changes will still cause the reported altitude
to move over time; it is not a replacement for GPS elevation.

The node also listens for these case-insensitive commands on MeshCore's built-in
Public channel:

```text
!hillvue
!hillvue temp
!hillvue humidity
!hillvue pressure
!hillvue air
```

Options are case-insensitive and may be shortened to their first letter: `t`,
`h`, `p` or `a`. Multiple space-separated options may be combined, for example
`!hillvue t a` or `!hillvue Temperature Humidity`. The command prefix remains
the exact word `!hillvue`.

The general command returns all primary measurements. The `air` command returns
a relative score from 0 (best) to 500 (worst), with a descriptive label. The
score combines gas resistance with humidity compensation. It is useful for
observing changes around a fixed installation but is not a calibrated Bosch
BSEC IAQ measurement.

The general `!hillvue` response places each measurement on its own line within
one Public-channel message.

Air-quality output warms up for the first ten successful samples. Its clean-air
baseline and all readings exist only in RAM, restart on every boot and are never
written to flash.

Responses use the most recent cached reading. Commands never trigger an extra
sensor measurement. Duplicate commands are suppressed and the bot sends no more
than one response per minute. It never posts periodically or responds to its own
messages.

The configured admin password grants full remote-management access. A blank
login preserves any existing ACL role; otherwise blank and incorrect passwords
receive a transient read-only session that can request telemetry but cannot
manage the node or issue text commands. Read-only logins are held in RAM and do
not trigger ACL writes to flash.

Build with:

```sh
pio run -e Xiao_nrf52_bme680_sensor
```
