# libsdxi Programmer Manual

This directory contains the programmer-facing manuals for `libsdxi`.

The documentation set includes:

1. Markdown guides for major API areas.
2. Installed `man3` and `man7` reference pages.
3. Runnable examples under `samples/`.

## Manual Set

- `libsdxi-overview.md`: object model, headers, and API split
- `libsdxi-device.md`: device discovery, capabilities, attributes, and admin helpers
- `libsdxi-context-dataplane.md`: context lifecycle and local dataplane APIs
- `libsdxi-key-management.md`: RKey/AKey payloads, bitmaps, commits, and retries
- `libsdxi-event-handling.md`: context eventfds and event-loop integration
- `libsdxi-vendor-extensions.md`: generic vendor transport and helpers
- `libsdxi-contracts.md`: thread-safety, ownership, privileges, and API status

## Installed Reference Pages

The grouped reference pages are installed as:

- `SDXI(7)`
- `sdxi_types(3)`
- `sdxi_root(3)`
- `sdxi_util(3)`
- `sdxi_dev(3)`
- `sdxi_event(3)`
- `sdxi_key(3)`
- `sdxi_cxt(3)`
- `sdxi_vendor(3)`
- `sdxi_vendor_amd(3)`
- `sdxi_dp(3)`

These pages are hand-authored for this source tree. Installed headers also carry file-level `DOC:` blocks, so the build can later move to generated reference output without changing the manual layout.

## Sample Programs

Examples live under `samples/`. They build from the configured source tree and exercise the library against real SDXI devices.
