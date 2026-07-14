// SPDX-License-Identifier: BSD-3-Clause
#ifndef SDXI_DP_INTERNAL_H
#define SDXI_DP_INTERNAL_H

#include <stdint.h>

#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

#ifndef __aligned
#define __aligned(x) __attribute__((aligned(x)))
#endif

/* SDXI descriptor size is fixed at 64 bytes. */
#define SDXI_DESC_BYTES 64u

/* SDXI operation group/type identifiers (from SDXI base spec). */
#define SDXI_OP_TYPE_DMA 0x001
#define SDXI_OP_TYPE_ADMIN 0x002
#define SDXI_OP_TYPE_ATOMIC 0x003
#define SDXI_OP_TYPE_INTR 0x004

/* DMA subgroup opcodes (subset used by current samples). */
#define SDXI_OP_DMA_NOP 0x01
#define SDXI_OP_DMA_WRT_IMM 0x02
#define SDXI_OP_DMA_COPY 0x03
#define SDXI_OP_DMA_REP_COPY 0x04

/* Atomic subgroup opcode (subset). */
#define SDXI_OP_ATOMIC_UADD 0x02

/* Intr subgroup opcode. */
#define SDXI_OP_INTR_INTERRUPT 0x00

/*
 * CRC Mode1 Operation Group (CrcM1Grp) subtypes (from SDXI base spec v1.0.3).
 *
 * Note: The descriptor header `type` value for CrcM1Grp is function-assigned
 * (1..2047) and must be discovered via AdminGrp DSC_DISC.
 */
#define SDXI_OP_CRCM1_COPY 0x00
#define SDXI_OP_CRCM1_CALC 0x01
#define SDXI_OP_CRCM1_COPY_BLK 0x02
#define SDXI_OP_CRCM1_CALC_BLK 0x03


/* Context state values (from SDXI base spec). */
#define SDXI_CXT_STATE_STOP_SW 0x0
#define SDXI_CXT_STATE_RUN 0x1
#define SDXI_CXT_STATE_STOPG_SW 0x2
#define SDXI_CXT_STATE_STOP_FN 0x4
#define SDXI_CXT_STATE_STOPG_FN 0x6
#define SDXI_CXT_STATE_ERR_FN 0xF

/* Private compatibility names used by existing diagnostic samples. */
#define SDXI_CXT_STATE_STOPPED SDXI_CXT_STATE_STOP_SW
#define SDXI_CXT_STATE_RUNNING SDXI_CXT_STATE_RUN
#define SDXI_CXT_STATE_STOPPING SDXI_CXT_STATE_STOPG_SW
#define SDXI_CXT_STATE_ERR SDXI_CXT_STATE_ERR_FN

struct sdxi_cxt_sts {
	uint8_t state; /* [3:0] */
	uint8_t misc0;
	uint8_t rsvd0[6];
	uint64_t read_index;
} __packed __aligned(16);

/* size of sdxi descriptor body in units of 32-bit chunks */
#define SDXI_DESC_BODY_DWORDS (0x34u / sizeof(uint32_t))

/* "32-bytes of immediate data" */
#define SDXI_DESC_WRT_IMM_DATA_DWORDS (32u / sizeof(uint32_t))

struct sdxi_desc_op_dmabasegrp_nop {
	uint32_t rsv[SDXI_DESC_BODY_DWORDS];
} __packed;

struct sdxi_desc_op_dmabasegrp_wrt_imm {
	uint64_t bsize : 5;
	uint64_t rsv1 : 27;
	uint64_t attr_dst : 4;
	uint64_t rsv2 : 4;
	uint64_t rsv3 : 24;
	uint64_t akey0 : 16;
	uint64_t rsv4 : 16;
	uint64_t addr0;
	uint32_t data[SDXI_DESC_WRT_IMM_DATA_DWORDS];
} __packed;

struct sdxi_desc_op_dmabasegrp_copy {
	uint64_t size : 32;
	uint64_t attr_src : 4;
	uint64_t attr_dst : 4;
	uint64_t rsv1 : 24;
	uint64_t akey0 : 16;
	uint64_t akey1 : 16;
	uint64_t addr0;
	uint64_t addr1;
	uint32_t rsv2[6];
} __packed;

struct sdxi_desc_op_dmabasegrp_repcopy {
	uint64_t rsv1 : 12;
	uint64_t nsize : 9;
	uint64_t rsv2 : 11;
	uint64_t attr_src : 4;
	uint64_t attr_dst : 4;
	uint64_t rsv3 : 24;
	uint64_t akey0 : 16;
	uint64_t akey1 : 16;
	uint64_t az : 1;
	uint64_t rsv4 : 11;
	uint64_t addr0_src : 52;
	uint64_t rsv5 : 12;
	uint64_t addr1_dst : 52;
	uint64_t rsv6 : 12;
	uint64_t num : 20;
	uint32_t rsv7[5];
} __packed;

union sdxi_desc_op_dmabasegrp {
	struct sdxi_desc_op_dmabasegrp_nop nop;
	struct sdxi_desc_op_dmabasegrp_wrt_imm wrt_imm;
	struct sdxi_desc_op_dmabasegrp_copy copy;
	struct sdxi_desc_op_dmabasegrp_repcopy repcopy;
} __packed;

struct sdxi_desc_op_atomicgrp {
	uint64_t rsv1 : 2;
	uint64_t osz : 3;
	uint64_t rsv2 : 27;
	uint64_t attr_dst : 4;
	uint64_t rsv3 : 4;
	uint64_t rsv4 : 24;
	uint64_t akey0 : 16;
	uint64_t rsv5 : 16;
	uint64_t rsv6 : 2;
	uint64_t n : 1;
	uint64_t addr0_dst : 61;
	uint64_t op1;
	uint64_t op2;
	uint64_t nr : 1;
	uint64_t rsv7 : 1;
	uint64_t ret_data_ptr : 62;
	uint32_t rsv8[2];
} __packed;

struct sdxi_desc_op_crcm1grp {
	uint32_t size; /* bytes minus 1 */
	uint8_t attr_src;
	uint8_t attr_dst;
	uint8_t rsvd0[2];
	uint16_t akey0;
	uint16_t akey1;
	uint64_t addr0;
	uint64_t addr1;
	uint64_t seed;
	uint64_t crc_dst_addr;
	uint16_t crc_ctrl;
	uint8_t rsvd2[6];
} __packed;

struct sdxi_desc_op_intrgrp {
	uint8_t rsvd0[8];
	uint16_t akey;
	uint8_t rsvd1[42];
} __packed;


union sdxi_desc_body {
	uint32_t raw[SDXI_DESC_BODY_DWORDS];
	union sdxi_desc_op_dmabasegrp dmabasegrp;
	struct sdxi_desc_op_atomicgrp atomicgrp;
	struct sdxi_desc_op_crcm1grp crcm1grp;
	struct sdxi_desc_op_intrgrp intrgrp;
} __packed;

struct sdxi_desc {
	uint32_t vl : 1;
	uint32_t se : 1;
	uint32_t fe : 1;
	uint32_t ch : 1;
	uint32_t csr : 1;
	uint32_t rb : 1;
	uint32_t rsvd1 : 2;
	uint32_t sub_type : 8;
	uint32_t type : 11;
	uint32_t rsvd2 : 5;
	union sdxi_desc_body body;
	uint64_t comp_ptr;
} __packed __aligned(64);

void sdxi_dsc_build_dma_nop(struct sdxi_desc *desc);

void sdxi_dsc_build_dma_copy(struct sdxi_desc *desc, uint64_t addr_src,
			     uint8_t attr_src, uint16_t src_akey,
			     uint64_t addr_dst, uint8_t attr_dst,
			     uint16_t dst_akey, uint32_t size);

void sdxi_dsc_build_dma_rep_copy(struct sdxi_desc *desc, uint64_t addr_src,
				 uint8_t attr_src, uint16_t src_akey,
				 uint64_t addr_dst, uint8_t attr_dst,
				 uint16_t dst_akey, uint32_t size_4k,
				 uint32_t repetitions);

void sdxi_dsc_build_dma_write_imm(struct sdxi_desc *desc, uint64_t addr_dst,
				  uint8_t attr_dst, uint16_t dst_akey,
				  const uint8_t *data, uint32_t size);

void sdxi_dsc_build_atomic_uadd(struct sdxi_desc *desc,
				uint8_t operand_bytes, uint64_t addr_dst,
				uint8_t attr_dst, uint16_t dst_akey,
				uint64_t operand, uint64_t ret_ptr);

void sdxi_dsc_build_intr(struct sdxi_desc *desc, uint16_t akey);

void sdxi_dsc_build_crcm1(struct sdxi_desc *desc, uint16_t op_type,
			  uint8_t sub_type, uint32_t size_bytes,
			  uint64_t addr_src, uint8_t attr_src,
			  uint16_t src_akey, uint64_t addr_dst,
			  uint8_t attr_dst, uint16_t dst_akey,
			  uint64_t seed, uint64_t crc_dst_addr, uint8_t blksz,
			  uint8_t meta_src, uint8_t meta_dst, uint8_t seed_loc,
			  uint8_t crcsel, uint8_t reflect, uint8_t invert);

#endif /* SDXI_DP_INTERNAL_H */
