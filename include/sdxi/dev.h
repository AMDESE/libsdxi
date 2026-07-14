// SPDX-License-Identifier: BSD-3-Clause
/**
 * DOC: libsdxi device-management API
 *
 * Device-management helpers cover device-node enumeration, open/close,
 * cached UAPI negotiation, capability and attribute queries, and OpGroup
 * discovery on an opened SDXI function.
 */

#ifndef SDXI_DEV_H
#define SDXI_DEV_H

#include <sdxi/types.h>

SDXI_BEGIN_DECLS

SDXI_EXPORT int sdxi_list_devs(char ***paths_out, size_t *count_out);
SDXI_EXPORT void sdxi_free_dev_list(char **paths, size_t count);

SDXI_EXPORT int sdxi_dev_open(const char *path, sdxi_dev_t **dev_out);
SDXI_EXPORT void sdxi_dev_close(sdxi_dev_t *dev);

SDXI_EXPORT const char *sdxi_dev_path(const sdxi_dev_t *dev);
SDXI_EXPORT int sdxi_dev_get_fd(const sdxi_dev_t *dev);

#define SDXI_UAPI_MAJOR_SUPPORTED 1U
#define SDXI_UAPI_INFO_CAP_WORDS 4U

enum sdxi_uapi_cap_bits {
	SDXI_UAPI_INFO_CAP_DEV_INFO = 1ULL << 0,
	SDXI_UAPI_INFO_CAP_DEV_ATTR_MGMT = 1ULL << 1,
	SDXI_UAPI_INFO_CAP_RKEY_MGMT = 1ULL << 2,
	SDXI_UAPI_INFO_CAP_CXT_CREATE = 1ULL << 3,
	SDXI_UAPI_INFO_CAP_CXT_EVENTFD = 1ULL << 4,
	SDXI_UAPI_INFO_CAP_AKEY_MGMT = 1ULL << 5,
	SDXI_UAPI_INFO_CAP_ADMIN_DISC = 1ULL << 6,
	SDXI_UAPI_INFO_CAP_VENDOR_OP = 1ULL << 7,
};

struct sdxi_uapi_info {
	uint16_t major;
	uint16_t minor;
	uint16_t cap_words;
	uint64_t caps[SDXI_UAPI_INFO_CAP_WORDS];
};

SDXI_EXPORT int sdxi_dev_get_uapi_info(sdxi_dev_t *dev,
				       struct sdxi_uapi_info *out);
SDXI_EXPORT bool sdxi_dev_has_uapi_cap(sdxi_dev_t *dev, uint64_t cap);

struct sdxi_dev_info_raw {
	uint16_t hw_major;
	uint16_t hw_minor;
	uint64_t cap0;
	uint64_t cap1;
	uint64_t sts0;
};

struct sdxi_dev_caps {
	uint8_t ver_major;
	uint8_t ver_minor;
	uint16_t sfunc;
	bool is_vf;
	uint8_t cs_cap;

	uint32_t db_stride_bytes;
	/* Raw MMIO_CAP1.opb_000_cap bits, including Minimal Atomic at bit 5. */
	uint32_t op_group_caps;

	bool rkey_supported;
	bool rm_required;
	bool mmio_64bit;

	uint64_t max_ds_ring_entries;
	uint64_t max_buffer_bytes;
	uint32_t max_rkey_entries;
	uint32_t max_akey_entries;
	uint64_t max_errlog_entries;
	uint32_t max_cxt_num;
};

#define SDXI_DEV_INFO_F_READ_ONLY (1U << 0)

/*
 * Normalized device snapshot: one serialization-friendly result struct that
 * keeps the cached open-time UAPI view together with the raw device info and
 * the library-decoded capability summary. On success, libsdxi fills the
 * struct, sets size to sizeof(struct sdxi_dev_info), and zeroes rsvd[].
 */
struct sdxi_dev_info {
	uint32_t size;
	uint32_t flags;

	struct sdxi_uapi_info uapi;
	struct sdxi_dev_info_raw raw;
	struct sdxi_dev_caps caps;

	uint32_t rsvd[8];
};

SDXI_EXPORT int sdxi_dev_get_info(sdxi_dev_t *dev, struct sdxi_dev_info *out);
SDXI_EXPORT int sdxi_dev_get_dev_info_raw(sdxi_dev_t *dev,
					  struct sdxi_dev_info_raw *out);
SDXI_EXPORT int sdxi_dev_get_caps(sdxi_dev_t *dev, struct sdxi_dev_caps *out);

enum sdxi_dev_attr_id {
	SDXI_DEV_ATTR_ID_GSR = 1,
	SDXI_DEV_ATTR_ID_GRP_ID = 2,
	SDXI_DEV_ATTR_ID_MAX_BUFFER = 3,
	SDXI_DEV_ATTR_ID_MAX_AKEY_SZ = 4,
	SDXI_DEV_ATTR_ID_MAX_RKEY_SZ = 5,
	SDXI_DEV_ATTR_ID_MAX_CXT = 6,
};

SDXI_EXPORT const char *sdxi_dev_attr_name(enum sdxi_dev_attr_id attr);
SDXI_EXPORT int sdxi_dev_attr_by_name(const char *name,
				      enum sdxi_dev_attr_id *attr_out);
SDXI_EXPORT bool sdxi_dev_attr_is_writable(enum sdxi_dev_attr_id attr);
SDXI_EXPORT int sdxi_dev_get_attr(sdxi_dev_t *dev,
				  enum sdxi_dev_attr_id attr,
				  uint64_t *val_out);
SDXI_EXPORT int sdxi_dev_set_attr(sdxi_dev_t *dev,
				  enum sdxi_dev_attr_id attr,
				  uint64_t *val_inout);

#define SDXI_DISC_RECORD_BYTES 16u
#define SDXI_DISC_MAX_INDEX 2047u

enum sdxi_grp_id {
	SDXI_GRP_UNKNOWN = 0,

	SDXI_GRP_DMA,
	SDXI_GRP_ADMIN,
	SDXI_GRP_ATOMIC,
	SDXI_GRP_INTR,

	SDXI_GRP_CRCM1,
	SDXI_GRP_XDMA,
	SDXI_GRP_DCOPY,

	SDXI_GRP__MAX,
};

SDXI_EXPORT const char *sdxi_grp_name(enum sdxi_grp_id grp);

struct sdxi_opgroup_info {
	uint16_t op_type;
	uint16_t rsvd0;
	uint32_t grp_id;
	uint8_t guid[16];
};

SDXI_EXPORT int sdxi_dev_admin_disc(sdxi_dev_t *dev, uint16_t index_start,
				    uint16_t index_end, void *buf,
				    size_t *buf_bytes_inout,
				    uint32_t timeout_ms,
				    struct sdxi_admin_cpl *cpl_out);
SDXI_EXPORT int sdxi_dev_opgroups_refresh(sdxi_dev_t *dev,
					  uint32_t timeout_ms);
SDXI_EXPORT int sdxi_dev_get_opgroups(sdxi_dev_t *dev,
				      struct sdxi_opgroup_info **out,
				      size_t *count_out);
SDXI_EXPORT void sdxi_free_opgroups(struct sdxi_opgroup_info *list);
SDXI_EXPORT int sdxi_dev_get_op_type(sdxi_dev_t *dev, enum sdxi_grp_id grp,
				     uint32_t timeout_ms,
				     uint16_t *op_type_out);

SDXI_END_DECLS

#endif /* SDXI_DEV_H */
