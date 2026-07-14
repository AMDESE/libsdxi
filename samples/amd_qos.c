// SPDX-License-Identifier: BSD-3-Clause
/*
 * SDXI library sample code - AMD QoS capability (ASQOSE / DSC_QOS_CAP)
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "libsdxi.h"
#include <sdxi/vendor/amd_qos.h>
#include "samples.h"

static const char *ts_units_name(uint8_t units)
{
	switch (units) {
	case SDXI_AMD_QOS_TS_UNITS_PS:
		return "ps";
	case SDXI_AMD_QOS_TS_UNITS_NS:
		return "ns";
	case SDXI_AMD_QOS_TS_UNITS_US:
		return "us";
	case SDXI_AMD_QOS_TS_UNITS_MS:
		return "ms";
	default:
		return "unknown";
	}
}

static void print_feature_flags(uint16_t flags)
{
	printf("    feature_flags=0x%04x\n", flags);
	printf("        dynamic_pool_reconfig: %s\n",
	       (flags & SDXI_AMD_QOS_FEATURE_DYNAMIC_POOL_RECONFIG) ? "yes" : "no");
	printf("        dynamic_pool_reassign: %s\n",
	       (flags & SDXI_AMD_QOS_FEATURE_DYNAMIC_POOL_REASSIGN) ? "yes" : "no");
	printf("        telemetry_timestamps:  %s\n",
	       (flags & SDXI_AMD_QOS_FEATURE_TEL_TIMESTAMPS) ? "yes" : "no");
	printf("        flexible_pool_scope:   %s\n",
	       (flags & SDXI_AMD_QOS_FEATURE_FLEXIBLE_POOL_SCOPE) ? "yes" : "no");
}

static int discover_amd_qos(sdxi_dev_t *dev, uint32_t timeout_ms,
			    uint16_t *op_type_out)
{
	int ret;

	ret = sdxi_dev_get_op_type(dev, SDXI_GRP_ASQOSE, timeout_ms, op_type_out);
	if (ret == -ENODEV || ret == -ENOTSUP)
		return 1;
	return ret;
}

int main(int argc, char *argv[])
{
	int ret = 1;
	sdxi_dev_t *dev = NULL;
	struct sample_args args;
	char **dev_list = NULL;
	size_t dev_count = 0;
	const char *path;
	struct sdxi_amd_qos_cap cap;
	uint16_t qos_op_type = 0;

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

	ret = discover_amd_qos(dev, 5000, &qos_op_type);
	if (ret == 1) {
		printf("AMD QoS capability missing on this device: "
		       "DSC_DISC does not advertise the ASQOSE GUID.\n");
		ret = 0;
		goto out;
	}
	if (ret) {
		fprintf(stderr, "sdxi_dev_get_op_type(ASQOSE): %s\n",
			sdxi_err_str(ret));
		goto out;
	}

	printf("DSC_DISC reports AMD QoS (ASQOSE) at type 0x%03x\n", qos_op_type);

	ret = sdxi_amd_qos_get_cap(dev, 5000, &cap, NULL);
	if (ret) {
		fprintf(stderr, "sdxi_amd_qos_get_cap: %s\n", sdxi_err_str(ret));
		if (ret == -ENOTSUP) {
			fprintf(stderr,
				"Note: this device advertises the ASQOSE GUID in discovery, "
				"but SDXI_ADMIN_VENDOR returned -ENOTSUP.\n");
			fprintf(stderr,
				"      The typed AMD QoS vendor handler is not enabled on this stack yet.\n");
		} else if (ret == -EPERM || ret == -EACCES) {
			fprintf(stderr,
				"Note: AMD QoS is PF-only and typically requires CAP_SYS_ADMIN.\n");
		}
		goto out;
	}

	printf("    qos_version=0x%04x\n", cap.qos_version);
	print_feature_flags(cap.feature_flags);
	printf("    device_maxbw_cap=%u MB/s\n", cap.device_maxbw_cap_mb_s);
	printf("    max_pools=%u\n", cap.max_pools);
	printf("    granularity_bw=%u MB/s\n", cap.granularity_bw_mb_s);
	printf("    variance_bw=%u MB/s\n", cap.variance_bw_mb_s);
	printf("    timestamp_units=%u (%s)\n", cap.timestamp_units,
	       ts_units_name(cap.timestamp_units));
	printf("    timestamp_resolution=%u\n", cap.timestamp_resolution);

	ret = 0;

out:
	sdxi_dev_close(dev);
	sdxi_free_dev_list(dev_list, dev_count);
	return ret;
}
