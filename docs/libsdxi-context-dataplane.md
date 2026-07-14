# libsdxi Context and Dataplane Guide

`sdxi_cxt_t` is the local userspace context object. It is where control-plane setup hands off to dataplane state.

## Header Boundary

The headers keep control-plane and local dataplane APIs separate:

- `sdxi/cxt.h`:
create, quiesce, destroy, and `sdxi_cxt_pasid()`
- `sdxi/event.h`:
context eventfd and interrupt metadata
- `sdxi/dp.h`:
raw local `cxt_fd`/mmap accessors and dataplane helpers

Raw rings, doorbells, and mappings are local-only details, so they stay out of the transport-neutral context header.

## Context Creation

Create a context with `sdxi_cxt_create()` and `struct sdxi_cxt_create_opts`.

Key fields:

- `size`:
use `sizeof(struct sdxi_cxt_create_opts)`; `0` is also accepted for the current struct layout
- `flags`:
may include `SDXI_CXT_CREATE_F_EVENTFD`
- `ds_ring_entries`:
requested ring entries, or `0` for the library default, currently 1024 entries
- `opb_000_enb`:
raw SDXI `opb_000` enable mask; bit N requests opcode type N for types 0..31, and `0` requests the driver default. Discover function-assigned types with `sdxi_dev_admin_disc()` or `sdxi_dev_get_op_type()` before setting their bits. The driver checks the requested mask against device availability and context policy.
- `eventfd`:
caller-owned eventfd when `SDXI_CXT_CREATE_F_EVENTFD` is set; if the flag is set and `eventfd == -1`, the library allocates one internally

When the eventfd flag is set, the library binds the context eventfd during context creation.

## Context Dataplane State

The `SDXI_CREATE_CXT` UAPI requires userspace-provided ring state.

`libsdxi` allocates `ds_ring`, `cxt_sts`, and `write_index` memory in userspace and passes those addresses to the kernel. The driver no longer returns mmap tokens for those structures.

Those userspace ring/status allocations are locked with `mlock()` so the kernel can safely access them for the lifetime of the context. Systems with a low `RLIMIT_MEMLOCK` may see `sdxi_cxt_create()` fail with `-ENOMEM`; increase the process locked-memory limit or grant an appropriate capability such as `CAP_IPC_LOCK` before creating contexts.

The library mmaps the doorbell from the context fd and exposes the address through `sdxi_cxt_get_mmaps()`.

When `sdxi_cxt_create()` is called with `opts == NULL`, the library uses that default ring size and per-descriptor CSB completion for high-level helper APIs.

## Introspection

Control-plane/context metadata accessors expose:

- `sdxi_cxt_pasid()`
- `sdxi_cxt_get_eventfd()`
- `sdxi_cxt_get_intr_info()`

Local dataplane accessors expose:

- `sdxi_cxt_get_fd()`
- `sdxi_cxt_get_mmaps()`

These are accessors only. There is no public post-create `sdxi_cxt_set_eventfd()` entry point.

## Byte-Exact Builders

`sdxi/dp.h` exposes byte-exact dataplane helper types:

- `struct sdxi_dp_desc`:
one 64-byte descriptor buffer suitable for copying into a mapped ring slot
- `struct sdxi_dp_csb`:
one 32-byte completion-status block with stable `signal` and `flags` fields

Builder helpers write descriptor bytes without exposing the library's private, compiler-dependent bitfield layouts. The request structs and helpers are:

- `sdxi_dp_desc_build_dma_nop()`
- `sdxi_dp_desc_build_dma_copy()`
- `sdxi_dp_desc_build_dma_rep_copy()`
- `sdxi_dp_desc_build_dma_write_imm()`
- `sdxi_dp_desc_build_atomic_uadd()`
- `sdxi_dp_desc_build_intr()`
- `sdxi_dp_desc_build_crc_m1()`
- `sdxi_dp_desc_set_csb()` / `sdxi_dp_desc_clear_csb()`
- `sdxi_dp_desc_set_read_barrier()`
- `sdxi_dp_csb_clear()` / `sdxi_dp_csb_*()` accessors

The raw builder surface is local-only. It is for applications that own ring submission and completion handling on a local context fd.

## Reserve, Store, Commit

The advanced submit path exposes ring reservation and commit primitives:

- `sdxi_dp_reserve()`
- `sdxi_dp_desc_store()`
- `sdxi_dp_commit()`

Submission sequence:

1. reserve one or more ring slots
2. store descriptor bytes into the reserved absolute positions
3. commit the reservation, which publishes the new write index and rings the doorbell

By default, reserve/commit is single-producer. This lower-cost mode assumes one submission producer per `sdxi_cxt_t`.

If the caller needs concurrent producers on one context, set `SDXI_DP_RESERVE_F_MULTI_PRODUCER` in `struct sdxi_dp_reserve_args`. That opt-in mode uses atomic compare-exchange loops on the shared producer counters. This allows multiple submitting threads to share one context at the cost of extra contention under load.

## Completion Waits

The advanced wait helpers are:

- `sdxi_dp_wait_read_idx()`
- `sdxi_dp_wait_read_idx_poll()`
- `sdxi_dp_wait_csb()`
- `sdxi_dp_wait_csb_poll()`

The non-`_poll` forms use the bound context eventfd when present and drain the notification as part of the wait. The `_poll` forms never drain the eventfd; use them for pure polling or for applications that observe eventfd readiness separately.

## Memory Ordering

The submission helpers define the ordering contract for raw mapped dataplane access:

- `sdxi_dp_desc_store()` writes descriptor bytes `8..63`, issues a release
fence, then writes bytes `0..7`
- `sdxi_dp_commit()` publishes the new write index with release semantics and only then rings the doorbell
- `sdxi_dp_csb_signal()`, `sdxi_dp_csb_flags()`, and the wait helpers use acquire semantics when observing completion state

Manual ring users should use these helpers rather than open-code the memory-ordering sequence.

## Quiesce and Destroy

`sdxi_cxt_quiesce()` is the explicit stop boundary. `sdxi_cxt_destroy()` automatically calls `sdxi_cxt_quiesce()` if the context is still live, tears down all library-owned mmaps and completion buffers, closes the owned eventfd when the library created it, and closes the context fd.

Applications should treat `sdxi_cxt_destroy()` as the normal cleanup path and use `sdxi_cxt_quiesce()` only when they want an explicit stop/sync point before destruction.

## High-Level Dataplane Helpers

The high-level helper set is intentionally small and local:

- `sdxi_nop()`
- `sdxi_mem_copy()`
- `sdxi_mem_copy_akeys()`
- `sdxi_mem_rep_copy()`
- `sdxi_write_imm()`
- `sdxi_atomic_uadd()`
- `sdxi_intr()`
- `sdxi_crc_m1()`


These helpers submit one operation, wait for per-descriptor completion, and return `0` or `-errno`. The library attaches and observes a CSB for each submitted descriptor. Applications that need raw ring progress semantics should use reserve/store/commit and wait helpers directly.

If an eventfd is bound, normal high-level helper waits may poll and drain it.

The library allocates its CSB array during `sdxi_cxt_create()`. After context setup, the normal helper hot path uses only that preallocated state plus stack storage.

## Local-Only Scope

`sdxi_cxt_t` is not transport-neutral. It owns mmaps, a doorbell, and a context fd, so the context/dataplane API is Linux local-backend only. A future daemon or broker can help create a context on the host that owns the SDXI device, but it should return a local handle rather than emulate raw rings, doorbells, or mmaps behind this API.
