#ifndef _SAMPLES_H
#define _SAMPLES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>

#include "libsdxi.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

inline static void posix_memalign_check(void ** ptr, size_t align, size_t size) {
	int ret = posix_memalign(ptr, align, size);

	switch (ret) {
	case 0:
		return;
	case EINVAL:
		fprintf(stderr, "posix_memalign: invalid argument\n");
		exit(1);
	case ENOMEM:
		fprintf(stderr, "posix_memalign: insufficient memory\n");
		exit(1);
	default:
		exit(1);
	}
}

struct sample_args {
	const char *dev_path;
	char dev_path_buf[PATH_MAX];
};

inline static int sample_normalize_dev_path(const char *path, char *buf,
					    size_t buf_len,
					    const char **path_out)
{
	int n;

	if (!path || !buf || !path_out || buf_len == 0)
		return -EINVAL;

	if (strchr(path, '/') != NULL) {
		*path_out = path;
		return 0;
	}

	n = snprintf(buf, buf_len, "/dev/%s", path);
	if (n < 0 || (size_t)n >= buf_len)
		return -ENAMETOOLONG;

	*path_out = buf;
	return 0;
}

inline static int sample_set_dev_path(struct sample_args *args, const char *path)
{
	if (!args)
		return -EINVAL;

	return sample_normalize_dev_path(path, args->dev_path_buf,
					 sizeof(args->dev_path_buf),
					 &args->dev_path);
}

static struct option sample_long_options[] = {
	{"dev", required_argument, 0, 'd'},
	{"help", no_argument, 0, 'h'},
	{0, 0, 0, 0}
};

inline static void print_usage(const char *prog_name) {
	fprintf(stderr, "Usage: %s [options]\n", prog_name);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -dev, --dev <path>  Specify SDXI device (e.g., sdxi0, /dev/sdxi0)\n");
	fprintf(stderr, "  -h, --help          Show this help message\n");
	fprintf(stderr, "\nIf -dev is not specified, the samples will use the first\n");
	fprintf(stderr, "device from a stable-sorted `/dev/sdxi*` scan.\n");
}

inline static int parse_sample_args(int argc, char *argv[], struct sample_args *args) {
	int opt;
	int option_index = 0;

	memset(args, 0, sizeof(*args));

	/* Reset getopt */
	optind = 1;

	while (1) {
		opt = getopt_long(argc, argv, "d:h", sample_long_options, &option_index);
		if (opt == -1)
			break;

		switch (opt) {
		case 'd':
			if (sample_set_dev_path(args, optarg) != 0) {
				fprintf(stderr, "Invalid device path: %s\n", optarg);
				return -1;
			}
			break;
		case 'h':
			print_usage(argv[0]);
			exit(0);
		default:
			print_usage(argv[0]);
			return -1;
		}
	}

	/* Also check for -dev without the long option format */
	optind = 1;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-dev") == 0 && i + 1 < argc) {
			if (sample_set_dev_path(args, argv[i + 1]) != 0) {
				fprintf(stderr, "Invalid device path: %s\n", argv[i + 1]);
				return -1;
			}
			break;
		}
	}

	return 0;
}

#endif
