// SPDX-License-Identifier: BSD-3-Clause
/*
 * SDXI library sample code - CRC Mode1 (CrcM1Grp)
 *
 * This sample discovers the function-assigned opcode `type` value for the
 * CRC Mode1 operation group via AdminGrp DSC_DISC and tests all CRC operation
 * subtypes:
 *   - CRC and Copy (DSC_CRC_COPY)
 *   - CRC Calculate only (DSC_CRC_CALC)
 *   - CRC and Copy Blocks (DSC_CRC_COPY_BLK)
 *   - CRC Calculate only Blocks (DSC_CRC_CALC_BLK)
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "libsdxi.h"
#include "samples.h"

#define TEST_BUFFER_ALIGN 4096
#define TEST_ENTRY_BYTES 4096

#define TEST_BLK_BYTES 512
#define TEST_BLK_COUNT 8
#define TEST_BLK_NBYTES (TEST_BLK_BYTES * TEST_BLK_COUNT)

/*
 * CrcM1Grp GUID: 15114db9-4a7e-47ab-97dd-ea86eda10b6e
 *
 * SDXI stores GUID records using little-endian for time_low/time_mid/
 * time_hi_and_version (RFC4122 4.1.2 with the first three fields LE).
 */
static const uint8_t guid_crcm1[16] = {
	0xb9, 0x4d, 0x11, 0x15, 0x7e, 0x4a, 0xab, 0x47,
	0x97, 0xdd, 0xea, 0x86, 0xed, 0xa1, 0x0b, 0x6e,
};

static uint16_t sdxi_le16_dec(const void *p)
{
	const uint8_t *b = p;

	return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static int sdxi_guid_eq(const uint8_t a[16], const uint8_t b[16])
{
	for (size_t i = 0; i < 16; i++) {
		if (a[i] != b[i])
			return 0;
	}
	return 1;
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

static int sdxi_find_op_type_by_guid(sdxi_dev_t *dev, const uint8_t guid[16],
				     uint16_t *type_out)
{
	void *disc0 = NULL;
	void *discs = NULL;
	uint16_t max_index;
	int ret;

	if (!dev || !guid || !type_out)
		return -EINVAL;

	*type_out = 0;

	posix_memalign_check(&disc0, SDXI_DISC_RECORD_BYTES, SDXI_DISC_RECORD_BYTES);
	memset(disc0, 0, SDXI_DISC_RECORD_BYTES);

	size_t disc0_bytes = SDXI_DISC_RECORD_BYTES;

	ret = sdxi_dev_admin_disc(dev, 0, 0, disc0, &disc0_bytes, 5000, NULL);
	if (ret)
		goto out;

	max_index = sdxi_le16_dec(disc0) & 0x7ff;
	if (max_index == 0) {
		ret = -ENODEV;
		goto out;
	}

	size_t discs_bytes = (size_t)max_index * SDXI_DISC_RECORD_BYTES;
	posix_memalign_check(&discs, SDXI_DISC_RECORD_BYTES, discs_bytes);
	memset(discs, 0, discs_bytes);

	ret = sdxi_dev_admin_disc(dev, 1, max_index, discs, &discs_bytes, 5000,
				  NULL);
	if (ret)
		goto out;

	for (uint16_t idx = 1; idx <= max_index; idx++) {
		const uint8_t *ent = (const uint8_t *)discs +
				     (size_t)(idx - 1) * SDXI_DISC_RECORD_BYTES;

		if (sdxi_guid_eq(ent, guid)) {
			*type_out = idx;
			ret = 0;
			goto out;
		}
	}

	ret = -ENODEV;

out:
	free(disc0);
	free(discs);
	return ret;
}

/*
 * Software CRC32 reference for the CrcM1Grp configuration used by this sample:
 *   - crcsel:  CRC32 0x04C11DB7
 *   - reflect: 11b (reflect data/seed/output)
 *   - invert:  10b (invert output)
 *   - seed:    0xFFFFFFFF
 *
 * This matches the common ZIP/PNG/ZLIB CRC32 definition.
 */
static uint32_t crc32_zip_ref(uint32_t seed, const uint8_t *buf, size_t len)
{
	uint32_t crc = seed;

	while (len--) {
		crc ^= *buf++;
		for (int i = 0; i < 8; i++) {
			uint32_t mask = 0u - (crc & 1u);
			crc = (crc >> 1) ^ (0xEDB88320u & mask);
		}
	}

	return crc ^ 0xFFFFFFFFu;
}

static int test_crcm1_copy(sdxi_cxt_t *cxt, uint16_t op_type, uint8_t *dst,
			   const uint8_t *src, size_t nbytes,
			   uint32_t *crc_out)
{
	struct sdxi_crc_m1_req req;
	uint32_t exp;
	int ret;

	memset(dst, 0x5a, nbytes);
	memset(crc_out, 0, sizeof(*crc_out));

	exp = crc32_zip_ref(0xFFFFFFFFu, src, nbytes);

	memset(&req, 0, sizeof(req));
	req.size = sizeof(req);
	req.op_type = op_type;
	req.sub_type = SDXI_CRC_M1_SUBTYPE_COPY;
	req.blksz = 0;
	req.seed_loc = SDXI_CRC_M1_SEEDLOC_COMMON;
	req.crcsel = SDXI_CRC_M1_CRCSEL_CRC32_04C11DB7;
	req.reflect = SDXI_CRC_M1_REFLECT_DATA_SEED_CRC;
	req.invert = SDXI_CRC_M1_INVERT_CRC;
	req.attr_src = 0x0;
	req.attr_dst = 0x0;
	req.akey_src = 0x1;
	req.akey_dst = 0x1;
	req.src = src;
	req.dst = dst;
	req.nbytes = nbytes;
	req.seed = 0xFFFFFFFFu;
	req.crc_dst = crc_out;
	req.crc_dst_bytes = sizeof(*crc_out);

	ret = sdxi_crc_m1(cxt, &req, 5000);
	if (ret)
		return ret;

	if (memcmp(src, dst, nbytes) != 0)
		return -EIO;
	if (*crc_out != exp)
		return -EIO;

	return 0;
}

static int test_crcm1_calc(sdxi_cxt_t *cxt, uint16_t op_type,
			   const uint8_t *src, size_t nbytes,
			   uint32_t *crc_out)
{
	struct sdxi_crc_m1_req req;
	uint32_t exp;
	int ret;

	memset(crc_out, 0, sizeof(*crc_out));
	exp = crc32_zip_ref(0xFFFFFFFFu, src, nbytes);

	memset(&req, 0, sizeof(req));
	req.size = sizeof(req);
	req.op_type = op_type;
	req.sub_type = SDXI_CRC_M1_SUBTYPE_CALC;
	req.blksz = 0;
	req.seed_loc = SDXI_CRC_M1_SEEDLOC_COMMON;
	req.crcsel = SDXI_CRC_M1_CRCSEL_CRC32_04C11DB7;
	req.reflect = SDXI_CRC_M1_REFLECT_DATA_SEED_CRC;
	req.invert = SDXI_CRC_M1_INVERT_CRC;
	req.attr_src = 0x0;
	req.attr_dst = 0x0;
	req.akey_src = 0x1;
	req.akey_dst = 0x1;
	req.src = src;
	req.dst = NULL;
	req.nbytes = nbytes;
	req.seed = 0xFFFFFFFFu;
	req.crc_dst = crc_out;
	req.crc_dst_bytes = sizeof(*crc_out);

	ret = sdxi_crc_m1(cxt, &req, 5000);
	if (ret)
		return ret;

	if (*crc_out != exp)
		return -EIO;

	return 0;
}

static int test_crcm1_copy_blk(sdxi_cxt_t *cxt, uint16_t op_type, uint8_t *dst,
			       const uint8_t *src, size_t nbytes,
			       uint32_t *crc_out, size_t crc_out_n)
{
	struct sdxi_crc_m1_req req;
	size_t nblocks;
	int ret;

	if (nbytes % TEST_BLK_BYTES)
		return -EINVAL;

	nblocks = nbytes / TEST_BLK_BYTES;
	if (crc_out_n < nblocks)
		return -EINVAL;

	memset(dst, 0x5a, nbytes);
	memset(crc_out, 0, crc_out_n * sizeof(crc_out[0]));

	memset(&req, 0, sizeof(req));
	req.size = sizeof(req);
	req.op_type = op_type;
	req.sub_type = SDXI_CRC_M1_SUBTYPE_COPY_BLK;
	req.blksz = SDXI_CRC_M1_BLKSZ_512B_NO_MD;
	req.meta_src = 0;
	req.meta_dst = 0;
	req.seed_loc = SDXI_CRC_M1_SEEDLOC_COMMON;
	req.crcsel = SDXI_CRC_M1_CRCSEL_CRC32_04C11DB7;
	req.reflect = SDXI_CRC_M1_REFLECT_DATA_SEED_CRC;
	req.invert = SDXI_CRC_M1_INVERT_CRC;
	req.attr_src = 0x0;
	req.attr_dst = 0x0;
	req.akey_src = 0x1;
	req.akey_dst = 0x1;
	req.src = src;
	req.dst = dst;
	req.nbytes = nbytes;
	req.seed = 0xFFFFFFFFu;
	req.crc_dst = crc_out;
	req.crc_dst_bytes = nblocks * sizeof(crc_out[0]);

	ret = sdxi_crc_m1(cxt, &req, 5000);
	if (ret)
		return ret;

	if (memcmp(src, dst, nbytes) != 0)
		return -EIO;

	for (size_t i = 0; i < nblocks; i++) {
		uint32_t exp = crc32_zip_ref(
			0xFFFFFFFFu, src + i * TEST_BLK_BYTES, TEST_BLK_BYTES);
		if (crc_out[i] != exp)
			return -EIO;
	}

	return 0;
}

static int test_crcm1_calc_blk(sdxi_cxt_t *cxt, uint16_t op_type,
			       const uint8_t *src, size_t nbytes,
			       uint32_t *crc_out, size_t crc_out_n)
{
	struct sdxi_crc_m1_req req;
	size_t nblocks;
	int ret;

	if (nbytes % TEST_BLK_BYTES)
		return -EINVAL;

	nblocks = nbytes / TEST_BLK_BYTES;
	if (crc_out_n < nblocks)
		return -EINVAL;

	memset(crc_out, 0, crc_out_n * sizeof(crc_out[0]));

	memset(&req, 0, sizeof(req));
	req.size = sizeof(req);
	req.op_type = op_type;
	req.sub_type = SDXI_CRC_M1_SUBTYPE_CALC_BLK;
	req.blksz = SDXI_CRC_M1_BLKSZ_512B_NO_MD;
	req.meta_src = 0;
	req.meta_dst = 0;
	req.seed_loc = SDXI_CRC_M1_SEEDLOC_COMMON;
	req.crcsel = SDXI_CRC_M1_CRCSEL_CRC32_04C11DB7;
	req.reflect = SDXI_CRC_M1_REFLECT_DATA_SEED_CRC;
	req.invert = SDXI_CRC_M1_INVERT_CRC;
	req.attr_src = 0x0;
	req.attr_dst = 0x0;
	req.akey_src = 0x1;
	req.akey_dst = 0x1;
	req.src = src;
	req.dst = NULL;
	req.nbytes = nbytes;
	req.seed = 0xFFFFFFFFu;
	req.crc_dst = crc_out;
	req.crc_dst_bytes = nblocks * sizeof(crc_out[0]);

	ret = sdxi_crc_m1(cxt, &req, 5000);
	if (ret)
		return ret;

	for (size_t i = 0; i < nblocks; i++) {
		uint32_t exp = crc32_zip_ref(
			0xFFFFFFFFu, src + i * TEST_BLK_BYTES, TEST_BLK_BYTES);
		if (crc_out[i] != exp)
			return -EIO;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int ret = 1;
	sdxi_dev_t *dev = NULL;
	sdxi_cxt_t *cxt = NULL;
	struct sample_args args;
	char **dev_list = NULL;
	size_t dev_count = 0;
	const char *path;
	uint8_t *src = NULL;
	uint8_t *dst = NULL;
	uint32_t *crc_out = NULL;
	uint16_t crcm1_type;
	int err;

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

	ret = sdxi_find_op_type_by_guid(dev, guid_crcm1, &crcm1_type);
	if (ret) {
		if (skip_capability_err(ret)) {
			if (ret == -EPERM || ret == -EACCES) {
				printf("SKIP: CRC Mode1 discovery needs DSC_DISC privileges (%s)\n",
				       sdxi_err_str(ret));
			} else if (ret == -ENODEV) {
				printf("SKIP: CRC Mode1 opgroup not present in DSC_DISC\n");
			} else {
				printf("SKIP: CRC Mode1 not supported on this device (%s)\n",
				       sdxi_err_str(ret));
			}
			ret = 0;
		} else {
			fprintf(stderr, "sdxi_dev_admin_disc: %s\n", sdxi_err_str(ret));
		}
		goto out;
	}
	printf("CRC Mode1 op_type discovered: 0x%03x\n", crcm1_type);

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

	posix_memalign_check((void **)&src, TEST_BUFFER_ALIGN, TEST_ENTRY_BYTES);
	posix_memalign_check((void **)&dst, TEST_BUFFER_ALIGN, TEST_ENTRY_BYTES);
	posix_memalign_check((void **)&crc_out, 64, TEST_BLK_COUNT * sizeof(crc_out[0]));

	for (size_t i = 0; i < TEST_ENTRY_BYTES; i++)
		src[i] = (uint8_t)i;

	printf("SDXI CRC Mode1 tests ...\n");

	err = test_crcm1_copy(cxt, crcm1_type, dst, src, TEST_ENTRY_BYTES,
			      &crc_out[0]);
	if (err) {
		fprintf(stderr, "DSC_CRC_COPY ==> FAIL (%s)\n", sdxi_err_str(err));
		ret = err;
		goto out;
	}
	printf("DSC_CRC_COPY ==> SUCCESS\n");

	err = test_crcm1_calc(cxt, crcm1_type, src, TEST_ENTRY_BYTES, &crc_out[0]);
	if (err) {
		fprintf(stderr, "DSC_CRC_CALC ==> FAIL (%s)\n", sdxi_err_str(err));
		ret = err;
		goto out;
	}
	printf("DSC_CRC_CALC ==> SUCCESS\n");

	err = test_crcm1_copy_blk(cxt, crcm1_type, dst, src, TEST_BLK_NBYTES,
				  crc_out, TEST_BLK_COUNT);
	if (err) {
		fprintf(stderr, "DSC_CRC_COPY_BLK ==> FAIL (%s)\n", sdxi_err_str(err));
		ret = err;
		goto out;
	}
	printf("DSC_CRC_COPY_BLK ==> SUCCESS\n");

	err = test_crcm1_calc_blk(cxt, crcm1_type, src, TEST_BLK_NBYTES, crc_out,
				  TEST_BLK_COUNT);
	if (err) {
		fprintf(stderr, "DSC_CRC_CALC_BLK ==> FAIL (%s)\n", sdxi_err_str(err));
		ret = err;
		goto out;
	}
	printf("DSC_CRC_CALC_BLK ==> SUCCESS\n");

	ret = 0;

out:
	(void)sdxi_cxt_destroy(cxt);
	sdxi_dev_close(dev);
	sdxi_free_dev_list(dev_list, dev_count);
	free(src);
	free(dst);
	free(crc_out);
	return ret;
}
