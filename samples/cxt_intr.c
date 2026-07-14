// SPDX-License-Identifier: BSD-3-Clause
/*
 * SDXI library sample code - context interrupt delivery via DSC_INTR + eventfd
 */
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "libsdxi.h"
#include "samples.h"

int main(int argc, char *argv[])
{
	int ret = 1;
	sdxi_dev_t *dev = NULL;
	sdxi_cxt_t *cxt = NULL;
	struct sample_args args;
	char **dev_list = NULL;
	size_t dev_count = 0;
	const char *path;
	int efd;
	uint16_t intr_akey;
	uint32_t intr_num;

	printf("SDXI DSC_INTR + eventfd test ...\n");

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
		.flags = SDXI_CXT_CREATE_F_EVENTFD,
		.ds_ring_entries = 0,
		.opb_000_enb = 0,
		.eventfd = -1,
	};

	ret = sdxi_cxt_create(dev, &opts, &cxt);
	if (ret) {
		fprintf(stderr, "sdxi_cxt_create: %s\n", sdxi_err_str(ret));
		goto out;
	}

	efd = sdxi_cxt_get_eventfd(cxt);
	if (efd < 0) {
		fprintf(stderr, "sdxi_cxt_get_eventfd: %s\n", sdxi_err_str(efd));
		goto out;
	}

	ret = sdxi_cxt_get_intr_info(cxt, &intr_akey, &intr_num);
	if (ret) {
		fprintf(stderr, "sdxi_cxt_get_intr_info: %s\n", sdxi_err_str(ret));
		goto out;
	}

	printf("cxt_fd=%d pasid=%u eventfd=%d intr_akey=0x%04x intr_num=%u\n",
	       sdxi_cxt_get_fd(cxt), sdxi_cxt_pasid(cxt), efd, intr_akey,
	       intr_num);

	printf("Submitting DSC_INTR ...\n");
	ret = sdxi_intr(cxt, 5000);
	if (ret) {
		fprintf(stderr, "sdxi_intr: %s\n", sdxi_err_str(ret));
		goto out;
	}

	struct pollfd pfd = {
		.fd = efd,
		.events = POLLIN,
	};

	ret = poll(&pfd, 1, 5000);
	if (ret < 0) {
		fprintf(stderr, "poll(eventfd): %s\n", strerror(errno));
		ret = 1;
		goto out;
	}
	if (ret == 0) {
		fprintf(stderr, "eventfd notification timeout\n");
		ret = 1;
		goto out;
	}
	if (!(pfd.revents & POLLIN)) {
		fprintf(stderr, "eventfd revents=0x%x\n", pfd.revents);
		ret = 1;
		goto out;
	}

	eventfd_t val = 0;
	if (eventfd_read(efd, &val) != 0) {
		fprintf(stderr, "eventfd_read: %s\n", strerror(errno));
		ret = 1;
		goto out;
	}
	if (!val) {
		fprintf(stderr, "eventfd returned 0\n");
		ret = 1;
		goto out;
	}

	printf("eventfd notified: value=%llu\n", (unsigned long long)val);
	printf("DSC_INTR ==> SUCCESS\n");
	ret = 0;

out:
	(void)sdxi_cxt_destroy(cxt);
	sdxi_dev_close(dev);
	sdxi_free_dev_list(dev_list, dev_count);
	return ret ? 1 : 0;
}
