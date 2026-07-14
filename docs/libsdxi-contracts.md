# libsdxi Contracts and Current Guarantees

This file summarizes the behavioral contracts that matter to callers.

## Error Model

Unless a function documents otherwise, public entry points return:

- `0` on success
- `-errno` on failure

`sdxi_err_name()` returns a short stable token such as `EINVAL` or `ENOSPC`. `sdxi_err_str()` returns a human-readable message using a thread-local buffer. The returned pointer stays valid until the next `sdxi_err_str()` call in the same thread.

## Thread-Safety

The library does not add internal locking around public handles.

Practical guidance:

- treat each `sdxi_dev_t` as externally serialized
- treat each `sdxi_cxt_t` as externally serialized for control-plane calls and the high-level helper path
- do not submit dataplane work concurrently through one `sdxi_cxt_t` unless every raw submitter uses `sdxi_dp_reserve()` with `SDXI_DP_RESERVE_F_MULTI_PRODUCER`
- using independent handles from independent threads is acceptable as long as the handles themselves are not shared concurrently without external locking

The helper path caches device data lazily and updates producer state inside the context. The only supported concurrent submit path on one context is the raw reserve/store/commit API in multi-producer mode.

## Ownership and Lifetime

`sdxi_dev_t` owns:

- the opened device fd
- the copied device path
- lazily populated device caches

`sdxi_cxt_t` owns:

- the context fd returned by create
- any mmaps the library established
- an eventfd it allocated internally on the caller's behalf
- local completion buffers for dataplane helpers

`sdxi_cxt_destroy()` is the normal terminal cleanup path. It will attempt an implicit `sdxi_cxt_quiesce()` before tearing down mappings and closing the context fd.

`sdxi_root_t` owns only the cached device-path inventory returned by the most recent successful `sdxi_root_scan()`.

A context can outlive the device handle used to create it because the context owns its own fd and mappings.

## Privilege Expectations

Under the current driver policy:

- device discovery and read-mostly capability queries are typically usable with an open device fd, including `sdxi_dev_get_info()`, `sdxi_dev_get_uapi_info()`, `sdxi_dev_get_dev_info_raw()`, `sdxi_dev_get_caps()`, and `sdxi_dev_get_attr()`
- read-only GUID-based discovery through `sdxi_dev_admin_disc()` and `sdxi_dev_opgroups_refresh()` is available with delegated device-node access; context creation separately authorizes the requested operation groups
- device attribute updates and all device-scoped RKey helpers require `CAP_SYS_ADMIN`
- `sdxi_dev_admin_vendor_probe()` and `sdxi_dev_admin_vendor()` follow vendor-handler authorization; PF-wide AMD QoS management requires `CAP_SYS_ADMIN`
- context-scoped dataplane and AKey flows are the normal user-facing path once a context exists

Applications should treat privilege failures as normal operational outcomes.

## Locked Memory

Context creation allocates userspace ring/status buffers and locks them with `mlock()` before handing their addresses to the kernel. Applications should ensure that `RLIMIT_MEMLOCK` is large enough for the requested ring size and completion buffers, or run with a policy/capability such as `CAP_IPC_LOCK` that allows the lock. A too-small limit is reported as a context creation failure, commonly `-ENOMEM`.

## Interfaces Expected to Survive Versus Provisional Areas

Expected to survive mostly intact:

- `sdxi_root_t`
- `sdxi_dev_t` / `sdxi_cxt_t`
- device open/query flows
- key-management data model
- high-level dataplane helper direction
- grouped header layout under `sdxi/`

Explicitly provisional today:

- vendor packaging boundaries may still move
- exact doc-generation tooling may change once automated extraction is added
