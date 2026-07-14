# libsdxi Overview

`libsdxi` is a local userspace C library for the SDXI Linux character-device UAPI. It wraps the driver ioctl contract and exposes a public API in installed headers under `sdxi/`.

## Current Object Model

The library exposes one inventory handle and two runtime handles:

- `sdxi_root_t`:
local device inventory with stable scan ordering.
- `sdxi_dev_t`:
one opened SDXI PF or VF device node.
- `sdxi_cxt_t`:
one local userspace context with its own context fd and mappings.

## Header Layout

Applications may include:

- `#include <libsdxi.h>` for the umbrella header, or
- narrower headers such as `sdxi/dev.h`, `sdxi/cxt.h`, `sdxi/key.h`, `sdxi/dp.h`, `sdxi/vendor/amd_qos.h`, and `sdxi/vendor/amd_crypto.h`.

The public header split is:

- `sdxi/types.h`: version macros, opaque handles, export annotations, common POD types
- `sdxi/root.h`: local inventory object, scan/rescan, and root-based open
- `sdxi/util.h`: generic helpers such as `sdxi_err_str()`
- `sdxi/dev.h`: device discovery, open/close, caps, attrs, OpGroup discovery
- `sdxi/cxt.h`: context lifecycle and PASID/control-plane helpers
- `sdxi/dp.h`: advanced local dataplane accessors, helpers, and request structs
- `sdxi/event.h`: context eventfd introspection
- `sdxi/key.h`: shared RKey/AKey data model and helpers
- `sdxi/vendor.h`: generic admin-vendor transport
- `sdxi/vendor/amd_qos.h`: AMD QoS admin helpers
- `sdxi/vendor/amd_crypto.h`: AMD S1/S2 crypto dataplane helper types and helpers

Internal UAPI and descriptor-building headers remain private to the build.

## Control Plane and Dataplane Split

The API is split into two halves:

- control plane: device open/close, capability and attribute queries, key management, discovery, generic vendor-admin helpers, typed AMD QoS helpers, and context lifecycle
- local dataplane: raw local context-fd/mmap accessors and high-level helper operations such as `sdxi_mem_copy()`,  `sdxi_write_imm()`, `sdxi_crc_m1()`, and typed AMD S1/S2 helpers

The dataplane is explicitly local. `sdxi_cxt_t` owns mmaps and a real context fd, so raw fd and mmap accessors live in `sdxi/dp.h` rather than `sdxi/cxt.h`.

## Open-Time UAPI Negotiation

`sdxi_dev_open()` negotiates the kernel UAPI once at open time by issuing `SDXI_GET_UAPI_INFO`. Major-version mismatch fails the open with `-EPROTONOSUPPORT`, and lack of the negotiation ioctl is treated as an unsupported-kernel error.

Later library calls do not require applications to repeat ABI negotiation.

## Minimal Discovery and Lifecycle

```c
	sdxi_root_t *root = NULL;
	sdxi_dev_t *dev = NULL;
	sdxi_cxt_t *cxt = NULL;
	struct sdxi_cxt_create_opts opts = {
	        .size = sizeof(opts),
	};

	ret = sdxi_root_create(NULL, &root);
	if (ret)
		return ret;

	ret = sdxi_root_scan(root);
	if (ret)
		goto out_root;

	if (sdxi_root_dev_count(root) == 0) {
		ret = -ENODEV;
		goto out_root;
	}

	ret = sdxi_root_dev_open(root, 0, &dev);
	if (ret)
		goto out_root;

	ret = sdxi_cxt_create(dev, &opts, &cxt);
	if (ret)
		goto out_dev;

	ret = sdxi_mem_copy(cxt, dst, src, len, timeout_ms);

	sdxi_cxt_destroy(cxt);
out_dev:
	sdxi_dev_close(dev);
out_root:
	sdxi_root_destroy(root);
```

## Current Scope

The public surface is intentionally small and local:

- no daemon-backed backend yet
- only a local root inventory object; no remote backend
- no transparent remote dataplane

The installed headers and manuals leave room for future additions without a broad include-tree rewrite.
