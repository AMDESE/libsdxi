// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2026 Advanced Micro Devices, Inc.
 *
 * SDXI library sample code - trigger and observe a fatal context error
 *
 * This sample submits malformed descriptors and then observes the driver's
 * context-kill path. "Kill" here means the SDXI driver marks this userspace
 * context fatal, wakes waiters polling the context fd, blocks further context
 * ioctls, and schedules deferred cleanup. It does not mean the process is
 * expected to receive SIGKILL.
 */
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libsdxi.h"
#include "samples.h"

/* Internal descriptor layout used only to manufacture a malformed descriptor. */
#include "../src/sdxi_dp.h"

#define CXT_ERR_BAD_DESCS 2u
#define CXT_ERR_TIMEOUT_MS 5000u
#define CXT_ERR_KILL_TIMEOUT_MS 10000u

#define CXT_ERR_STATE_UNKNOWN 0xffu
#define CXT_ERR_STATE_STOP_FN 0x4u
#define CXT_ERR_STATE_STOPG_FN 0x6u

struct cxt_err_observation {
	uint8_t state;
	uint64_t read_index;
	short revents;
};

static const char *cxt_err_state_str(uint8_t state)
{
	switch (state) {
	case SDXI_CXT_STATE_STOPPED:
		return "stopped";
	case SDXI_CXT_STATE_RUNNING:
		return "running";
	case SDXI_CXT_STATE_STOPPING:
		return "stopping";
	case CXT_ERR_STATE_STOP_FN:
		return "stopped-by-function";
	case CXT_ERR_STATE_STOPG_FN:
		return "stopping-by-function";
	case SDXI_CXT_STATE_ERR:
		return "error";
	case CXT_ERR_STATE_UNKNOWN:
		return "unknown";
	default:
		return "unrecognized";
	}
}

static void cxt_err_read_status(sdxi_cxt_t *cxt, uint8_t *state_out,
				uint64_t *read_index_out)
{
	struct sdxi_cxt_mmaps mmaps;
	const struct sdxi_cxt_sts *sts;

	if (state_out)
		*state_out = CXT_ERR_STATE_UNKNOWN;
	if (read_index_out)
		*read_index_out = 0;

	if (!cxt || sdxi_cxt_get_mmaps(cxt, &mmaps) || !mmaps.cxt_sts)
		return;

	sts = mmaps.cxt_sts;
	if (state_out)
		*state_out = __atomic_load_n(&sts->state, __ATOMIC_ACQUIRE) & 0xfu;
	if (read_index_out)
		*read_index_out = __atomic_load_n(&sts->read_index, __ATOMIC_ACQUIRE);
}

static void cxt_err_build_bad_desc(struct sdxi_dp_desc *out, uint32_t marker)
{
	struct sdxi_desc desc;

	memset(&desc, 0, sizeof(desc));

	/*
	 * type == 0 is not a valid SDXI operation group. Set valid+final and
	 * also poison reserved header/body bits so this cannot be interpreted
	 * as a valid DMA operation by a permissive implementation.
	 */
	desc.vl = 1;
	desc.fe = 1;
	desc.rsvd2 = 0x1f;
	desc.body.raw[0] = 0x43585445u; /* "CXTE" */
	desc.body.raw[1] = marker;
	desc.comp_ptr = 1;

	memcpy(out->bytes, &desc, sizeof(desc));
}

static int cxt_err_submit_bad_descs(sdxi_cxt_t *cxt, uint64_t *target_out)
{
	struct sdxi_dp_reserve_args reserve_args;
	struct sdxi_dp_reservation resv;
	uint64_t write_idx;
	int ret;

	memset(&reserve_args, 0, sizeof(reserve_args));
	reserve_args.size = sizeof(reserve_args);
	reserve_args.ndesc = CXT_ERR_BAD_DESCS;
	reserve_args.timeout_ms = CXT_ERR_TIMEOUT_MS;

	ret = sdxi_dp_reserve(cxt, &reserve_args, &resv);
	if (ret)
		return ret;

	for (uint32_t i = 0; i < CXT_ERR_BAD_DESCS; i++) {
		struct sdxi_dp_desc bad;

		cxt_err_build_bad_desc(&bad, i + 1);
		ret = sdxi_dp_desc_store(cxt, resv.start + i, &bad);
		if (ret)
			return ret;
	}

	ret = sdxi_dp_commit(cxt, &resv, CXT_ERR_TIMEOUT_MS, &write_idx);
	if (ret)
		return ret;

	if (target_out)
		*target_out = write_idx;
	return 0;
}

static int cxt_err_probe_context(sdxi_cxt_t *cxt)
{
	struct sdxi_key_ent ent;

	memset(&ent, 0, sizeof(ent));
	return sdxi_cxt_get_akey(cxt, 1, &ent);
}

static int cxt_err_wait_context_killed(sdxi_cxt_t *cxt,
				       struct cxt_err_observation *obs)
{
	struct pollfd pfd;
	int fd;
	int ret;

	memset(obs, 0, sizeof(*obs));
	obs->state = CXT_ERR_STATE_UNKNOWN;

	fd = sdxi_cxt_get_fd(cxt);
	if (fd < 0)
		return fd;

	pfd.fd = fd;
	pfd.events = POLLERR | POLLHUP;
	pfd.revents = 0;

	ret = poll(&pfd, 1, CXT_ERR_KILL_TIMEOUT_MS);
	if (ret < 0)
		return -errno;

	obs->revents = pfd.revents;
	cxt_err_read_status(cxt, &obs->state, &obs->read_index);

	if (ret == 0)
		return -ETIMEDOUT;
	if (pfd.revents & POLLNVAL)
		return -EBADF;
	if (pfd.revents & (POLLERR | POLLHUP))
		return 0;

	return -EIO;
}

static int cxt_err_open_device(const struct sample_args *args, sdxi_dev_t **dev_out,
			       char ***dev_list_out, size_t *dev_count_out)
{
	const char *path = args->dev_path;
	int ret;

	*dev_out = NULL;
	*dev_list_out = NULL;
	*dev_count_out = 0;

	if (!path) {
		ret = sdxi_list_devs(dev_list_out, dev_count_out);
		if (ret) {
			fprintf(stderr, "sdxi_list_devs: %s\n", sdxi_err_str(ret));
			return ret;
		}
		if (*dev_count_out == 0) {
			fprintf(stderr, "No SDXI devices found under /dev\n");
			return -ENODEV;
		}

		path = (*dev_list_out)[0];
		printf("Using device: %s (first)\n", path);
	} else {
		printf("Using device: %s\n", path);
	}

	ret = sdxi_dev_open(path, dev_out);
	if (ret)
		fprintf(stderr, "sdxi_dev_open: %s\n", sdxi_err_str(ret));

	return ret;
}

static int cxt_err_create_context(sdxi_dev_t *dev, sdxi_cxt_t **cxt_out)
{
	struct sdxi_cxt_create_opts opts = {
		.size = sizeof(opts),
		.flags = 0,
		.ds_ring_entries = 0,
		.opb_000_enb = 0,
		.eventfd = -1,
	};
	uint32_t pasid = 0;
	int ret;

	*cxt_out = NULL;

	ret = sdxi_cxt_create(dev, &opts, cxt_out);
	if (ret) {
		fprintf(stderr, "sdxi_cxt_create: %s\n", sdxi_err_str(ret));
		return ret;
	}

	pasid = sdxi_cxt_pasid(*cxt_out);

	printf("cxt_fd=%d pasid=%u\n", sdxi_cxt_get_fd(*cxt_out), pasid);
	return 0;
}

static int cxt_err_confirm_context_killed(sdxi_cxt_t *cxt)
{
	int ret = cxt_err_probe_context(cxt);

	if (ret == -ENODEV || ret == -EIO) {
		printf("context-fd rejected after poll wake: %s\n",
		       sdxi_err_str(ret));
		return 0;
	}

	if (ret) {
		fprintf(stderr, "post-poll context probe failed unexpectedly: %s\n",
			sdxi_err_str(ret));
		return ret;
	}

	fprintf(stderr, "context fd poll woke but context ioctl still succeeded\n");
	return -EIO;
}

static int cxt_err_run(sdxi_cxt_t *cxt)
{
	struct cxt_err_observation obs;
	uint64_t target = 0;
	int ret;

	printf("Injecting context error (%u bad descriptors)\n", CXT_ERR_BAD_DESCS);

	ret = cxt_err_probe_context(cxt);
	if (ret) {
		fprintf(stderr, "context probe before injection: %s\n", sdxi_err_str(ret));
		return ret;
	}

	ret = cxt_err_submit_bad_descs(cxt, &target);
	if (ret) {
		fprintf(stderr, "submit bad descriptors: %s\n", sdxi_err_str(ret));
		return ret;
	}

	printf("Waiting for context kill (write_index=%llu)\n",
	       (unsigned long long)target);

	ret = cxt_err_wait_context_killed(cxt, &obs);
	if (ret == -ETIMEDOUT) {
		fprintf(stderr,
			"timeout: state=%s (0x%x), read_index=%llu/%llu\n",
			cxt_err_state_str(obs.state), obs.state,
			(unsigned long long)obs.read_index,
			(unsigned long long)target);
		return ret;
	}
	if (ret) {
		fprintf(stderr, "poll context kill: %s\n", sdxi_err_str(ret));
		return ret;
	}

	printf("poll: revents=0x%x%s%s\n", (unsigned int)obs.revents,
	       (obs.revents & POLLERR) ? " POLLERR" : "",
	       (obs.revents & POLLHUP) ? " POLLHUP" : "");
	printf("state: %s (0x%x), read_index=%llu/%llu\n",
	       cxt_err_state_str(obs.state), obs.state,
	       (unsigned long long)obs.read_index,
	       (unsigned long long)target);

	ret = cxt_err_confirm_context_killed(cxt);
	if (ret)
		return ret;

	printf("PASS\n");
	return 0;
}

int main(int argc, char *argv[])
{
	sdxi_dev_t *dev = NULL;
	sdxi_cxt_t *cxt = NULL;
	struct sample_args args;
	char **dev_list = NULL;
	size_t dev_count = 0;
	int ret;

	printf("SDXI context error injection test ...\n");

	if (parse_sample_args(argc, argv, &args) < 0)
		return 1;

	ret = cxt_err_open_device(&args, &dev, &dev_list, &dev_count);
	if (ret)
		goto out;

	ret = cxt_err_create_context(dev, &cxt);
	if (ret)
		goto out;

	ret = cxt_err_run(cxt);

out:
	(void)sdxi_cxt_destroy(cxt);
	sdxi_dev_close(dev);
	sdxi_free_dev_list(dev_list, dev_count);
	return ret ? 1 : 0;
}
