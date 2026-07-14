# libsdxi

`libsdxi` is a local userspace C library for the Linux SDXI software stack. It wraps the SDXI character-device UAPI and provides public helpers for device discovery, capability queries, key management, context lifecycle, local dataplane submission, and selected vendor extensions.

## Contents

- [Supported Environment](#supported-environment)
- [Build Requirements](#build-requirements)
- [Build, Test, and Install](#build-test-and-install)
- [Quick Start](#quick-start)
- [Using pkg-config](#using-pkg-config)
- [Runtime Requirements](#runtime-requirements)
- [Samples](#samples)
- [Documentation](#documentation)
- [License and Contributions](#license-and-contributions)

## Supported Environment

`libsdxi` currently targets Linux systems with an SDXI kernel driver exposing device nodes under `/dev`.

SDXI devices are presented as `/dev/sdxiM` for physical functions (PFs) and `/dev/sdxiMvN` for virtual functions (VFs), where `M` and `N` are integers.

Examples:

- `/dev/sdxi0`: physical function 0
- `/dev/sdxi1`: physical function 1
- `/dev/sdxi0v0`: virtual function 0 on PF 0
- `/dev/sdxi1v1`: virtual function 1 on PF 1

## Build Requirements

From a git checkout, the build requires:

- C compiler such as `gcc` or `clang`
- `make`
- `autoconf`
- `automake`
- `libtool`
- `pkg-config`

Release tarballs include generated autotools files, so users building from a tarball can normally start at `./configure`.

## Build, Test, and Install

From a git checkout:

```sh
./autogen.sh
./configure
make
make check
```

From a release tarball:

```sh
./configure
make
make check
```

Install with:

```sh
sudo make install
```

Before publishing a release tarball, run:

```sh
make distcheck
```

## Quick Start

This example opens the first discovered SDXI device, creates a context, and submits one local memory copy.

```c
#include <libsdxi.h>

#include <stdlib.h>
#include <string.h>

int main(void)
{
	sdxi_root_t *root = NULL;
	sdxi_dev_t *dev = NULL;
	sdxi_cxt_t *cxt = NULL;
	void *src = NULL;
	void *dst = NULL;
	size_t len = 4096;
	int ret;

	ret = posix_memalign(&src, 4096, len);
	if (ret)
		return ret;
	ret = posix_memalign(&dst, 4096, len);
	if (ret) {
		free(src);
		return ret;
	}

	memset(src, 0xaf, len);
	memset(dst, 0x00, len);

	ret = sdxi_root_create(NULL, &root);
	if (ret)
		goto out;

	ret = sdxi_root_scan(root);
	if (ret)
		goto out;
	if (sdxi_root_dev_count(root) == 0) {
		ret = -1;
		goto out;
	}

	ret = sdxi_root_dev_open(root, 0, &dev);
	if (ret)
		goto out;

	ret = sdxi_cxt_create(dev, NULL, &cxt);
	if (ret)
		goto out;

	ret = sdxi_mem_copy(cxt, dst, src, len, 5000);
	if (ret)
		goto out;

out:
	(void)sdxi_cxt_destroy(cxt);
	sdxi_dev_close(dev);
	sdxi_root_destroy(root);
	free(src);
	free(dst);
	return ret ? 1 : 0;
}
```

## Using pkg-config

After installation, compile applications with:

```sh
cc myapp.c $(pkg-config --cflags --libs libsdxi)
```

Applications may include the umbrella header:

```c
#include <libsdxi.h>
```

or narrower headers under `sdxi/`, for example:

```c
#include <sdxi/dev.h>
#include <sdxi/cxt.h>
#include <sdxi/dp.h>
```

Vendor-specific helpers are explicit includes under `sdxi/vendor/`, such as:

```c
#include <sdxi/vendor/amd_qos.h>
#include <sdxi/vendor/amd_crypto.h>
```

## Runtime Requirements

Applications need permission to open the selected `/dev/sdxi*` node. Depending on the system policy, this may require running as root or being in the group that owns the SDXI device node.

Context creation allocates and locks userspace ring/status memory with `mlock()`. If `sdxi_cxt_create()` fails with `ENOMEM` even though the device can be opened, check the process locked-memory limit:

```sh
ulimit -l
```

Increase the limit or run with a policy/capability such as `CAP_IPC_LOCK` when needed.

## Samples

Runnable examples live in `samples/`. Build them with:

```sh
cd samples
make
```

Common samples include:

- `memcopy`: local memory copy on one SDXI device
- `repcopy`: repeated-copy dataplane operation
- `xcopy`: cross-function memory copy using two SDXI devices/functions
- `uadd`: atomic unsigned add
- `write_imm`: write-immediate descriptor
- `cxt_intr`: context interrupt delivery
- `opgroups`: OpGroup discovery
- `crc`: SDXI CRC Mode1 operations
- `amd_aes`: AMD AES operations
- `amd_sha`: AMD SHA operations
- `amd_qos`: AMD QoS capability query

Most samples accept `-dev` or `--dev` to choose a device:

```sh
./memcopy --dev /dev/sdxi0
```

If no device is specified, samples use the first device from a stable sorted `/dev/sdxi*` scan. `xcopy` is different: it needs a source and destination function, and accepts `--src-dev` and `--dst-dev`.

## Documentation

The detailed documentation lives under `docs/`:

- `libsdxi-overview.md`: public object model and header layout
- `libsdxi-device.md`: device discovery, capabilities, and attributes
- `libsdxi-context-dataplane.md`: context lifecycle and dataplane helpers
- `libsdxi-key-management.md`: RKey and AKey helper model
- `libsdxi-event-handling.md`: eventfd and interrupt helpers
- `libsdxi-vendor-extensions.md`: AMD QoS and AMD crypto helpers
- `libsdxi-contracts.md`: return codes, lifetime, and thread-safety

Manual pages are under `docs/man/` and are installed into the normal manpath. Start with:

```sh
man SDXI
man sdxi_dev
man sdxi_cxt
man sdxi_dp
```

Markdown manuals are installed under `${prefix}/share/doc/libsdxi/`.

## License and Contributions

`libsdxi` is distributed under the BSD 3-Clause License. See `LICENSE`.

Report issues and send patches through the project repository. For useful bug reports, include the libsdxi version, SDXI driver/kernel version, device node, command output, etc.
