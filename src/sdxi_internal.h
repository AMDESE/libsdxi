// SPDX-License-Identifier: BSD-3-Clause
#ifndef SDXI_INTERNAL_H
#define SDXI_INTERNAL_H

#include "libsdxi.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sdxi_abi.h"

enum sdxi_backend_kind {
	SDXI_BACKEND_LOCAL = 0,
};

struct sdxi_root_dev_ent {
	int pf;
	int vf;
	char *path;
};

struct sdxi_root {
	enum sdxi_backend_kind backend;
	struct sdxi_root_dev_ent *devs;
	size_t ndevs;
};

struct sdxi_dev {
	int fd;
	char *path;
	bool read_only;

	bool dev_info_valid;
	struct sdxi_dev_info_raw dev_info;

	bool caps_valid;
	struct sdxi_dev_caps caps;

	bool uapi_info_valid;
	struct sdxi_uapi_info uapi_info;

	/* OpGroup cache (op_type -> grp_id and reverse). */
	bool opgroups_base_valid;

	enum sdxi_grp_id grp_by_type[SDXI_DISC_MAX_INDEX + 1];
	uint16_t type_by_grp[SDXI_GRP__MAX];

	bool disc_tried;
	bool disc_valid;
	int disc_err;
	uint16_t disc_max_index;
	uint8_t guid_by_type[SDXI_DISC_MAX_INDEX + 1][16];
};

struct sdxi_cxt {
	int fd; /* context fd returned by SDXI_CREATE_CXT */
	uint32_t pasid;
	bool created;
	bool quiesced;

	uint32_t ds_ring_entries;
	uint32_t opb_000_enb;

	uint16_t intr_akey;
	uint32_t intr_num;

	int eventfd;
	bool eventfd_owned;

	struct sdxi_cxt_mmaps mmaps;

	void *csbs;
	size_t csbs_bytes;

	uint64_t prod_reserve;
	uint64_t prod_commit;
};

int sdxi_cxt_submit_public_desc(sdxi_cxt_t *cxt,
				const struct sdxi_dp_desc *raw,
				uint32_t timeout_ms);

#endif /* SDXI_INTERNAL_H */
