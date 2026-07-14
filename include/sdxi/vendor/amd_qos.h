// SPDX-License-Identifier: BSD-3-Clause
/**
 * DOC: libsdxi AMD QoS vendor helpers
 *
 * This header contains the typed AMD QoS control-plane helper family layered
 * on top of the generic vendor-admin transport in <sdxi/vendor.h>.
 *
 * Applications must include this header explicitly when they use AMD QoS; the
 * generic core headers intentionally do not pull it in automatically.
 */

#ifndef SDXI_VENDOR_AMD_QOS_H
#define SDXI_VENDOR_AMD_QOS_H

#include <sdxi/vendor.h>

SDXI_BEGIN_DECLS

#define SDXI_AMD_QOS_CAP_BYTES 48u
#define SDXI_AMD_QOS_TEL_FIXED_BYTES 48u
#define SDXI_AMD_QOS_TEL_HDR_BYTES 48u
#define SDXI_AMD_QOS_TEL_POOL_REPORT_ENTRY_BYTES 132u
#define SDXI_AMD_QOS_TEL_MAX_BYTES (1024u * 1024u)

enum sdxi_amd_qos_dsc_subtype {
	SDXI_AMD_QOS_DSC_SUBTYPE_CAP = 0x00,
	SDXI_AMD_QOS_DSC_SUBTYPE_DEVUPD = 0x01,
	SDXI_AMD_QOS_DSC_SUBTYPE_POOLUPD = 0x02,
	SDXI_AMD_QOS_DSC_SUBTYPE_FNUPD = 0x03,
	SDXI_AMD_QOS_DSC_SUBTYPE_TEL = 0x04,
};

enum sdxi_amd_qos_feature_flags {
	SDXI_AMD_QOS_FEATURE_DYNAMIC_POOL_RECONFIG = 1u << 0,
	SDXI_AMD_QOS_FEATURE_DYNAMIC_POOL_REASSIGN = 1u << 1,
	SDXI_AMD_QOS_FEATURE_TEL_TIMESTAMPS = 1u << 2,
	SDXI_AMD_QOS_FEATURE_FLEXIBLE_POOL_SCOPE = 1u << 3,
};

enum sdxi_amd_qos_pool_op {
	SDXI_AMD_QOS_POOL_OP_CREATE = 0,
	SDXI_AMD_QOS_POOL_OP_UPDATE = 1,
	SDXI_AMD_QOS_POOL_OP_DESTROY = 2,
};

enum sdxi_amd_qos_tel_qtype {
	SDXI_AMD_QOS_TEL_QTYPE_DEVICE = 0,
	SDXI_AMD_QOS_TEL_QTYPE_POOL_CONFIG = 1,
	SDXI_AMD_QOS_TEL_QTYPE_POOL_ASSIGNMENT = 2,
	SDXI_AMD_QOS_TEL_QTYPE_ACTIVE_POOLS = 3,
	SDXI_AMD_QOS_TEL_QTYPE_POOL_REPORT = 4,
	SDXI_AMD_QOS_TEL_QTYPE_POOL_VARIANCE = 5,
};

enum sdxi_amd_qos_ts_units {
	SDXI_AMD_QOS_TS_UNITS_PS = 0,
	SDXI_AMD_QOS_TS_UNITS_NS = 1,
	SDXI_AMD_QOS_TS_UNITS_US = 2,
	SDXI_AMD_QOS_TS_UNITS_MS = 3,
};

struct sdxi_amd_qos_cap {
	uint16_t qos_version;
	uint16_t feature_flags;

	uint32_t device_maxbw_cap_mb_s;
	uint16_t max_pools;

	uint32_t granularity_bw_mb_s;
	uint32_t variance_bw_mb_s;

	uint8_t timestamp_units;
	uint32_t timestamp_resolution;
};

struct sdxi_amd_qos_tel_target {
	uint16_t pool_num;
	uint16_t vf_num;
	uint16_t cxt_num;
	bool cxt;
	bool vf;
};

SDXI_EXPORT int sdxi_amd_qos_get_cap(sdxi_dev_t *dev, uint32_t timeout_ms,
				     struct sdxi_amd_qos_cap *cap_out,
				     struct sdxi_admin_cpl *cpl_out);
SDXI_EXPORT int sdxi_amd_qos_dev_update(sdxi_dev_t *dev,
					uint32_t device_maxbw_mb_s,
					uint32_t timeout_ms,
					struct sdxi_admin_cpl *cpl_out);
SDXI_EXPORT int sdxi_amd_qos_pool_update(sdxi_dev_t *dev, uint16_t pool_num,
					 uint8_t pool_operation,
					 uint32_t pool_maxbw_mb_s,
					 uint32_t pool_minbw_mb_s,
					 uint32_t timeout_ms,
					 struct sdxi_admin_cpl *cpl_out);
SDXI_EXPORT int sdxi_amd_qos_fn_update(sdxi_dev_t *dev, uint16_t pool_num,
				       bool cxt, bool vf, uint16_t vf_num,
				       uint16_t cxt_start, uint16_t cxt_end,
				       uint32_t timeout_ms,
				       struct sdxi_admin_cpl *cpl_out);
SDXI_EXPORT int sdxi_amd_qos_tel(sdxi_dev_t *dev, uint8_t query_type,
				 const struct sdxi_amd_qos_tel_target *target,
				 void *buf, size_t *buf_bytes_inout,
				 uint32_t timeout_ms,
				 struct sdxi_admin_cpl *cpl_out);
SDXI_EXPORT size_t sdxi_amd_qos_tel_active_pools_bytes(uint16_t max_pools);
SDXI_EXPORT size_t sdxi_amd_qos_tel_pool_report_bytes(
	uint16_t assigned_functions);

SDXI_END_DECLS

#endif /* SDXI_VENDOR_AMD_QOS_H */
