// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2026 Advanced Micro Devices, Inc.
 *
 * SDXI library sample code - submit one DMA NOP descriptor
 */
#include <stdio.h>

#include "libsdxi.h"
#include "samples.h"

int main(int argc, char *argv[])
{
	int ret = 1;
	sdxi_dev_t *dev = NULL;
	sdxi_cxt_t *cxt = NULL;
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

	printf("SDXI DMA NOP test ...\n");

	ret = sdxi_nop(cxt, 2000);
	if (ret) {
		fprintf(stderr, "sdxi_nop: %s\n", sdxi_err_str(ret));
		goto out;
	}

	fprintf(stderr, "DMA NOP ==> SUCCESS\n");
	ret = 0;

out:
	(void)sdxi_cxt_destroy(cxt);
	sdxi_dev_close(dev);
	sdxi_free_dev_list(dev_list, dev_count);

	return ret;
}
