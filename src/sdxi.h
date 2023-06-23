// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2022 AMD, Inc. - All Rights Reserved
 *
 * Wei Huang <wei.huang2@amd.com>
 *
 */

#ifndef __INTERNAL_H
#define __INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

#ifndef __aligned
#define __aligned(x) __attribute__((aligned(x)))
#endif

/* SDXI consts */
#define OP_TYPE_DMA             0x001
#define OP_TYPE_ADMIN           0x002
#define OP_TYPE_ATOMIC          0x003
#define OP_TYPE_INTR            0x004

#define OP_DMA_NOP              0x01
#define OP_DMA_WRT_IMM          0x02
#define OP_DMA_COPY             0x03
#define OP_DMA_REP_COPY         0x04
#define OP_ADMIN_UPDATE_FUNC    0x00
#define OP_ADMIN_UPDATE_CTXT    0x01
#define OP_ADMIN_UPDATE_AKEY    0x02
#define OP_ADMIN_START          0x03
#define OP_ADMIN_STOP           0x04
#define OP_ADMIN_INTR           0x05
#define OP_ADMIN_SYNC           0x06
#define OP_ATOMIC_SWAP          0x01
#define OP_ATOMIC_UADD          0x02
#define OP_ATOMIC_USUB          0x03
#define OP_ATOMIC_AND           0x05
#define OP_ATOMIC_OR            0x06
#define OP_ATOMIC_XOR           0x07
#define OP_ATOMIC_SMIN          0x08
#define OP_ATOMIC_SMAX          0x09
#define OP_ATOMIC_UMIN          0x0A
#define OP_ATOMIC_UMAX          0x0B
#define OP_ATOMIC_UCLAMPI       0x0C
#define OP_ATOMIC_UCLAMPD       0x0D
#define OP_ATOMIC_CMPSWAP       0x0E
#define OP_INTR_INTERRUPT       0x00

#define CTXT_STATE_STOPPED      0x0
#define CTXT_STATE_RUNNING      0x1
#define CTXT_STATE_STOPPING     0x2
#define CTXT_STATE_ERR          0xF

struct akey {
	uint32_t valid               : 1;
	uint32_t iv                  : 1;
	uint32_t pv                  : 1;
	uint32_t se                  : 1;
	uint32_t irq_num             : 11;
	uint32_t rsvd1               : 1;
	uint32_t target_func_handle  : 16;
	uint32_t pasid               : 20;
	uint32_t rsvd2               : 12;
	uint32_t stag                : 16;
	uint32_t ph                  : 2;
	uint32_t rsvd3               : 14;
	uint32_t rkey                : 16;
	uint32_t rsvd4               : 16;
};

struct ctxt_status {
	uint64_t state               : 4;    /* QW0 */
	uint64_t rsvd1               : 60;
	uint64_t read_idx;                   /* QW1 */
} __packed __aligned(16);

struct ctxt_ctrl {
	uint64_t valid               : 1;    /* QW0 */
	uint64_t rsvd1               : 1;
	uint64_t qos                 : 2;
	uint64_t sh                  : 1;
	uint64_t rsvd2               : 1;
	uint64_t desc_ring_base      : 58;
	uint64_t desc_ring_size      : 32;   /* QW1 */
	uint64_t rsvd3               : 32;
	uint64_t rsvd4               : 4;    /* QW2 */
	uint64_t ctxt_status_ptr     : 60;
	uint64_t rsvd5               : 3;    /* QW3 */
	uint64_t wrt_index_ptr       : 61;
	uint32_t rsvd6[8];                   /* QW4+ */
} __packed __aligned(64);


/* size of sdxi descriptor body in units of 32-bit chunks */
#define SDXI_DESC_OP_UINT32_LEN (0x34 / sizeof(uint32_t))

struct sdxi_desc_op_dmabasegrp_nop {
	/* no-op reserves the whole body and does no operation */
	uint32_t rsv[SDXI_DESC_OP_UINT32_LEN];
} __packed;

/* "32-bytes of immediate data" -- spec v1.0 p.92 */
#define SDXI_DESC_WRT_IMM_DATA_UINT32_LEN (32 / sizeof(uint32_t))

/* type 0x1 subtype 0x1: DMAB_NOP (spec v1.0 pp.91) */
struct sdxi_desc_op_dmabasegrp_wrt_imm {
	/* + 0x4 */
	uint64_t bsize		     : 5;
	uint64_t rsv1		     : 27;
	/* + 0x8 */
	uint64_t attr_dst	     : 4;
	uint64_t rsv2	             : 4;
	uint64_t rsv3	     	     : 24;
	/* + 0xC */
	uint64_t akey0		     : 16;
	uint64_t rsv4		     : 16;
	/* + 0x10 */
	uint64_t addr0;
	/* + 0x18 */
	uint32_t data[SDXI_DESC_WRT_IMM_DATA_UINT32_LEN];
} __packed;

/* type 0x1 subtype 0x2: DMAB_WRT_IMM (spec v1.0 pp.93) */
struct sdxi_desc_op_dmabasegrp_copy {
	/* + 0x4 */
	uint64_t size		     : 32;
	/* + 0x8 */
	uint64_t attr_src	     : 4;
	uint64_t attr_dst	     : 4;
	uint64_t rsv1	     	     : 24;
	/* + 0xC */
	uint64_t akey0		     : 16;
	uint64_t akey1		     : 16;
	/* + 0x10 */
	uint64_t addr0;
	/* + 0x18 */
	uint64_t addr1;
	/* + 0x20 */
	uint32_t rsv2[6];
} __packed;

/* type 0x1 subtype 0x3: DMAB_COPY (spec v1.0 pp.95) */
struct sdxi_desc_op_dmabasegrp_repcopy {
	/* + 0x4 */
	uint64_t rsv1		     : 12;
	uint64_t nsize		     : 9;
	uint64_t rsv2		     : 11;
	/* + 0x8 */
	uint64_t attr_src	     : 4;
	uint64_t attr_dst	     : 4;
	uint64_t rsv3	     	     : 24;
	/* + 0xC */
	uint64_t akey0		     : 16;
	uint64_t akey1		     : 16;
	/* + 0x10 */
	uint64_t az		     : 1;
	uint64_t rsv4		     : 11;
	uint64_t addr0_src	     : 52;
	/* + 0x18 */
	uint64_t rsv5		     : 12;
	uint64_t addr1_dst	     : 52;
	/* + 0x20 */
	uint64_t rsv6		     : 12;
	uint64_t num 		     : 20;
	/* + 0x24 */
	uint32_t rsv7[5];
} __packed;

/* type 0x1: DMA Base Operation Group (DmaBaseGrp) */
union sdxi_desc_op_dmabasegrp {
	struct sdxi_desc_op_dmabasegrp_nop 	nop;
	struct sdxi_desc_op_dmabasegrp_wrt_imm 	wrt_imm;
	struct sdxi_desc_op_dmabasegrp_copy 	copy;
	struct sdxi_desc_op_dmabasegrp_repcopy 	repcopy;
};

/* type 0x2: Administrative Operation Group (AdminGrp) */
struct sdxi_desc_op_admingrp {
	/* this is here for completeness but I don't know
	 * if there are use cases for userspace usage
	 * of struct sdxi_desc for AdminGrp requests
	 */
	uint32_t rsv[SDXI_DESC_OP_UINT32_LEN];
} __packed;

/* type 0x3: Full Atomic Operation Group (AtomicGrp)
 * All AtomicGrp operations use the same sdxi_desc format
 * (see spec v1.0 p.97)
 */
struct sdxi_desc_op_atomicgrp {
	/* + 0x4 */
	uint64_t rsv1		     : 2;
	uint64_t osz		     : 3;
	uint64_t rsv2		     : 27;
	/* + 0x8 */
	uint64_t attr_dst	     : 4;
	uint64_t rsv3	     	     : 4;
	uint64_t rsv4	     	     : 24;
	/* + 0xC */
	uint64_t akey0		     : 16;
	uint64_t rsv5		     : 16;
	/* + 0x10 */
	uint64_t rsv6		     : 2;
	uint64_t n		     : 1;
	uint64_t addr0_dst 	     : 61;
	/* + 0x18 */
	uint64_t op1;
	/* + 0x20 */
	uint64_t op2;
	/* + 0x28 */
	uint64_t nr		     : 1;
	uint64_t rsv7		     : 1;
	uint64_t ret_data_ptr	     : 61;
	/* + 0x30 */
	uint32_t rsv8[2];
} __packed;

/* type 0x4: Interrupt Operation Group (IntrGrp)
 * has only subtype 0x0 (DSC_INTR) so no need for union
 * (see spec v1.0 p.100)
 */
struct sdxi_desc_op_intrgrp {
	/* + 0x4 */
	uint64_t rsv1;
	/* + 0xC */
	uint64_t akey		     : 16;
	uint64_t rsv2		     : 16;
	/* + 0x10 */
	uint32_t rsv3[10];
} __packed;

union sdxi_desc_body {
	uint32_t raw[SDXI_DESC_OP_UINT32_LEN];
	/* See spec v1.0 p.85 for details on these groups*/
	/* "_op" name from p86 of spec v1.0 */
	union sdxi_desc_op_dmabasegrp 	dmabasegrp;
	struct sdxi_desc_op_admingrp 	admingrp;
	struct sdxi_desc_op_atomicgrp  	atomicgrp;
	struct sdxi_desc_op_intrgrp 	intrgrp;
} __packed;

struct sdxi_desc {
	uint32_t vl                  : 1;
	uint32_t se                  : 1;
	uint32_t fe                  : 1;
	uint32_t ch                  : 1;
	uint32_t csr                 : 1;
	uint32_t rsvd1               : 3;
	uint32_t sub_type            : 8;
	uint32_t type                : 11;
	uint32_t rsvd2               : 5;
	union sdxi_desc_body body;
	uint64_t comp_ptr;                   /* DW14 */
} __packed __aligned(64);

void build_dma_nop(struct sdxi_desc *desc);
void build_dma_copy(struct sdxi_desc *desc,
		    uint64_t src, uint8_t src_attr, uint16_t src_akey,
		    uint64_t dst, uint8_t dst_attr, uint16_t dst_akey,
		    int32_t size);
void build_dma_rep_copy(struct sdxi_desc *desc,
			uint64_t src, uint8_t src_attr, uint16_t src_akey,
			uint64_t dst, uint8_t dst_attr, uint16_t dst_akey,
			int32_t n_4k, uint32_t repeat);
void build_dma_write_imm(struct sdxi_desc *desc,
			 uint64_t dst, uint8_t dst_attr, uint16_t dst_akey,
			 const uint8_t *data, uint32_t size);
void build_atomic_ops(struct sdxi_desc *desc, uint8_t sub_type, uint8_t osz,
		      uint64_t dst, uint8_t dst_attr, uint16_t dst_akey,
		      uint64_t op1, uint64_t op2, uint64_t ret_ptr);

sdxi_status ctxt_submit_desc(sdxi_ctxt_h ctxt, struct sdxi_desc *desc);
#ifdef __cplusplus
}
#endif

#endif /* __INTERNAL_H */
