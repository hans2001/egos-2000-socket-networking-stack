# EGOS-2000 Network Driver & Socket API

This repository extracts the networking work I built on top of the EGOS-2000 teaching operating system: an Intel E1000 transmit path, a small socket-style service interface, and user-space demos that exercise the stack end to end in QEMU.

## What This Includes

- An E1000-backed Ethernet transmit path using DMA descriptors and MMIO register programming
- A network system service exposing `listen`, `accept`, `recv`, `send`, and `close`
- User-space syscall wrappers for the socket-style interface
- Two demos:
  - a 60-byte raw Ethernet transmit smoke test
  - a socket-style HTTP demo reachable from host `curl`

## Architecture

Host `curl`
-> QEMU user-mode networking
-> emulated Intel E1000 NIC
-> `snapshot/earth/dev_net.c`
-> `uIP` TCP/IP processing
-> `snapshot/apps/system/sys_net.c`
-> socket-style service API
-> `snapshot/apps/user/http.c`

## Validated Behavior

This extracted snapshot is backed by the implementation and demo flow in the original worktree:

- Raw Ethernet transmit smoke test sends a 60-byte frame through `net_send`
- HTTP demo listens on guest port `80` and is forwarded to host port `8080`
- Expected service lifecycle during the HTTP demo:
  - `sock_accept`
  - `sock_recv`
  - `sock_send`
  - `sock_close`

## Repository Layout

- `snapshot/earth/dev_net.c`: E1000 driver bring-up, TX/RX rings, DMA buffers, and MMIO queue programming
- `snapshot/apps/system/sys_net.c`: EGOS network system service and socket lifecycle handlers
- `snapshot/library/syscall/servers.h`: socket request and reply protocol
- `snapshot/library/syscall/servers.c`: user-side wrappers for `sock_*`
- `snapshot/library/uip/httpd.c`: small `uIP` bridge used to surface socket state
- `snapshot/apps/user/http.c`: socket-style HTTP demo app
- `snapshot/apps/user/net_raw_test.c`: raw Ethernet transmit smoke test

## Important Scope Note

This is a focused code snapshot, not a standalone fork of the full OS. The files here are meant to document and publish the networking slice of the project cleanly:

- the driver path I worked on
- the socket-style API I exposed
- the demo applications used to validate it

The full EGOS runtime, toolchain glue, and broader teaching-OS infrastructure are intentionally not duplicated here.

## Provenance and Licensing

The extracted code is derived from the MIT-licensed EGOS-2000 codebase, with preserved license text in [`LICENSE`](./LICENSE). Some included `uIP` headers retain their original BSD-style notices. See [`ATTRIBUTION.md`](./ATTRIBUTION.md) and [`THIRD_PARTY_NOTICES.md`](./THIRD_PARTY_NOTICES.md).

## Resume Summary

Suggested resume entry:

`EGOS-2000 Network Driver & Socket API | C, DMA, MMIO`

- Built a socket-style networking layer for EGOS-2000, exposing `listen`, `accept`, `recv`, `send`, and `close` endpoints over the OS system-service interface for user-space network applications.
- Implemented and validated the E1000 Ethernet transmit path underneath the socket layer, mapping packets into a 16-entry DMA descriptor ring with 2KB buffers and confirming end-to-end HTTP traffic in QEMU from host `curl` to EGOS user space.
