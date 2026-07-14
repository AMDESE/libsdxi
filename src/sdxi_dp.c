// SPDX-License-Identifier: BSD-3-Clause
#include "sdxi_internal.h"

#include <errno.h>

#include <stdbool.h>
#include <string.h>

#include "sdxi_dp.h"

static void sdxi_dsc_set_hdr(struct sdxi_desc *desc, uint32_t type,
			     uint32_t sub_type)
{
	desc->vl = 1;
	desc->type = type;
	desc->sub_type = sub_type;
	desc->comp_ptr = 1;
}

void sdxi_dsc_build_dma_nop(struct sdxi_desc *desc)
{
	memset(desc, 0, sizeof(*desc));
	desc->fe = 1;

	sdxi_dsc_set_hdr(desc, SDXI_OP_TYPE_DMA, SDXI_OP_DMA_NOP);
}

void sdxi_dsc_build_dma_copy(struct sdxi_desc *desc, uint64_t addr_src,
			     uint8_t attr_src, uint16_t src_akey,
			     uint64_t addr_dst, uint8_t attr_dst,
			     uint16_t dst_akey, uint32_t size)
{
	memset(desc, 0, sizeof(*desc));
	desc->fe = 1;

	/* size field is "bytes minus 1" */
	desc->body.dmabasegrp.copy.size = size - 1;
	desc->body.dmabasegrp.copy.attr_src = attr_src;
	desc->body.dmabasegrp.copy.attr_dst = attr_dst;
	desc->body.dmabasegrp.copy.akey0 = src_akey;
	desc->body.dmabasegrp.copy.akey1 = dst_akey;
	desc->body.dmabasegrp.copy.addr0 = addr_src;
	desc->body.dmabasegrp.copy.addr1 = addr_dst;

	sdxi_dsc_set_hdr(desc, SDXI_OP_TYPE_DMA, SDXI_OP_DMA_COPY);
}

void sdxi_dsc_build_dma_rep_copy(struct sdxi_desc *desc, uint64_t addr_src,
				 uint8_t attr_src, uint16_t src_akey,
				 uint64_t addr_dst, uint8_t attr_dst,
				 uint16_t dst_akey, uint32_t size_4k,
				 uint32_t repetitions)
{
	memset(desc, 0, sizeof(*desc));
	desc->fe = 1;

	/* size is in 4KB units, encoded as (n - 1). */
	desc->body.dmabasegrp.repcopy.nsize = size_4k - 1;
	desc->body.dmabasegrp.repcopy.attr_src = attr_src;
	desc->body.dmabasegrp.repcopy.attr_dst = attr_dst;
	desc->body.dmabasegrp.repcopy.akey0 = src_akey;
	desc->body.dmabasegrp.repcopy.akey1 = dst_akey;
	desc->body.dmabasegrp.repcopy.addr0_src = addr_src >> 12;
	desc->body.dmabasegrp.repcopy.addr1_dst = addr_dst >> 12;
	/* repeat count is encoded as (n - 1). */
	desc->body.dmabasegrp.repcopy.num = repetitions - 1;

	sdxi_dsc_set_hdr(desc, SDXI_OP_TYPE_DMA, SDXI_OP_DMA_REP_COPY);
}

void sdxi_dsc_build_dma_write_imm(struct sdxi_desc *desc, uint64_t addr_dst,
				  uint8_t attr_dst, uint16_t dst_akey,
				  const uint8_t *data, uint32_t size)
{
	memset(desc, 0, sizeof(*desc));
	desc->fe = 1;

	desc->body.dmabasegrp.wrt_imm.bsize = size - 1;
	desc->body.dmabasegrp.wrt_imm.attr_dst = attr_dst;
	desc->body.dmabasegrp.wrt_imm.akey0 = dst_akey;
	desc->body.dmabasegrp.wrt_imm.addr0 = addr_dst;

	if (size > SDXI_DESC_WRT_IMM_DATA_DWORDS * sizeof(uint32_t))
		size = SDXI_DESC_WRT_IMM_DATA_DWORDS * sizeof(uint32_t);

	memcpy(desc->body.dmabasegrp.wrt_imm.data, data, size);
	sdxi_dsc_set_hdr(desc, SDXI_OP_TYPE_DMA, SDXI_OP_DMA_WRT_IMM);
}

void sdxi_dsc_build_atomic_uadd(struct sdxi_desc *desc,
				uint8_t operand_bytes, uint64_t addr_dst,
				uint8_t attr_dst, uint16_t dst_akey,
				uint64_t operand, uint64_t ret_ptr)
{
	memset(desc, 0, sizeof(*desc));
	desc->fe = 1;

	desc->body.atomicgrp.osz = operand_bytes / 4 - 1;
	desc->body.atomicgrp.attr_dst = attr_dst;
	desc->body.atomicgrp.akey0 = dst_akey;
	desc->body.atomicgrp.n = operand_bytes == 4 ?
		(uint8_t)((addr_dst >> 2) & 0x1) : 0;
	desc->body.atomicgrp.addr0_dst = addr_dst >> 3;
	desc->body.atomicgrp.op1 = operand;
	desc->body.atomicgrp.op2 = 0;
	desc->body.atomicgrp.ret_data_ptr = ret_ptr >> 2;
	desc->body.atomicgrp.nr = !ret_ptr;

	sdxi_dsc_set_hdr(desc, SDXI_OP_TYPE_ATOMIC, SDXI_OP_ATOMIC_UADD);
}

void sdxi_dsc_build_intr(struct sdxi_desc *desc, uint16_t akey)
{
	memset(desc, 0, sizeof(*desc));
	desc->fe = 1;

	desc->body.intrgrp.akey = akey;
	sdxi_dsc_set_hdr(desc, SDXI_OP_TYPE_INTR, SDXI_OP_INTR_INTERRUPT);
}

void sdxi_dsc_build_crcm1(struct sdxi_desc *desc, uint16_t op_type,
			  uint8_t sub_type, uint32_t size_bytes,
			  uint64_t addr_src, uint8_t attr_src,
			  uint16_t src_akey, uint64_t addr_dst,
			  uint8_t attr_dst, uint16_t dst_akey, uint64_t seed,
			  uint64_t crc_dst_addr, uint8_t blksz,
			  uint8_t meta_src, uint8_t meta_dst, uint8_t seed_loc,
			  uint8_t crcsel, uint8_t reflect, uint8_t invert)
{
	uint16_t crc_ctrl = 0;
	bool has_copy;

	memset(desc, 0, sizeof(*desc));
	desc->fe = 1;

	/* size field is "bytes minus 1" */
	desc->body.crcm1grp.size = size_bytes - 1;
	desc->body.crcm1grp.attr_src = attr_src;
	desc->body.crcm1grp.akey0 = src_akey;
	desc->body.crcm1grp.addr0 = addr_src;

	has_copy = sub_type == SDXI_OP_CRCM1_COPY ||
		   sub_type == SDXI_OP_CRCM1_COPY_BLK;
	if (has_copy) {
		desc->body.crcm1grp.attr_dst = attr_dst;
		desc->body.crcm1grp.akey1 = dst_akey;
		desc->body.crcm1grp.addr1 = addr_dst;
	}

	desc->body.crcm1grp.seed = seed;
	desc->body.crcm1grp.crc_dst_addr = crc_dst_addr;

	crc_ctrl |= (uint16_t)(blksz & 0x7);
	crc_ctrl |= (uint16_t)((meta_src & 0x1) << 3);
	crc_ctrl |= (uint16_t)((meta_dst & 0x1) << 4);
	crc_ctrl |= (uint16_t)((seed_loc & 0x1) << 5);
	crc_ctrl |= (uint16_t)((crcsel & 0x7) << 6);
	crc_ctrl |= (uint16_t)((reflect & 0x3) << 9);
	crc_ctrl |= (uint16_t)((invert & 0x3) << 11);
	desc->body.crcm1grp.crc_ctrl = crc_ctrl;

	sdxi_dsc_set_hdr(desc, op_type, sub_type);
}

static void sdxi_dp_desc_export(struct sdxi_dp_desc *out,
				const struct sdxi_desc *in)
{
	memcpy(out->bytes, in, sizeof(*in));
}

static void sdxi_dp_desc_import(struct sdxi_desc *out,
				const struct sdxi_dp_desc *in)
{
	memcpy(out, in->bytes, sizeof(*out));
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

static int sdxi_dp_crc_m1_crc_bytes(uint8_t crcsel, size_t *bytes_out)
{
	if (!bytes_out)
		return -EINVAL;

	switch (crcsel) {
	case SDXI_CRC_M1_CRCSEL_CRC16_8BB7:
		*bytes_out = 2;
		return 0;
	case SDXI_CRC_M1_CRCSEL_CRC32C_1EDC6F41:
	case SDXI_CRC_M1_CRCSEL_CRC32_04C11DB7:
		*bytes_out = 4;
		return 0;
	case SDXI_CRC_M1_CRCSEL_CRC64_AD93D23594C93659:
		*bytes_out = 8;
		return 0;
	default:
		return -EINVAL;
	}
}

static int sdxi_dp_crc_m1_blk_info(uint8_t blksz, uint32_t *blk_bytes_out,
				   uint32_t *meta_bytes_out)
{
	uint32_t blk_bytes;
	uint32_t meta_bytes;

	switch (blksz) {
	case SDXI_CRC_M1_BLKSZ_512B_NO_MD:
		blk_bytes = 512;
		meta_bytes = 0;
		break;
	case SDXI_CRC_M1_BLKSZ_512B_8B_MD:
		blk_bytes = 512;
		meta_bytes = 8;
		break;
	case SDXI_CRC_M1_BLKSZ_4096B_NO_MD:
		blk_bytes = 4096;
		meta_bytes = 0;
		break;
	case SDXI_CRC_M1_BLKSZ_4096B_16B_MD:
		blk_bytes = 4096;
		meta_bytes = 16;
		break;
	case SDXI_CRC_M1_BLKSZ_4096B_8B_MD:
		blk_bytes = 4096;
		meta_bytes = 8;
		break;
	default:
		return -EINVAL;
	}

	if (blk_bytes_out)
		*blk_bytes_out = blk_bytes;
	if (meta_bytes_out)
		*meta_bytes_out = meta_bytes;
	return 0;
}

void sdxi_dp_csb_clear(struct sdxi_dp_csb *csb)
{
	if (!csb)
		return;

	memset(csb, 0, sizeof(*csb));
}

uint64_t sdxi_dp_csb_signal(const struct sdxi_dp_csb *csb)
{
	if (!csb)
		return 0;

	return __atomic_load_n(&csb->signal, __ATOMIC_ACQUIRE);
}

uint32_t sdxi_dp_csb_flags(const struct sdxi_dp_csb *csb)
{
	if (!csb)
		return 0;

	return __atomic_load_n(&csb->flags, __ATOMIC_ACQUIRE);
}

bool sdxi_dp_csb_done(const struct sdxi_dp_csb *csb)
{
	return sdxi_dp_csb_signal(csb) != 0;
}

bool sdxi_dp_csb_error(const struct sdxi_dp_csb *csb)
{
	return (sdxi_dp_csb_flags(csb) & SDXI_DP_CSB_F_ERROR) != 0;
}

int sdxi_dp_desc_set_csb(struct sdxi_dp_desc *desc, struct sdxi_dp_csb *csb)
{
	struct sdxi_desc tmp;

	if (!desc || !csb)
		return -EINVAL;
	if (((uintptr_t)csb % SDXI_DP_CSB_ALIGN) != 0)
		return -EINVAL;

	sdxi_dp_desc_import(&tmp, desc);
	tmp.csr = 1;
	tmp.comp_ptr = (uint64_t)(uintptr_t)csb;
	sdxi_dp_desc_export(desc, &tmp);

	return 0;
}

int sdxi_dp_desc_clear_csb(struct sdxi_dp_desc *desc)
{
	struct sdxi_desc tmp;

	if (!desc)
		return -EINVAL;

	sdxi_dp_desc_import(&tmp, desc);
	tmp.csr = 0;
	tmp.comp_ptr = 1;
	sdxi_dp_desc_export(desc, &tmp);

	return 0;
}

int sdxi_dp_desc_set_read_barrier(struct sdxi_dp_desc *desc, bool enable)
{
	struct sdxi_desc tmp;

	if (!desc)
		return -EINVAL;

	sdxi_dp_desc_import(&tmp, desc);
	tmp.rb = enable;
	sdxi_dp_desc_export(desc, &tmp);
	return 0;
}

int sdxi_dp_desc_build_dma_nop(struct sdxi_dp_desc *out)
{
	struct sdxi_desc desc;

	if (!out)
		return -EINVAL;

	sdxi_dsc_build_dma_nop(&desc);
	sdxi_dp_desc_export(out, &desc);
	return 0;
}

int sdxi_dp_desc_build_dma_copy(struct sdxi_dp_desc *out,
				const struct sdxi_dp_dma_copy_req *req)
{
	struct sdxi_desc desc;
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
	if (req->attr_src > 0xF || req->attr_dst > 0xF)
		return -EINVAL;
	if (req->nbytes == 0)
		return -EINVAL;

	sdxi_dsc_build_dma_copy(&desc, req->addr_src, req->attr_src,
				req->src_akey, req->addr_dst, req->attr_dst,
				req->dst_akey, req->nbytes);
	sdxi_dp_desc_export(out, &desc);
	return 0;
}

int sdxi_dp_desc_build_dma_rep_copy(
	struct sdxi_dp_desc *out, const struct sdxi_dp_dma_rep_copy_req *req)
{
	struct sdxi_desc desc;
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
	if (req->attr_src > 0xF || req->attr_dst > 0xF)
		return -EINVAL;
	if ((req->addr_src | req->addr_dst) & 4095u)
		return -EINVAL;
	if (req->size_4k == 0 || req->size_4k > 512)
		return -EINVAL;
	if (req->repeat == 0 || req->repeat > (1u << 20))
		return -EINVAL;

	sdxi_dsc_build_dma_rep_copy(&desc, req->addr_src, req->attr_src,
				    req->src_akey, req->addr_dst,
				    req->attr_dst, req->dst_akey,
				    req->size_4k, req->repeat);
	sdxi_dp_desc_export(out, &desc);
	return 0;
}

int sdxi_dp_desc_build_dma_write_imm(
	struct sdxi_dp_desc *out, const struct sdxi_dp_dma_write_imm_req *req)
{
	struct sdxi_desc desc;
	int ret;

	if (!out || !req)
		return -EINVAL;

	ret = sdxi_dp_req_size_ok(req->size, sizeof(*req));
	if (ret)
		return ret;
	ret = sdxi_dp_check_rsvd_u32(req->rsvd,
				     sizeof(req->rsvd) / sizeof(req->rsvd[0]));
	if (ret || req->rsvd1)
		return -EINVAL;
	if (req->rsvd0[0] || req->rsvd0[1] || req->rsvd0[2])
		return -EINVAL;
	if (req->attr_dst > 0xF)
		return -EINVAL;
	if (req->data_bytes == 0 || req->data_bytes > SDXI_DP_WRITE_IMM_MAX_BYTES)
		return -EINVAL;

	sdxi_dsc_build_dma_write_imm(&desc, req->addr_dst, req->attr_dst,
				     req->dst_akey, req->data,
				     req->data_bytes);
	sdxi_dp_desc_export(out, &desc);
	return 0;
}

int sdxi_dp_desc_build_atomic_uadd(
	struct sdxi_dp_desc *out, const struct sdxi_dp_atomic_uadd_req *req)
{
	struct sdxi_desc desc;
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
	if (req->attr_dst > 0xF)
		return -EINVAL;
	switch (req->operand_bytes) {
	case 4:
	case 8:
		break;
	default:
		return -EINVAL;
	}
	if ((req->addr_dst & (req->operand_bytes - 1u)) != 0)
		return -EINVAL;
	if (req->ret_data_addr != 0 &&
	    (req->ret_data_addr & (req->operand_bytes - 1u)) != 0)
		return -EINVAL;

	sdxi_dsc_build_atomic_uadd(&desc, req->operand_bytes, req->addr_dst,
				   req->attr_dst, req->dst_akey, req->operand,
				   req->ret_data_addr);
	sdxi_dp_desc_export(out, &desc);
	return 0;
}

int sdxi_dp_desc_build_intr(struct sdxi_dp_desc *out,
			    const struct sdxi_dp_intr_req *req)
{
	struct sdxi_desc desc;
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

	sdxi_dsc_build_intr(&desc, req->akey);
	sdxi_dp_desc_export(out, &desc);
	return 0;
}

int sdxi_dp_desc_build_crc_m1(struct sdxi_dp_desc *out,
			      const struct sdxi_crc_m1_req *req)
{
	struct sdxi_desc desc;
	bool is_blk;
	bool has_copy;
	uint64_t addr_dst = 0;
	uint8_t attr_dst = 0;
	uint16_t akey_dst = 0;
	uint32_t blk_bytes = 0;
	uint32_t meta_bytes = 0;
	size_t crc_bytes = 0;
	size_t need_crc_bytes = 0;
	size_t ncrcs = 1;
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

	if (!req->src || !req->crc_dst)
		return -EINVAL;
	if (req->nbytes == 0 || req->nbytes > UINT32_MAX)
		return -EINVAL;
	if (req->op_type == 0 || req->op_type > SDXI_DISC_MAX_INDEX)
		return -EINVAL;
	if (req->attr_src > 0xF || req->attr_dst > 0xF)
		return -EINVAL;

	if (req->sub_type > SDXI_CRC_M1_SUBTYPE_CALC_BLK)
		return -EINVAL;

	is_blk = req->sub_type == SDXI_CRC_M1_SUBTYPE_COPY_BLK ||
		 req->sub_type == SDXI_CRC_M1_SUBTYPE_CALC_BLK;
	has_copy = req->sub_type == SDXI_CRC_M1_SUBTYPE_COPY ||
		   req->sub_type == SDXI_CRC_M1_SUBTYPE_COPY_BLK;
	if (has_copy) {
		if (!req->dst)
			return -EINVAL;
		addr_dst = (uint64_t)(uintptr_t)req->dst;
		attr_dst = req->attr_dst;
		akey_dst = req->akey_dst;
	}

	if (req->seed_loc != SDXI_CRC_M1_SEEDLOC_COMMON &&
	    req->seed_loc != SDXI_CRC_M1_SEEDLOC_PTR)
		return -EINVAL;
	if (req->crcsel > SDXI_CRC_M1_CRCSEL_CRC64_AD93D23594C93659)
		return -EINVAL;
	if (req->reflect > SDXI_CRC_M1_REFLECT_DATA_SEED_CRC)
		return -EINVAL;
	if (req->invert > SDXI_CRC_M1_INVERT_SEED_CRC)
		return -EINVAL;

	ret = sdxi_dp_crc_m1_crc_bytes(req->crcsel, &crc_bytes);
	if (ret)
		return ret;

	if (((uintptr_t)req->crc_dst % crc_bytes) != 0)
		return -EINVAL;
	if (req->seed_loc == SDXI_CRC_M1_SEEDLOC_PTR) {
		if (!req->seed)
			return -EINVAL;
		if (((uintptr_t)req->seed % crc_bytes) != 0)
			return -EINVAL;
	}

	if (is_blk) {
		ret = sdxi_dp_crc_m1_blk_info(req->blksz, &blk_bytes,
					      &meta_bytes);
		if (ret)
			return ret;
		if ((req->nbytes % blk_bytes) != 0)
			return -EINVAL;

		if (meta_bytes == 0) {
			if (req->meta_src || req->meta_dst)
				return -EINVAL;
		} else if (req->sub_type != SDXI_CRC_M1_SUBTYPE_COPY_BLK &&
			   req->meta_dst) {
			return -EINVAL;
		}

		ncrcs = req->nbytes / blk_bytes;
	} else {
		if (req->blksz != 0)
			return -EINVAL;
		if (req->meta_src || req->meta_dst)
			return -EINVAL;
		ncrcs = 1;
	}

	if (ncrcs > SIZE_MAX / crc_bytes)
		return -EOVERFLOW;
	need_crc_bytes = ncrcs * crc_bytes;
	if (req->crc_dst_bytes < need_crc_bytes)
		return -ENOSPC;

	sdxi_dsc_build_crcm1(&desc, req->op_type, req->sub_type,
			     (uint32_t)req->nbytes,
			     (uint64_t)(uintptr_t)req->src, req->attr_src,
			     req->akey_src, addr_dst, attr_dst, akey_dst,
			     req->seed, (uint64_t)(uintptr_t)req->crc_dst,
			     req->blksz, req->meta_src, req->meta_dst,
			     req->seed_loc, req->crcsel, req->reflect,
			     req->invert);
	sdxi_dp_desc_export(out, &desc);
	return 0;
}
