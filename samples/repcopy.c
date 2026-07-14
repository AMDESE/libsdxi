// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2023 Advanced Micro Devices, Inc.
 *
 * Author: Wei Huang <wei.huang2@amd.com>
 *
 * SDXI library sample code - user space repeat memory copy
 */
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include "libsdxi.h"
#include "samples.h"

#define TEST_BUFFER_ALIGN	4096
#define PAGE_SIZE_4KB		4096
#define SRC_BUFFER_4KB_NUM	1
#define DEFAULT_BUFFER_REPEAT	16u
#define DEFAULT_TIMEOUT_MS	5000u

struct repcopy_args {
	struct sample_args sample;
	size_t repeat;
};

static void repcopy_print_usage(const char *prog_name)
{
	print_usage(prog_name);
	fprintf(stderr, "Repcopy-specific options:\n");
	fprintf(stderr,
		"  -r, --repeat <count>  Repeat count for each source copy (default: %u)\n",
		DEFAULT_BUFFER_REPEAT);
	fprintf(stderr,
		"\nThe sample also accepts `-repeat <count>` for consistency with `-dev`.\n");
}

static int repcopy_parse_repeat(const char *arg, size_t *repeat_out)
{
	unsigned long long value;
	char *end = NULL;

	errno = 0;
	value = strtoull(arg, &end, 0);
	if (errno != 0 || end == arg || *end != '\0' || value == 0 ||
	    value > SIZE_MAX)
		return -EINVAL;

	*repeat_out = (size_t)value;
	return 0;
}

static char **repcopy_normalize_argv(int argc, char *argv[])
{
	char **argv_norm;
	int i;

	argv_norm = calloc((size_t)argc + 1, sizeof(*argv_norm));
	if (!argv_norm)
		return NULL;

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-dev") == 0)
			argv_norm[i] = "--dev";
		else if (strcmp(argv[i], "-repeat") == 0)
			argv_norm[i] = "--repeat";
		else if (strcmp(argv[i], "-help") == 0)
			argv_norm[i] = "--help";
		else
			argv_norm[i] = argv[i];
	}

	return argv_norm;
}

static int repcopy_parse_args(int argc, char *argv[], struct repcopy_args *args)
{
	static struct option long_opts[] = {
		{"dev", required_argument, 0, 'd'},
		{"repeat", required_argument, 0, 'r'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};
	char **argv_norm;
	int opt;
	int option_index = 0;
	int ret = 0;

	memset(args, 0, sizeof(*args));
	args->repeat = DEFAULT_BUFFER_REPEAT;

	argv_norm = repcopy_normalize_argv(argc, argv);
	if (!argv_norm) {
		perror("calloc");
		return -1;
	}

	optind = 1;
	while (1) {
		opt = getopt_long(argc, argv_norm, "d:r:h", long_opts,
				  &option_index);
		if (opt == -1)
			break;

		switch (opt) {
		case 'd':
			if (sample_set_dev_path(&args->sample, optarg) != 0) {
				fprintf(stderr, "Invalid --dev: %s\n", optarg);
				repcopy_print_usage(argv[0]);
				ret = -1;
				goto out;
			}
			break;
		case 'r':
			if (repcopy_parse_repeat(optarg, &args->repeat) != 0) {
				fprintf(stderr, "Invalid --repeat: %s\n", optarg);
				repcopy_print_usage(argv[0]);
				ret = -1;
				goto out;
			}
			break;
		case 'h':
			repcopy_print_usage(argv[0]);
			free(argv_norm);
			exit(0);
		default:
			repcopy_print_usage(argv[0]);
			ret = -1;
			goto out;
		}
	}

out:
	free(argv_norm);
	return ret;
}

int main(int argc, char *argv[])
{
	int ret = 1;
	sdxi_dev_t *dev = NULL;
	sdxi_cxt_t *cxt = NULL;
	size_t src_size, dst_size, i;
	double elapsed_ns, elapsed_ms, size_mb, bw_gbps;
	struct timespec ts_start, ts_end;
	void *src = NULL, *dst = NULL;
	struct repcopy_args args;
	char **dev_list = NULL;
	size_t dev_count = 0;
	const char *path;

	if (repcopy_parse_args(argc, argv, &args) < 0)
		return 1;

	path = args.sample.dev_path;
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

	src_size = PAGE_SIZE_4KB * SRC_BUFFER_4KB_NUM;
	if (args.repeat > SIZE_MAX / src_size) {
		fprintf(stderr, "repeat count %zu overflows destination size\n",
			args.repeat);
		goto out;
	}
	dst_size = src_size * args.repeat;

	/* init source and dest buffers */
	posix_memalign_check((void **)&src, TEST_BUFFER_ALIGN, src_size);
	posix_memalign_check((void **)&dst, TEST_BUFFER_ALIGN, dst_size);

	printf("SDXI memory rep copy test ...\n");
	printf("    memory buffer src = %p, size = 0x%zx\n", src, src_size);
	printf("    memory buffer dst = %p, size = 0x%zx\n", dst, dst_size);
	printf("    repeat = %zu\n", args.repeat);

	memset(src, 0xAF, src_size);
	memset(dst, 0xBE, dst_size);

	clock_gettime(CLOCK_MONOTONIC, &ts_start);

	ret = sdxi_mem_rep_copy(cxt, dst, src, src_size, args.repeat,
				DEFAULT_TIMEOUT_MS);
	if (ret) {
		fprintf(stderr, "sdxi_mem_rep_copy: %s\n", sdxi_err_str(ret));
		goto out;
	}

	clock_gettime(CLOCK_MONOTONIC, &ts_end);
	elapsed_ns = (ts_end.tv_sec - ts_start.tv_sec) * 1e9 +
		     (ts_end.tv_nsec - ts_start.tv_nsec);
	elapsed_ms = elapsed_ns / 1e6;
	size_mb = (double)dst_size / (1024.0 * 1024.0);

	if (elapsed_ns > 0.0) {
		bw_gbps = (size_mb / 1024.0) / (elapsed_ns / 1e9);
		printf("    size: %.2f MB, elapsed: %.3f ms, bandwidth: %.2f GB/s\n",
		       size_mb, elapsed_ms, bw_gbps);
	} else {
		printf("    size: %.2f MB, elapsed: %.3f ms, bandwidth: n/a\n",
		       size_mb, elapsed_ms);
	}

	/* compare buffer content */
	ret = 0;
	for (i = 0; i < args.repeat; i++) {
		if (memcmp(src, (uint8_t *)dst + i * src_size, src_size) != 0) {
			fprintf(stderr, "Memory rep copy ==> FAIL at repeat %zu\n", i);
			ret = 1;
		}
	}
	if (ret == 0) {
		fprintf(stderr, "Memory rep copy ==> SUCCESS\n");
	}

out:
	(void)sdxi_cxt_destroy(cxt);
	sdxi_dev_close(dev);
	sdxi_free_dev_list(dev_list, dev_count);
	free(src);
	free(dst);

	return ret;
}
