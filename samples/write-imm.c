// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2023 Advanced Micro Devices, Inc.
 *
 * Author: Wei Huang <wei.huang2@amd.com>
 *
 * SDXI library sample code - user space write imm value
 */
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "libsdxi.h"
#include "samples.h"

#define DST_BUFFER_ALIGN	4096
#define DST_BUFFER_SIZE		4096
#define DATA_BUFFER_SIZE	32

int main(int argc, char *argv[])
{
	int ret = 1;
	sdxi_dev_t *dev = NULL;
	sdxi_cxt_t *cxt = NULL;
	void *dst = NULL;
	char data[DATA_BUFFER_SIZE];
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

	/* init dst buffer */
	posix_memalign_check((void **)&dst, DST_BUFFER_ALIGN, DST_BUFFER_SIZE);

	memset(dst, 0xBE, DST_BUFFER_SIZE);
	memset(data, 0xCD, DATA_BUFFER_SIZE);

	printf("SDXI write imm test ...\n");
	printf("    dst buffer  = %p\n", dst);
	printf("    data buffer = %p\n", data);

	/* submit to sdxi */
	ret = sdxi_write_imm(cxt, dst, data, DATA_BUFFER_SIZE, 5000);
	if (ret) {
		fprintf(stderr, "sdxi_write_imm: %s\n", sdxi_err_str(ret));
		goto out;
	}

	/* compare buffer contents */
	if (memcmp(dst, data, DATA_BUFFER_SIZE) == 0 &&
	    memcmp((uint8_t *)dst + DATA_BUFFER_SIZE, data, DATA_BUFFER_SIZE) != 0) {
		fprintf(stderr, "Write imm ==> SUCCESS\n");
		ret = 0;
	} else {
		fprintf(stderr, "Write imm ==> FAIL\n");
		ret = 1;
	}

out:
	(void)sdxi_cxt_destroy(cxt);
	sdxi_dev_close(dev);
	sdxi_free_dev_list(dev_list, dev_count);
	free(dst);

	return ret;
}
