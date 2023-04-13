// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2023 Advanced Micro Devices, Inc.
 *
 * Author: Wei Huang <wei.huang2@amd.com>
 *
 * SDXI library sample code - user space write imm value
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

#define DST_BUFFER_ALIGN	4096
#define DST_BUFFER_SIZE		4096
#define DATA_BUFFER_SIZE	32

int main(int argc, char *argv[])
{
	sdxi_ctxt_h ctxt;
	void *dst;
	char data[DATA_BUFFER_SIZE];

	if (sdxi_ctxt_create("write_imm_ctxt", &ctxt) != SDXI_OK) {
		fprintf(stderr, "Failed to create SDXI context\n");
		exit(1);
	}

	/* init dst buffer */
	posix_memalign((void **)&dst, DST_BUFFER_ALIGN, DST_BUFFER_SIZE);

	memset(dst, 0xBE, DST_BUFFER_SIZE);
	memset(data, 0xCD, DATA_BUFFER_SIZE);

	printf("SDXI write imm test ...\n");
	printf("    dst buffer  = %p\n", dst);
	printf("    data buffer = %p\n", data);

	/* submit to sdxi */
	sdxi_write_imm(ctxt, dst, data, DATA_BUFFER_SIZE);

	sleep(2);

	/* compare buffer contents */
	if (memcmp(dst, data, DATA_BUFFER_SIZE) == 0 &&
	    memcmp(dst + DATA_BUFFER_SIZE, data, DATA_BUFFER_SIZE) != 0)
		printf("Write imm ==> SUCESS\n");
	else
		printf("Write imm ==> FAIL\n");

	sdxi_ctxt_close(ctxt);

	return 0;
}
