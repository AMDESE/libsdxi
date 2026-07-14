// SPDX-License-Identifier: BSD-3-Clause
#include "sdxi_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sdxi_dp.h"

static int sdxi_open_devnode(const char *path, bool *read_only_out)
{
	int fd;

	if (read_only_out)
		*read_only_out = false;

	fd = open(path, O_RDWR | O_CLOEXEC);
	if (fd >= 0)
		return fd;

	if (errno != EACCES && errno != EPERM)
		return -1;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd >= 0 && read_only_out)
		*read_only_out = true;

	return fd;
}

int sdxi_dev_open(const char *path, sdxi_dev_t **dev_out)
{
	sdxi_dev_t *dev;
	bool read_only;
	int fd;
	int ret;

	if (!path || !dev_out)
		return -EINVAL;

	*dev_out = NULL;

	fd = sdxi_open_devnode(path, &read_only);
	if (fd < 0)
		return -errno;

	dev = calloc(1, sizeof(*dev));
	if (!dev) {
		close(fd);
		return -ENOMEM;
	}

	dev->fd = fd;
	dev->read_only = read_only;
	dev->path = strdup(path);
	if (!dev->path) {
		sdxi_dev_close(dev);
		return -ENOMEM;
	}

	/*
	 * Negotiate the UAPI version once at open time. A major mismatch means
	 * the kernel speaks an incompatible ABI and the library cannot use the
	 * device safely; bail out here rather than failing every later ioctl.
	 *
	 * Treat -ENOTTY as a hard error too: any kernel that exports the SDXI
	 * char device must support SDXI_GET_UAPI_INFO.
	 */
	ret = sdxi_abi_get_uapi_info(fd, &dev->uapi_info);
	if (ret) {
		sdxi_dev_close(dev);
		return ret == -ENOTTY ? -ENOTSUP : ret;
	}
	if (dev->uapi_info.major != SDXI_UAPI_MAJOR_SUPPORTED) {
		sdxi_dev_close(dev);
		return -EPROTONOSUPPORT;
	}
	dev->uapi_info_valid = true;

	*dev_out = dev;
	return 0;
}

static uint16_t sdxi_le16_dec(const void *p)
{
	const uint8_t *b = p;

	return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static bool sdxi_dev_is_v1_1_or_later(const struct sdxi_dev *dev)
{
	if (!dev)
		return false;

	return dev->caps.ver_major > 1 ||
	       (dev->caps.ver_major == 1 && dev->caps.ver_minor >= 1);
}

static enum sdxi_grp_id sdxi_grp_from_guid(const uint8_t guid[16])
{
	/*
	 * GUID values are stored in SDXI DISC byte order (RFC4122 with
	 * time_low/time_mid/time_hi_and_version little-endian).
	 */
	static const struct {
		enum sdxi_grp_id grp;
		uint8_t guid[16];
	} map[] = {
		/* OP_TYPE_DMA: 193259fe-150b-4008-a83f-6faf59f5272a */
		{
			.grp = SDXI_GRP_DMA,
			.guid = {
				0xfe, 0x59, 0x32, 0x19, 0x0b, 0x15, 0x08, 0x40,
				0xa8, 0x3f, 0x6f, 0xaf, 0x59, 0xf5, 0x27, 0x2a,
			},
		},
		/* OP_TYPE_ATOMIC: 3ac7630e-7009-424b-8299-20e2901ecc5e */
		{
			.grp = SDXI_GRP_ATOMIC,
			.guid = {
				0x0e, 0x63, 0xc7, 0x3a, 0x09, 0x70, 0x4b, 0x42,
				0x82, 0x99, 0x20, 0xe2, 0x90, 0x1e, 0xcc, 0x5e,
			},
		},
		/* OP_TYPE_INTR: 7fb16347-4c3a-4d16-8442-cba51e340880 */
		{
			.grp = SDXI_GRP_INTR,
			.guid = {
				0x47, 0x63, 0xb1, 0x7f, 0x3a, 0x4c, 0x16, 0x4d,
				0x84, 0x42, 0xcb, 0xa5, 0x1e, 0x34, 0x08, 0x80,
			},
		},
		/* CrcM1Grp: 15114db9-4a7e-47ab-97dd-ea86eda10b6e */
		{
			.grp = SDXI_GRP_CRCM1,
			.guid = {
				0xb9, 0x4d, 0x11, 0x15, 0x7e, 0x4a, 0xab, 0x47,
				0x97, 0xdd, 0xea, 0x86, 0xed, 0xa1, 0x0b, 0x6e,
			},
		},
		/* DblCopyGrp: 989b9a64-28d9-4294-9c7b-9a3f27afaf5c */
		{
			.grp = SDXI_GRP_DCOPY,
			.guid = {
				0x64, 0x9a, 0x9b, 0x98, 0xd9, 0x28, 0x94, 0x42,
				0x9c, 0x7b, 0x9a, 0x3f, 0x27, 0xaf, 0xaf, 0x5c,
			},
		},
		/* AMD QoS (ASQOSE): f5b7610b-0dc6-4c03-ab79-65f1f4efcf4c */
		{
			.grp = SDXI_GRP_ASQOSE,
			.guid = {
				0x0b, 0x61, 0xb7, 0xf5, 0xc6, 0x0d, 0x03, 0x4c,
				0xab, 0x79, 0x65, 0xf1, 0xf4, 0xef, 0xcf, 0x4c,
			},
		},
		/* XDMA: 0af23293-7595-4939-b08a-1c7293c2e975 */
		{
			.grp = SDXI_GRP_XDMA,
			.guid = {
				0x93, 0x32, 0xf2, 0x0a, 0x95, 0x75, 0x39, 0x49,
				0xb0, 0x8a, 0x1c, 0x72, 0x93, 0xc2, 0xe9, 0x75,
			},
		},
	};

	if (!guid)
		return SDXI_GRP_UNKNOWN;

	for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
		if (memcmp(map[i].guid, guid, 16) == 0)
			return map[i].grp;
	}

	return SDXI_GRP_UNKNOWN;
}

static void sdxi_opgroups_set(struct sdxi_dev *dev, uint16_t op_type,
			      enum sdxi_grp_id grp)
{
	if (!dev || op_type == 0 || op_type > SDXI_DISC_MAX_INDEX)
		return;

	dev->grp_by_type[op_type] = grp;
	if (grp > SDXI_GRP_UNKNOWN && grp < SDXI_GRP__MAX)
		dev->type_by_grp[grp] = op_type;
}

static void sdxi_opgroups_init_base(struct sdxi_dev *dev)
{
	uint32_t opb_000_avl;

	if (!dev || dev->opgroups_base_valid)
		return;

	/*
	 * SDXI 1.1+ devices expose opgroups through DSC_DISC instead of the
	 * legacy MMIO_CAP1 op_group_caps bitmask. AdminGrp remains special:
	 * it has a fixed descriptor type (0x2) but no GUID-defined DISC entry.
	 */
	if (sdxi_dev_is_v1_1_or_later(dev)) {
		sdxi_opgroups_set(dev, SDXI_OP_TYPE_ADMIN, SDXI_GRP_ADMIN);
		dev->opgroups_base_valid = true;
		return;
	}

	/*
	 * The opb_000_avl bitmask is no longer exposed as a per-attribute
	 * UAPI selector; it is the same bitfield as MMIO_CAP1.op_grp_cap and
	 * is decoded from SDXI_GET_DEV_INFO into caps.op_group_caps.
	 */
	opb_000_avl = dev->caps.op_group_caps;

	/* DMA and Admin are mandatory; their capability bits are reserved. */
	sdxi_opgroups_set(dev, SDXI_OP_TYPE_DMA, SDXI_GRP_DMA);
	sdxi_opgroups_set(dev, SDXI_OP_TYPE_ADMIN, SDXI_GRP_ADMIN);
	/* Full and Minimal Atomic both use opcode type 3. */
	if (opb_000_avl & ((1U << SDXI_OP_TYPE_ATOMIC) | (1U << 5)))
		sdxi_opgroups_set(dev, SDXI_OP_TYPE_ATOMIC, SDXI_GRP_ATOMIC);
	if (opb_000_avl & (1U << SDXI_OP_TYPE_INTR))
		sdxi_opgroups_set(dev, SDXI_OP_TYPE_INTR, SDXI_GRP_INTR);

	/*
	 * Provide GUIDs for a subset of fixed-type groups when available.
	 * This is informational only; SDXI 1.0 does not expose GUID discovery.
	 */
	if (dev->grp_by_type[SDXI_OP_TYPE_DMA] == SDXI_GRP_DMA) {
		static const uint8_t guid_dma[16] = {
			0xfe, 0x59, 0x32, 0x19, 0x0b, 0x15, 0x08, 0x40,
			0xa8, 0x3f, 0x6f, 0xaf, 0x59, 0xf5, 0x27, 0x2a,
		};

		memcpy(dev->guid_by_type[SDXI_OP_TYPE_DMA], guid_dma,
		       sizeof(guid_dma));
	}
	if (dev->grp_by_type[SDXI_OP_TYPE_ATOMIC] == SDXI_GRP_ATOMIC) {
		static const uint8_t guid_atomic[16] = {
			0x0e, 0x63, 0xc7, 0x3a, 0x09, 0x70, 0x4b, 0x42,
			0x82, 0x99, 0x20, 0xe2, 0x90, 0x1e, 0xcc, 0x5e,
		};

		memcpy(dev->guid_by_type[SDXI_OP_TYPE_ATOMIC], guid_atomic,
		       sizeof(guid_atomic));
	}

	dev->opgroups_base_valid = true;
}

static int sdxi_opgroups_disc_refresh(struct sdxi_dev *dev, uint32_t timeout_ms)
{
	void *disc0 = NULL;
	void *discs = NULL;
	uint16_t max_index;
	size_t disc0_bytes;
	size_t discs_bytes;
	int ret;

	if (!dev)
		return -EINVAL;

	if (dev->disc_valid)
		return 0;
	if (dev->disc_tried)
		return dev->disc_err;

	dev->disc_tried = true;

	ret = posix_memalign(&disc0, SDXI_DISC_RECORD_BYTES, SDXI_DISC_RECORD_BYTES);
	if (ret) {
		dev->disc_err = -ret;
		return dev->disc_err;
	}
	memset(disc0, 0, SDXI_DISC_RECORD_BYTES);

	disc0_bytes = SDXI_DISC_RECORD_BYTES;
	ret = sdxi_dev_admin_disc(dev, 0, 0, disc0, &disc0_bytes, timeout_ms, NULL);
	if (ret)
		goto out;
	if (disc0_bytes != SDXI_DISC_RECORD_BYTES) {
		ret = -EIO;
		goto out;
	}

	max_index = sdxi_le16_dec(disc0) & 0x7ff;
	if (max_index == 0) {
		ret = -ENODEV;
		goto out;
	}

	discs_bytes = (size_t)max_index * SDXI_DISC_RECORD_BYTES;
	ret = posix_memalign(&discs, SDXI_DISC_RECORD_BYTES, discs_bytes);
	if (ret) {
		ret = -ret;
		goto out;
	}
	memset(discs, 0, discs_bytes);

	ret = sdxi_dev_admin_disc(dev, 1, max_index, discs, &discs_bytes,
				  timeout_ms, NULL);
	if (ret)
		goto out;
	if (discs_bytes != (size_t)max_index * SDXI_DISC_RECORD_BYTES) {
		ret = -EIO;
		goto out;
	}

	dev->disc_max_index = max_index;
	dev->disc_valid = true;
	dev->disc_err = 0;

	/*
	 * AdminGrp has a fixed descriptor type (0x2) but no GUID-defined DISC
	 * record, so preserve the fixed mapping explicitly.
	 */
	if (max_index >= SDXI_OP_TYPE_ADMIN)
		sdxi_opgroups_set(dev, SDXI_OP_TYPE_ADMIN, SDXI_GRP_ADMIN);

	for (uint16_t idx = 1; idx <= max_index; idx++) {
		const uint8_t *ent = (const uint8_t *)discs +
				     (size_t)(idx - 1) * SDXI_DISC_RECORD_BYTES;
		enum sdxi_grp_id grp;

		memcpy(dev->guid_by_type[idx], ent, 16);

		if (idx == SDXI_OP_TYPE_ADMIN)
			continue;

		grp = sdxi_grp_from_guid(ent);
		if (grp != SDXI_GRP_UNKNOWN)
			sdxi_opgroups_set(dev, idx, grp);
	}

out:
	free(disc0);
	free(discs);
	dev->disc_err = ret;
	return ret;
}

void sdxi_dev_close(sdxi_dev_t *dev)
{
	if (!dev)
		return;
	if (dev->fd >= 0)
		close(dev->fd);
	free(dev->path);
	free(dev);
}

const char *sdxi_dev_path(const sdxi_dev_t *dev)
{
	if (!dev)
		return NULL;
	return dev->path;
}

int sdxi_dev_get_fd(const sdxi_dev_t *dev)
{
	if (!dev)
		return -EINVAL;
	return dev->fd;
}

int sdxi_dev_get_uapi_info(sdxi_dev_t *dev, struct sdxi_uapi_info *out)
{
	if (!dev || !out)
		return -EINVAL;
	if (!dev->uapi_info_valid)
		return -ENOTSUP;
	*out = dev->uapi_info;
	return 0;
}

bool sdxi_dev_has_uapi_cap(sdxi_dev_t *dev, uint64_t cap)
{
	uint16_t i;

	if (!dev || !dev->uapi_info_valid || cap == 0)
		return false;

	for (i = 0; i < dev->uapi_info.cap_words; i++) {
		if (dev->uapi_info.caps[i] & cap)
			return true;
	}
	return false;
}

static int sdxi_dev_cache_dev_info(struct sdxi_dev *dev)
{
	int ret;

	if (!dev)
		return -EINVAL;

	if (dev->dev_info_valid)
		return 0;

	ret = sdxi_abi_get_dev_info(dev->fd, &dev->dev_info);
	if (ret)
		return ret;

	dev->dev_info_valid = true;
	return 0;
}

int sdxi_dev_get_dev_info_raw(sdxi_dev_t *dev, struct sdxi_dev_info_raw *out)
{
	int ret;

	if (!dev || !out)
		return -EINVAL;

	ret = sdxi_dev_cache_dev_info(dev);
	if (ret)
		return ret;

	*out = dev->dev_info;
	return 0;
}

static void sdxi_decode_caps(struct sdxi_dev_caps *out,
			     const struct sdxi_dev_info_raw *in)
{
	const uint64_t cap0 = in->cap0;
	const uint64_t cap1 = in->cap1;

	memset(out, 0, sizeof(*out));

	/*
	 * MMIO_VERSION is parsed by the kernel and split into hw_major/hw_minor;
	 * the public caps struct keeps the same 8-bit fields for compatibility
	 * with downstream consumers.
	 */
	out->ver_major = (uint8_t)in->hw_major;
	out->ver_minor = (uint8_t)in->hw_minor;

	out->sfunc = cap0 & 0xFFFF;
	out->is_vf = (cap0 >> 16) & 0x1;
	out->cs_cap = (cap0 >> 17) & 0x3;
	out->db_stride_bytes = 1U << (((cap0 >> 20) & 0x7) + 12);

	out->max_ds_ring_entries = 1ULL << (((cap0 >> 24) & 0x1F) + 10);
	out->max_rkey_entries = 1U << (((cap0 >> 32) & 0xF) + 8);

	out->max_buffer_bytes = 1ULL << ((cap1 & 0xF) + 21);
	out->rkey_supported = (cap1 >> 4) & 0x1;
	out->rm_required = (cap1 >> 5) & 0x1;
	out->mmio_64bit = (cap1 >> 6) & 0x1;
	out->max_errlog_entries = 1ULL << (((cap1 >> 8) & 0xF) + 17);
	out->max_akey_entries = 1U << (((cap1 >> 12) & 0xF) + 8);
	out->max_cxt_num = ((cap1 >> 16) & 0xFFFF) + 1;
	out->op_group_caps = (cap1 >> 32) & 0xFFFFFFFF;
}

int sdxi_dev_get_caps(sdxi_dev_t *dev, struct sdxi_dev_caps *out)
{
	struct sdxi_dev_info_raw raw;
	int ret;

	if (!dev || !out)
		return -EINVAL;

	if (dev->caps_valid) {
		*out = dev->caps;
		return 0;
	}

	ret = sdxi_dev_get_dev_info_raw(dev, &raw);
	if (ret)
		return ret;

	sdxi_decode_caps(&dev->caps, &raw);
	dev->caps_valid = true;
	sdxi_opgroups_init_base(dev);
	*out = dev->caps;
	return 0;
}

int sdxi_dev_get_info(sdxi_dev_t *dev, struct sdxi_dev_info *out)
{
	int ret;

	if (!dev || !out)
		return -EINVAL;

	ret = sdxi_dev_get_caps(dev, &dev->caps);
	if (ret)
		return ret;

	memset(out, 0, sizeof(*out));
	out->size = sizeof(*out);
	if (dev->read_only)
		out->flags |= SDXI_DEV_INFO_F_READ_ONLY;
	out->uapi = dev->uapi_info;
	out->raw = dev->dev_info;
	out->caps = dev->caps;

	return 0;
}

int sdxi_dev_get_attr(sdxi_dev_t *dev, enum sdxi_dev_attr_id attr,
		      uint64_t *val_out)
{
	if (!dev || !val_out)
		return -EINVAL;
	if (attr == 0)
		return -EINVAL;
	return sdxi_abi_get_dev_attr(dev->fd, (uint64_t)attr, val_out);
}

int sdxi_dev_set_attr(sdxi_dev_t *dev, enum sdxi_dev_attr_id attr,
		      uint64_t *val_inout)
{
	if (!dev || !val_inout)
		return -EINVAL;
	if (attr == 0)
		return -EINVAL;
	if (!sdxi_dev_attr_is_writable(attr))
		return -ENOTSUP;
	return sdxi_abi_set_dev_attr(dev->fd, (uint64_t)attr, val_inout);
}

int sdxi_dev_get_rkey_list(sdxi_dev_t *dev, struct sdxi_bitmap *out)
{
	if (!dev || !out)
		return -EINVAL;
	return sdxi_abi_get_rkey_list(dev->fd, out);
}

int sdxi_dev_get_rkeys(sdxi_dev_t *dev, struct sdxi_key_ent *ents, size_t nents)
{
	if (!dev)
		return -EINVAL;
	return sdxi_abi_get_rkeys(dev->fd, ents, nents);
}

int sdxi_dev_set_rkeys(sdxi_dev_t *dev, const struct sdxi_key_ent *ents,
		       size_t nents, uint32_t timeout_ms,
		       struct sdxi_key_set_result *res)
{
	struct sdxi_key_set_result local;
	struct sdxi_key_set_result *out = res ? res : &local;

	if (!dev)
		return -EINVAL;

	out->commit = SDXI_KEY_COMMIT_NONE_E;
	out->first_failed_index = 0;
	out->cst_flags = 0;

	return sdxi_abi_set_rkeys(dev->fd, ents, nents, timeout_ms, out);
}

int sdxi_dev_get_rkey(sdxi_dev_t *dev, uint32_t index,
		      struct sdxi_key_ent *ent_out)
{
	struct sdxi_key_ent ent;
	int ret;

	if (!dev || !ent_out)
		return -EINVAL;

	memset(&ent, 0, sizeof(ent));
	ent.index = index;

	ret = sdxi_dev_get_rkeys(dev, &ent, 1);
	if (ret)
		return ret;

	*ent_out = ent;
	return 0;
}

int sdxi_dev_set_rkey(sdxi_dev_t *dev, const struct sdxi_key_ent *ent)
{
	struct sdxi_key_set_result res;
	int ret;

	if (!dev || !ent)
		return -EINVAL;

	ret = sdxi_dev_set_rkeys(dev, ent, 1, SDXI_KEY_TIMEOUT_DEFAULT, &res);
	if (ret)
		return ret;
	if (res.commit != SDXI_KEY_COMMIT_ALL_E)
		return -EIO;
	return 0;
}

int sdxi_dev_set_rkeys_resume(sdxi_dev_t *dev,
			      const struct sdxi_key_ent *ents, size_t nents,
			      uint32_t timeout_ms,
			      struct sdxi_key_set_result *res)
{
	struct sdxi_key_set_result local;
	struct sdxi_key_set_result *out = res ? res : &local;
	uint32_t off = 0;
	/*
	 * Cap retries at one extra attempt per entry (plus the original call)
	 * so we cannot get stuck even if the kernel keeps reporting partial
	 * commits without forward progress.
	 */
	uint32_t budget = (uint32_t)nents + 1;
	int ret;

	if (!dev)
		return -EINVAL;
	if (!ents && nents)
		return -EINVAL;
	if (nents > UINT32_MAX)
		return -EINVAL;
	if (nents == 0)
		return 0;

	out->commit = SDXI_KEY_COMMIT_NONE_E;
	out->first_failed_index = 0;
	out->cst_flags = 0;

	while (budget--) {
		ret = sdxi_abi_set_rkeys(dev->fd, ents + off,
					 (size_t)((uint32_t)nents - off),
					 timeout_ms, out);
		if (ret)
			return ret;

		if (out->commit == SDXI_KEY_COMMIT_ALL_E)
			return 0;
		if (out->commit != SDXI_KEY_COMMIT_PARTIAL_E)
			return 0;

		/*
		 * Kernel partially committed; first_failed_index is relative to
		 * the entries[] passed in this call. Translate to an absolute
		 * offset and ensure we always make progress.
		 */
		if (out->first_failed_index == 0)
			return -EIO;
		off += out->first_failed_index;
		if (off >= nents)
			return 0;
	}

	return -EIO;
}

int sdxi_dev_alloc_rkey_index(sdxi_dev_t *dev, uint32_t *index_out)
{
	struct sdxi_bitmap bm;
	uint32_t i;
	int ret;

	if (!dev || !index_out)
		return -EINVAL;

	memset(&bm, 0, sizeof(bm));
	ret = sdxi_dev_get_rkey_list(dev, &bm);
	if (ret)
		return ret;

	ret = -ENOSPC;
	for (i = 0; i < bm.nentries; i++) {
		uint8_t byte = bm.data ? bm.data[i / 8] : 0;

		if (!(byte & (uint8_t)(1u << (i % 8)))) {
			*index_out = i;
			ret = 0;
			break;
		}
	}

	sdxi_bitmap_free(&bm);
	return ret;
}

int sdxi_dev_invalidate_rkey(sdxi_dev_t *dev, uint32_t index,
			     uint32_t timeout_ms,
			     struct sdxi_key_set_result *res)
{
	struct sdxi_key_ent ent;

	if (!dev)
		return -EINVAL;

	memset(&ent, 0, sizeof(ent));
	ent.index = index;
	sdxi_key_payload_clear(&ent.payload);
	return sdxi_dev_set_rkeys(dev, &ent, 1, timeout_ms, res);
}

int sdxi_dev_opgroups_refresh(sdxi_dev_t *dev, uint32_t timeout_ms)
{
	int ret;

	if (!dev)
		return -EINVAL;

	ret = sdxi_dev_get_caps(dev, &dev->caps);
	if (ret)
		return ret;

	sdxi_opgroups_init_base(dev);
	return sdxi_opgroups_disc_refresh(dev, timeout_ms);
}

int sdxi_dev_get_opgroups(sdxi_dev_t *dev, struct sdxi_opgroup_info **out,
			  size_t *count_out)
{
	struct sdxi_opgroup_info *list = NULL;
	size_t count = 0;
	int ret;

	if (!dev || !out || !count_out)
		return -EINVAL;

	*out = NULL;
	*count_out = 0;

	ret = sdxi_dev_get_caps(dev, &dev->caps);
	if (ret)
		return ret;
	sdxi_opgroups_init_base(dev);

	if (dev->disc_valid && dev->disc_max_index > 0) {
		count = dev->disc_max_index;
		list = calloc(count, sizeof(*list));
		if (!list)
			return -ENOMEM;

		for (uint16_t idx = 1; idx <= dev->disc_max_index; idx++) {
			struct sdxi_opgroup_info *ent = &list[idx - 1];

			ent->op_type = idx;
			ent->grp_id = dev->grp_by_type[idx];
			memcpy(ent->guid, dev->guid_by_type[idx],
			       sizeof(ent->guid));
		}

		*out = list;
		*count_out = count;
		return 0;
	}

	for (uint16_t idx = 1; idx <= SDXI_DISC_MAX_INDEX; idx++) {
		if (dev->grp_by_type[idx] != SDXI_GRP_UNKNOWN)
			count++;
	}
	if (count == 0)
		return 0;

	list = calloc(count, sizeof(*list));
	if (!list)
		return -ENOMEM;

	for (uint16_t idx = 1, j = 0; idx <= SDXI_DISC_MAX_INDEX; idx++) {
		if (dev->grp_by_type[idx] == SDXI_GRP_UNKNOWN)
			continue;
		list[j].op_type = idx;
		list[j].grp_id = dev->grp_by_type[idx];
		memcpy(list[j].guid, dev->guid_by_type[idx],
		       sizeof(list[j].guid));
		j++;
	}

	*out = list;
	*count_out = count;
	return 0;
}

void sdxi_free_opgroups(struct sdxi_opgroup_info *list)
{
	free(list);
}

int sdxi_dev_get_op_type(sdxi_dev_t *dev, enum sdxi_grp_id grp,
			 uint32_t timeout_ms, uint16_t *op_type_out)
{
	uint16_t op_type;
	int ret;

	if (!dev || !op_type_out)
		return -EINVAL;
	if (grp <= SDXI_GRP_UNKNOWN || grp >= SDXI_GRP__MAX)
		return -EINVAL;

	ret = sdxi_dev_get_caps(dev, &dev->caps);
	if (ret)
		return ret;
	sdxi_opgroups_init_base(dev);

	op_type = dev->type_by_grp[grp];
	if (op_type) {
		*op_type_out = op_type;
		return 0;
	}

	ret = sdxi_opgroups_disc_refresh(dev, timeout_ms);
	if (ret)
		return ret;

	op_type = dev->type_by_grp[grp];
	if (!op_type)
		return -ENODEV;

	*op_type_out = op_type;
	return 0;
}

int sdxi_dev_admin_disc(sdxi_dev_t *dev, uint16_t index_start,
			uint16_t index_end, void *buf, size_t *buf_bytes_inout,
			uint32_t timeout_ms, struct sdxi_admin_cpl *cpl_out)
{
	struct sdxi_dev_caps caps;
	uint32_t bytes_u32;
	int ret;

	if (!dev || !buf_bytes_inout)
		return -EINVAL;
	if (index_start > index_end || index_end > SDXI_DISC_MAX_INDEX)
		return -EINVAL;
	if (*buf_bytes_inout > UINT32_MAX)
		return -EINVAL;
	if (!buf && *buf_bytes_inout)
		return -EINVAL;

	if (!sdxi_dev_has_uapi_cap(dev, SDXI_UAPI_INFO_CAP_ADMIN_DISC))
		return -ENOTSUP;

	ret = sdxi_dev_get_caps(dev, &caps);
	if (ret)
		return ret;
	if (caps.ver_major < 1 || (caps.ver_major == 1 && caps.ver_minor < 1))
		return -ENOTSUP;

	bytes_u32 = (uint32_t)*buf_bytes_inout;
	ret = sdxi_abi_admin_disc(dev->fd, index_start, index_end, buf,
				 &bytes_u32, timeout_ms, cpl_out);
	*buf_bytes_inout = bytes_u32;

	return ret;
}

int sdxi_dev_admin_vendor(sdxi_dev_t *dev,
			  const uint8_t guid[SDXI_VENDOR_GUID_BYTES],
			  uint32_t op, const void *in, size_t in_size,
			  void *out, size_t *out_bytes_inout,
			  uint32_t timeout_ms, struct sdxi_admin_cpl *cpl_out)
{
	struct sdxi_dev_caps caps;
	uint32_t in_bytes_u32;
	uint32_t out_bytes_u32 = 0;
	int ret;

	if (!dev || !guid)
		return -EINVAL;
	if (in_size && !in)
		return -EINVAL;
	if (out && !out_bytes_inout)
		return -EINVAL;
	if (out_bytes_inout) {
		if (*out_bytes_inout > UINT32_MAX)
			return -EINVAL;
		if (!out && *out_bytes_inout)
			return -EINVAL;
		out_bytes_u32 = (uint32_t)*out_bytes_inout;
	}
	if (in_size > UINT32_MAX)
		return -EINVAL;

	if (!sdxi_dev_has_uapi_cap(dev, SDXI_UAPI_INFO_CAP_VENDOR_OP))
		return -ENOTSUP;

	ret = sdxi_dev_get_caps(dev, &caps);
	if (ret)
		return ret;
	if (caps.ver_major < 1 || (caps.ver_major == 1 && caps.ver_minor < 1))
		return -ENOTSUP;

	in_bytes_u32 = (uint32_t)in_size;
	ret = sdxi_abi_admin_vendor(dev->fd, guid, op, in, in_bytes_u32, out,
				    out_bytes_inout ? &out_bytes_u32 : NULL,
				    timeout_ms, cpl_out);

	if (out_bytes_inout)
		*out_bytes_inout = out_bytes_u32;

	return ret;
}

int sdxi_dev_admin_vendor_probe(sdxi_dev_t *dev,
				const uint8_t guid[SDXI_VENDOR_GUID_BYTES],
				uint32_t timeout_ms,
				struct sdxi_admin_cpl *cpl_out)
{
	return sdxi_dev_admin_vendor(dev, guid, SDXI_ADMIN_VENDOR_PROBE,
				      NULL, 0, NULL, NULL, timeout_ms,
				      cpl_out);
}
