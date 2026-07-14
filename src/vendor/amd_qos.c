// SPDX-License-Identifier: BSD-3-Clause
#include "../sdxi_internal.h"

#include <errno.h>
#include <string.h>

#include <sdxi/vendor/amd_qos.h>

/*
 * SDXI_ADMIN_VENDOR reserves op 0 for probing. The current typed AMD QoS
 * helper contract uses consecutive op ids for the descriptor classes the
 * kernel composes internally.
 */
enum sdxi_amd_qos_vendor_op {
	SDXI_AMD_QOS_VENDOR_OP_CAP = 1,
	SDXI_AMD_QOS_VENDOR_OP_DEVUPD = 2,
	SDXI_AMD_QOS_VENDOR_OP_POOLUPD = 3,
	SDXI_AMD_QOS_VENDOR_OP_FNUPD = 4,
	SDXI_AMD_QOS_VENDOR_OP_TEL = 5,
};

struct sdxi_amd_qos_devupd_req {
	uint32_t device_maxbw_mb_s;
	uint32_t rsvd0[3];
};

struct sdxi_amd_qos_poolupd_req {
	uint32_t pool_num;
	uint32_t pool_operation;
	uint32_t pool_maxbw_mb_s;
	uint32_t pool_minbw_mb_s;
	uint32_t rsvd0[4];
};

struct sdxi_amd_qos_fnupd_req {
	uint32_t pool_num;
	uint32_t cxt;
	uint32_t vf;
	uint32_t vf_num;
	uint32_t cxt_start;
	uint32_t cxt_end;
	uint32_t rsvd0[2];
};

struct sdxi_amd_qos_tel_req {
	uint32_t query_type;
	uint32_t pool_num;
	uint32_t cxt;
	uint32_t vf;
	uint32_t vf_num;
	uint32_t cxt_num;
	uint32_t rsvd0[2];
};

static const uint8_t sdxi_amd_qos_guid[SDXI_VENDOR_GUID_BYTES] = {
	0x0b, 0x61, 0xb7, 0xf5, 0xc6, 0x0d, 0x03, 0x4c,
	0xab, 0x79, 0x65, 0xf1, 0xf4, 0xef, 0xcf, 0x4c,
};

static uint16_t sdxi_le16_dec(const void *p)
{
	const uint8_t *b = p;

	return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static uint32_t sdxi_le32_dec(const void *p)
{
	const uint8_t *b = p;

	return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
	       ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static void sdxi_amd_qos_decode_cap(struct sdxi_amd_qos_cap *out,
				      const uint8_t cap[SDXI_AMD_QOS_CAP_BYTES])
{
	uint32_t legacy_max_pools_dw;
	uint32_t legacy_granularity_dw;
	uint32_t legacy_ts_dw;
	uint32_t typed_version_dw;
	uint32_t typed_feature_dw;
	uint32_t typed_max_pools_dw;
	uint32_t typed_granularity_dw;
	uint32_t typed_ts_dw;
	int use_typed_layout;

	legacy_max_pools_dw = sdxi_le32_dec(&cap[8]);
	legacy_granularity_dw = sdxi_le32_dec(&cap[12]);
	legacy_ts_dw = sdxi_le32_dec(&cap[40]);

	typed_version_dw = sdxi_le32_dec(&cap[0]);
	typed_feature_dw = sdxi_le32_dec(&cap[12]);
	typed_max_pools_dw = sdxi_le32_dec(&cap[8]);
	typed_granularity_dw = sdxi_le32_dec(&cap[16]);
	typed_ts_dw = sdxi_le32_dec(&cap[24]);

	/*
	 * The current driver-side typed SDXI_ADMIN_VENDOR CAP payload places
	 * feature_flags at 0x0c and granularity_bw at 0x10, unlike the raw
	 * v0.8 Table 2 wire image. Prefer that typed layout when the legacy
	 * granularity slot is zero but the typed granularity slot is populated.
	 */
	use_typed_layout = legacy_granularity_dw == 0 && typed_granularity_dw != 0;

	memset(out, 0, sizeof(*out));
	if (use_typed_layout) {
		out->qos_version = (uint16_t)typed_version_dw;
		out->feature_flags = (uint16_t)typed_feature_dw;
		out->device_maxbw_cap_mb_s = sdxi_le32_dec(&cap[4]);
		out->max_pools = (uint16_t)typed_max_pools_dw;
		out->granularity_bw_mb_s = typed_granularity_dw;
		legacy_ts_dw = typed_ts_dw;
	} else {
		out->qos_version = sdxi_le16_dec(&cap[0]);
		out->feature_flags = sdxi_le16_dec(&cap[2]);
		out->device_maxbw_cap_mb_s = sdxi_le32_dec(&cap[4]);
		out->max_pools = (uint16_t)(legacy_max_pools_dw & 0x0fffu);
		out->granularity_bw_mb_s = legacy_granularity_dw;
	}

	/* v0.8+ reports variance through telemetry query type 5, not CAP. */
	out->variance_bw_mb_s = 0;
	out->timestamp_units = (uint8_t)(legacy_ts_dw & 0xfu);
	out->timestamp_resolution = legacy_ts_dw >> 4;
}

static int sdxi_amd_qos_validate_pf(sdxi_dev_t *dev)
{
	struct sdxi_dev_caps caps;
	int ret;

	ret = sdxi_dev_get_caps(dev, &caps);
	if (ret)
		return ret;
	if (caps.is_vf)
		return -ENOTSUP;
	return 0;
}

static int sdxi_amd_qos_prepare(sdxi_dev_t *dev, uint32_t timeout_ms)
{
	int ret;

	ret = sdxi_amd_qos_validate_pf(dev);
	if (ret)
		return ret;

	return sdxi_dev_admin_vendor_probe(dev, sdxi_amd_qos_guid, timeout_ms,
					       NULL);
}

static int sdxi_amd_qos_vendor(sdxi_dev_t *dev, uint32_t op,
			       const void *in, size_t in_size,
			       void *out, size_t *out_bytes_inout,
			       uint32_t timeout_ms,
			       struct sdxi_admin_cpl *cpl_out)
{
	return sdxi_dev_admin_vendor(dev, sdxi_amd_qos_guid, op, in, in_size,
				     out, out_bytes_inout, timeout_ms,
				     cpl_out);
}

size_t sdxi_amd_qos_tel_active_pools_bytes(uint16_t max_pools)
{
	return SDXI_AMD_QOS_TEL_HDR_BYTES + (size_t)(max_pools + 7u) / 8u;
}

size_t sdxi_amd_qos_tel_pool_report_bytes(uint16_t assigned_functions)
{
	size_t count = assigned_functions;
	size_t bytes;

	if (count > (SIZE_MAX - SDXI_AMD_QOS_TEL_HDR_BYTES) /
		    SDXI_AMD_QOS_TEL_POOL_REPORT_ENTRY_BYTES)
		return 0;

	bytes = SDXI_AMD_QOS_TEL_HDR_BYTES +
		count * SDXI_AMD_QOS_TEL_POOL_REPORT_ENTRY_BYTES;

	if (bytes > SDXI_AMD_QOS_TEL_MAX_BYTES)
		return 0;
	return bytes;
}

int sdxi_amd_qos_get_cap(sdxi_dev_t *dev, uint32_t timeout_ms,
			 struct sdxi_amd_qos_cap *cap_out,
			 struct sdxi_admin_cpl *cpl_out)
{
	uint8_t cap[SDXI_AMD_QOS_CAP_BYTES];
	size_t cap_bytes;
	int ret;

	if (!dev || !cap_out)
		return -EINVAL;

	ret = sdxi_amd_qos_prepare(dev, timeout_ms);
	if (ret)
		return ret;

	cap_bytes = sizeof(cap);
	ret = sdxi_amd_qos_vendor(dev, SDXI_AMD_QOS_VENDOR_OP_CAP, NULL, 0, cap,
				   &cap_bytes, timeout_ms, cpl_out);
	if (ret)
		return ret;
	if (cap_bytes != sizeof(cap))
		return -EIO;

	sdxi_amd_qos_decode_cap(cap_out, cap);
	return 0;
}

int sdxi_amd_qos_dev_update(sdxi_dev_t *dev, uint32_t device_maxbw_mb_s,
			    uint32_t timeout_ms, struct sdxi_admin_cpl *cpl_out)
{
	struct sdxi_amd_qos_devupd_req req;
	int ret;

	if (!dev)
		return -EINVAL;

	ret = sdxi_amd_qos_prepare(dev, timeout_ms);
	if (ret)
		return ret;

	memset(&req, 0, sizeof(req));
	req.device_maxbw_mb_s = device_maxbw_mb_s;

	return sdxi_amd_qos_vendor(dev, SDXI_AMD_QOS_VENDOR_OP_DEVUPD, &req,
				   sizeof(req), NULL, NULL, timeout_ms,
				   cpl_out);
}

int sdxi_amd_qos_pool_update(sdxi_dev_t *dev, uint16_t pool_num,
			     uint8_t pool_operation, uint32_t pool_maxbw_mb_s,
			     uint32_t pool_minbw_mb_s, uint32_t timeout_ms,
			     struct sdxi_admin_cpl *cpl_out)
{
	struct sdxi_amd_qos_poolupd_req req;
	int ret;

	if (!dev)
		return -EINVAL;
	if (pool_num > 0x0fffu)
		return -EINVAL;
	if (pool_operation > SDXI_AMD_QOS_POOL_OP_DESTROY)
		return -EINVAL;

	ret = sdxi_amd_qos_prepare(dev, timeout_ms);
	if (ret)
		return ret;

	memset(&req, 0, sizeof(req));
	req.pool_num = pool_num;
	req.pool_operation = pool_operation;
	req.pool_maxbw_mb_s = pool_maxbw_mb_s;
	req.pool_minbw_mb_s = pool_minbw_mb_s;

	return sdxi_amd_qos_vendor(dev, SDXI_AMD_QOS_VENDOR_OP_POOLUPD, &req,
				   sizeof(req), NULL, NULL, timeout_ms,
				   cpl_out);
}

int sdxi_amd_qos_fn_update(sdxi_dev_t *dev, uint16_t pool_num, bool cxt,
			   bool vf, uint16_t vf_num, uint16_t cxt_start,
			   uint16_t cxt_end, uint32_t timeout_ms,
			   struct sdxi_admin_cpl *cpl_out)
{
	struct sdxi_amd_qos_fnupd_req req;
	int ret;

	if (!dev)
		return -EINVAL;
	if (pool_num > 0x0fffu)
		return -EINVAL;

	ret = sdxi_amd_qos_prepare(dev, timeout_ms);
	if (ret)
		return ret;

	memset(&req, 0, sizeof(req));
	req.pool_num = pool_num;
	req.cxt = cxt ? 1u : 0u;
	req.vf = vf ? 1u : 0u;
	req.vf_num = vf_num;
	req.cxt_start = cxt_start;
	req.cxt_end = cxt_end;

	return sdxi_amd_qos_vendor(dev, SDXI_AMD_QOS_VENDOR_OP_FNUPD, &req,
				   sizeof(req), NULL, NULL, timeout_ms,
				   cpl_out);
}

int sdxi_amd_qos_tel(sdxi_dev_t *dev, uint8_t query_type,
		     const struct sdxi_amd_qos_tel_target *target, void *buf,
		     size_t *buf_bytes_inout, uint32_t timeout_ms,
		     struct sdxi_admin_cpl *cpl_out)
{
	struct sdxi_amd_qos_tel_req req;
	int ret;

	if (!dev || !target || !buf_bytes_inout)
		return -EINVAL;
	if (query_type > SDXI_AMD_QOS_TEL_QTYPE_POOL_VARIANCE)
		return -EINVAL;
	if (!buf && *buf_bytes_inout)
		return -EINVAL;

	ret = sdxi_amd_qos_prepare(dev, timeout_ms);
	if (ret)
		return ret;

	memset(&req, 0, sizeof(req));
	req.query_type = query_type;
	req.pool_num = target->pool_num;
	req.cxt = target->cxt ? 1u : 0u;
	req.vf = target->vf ? 1u : 0u;
	req.vf_num = target->vf_num;
	req.cxt_num = target->cxt_num;

	return sdxi_amd_qos_vendor(dev, SDXI_AMD_QOS_VENDOR_OP_TEL, &req,
				   sizeof(req), buf, buf_bytes_inout,
				   timeout_ms, cpl_out);
}
