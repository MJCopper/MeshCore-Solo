# Public Archive Features

[Back to README](./README.md)

## Hardware

- Seeed Studio XIAO nRF52840 controller.
- SX1262 LoRa radio with DIO2 RF switching and a DIO3-controlled 1.8 V TCXO.
- Display-free operation using MeshCore's null display driver.
- USB serial configuration and UF2 bootloader flashing.
- Standard XIAO battery measurement and low-voltage protection.

## Archive

- Passive reception of text from MeshCore's built-in Public channel.
- Circular, volatile storage for the newest 256 messages.
- No message-history writes to internal flash.
- Login-time snapshots containing only messages present when login begins.
- Full available history for direct and one-hop sessions.
- Newest 20 messages for sessions detected over two or more hops.
- Companion `sync_since` handling to avoid resending messages already received.
- No unsolicited delivery when a new Public message is captured.

## Access

- Standard MeshCore room-server discovery and login protocol.
- Standard administrator password login and remote-management interface.
- Limited Guest interface for guest-password, blank-password and
  incorrect-password logins.
- Server-side rejection of room posts from guests and administrators.
- Local administration through USB serial.

## Mesh behaviour

- Leaf-node operation with packet forwarding disabled at startup.
- Public messages are received but not acknowledged or retransmitted.
- Public-channel binary data and malformed text payloads are ignored.
- Archived messages are transmitted only as part of a companion-initiated room
  synchronization session.
