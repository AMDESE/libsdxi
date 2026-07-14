// SPDX-License-Identifier: BSD-3-Clause
/**
 * DOC: libsdxi context-management API
 *
 * Context-management helpers cover context creation, quiesce/destroy, and the
 * stable control-plane state that belongs to a local userspace context.
 *
 * Raw context-fd and mmap accessors live in <sdxi/dp.h> because they are part
 * of the explicit local dataplane surface rather than the control-plane API.
 */

#ifndef SDXI_CXT_H
#define SDXI_CXT_H

#include <sdxi/types.h>

SDXI_BEGIN_DECLS

enum sdxi_cxt_create_flags {
	SDXI_CXT_CREATE_F_EVENTFD = 1U << 0,
};

#define SDXI_CXT_QUIESCE_TIMEOUT_DEFAULT 0U

struct sdxi_cxt_create_opts {
	uint32_t size;

	uint32_t flags;

	uint32_t ds_ring_entries;
	/*
	 * Raw opb_000 mask: bit N enables opcode type N (0..31); 0 uses
	 * the driver default. Discover function-assigned types with
	 * sdxi_dev_admin_disc(); the driver authorizes the requested groups.
	 */
	uint32_t opb_000_enb;

	int eventfd;

	uint32_t rsvd[8];
};

/*
 * Passing NULL opts to sdxi_cxt_create() selects the library default ring size.
 * The current SDXI_CREATE_CXT UAPI always uses userspace-provided ring state.
 */
SDXI_EXPORT int sdxi_cxt_create(sdxi_dev_t *dev,
				const struct sdxi_cxt_create_opts *opts,
				sdxi_cxt_t **cxt_out);
SDXI_EXPORT int sdxi_cxt_quiesce(sdxi_cxt_t *cxt, uint32_t timeout_ms,
				 uint32_t *cst_flags_out);
SDXI_EXPORT int sdxi_cxt_destroy(sdxi_cxt_t *cxt);

SDXI_EXPORT uint32_t sdxi_cxt_pasid(const sdxi_cxt_t *cxt);

SDXI_END_DECLS

#endif /* SDXI_CXT_H */
