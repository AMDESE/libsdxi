// SPDX-License-Identifier: BSD-3-Clause
/**
 * DOC: libsdxi root header
 *
 * Root helpers provide local device discovery and inventory caching. The
 * current implementation supports only the local backend; daemon-backed
 * inventory is left for future.
 */

#ifndef SDXI_ROOT_H
#define SDXI_ROOT_H

#include <sdxi/types.h>

SDXI_BEGIN_DECLS

#define SDXI_ROOT_URI_LOCAL "sdxi:///local"

struct sdxi_root_opts {
	uint32_t size;
	const char *uri;
	uint32_t flags;
	uint32_t rsvd[8];
};

SDXI_EXPORT int sdxi_root_create(const struct sdxi_root_opts *opts,
				 sdxi_root_t **root_out);
SDXI_EXPORT void sdxi_root_destroy(sdxi_root_t *root);

SDXI_EXPORT int sdxi_root_scan(sdxi_root_t *root);
SDXI_EXPORT size_t sdxi_root_dev_count(const sdxi_root_t *root);
SDXI_EXPORT const char *sdxi_root_dev_path(const sdxi_root_t *root,
					   size_t index);
SDXI_EXPORT int sdxi_root_dev_open(const sdxi_root_t *root, size_t index,
				   sdxi_dev_t **dev_out);

SDXI_END_DECLS

#endif /* SDXI_ROOT_H */
