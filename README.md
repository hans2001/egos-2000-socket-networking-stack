# EGOS-2000 Network Driver & Socket API

Built a socket-style networking layer for the EGOS-2000 teaching operating system, backed by an emulated Intel E1000 NIC in QEMU. The project takes networking all the way from hardware-facing transmit descriptors and MMIO queue updates to a user-space HTTP application calling `listen`, `accept`, `recv`, `send`, and `close`.

## Highlights

- Implemented the E1000 Ethernet transmit path in C using a 16-entry DMA-backed descriptor ring
- Programmed NIC transmit queues through MMIO registers with 2KB transmit buffers per descriptor
- Exposed a socket-style OS service interface for user-space applications
- Validated the stack with both a raw Ethernet smoke test and an end-to-end HTTP demo from host `curl` to EGOS user space

## Architecture

```text
Host curl
  -> QEMU user-mode networking (hostfwd tcp::8080-:80)
  -> Intel E1000 emulated NIC
  -> earth/dev_net.c
     - E1000 init
     - TX/RX rings
     - DMA buffer submission
     - MMIO head/tail control
  -> uIP TCP/IP stack
  -> apps/system/sys_net.c
     - socket-style service interface
     - listen / accept / recv / send / close
  -> apps/user/http.c
     - user-space HTTP response
```

## What I Built

- E1000 driver path:
  implemented NIC initialization, transmit/receive ring setup, DMA buffer addressing, descriptor ownership handoff, and MMIO tail-pointer submission in `snapshot/earth/dev_net.c`
- Socket service layer:
  built the EGOS network system service in `snapshot/apps/system/sys_net.c` so applications could use `sock_listen`, `sock_accept`, `sock_recv`, `sock_send`, and `sock_close`
- User-facing syscall wrappers:
  added the request/reply protocol and user-side wrappers in `snapshot/library/syscall/servers.h` and `snapshot/library/syscall/servers.c`
- End-to-end demo apps:
  validated the stack with `snapshot/apps/user/net_raw_test.c` and `snapshot/apps/user/http.c`

## What I Owned

- The E1000 network-device integration and queue programming path
- The socket-style service API exposed over the EGOS system-service boundary
- The wiring needed for the network service to boot and run inside EGOS
- The demo and validation flow used to prove end-to-end functionality in QEMU

## Validation

Exact behaviors validated in this project:

- Raw packet path:
  `snapshot/apps/user/net_raw_test.c` sends a 60-byte Ethernet frame through `net_send`, ending with `Raw Ethernet transmit path completed`
- Descriptor-ring configuration:
  `snapshot/earth/dev_net.c` sets up a 16-entry E1000 TX ring and 2KB transmit buffers
- HTTP path:
  QEMU forwards host port `8080` to guest port `80`, the HTTP app listens in EGOS, and host `curl http://127.0.0.1:8080/` returns:

```http
HTTP/1.0 200 OK
Content-Type: text/plain
Connection: close

EGOS socket abstraction demonstration
```

- Socket lifecycle visibility:
  the expected guest logs show `sock_accept`, `sock_recv`, `sock_send`, and `sock_close`

## Repository Layout

- `snapshot/earth/dev_net.c`: E1000 driver bring-up, TX/RX rings, DMA buffers, and MMIO queue programming
- `snapshot/earth/boot.c`: boot wiring for the network device hooks
- `snapshot/earth/mem_vm.c`: virtual-memory mappings needed for E1000 MMIO visibility
- `snapshot/grass/process.h` and `snapshot/grass/mlfq.c`: process and scheduler constants updated for the network service
- `snapshot/apps/system/sys_proc.c`: system-process boot path that spawns the network service
- `snapshot/apps/system/sys_net.c`: EGOS network system service and socket lifecycle handlers
- `snapshot/apps/app.h`: app-side includes needed by the imported network stack
- `snapshot/library/syscall/servers.h`: socket request and reply protocol
- `snapshot/library/syscall/servers.c`: user-side wrappers for `sock_*`
- `snapshot/library/egos.h`: earth interfaces extended with `net_send` and `net_recv`
- `snapshot/library/file/disk.h` and `snapshot/tools/mkfs.c`: system image layout updated to include `sys_net`
- `snapshot/library/uip/httpd.c`: bridge used to surface socket state from `uIP`
- `snapshot/library/uip/uip*.{c,h}` and `snapshot/library/uip/uip-conf.h`: protocol-stack core and EGOS-specific configuration
- `snapshot/apps/user/http.c`: socket-style HTTP demo app
- `snapshot/apps/user/net_raw_test.c`: raw Ethernet transmit smoke test
- `snapshot/Makefile`: QEMU network device and HTTP-forwarding run targets
- `history/demo_socket_http.sh`: archived demo script restored from project history
- `history/final_proj.txt`: archived local final-project writeup restored from project history

## Notes

This repository publishes the networking project as a focused portfolio artifact. It includes the modules directly changed for the final networking stack, the support code that stack depended on, and the demo materials used to validate it.

## License

The code here is derived from the MIT-licensed EGOS-2000 codebase, with preserved notices in [`LICENSE`](./LICENSE). Some included `uIP` files retain their original BSD-style notices. See [`ATTRIBUTION.md`](./ATTRIBUTION.md) and [`THIRD_PARTY_NOTICES.md`](./THIRD_PARTY_NOTICES.md).
