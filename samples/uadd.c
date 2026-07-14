// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2023 Advanced Micro Devices, Inc.
 *
 * Author: Wei Huang <wei.huang2@amd.com>
 *
 * SDXI library sample code - User space uadd
 */
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "libsdxi.h"
#include "samples.h"

int main(int argc, char *argv[])
{
	int ret = 1;
	sdxi_dev_t *dev = NULL;
	sdxi_cxt_t *cxt = NULL;
	uint64_t dest;
        uint64_t original;
	uint64_t addend;
	struct sample_args args;
	char **dev_list = NULL;
	size_t dev_count = 0;
	const char *path;

	if (parse_sample_args(argc, argv, &args) < 0)
		return 1;

	path = args.dev_path;
	if (!path) {
		ret = sdxi_list_devs(&dev_list, &dev_count);
		if (ret) {
			fprintf(stderr, "sdxi_list_devs: %s\n", sdxi_err_str(ret));
			goto out;
		}
		if (dev_count == 0) {
			fprintf(stderr, "No SDXI devices found under /dev\n");
			goto out;
		}
		path = dev_list[0];
		printf("Using device: %s (first)\n", path);
	} else {
		printf("Using device: %s\n", path);
	}

	ret = sdxi_dev_open(path, &dev);
	if (ret) {
		fprintf(stderr, "sdxi_dev_open: %s\n", sdxi_err_str(ret));
		goto out;
	}

	struct sdxi_cxt_create_opts opts = {
		.size = sizeof(opts),
		.flags = 0,
		.ds_ring_entries = 0,
		.opb_000_enb = 0,
		.eventfd = -1,
	};

	ret = sdxi_cxt_create(dev, &opts, &cxt);
	if (ret) {
		fprintf(stderr, "sdxi_cxt_create: %s\n", sdxi_err_str(ret));
		goto out;
	}

	printf("SDXI atomic uadd test ...\n");

	memset(&dest, 0xAB, sizeof(uint64_t));
	addend = 0x1100;
	original = dest;

	/* submit to sdxi */
	ret = sdxi_atomic_uadd(cxt, &dest, addend, 2000);
	if (ret) {
		fprintf(stderr, "sdxi_atomic_uadd: %s\n", sdxi_err_str(ret));
		goto out;
	}

	/* compare the values */
	if (dest == (original + addend)) {
		fprintf(stderr, "Atomic uadd ==> SUCCESS\n");
		ret = 0;
	} else {
		fprintf(stderr, "Atomic uadd ==> FAIL\n");
		ret = 1;
	}

out:
	(void)sdxi_cxt_destroy(cxt);
	sdxi_dev_close(dev);
	sdxi_free_dev_list(dev_list, dev_count);

	return ret;
}
