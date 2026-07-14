# libsdxi Key Management Guide

`libsdxi` uses one public data model for device-scoped RKeys and context-scoped AKeys.

## Shared Types

The shared types are:

- `struct sdxi_key_payload`:
raw 16-byte key-table payload expressed as four little-endian dwords
- `struct sdxi_key_ent`:
one table index plus one raw `sdxi_key_payload`
- `struct sdxi_bitmap`:
validity bitmap returned by the `*_get_*key_list()` helpers
- `struct sdxi_key_set_result`:
commit state, first failed entry index, and completion flags

The timeout sentinel for both table-update paths is `SDXI_KEY_TIMEOUT_DEFAULT`.

## Entry Encoding

Payload encoders live in `sdxi/key.h`, not `sdxi/dp.h`, because they operate on key-table bytes rather than descriptor/ring state.

Encoder entry points:

- `sdxi_key_payload_clear()`
- `sdxi_rkey_payload_encode()`
- `sdxi_akey_payload_encode()`

Encoding rules:

- an entry with `SDXI_KEY_CTRL_VL` clear encodes as invalid all-zero dwords
- RKey encoding requires non-zero requester `sfunc`
- local AKey encoding requires `tgt_sfunc == 0` and `rkey == 0`
- remote AKey encoding requires `tgt_sfunc != 0` and only the `VL` control bit

The encoders return `-EINVAL` for combinations that do not fit the public table model. They do not encode driver-private policy such as reserved slots or privilege checks.

## Device-Scoped RKeys

RKey helpers operate on `sdxi_dev_t`:

- `sdxi_dev_get_rkey_list()`
- `sdxi_dev_get_rkeys()`
- `sdxi_dev_get_rkey()`
- `sdxi_dev_set_rkeys()`
- `sdxi_dev_set_rkey()`
- `sdxi_dev_set_rkeys_resume()`
- `sdxi_dev_alloc_rkey_index()`
- `sdxi_dev_invalidate_rkey()`

`sdxi_dev_get_rkey()` and `sdxi_dev_set_rkey()` are single-entry convenience wrappers around the batch helpers. `sdxi_dev_set_rkeys_resume()` is a library-side retry helper for partial commits. It re-issues the remaining entries until the kernel reports `SDXI_KEY_COMMIT_ALL_E` or forward progress stops.

`sdxi_dev_alloc_rkey_index()` returns a currently free RKey index from the validity bitmap. It does not reserve the slot; the caller  reserves it only by successfully programming an entry. `sdxi_dev_invalidate_rkey()` clears one RKey slot by submitting an all-zero payload.

Device-scoped RKey management is privileged under the current driver policy. RKey interrupt routing uses `SDXI_KEY_CTRL_IV` plus `intr_num`; if `IV` is clear, `intr_num` must be zero.

## Context-Scoped AKeys

AKey helpers operate on `sdxi_cxt_t`:

- `sdxi_cxt_get_akey_list()`
- `sdxi_cxt_get_akeys()`
- `sdxi_cxt_get_akey()`
- `sdxi_cxt_set_akeys()`
- `sdxi_cxt_set_akey()`
- `sdxi_cxt_set_akeys_resume()`
- `sdxi_cxt_alloc_akey_index()`
- `sdxi_cxt_invalidate_akey()`

`sdxi_cxt_get_akey()` and `sdxi_cxt_set_akey()` are single-entry convenience wrappers around the batch helpers. `sdxi_cxt_set_akeys_resume()` mirrors the RKey resume helper for partial AKey commits.

High-level local dataplane helpers use AKey slot `1` by default for normal userspace memory. Applications that manage AKeys manually should not overwrite slot `1` unless they also take responsibility for helper behavior. Applications with interrupt-capable contexts should also avoid the routed interrupt AKey returned by `sdxi_cxt_get_intr_info()`.

`sdxi_cxt_alloc_akey_index()` returns a currently free AKey index from the validity bitmap. It skips slot `0`, slot `1`, and the context interrupt AKey when one is assigned. It does not reserve the slot; the caller reserves it only by successfully programming an entry. `sdxi_cxt_invalidate_akey()` clears one AKey slot by submitting an all-zero payload.

AKey privilege and routing rules differ from RKeys:

- AKeys are context-scoped and use the context fd ownership model
- local AKey interrupt routing uses `SDXI_KEY_CTRL_IV` plus `intr_num`
- remote AKeys (`tgt_sfunc != 0`) are limited to the `VL` control bit and the referenced `rkey`

## Commit Semantics

Both RKey and AKey updates report commit state through `struct sdxi_key_set_result`.

- `SDXI_KEY_COMMIT_NONE_E`:
no progress reported
- `SDXI_KEY_COMMIT_ALL_E`:
the whole batch is committed
- `SDXI_KEY_COMMIT_PARTIAL_E`:
entries below `first_failed_index` are committed and later entries are not confirmed

Callers should check the result struct even when the function returns success. Partial commits are reported through the result object, not as transport errors.