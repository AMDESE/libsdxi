// SPDX-License-Identifier: BSD-3-Clause
#include "../sdxi_internal.h"

#include <errno.h>

#include <stdbool.h>
#include <string.h>

#include "../sdxi_dp.h"

#include <sdxi/vendor/amd_crypto.h>

static void sdxi_dsc_set_hdr(struct sdxi_desc *desc, uint32_t type,
			     uint32_t sub_type)
{
	desc->vl = 1;
	desc->type = type;
	desc->sub_type = sub_type;
	desc->comp_ptr = 1;
}

static void sdxi_dsc_build_amd_crypto(struct sdxi_desc *desc, uint16_t op_type,
				      uint8_t sub_type, uint32_t size_field,
				      uint64_t addr_src, uint8_t attr_src,
				      uint16_t akey_src, uint64_t addr_dst,
				      uint8_t attr_dst, uint16_t akey_dst,
				      uint64_t aes_key_ptr, uint64_t state_ptr,
				      uint32_t ctrl)
{
	memset(desc, 0, sizeof(*desc));
	desc->fe = 1;

	desc->body.amd_s2.size = size_field;
	desc->body.amd_s2.attr0 = attr_src;
	desc->body.amd_s2.attr1 = attr_dst;
	desc->body.amd_s2.akey0 = akey_src;
	desc->body.amd_s2.akey1 = akey_dst;
	desc->body.amd_s2.addr0 = addr_src;
	desc->body.amd_s2.addr1 = addr_dst;
	desc->body.amd_s2.aes_key_ptr = aes_key_ptr;
	desc->body.amd_s2.state_ptr = state_ptr;
	desc->body.amd_s2.ctrl = ctrl;

	sdxi_dsc_set_hdr(desc, op_type, sub_type);
}

void sdxi_dsc_build_amd_s1_aes_xts(struct sdxi_desc *desc, uint16_t op_type,
				   uint32_t size_field, uint64_t addr_src,
				   uint8_t attr_src, uint16_t akey_src,
				   uint64_t addr_dst, uint8_t attr_dst,
				   uint16_t akey_dst, uint64_t aes_key_ptr,
				   uint64_t state_ptr, uint8_t ed, uint8_t ks,
				   uint8_t xbs)
{
	uint32_t ctrl = 0;

	ctrl |= (uint32_t)(ed & 0x1) << 0;
	ctrl |= (uint32_t)(ks & 0x1) << 1;
	ctrl |= (uint32_t)(xbs & 0x3) << 4;

	sdxi_dsc_build_amd_crypto(desc, op_type, SDXI_OP_AMD_S1_AES_XTS,
				  size_field, addr_src, attr_src, akey_src,
				  addr_dst, attr_dst, akey_dst, aes_key_ptr,
				  state_ptr, ctrl);
}

void sdxi_dsc_build_amd_s1_aes_ecb(struct sdxi_desc *desc, uint16_t op_type,
				   uint32_t size_field, uint64_t addr_src,
				   uint8_t attr_src, uint16_t akey_src,
				   uint64_t addr_dst, uint8_t attr_dst,
				   uint16_t akey_dst, uint64_t aes_key_ptr,
				   uint64_t state_ptr, uint8_t ed, uint8_t ks)
{
	uint32_t ctrl = 0;

	ctrl |= (uint32_t)(ed & 0x1) << 0;
	ctrl |= (uint32_t)(ks & 0x1) << 1;

	sdxi_dsc_build_amd_crypto(desc, op_type, SDXI_OP_AMD_S1_AES_ECB,
				  size_field, addr_src, attr_src, akey_src,
				  addr_dst, attr_dst, akey_dst, aes_key_ptr,
				  state_ptr, ctrl);
}

void sdxi_dsc_build_amd_s2_aes_xts_c(struct sdxi_desc *desc, uint16_t op_type,
				     uint32_t size_field, uint64_t addr_src,
				     uint8_t attr_src, uint16_t akey_src,
				     uint64_t addr_dst, uint8_t attr_dst,
				     uint16_t akey_dst, uint64_t aes_key_ptr,
				     uint64_t state_ptr, uint8_t ed,
				     uint8_t ks, uint8_t xbs, uint8_t ic,
				     uint8_t ec)
{
	uint32_t ctrl = 0;

	ctrl |= (uint32_t)(ed & 0x1) << 0;
	ctrl |= (uint32_t)(ks & 0x1) << 1;
	ctrl |= (uint32_t)(xbs & 0x3) << 4;
	ctrl |= (uint32_t)(ic & 0x1) << 28;
	ctrl |= (uint32_t)(ec & 0x1) << 29;

	sdxi_dsc_build_amd_crypto(desc, op_type, SDXI_OP_AMD_S2_AES_XTS_C,
				  size_field, addr_src, attr_src, akey_src,
				  addr_dst, attr_dst, akey_dst, aes_key_ptr,
				  state_ptr, ctrl);
}

void sdxi_dsc_build_amd_s2_aes_ecb_c(struct sdxi_desc *desc, uint16_t op_type,
				     uint32_t size_field, uint64_t addr_src,
				     uint8_t attr_src, uint16_t akey_src,
				     uint64_t addr_dst, uint8_t attr_dst,
				     uint16_t akey_dst, uint64_t aes_key_ptr,
				     uint64_t state_ptr, uint8_t ed,
				     uint8_t ks, uint8_t ic, uint8_t ec)
{
	uint32_t ctrl = 0;

	ctrl |= (uint32_t)(ed & 0x1) << 0;
	ctrl |= (uint32_t)(ks & 0x1) << 1;
	ctrl |= (uint32_t)(ic & 0x1) << 28;
	ctrl |= (uint32_t)(ec & 0x1) << 29;

	sdxi_dsc_build_amd_crypto(desc, op_type, SDXI_OP_AMD_S2_AES_ECB_C,
				  size_field, addr_src, attr_src, akey_src,
				  addr_dst, attr_dst, akey_dst, aes_key_ptr,
				  state_ptr, ctrl);
}

void sdxi_dsc_build_amd_s2_sha(struct sdxi_desc *desc, uint16_t op_type,
			       uint32_t size_field, uint64_t addr_src,
			       uint8_t attr_src, uint16_t akey_src,
			       uint64_t state_ptr, uint8_t sha_mode,
			       uint8_t fin, uint8_t zbr, uint8_t ic)
{
	uint32_t ctrl = 0;

	memset(desc, 0, sizeof(*desc));
	desc->fe = 1;

	desc->body.amd_s2.size = size_field;
	desc->body.amd_s2.attr0 = attr_src;
	desc->body.amd_s2.akey0 = akey_src;
	desc->body.amd_s2.addr0 = addr_src;
	desc->body.amd_s2.state_ptr = state_ptr;

	ctrl |= (uint32_t)(sha_mode & 0xF) << 12;
	ctrl |= (uint32_t)(fin & 0x1) << 25;
	ctrl |= (uint32_t)(zbr & 0x1) << 26;
	ctrl |= (uint32_t)(ic & 0x1) << 28;
	desc->body.amd_s2.ctrl = ctrl;

	sdxi_dsc_set_hdr(desc, op_type, SDXI_OP_AMD_S2_SHA);
}

void sdxi_dsc_build_amd_s2_sha_copy(struct sdxi_desc *desc, uint16_t op_type,
				    uint32_t size_field, uint64_t addr_src,
				    uint8_t attr_src, uint16_t akey_src,
				    uint64_t addr_dst, uint8_t attr_dst,
				    uint16_t akey_dst, uint64_t state_ptr,
				    uint8_t sha_mode, uint8_t fin, uint8_t zbr,
				    uint8_t ic, uint8_t ec)
{
	uint32_t ctrl = 0;

	memset(desc, 0, sizeof(*desc));
	desc->fe = 1;

	desc->body.amd_s2.size = size_field;
	desc->body.amd_s2.attr0 = attr_src;
	desc->body.amd_s2.attr1 = attr_dst;
	desc->body.amd_s2.akey0 = akey_src;
	desc->body.amd_s2.akey1 = akey_dst;
	desc->body.amd_s2.addr0 = addr_src;
	desc->body.amd_s2.addr1 = addr_dst;
	desc->body.amd_s2.state_ptr = state_ptr;

	ctrl |= (uint32_t)(sha_mode & 0xF) << 12;
	ctrl |= (uint32_t)(fin & 0x1) << 25;
	ctrl |= (uint32_t)(zbr & 0x1) << 26;
	ctrl |= (uint32_t)(ic & 0x1) << 28;
	ctrl |= (uint32_t)(ec & 0x1) << 29;
	desc->body.amd_s2.ctrl = ctrl;

	sdxi_dsc_set_hdr(desc, op_type, SDXI_OP_AMD_S2_SHA_COPY);
}

void sdxi_dsc_build_amd_s2_aes_gcm(struct sdxi_desc *desc, uint16_t op_type,
				   uint32_t size_field, uint64_t addr_src,
				   uint8_t attr_src, uint16_t akey_src,
				   uint64_t addr_dst, uint8_t attr_dst,
				   uint16_t akey_dst, uint64_t aes_key_ptr,
				   uint64_t state_ptr, uint8_t ed, uint8_t ks,
				   uint8_t aad, uint8_t al, uint8_t fin,
				   uint8_t zbr, uint8_t ic, uint8_t ec)
{
	uint32_t ctrl = 0;

	memset(desc, 0, sizeof(*desc));
	desc->fe = 1;

	desc->body.amd_s2.size = size_field;
	desc->body.amd_s2.attr0 = attr_src;
	desc->body.amd_s2.attr1 = attr_dst;
	desc->body.amd_s2.akey0 = akey_src;
	desc->body.amd_s2.akey1 = akey_dst;
	desc->body.amd_s2.addr0 = addr_src;
	desc->body.amd_s2.addr1 = addr_dst;
	desc->body.amd_s2.aes_key_ptr = aes_key_ptr;
	desc->body.amd_s2.state_ptr = state_ptr;

	ctrl |= (uint32_t)(ed & 0x1) << 0;
	ctrl |= (uint32_t)(ks & 0x1) << 1;
	ctrl |= (uint32_t)(aad & 0x1) << 8;
	ctrl |= (uint32_t)(al & 0x1) << 9;
	ctrl |= (uint32_t)(fin & 0x1) << 25;
	ctrl |= (uint32_t)(zbr & 0x1) << 26;
	ctrl |= (uint32_t)(ic & 0x1) << 28;
	ctrl |= (uint32_t)(ec & 0x1) << 29;
	desc->body.amd_s2.ctrl = ctrl;

	sdxi_dsc_set_hdr(desc, op_type, SDXI_OP_AMD_S2_AES_GCM);
}

void sdxi_dsc_build_amd_s2_aes_ctr(struct sdxi_desc *desc, uint16_t op_type,
				   uint32_t size_field, uint64_t addr_src,
				   uint8_t attr_src, uint16_t akey_src,
				   uint64_t addr_dst, uint8_t attr_dst,
				   uint16_t akey_dst, uint64_t aes_key_ptr,
				   uint64_t state_ptr, uint8_t ed, uint8_t ks,
				   uint8_t ivs, uint8_t ic, uint8_t ec)
{
	uint32_t ctrl = 0;

	ctrl |= (uint32_t)(ed & 0x1) << 0;
	ctrl |= (uint32_t)(ks & 0x1) << 1;
	ctrl |= (uint32_t)(ivs & 0x1) << 2;
	ctrl |= (uint32_t)(ic & 0x1) << 28;
	ctrl |= (uint32_t)(ec & 0x1) << 29;

	sdxi_dsc_build_amd_crypto(desc, op_type, SDXI_OP_AMD_S2_AES_CTR,
				  size_field, addr_src, attr_src, akey_src,
				  addr_dst, attr_dst, akey_dst, aes_key_ptr,
				  state_ptr, ctrl);
}

void sdxi_dsc_build_amd_s2_aes_ctr_sha(
	struct sdxi_desc *desc, uint16_t op_type, uint32_t size_field,
	uint64_t addr_src, uint8_t attr_src, uint16_t akey_src,
	uint64_t addr_dst, uint8_t attr_dst, uint16_t akey_dst,
	uint64_t aes_key_ptr, uint64_t state_ptr, uint8_t ed, uint8_t ks,
	uint8_t ivs, uint8_t sha_mode, uint8_t sha_src, uint8_t fin,
	uint8_t zbr, uint8_t ic, uint8_t ec)
{
	uint32_t ctrl = 0;

	ctrl |= (uint32_t)(ed & 0x1) << 0;
	ctrl |= (uint32_t)(ks & 0x1) << 1;
	ctrl |= (uint32_t)(ivs & 0x1) << 2;
	ctrl |= (uint32_t)(sha_mode & 0xF) << 12;
	ctrl |= (uint32_t)(sha_src & 0x1) << 24;
	ctrl |= (uint32_t)(fin & 0x1) << 25;
	ctrl |= (uint32_t)(zbr & 0x1) << 26;
	ctrl |= (uint32_t)(ic & 0x1) << 28;
	ctrl |= (uint32_t)(ec & 0x1) << 29;

	sdxi_dsc_build_amd_crypto(desc, op_type, SDXI_OP_AMD_S2_AES_CTR_SHA,
				  size_field, addr_src, attr_src, akey_src,
				  addr_dst, attr_dst, akey_dst, aes_key_ptr,
				  state_ptr, ctrl);
}

void sdxi_dsc_build_amd_s2_aes_xts_sha(
	struct sdxi_desc *desc, uint16_t op_type, uint32_t size_field,
	uint64_t addr_src, uint8_t attr_src, uint16_t akey_src,
	uint64_t addr_dst, uint8_t attr_dst, uint16_t akey_dst,
	uint64_t aes_key_ptr, uint64_t state_ptr, uint8_t ed, uint8_t ks,
	uint8_t xbs, uint8_t sha_mode, uint8_t sha_src, uint8_t fin,
	uint8_t zbr, uint8_t ic, uint8_t ec)
{
	uint32_t ctrl = 0;

	ctrl |= (uint32_t)(ed & 0x1) << 0;
	ctrl |= (uint32_t)(ks & 0x1) << 1;
	ctrl |= (uint32_t)(xbs & 0x3) << 4;
	ctrl |= (uint32_t)(sha_mode & 0xF) << 12;
	ctrl |= (uint32_t)(sha_src & 0x1) << 24;
	ctrl |= (uint32_t)(fin & 0x1) << 25;
	ctrl |= (uint32_t)(zbr & 0x1) << 26;
	ctrl |= (uint32_t)(ic & 0x1) << 28;
	ctrl |= (uint32_t)(ec & 0x1) << 29;

	sdxi_dsc_build_amd_crypto(desc, op_type, SDXI_OP_AMD_S2_AES_XTS_SHA,
				  size_field, addr_src, attr_src, akey_src,
				  addr_dst, attr_dst, akey_dst, aes_key_ptr,
				  state_ptr, ctrl);
}

static void sdxi_dp_desc_export(struct sdxi_dp_desc *out,
				const struct sdxi_desc *in)
{
	memcpy(out->bytes, in, sizeof(*in));
}

static int sdxi_dp_req_size_ok(uint32_t size, size_t expect)
{
	if (size != 0 && size != expect)
		return -EINVAL;
	return 0;
}

static int sdxi_dp_check_rsvd_u32(const uint32_t *rsvd, size_t count)
{
	if (!rsvd)
		return -EINVAL;

	for (size_t i = 0; i < count; i++) {
		if (rsvd[i] != 0)
			return -EINVAL;
	}

	return 0;
}

static int sdxi_dp_amd_s2_size_field(size_t nbytes, uint8_t zero_byte_req,
				     uint32_t *size_field_out)
{
	uint64_t n64;

	if (!size_field_out)
		return -EINVAL;

	if (zero_byte_req) {
		if (nbytes)
			return -EINVAL;
		*size_field_out = 0;
		return 0;
	}

	if (nbytes == 0)
		return -EINVAL;

	n64 = (uint64_t)nbytes;
	if (n64 > (uint64_t)UINT32_MAX + 1)
		return -EOVERFLOW;

	*size_field_out = (uint32_t)(n64 - 1);
	return 0;
}

static int sdxi_dp_amd_crypto_size_field(size_t nbytes,
					 uint32_t *size_field_out)
{
	return sdxi_dp_amd_s2_size_field(nbytes, 0, size_field_out);
}

static int sdxi_dp_amd_crypto_common_ok(uint16_t op_type, uint8_t aes_op,
					uint8_t aes_key_size, uint8_t attr_src,
					uint8_t attr_dst, const void *src,
					const void *dst, size_t nbytes,
					const void *key, size_t key_align,
					const void *state, size_t state_align,
					uint32_t *size_field_out)
{
	int ret;

	if (op_type == 0 || op_type > SDXI_DISC_MAX_INDEX)
		return -EINVAL;
	if (aes_op > 1)
		return -EINVAL;
	if (aes_key_size > 1)
		return -EINVAL;
	if (attr_src > 0xF || attr_dst > 0xF)
		return -EINVAL;
	if (!src || !dst || !key || !state)
		return -EINVAL;
	if (((uintptr_t)key % key_align) != 0)
		return -EINVAL;
	if (((uintptr_t)state % state_align) != 0)
		return -EINVAL;

	ret = sdxi_dp_amd_crypto_size_field(nbytes, size_field_out);
	if (ret)
		return ret;

	return 0;
}

struct sdxi_dp_amd_s2_io_fields {
	uint64_t addr_src;
	uint64_t addr_dst;
	uint8_t attr_src;
	uint8_t attr_dst;
	uint16_t akey_src;
	uint16_t akey_dst;
	uint8_t ingress_crc;
	uint8_t egress_crc;
};

static int sdxi_dp_amd_s2_aes_sha_common_ok(
	uint16_t op_type, uint8_t aes_op, uint8_t aes_key_size,
	uint8_t sha_mode, uint8_t sha_src, uint8_t finalize,
	uint8_t zero_byte_req, uint8_t ingress_crc, uint8_t egress_crc,
	uint8_t attr_src, uint8_t attr_dst, uint16_t akey_src,
	uint16_t akey_dst, const void *src, const void *dst, size_t nbytes,
	const struct sdxi_amd_s2_aes_key *key,
	const struct sdxi_amd_s2_crypto_state *state,
	uint32_t *size_field_out, struct sdxi_dp_amd_s2_io_fields *io_out)
{
	int ret;

	if (!size_field_out || !io_out)
		return -EINVAL;
	if (op_type == 0 || op_type > SDXI_DISC_MAX_INDEX)
		return -EINVAL;
	if (aes_op > SDXI_AMD_S2_AES_OP_DECRYPT)
		return -EINVAL;
	if (aes_key_size > SDXI_AMD_S2_AES_KEY_SIZE_256)
		return -EINVAL;
	if (sha_mode > SDXI_AMD_S2_SHA_MODE_SHAKE256)
		return -EINVAL;
	if (sha_src > SDXI_AMD_S2_SHA_SRC_EGRESS)
		return -EINVAL;
	if (finalize > 1 || zero_byte_req > 1)
		return -EINVAL;
	if (ingress_crc > 1 || egress_crc > 1)
		return -EINVAL;
	if (attr_src > 0xF || attr_dst > 0xF)
		return -EINVAL;
	if (!key || !state)
		return -EINVAL;
	if (((uintptr_t)key % SDXI_AMD_S2_AES_KEY_ALIGN) != 0)
		return -EINVAL;
	if (((uintptr_t)state % SDXI_AMD_S2_CRYPTO_STATE_ALIGN) != 0)
		return -EINVAL;

	ret = sdxi_dp_amd_s2_size_field(nbytes, zero_byte_req,
					size_field_out);
	if (ret)
		return ret;

	memset(io_out, 0, sizeof(*io_out));
	if (zero_byte_req) {
		if (src || dst || attr_src || attr_dst || akey_src || akey_dst ||
		    ingress_crc || egress_crc)
			return -EINVAL;
		return 0;
	}

	if (!src || !dst)
		return -EINVAL;

	io_out->addr_src = (uint64_t)(uintptr_t)src;
	io_out->addr_dst = (uint64_t)(uintptr_t)dst;
	io_out->attr_src = attr_src;
	io_out->attr_dst = attr_dst;
	io_out->akey_src = akey_src;
	io_out->akey_dst = akey_dst;
	io_out->ingress_crc = ingress_crc;
	io_out->egress_crc = egress_crc;
	return 0;
}

int sdxi_dp_desc_build_amd_s1_aes_xts(
	struct sdxi_dp_desc *out, const struct sdxi_amd_s1_aes_xts_req *req)
{
	struct sdxi_desc desc;
	uint32_t size_field;
	int ret;

	if (!out || !req)
		return -EINVAL;
	ret = sdxi_dp_req_size_ok(req->size, sizeof(*req));
	if (ret)
		return ret;
	ret = sdxi_dp_check_rsvd_u32(req->rsvd,
				     sizeof(req->rsvd) / sizeof(req->rsvd[0]));
	if (ret || req->rsvd0)
		return -EINVAL;
	if (req->xts_block_size > SDXI_AMD_S1_XTS_BLOCK_SIZE_INFINITE)
		return -EINVAL;

	ret = sdxi_dp_amd_crypto_common_ok(
		req->op_type, req->aes_op, req->aes_key_size, req->attr_src,
		req->attr_dst, req->src, req->dst, req->nbytes, req->key,
		SDXI_AMD_S1_AES_KEY_ALIGN, req->state,
		SDXI_AMD_S1_CRYPTO_STATE_ALIGN, &size_field);
	if (ret)
		return ret;

	sdxi_dsc_build_amd_s1_aes_xts(
		&desc, req->op_type, size_field, (uint64_t)(uintptr_t)req->src,
		req->attr_src, req->akey_src, (uint64_t)(uintptr_t)req->dst,
		req->attr_dst, req->akey_dst, (uint64_t)(uintptr_t)req->key,
		(uint64_t)(uintptr_t)req->state, req->aes_op,
		req->aes_key_size, req->xts_block_size);
	sdxi_dp_desc_export(out, &desc);
	return 0;
}

int sdxi_dp_desc_build_amd_s1_aes_ecb(
	struct sdxi_dp_desc *out, const struct sdxi_amd_s1_aes_ecb_req *req)
{
	struct sdxi_desc desc;
	uint32_t size_field;
	int ret;

	if (!out || !req)
		return -EINVAL;
	ret = sdxi_dp_req_size_ok(req->size, sizeof(*req));
	if (ret)
		return ret;
	ret = sdxi_dp_check_rsvd_u32(req->rsvd,
				     sizeof(req->rsvd) / sizeof(req->rsvd[0]));
	if (ret || req->rsvd0)
		return -EINVAL;

	ret = sdxi_dp_amd_crypto_common_ok(
		req->op_type, req->aes_op, req->aes_key_size, req->attr_src,
		req->attr_dst, req->src, req->dst, req->nbytes, req->key,
		SDXI_AMD_S1_AES_KEY_ALIGN, req->state,
		SDXI_AMD_S1_CRYPTO_STATE_ALIGN, &size_field);
	if (ret)
		return ret;

	sdxi_dsc_build_amd_s1_aes_ecb(
		&desc, req->op_type, size_field, (uint64_t)(uintptr_t)req->src,
		req->attr_src, req->akey_src, (uint64_t)(uintptr_t)req->dst,
		req->attr_dst, req->akey_dst, (uint64_t)(uintptr_t)req->key,
		(uint64_t)(uintptr_t)req->state, req->aes_op,
		req->aes_key_size);
	sdxi_dp_desc_export(out, &desc);
	return 0;
}

int sdxi_dp_desc_build_amd_s2_aes_xts_c(
	struct sdxi_dp_desc *out, const struct sdxi_amd_s2_aes_xts_req *req)
{
	struct sdxi_desc desc;
	uint32_t size_field;
	int ret;

	if (!out || !req)
		return -EINVAL;
	ret = sdxi_dp_req_size_ok(req->size, sizeof(*req));
	if (ret)
		return ret;
	ret = sdxi_dp_check_rsvd_u32(req->rsvd,
				     sizeof(req->rsvd) / sizeof(req->rsvd[0]));
	if (ret || req->rsvd0 || req->rsvd1)
		return -EINVAL;
	if (req->xts_block_size > SDXI_AMD_S2_XTS_BLOCK_SIZE_INFINITE)
		return -EINVAL;
	if (req->ingress_crc > 1 || req->egress_crc > 1)
		return -EINVAL;

	ret = sdxi_dp_amd_crypto_common_ok(
		req->op_type, req->aes_op, req->aes_key_size, req->attr_src,
		req->attr_dst, req->src, req->dst, req->nbytes, req->key,
		SDXI_AMD_S2_AES_KEY_ALIGN, req->state,
		SDXI_AMD_S2_CRYPTO_STATE_ALIGN, &size_field);
	if (ret)
		return ret;

	sdxi_dsc_build_amd_s2_aes_xts_c(
		&desc, req->op_type, size_field, (uint64_t)(uintptr_t)req->src,
		req->attr_src, req->akey_src, (uint64_t)(uintptr_t)req->dst,
		req->attr_dst, req->akey_dst, (uint64_t)(uintptr_t)req->key,
		(uint64_t)(uintptr_t)req->state, req->aes_op,
		req->aes_key_size, req->xts_block_size, req->ingress_crc,
		req->egress_crc);
	sdxi_dp_desc_export(out, &desc);
	return 0;
}

int sdxi_dp_desc_build_amd_s2_aes_ecb_c(
	struct sdxi_dp_desc *out, const struct sdxi_amd_s2_aes_ecb_req *req)
{
	struct sdxi_desc desc;
	uint32_t size_field;
	int ret;

	if (!out || !req)
		return -EINVAL;
	ret = sdxi_dp_req_size_ok(req->size, sizeof(*req));
	if (ret)
		return ret;
	ret = sdxi_dp_check_rsvd_u32(req->rsvd,
				     sizeof(req->rsvd) / sizeof(req->rsvd[0]));
	if (ret)
		return ret;
	if (req->ingress_crc > 1 || req->egress_crc > 1)
		return -EINVAL;

	ret = sdxi_dp_amd_crypto_common_ok(
		req->op_type, req->aes_op, req->aes_key_size, req->attr_src,
		req->attr_dst, req->src, req->dst, req->nbytes, req->key,
		SDXI_AMD_S2_AES_KEY_ALIGN, req->state,
		SDXI_AMD_S2_CRYPTO_STATE_ALIGN, &size_field);
	if (ret)
		return ret;

	sdxi_dsc_build_amd_s2_aes_ecb_c(
		&desc, req->op_type, size_field, (uint64_t)(uintptr_t)req->src,
		req->attr_src, req->akey_src, (uint64_t)(uintptr_t)req->dst,
		req->attr_dst, req->akey_dst, (uint64_t)(uintptr_t)req->key,
		(uint64_t)(uintptr_t)req->state, req->aes_op,
		req->aes_key_size, req->ingress_crc, req->egress_crc);
	sdxi_dp_desc_export(out, &desc);
	return 0;
}

int sdxi_dp_desc_build_amd_s2_sha(struct sdxi_dp_desc *out,
				  const struct sdxi_amd_s2_sha_req *req)
{
	struct sdxi_desc desc;
	uint32_t size_field;
	int ret;

	if (!out || !req)
		return -EINVAL;
	ret = sdxi_dp_req_size_ok(req->size, sizeof(*req));
	if (ret)
		return ret;
	ret = sdxi_dp_check_rsvd_u32(req->rsvd,
				     sizeof(req->rsvd) / sizeof(req->rsvd[0]));
	if (ret)
		return ret;

	if (req->op_type == 0 || req->op_type > SDXI_DISC_MAX_INDEX)
		return -EINVAL;
	if (req->sha_mode > SDXI_AMD_S2_SHA_MODE_SHAKE256)
		return -EINVAL;
	if (req->sha_src != SDXI_AMD_S2_SHA_SRC_INGRESS)
		return -EINVAL;
	if (req->finalize > 1 || req->zero_byte_req > 1 || req->ingress_crc > 1)
		return -EINVAL;
	if (req->attr_src > 0xF)
		return -EINVAL;
	if (!req->state)
		return -EINVAL;
	if (((uintptr_t)req->state % SDXI_AMD_S2_CRYPTO_STATE_ALIGN) != 0)
		return -EINVAL;

	ret = sdxi_dp_amd_s2_size_field(req->nbytes, req->zero_byte_req,
					&size_field);
	if (ret)
		return ret;
	if (!req->zero_byte_req && !req->src)
		return -EINVAL;

	sdxi_dsc_build_amd_s2_sha(&desc, req->op_type, size_field,
				  (uint64_t)(uintptr_t)req->src, req->attr_src,
				  req->akey_src,
				  (uint64_t)(uintptr_t)req->state,
				  req->sha_mode, req->finalize,
				  req->zero_byte_req, req->ingress_crc);
	sdxi_dp_desc_export(out, &desc);
	return 0;
}

int sdxi_dp_desc_build_amd_s2_sha_copy(
	struct sdxi_dp_desc *out, const struct sdxi_amd_s2_sha_copy_req *req)
{
	struct sdxi_desc desc;
	uint32_t size_field;
	uint64_t addr_src = 0;
	uint64_t addr_dst = 0;
	uint8_t attr_src = 0;
	uint8_t attr_dst = 0;
	uint16_t akey_src = 0;
	uint16_t akey_dst = 0;
	uint8_t ic = 0;
	uint8_t ec = 0;
	int ret;

	if (!out || !req)
		return -EINVAL;
	ret = sdxi_dp_req_size_ok(req->size, sizeof(*req));
	if (ret)
		return ret;
	ret = sdxi_dp_check_rsvd_u32(req->rsvd,
				     sizeof(req->rsvd) / sizeof(req->rsvd[0]));
	if (ret || req->rsvd0)
		return -EINVAL;

	if (req->op_type == 0 || req->op_type > SDXI_DISC_MAX_INDEX)
		return -EINVAL;
	if (req->sha_mode > SDXI_AMD_S2_SHA_MODE_SHAKE256)
		return -EINVAL;
	if (req->finalize > 1 || req->zero_byte_req > 1)
		return -EINVAL;
	if (req->ingress_crc > 1 || req->egress_crc > 1)
		return -EINVAL;
	if (req->attr_src > 0xF || req->attr_dst > 0xF)
		return -EINVAL;
	if (!req->state)
		return -EINVAL;
	if (((uintptr_t)req->state % SDXI_AMD_S2_CRYPTO_STATE_ALIGN) != 0)
		return -EINVAL;

	ret = sdxi_dp_amd_s2_size_field(req->nbytes, req->zero_byte_req,
					&size_field);
	if (ret)
		return ret;

	if (req->zero_byte_req) {
		if (req->src || req->dst || req->attr_src || req->attr_dst ||
		    req->akey_src || req->akey_dst || req->ingress_crc ||
		    req->egress_crc)
			return -EINVAL;
	} else {
		if (!req->src || !req->dst)
			return -EINVAL;
		addr_src = (uint64_t)(uintptr_t)req->src;
		addr_dst = (uint64_t)(uintptr_t)req->dst;
		attr_src = req->attr_src;
		attr_dst = req->attr_dst;
		akey_src = req->akey_src;
		akey_dst = req->akey_dst;
		ic = req->ingress_crc;
		ec = req->egress_crc;
	}

	sdxi_dsc_build_amd_s2_sha_copy(
		&desc, req->op_type, size_field, addr_src, attr_src, akey_src,
		addr_dst, attr_dst, akey_dst, (uint64_t)(uintptr_t)req->state,
		req->sha_mode, req->finalize, req->zero_byte_req, ic, ec);
	sdxi_dp_desc_export(out, &desc);
	return 0;
}

int sdxi_dp_desc_build_amd_s2_aes_gcm(
	struct sdxi_dp_desc *out, const struct sdxi_amd_s2_aes_gcm_req *req)
{
	struct sdxi_desc desc;
	uint32_t size_field;
	uint64_t addr_dst = 0;
	uint8_t attr_dst = 0;
	uint16_t akey_dst = 0;
	uint8_t ec = 0;
	int ret;

	if (!out || !req)
		return -EINVAL;
	ret = sdxi_dp_req_size_ok(req->size, sizeof(*req));
	if (ret)
		return ret;
	ret = sdxi_dp_check_rsvd_u32(req->rsvd,
				     sizeof(req->rsvd) / sizeof(req->rsvd[0]));
	if (ret)
		return ret;

	if (req->op_type == 0 || req->op_type > SDXI_DISC_MAX_INDEX)
		return -EINVAL;
	if (req->aes_op > SDXI_AMD_S2_AES_OP_DECRYPT)
		return -EINVAL;
	if (req->aes_key_size > SDXI_AMD_S2_AES_KEY_SIZE_256)
		return -EINVAL;
	if (req->gcm_aad > 1 || req->gcm_last_aad > 1)
		return -EINVAL;
	if (!req->gcm_aad && req->gcm_last_aad)
		return -EINVAL;
	if (req->finalize > 1 || req->zero_byte_req > 1)
		return -EINVAL;
	if (req->ingress_crc > 1 || req->egress_crc > 1)
		return -EINVAL;
	if (req->attr_src > 0xF || req->attr_dst > 0xF)
		return -EINVAL;
	if (!req->key || !req->state)
		return -EINVAL;
	if (((uintptr_t)req->key % SDXI_AMD_S2_AES_KEY_ALIGN) != 0)
		return -EINVAL;
	if (((uintptr_t)req->state % SDXI_AMD_S2_CRYPTO_STATE_ALIGN) != 0)
		return -EINVAL;

	ret = sdxi_dp_amd_s2_size_field(req->nbytes, req->zero_byte_req,
					&size_field);
	if (ret)
		return ret;
	if (!req->zero_byte_req && !req->src)
		return -EINVAL;

	if (req->gcm_aad) {
		if (req->dst || req->egress_crc)
			return -EINVAL;
	} else {
		if (!req->zero_byte_req && !req->dst)
			return -EINVAL;
		ec = req->egress_crc;
		addr_dst = (uint64_t)(uintptr_t)req->dst;
		attr_dst = req->attr_dst;
		akey_dst = req->akey_dst;
	}

	sdxi_dsc_build_amd_s2_aes_gcm(
		&desc, req->op_type, size_field, (uint64_t)(uintptr_t)req->src,
		req->attr_src, req->akey_src, addr_dst, attr_dst, akey_dst,
		(uint64_t)(uintptr_t)req->key, (uint64_t)(uintptr_t)req->state,
		req->aes_op, req->aes_key_size, req->gcm_aad, req->gcm_last_aad,
		req->finalize, req->zero_byte_req, req->ingress_crc, ec);
	sdxi_dp_desc_export(out, &desc);
	return 0;
}

int sdxi_dp_desc_build_amd_s2_aes_ctr(
	struct sdxi_dp_desc *out, const struct sdxi_amd_s2_aes_ctr_req *req)
{
	struct sdxi_desc desc;
	uint32_t size_field;
	int ret;

	if (!out || !req)
		return -EINVAL;
	ret = sdxi_dp_req_size_ok(req->size, sizeof(*req));
	if (ret)
		return ret;
	ret = sdxi_dp_check_rsvd_u32(req->rsvd,
				     sizeof(req->rsvd) / sizeof(req->rsvd[0]));
	if (ret || req->rsvd0 || req->rsvd1)
		return -EINVAL;
	if (req->ctr_iv_size > SDXI_AMD_S2_AES_CTR_IV_SIZE_16B)
		return -EINVAL;
	if (req->ingress_crc > 1 || req->egress_crc > 1)
		return -EINVAL;

	ret = sdxi_dp_amd_crypto_common_ok(
		req->op_type, req->aes_op, req->aes_key_size, req->attr_src,
		req->attr_dst, req->src, req->dst, req->nbytes, req->key,
		SDXI_AMD_S2_AES_KEY_ALIGN, req->state,
		SDXI_AMD_S2_CRYPTO_STATE_ALIGN, &size_field);
	if (ret)
		return ret;

	sdxi_dsc_build_amd_s2_aes_ctr(
		&desc, req->op_type, size_field, (uint64_t)(uintptr_t)req->src,
		req->attr_src, req->akey_src, (uint64_t)(uintptr_t)req->dst,
		req->attr_dst, req->akey_dst, (uint64_t)(uintptr_t)req->key,
		(uint64_t)(uintptr_t)req->state, req->aes_op,
		req->aes_key_size, req->ctr_iv_size, req->ingress_crc,
		req->egress_crc);
	sdxi_dp_desc_export(out, &desc);
	return 0;
}

int sdxi_dp_desc_build_amd_s2_aes_ctr_sha(
	struct sdxi_dp_desc *out,
	const struct sdxi_amd_s2_aes_ctr_sha_req *req)
{
	struct sdxi_dp_amd_s2_io_fields io;
	struct sdxi_desc desc;
	uint32_t size_field;
	int ret;

	if (!out || !req)
		return -EINVAL;
	ret = sdxi_dp_req_size_ok(req->size, sizeof(*req));
	if (ret)
		return ret;
	ret = sdxi_dp_check_rsvd_u32(req->rsvd,
				     sizeof(req->rsvd) / sizeof(req->rsvd[0]));
	if (ret || req->rsvd0 || req->rsvd1)
		return -EINVAL;
	if (req->ctr_iv_size > SDXI_AMD_S2_AES_CTR_IV_SIZE_16B)
		return -EINVAL;

	ret = sdxi_dp_amd_s2_aes_sha_common_ok(
		req->op_type, req->aes_op, req->aes_key_size, req->sha_mode,
		req->sha_src, req->finalize, req->zero_byte_req,
		req->ingress_crc, req->egress_crc, req->attr_src,
		req->attr_dst, req->akey_src, req->akey_dst, req->src,
		req->dst, req->nbytes, req->key, req->state, &size_field, &io);
	if (ret)
		return ret;

	sdxi_dsc_build_amd_s2_aes_ctr_sha(
		&desc, req->op_type, size_field, io.addr_src, io.attr_src,
		io.akey_src, io.addr_dst, io.attr_dst, io.akey_dst,
		(uint64_t)(uintptr_t)req->key, (uint64_t)(uintptr_t)req->state,
		req->aes_op, req->aes_key_size, req->ctr_iv_size,
		req->sha_mode, req->sha_src, req->finalize,
		req->zero_byte_req, io.ingress_crc, io.egress_crc);
	sdxi_dp_desc_export(out, &desc);
	return 0;
}

int sdxi_dp_desc_build_amd_s2_aes_xts_sha(
	struct sdxi_dp_desc *out,
	const struct sdxi_amd_s2_aes_xts_sha_req *req)
{
	struct sdxi_dp_amd_s2_io_fields io;
	struct sdxi_desc desc;
	uint32_t size_field;
	int ret;

	if (!out || !req)
		return -EINVAL;
	ret = sdxi_dp_req_size_ok(req->size, sizeof(*req));
	if (ret)
		return ret;
	ret = sdxi_dp_check_rsvd_u32(req->rsvd,
				     sizeof(req->rsvd) / sizeof(req->rsvd[0]));
	if (ret || req->rsvd0 || req->rsvd1)
		return -EINVAL;
	if (req->xts_block_size > SDXI_AMD_S2_XTS_BLOCK_SIZE_INFINITE)
		return -EINVAL;

	ret = sdxi_dp_amd_s2_aes_sha_common_ok(
		req->op_type, req->aes_op, req->aes_key_size, req->sha_mode,
		req->sha_src, req->finalize, req->zero_byte_req,
		req->ingress_crc, req->egress_crc, req->attr_src,
		req->attr_dst, req->akey_src, req->akey_dst, req->src,
		req->dst, req->nbytes, req->key, req->state, &size_field, &io);
	if (ret)
		return ret;

	sdxi_dsc_build_amd_s2_aes_xts_sha(
		&desc, req->op_type, size_field, io.addr_src, io.attr_src,
		io.akey_src, io.addr_dst, io.attr_dst, io.akey_dst,
		(uint64_t)(uintptr_t)req->key, (uint64_t)(uintptr_t)req->state,
		req->aes_op, req->aes_key_size, req->xts_block_size,
		req->sha_mode, req->sha_src, req->finalize,
		req->zero_byte_req, io.ingress_crc, io.egress_crc);
	sdxi_dp_desc_export(out, &desc);
	return 0;
}

int sdxi_amd_s1_aes_xts(sdxi_cxt_t *cxt,
			const struct sdxi_amd_s1_aes_xts_req *req,
			uint32_t timeout_ms)
{
	struct sdxi_dp_desc raw;
	int ret;

	if (!cxt || !req)
		return -EINVAL;

	ret = sdxi_dp_desc_build_amd_s1_aes_xts(&raw, req);
	if (ret)
		return ret;
	return sdxi_cxt_submit_public_desc(cxt, &raw, timeout_ms);
}

int sdxi_amd_s1_aes_ecb(sdxi_cxt_t *cxt,
			const struct sdxi_amd_s1_aes_ecb_req *req,
			uint32_t timeout_ms)
{
	struct sdxi_dp_desc raw;
	int ret;

	if (!cxt || !req)
		return -EINVAL;

	ret = sdxi_dp_desc_build_amd_s1_aes_ecb(&raw, req);
	if (ret)
		return ret;
	return sdxi_cxt_submit_public_desc(cxt, &raw, timeout_ms);
}

int sdxi_amd_s2_aes_xts_c(sdxi_cxt_t *cxt,
			  const struct sdxi_amd_s2_aes_xts_req *req,
			  uint32_t timeout_ms)
{
	struct sdxi_dp_desc raw;
	int ret;

	if (!cxt || !req)
		return -EINVAL;

	ret = sdxi_dp_desc_build_amd_s2_aes_xts_c(&raw, req);
	if (ret)
		return ret;
	return sdxi_cxt_submit_public_desc(cxt, &raw, timeout_ms);
}

int sdxi_amd_s2_aes_ecb_c(sdxi_cxt_t *cxt,
			  const struct sdxi_amd_s2_aes_ecb_req *req,
			  uint32_t timeout_ms)
{
	struct sdxi_dp_desc raw;
	int ret;

	if (!cxt || !req)
		return -EINVAL;

	ret = sdxi_dp_desc_build_amd_s2_aes_ecb_c(&raw, req);
	if (ret)
		return ret;
	return sdxi_cxt_submit_public_desc(cxt, &raw, timeout_ms);
}

int sdxi_amd_s2_sha(sdxi_cxt_t *cxt, const struct sdxi_amd_s2_sha_req *req,
		    uint32_t timeout_ms)
{
	struct sdxi_dp_desc raw;
	int ret;

	if (!cxt || !req)
		return -EINVAL;

	ret = sdxi_dp_desc_build_amd_s2_sha(&raw, req);
	if (ret)
		return ret;
	return sdxi_cxt_submit_public_desc(cxt, &raw, timeout_ms);
}

int sdxi_amd_s2_sha_copy(sdxi_cxt_t *cxt,
			 const struct sdxi_amd_s2_sha_copy_req *req,
			 uint32_t timeout_ms)
{
	struct sdxi_dp_desc raw;
	int ret;

	if (!cxt || !req)
		return -EINVAL;

	ret = sdxi_dp_desc_build_amd_s2_sha_copy(&raw, req);
	if (ret)
		return ret;
	return sdxi_cxt_submit_public_desc(cxt, &raw, timeout_ms);
}

int sdxi_amd_s2_aes_gcm(sdxi_cxt_t *cxt,
			const struct sdxi_amd_s2_aes_gcm_req *req,
			uint32_t timeout_ms)
{
	struct sdxi_dp_desc raw;
	int ret;

	if (!cxt || !req)
		return -EINVAL;

	ret = sdxi_dp_desc_build_amd_s2_aes_gcm(&raw, req);
	if (ret)
		return ret;
	return sdxi_cxt_submit_public_desc(cxt, &raw, timeout_ms);
}

int sdxi_amd_s2_aes_ctr(sdxi_cxt_t *cxt,
			const struct sdxi_amd_s2_aes_ctr_req *req,
			uint32_t timeout_ms)
{
	struct sdxi_dp_desc raw;
	int ret;

	if (!cxt || !req)
		return -EINVAL;

	ret = sdxi_dp_desc_build_amd_s2_aes_ctr(&raw, req);
	if (ret)
		return ret;
	return sdxi_cxt_submit_public_desc(cxt, &raw, timeout_ms);
}

int sdxi_amd_s2_aes_ctr_sha(
	sdxi_cxt_t *cxt, const struct sdxi_amd_s2_aes_ctr_sha_req *req,
	uint32_t timeout_ms)
{
	struct sdxi_dp_desc raw;
	int ret;

	if (!cxt || !req)
		return -EINVAL;

	ret = sdxi_dp_desc_build_amd_s2_aes_ctr_sha(&raw, req);
	if (ret)
		return ret;
	return sdxi_cxt_submit_public_desc(cxt, &raw, timeout_ms);
}

int sdxi_amd_s2_aes_xts_sha(
	sdxi_cxt_t *cxt, const struct sdxi_amd_s2_aes_xts_sha_req *req,
	uint32_t timeout_ms)
{
	struct sdxi_dp_desc raw;
	int ret;

	if (!cxt || !req)
		return -EINVAL;

	ret = sdxi_dp_desc_build_amd_s2_aes_xts_sha(&raw, req);
	if (ret)
		return ret;
	return sdxi_cxt_submit_public_desc(cxt, &raw, timeout_ms);
}
