// SPDX-License-Identifier: BSD-3-Clause
/*
 * SDXI library sample code - AMD AES crypto coverage
 *
 * The program discovers AMDCRYPTO_S1 / AMDCRYPTO_S2 by GUID-backed OpGroup
 * discovery and runs the AES operations advertised by the matching group.
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
#define AES_TEST_BYTES 64
#define AES_XTS_TEST_BYTES 512
#define GCM_AAD_BYTES 20
#define TEST_TIMEOUT_MS 5000

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static const uint8_t aes_plaintext[AES_TEST_BYTES] = {
	0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
	0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
	0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
	0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
	0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
	0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
	0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
	0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10,
};

static const uint8_t aes_key_128[16] = {
	0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
	0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
};

static const uint8_t aes_key_256[32] = {
	0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
	0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
	0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
	0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4,
};

static const uint8_t ecb_ciphertext[2][AES_TEST_BYTES] = {
	{
		0x3a, 0xd7, 0x7b, 0xb4, 0x0d, 0x7a, 0x36, 0x60,
		0xa8, 0x9e, 0xca, 0xf3, 0x24, 0x66, 0xef, 0x97,
		0xf5, 0xd3, 0xd5, 0x85, 0x03, 0xb9, 0x69, 0x9d,
		0xe7, 0x85, 0x89, 0x5a, 0x96, 0xfd, 0xba, 0xaf,
		0x43, 0xb1, 0xcd, 0x7f, 0x59, 0x8e, 0xce, 0x23,
		0x88, 0x1b, 0x00, 0xe3, 0xed, 0x03, 0x06, 0x88,
		0x7b, 0x0c, 0x78, 0x5e, 0x27, 0xe8, 0xad, 0x3f,
		0x82, 0x23, 0x20, 0x71, 0x04, 0x72, 0x5d, 0xd4,
	},
	{
		0xf3, 0xee, 0xd1, 0xbd, 0xb5, 0xd2, 0xa0, 0x3c,
		0x06, 0x4b, 0x5a, 0x7e, 0x3d, 0xb1, 0x81, 0xf8,
		0x59, 0x1c, 0xcb, 0x10, 0xd4, 0x10, 0xed, 0x26,
		0xdc, 0x5b, 0xa7, 0x4a, 0x31, 0x36, 0x28, 0x70,
		0xb6, 0xed, 0x21, 0xb9, 0x9c, 0xa6, 0xf4, 0xf9,
		0xf1, 0x53, 0xe7, 0xb1, 0xbe, 0xaf, 0xed, 0x1d,
		0x23, 0x30, 0x4b, 0x7a, 0x39, 0xf9, 0xf3, 0xff,
		0x06, 0x7d, 0x8d, 0x8f, 0x9e, 0x24, 0xec, 0xc7,
	},
};

static const uint8_t ctr_ciphertext[2][AES_TEST_BYTES] = {
	{
		0x87, 0x4d, 0x61, 0x91, 0xb6, 0x20, 0xe3, 0x26,
		0x1b, 0xef, 0x68, 0x64, 0x99, 0x0d, 0xb6, 0xce,
		0x98, 0x06, 0xf6, 0x6b, 0x79, 0x70, 0xfd, 0xff,
		0x86, 0x17, 0x18, 0x7b, 0xb9, 0xff, 0xfd, 0xff,
		0x5a, 0xe4, 0xdf, 0x3e, 0xdb, 0xd5, 0xd3, 0x5e,
		0x5b, 0x4f, 0x09, 0x02, 0x0d, 0xb0, 0x3e, 0xab,
		0x1e, 0x03, 0x1d, 0xda, 0x2f, 0xbe, 0x03, 0xd1,
		0x79, 0x21, 0x70, 0xa0, 0xf3, 0x00, 0x9c, 0xee,
	},
	{
		0x60, 0x1e, 0xc3, 0x13, 0x77, 0x57, 0x89, 0xa5,
		0xb7, 0xa7, 0xf5, 0x04, 0xbb, 0xf3, 0xd2, 0x28,
		0xf4, 0x43, 0xe3, 0xca, 0x4d, 0x62, 0xb5, 0x9a,
		0xca, 0x84, 0xe9, 0x90, 0xca, 0xca, 0xf5, 0xc5,
		0x2b, 0x09, 0x30, 0xda, 0xa2, 0x3d, 0xe9, 0x4c,
		0xe8, 0x70, 0x17, 0xba, 0x2d, 0x84, 0x98, 0x8d,
		0xdf, 0xc9, 0xc5, 0x8d, 0xb6, 0x7a, 0xad, 0xa6,
		0x13, 0xc2, 0xdd, 0x08, 0x45, 0x79, 0x41, 0xa6,
	},
};

static const uint8_t ctr_counter[16] = {
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
};

static const uint8_t xts_key_128[64] = {
	0x27, 0x18, 0x28, 0x18, 0x28, 0x45, 0x90, 0x45,
	0x23, 0x53, 0x60, 0x28, 0x74, 0x71, 0x35, 0x26,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x31, 0x41, 0x59, 0x26, 0x53, 0x58, 0x97, 0x93,
	0x23, 0x84, 0x62, 0x64, 0x33, 0x83, 0x27, 0x95,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t xts_tweak_0[16] = { 0 };

static const uint8_t gcm_key_128[16] = {
	0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c,
	0x6d, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08,
};

static const uint8_t gcm_iv_96b[12] = {
	0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce, 0xdb, 0xad,
	0xde, 0xca, 0xf8, 0x88,
};

static const uint8_t gcm_aad[GCM_AAD_BYTES] = {
	0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef,
	0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef,
	0xab, 0xad, 0xda, 0xd2,
};

static const uint8_t gcm_plaintext[AES_TEST_BYTES] = {
	0xd9, 0x31, 0x32, 0x25, 0xf8, 0x84, 0x06, 0xe5,
	0xa5, 0x59, 0x09, 0xc5, 0xaf, 0xf5, 0x26, 0x9a,
	0x86, 0xa7, 0xa9, 0x53, 0x15, 0x34, 0xf7, 0xda,
	0x2e, 0x4c, 0x30, 0x3d, 0x8a, 0x31, 0x8a, 0x72,
	0x1c, 0x3c, 0x0c, 0x95, 0x95, 0x68, 0x09, 0x53,
	0x2f, 0xcf, 0x0e, 0x24, 0x49, 0xa6, 0xb5, 0x25,
	0xb1, 0x6a, 0xed, 0xf5, 0xaa, 0x0d, 0xe6, 0x57,
	0xba, 0x63, 0x7b, 0x39, 0x1a, 0xaf, 0xd2, 0x55,
};

static const uint8_t gcm_ciphertext[AES_TEST_BYTES] = {
	0x42, 0x83, 0x1e, 0xc2, 0x21, 0x77, 0x74, 0x24,
	0x4b, 0x72, 0x21, 0xb7, 0x84, 0xd0, 0xd4, 0x9c,
	0xe3, 0xaa, 0x21, 0x2f, 0x2c, 0x02, 0xa4, 0xe0,
	0x35, 0xc1, 0x7e, 0x23, 0x29, 0xac, 0xa1, 0x2e,
	0x21, 0xd5, 0x14, 0xb2, 0x54, 0x66, 0x93, 0x1c,
	0x7d, 0x8f, 0x6a, 0x5a, 0xac, 0x84, 0xaa, 0x05,
	0x1b, 0xa3, 0x0b, 0x39, 0x6a, 0x0a, 0xac, 0x97,
	0x3d, 0x58, 0xe0, 0x91, 0x47, 0x3f, 0x59, 0x85,
};

static const uint8_t gcm_tag[SDXI_AMD_S2_GCM_TAG_BYTES] = {
	0xda, 0x80, 0xce, 0x83, 0x0c, 0xfd, 0xa0, 0x2d,
	0xa2, 0xa2, 0x18, 0xa1, 0x74, 0x4f, 0x4c, 0x76,
};

struct amd_aes_args {
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

static void print_amd_aes_usage(const char *prog)
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

static int parse_args(int argc, char *argv[], struct amd_aes_args *args)
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
			print_amd_aes_usage(argv[0]);
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

static void fill_xts_plaintext(uint8_t *buf)
{
	for (size_t i = 0; i < AES_XTS_TEST_BYTES; i++)
		buf[i] = (uint8_t)i;
}

static void s1_xts_state_init(struct sdxi_amd_s1_crypto_state *state)
{
	memset(state, 0, sizeof(*state));
	memcpy(state->aes_ctr_twk, xts_tweak_0, sizeof(xts_tweak_0));
}

static void s2_xts_state_init(struct sdxi_amd_s2_crypto_state *state)
{
	memset(state, 0, sizeof(*state));
	memcpy(state->aes_ctr_twk, xts_tweak_0, sizeof(xts_tweak_0));
}

static void s2_ctr_state_init(struct sdxi_amd_s2_crypto_state *state)
{
	memset(state, 0, sizeof(*state));
	memcpy(state->aes_ctr_twk, ctr_counter, sizeof(ctr_counter));
}

static void s2_gcm_state_init(struct sdxi_amd_s2_crypto_state *state)
{
	memset(state, 0, sizeof(*state));
	memcpy(state->aes_ctr_twk, gcm_iv_96b, sizeof(gcm_iv_96b));

	state->aes_ctr_twk[12] = 0x00;
	state->aes_ctr_twk[13] = 0x00;
	state->aes_ctr_twk[14] = 0x00;
	state->aes_ctr_twk[15] = 0x02;
	state->hash_data[0x10] = 0x00;
	state->hash_data[0x11] = 0x00;
	state->hash_data[0x12] = 0x00;
	state->hash_data[0x13] = 0x01;
}

static void s1_key_init(struct sdxi_amd_s1_aes_key *key, const uint8_t *raw,
			size_t bytes)
{
	size_t copy = bytes;

	if (copy > sizeof(key->aes_key))
		copy = sizeof(key->aes_key);

	memset(key, 0, sizeof(*key));
	memcpy(key->aes_key, raw, copy);
}

static void s1_xts_key_init(struct sdxi_amd_s1_aes_key *key)
{
	memcpy(key, xts_key_128, sizeof(*key));
}

static void s2_key_init(struct sdxi_amd_s2_aes_key *key, const uint8_t *raw,
			size_t bytes)
{
	size_t copy = bytes;

	if (copy > sizeof(key->aes_key))
		copy = sizeof(key->aes_key);

	memset(key, 0, sizeof(*key));
	memcpy(key->aes_key, raw, copy);
}

static void s2_xts_key_init(struct sdxi_amd_s2_aes_key *key)
{
	memcpy(key, xts_key_128, sizeof(*key));
}

static int run_s1_ecb(sdxi_cxt_t *cxt, uint16_t op_type)
{
	struct sdxi_amd_s1_aes_key *key = NULL;
	struct sdxi_amd_s1_crypto_state *state = NULL;
	uint8_t *src = NULL;
	uint8_t *dst = NULL;
	int ret = 0;

	posix_memalign_check((void **)&key, SDXI_AMD_S1_AES_KEY_ALIGN, sizeof(*key));
	posix_memalign_check((void **)&state, SDXI_AMD_S1_CRYPTO_STATE_ALIGN, sizeof(*state));
	posix_memalign_check((void **)&src, TEST_BUFFER_ALIGN, AES_TEST_BYTES);
	posix_memalign_check((void **)&dst, TEST_BUFFER_ALIGN, AES_TEST_BYTES);

	for (uint8_t ks = 0; ks < 2; ks++) {
		const uint8_t *raw_key = ks ? aes_key_256 : aes_key_128;
		size_t key_bytes = ks ? sizeof(aes_key_256) : sizeof(aes_key_128);
		struct sdxi_amd_s1_aes_ecb_req req = {
			.size = sizeof(req),
			.op_type = op_type,
			.aes_key_size = ks,
			.attr_src = 0,
			.attr_dst = 0,
			.akey_src = 0x1,
			.akey_dst = 0x1,
			.src = src,
			.dst = dst,
			.nbytes = AES_TEST_BYTES,
			.key = key,
			.state = state,
		};

		s1_key_init(key, raw_key, key_bytes);
		memset(state, 0, sizeof(*state));
		memcpy(src, aes_plaintext, AES_TEST_BYTES);
		memset(dst, 0, AES_TEST_BYTES);
		req.aes_op = SDXI_AMD_S1_AES_OP_ENCRYPT;
		ret = sdxi_amd_s1_aes_ecb(cxt, &req, TEST_TIMEOUT_MS);
		if (ret || memcmp(dst, ecb_ciphertext[ks], AES_TEST_BYTES) != 0)
			goto out_fail;

		memset(state, 0, sizeof(*state));
		memcpy(src, ecb_ciphertext[ks], AES_TEST_BYTES);
		memset(dst, 0, AES_TEST_BYTES);
		req.aes_op = SDXI_AMD_S1_AES_OP_DECRYPT;
		ret = sdxi_amd_s1_aes_ecb(cxt, &req, TEST_TIMEOUT_MS);
		if (ret || memcmp(dst, aes_plaintext, AES_TEST_BYTES) != 0)
			goto out_fail;
	}

	printf("AMD S1 AES-ECB: PASS\n");
	goto out;

out_fail:
	if (ret)
		fprintf(stderr, "AMD S1 AES-ECB: %s\n", sdxi_err_str(ret));
	else
		fprintf(stderr, "AMD S1 AES-ECB: data mismatch\n");
	if (!ret)
		ret = -EIO;
out:
	free(key);
	free(state);
	free(src);
	free(dst);
	return ret;
}

static int run_s1_xts(sdxi_cxt_t *cxt, uint16_t op_type)
{
	struct sdxi_amd_s1_aes_key *key = NULL;
	struct sdxi_amd_s1_crypto_state *state = NULL;
	uint8_t *plain = NULL;
	uint8_t *cipher = NULL;
	uint8_t *out = NULL;
	int ret;

	posix_memalign_check((void **)&key, SDXI_AMD_S1_AES_KEY_ALIGN, sizeof(*key));
	posix_memalign_check((void **)&state, SDXI_AMD_S1_CRYPTO_STATE_ALIGN, sizeof(*state));
	posix_memalign_check((void **)&plain, TEST_BUFFER_ALIGN, AES_XTS_TEST_BYTES);
	posix_memalign_check((void **)&cipher, TEST_BUFFER_ALIGN, AES_XTS_TEST_BYTES);
	posix_memalign_check((void **)&out, TEST_BUFFER_ALIGN, AES_XTS_TEST_BYTES);

	fill_xts_plaintext(plain);
	memset(cipher, 0, AES_XTS_TEST_BYTES);
	memset(out, 0, AES_XTS_TEST_BYTES);
	s1_xts_key_init(key);
	s1_xts_state_init(state);

	struct sdxi_amd_s1_aes_xts_req req = {
		.size = sizeof(req),
		.op_type = op_type,
		.aes_op = SDXI_AMD_S1_AES_OP_ENCRYPT,
		.aes_key_size = SDXI_AMD_S1_AES_KEY_SIZE_128,
		.xts_block_size = SDXI_AMD_S1_XTS_BLOCK_SIZE_512B,
		.attr_src = 0,
		.attr_dst = 0,
		.akey_src = 0x1,
		.akey_dst = 0x1,
		.src = plain,
		.dst = cipher,
		.nbytes = AES_XTS_TEST_BYTES,
		.key = key,
		.state = state,
	};

	ret = sdxi_amd_s1_aes_xts(cxt, &req, TEST_TIMEOUT_MS);
	if (ret)
		goto out_fail;
	if (memcmp(plain, cipher, AES_XTS_TEST_BYTES) == 0) {
		ret = -EIO;
		goto out_fail;
	}

	s1_xts_state_init(state);
	req.aes_op = SDXI_AMD_S1_AES_OP_DECRYPT;
	req.src = cipher;
	req.dst = out;
	ret = sdxi_amd_s1_aes_xts(cxt, &req, TEST_TIMEOUT_MS);
	if (ret || memcmp(out, plain, AES_XTS_TEST_BYTES) != 0)
		goto out_fail;

	printf("AMD S1 AES-XTS: PASS\n");
	goto out;

out_fail:
	if (ret)
		fprintf(stderr, "AMD S1 AES-XTS: %s\n", sdxi_err_str(ret));
	else
		fprintf(stderr, "AMD S1 AES-XTS: data mismatch\n");
	if (!ret)
		ret = -EIO;
out:
	free(key);
	free(state);
	free(plain);
	free(cipher);
	free(out);
	return ret;
}

static int run_s2_ecb_c(sdxi_cxt_t *cxt, uint16_t op_type)
{
	struct sdxi_amd_s2_aes_key *key = NULL;
	struct sdxi_amd_s2_crypto_state *state = NULL;
	uint8_t *src = NULL;
	uint8_t *dst = NULL;
	int ret = 0;

	posix_memalign_check((void **)&key, SDXI_AMD_S2_AES_KEY_ALIGN, sizeof(*key));
	posix_memalign_check((void **)&state, SDXI_AMD_S2_CRYPTO_STATE_ALIGN, sizeof(*state));
	posix_memalign_check((void **)&src, TEST_BUFFER_ALIGN, AES_TEST_BYTES);
	posix_memalign_check((void **)&dst, TEST_BUFFER_ALIGN, AES_TEST_BYTES);

	for (uint8_t ks = 0; ks < 2; ks++) {
		const uint8_t *raw_key = ks ? aes_key_256 : aes_key_128;
		size_t key_bytes = ks ? sizeof(aes_key_256) : sizeof(aes_key_128);
		struct sdxi_amd_s2_aes_ecb_req req = {
			.size = sizeof(req),
			.op_type = op_type,
			.aes_key_size = ks,
			.attr_src = 0,
			.attr_dst = 0,
			.akey_src = 0x1,
			.akey_dst = 0x1,
			.src = src,
			.dst = dst,
			.nbytes = AES_TEST_BYTES,
			.key = key,
			.state = state,
		};

		s2_key_init(key, raw_key, key_bytes);
		memset(state, 0, sizeof(*state));
		memcpy(src, aes_plaintext, AES_TEST_BYTES);
		memset(dst, 0, AES_TEST_BYTES);
		req.aes_op = SDXI_AMD_S2_AES_OP_ENCRYPT;
		ret = sdxi_amd_s2_aes_ecb_c(cxt, &req, TEST_TIMEOUT_MS);
		if (ret || memcmp(dst, ecb_ciphertext[ks], AES_TEST_BYTES) != 0)
			goto out_fail;

		memset(state, 0, sizeof(*state));
		memcpy(src, ecb_ciphertext[ks], AES_TEST_BYTES);
		memset(dst, 0, AES_TEST_BYTES);
		req.aes_op = SDXI_AMD_S2_AES_OP_DECRYPT;
		ret = sdxi_amd_s2_aes_ecb_c(cxt, &req, TEST_TIMEOUT_MS);
		if (ret || memcmp(dst, aes_plaintext, AES_TEST_BYTES) != 0)
			goto out_fail;
	}

	printf("AMD S2 AES-ECB-C: PASS\n");
	goto out;

out_fail:
	if (ret)
		fprintf(stderr, "AMD S2 AES-ECB-C: %s\n", sdxi_err_str(ret));
	else
		fprintf(stderr, "AMD S2 AES-ECB-C: data mismatch\n");
	if (!ret)
		ret = -EIO;
out:
	free(key);
	free(state);
	free(src);
	free(dst);
	return ret;
}

static int run_s2_xts_c(sdxi_cxt_t *cxt, uint16_t op_type)
{
	struct sdxi_amd_s2_aes_key *key = NULL;
	struct sdxi_amd_s2_crypto_state *state = NULL;
	uint8_t *plain = NULL;
	uint8_t *cipher = NULL;
	uint8_t *out = NULL;
	int ret;

	posix_memalign_check((void **)&key, SDXI_AMD_S2_AES_KEY_ALIGN, sizeof(*key));
	posix_memalign_check((void **)&state, SDXI_AMD_S2_CRYPTO_STATE_ALIGN, sizeof(*state));
	posix_memalign_check((void **)&plain, TEST_BUFFER_ALIGN, AES_XTS_TEST_BYTES);
	posix_memalign_check((void **)&cipher, TEST_BUFFER_ALIGN, AES_XTS_TEST_BYTES);
	posix_memalign_check((void **)&out, TEST_BUFFER_ALIGN, AES_XTS_TEST_BYTES);

	fill_xts_plaintext(plain);
	memset(cipher, 0, AES_XTS_TEST_BYTES);
	memset(out, 0, AES_XTS_TEST_BYTES);
	s2_xts_key_init(key);
	s2_xts_state_init(state);

	struct sdxi_amd_s2_aes_xts_req req = {
		.size = sizeof(req),
		.op_type = op_type,
		.aes_op = SDXI_AMD_S2_AES_OP_ENCRYPT,
		.aes_key_size = SDXI_AMD_S2_AES_KEY_SIZE_128,
		.xts_block_size = SDXI_AMD_S2_XTS_BLOCK_SIZE_512B,
		.attr_src = 0,
		.attr_dst = 0,
		.akey_src = 0x1,
		.akey_dst = 0x1,
		.src = plain,
		.dst = cipher,
		.nbytes = AES_XTS_TEST_BYTES,
		.key = key,
		.state = state,
	};

	ret = sdxi_amd_s2_aes_xts_c(cxt, &req, TEST_TIMEOUT_MS);
	if (ret)
		goto out_fail;
	if (memcmp(plain, cipher, AES_XTS_TEST_BYTES) == 0) {
		ret = -EIO;
		goto out_fail;
	}

	s2_xts_state_init(state);
	req.aes_op = SDXI_AMD_S2_AES_OP_DECRYPT;
	req.src = cipher;
	req.dst = out;
	ret = sdxi_amd_s2_aes_xts_c(cxt, &req, TEST_TIMEOUT_MS);
	if (ret || memcmp(out, plain, AES_XTS_TEST_BYTES) != 0)
		goto out_fail;

	printf("AMD S2 AES-XTS-C: PASS\n");
	goto out;

out_fail:
	if (ret)
		fprintf(stderr, "AMD S2 AES-XTS-C: %s\n", sdxi_err_str(ret));
	else
		fprintf(stderr, "AMD S2 AES-XTS-C: data mismatch\n");
	if (!ret)
		ret = -EIO;
out:
	free(key);
	free(state);
	free(plain);
	free(cipher);
	free(out);
	return ret;
}

static int run_s2_ctr(sdxi_cxt_t *cxt, uint16_t op_type)
{
	struct sdxi_amd_s2_aes_key *key = NULL;
	struct sdxi_amd_s2_crypto_state *state = NULL;
	uint8_t *src = NULL;
	uint8_t *dst = NULL;
	int ret = 0;

	posix_memalign_check((void **)&key, SDXI_AMD_S2_AES_KEY_ALIGN, sizeof(*key));
	posix_memalign_check((void **)&state, SDXI_AMD_S2_CRYPTO_STATE_ALIGN, sizeof(*state));
	posix_memalign_check((void **)&src, TEST_BUFFER_ALIGN, AES_TEST_BYTES);
	posix_memalign_check((void **)&dst, TEST_BUFFER_ALIGN, AES_TEST_BYTES);

	for (uint8_t ks = 0; ks < 2; ks++) {
		const uint8_t *raw_key = ks ? aes_key_256 : aes_key_128;
		size_t key_bytes = ks ? sizeof(aes_key_256) : sizeof(aes_key_128);
		struct sdxi_amd_s2_aes_ctr_req req = {
			.size = sizeof(req),
			.op_type = op_type,
			.aes_key_size = ks,
			.ctr_iv_size = SDXI_AMD_S2_AES_CTR_IV_SIZE_16B,
			.attr_src = 0,
			.attr_dst = 0,
			.akey_src = 0x1,
			.akey_dst = 0x1,
			.src = src,
			.dst = dst,
			.nbytes = AES_TEST_BYTES,
			.key = key,
			.state = state,
		};

		s2_key_init(key, raw_key, key_bytes);
		s2_ctr_state_init(state);
		memcpy(src, aes_plaintext, AES_TEST_BYTES);
		memset(dst, 0, AES_TEST_BYTES);
		req.aes_op = SDXI_AMD_S2_AES_OP_ENCRYPT;
		ret = sdxi_amd_s2_aes_ctr(cxt, &req, TEST_TIMEOUT_MS);
		if (ret || memcmp(dst, ctr_ciphertext[ks], AES_TEST_BYTES) != 0)
			goto out_fail;

		s2_ctr_state_init(state);
		memcpy(src, ctr_ciphertext[ks], AES_TEST_BYTES);
		memset(dst, 0, AES_TEST_BYTES);
		req.aes_op = SDXI_AMD_S2_AES_OP_DECRYPT;
		ret = sdxi_amd_s2_aes_ctr(cxt, &req, TEST_TIMEOUT_MS);
		if (ret || memcmp(dst, aes_plaintext, AES_TEST_BYTES) != 0)
			goto out_fail;
	}

	printf("AMD S2 AES-CTR: PASS\n");
	goto out;

out_fail:
	if (ret)
		fprintf(stderr, "AMD S2 AES-CTR: %s\n", sdxi_err_str(ret));
	else
		fprintf(stderr, "AMD S2 AES-CTR: data mismatch\n");
	if (!ret)
		ret = -EIO;
out:
	free(key);
	free(state);
	free(src);
	free(dst);
	return ret;
}

static int run_s2_ctr_sha(sdxi_cxt_t *cxt, uint16_t op_type)
{
	struct sdxi_amd_s2_aes_key *key = NULL;
	struct sdxi_amd_s2_crypto_state *state = NULL;
	uint8_t *src = NULL;
	uint8_t *cipher = NULL;
	uint8_t *out = NULL;
	int ret;

	posix_memalign_check((void **)&key, SDXI_AMD_S2_AES_KEY_ALIGN, sizeof(*key));
	posix_memalign_check((void **)&state, SDXI_AMD_S2_CRYPTO_STATE_ALIGN,
			     sizeof(*state));
	posix_memalign_check((void **)&src, TEST_BUFFER_ALIGN, AES_TEST_BYTES);
	posix_memalign_check((void **)&cipher, TEST_BUFFER_ALIGN, AES_TEST_BYTES);
	posix_memalign_check((void **)&out, TEST_BUFFER_ALIGN, AES_TEST_BYTES);

	s2_key_init(key, aes_key_128, sizeof(aes_key_128));
	memcpy(src, aes_plaintext, AES_TEST_BYTES);
	memset(cipher, 0, AES_TEST_BYTES);
	memset(out, 0, AES_TEST_BYTES);
	s2_ctr_state_init(state);

	struct sdxi_amd_s2_aes_ctr_sha_req req = {
		.size = sizeof(req),
		.op_type = op_type,
		.aes_op = SDXI_AMD_S2_AES_OP_ENCRYPT,
		.aes_key_size = SDXI_AMD_S2_AES_KEY_SIZE_128,
		.ctr_iv_size = SDXI_AMD_S2_AES_CTR_IV_SIZE_16B,
		.sha_mode = SDXI_AMD_S2_SHA_MODE_SHA2_256,
		.sha_src = SDXI_AMD_S2_SHA_SRC_EGRESS,
		.finalize = 1,
		.attr_src = 0,
		.attr_dst = 0,
		.akey_src = 0x1,
		.akey_dst = 0x1,
		.src = src,
		.dst = cipher,
		.nbytes = AES_TEST_BYTES,
		.key = key,
		.state = state,
	};

	ret = sdxi_amd_s2_aes_ctr_sha(cxt, &req, TEST_TIMEOUT_MS);
	if (ret)
		goto out_fail;
	if (memcmp(src, cipher, AES_TEST_BYTES) == 0 || state->current_size == 0) {
		ret = -EIO;
		goto out_fail;
	}

	s2_ctr_state_init(state);
	req.aes_op = SDXI_AMD_S2_AES_OP_DECRYPT;
	req.src = cipher;
	req.dst = out;
	ret = sdxi_amd_s2_aes_ctr_sha(cxt, &req, TEST_TIMEOUT_MS);
	if (ret || memcmp(out, src, AES_TEST_BYTES) != 0 ||
	    state->current_size == 0)
		goto out_fail;

	printf("AMD S2 AES-CTR-SHA: PASS\n");
	goto out;

out_fail:
	if (ret)
		fprintf(stderr, "AMD S2 AES-CTR-SHA: %s\n", sdxi_err_str(ret));
	else
		fprintf(stderr, "AMD S2 AES-CTR-SHA: data mismatch\n");
	if (!ret)
		ret = -EIO;
out:
	free(key);
	free(state);
	free(src);
	free(cipher);
	free(out);
	return ret;
}

static int run_s2_xts_sha(sdxi_cxt_t *cxt, uint16_t op_type)
{
	struct sdxi_amd_s2_aes_key *key = NULL;
	struct sdxi_amd_s2_crypto_state *state = NULL;
	uint8_t *plain = NULL;
	uint8_t *cipher = NULL;
	uint8_t *out = NULL;
	int ret;

	posix_memalign_check((void **)&key, SDXI_AMD_S2_AES_KEY_ALIGN, sizeof(*key));
	posix_memalign_check((void **)&state, SDXI_AMD_S2_CRYPTO_STATE_ALIGN,
			     sizeof(*state));
	posix_memalign_check((void **)&plain, TEST_BUFFER_ALIGN, AES_XTS_TEST_BYTES);
	posix_memalign_check((void **)&cipher, TEST_BUFFER_ALIGN, AES_XTS_TEST_BYTES);
	posix_memalign_check((void **)&out, TEST_BUFFER_ALIGN, AES_XTS_TEST_BYTES);

	fill_xts_plaintext(plain);
	memset(cipher, 0, AES_XTS_TEST_BYTES);
	memset(out, 0, AES_XTS_TEST_BYTES);
	s2_xts_key_init(key);
	s2_xts_state_init(state);

	struct sdxi_amd_s2_aes_xts_sha_req req = {
		.size = sizeof(req),
		.op_type = op_type,
		.aes_op = SDXI_AMD_S2_AES_OP_ENCRYPT,
		.aes_key_size = SDXI_AMD_S2_AES_KEY_SIZE_128,
		.xts_block_size = SDXI_AMD_S2_XTS_BLOCK_SIZE_512B,
		.sha_mode = SDXI_AMD_S2_SHA_MODE_SHA2_256,
		.sha_src = SDXI_AMD_S2_SHA_SRC_EGRESS,
		.finalize = 1,
		.attr_src = 0,
		.attr_dst = 0,
		.akey_src = 0x1,
		.akey_dst = 0x1,
		.src = plain,
		.dst = cipher,
		.nbytes = AES_XTS_TEST_BYTES,
		.key = key,
		.state = state,
	};

	ret = sdxi_amd_s2_aes_xts_sha(cxt, &req, TEST_TIMEOUT_MS);
	if (ret)
		goto out_fail;
	if (memcmp(plain, cipher, AES_XTS_TEST_BYTES) == 0 ||
	    state->current_size == 0) {
		ret = -EIO;
		goto out_fail;
	}

	s2_xts_state_init(state);
	req.aes_op = SDXI_AMD_S2_AES_OP_DECRYPT;
	req.src = cipher;
	req.dst = out;
	ret = sdxi_amd_s2_aes_xts_sha(cxt, &req, TEST_TIMEOUT_MS);
	if (ret || memcmp(out, plain, AES_XTS_TEST_BYTES) != 0 ||
	    state->current_size == 0)
		goto out_fail;

	printf("AMD S2 AES-XTS-SHA: PASS\n");
	goto out;

out_fail:
	if (ret)
		fprintf(stderr, "AMD S2 AES-XTS-SHA: %s\n", sdxi_err_str(ret));
	else
		fprintf(stderr, "AMD S2 AES-XTS-SHA: data mismatch\n");
	if (!ret)
		ret = -EIO;
out:
	free(key);
	free(state);
	free(plain);
	free(cipher);
	free(out);
	return ret;
}

static int run_s2_gcm(sdxi_cxt_t *cxt, uint16_t op_type)
{
	struct sdxi_amd_s2_aes_key *key = NULL;
	struct sdxi_amd_s2_crypto_state *state = NULL;
	uint8_t *src = NULL;
	uint8_t *dst = NULL;
	uint8_t *aad = NULL;
	int ret;

	posix_memalign_check((void **)&key, SDXI_AMD_S2_AES_KEY_ALIGN, sizeof(*key));
	posix_memalign_check((void **)&state, SDXI_AMD_S2_CRYPTO_STATE_ALIGN, sizeof(*state));
	posix_memalign_check((void **)&src, TEST_BUFFER_ALIGN, AES_TEST_BYTES);
	posix_memalign_check((void **)&dst, TEST_BUFFER_ALIGN, AES_TEST_BYTES);
	posix_memalign_check((void **)&aad, TEST_BUFFER_ALIGN, GCM_AAD_BYTES);

	s2_key_init(key, gcm_key_128, sizeof(gcm_key_128));
	memcpy(aad, gcm_aad, GCM_AAD_BYTES);

	struct sdxi_amd_s2_aes_gcm_req aad_req = {
		.size = sizeof(aad_req),
		.op_type = op_type,
		.aes_key_size = SDXI_AMD_S2_AES_KEY_SIZE_128,
		.gcm_aad = 1,
		.gcm_last_aad = 1,
		.finalize = 0,
		.attr_src = 0,
		.attr_dst = 0,
		.akey_src = 0x1,
		.akey_dst = 0,
		.src = aad,
		.dst = NULL,
		.nbytes = GCM_AAD_BYTES,
		.key = key,
		.state = state,
	};
	struct sdxi_amd_s2_aes_gcm_req data_req = {
		.size = sizeof(data_req),
		.op_type = op_type,
		.aes_key_size = SDXI_AMD_S2_AES_KEY_SIZE_128,
		.gcm_aad = 0,
		.gcm_last_aad = 0,
		.finalize = 1,
		.attr_src = 0,
		.attr_dst = 0,
		.akey_src = 0x1,
		.akey_dst = 0x1,
		.src = src,
		.dst = dst,
		.nbytes = AES_TEST_BYTES,
		.key = key,
		.state = state,
	};

	memcpy(src, gcm_plaintext, AES_TEST_BYTES);
	memset(dst, 0, AES_TEST_BYTES);
	s2_gcm_state_init(state);
	aad_req.aes_op = SDXI_AMD_S2_AES_OP_ENCRYPT;
	data_req.aes_op = SDXI_AMD_S2_AES_OP_ENCRYPT;

	ret = sdxi_amd_s2_aes_gcm(cxt, &aad_req, TEST_TIMEOUT_MS);
	if (ret)
		goto out_fail;
	ret = sdxi_amd_s2_aes_gcm(cxt, &data_req, TEST_TIMEOUT_MS);
	if (ret || memcmp(dst, gcm_ciphertext, AES_TEST_BYTES) != 0 ||
	    memcmp(state->hash_data, gcm_tag, sizeof(gcm_tag)) != 0)
		goto out_fail;

	memcpy(src, gcm_ciphertext, AES_TEST_BYTES);
	memset(dst, 0, AES_TEST_BYTES);
	s2_gcm_state_init(state);
	aad_req.aes_op = SDXI_AMD_S2_AES_OP_DECRYPT;
	data_req.aes_op = SDXI_AMD_S2_AES_OP_DECRYPT;

	ret = sdxi_amd_s2_aes_gcm(cxt, &aad_req, TEST_TIMEOUT_MS);
	if (ret)
		goto out_fail;
	ret = sdxi_amd_s2_aes_gcm(cxt, &data_req, TEST_TIMEOUT_MS);
	if (ret || memcmp(dst, gcm_plaintext, AES_TEST_BYTES) != 0 ||
	    memcmp(state->hash_data, gcm_tag, sizeof(gcm_tag)) != 0)
		goto out_fail;

	printf("AMD S2 AES-GCM: PASS\n");
	goto out;

out_fail:
	if (ret)
		fprintf(stderr, "AMD S2 AES-GCM: %s\n", sdxi_err_str(ret));
	else
		fprintf(stderr, "AMD S2 AES-GCM: data mismatch\n");
	if (!ret)
		ret = -EIO;
out:
	free(key);
	free(state);
	free(src);
	free(dst);
	free(aad);
	return ret;
}

int main(int argc, char *argv[])
{
	int ret = 1;
	int failures = 0;
	struct amd_aes_args args;
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
		print_amd_aes_usage(argv[0]);
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

	if (!has_s1 && !has_s2) {
		printf("SKIP: no AMD crypto AES OpGroup present on this device\n");
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

	if (has_s1) {
		if (run_s1_xts(cxt, s1_op_type))
			failures++;
		if (run_s1_ecb(cxt, s1_op_type))
			failures++;
	}

	if (has_s2) {
		if (run_s2_xts_c(cxt, s2_op_type))
			failures++;
		if (run_s2_ecb_c(cxt, s2_op_type))
			failures++;
		if (run_s2_ctr(cxt, s2_op_type))
			failures++;
		if (run_s2_ctr_sha(cxt, s2_op_type))
			failures++;
		if (run_s2_xts_sha(cxt, s2_op_type))
			failures++;
		if (run_s2_gcm(cxt, s2_op_type))
			failures++;
	}

	if (failures) {
		fprintf(stderr, "AMD AES: FAIL (%d failing tests)\n", failures);
		ret = 1;
	} else {
		printf("AMD AES: PASS\n");
		ret = 0;
	}

out:
	(void)sdxi_cxt_destroy(cxt);
	sdxi_dev_close(dev);
	sdxi_free_dev_list(dev_list, dev_count);
	return ret ? 1 : 0;
}
