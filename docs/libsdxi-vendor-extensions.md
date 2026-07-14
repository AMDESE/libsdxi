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

## Current Packaging Expectation

The generic transport stays in the core header set. Typed vendor helpers live in explicit vendor headers under `sdxi/vendor/`, so they can remain in the main package or later move to companion packages without an include-tree rewrite.
