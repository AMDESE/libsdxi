// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2022 Advanced Micro Devices, Inc.
 *
 * Author: Wei Huang <wei.huang2@amd.com>
 *
 * SDXI library sample code - context creation and destroy
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

	printf("SDXI context creation test ...\n");

	if (sdxi_ctxt_create("sample_ctxt", &ctxt) != SDXI_OK) {
		fprintf(stderr, "Context creation ==> FAILED\n");
                exit(1);
	}

	fprintf(stderr, "Context creation ==> SUCCESS\n");

        sdxi_ctxt_close(ctxt);

        return 0;
}
