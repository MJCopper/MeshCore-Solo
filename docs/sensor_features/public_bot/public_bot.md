# Public-channel Commands

[Back to README](../../../README.md)

The node listens on MeshCore's built-in Public channel for messages beginning
with the exact command word `!hillvue`. Matching is case-insensitive.

## Commands

| Message | Response |
| ------- | -------- |
| `!hillvue` | All measurements |
| `!hillvue all` | All measurements |
| `!hillvue t` | Temperature |
| `!hillvue h` | Humidity |
| `!hillvue p` | Pressure |
| `!hillvue a` | Air quality |

An option is selected by its first letter, so forms such as `temp`,
`Temperature` and `T` all select temperature. Multiple space-separated options
can be combined:

```text
!hillvue t air
!hillvue Temperature Humidity
!hillvue p h a
```

Each selected value appears on its own line in a single Public-channel reply.
Before the first successful sample, the reply is `Sensor data is not ready`.
During air-quality calibration it reports, for example:

```text
Air Quality: Warming Up (4/10)
```

## Mesh traffic controls

- Commands use the latest cached reading and never trigger an extra sample.
- The node sends at most one bot response in any 60-second period.
- Hashes for the eight most recent accepted commands are retained in RAM to
  suppress duplicate packet delivery.
- The bot does not publish on a schedule and does not respond to its own output.
- A response is sent once as a normal MeshCore flood; the node does not repeat
  other mesh traffic.

Unknown command options are ignored without a response.
