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

#define TEST_BUFFER_ALIGN	4096
#define TEST_BUFFER_SIZE	4096

int main(int argc, char *argv[])
{
	sdxi_ctxt_h ctxt;
	void *src, *dst;

	if (sdxi_ctxt_create("dma_copy_ctxt", &ctxt) != SDXI_OK) {
		fprintf(stderr, "Failed to create SDXI context\n");
		exit(1);
	}

	/* init source and dest buffers */
	posix_memalign((void **)&src, TEST_BUFFER_ALIGN, TEST_BUFFER_SIZE);
	posix_memalign((void **)&dst, TEST_BUFFER_ALIGN, TEST_BUFFER_SIZE);

	printf("SDXI memory copy test ...\n");
	printf("    memory buffer src = %p\n", src);
	printf("    memory buffer dst = %p\n", dst);

	memset(src, 0xAF, TEST_BUFFER_SIZE);
	memset(dst, 0xBE, TEST_BUFFER_SIZE);

	/* submit to sdxi */
	sdxi_mem_copy(ctxt, dst, src, TEST_BUFFER_SIZE);
	sleep(3);

	/* compare buffer contents */
	if (memcmp(src, dst, TEST_BUFFER_SIZE) == 0)
		printf("Memory copy ==> SUCESS\n");
	else
		printf("Memory copy ==> FAIL\n");

	sdxi_ctxt_close(ctxt);

	return 0;
}
