// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2022 Advanced Micro Devices, Inc.
 *
 * Author: Wei Huang <wei.huang2@amd.com>
 *
 * SDXI library sample code - user space memory copy
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "libsdxi.h"
#include "samples.h"

#define TEST_BUFFER_ALIGN	4096
#define TEST_BUFFER_SIZE	4096 * 1024 * 64

int main(int argc, char *argv[])
{
	int ret = 1;
	sdxi_dev_t *dev = NULL;
	sdxi_cxt_t *cxt = NULL;
	void *src = NULL, *dst = NULL;
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

	/* init source and dest buffers */
	posix_memalign_check((void **)&src, TEST_BUFFER_ALIGN, TEST_BUFFER_SIZE);
	posix_memalign_check((void **)&dst, TEST_BUFFER_ALIGN, TEST_BUFFER_SIZE);

	printf("SDXI memory copy test ...\n");
	printf("    memory buffer src = %p\n", src);
	printf("    memory buffer dst = %p\n", dst);

	memset(src, 0xAF, TEST_BUFFER_SIZE);
	memset(dst, 0xBE, TEST_BUFFER_SIZE);

	/* timing start */
	struct timespec ts_start, ts_end;
	clock_gettime(CLOCK_MONOTONIC, &ts_start);

	/* submit to sdxi */
	ret = sdxi_mem_copy(cxt, dst, src, TEST_BUFFER_SIZE, 5000);
	if (ret) {
		fprintf(stderr, "sdxi_mem_copy: %s\n", sdxi_err_str(ret));
		goto out;
	}

	/* timing end */
	clock_gettime(CLOCK_MONOTONIC, &ts_end);

	/* calculate elapsed time and bandwidth */
	double elapsed_ns = (ts_end.tv_sec - ts_start.tv_sec) * 1e9 +
			    (ts_end.tv_nsec - ts_start.tv_nsec);
	double elapsed_ms = elapsed_ns / 1e6;
	double size_mb = (double)TEST_BUFFER_SIZE / (1024.0 * 1024.0);
	double bw_gbps = (size_mb / 1024.0) / (elapsed_ns / 1e9);

	printf("    size: %.2f MB, elapsed: %.3f ms, bandwidth: %.2f GB/s\n",
	       size_mb, elapsed_ms, bw_gbps);

	/* compare buffer contents */
	if (memcmp(src, dst, TEST_BUFFER_SIZE) == 0) {
		fprintf(stderr, "Memory copy ==> SUCCESS\n");
		ret = 0;
	} else {
		fprintf(stderr, "Memory copy ==> FAIL\n");
		ret = 1;
	}

out:
	(void)sdxi_cxt_destroy(cxt);
	sdxi_dev_close(dev);
	sdxi_free_dev_list(dev_list, dev_count);
	free(src);
	free(dst);

	return ret;
}
