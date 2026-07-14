// SPDX-License-Identifier: BSD-3-Clause
/*
 * SDXI library sample code - AMD SHA/SHAKE crypto coverage
 *
 * AMD S2 provides SHA and SHA_COPY commands with SHA2, SHA3, and SHAKE modes.
 * This sample discovers S1/S2 by OpGroup GUID and runs every S2 SHA mode when
 * S2 is present.
 */
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libsdxi.h"
#include <sdxi/vendor/amd_crypto.h>

#define TEST_BUFFER_ALIGN 4096
#define TEST_TIMEOUT_MS 5000

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static const uint8_t sha_msg[] = { 'a', 'b', 'c' };

static const uint8_t sha2_224_init[32] = {
	0xd8, 0x9e, 0x05, 0xc1, 0x07, 0xd5, 0x7c, 0x36,
	0x17, 0xdd, 0x70, 0x30, 0x39, 0x59, 0x0e, 0xf7,
	0x31, 0x0b, 0xc0, 0xff, 0x11, 0x15, 0x58, 0x68,
	0xa7, 0x8f, 0xf9, 0x64, 0xa4, 0x4f, 0xfa, 0xbe,
};

static const uint8_t sha2_256_init[32] = {
	0x67, 0xe6, 0x09, 0x6a, 0x85, 0xae, 0x67, 0xbb,
	0x72, 0xf3, 0x6e, 0x3c, 0x3a, 0xf5, 0x4f, 0xa5,
	0x7f, 0x52, 0x0e, 0x51, 0x8c, 0x68, 0x05, 0x9b,
	0xab, 0xd9, 0x83, 0x1f, 0x19, 0xcd, 0xe0, 0x5b,
};

static const uint8_t sha2_384_init[64] = {
	0xd8, 0x9e, 0x05, 0xc1, 0x5d, 0x9d, 0xbb, 0xcb,
	0x07, 0xd5, 0x7c, 0x36, 0x2a, 0x29, 0x9a, 0x62,
	0x17, 0xdd, 0x70, 0x30, 0x5a, 0x01, 0x59, 0x91,
	0x39, 0x59, 0x0e, 0xf7, 0xd8, 0xec, 0x2f, 0x15,
	0x31, 0x0b, 0xc0, 0xff, 0x67, 0x26, 0x33, 0x67,
	0x11, 0x15, 0x58, 0x68, 0x87, 0x4a, 0xb4, 0x8e,
	0xa7, 0x8f, 0xf9, 0x64, 0x0d, 0x2e, 0x0c, 0xdb,
	0xa4, 0x4f, 0xfa, 0xbe, 0x1d, 0x48, 0xb5, 0x47,
};

static const uint8_t sha2_512_init[64] = {
	0x08, 0xc9, 0xbc, 0xf3, 0x67, 0xe6, 0x09, 0x6a,
	0x3b, 0xa7, 0xca, 0x84, 0x85, 0xae, 0x67, 0xbb,
	0x2b, 0xf8, 0x94, 0xfe, 0x72, 0xf3, 0x6e, 0x3c,
	0xf1, 0x36, 0x1d, 0x5f, 0x3a, 0xf5, 0x4f, 0xa5,
	0xd1, 0x82, 0xe6, 0xad, 0x7f, 0x52, 0x0e, 0x51,
	0x1f, 0x6c, 0x3e, 0x2b, 0x8c, 0x68, 0x05, 0x9b,
	0x6b, 0xbd, 0x41, 0xfb, 0xab, 0xd9, 0x83, 0x1f,
	0x79, 0x21, 0x7e, 0x13, 0x19, 0xcd, 0xe0, 0x5b,
};

struct sha_vec {
	const char *name;
	uint8_t mode;
	size_t digest_len;
	const uint8_t *init;
	size_t init_len;
	uint8_t digest[64];
};

static const struct sha_vec sha_vecs[] = {
	{
		.name = "SHA2-224",
		.mode = SDXI_AMD_S2_SHA_MODE_SHA2_224,
		.digest_len = 28,
		.init = sha2_224_init,
		.init_len = sizeof(sha2_224_init),
		.digest = {
			0x23, 0x09, 0x7d, 0x22, 0x34, 0x05, 0xd8, 0x22,
			0x86, 0x42, 0xa4, 0x77, 0xbd, 0xa2, 0x55, 0xb3,
			0x2a, 0xad, 0xbc, 0xe4, 0xbd, 0xa0, 0xb3, 0xf7,
			0xe3, 0x6c, 0x9d, 0xa7,
		},
	},
	{
		.name = "SHA2-256",
		.mode = SDXI_AMD_S2_SHA_MODE_SHA2_256,
		.digest_len = 32,
		.init = sha2_256_init,
		.init_len = sizeof(sha2_256_init),
		.digest = {
			0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
			0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
			0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
			0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
		},
	},
	{
		.name = "SHA2-384",
		.mode = SDXI_AMD_S2_SHA_MODE_SHA2_384,
		.digest_len = 48,
		.init = sha2_384_init,
		.init_len = sizeof(sha2_384_init),
		.digest = {
			0xcb, 0x00, 0x75, 0x3f, 0x45, 0xa3, 0x5e, 0x8b,
			0xb5, 0xa0, 0x3d, 0x69, 0x9a, 0xc6, 0x50, 0x07,
			0x27, 0x2c, 0x32, 0xab, 0x0e, 0xde, 0xd1, 0x63,
			0x1a, 0x8b, 0x60, 0x5a, 0x43, 0xff, 0x5b, 0xed,
			0x80, 0x86, 0x07, 0x2b, 0xa1, 0xe7, 0xcc, 0x23,
			0x58, 0xba, 0xec, 0xa1, 0x34, 0xc8, 0x25, 0xa7,
		},
	},
	{
		.name = "SHA2-512",
		.mode = SDXI_AMD_S2_SHA_MODE_SHA2_512,
		.digest_len = 64,
		.init = sha2_512_init,
		.init_len = sizeof(sha2_512_init),
		.digest = {
			0xdd, 0xaf, 0x35, 0xa1, 0x93, 0x61, 0x7a, 0xba,
			0xcc, 0x41, 0x73, 0x49, 0xae, 0x20, 0x41, 0x31,
			0x12, 0xe6, 0xfa, 0x4e, 0x89, 0xa9, 0x7e, 0xa2,
			0x0a, 0x9e, 0xee, 0xe6, 0x4b, 0x55, 0xd3, 0x9a,
			0x21, 0x92, 0x99, 0x2a, 0x27, 0x4f, 0xc1, 0xa8,
			0x36, 0xba, 0x3c, 0x23, 0xa3, 0xfe, 0xeb, 0xbd,
			0x45, 0x4d, 0x44, 0x23, 0x64, 0x3c, 0xe8, 0x0e,
			0x2a, 0x9a, 0xc9, 0x4f, 0xa5, 0x4c, 0xa4, 0x9f,
		},
	},
	{
		.name = "SHA3-224",
		.mode = SDXI_AMD_S2_SHA_MODE_SHA3_224,
		.digest_len = 28,
		.digest = {
			0xe6, 0x42, 0x82, 0x4c, 0x3f, 0x8c, 0xf2, 0x4a,
			0xd0, 0x92, 0x34, 0xee, 0x7d, 0x3c, 0x76, 0x6f,
			0xc9, 0xa3, 0xa5, 0x16, 0x8d, 0x0c, 0x94, 0xad,
			0x73, 0xb4, 0x6f, 0xdf,
		},
	},
	{
		.name = "SHA3-256",
		.mode = SDXI_AMD_S2_SHA_MODE_SHA3_256,
		.digest_len = 32,
		.digest = {
			0x3a, 0x98, 0x5d, 0xa7, 0x4f, 0xe2, 0x25, 0xb2,
			0x04, 0x5c, 0x17, 0x2d, 0x6b, 0xd3, 0x90, 0xbd,
			0x85, 0x5f, 0x08, 0x6e, 0x3e, 0x9d, 0x52, 0x5b,
			0x46, 0xbf, 0xe2, 0x45, 0x11, 0x43, 0x15, 0x32,
		},
	},
	{
		.name = "SHA3-384",
		.mode = SDXI_AMD_S2_SHA_MODE_SHA3_384,
		.digest_len = 48,
		.digest = {
			0xec, 0x01, 0x49, 0x82, 0x88, 0x51, 0x6f, 0xc9,
			0x26, 0x45, 0x9f, 0x58, 0xe2, 0xc6, 0xad, 0x8d,
			0xf9, 0xb4, 0x73, 0xcb, 0x0f, 0xc0, 0x8c, 0x25,
			0x96, 0xda, 0x7c, 0xf0, 0xe4, 0x9b, 0xe4, 0xb2,
			0x98, 0xd8, 0x8c, 0xea, 0x92, 0x7a, 0xc7, 0xf5,
			0x39, 0xf1, 0xed, 0xf2, 0x28, 0x37, 0x6d, 0x25,
		},
	},
	{
		.name = "SHA3-512",
		.mode = SDXI_AMD_S2_SHA_MODE_SHA3_512,
		.digest_len = 64,
		.digest = {
			0xb7, 0x51, 0x85, 0x0b, 0x1a, 0x57, 0x16, 0x8a,
			0x56, 0x93, 0xcd, 0x92, 0x4b, 0x6b, 0x09, 0x6e,
			0x08, 0xf6, 0x21, 0x82, 0x74, 0x44, 0xf7, 0x0d,
			0x88, 0x4f, 0x5d, 0x02, 0x40, 0xd2, 0x71, 0x2e,
			0x10, 0xe1, 0x16, 0xe9, 0x19, 0x2a, 0xf3, 0xc9,
			0x1a, 0x7e, 0xc5, 0x76, 0x47, 0xe3, 0x93, 0x40,
			0x57, 0x34, 0x0b, 0x4c, 0xf4, 0x08, 0xd5, 0xa5,
			0x65, 0x92, 0xf8, 0x27, 0x4e, 0xec, 0x53, 0xf0,
		},
	},
	{
		.name = "SHAKE128",
		.mode = SDXI_AMD_S2_SHA_MODE_SHAKE128,
		.digest_len = 16,
		.digest = {
			0x58, 0x81, 0x09, 0x2d, 0xd8, 0x18, 0xbf, 0x5c,
			0xf8, 0xa3, 0xdd, 0xb7, 0x93, 0xfb, 0xcb, 0xa7,
		},
	},
	{
		.name = "SHAKE256",
		.mode = SDXI_AMD_S2_SHA_MODE_SHAKE256,
		.digest_len = 32,
		.digest = {
			0x48, 0x33, 0x66, 0x60, 0x13, 0x60, 0xa8, 0x77,
			0x1c, 0x68, 0x63, 0x08, 0x0c, 0xc4, 0x11, 0x4d,
			0x8d, 0xb4, 0x45, 0x30, 0xf8, 0xf1, 0xe1, 0xee,
			0x4f, 0x94, 0xea, 0x37, 0xe7, 0x8b, 0x57, 0x39,
		},
	},
};

struct amd_sha_args {
	const char *dev_path;
	char dev_path_buf[PATH_MAX];
	uint16_t s1_op_type;
	uint16_t s2_op_type;
};

static int normalize_dev_path(const char *path, char *buf, size_t buf_len,
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

static void posix_memalign_check(void **ptr, size_t align, size_t size)
{
	int ret = posix_memalign(ptr, align, size);

	switch (ret) {
	case 0:
		return;
	case EINVAL:
		fprintf(stderr, "posix_memalign: invalid argument\n");
		break;
	case ENOMEM:
		fprintf(stderr, "posix_memalign: insufficient memory\n");
		break;
	default:
		fprintf(stderr, "posix_memalign: error %d\n", ret);
		break;
	}

	exit(1);
}

static void print_amd_sha_usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [options]\n", prog);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -d, --dev <path>          Specify SDXI device (e.g., sdxi0, /dev/sdxi0)\n");
	fprintf(stderr, "      --s1-op-type <type>   Override AMDCRYPTO_S1 opcode type\n");
	fprintf(stderr, "      --s2-op-type <type>   Override AMDCRYPTO_S2 opcode type\n");
	fprintf(stderr, "      --op-type <type>      Backward-compatible alias for --s2-op-type\n");
	fprintf(stderr, "  -h, --help                Show this help message\n");
}

static int parse_u16_value(const char *s, uint16_t *out)
{
	char *end = NULL;
	unsigned long v;

	if (!s || !out)
		return -EINVAL;

	errno = 0;
	v = strtoul(s, &end, 0);
	if (errno || end == s || *end != '\0' || v == 0 || v > SDXI_DISC_MAX_INDEX)
		return -EINVAL;

	*out = (uint16_t)v;
	return 0;
}

static int parse_args(int argc, char *argv[], struct amd_sha_args *args)
{
	static const struct option long_opts[] = {
		{ "dev", required_argument, 0, 'd' },
		{ "s1-op-type", required_argument, 0, 1000 },
		{ "s2-op-type", required_argument, 0, 1001 },
		{ "op-type", required_argument, 0, 1002 },
		{ "help", no_argument, 0, 'h' },
		{ 0, 0, 0, 0 },
	};
	int opt;

	memset(args, 0, sizeof(*args));

	optind = 1;
	while ((opt = getopt_long(argc, argv, "d:h", long_opts, NULL)) != -1) {
		switch (opt) {
		case 'd':
			if (normalize_dev_path(optarg, args->dev_path_buf,
					       sizeof(args->dev_path_buf),
					       &args->dev_path))
				return -EINVAL;
			break;
		case 1000:
			if (parse_u16_value(optarg, &args->s1_op_type))
				return -EINVAL;
			break;
		case 1001:
		case 1002:
			if (parse_u16_value(optarg, &args->s2_op_type))
				return -EINVAL;
			break;
		case 'h':
			print_amd_sha_usage(argv[0]);
			exit(0);
		default:
			return -EINVAL;
		}
	}

	for (int i = 1; i + 1 < argc; i++) {
		if (strcmp(argv[i], "-dev") == 0) {
			if (normalize_dev_path(argv[i + 1], args->dev_path_buf,
					       sizeof(args->dev_path_buf),
					       &args->dev_path))
				return -EINVAL;
			break;
		}
	}

	return 0;
}

static int skip_capability_err(int err)
{
	switch (err) {
	case -ENODEV:
	case -ENOTSUP:
	case -EPERM:
	case -EACCES:
		return 1;
	default:
		return 0;
	}
}

static int get_op_type(sdxi_dev_t *dev, enum sdxi_grp_id grp, const char *name,
		       uint16_t manual, const char *manual_opt,
		       uint16_t *op_type_out, bool *present_out)
{
	int ret;

	*op_type_out = 0;
	*present_out = false;

	if (manual) {
		*op_type_out = manual;
		*present_out = true;
		printf("%s opcode type: %u (manual)\n", name, manual);
		return 0;
	}

	ret = sdxi_dev_get_op_type(dev, grp, TEST_TIMEOUT_MS, op_type_out);
	if (!ret) {
		*present_out = true;
		printf("%s opcode type: %u\n", name, *op_type_out);
		return 0;
	}

	if (skip_capability_err(ret)) {
		if (ret == -EPERM || ret == -EACCES)
			printf("%s: discovery needs privileges; pass %s to override\n",
			       name, manual_opt);
		else
			printf("%s: not present (%s)\n", name, sdxi_err_str(ret));
		return 0;
	}

	fprintf(stderr, "sdxi_dev_get_op_type(%s): %s\n", name, sdxi_err_str(ret));
	return ret;
}

static void sha_state_init(struct sdxi_amd_s2_crypto_state *state,
			   const struct sha_vec *vec)
{
	memset(state, 0, sizeof(*state));
	if (vec->init)
		memcpy(state->hash_data, vec->init, vec->init_len);
}

static int run_s2_sha_one(sdxi_cxt_t *cxt, uint16_t op_type,
			  const struct sha_vec *vec, bool copy)
{
	struct sdxi_amd_s2_crypto_state *state = NULL;
	uint8_t *src = NULL;
	uint8_t *dst = NULL;
	int ret;

	posix_memalign_check((void **)&state, SDXI_AMD_S2_CRYPTO_STATE_ALIGN,
			     sizeof(*state));
	posix_memalign_check((void **)&src, TEST_BUFFER_ALIGN, sizeof(sha_msg));
	if (copy)
		posix_memalign_check((void **)&dst, TEST_BUFFER_ALIGN, sizeof(sha_msg));

	memcpy(src, sha_msg, sizeof(sha_msg));
	if (dst)
		memset(dst, 0, sizeof(sha_msg));
	sha_state_init(state, vec);

	if (copy) {
		struct sdxi_amd_s2_sha_copy_req req = {
			.size = sizeof(req),
			.op_type = op_type,
			.sha_mode = vec->mode,
			.finalize = 1,
			.zero_byte_req = 0,
			.ingress_crc = 0,
			.egress_crc = 0,
			.attr_src = 0,
			.attr_dst = 0,
			.akey_src = 0x1,
			.akey_dst = 0x1,
			.src = src,
			.dst = dst,
			.nbytes = sizeof(sha_msg),
			.state = state,
		};

		ret = sdxi_amd_s2_sha_copy(cxt, &req, TEST_TIMEOUT_MS);
	} else {
		struct sdxi_amd_s2_sha_req req = {
			.size = sizeof(req),
			.op_type = op_type,
			.sha_mode = vec->mode,
			.sha_src = SDXI_AMD_S2_SHA_SRC_INGRESS,
			.finalize = 1,
			.zero_byte_req = 0,
			.ingress_crc = 0,
			.attr_src = 0,
			.akey_src = 0x1,
			.src = src,
			.nbytes = sizeof(sha_msg),
			.state = state,
		};

		ret = sdxi_amd_s2_sha(cxt, &req, TEST_TIMEOUT_MS);
	}

	if (ret) {
		fprintf(stderr, "AMD S2 %s%s: %s\n", vec->name,
			copy ? " SHA_COPY" : " SHA", sdxi_err_str(ret));
		goto out;
	}

	if (state->current_size != sizeof(sha_msg)) {
		fprintf(stderr, "AMD S2 %s%s: current_size=%lu expected=%lu\n",
			vec->name, copy ? " SHA_COPY" : " SHA",
			(unsigned long)state->current_size,
			(unsigned long)sizeof(sha_msg));
		ret = -EIO;
		goto out;
	}

	if (memcmp(state->hash_data, vec->digest, vec->digest_len) != 0) {
		fprintf(stderr, "AMD S2 %s%s: digest mismatch\n", vec->name,
			copy ? " SHA_COPY" : " SHA");
		ret = -EIO;
		goto out;
	}

	if (copy && memcmp(dst, src, sizeof(sha_msg)) != 0) {
		fprintf(stderr, "AMD S2 %s SHA_COPY: copy mismatch\n", vec->name);
		ret = -EIO;
		goto out;
	}

	printf("AMD S2 %s%s: PASS\n", vec->name, copy ? " SHA_COPY" : " SHA");
	ret = 0;

out:
	free(state);
	free(src);
	free(dst);
	return ret;
}

static int run_s2_sha_suite(sdxi_cxt_t *cxt, uint16_t op_type)
{
	int failures = 0;

	for (size_t i = 0; i < sizeof(sha_vecs) / sizeof(sha_vecs[0]); i++) {
		if (run_s2_sha_one(cxt, op_type, &sha_vecs[i], false))
			failures++;
		if (run_s2_sha_one(cxt, op_type, &sha_vecs[i], true))
			failures++;
	}

	return failures ? -EIO : 0;
}

int main(int argc, char *argv[])
{
	int ret = 1;
	struct amd_sha_args args;
	char **dev_list = NULL;
	size_t dev_count = 0;
	const char *path;
	uint16_t s1_op_type = 0;
	uint16_t s2_op_type = 0;
	bool has_s1 = false;
	bool has_s2 = false;
	sdxi_dev_t *dev = NULL;
	sdxi_cxt_t *cxt = NULL;

	if (parse_args(argc, argv, &args) < 0) {
		print_amd_sha_usage(argv[0]);
		return 1;
	}

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

	ret = get_op_type(dev, SDXI_GRP_AMDCRYPTO_S1, "AMDCRYPTO_S1",
			  args.s1_op_type, "--s1-op-type", &s1_op_type, &has_s1);
	if (ret)
		goto out;
	ret = get_op_type(dev, SDXI_GRP_AMDCRYPTO_S2, "AMDCRYPTO_S2",
			  args.s2_op_type, "--s2-op-type", &s2_op_type, &has_s2);
	if (ret)
		goto out;

	if (has_s1)
		printf("AMDCRYPTO_S1 has no SHA/SHAKE command; skipping S1 SHA tests\n");

	if (!has_s2) {
		printf("SKIP: AMDCRYPTO_S2 SHA/SHAKE opgroup not present\n");
		ret = 0;
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

	ret = run_s2_sha_suite(cxt, s2_op_type);
	if (ret) {
		fprintf(stderr, "AMD SHA: FAIL\n");
		ret = 1;
	} else {
		printf("AMD SHA: PASS\n");
	}

out:
	(void)sdxi_cxt_destroy(cxt);
	sdxi_dev_close(dev);
	sdxi_free_dev_list(dev_list, dev_count);
	return ret ? 1 : 0;
}
