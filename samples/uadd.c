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
	int ret;
	sdxi_ctxt_h ctxt;
	sdxi_status res;
	uint64_t dest;
        uint64_t original;
	uint64_t addend;

	res = sdxi_ctxt_create("atomic_uadd_ctxt", &ctxt);
	sdxi_status_ok_or_die(res, "Failed to create SDXI context");

	printf("SDXI atomic uadd test ...\n");

	memset(&dest, 0xAB, sizeof(uint64_t));
	addend = 0x1100;
	original = dest;

	/* submit to sdxi */
	res = sdxi_atomic_uadd(ctxt, &dest, addend);
	sdxi_status_ok_or_die(res, "sdxi_atomic_uadd returned failure");
	sleep(1);

	/* compare the values */
	if (dest == (original + addend)) {
		fprintf(stderr, "Atomic uadd ==> SUCCESS\n");
		ret = 0;
	} else {
		fprintf(stderr, "Atomic uadd ==> FAIL\n");
		ret = 1;
	}

	sdxi_ctxt_close(ctxt);

	return ret;
}
