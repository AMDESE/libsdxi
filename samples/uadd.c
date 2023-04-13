// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2023 Advanced Micro Devices, Inc.
 *
 * Author: Wei Huang <wei.huang2@amd.com>
 *
 * SDXI library sample code - User space uadd
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

int main(int argc, char *argv[])
{
	sdxi_ctxt_h ctxt;
	uint64_t dest;
        uint64_t original;
	uint64_t addend;

	if (sdxi_ctxt_create("atomic_uadd_ctxt", &ctxt) != SDXI_OK) {
		fprintf(stderr, "Failed to create SDXI context\n");
		exit(1);
	}

	printf("SDXI atomic uadd test ...\n");

	memset(&dest, 0xAB, sizeof(uint64_t));
	addend = 0x1100;
	original = dest;

	/* submit to sdxi */
	sdxi_atomic_uadd(ctxt, &dest, addend);

	sleep(1);

	/* compare the values */
	if (dest == (original + addend))
		printf("Atomic uadd ==> SUCESS\n");
	else
		printf("Atomic uadd ==> FAIL\n");

	sdxi_ctxt_close(ctxt);

	return 0;
}
