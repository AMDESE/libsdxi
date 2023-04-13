// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2022 AMD, Inc. - All Rights Reserved
 *
 * Wei Huang <wei.huang2@amd.com>
 */

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "libsdxi.h"
#include "sdxi.h"

#define DMA_TEST_BUFFER_SIZE      4096
#define DEFAULT_DESC_RING_ENTRIES 16

#define DESC_BUILD_TYPE(d, t, s)		\
	do {					\
		(d)->vl = 1;			\
		(d)->type = (t);		\
		(d)->sub_type = (s);		\
		(d)->comp_ptr = 1;		\
	} while (0)

void build_dma_nop(struct sdxi_desc *desc)
{
	memset(desc, 0, sizeof(*desc));

	DESC_BUILD_TYPE(desc, OP_TYPE_DMA, OP_DMA_NOP);
}

void build_dma_copy(struct sdxi_desc *desc,
		    uint64_t src, uint8_t src_attr, uint16_t src_akey,
		    uint64_t dst, uint8_t dst_attr, uint16_t dst_akey,
		    int32_t size)
{
	memset(desc, 0, sizeof(*desc));

	desc->fe = 1;
	desc->body[0] |= size - 1;
	desc->body[1] |= (src_attr & 0xF);
	desc->body[1] |= ((dst_attr & 0xF) << 4);
	desc->body[2] |= (src_akey & 0xFFFF);
	desc->body[2] |= ((dst_akey & 0xFFFF) << 16);
	desc->body[3] = (src & 0xFFFFFFFF);
	desc->body[4] = ((src >> 32) & 0xFFFFFFFF);
	desc->body[5] = (dst & 0xFFFFFFFF);
	desc->body[6] = ((dst >> 32) & 0xFFFFFFFF);

	DESC_BUILD_TYPE(desc, OP_TYPE_DMA, OP_DMA_COPY);
}

void build_dma_rep_copy(struct sdxi_desc *desc,
			uint64_t src, uint8_t src_attr, uint16_t src_akey,
			uint64_t dst, uint8_t dst_attr, uint16_t dst_akey,
			int32_t n_4k, uint32_t repeat)
{
	memset(desc, 0, sizeof(*desc));

	desc->fe = 1;

	desc->body[0] |= ((n_4k - 1) << 12);
	desc->body[1] |= (src_attr & 0xF);
	desc->body[1] |= ((dst_attr & 0xF) << 4);
	desc->body[2] |= (src_akey & 0xFFFF);
	desc->body[2] |= ((dst_akey & 0xFFFF) << 16);
	desc->body[3] = (src & 0xFFFFFFFF);
	desc->body[4] = ((src >> 32) & 0xFFFFFFFF);
	desc->body[5] = (dst & 0xFFFFFFFF);
	desc->body[6] = ((dst >> 32) & 0xFFFFFFFF);
	desc->body[7] = (((repeat - 1) << 12) & 0xFFFFFFFF);

	DESC_BUILD_TYPE(desc, OP_TYPE_DMA, OP_DMA_REP_COPY);
}

void build_dma_write_imm(struct sdxi_desc *desc,
			 uint64_t dst, uint8_t dst_attr, uint16_t dst_akey,
			 const uint8_t *data, uint32_t size)
{
	memset(desc, 0, sizeof(*desc));

	desc->fe = 1;

	desc->body[0] |= ((size-1) & 0x1F);
	desc->body[1] |= (dst_attr & 0xF);
	desc->body[2] |= (dst_akey & 0xFFFF);
	desc->body[3] = (dst & 0xFFFFFFFF);
	desc->body[4] = ((dst >> 32) & 0xFFFFFFFF);
	memcpy(&(desc->body[5]), data, size);

	DESC_BUILD_TYPE(desc, OP_TYPE_DMA, OP_DMA_WRT_IMM);
}

void build_atomic_ops(struct sdxi_desc *desc,
		      uint8_t sub_type, uint8_t osz,
		      uint64_t dst, uint8_t dst_attr, uint16_t dst_akey,
		      uint64_t op1, uint64_t op2, uint64_t ret_ptr)
{
	memset(desc, 0, sizeof(*desc));

	desc->fe = 1;
	desc->body[0] |= ((osz & 0x7) << 2);
	desc->body[1] |= (dst_attr & 0xF);
	desc->body[2] |= (dst_akey & 0xFFFF);
	desc->body[3] = (dst & 0xFFFFFFFF);
	desc->body[4] = ((dst >> 32) & 0xFFFFFFFF);
	desc->body[5] = (op1 & 0xFFFFFFFF);
	desc->body[6] = ((op1 >> 32) & 0xFFFFFFFF);
	desc->body[7] = (op2 & 0xFFFFFFFF);
	desc->body[8] = ((op2 >> 32) & 0xFFFFFFFF);
	desc->body[9] = (ret_ptr & 0xFFFFFFFF);
	desc->body[10] = ((ret_ptr >> 32) & 0xFFFFFFFF);
	if (ret_ptr)
		desc->body[9] |= 0x1;

	DESC_BUILD_TYPE(desc, OP_TYPE_ATOMIC, sub_type);
}
