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
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "libsdxi.h"
#include "sdxi.h"

sdxi_status sdxi_nop(sdxi_ctxt_h ctxt)
{
	return SDXI_OK;
}

sdxi_status sdxi_mem_copy(sdxi_ctxt_h ctxt, void *dst,
			  const void *src, size_t size)
{
	struct sdxi_desc desc;

	build_dma_copy(&desc, (uint64_t)src, 0x0, 0x1, (uint64_t)dst, 0,
		       0x1, (uint32_t)size);

	ctxt_submit_desc(ctxt, &desc);

	return SDXI_OK;
}

sdxi_status sdxi_mem_rep_copy(sdxi_ctxt_h ctxt, void *dst,
			      const void *src, size_t src_size, size_t repeat)
{
	struct sdxi_desc desc;
	int n_4k;

	/* rep_copy command requires 4KB granularity */
	if (src_size % 4096)
		return SDXI_INVAL;
	n_4k = src_size / 4096;

	build_dma_rep_copy(&desc, (uint64_t)src, 0x0, 0x1, (uint64_t)dst, 0,
			   0x1, n_4k, (uint32_t)repeat);

	ctxt_submit_desc(ctxt, &desc);

	return SDXI_OK;
}

sdxi_status sdxi_write_imm(sdxi_ctxt_h ctxt, void *dst,
			   const void *data, size_t size)
{
	struct sdxi_desc desc;

	if (size > 32)
		return SDXI_INVAL;

	build_dma_write_imm(&desc, (uint64_t)dst, 0x0, 0x1, data, size);

	ctxt_submit_desc(ctxt, &desc);

	return SDXI_OK;
}

sdxi_status sdxi_atomic_uadd(sdxi_ctxt_h ctxt, void *addr, uint64_t operand)
{
	struct sdxi_desc desc;

	build_atomic_ops(&desc, OP_ATOMIC_UADD, 1, (uint64_t)addr, 0x0, 0x1,
			 operand, 0, 0);

	ctxt_submit_desc(ctxt, &desc);

	return SDXI_OK;
}
