# EGOS-2000 Network Driver & Socket API

This repository extracts the networking work I built on top of the EGOS-2000 teaching operating system: an Intel E1000 transmit path, a small socket-style service interface, and user-space demos that exercise the stack end to end in QEMU.

## What I Built

This project focused on turning EGOS-2000's low-level network plumbing into a usable end-to-end networking path for applications.

- Implemented the Intel E1000 Ethernet transmit path in C using DMA-backed descriptor rings and MMIO register programming
- Exposed a socket-style OS service interface with `listen`, `accept`, `recv`, `send`, and `close` for user-space applications
- Bridged packet I/O from the NIC driver through `uIP` into an EGOS system service and back into user-space apps
- Validated the stack with both a raw Ethernet transmit smoke test and a host-to-guest HTTP demo in QEMU

## Key Achievements

- Built a 16-entry E1000 transmit ring with 2KB DMA buffers and tail-pointer driven queue submission
- Published a minimal socket API over the EGOS system-service boundary instead of forcing apps to handle raw Ethernet frames
- Demonstrated end-to-end request handling from host `curl` to guest user space through `listen` -> `accept` -> `recv` -> `send` -> `close`
- Packaged the networking slice as a focused public artifact rather than burying the work inside an unrelated full teaching-OS fork

## What This Includes

- An E1000-backed Ethernet transmit path using DMA descriptors and MMIO register programming
- A network system service exposing `listen`, `accept`, `recv`, `send`, and `close`
- User-space syscall wrappers for the socket-style interface
- The `uIP` protocol modules and configuration files used by the socket service
- Supporting OS wiring needed by the final project: boot flow, service spawn, DMA/MMIO-visible memory mapping, and build/image integration
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
- `snapshot/earth/boot.c`: earth boot wiring for the network device hooks
- `snapshot/earth/mem_vm.c`: virtual-memory mappings needed for E1000 MMIO visibility
- `snapshot/grass/process.h` and `snapshot/grass/mlfq.c`: process and scheduler constants updated for the network service
- `snapshot/apps/system/sys_net.c`: EGOS network system service and socket lifecycle handlers
- `snapshot/apps/system/sys_proc.c`: system-process boot path that spawns the network service
- `snapshot/apps/app.h`: app-side includes needed by the imported network stack
- `snapshot/library/syscall/servers.h`: socket request and reply protocol
- `snapshot/library/syscall/servers.c`: user-side wrappers for `sock_*`
- `snapshot/library/egos.h`: earth interfaces extended with `net_send` and `net_recv`
- `snapshot/library/file/disk.h` and `snapshot/tools/mkfs.c`: system image layout updated to include `sys_net`
- `snapshot/library/uip/httpd.c`: small `uIP` bridge used to surface socket state
- `snapshot/library/uip/uip*.{c,h}` and `snapshot/library/uip/uip-conf.h`: protocol-stack core and EGOS-specific configuration
- `snapshot/apps/user/http.c`: socket-style HTTP demo app
- `snapshot/apps/user/net_raw_test.c`: raw Ethernet transmit smoke test
- `snapshot/Makefile`: QEMU network device and HTTP-forwarding run targets
- `history/demo_socket_http.sh`: archived demo script restored from project history
- `history/final_proj.txt`: archived local final-project writeup restored from project history

## Important Scope Note

This is a focused code snapshot, not a standalone fork of the full OS. The files here are meant to document and publish the networking slice of the project cleanly:

- the driver path I worked on
- the socket-style API I exposed
- the support modules I changed so the final networking stack could boot, map MMIO correctly, and run inside EGOS
- the demo applications used to validate it

I selected these files by walking the networking commit history and including:

- every file directly changed across the final-project networking sequence
- imported `uIP` modules that the final stack depended on
- earlier-lab support modules whose changes were required for the network service to boot or access hardware correctly

Broader EGOS modules that were untouched by the networking work are intentionally left out.

## Provenance and Licensing

The extracted code is derived from the MIT-licensed EGOS-2000 codebase, with preserved license text in [`LICENSE`](./LICENSE). Some included `uIP` headers retain their original BSD-style notices. See [`ATTRIBUTION.md`](./ATTRIBUTION.md) and [`THIRD_PARTY_NOTICES.md`](./THIRD_PARTY_NOTICES.md).

## Resume Summary

Suggested resume entry:

`EGOS-2000 Network Driver & Socket API | C, DMA, MMIO`

- Built a socket-style networking layer for EGOS-2000, exposing `listen`, `accept`, `recv`, `send`, and `close` endpoints over the OS system-service interface for user-space network applications.
- Implemented and validated the E1000 Ethernet transmit path underneath the socket layer, mapping packets into a 16-entry DMA descriptor ring with 2KB buffers and confirming end-to-end HTTP traffic in QEMU from host `curl` to EGOS user space.
