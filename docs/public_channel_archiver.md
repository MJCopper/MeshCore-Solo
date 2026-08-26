# Public Channel Archiver

The `Xiao_nrf52_archiver` firmware turns a Seeed Studio XIAO nRF52840 and
supported SX1262 radio into a read-only, volatile archive of MeshCore's built-in
Public channel.

The archiver stores the latest 256 Public text messages in a circular RAM
buffer. History is lost when the device restarts and is never written to flash.
Public channel data packets and malformed text packets are ignored.

The server does not forward mesh packets or push newly captured messages to
companions. Logging into the room opens a snapshot containing messages that
were already present at login. Messages arriving after that point remain silent
until the companion deliberately logs in again.

Direct and one-hop login sessions can synchronize the complete 256-message RAM
archive. Sessions detected over two or more hops are limited to the newest 20
messages to reduce mesh traffic. Flood routes provide an exact hop count. For a
direct-routed login, the server uses its stored reverse path when available;
otherwise the protocol provides no earlier-hop metadata and the connection is
treated as direct.

The admin password grants the standard administrator role and remote-management
UI. Guest, blank and incorrect passwords receive MeshCore's limited read-only
guest UI. The archive independently rejects room posts from every ACL role.
Local administrative CLI commands also remain available over USB serial.

The archive cannot prevent a companion that has the Public channel configured
from receiving the original live Public message directly. It prevents the
archive server from delivering a second, unsolicited copy.

Build with:

```sh
pio run -e Xiao_nrf52_archiver
```
