// SPDX-License-Identifier: BSD-3-Clause
/*
 * SDXI library sample code - OpGroup discovery dump (op_grp_cap + DSC_DISC)
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libsdxi.h"
#include "samples.h"

static int guid_is_zero(const uint8_t guid[16])
{
	for (size_t i = 0; i < 16; i++) {
		if (guid[i] != 0)
			return 0;
	}
	return 1;
}

static void guid_sdxi_to_str(const uint8_t guid[16], char out[37])
{
	static const char hex[] = "0123456789abcdef";
	uint8_t b[16];
	size_t o = 0;

	/* Convert from SDXI DISC byte order to canonical UUID string. */
	b[0] = guid[3];
	b[1] = guid[2];
	b[2] = guid[1];
	b[3] = guid[0];
	b[4] = guid[5];
	b[5] = guid[4];
	b[6] = guid[7];
	b[7] = guid[6];
	memcpy(b + 8, guid + 8, 8);

	for (size_t i = 0; i < 16; i++) {
		if (i == 4 || i == 6 || i == 8 || i == 10)
			out[o++] = '-';
		out[o++] = hex[(b[i] >> 4) & 0xF];
		out[o++] = hex[b[i] & 0xF];
	}
	out[o] = '\0';
}

int main(int argc, char *argv[])
{
	int ret = 1;
	sdxi_dev_t *dev = NULL;
	struct sample_args args;
	char **dev_list = NULL;
	size_t dev_count = 0;
	const char *path;
	struct sdxi_opgroup_info *ops = NULL;
	size_t nops = 0;

	printf("SDXI OpGroup dump ...\n");

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

	struct sdxi_dev_caps caps;
	ret = sdxi_dev_get_caps(dev, &caps);
	if (ret) {
		fprintf(stderr, "sdxi_dev_get_caps: %s\n", sdxi_err_str(ret));
		goto out;
	}

	printf("version=%u.%u op_group_caps=0x%08x\n",
	       caps.ver_major, caps.ver_minor, caps.op_group_caps);

	/*
	 * The opb_000_avl bitmask is no longer a per-attribute UAPI selector;
	 * print the op_grp_cap value already decoded into caps.op_group_caps
	 * so the dump still shows what's available before discovery runs.
	 */
	ret = sdxi_dev_opgroups_refresh(dev, 5000);
	if (ret) {
		fprintf(stderr, "sdxi_dev_opgroups_refresh (DSC_DISC): %s\n",
			sdxi_err_str(ret));
	}

	ret = sdxi_dev_get_opgroups(dev, &ops, &nops);
	if (ret) {
		fprintf(stderr, "sdxi_dev_get_opgroups: %s\n", sdxi_err_str(ret));
		goto out;
	}

	printf("opgroups=%zu\n", nops);
	for (size_t i = 0; i < nops; i++) {
		const struct sdxi_opgroup_info *ent = &ops[i];
		char guid_str[37] = "-";

		if (!guid_is_zero(ent->guid))
			guid_sdxi_to_str(ent->guid, guid_str);

		printf("  TYPE=%04u GRP=%-8s GUID=%s\n", ent->op_type,
		       sdxi_grp_name((enum sdxi_grp_id)ent->grp_id), guid_str);
	}

	ret = 0;

out:
	sdxi_free_opgroups(ops);
	sdxi_dev_close(dev);
	sdxi_free_dev_list(dev_list, dev_count);
	return ret ? 1 : 0;
}
