# libsdxi Device Management Guide

Device discovery starts with `sdxi_root_t`. A root caches the local `/dev/sdxi*` inventory and exposes stable PF/VF ordering for each scan.

## Root Inventory

Create and scan a local root with:

- `sdxi_root_create()`
- `sdxi_root_scan()`

Then inspect the current inventory with:

- `sdxi_root_dev_count()`
- `sdxi_root_dev_path()`
- `sdxi_root_dev_open()`

The implementation supports only the local backend. `NULL` and `SDXI_ROOT_URI_LOCAL` are accepted as root URIs; anything else returns `-ENOTSUP`.

Scan results are sorted by PF, then VF. A PF entry appears before its VFs.

## Discovery and Open

Use `sdxi_root_*()` for structured discovery. `sdxi_list_devs()` remains available as a convenience wrapper around a local root scan. `sdxi_dev_open()` can open a known path directly.

Open behavior is:

- try `O_RDWR | O_CLOEXEC` first
- fall back to `O_RDONLY | O_CLOEXEC` on `EACCES` or `EPERM`
- negotiate UAPI compatibility immediately

This allows read-only capability and discovery workloads when the caller lacks permission for privileged updates.

## Device Snapshot, UAPI Info, and Capabilities

`sdxi_dev_get_info()` is the primary inspection function. It returns one serialization-friendly `struct sdxi_dev_info` snapshot:

- the cached open-time UAPI negotiation result
- the raw `SDXI_GET_DEV_INFO` fields
- the library-decoded `sdxi_dev_caps` summary
- a `flags` bitmask that reports whether open fell back to read-only (`SDXI_DEV_INFO_F_READ_ONLY`)
- `size` set by the library to `sizeof(struct sdxi_dev_info)` and reserved fields zeroed on output

Use the narrower getters when only one part of that snapshot is needed:

- `sdxi_dev_get_uapi_info()` for the cached open-time ABI negotiation result
- `sdxi_dev_has_uapi_cap()` for one cached capability bit
- `sdxi_dev_get_dev_info_raw()` for the raw cached device-info fields
- `sdxi_dev_get_caps()` for the decoded capability summary

The decoded `sdxi_dev_caps` fields include:

- hardware version fields
- PF/VF identity
- queue, key-table, and errlog limits
- descriptor doorbell stride
- raw `MMIO_CAP1.opb_000_cap` operation-group capability mask

## Attributes

Device attributes are per-selector `uint64_t` values exposed through:

- `sdxi_dev_get_attr()`
- `sdxi_dev_set_attr()`
- `sdxi_dev_attr_name()`
- `sdxi_dev_attr_by_name()`
- `sdxi_dev_attr_is_writable()`

The library maps the kernel's `-EOPNOTSUPP` onto `-ENOTSUP` so callers can probe optional selectors cleanly.

Device attribute updates are privileged under the current driver policy. Read-only queries may succeed with a read-only device open.

## OpGroup Discovery

The library exposes OpGroup information in two layers:

- SDXI 1.0 base-group decoding from the raw `caps.op_group_caps` mask: DMA and Admin are mandatory, Atomic is advertised by Full Atomic bit 3 or Minimal Atomic bit 5, and Interrupt by bit 4
- GUID-based `SDXI_ADMIN_DISC` discovery for SDXI 1.1+ devices

The helper set is:

- `sdxi_dev_admin_disc()`
- `sdxi_dev_opgroups_refresh()`
- `sdxi_dev_get_opgroups()`
- `sdxi_dev_get_op_type()`
- `sdxi_grp_name()`

AdminGrp remains special: it keeps its fixed descriptor type even when GUID-based discovery data is available.

GUID-based discovery is read-only and available to callers that can open the delegated device node. Discovering a group does not authorize a context to enable or submit it; the driver checks the raw `opb_000_enb` mask requested during context creation.

## Generic Vendor Transport

Device handles also anchor generic vendor-admin transport:

- `sdxi_dev_admin_vendor_probe()`
- `sdxi_dev_admin_vendor()`

Vendor specific helpers use this control-plane hook.

## Device Operation Privileges

Under the current driver policy, device-facing helpers split as follows:


| Operation class                      | Helpers                                                                                                                                                                                                              | Current expectation                                                                                                    |
| ------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------- |
| Discovery and read-mostly inspection | `sdxi_list_devs()`, `sdxi_root_*()`, `sdxi_dev_open()`, `sdxi_dev_get_info()`, `sdxi_dev_get_uapi_info()`, `sdxi_dev_has_uapi_cap()`, `sdxi_dev_get_dev_info_raw()`, `sdxi_dev_get_caps()`, `sdxi_dev_get_attr()`    | typically usable with an opened device fd; `sdxi_dev_open()` may return a read-only handle when write access is denied |
| Device attribute updates             | `sdxi_dev_set_attr()`                                                                                                                                                                                                | privileged (`CAP_SYS_ADMIN`)                                                                                           |
| Device-scoped RKey management        | `sdxi_dev_get_rkey_list()`, `sdxi_dev_get_rkeys()`, `sdxi_dev_get_rkey()`, `sdxi_dev_set_rkeys()`, `sdxi_dev_set_rkey()`, `sdxi_dev_set_rkeys_resume()`, `sdxi_dev_alloc_rkey_index()`, `sdxi_dev_invalidate_rkey()` | privileged (`CAP_SYS_ADMIN`)                                                                                           |
| GUID-based OpGroup discovery         | `sdxi_dev_admin_disc()`, `sdxi_dev_opgroups_refresh()`                                                                                                                                                               | available with delegated device-node access                                                                           |
| Vendor-admin transport               | `sdxi_dev_admin_vendor_probe()`, `sdxi_dev_admin_vendor()`, vendor-specific helpers                                                                                                                                  | authorized by the vendor handler; read-only operations may be allowed without `CAP_SYS_ADMIN`                                              |


`sdxi_dev_get_opgroups()` and `sdxi_dev_get_op_type()` still work on the locally cached base-group decode. Full GUID-based discovery for SDXI 1.1+ devices requires a successful refresh first.

## Device Handle Lifetime

`sdxi_root_t` owns only the cached inventory list for the latest scan. Destroying a root does not affect already opened `sdxi_dev_t` or `sdxi_cxt_t` objects.

`sdxi_dev_t` owns the opened device fd and lazily populated caches. It does not own contexts created from that device. `sdxi_cxt_t` owns its own context fd and mappings and can outlive the `sdxi_dev_t` used to create it.

Applications should still keep the device handle alive for clarity unless they have a reason to detach those lifetimes explicitly.
