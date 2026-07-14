// SPDX-License-Identifier: BSD-3-Clause
#ifndef SDXI_ABI_H
#define SDXI_ABI_H

#include <stddef.h>
#include <stdint.h>

#include "libsdxi.h"

struct sdxi_abi_cxt_create_req {
	uint32_t opb_000_enb;

	uint32_t ds_ring_entries;
	uint64_t ds_ring_base;
	uint64_t cxt_sts_base;
	uint64_t write_index_base;
};

struct sdxi_abi_cxt_create_resp {
	uint32_t pasid;
	int cxt_fd;

	uint32_t ds_ring_entries;

	uint64_t doorbell_mmap_base;
};

int sdxi_abi_get_dev_info(int fd, struct sdxi_dev_info_raw *out);

/*
 * Per-attribute device attribute helpers backed by SDXI_{GET,SET}_DEV_ATTR.
 *
 * - GET writes the current value into *val_out.
 * - SET passes *val_inout to the kernel, then overwrites it with the value
 *   the kernel actually applied (the UAPI is IOWR for both).
 * - Both wrappers map the kernel's -EOPNOTSUPP onto -ENOTSUP so callers can
 *   probe for unimplemented selectors / read-only attributes uniformly.
 */
int sdxi_abi_get_dev_attr(int fd, uint64_t attr, uint64_t *val_out);
int sdxi_abi_set_dev_attr(int fd, uint64_t attr, uint64_t *val_inout);
int sdxi_abi_get_uapi_info(int fd, struct sdxi_uapi_info *out);

int sdxi_abi_get_rkey_list(int fd, struct sdxi_bitmap *out);
int sdxi_abi_get_rkeys(int fd, struct sdxi_key_ent *ents, size_t nents);
int sdxi_abi_set_rkeys(int fd, const struct sdxi_key_ent *ents, size_t nents,
		       uint32_t timeout_ms, struct sdxi_key_set_result *res);

int sdxi_abi_create_cxt(int fd, const struct sdxi_abi_cxt_create_req *req,
			struct sdxi_abi_cxt_create_resp *resp);
int sdxi_abi_cxt_eventfd(int cxt_fd, int eventfd, uint16_t *intr_akey_out,
			 uint32_t *intr_num_out);
int sdxi_abi_quiesce_cxt(int cxt_fd, uint32_t timeout_ms,
			 uint32_t *cst_flags_out);

int sdxi_abi_get_akey_list(int cxt_fd, struct sdxi_bitmap *out);
int sdxi_abi_get_akeys(int cxt_fd, struct sdxi_key_ent *ents, size_t nents);
int sdxi_abi_set_akeys(int cxt_fd, const struct sdxi_key_ent *ents,
		       size_t nents, uint32_t timeout_ms,
		       struct sdxi_key_set_result *res);

int sdxi_abi_admin_disc(int fd, uint16_t index_start, uint16_t index_end,
			void *buf, uint32_t *buf_bytes_inout,
			uint32_t timeout_ms, struct sdxi_admin_cpl *cpl_out);

int sdxi_abi_admin_vendor(int fd,
			  const uint8_t guid[SDXI_VENDOR_GUID_BYTES],
			  uint32_t op, const void *in, uint32_t in_size,
			  void *out, uint32_t *out_bytes_inout,
			  uint32_t timeout_ms, struct sdxi_admin_cpl *cpl_out);

#endif /* SDXI_ABI_H */
