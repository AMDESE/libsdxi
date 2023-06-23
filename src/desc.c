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
		    uint64_t addr_src, uint8_t attr_src, uint16_t src_akey,
		    uint64_t addr_dst, uint8_t attr_dst, uint16_t dst_akey,
		    int32_t size)
{
	memset(desc, 0, sizeof(*desc));

	desc->fe = 1;

	desc->body.dmabasegrp.copy.size 	= size - 1;
	desc->body.dmabasegrp.copy.attr_src 	= attr_src;
	desc->body.dmabasegrp.copy.attr_dst 	= attr_dst;
	desc->body.dmabasegrp.copy.akey0	= src_akey;
	desc->body.dmabasegrp.copy.akey1 	= dst_akey;
	desc->body.dmabasegrp.copy.addr0 	= addr_src;
	desc->body.dmabasegrp.copy.addr1 	= addr_dst;

	DESC_BUILD_TYPE(desc, OP_TYPE_DMA, OP_DMA_COPY);
}

void build_dma_rep_copy(struct sdxi_desc *desc,
			uint64_t addr_src, uint8_t attr_src, uint16_t akey_src,
			uint64_t addr_dst, uint8_t attr_dst, uint16_t akey_dst,
			int32_t size_4k, uint32_t repetitions)
{
	/* convert value from "number if 4KB chunks minus 1"
	 * to value in bytes (see spec v1.0 p.96)
	 */
	uint32_t nsize = (size_4k - 1) << 12;

	memset(desc, 0, sizeof(*desc));

	desc->fe = 1;

	desc->body.dmabasegrp.repcopy.nsize 	= nsize;
	desc->body.dmabasegrp.repcopy.attr_src 	= attr_src;
	desc->body.dmabasegrp.repcopy.attr_dst 	= attr_dst;
	desc->body.dmabasegrp.repcopy.akey0	= akey_src;
	desc->body.dmabasegrp.repcopy.akey1 	= akey_dst;
	desc->body.dmabasegrp.repcopy.addr0_src	= addr_src >> 12;
	desc->body.dmabasegrp.repcopy.addr1_dst	= addr_dst >> 12;
	desc->body.dmabasegrp.repcopy.num 	= repetitions;

	DESC_BUILD_TYPE(desc, OP_TYPE_DMA, OP_DMA_REP_COPY);
}

void build_dma_write_imm(struct sdxi_desc *desc,
			 uint64_t addr_dst, uint8_t attr_dst, uint16_t akey_dst,
			 const uint8_t *data, uint32_t size)
{
	memset(desc, 0, sizeof(*desc));

	desc->fe = 1;

	desc->body.dmabasegrp.wrt_imm.bsize	= size - 1;
	desc->body.dmabasegrp.wrt_imm.attr_dst 	= attr_dst;
	desc->body.dmabasegrp.wrt_imm.akey0 	= akey_dst;
	desc->body.dmabasegrp.wrt_imm.addr0	= addr_dst;

	/* make sure we write no more immediate data than
	 * the space available to avoid buffer overflow
	 */
	if (size > SDXI_DESC_WRT_IMM_DATA_UINT32_LEN * sizeof(uint32_t))
		size = SDXI_DESC_WRT_IMM_DATA_UINT32_LEN;

	memcpy(&(desc->body.dmabasegrp.wrt_imm.data), data, size);

	DESC_BUILD_TYPE(desc, OP_TYPE_DMA, OP_DMA_WRT_IMM);
}

void build_atomic_ops(struct sdxi_desc *desc,
		      uint8_t sub_type, uint8_t osz,
		      uint64_t addr_dst, uint8_t attr_dst, uint16_t akey_dst,
		      uint64_t op1, uint64_t op2, uint64_t ret_ptr)
{
	memset(desc, 0, sizeof(*desc));

	desc->fe = 1;

	desc->body.atomicgrp.osz 		= osz - 1;
	desc->body.atomicgrp.attr_dst 		= attr_dst;
	desc->body.atomicgrp.akey0 		= akey_dst;
	desc->body.atomicgrp.addr0_dst 		= addr_dst >> 3;
	desc->body.atomicgrp.op1 		= op1;
	desc->body.atomicgrp.op2 		= op2;
	desc->body.atomicgrp.ret_data_ptr	= ret_ptr;
	/* nr := no return
	 * 	set this to 0 when ret_ptr is valid
	 * 		result will end up here
	 * 	set to 1 when it is not valid
	 * 		result will be ignored
	 */
	desc->body.atomicgrp.nr 	= !ret_ptr;

	DESC_BUILD_TYPE(desc, OP_TYPE_ATOMIC, sub_type);
}
