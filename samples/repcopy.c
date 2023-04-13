// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2023 Advanced Micro Devices, Inc.
 *
 * Author: Wei Huang <wei.huang2@amd.com>
 *
 * SDXI library sample code - user space repeat memory copy
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
#define PAGE_SIZE_4KB		4096
#define SRC_BUFFER_4KB_NUM	1
#define BUFFER_REPEAT		16

int main(int argc, char *argv[])
{
	sdxi_ctxt_h ctxt;
	int src_size, dst_size, i;
	void *src = NULL, *dst = NULL;

	if (sdxi_ctxt_create("dma_rep_copy_ctxt", &ctxt) != SDXI_OK) {
		fprintf(stderr, "Failed to create SDXI context\n");
		exit(1);
	}

	src_size = PAGE_SIZE_4KB * SRC_BUFFER_4KB_NUM;
	dst_size = PAGE_SIZE_4KB * SRC_BUFFER_4KB_NUM * BUFFER_REPEAT;

	/* init source and dest buffers */
	posix_memalign_check((void **)&src, TEST_BUFFER_ALIGN, src_size);
	posix_memalign_check((void **)&dst, TEST_BUFFER_ALIGN, dst_size);

	printf("SDXI memory rep copy test ...\n");
	printf("    memory buffer src = %p, size = 0x%x\n", src, src_size);
	printf("    memory buffer dst = %p, size = 0x%x\n", dst, dst_size);
	printf("    repeat = %d\n", BUFFER_REPEAT);

        memset(src, 0xAF, src_size);
        memset(dst, 0xBE, dst_size);

	sdxi_mem_rep_copy(ctxt, dst, src, src_size, BUFFER_REPEAT);

	sleep(3);

        /* compare buffer content */
        for (i = 0; i < BUFFER_REPEAT; i++) {
                if (memcmp(src, dst + i * src_size, src_size) != 0) {
                        printf("Memory rep copy ==> FAIL at %d 4KB\n", i);
                        return -1;
                }
        }
	printf("Memory rep copy ==> SUCESS\n");

	sdxi_ctxt_close(ctxt);

	return 0;
}
