# libsdxi Vendor Extensions Guide

Vendor support has two layers:

- a generic typed admin-vendor transport in `sdxi/vendor.h`
- typed vendor headers under `sdxi/vendor/`

## Generic Typed Vendor Transport

`sdxi_dev_admin_vendor_probe()` asks whether a device has a registered handler for a vendor GUID.

`sdxi_dev_admin_vendor()` submits:

- a vendor GUID
- a vendor-defined operation id
- an optional structured input payload
- an optional output buffer with size-in/size-out semantics

This path is for control-plane vendor operations. The kernel owns admin descriptor construction and execution details.

## AMD QoS

The typed AMD QoS control-plane header is `sdxi/vendor/amd_qos.h`.

Helper set:

- `sdxi_amd_qos_get_cap()`
- `sdxi_amd_qos_dev_update()`
- `sdxi_amd_qos_pool_update()`
- `sdxi_amd_qos_fn_update()`
- `sdxi_amd_qos_tel()`

AMD QoS helpers are PF-only. The library rejects VF targets before issuing the vendor transport call.

Telemetry buffer sizing helpers:

- `sdxi_amd_qos_tel_active_pools_bytes()`
- `sdxi_amd_qos_tel_pool_report_bytes()`

## AMD S1/S2 Crypto

AMD S1 and S2 crypto are represented in the public API as local dataplane helpers in `sdxi/vendor/amd_crypto.h`, not as admin-vendor functions:

- `sdxi_amd_s1_aes_xts()`
- `sdxi_amd_s1_aes_ecb()`
- `sdxi_amd_s2_aes_xts_c()`
- `sdxi_amd_s2_aes_ecb_c()`
- `sdxi_amd_s2_sha()`
- `sdxi_amd_s2_sha_copy()`
- `sdxi_amd_s2_aes_ctr()`
- `sdxi_amd_s2_aes_gcm()`
- `sdxi_amd_s2_aes_ctr_sha()`
- `sdxi_amd_s2_aes_xts_sha()`

These helpers depend on function-assigned OpGroup `type` values discovered from the device, so vendor bring-up spans both device discovery and dataplane use.

## Current Packaging Expectation

The generic transport stays in the core header set. Typed AMD helpers live in explicit vendor headers under `sdxi/vendor/`, so they can remain in the main package or later move to a companion package such as `libsdxi-amd` without an include-tree rewrite.
