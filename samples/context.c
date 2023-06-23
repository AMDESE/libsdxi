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
	sdxi_status res;

	printf("SDXI context creation test ...\n");

	res = sdxi_ctxt_create("sample_ctxt", &ctxt);
	sdxi_status_ok_or_die(res, "Context creation ==> FAILED");

	fprintf(stderr, "Context creation ==> SUCCESS\n");

        sdxi_ctxt_close(ctxt);

        return 0;
}
