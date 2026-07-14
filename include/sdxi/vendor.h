// SPDX-License-Identifier: BSD-3-Clause
/**
 * DOC: libsdxi vendor and admin helpers
 *
 * Generic typed admin-vendor transport lives here.
 *
 * Typed vendor schemas and helper families live in vendor-specific installed
 * headers under `sdxi/vendor/` so the core header remains vendor-neutral.
 */

#ifndef SDXI_VENDOR_H
#define SDXI_VENDOR_H

#include <sdxi/types.h>

SDXI_BEGIN_DECLS

#define SDXI_ADMIN_TIMEOUT_DEFAULT 0U

#define SDXI_VENDOR_GUID_BYTES 16U
#define SDXI_ADMIN_VENDOR_PROBE 0U

SDXI_EXPORT int sdxi_dev_admin_vendor_probe(
	sdxi_dev_t *dev, const uint8_t guid[SDXI_VENDOR_GUID_BYTES],
	uint32_t timeout_ms, struct sdxi_admin_cpl *cpl_out);
SDXI_EXPORT int sdxi_dev_admin_vendor(
	sdxi_dev_t *dev, const uint8_t guid[SDXI_VENDOR_GUID_BYTES],
	uint32_t op, const void *in, size_t in_size, void *out,
	size_t *out_bytes_inout, uint32_t timeout_ms,
	struct sdxi_admin_cpl *cpl_out);

SDXI_END_DECLS

#endif /* SDXI_VENDOR_H */
