// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2022 Advanced Micro Devices, Inc.
 *
 * Author: Wei Huang <wei.huang2@amd.com>
 *
 * SDXI library sample code - user space memory copy
 */
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "libsdxi.h"
#include "samples.h"

#define TEST_BUFFER_ALIGN	4096
#define TEST_BUFFER_SIZE	4096

int main(int argc, char *argv[])
{
	int ret;
	sdxi_ctxt_h ctxt;
	sdxi_status res;
	void *src = NULL, *dst = NULL;

	res = sdxi_ctxt_create("dma_copy_ctxt", &ctxt);
	sdxi_status_ok_or_die(res, "Failed to create SDXI context");

	/* init source and dest buffers */
	posix_memalign_check((void **)&src, TEST_BUFFER_ALIGN, TEST_BUFFER_SIZE);
	posix_memalign_check((void **)&dst, TEST_BUFFER_ALIGN, TEST_BUFFER_SIZE);

	printf("SDXI memory copy test ...\n");
	printf("    memory buffer src = %p\n", src);
	printf("    memory buffer dst = %p\n", dst);

	memset(src, 0xAF, TEST_BUFFER_SIZE);
	memset(dst, 0xBE, TEST_BUFFER_SIZE);

	/* submit to sdxi */
	res = sdxi_mem_copy(ctxt, dst, src, TEST_BUFFER_SIZE);
	sdxi_status_ok_or_die(res, "sdxi_mem_copy returned failure");
	sleep(3);

	/* compare buffer contents */
	if (memcmp(src, dst, TEST_BUFFER_SIZE) == 0) {
		fprintf(stderr, "Memory copy ==> SUCCESS\n");
		ret = 0;
	} else {
		fprintf(stderr, "Memory copy ==> FAIL\n");
		ret = 1;
	}

	sdxi_ctxt_close(ctxt);

	return ret;
}
