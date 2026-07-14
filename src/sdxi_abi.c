// SPDX-License-Identifier: BSD-3-Clause
#include "sdxi_abi.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include "sdxi_uapi_internal.h"

static int sdxi_ioctl(int fd, unsigned long cmd, void *arg)
{
	if (ioctl(fd, cmd, arg) == 0)
		return 0;
	return -errno;
}

static void sdxi_bitmap_reset(struct sdxi_bitmap *bm)
{
	if (!bm)
		return;
	bm->nvalid = 0;
	bm->nentries = 0;
	bm->bytes = 0;
	bm->data = NULL;
}

int sdxi_abi_get_uapi_info(int fd, struct sdxi_uapi_info *out)
{
	struct sdxi_uapi_info_args args;
	uint16_t words;
	int ret;

	if (!out)
		return -EINVAL;

	memset(&args, 0, sizeof(args));
	args.argsz = sizeof(args);

	ret = sdxi_ioctl(fd, SDXI_GET_UAPI_INFO, &args);
	if (ret)
		return ret;

	memset(out, 0, sizeof(*out));
	out->major = args.uapi_major;
	out->minor = args.uapi_minor;

	words = args.cap_words;
	if (words > SDXI_UAPI_INFO_CAP_WORDS)
		words = SDXI_UAPI_INFO_CAP_WORDS;
	out->cap_words = words;
	for (uint16_t i = 0; i < words; i++)
		out->caps[i] = args.caps[i];
	return 0;
}

int sdxi_abi_get_dev_info(int fd, struct sdxi_dev_info_raw *out)
{
	struct sdxi_dev_info_args args;
	int ret;

	if (!out)
		return -EINVAL;

	memset(&args, 0, sizeof(args));
	args.argsz = sizeof(args);
	ret = sdxi_ioctl(fd, SDXI_GET_DEV_INFO, &args);
	if (ret)
		return ret;

	out->hw_major = args.hw_major;
	out->hw_minor = args.hw_minor;
	out->cap0 = args.cap0;
	out->cap1 = args.cap1;
	out->sts0 = args.sts0;
	return 0;
}

/*
 * Map kernel attribute-ioctl errno values onto the libsdxi convention.
 * EOPNOTSUPP is the kernel's "this selector is not implemented" / "this
 * attribute is read-only" signal; surface it as -ENOTSUP so the public API
 * doesn't depend on Linux-specific errno aliasing.
 */
static int sdxi_abi_dev_attr_map_err(int err)
{
	if (err == -EOPNOTSUPP)
		return -ENOTSUP;
	return err;
}

int sdxi_abi_get_dev_attr(int fd, uint64_t attr, uint64_t *val_out)
{
	struct sdxi_dev_attr_args args;
	int ret;

	if (!val_out)
		return -EINVAL;

	memset(&args, 0, sizeof(args));
	args.argsz = sizeof(args);
	args.attr = attr;

	ret = sdxi_ioctl(fd, SDXI_GET_DEV_ATTR, &args);
	if (ret)
		return sdxi_abi_dev_attr_map_err(ret);

	*val_out = args.val;
	return 0;
}

int sdxi_abi_set_dev_attr(int fd, uint64_t attr, uint64_t *val_inout)
{
	struct sdxi_dev_attr_args args;
	int ret;

	if (!val_inout)
		return -EINVAL;

	memset(&args, 0, sizeof(args));
	args.argsz = sizeof(args);
	args.attr = attr;
	args.val = *val_inout;

	ret = sdxi_ioctl(fd, SDXI_SET_DEV_ATTR, &args);
	if (ret)
		return sdxi_abi_dev_attr_map_err(ret);

	/* SET is IOWR: kernel echoes back the value actually applied. */
	*val_inout = args.val;
	return 0;
}

static size_t sdxi_bitmap_bytes(uint32_t nentries)
{
	return (nentries + 7u) / 8u;
}

static int sdxi_copy_key_ents_to_uapi(struct sdxi_key_entry *dst,
				      const struct sdxi_key_ent *src,
				      size_t nents)
{
	size_t i;

	if (!nents)
		return 0;
	if (!dst || !src)
		return -EINVAL;

	for (i = 0; i < nents; i++) {
		dst[i].index = src[i].index;
		dst[i].rsvd0 = 0;
		memcpy(dst[i].data, src[i].payload.dword, sizeof(dst[i].data));
	}
	return 0;
}

static void sdxi_copy_key_ents_from_uapi(struct sdxi_key_ent *dst,
					 const struct sdxi_key_entry *src,
					 size_t nents)
{
	size_t i;

	for (i = 0; i < nents; i++) {
		dst[i].index = src[i].index;
		memcpy(dst[i].payload.dword, src[i].data,
		       sizeof(dst[i].payload.dword));
	}
}

static int sdxi_abi_get_key_list(int fd, unsigned long cmd,
				 struct sdxi_bitmap *out)
{
	struct sdxi_key_list_args *args = NULL;
	size_t args_sz;
	size_t data_sz = 0;
	int ret;

	if (!out)
		return -EINVAL;
	sdxi_bitmap_reset(out);

	for (;;) {
		if (data_sz > SIZE_MAX - sizeof(*args))
			return -EOVERFLOW;

		args_sz = sizeof(*args) + data_sz;
		args = calloc(1, args_sz);
		if (!args)
			return -ENOMEM;

		args->argsz = (uint32_t)sizeof(*args);
		args->flags = 0;
		args->buf_size = (uint32_t)data_sz;
		ret = sdxi_ioctl(fd, cmd, args);
		if (ret == -ENOSPC || (ret == 0 && data_sz == 0 && args->nentries)) {
			size_t needed = sdxi_bitmap_bytes(args->nentries);

			if (args->buf_size > needed)
				needed = args->buf_size;
			if (needed == 0) {
				out->nvalid = args->nvalid;
				out->nentries = args->nentries;
				out->bytes = 0;
				out->data = NULL;
				free(args);
				return 0;
			}
			data_sz = needed;
			free(args);
			args = NULL;
			continue;
		}
		if (ret) {
			free(args);
			return ret;
		}

		out->nvalid = args->nvalid;
		out->nentries = args->nentries;
		out->bytes = sdxi_bitmap_bytes(args->nentries);
		if (out->bytes > data_sz) {
			free(args);
			sdxi_bitmap_reset(out);
			return -EIO;
		}
		if (out->bytes) {
			out->data = malloc(out->bytes);
			if (!out->data) {
				free(args);
				sdxi_bitmap_reset(out);
				return -ENOMEM;
			}
			memcpy(out->data, args->data, out->bytes);
		} else {
			out->data = NULL;
		}
		free(args);
		return 0;
	}
}

static int sdxi_abi_get_keys(int fd, unsigned long cmd,
			     struct sdxi_key_ent *ents, size_t nents)
{
	struct sdxi_get_keys_args *args;
	size_t ents_sz;
	size_t args_sz;
	uint32_t nret;
	int ret;

	if (!ents && nents)
		return -EINVAL;
	if (nents > UINT32_MAX)
		return -EINVAL;
	if (nents > SIZE_MAX / sizeof(args->entries[0]))
		return -EOVERFLOW;

	ents_sz = nents * sizeof(args->entries[0]);
	if (ents_sz > SIZE_MAX - sizeof(*args))
		return -EOVERFLOW;

	args_sz = sizeof(*args) + ents_sz;
	args = calloc(1, args_sz);
	if (!args)
		return -ENOMEM;

	args->argsz = (uint32_t)sizeof(*args);
	args->flags = 0;
	args->nkeys = (uint32_t)nents;
	if (nents) {
		ret = sdxi_copy_key_ents_to_uapi(args->entries, ents, nents);
		if (ret) {
			free(args);
			return ret;
		}
	}

	ret = sdxi_ioctl(fd, cmd, args);
	if (!ret) {
		nret = args->nkeys;
		if (nret > nents)
			nret = (uint32_t)nents;
		sdxi_copy_key_ents_from_uapi(ents, args->entries, nret);
	}

	free(args);
	return ret;
}

static int sdxi_abi_set_keys(int fd, unsigned long cmd,
			     const struct sdxi_key_ent *ents, size_t nents,
			     uint32_t timeout_ms,
			     struct sdxi_key_set_result *res)
{
	struct sdxi_set_keys_args *args;
	size_t ents_sz;
	size_t args_sz;
	int ret;

	if (!ents && nents)
		return -EINVAL;
	if (nents > UINT32_MAX)
		return -EINVAL;
	if (nents > SIZE_MAX / sizeof(args->entries[0]))
		return -EOVERFLOW;

	ents_sz = nents * sizeof(args->entries[0]);
	if (ents_sz > SIZE_MAX - sizeof(*args))
		return -EOVERFLOW;

	args_sz = sizeof(*args) + ents_sz;
	args = calloc(1, args_sz);
	if (!args)
		return -ENOMEM;

	args->argsz = (uint32_t)sizeof(*args);
	args->flags = 0;
	args->timeout_ms = timeout_ms;
	args->nkeys = (uint32_t)nents;
	if (nents) {
		ret = sdxi_copy_key_ents_to_uapi(args->entries, ents, nents);
		if (ret) {
			free(args);
			return ret;
		}
	}

	ret = sdxi_ioctl(fd, cmd, args);

	if (res) {
		res->commit = (enum sdxi_key_commit)args->commit_state;
		res->first_failed_index = args->first_failed_index;
		res->cst_flags = args->cst_flags;
	}

	free(args);
	return ret;
}

int sdxi_abi_get_rkey_list(int fd, struct sdxi_bitmap *out)
{
	return sdxi_abi_get_key_list(fd, SDXI_GET_RKEY_LIST, out);
}

int sdxi_abi_get_rkeys(int fd, struct sdxi_key_ent *ents, size_t nents)
{
	return sdxi_abi_get_keys(fd, SDXI_GET_RKEYS, ents, nents);
}

int sdxi_abi_set_rkeys(int fd, const struct sdxi_key_ent *ents, size_t nents,
		       uint32_t timeout_ms, struct sdxi_key_set_result *res)
{
	return sdxi_abi_set_keys(fd, SDXI_SET_RKEYS, ents, nents, timeout_ms,
				 res);
}

int sdxi_abi_create_cxt(int fd, const struct sdxi_abi_cxt_create_req *req,
			struct sdxi_abi_cxt_create_resp *resp)
{
	struct sdxi_create_cxt_args args;
	int ret;

	if (!req || !resp)
		return -EINVAL;

	memset(resp, 0, sizeof(*resp));

	memset(&args, 0, sizeof(args));
	args.argsz = sizeof(args);
	args.flags = 0;
	args.opb_000_enb = req->opb_000_enb;
	args.ds_ring_entries = req->ds_ring_entries;
	args.ds_ring_base = req->ds_ring_base;
	args.cxt_sts_base = req->cxt_sts_base;
	args.write_index_base = req->write_index_base;

	ret = sdxi_ioctl(fd, SDXI_CREATE_CXT, &args);
	if (ret)
		return ret;
	if (args.cxt_fd < 0)
		return -EIO;

	resp->pasid = args.pasid;
	resp->cxt_fd = args.cxt_fd;
	resp->ds_ring_entries = args.ds_ring_entries;
	resp->doorbell_mmap_base = args.doorbell_mmap_base;
	return 0;
}

int sdxi_abi_cxt_eventfd(int cxt_fd, int eventfd, uint16_t *intr_akey_out,
			 uint32_t *intr_num_out)
{
	struct sdxi_cxt_eventfd_args args;
	int ret;

	if (eventfd < -1)
		return -EINVAL;

	memset(&args, 0, sizeof(args));
	args.argsz = sizeof(args);
	args.flags = 0;
	args.eventfd = eventfd;

	ret = sdxi_ioctl(cxt_fd, SDXI_CXT_EVENTFD, &args);
	if (ret)
		return ret;

	if (intr_akey_out)
		*intr_akey_out = args.intr_akey;
	if (intr_num_out)
		*intr_num_out = args.intr_num;
	return 0;
}

int sdxi_abi_quiesce_cxt(int cxt_fd, uint32_t timeout_ms,
			 uint32_t *cst_flags_out)
{
	struct sdxi_quiesce_cxt_args args;
	int ret;

	memset(&args, 0, sizeof(args));
	args.argsz = sizeof(args);
	args.flags = 0;
	args.timeout_ms = timeout_ms;

	ret = sdxi_ioctl(cxt_fd, SDXI_QUIESCE_CXT, &args);
	if (ret)
		return ret;

	if (cst_flags_out)
		*cst_flags_out = args.cst_flags;
	return 0;
}

int sdxi_abi_get_akey_list(int cxt_fd, struct sdxi_bitmap *out)
{
	return sdxi_abi_get_key_list(cxt_fd, SDXI_GET_AKEY_LIST, out);
}

int sdxi_abi_get_akeys(int cxt_fd, struct sdxi_key_ent *ents, size_t nents)
{
	return sdxi_abi_get_keys(cxt_fd, SDXI_GET_AKEYS, ents, nents);
}

int sdxi_abi_set_akeys(int cxt_fd, const struct sdxi_key_ent *ents,
		       size_t nents, uint32_t timeout_ms,
		       struct sdxi_key_set_result *res)
{
	return sdxi_abi_set_keys(cxt_fd, SDXI_SET_AKEYS, ents, nents,
				 timeout_ms, res);
}

static int sdxi_abi_admin_disc_map_err(int err)
{
	if (err == -EOPNOTSUPP || err == -ENOTTY)
		return -ENOTSUP;
	return err;
}

int sdxi_abi_admin_disc(int fd, uint16_t index_start, uint16_t index_end,
			void *buf, uint32_t *buf_bytes_inout,
			uint32_t timeout_ms, struct sdxi_admin_cpl *cpl_out)
{
	struct sdxi_admin_disc_args args;
	int ret;

	if (!buf_bytes_inout)
		return -EINVAL;
	if (index_start > index_end || index_end > SDXI_DISC_MAX_INDEX)
		return -EINVAL;
	if (!buf && *buf_bytes_inout)
		return -EINVAL;

	memset(&args, 0, sizeof(args));
	args.argsz = sizeof(args);
	args.flags = 0;
	args.timeout_ms = timeout_ms;
	args.index.start = index_start;
	args.index.end = index_end;
	args.buf_size = *buf_bytes_inout;
	args.buf_uptr = (uint64_t)(uintptr_t)buf;

	ret = sdxi_ioctl(fd, SDXI_ADMIN_DISC, &args);
	*buf_bytes_inout = args.buf_size;
	if (ret)
		return sdxi_abi_admin_disc_map_err(ret);

	if (cpl_out)
		cpl_out->cst_flags = args.cst_flags;
	return 0;
}

static int sdxi_abi_admin_vendor_map_err(int err)
{
	if (err == -EOPNOTSUPP || err == -ENOTTY)
		return -ENOTSUP;
	return err;
}

int sdxi_abi_admin_vendor(int fd,
			  const uint8_t guid[SDXI_VENDOR_GUID_BYTES],
			  uint32_t op, const void *in, uint32_t in_size,
			  void *out, uint32_t *out_bytes_inout,
			  uint32_t timeout_ms, struct sdxi_admin_cpl *cpl_out)
{
	struct sdxi_admin_vendor_args args;
	int ret;

	if (!guid)
		return -EINVAL;
	if (in_size && !in)
		return -EINVAL;
	if (out && !out_bytes_inout)
		return -EINVAL;
	if (!out && out_bytes_inout && *out_bytes_inout)
		return -EINVAL;

	memset(&args, 0, sizeof(args));
	args.argsz = sizeof(args);
	args.flags = 0;
	memcpy(args.guid, guid, sizeof(args.guid));
	args.op = op;
	args.timeout_ms = timeout_ms;
	args.in_uptr = (uint64_t)(uintptr_t)in;
	args.in_size = in_size;
	args.out_uptr = (uint64_t)(uintptr_t)out;
	args.out_size = out_bytes_inout ? *out_bytes_inout : 0;

	ret = sdxi_ioctl(fd, SDXI_ADMIN_VENDOR, &args);
	if (out_bytes_inout)
		*out_bytes_inout = args.out_size;
	if (ret)
		return sdxi_abi_admin_vendor_map_err(ret);

	if (cpl_out)
		cpl_out->cst_flags = args.cst_flags;
	return 0;
}
