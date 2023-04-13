// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2022 AMD, Inc. - All Rights Reserved
 *
 * Wei Huang <wei.huang2@amd.com>
 */

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "libsdxi.h"
#include "syscall.h"
#include "sdxi.h"

/* RING OPERATIONS */
#define SDXI_DESCR_SIZE     64
#define SDXI_DS_NUM_QW      (SDXI_DESCR_SIZE / sizeof(uint64_t))
#define _ALIGN64            __attribute__ ((aligned (64)))
#define SDXI_MULTI_PRODUCER 1  //0 if single-producer

static int update_ring(uint64_t * const enq_entries,
		       uint64_t enq_num,
		       uint64_t * ring_base,
		       uint64_t ring_size,
		       uint64_t index)
{
	uint64_t skip = 1;

	for (uint64_t i = 0; i < enq_num; i++){
		uint64_t * ringp = ring_base + ((index + i) % ring_size) * SDXI_DS_NUM_QW;
		uint64_t * entryp = enq_entries + (i * SDXI_DS_NUM_QW);

		for (uint64_t j = skip; j < SDXI_DS_NUM_QW; j++ ){
			*(ringp + j) = *(entryp + j);
		}
		skip = 0;
	}

	__atomic_thread_fence(__ATOMIC_ACQ_REL);
	*(ring_base + (index % ring_size) * SDXI_DS_NUM_QW) = *enq_entries;

	return 0;
}

/**
 * @enq_entries: Ptr to entries to enqueue
 * @enq_num: Num of entries to enqueue
 * @ring_base: Ptr to ring location
 * @ring_size: Ring size in bytes / 64
 * @read_index: Ptr to read_index location
 * @write_index: Ptr to write_index location
 * @doorbell: Ptr to doorbell location
 *
 * Submit a list of descriptors to SDXI context desciptor ring. Return 0 if success. Otherwise
 * return -1;
 */
static int sdxi_enqueue(uint64_t *enq_entries,
			uint64_t enq_num,
			uint64_t *ring_base,
			uint64_t ring_size,
			uint64_t *read_index,
			uint64_t *write_index,
			uint64_t *doorbell)
{
	uint64_t read_idx, old_write_idx, new_idx;

	while (true) {
		/* get read_index before write_index to guarantee consistent values */
		read_idx = *read_index;
		__atomic_thread_fence(__ATOMIC_ACQ_REL);
		old_write_idx = *write_index;

		if (read_idx > old_write_idx) {
			/* only happens if Write_Index wraps or ring has bad setup */
			return SDXI_INVAL;
		}

		new_idx = old_write_idx + enq_num;
		if (new_idx - read_idx > ring_size) {
			/* not enough free entries, retry */
			continue;
		}

		if (SDXI_MULTI_PRODUCER) {
			/* Try to atomically update Write_Index before other threads with
			 * compare_exchange operation. This built-in also ensures the
			 * required single-threaded guarantees wrt the update of Write_Index.
			 */
			bool success = __atomic_compare_exchange_n(write_index,
								   &old_write_idx,
								   new_idx, false,
								   __ATOMIC_SEQ_CST,
								   __ATOMIC_SEQ_CST);
			if (success)
				break;
		} else {
			/* single-producer case */
			*write_index = new_idx;
			__atomic_thread_fence(__ATOMIC_ACQ_REL);
			break;
		}
	}

	/* write_index is now advanced, write out entries to the ring */
	update_ring(enq_entries, enq_num, ring_base, ring_size, old_write_idx);

	/* ring the doorbell */
	*doorbell = new_idx;

	return SDXI_OK;
}

sdxi_status ctxt_submit_desc(sdxi_ctxt_h ctxt, struct sdxi_desc *desc)
{
	int ret;

	if (ctxt->status->state != CTXT_STATE_RUNNING) {
		return SDXI_INVAL;
	}

	ret = sdxi_enqueue((uint64_t *)desc, 1, (uint64_t *)ctxt->desc_ring,
			   ctxt->ring_entries, (uint64_t *)(&(ctxt->status->read_idx)),
			   (uint64_t *)ctxt->write_idx, (uint64_t *)ctxt->doorbell);

	return ret;
}

sdxi_status sdxi_ctxt_alloc(sdxi_ctxt_h *ctxt_out)
{
	sdxi_ctxt_t *ctxt = malloc(sizeof(sdxi_ctxt_t));

	if (!ctxt) {
		return SDXI_NO_MEM;
	}

	/* ctxt initialization */
	ctxt->sdxi_fd = -1;

	*ctxt_out = ctxt;

	return SDXI_OK;
}

#define ctxt_unmap(p, size) \
	do { if ((p) != MAP_FAILED) munmap((p), (size)); } while (0)

/* context management */
sdxi_status sdxi_ctxt_create(const char *name,
			     sdxi_ctxt_h *ctxt_out)
{
	struct sdxi_ioctl_create_ctxt_args ctxt_open;
	void *desc_ring, *ctxt_status, *write_index, *doorbell;
	int sdxi_fd = -1;
	sdxi_status err = SDXI_OK;

	/* alloc context struct */
	err = sdxi_ctxt_alloc(ctxt_out);
	if (err != SDXI_OK)
		return err;

	/* open SDXI device */
	sdxi_fd = open(SDXI_DEV_PATH, O_RDWR);
	if (sdxi_fd < 0) {
		err = SDXI_NO_DEV;
		goto err_no_dev;
	}

	/* specify context properties */
	ctxt_open.ctxt_type = SDXI_IOCTL_CTXT_TYPE_USER;
	ctxt_open.ring_entries = 1024;

	/* create a new context */
	if (ioctl(sdxi_fd, SDXI_IOCTL_CREATE_CTXT, &ctxt_open)) {
		err = SDXI_NO_CTXT;
		goto err_no_ctxt;
	}

	/* mmap */
	ctxt_status = mmap(NULL, 4096, PROT_READ, MAP_SHARED,
			   sdxi_fd, ctxt_open.ctxt_status_mmap_base);
	desc_ring = mmap(NULL, ctxt_open.ring_entries * 64, PROT_WRITE | PROT_READ,
			 MAP_SHARED, sdxi_fd, ctxt_open.desc_ring_mmap_base);
	write_index = mmap(NULL,4096, PROT_READ | PROT_WRITE, MAP_SHARED,
			   sdxi_fd, ctxt_open.write_index_mmap_base);
	doorbell = mmap(NULL, 8, PROT_READ | PROT_WRITE, MAP_SHARED,
			sdxi_fd, ctxt_open.doorbell_mmap_base);

	if (ctxt_status == MAP_FAILED || desc_ring == MAP_FAILED || write_index == MAP_FAILED
	    || doorbell == MAP_FAILED) {
		err = SDXI_NO_MEM;
		goto err_no_mmap;
	}

	/* store the properties before return */
	(*ctxt_out)->sdxi_fd = sdxi_fd;
	(*ctxt_out)->ctxt_type = ctxt_open.ctxt_type;
	(*ctxt_out)->ring_entries = ctxt_open.ring_entries;

	(*ctxt_out)->id = ctxt_open.ctxt_id;
	(*ctxt_out)->pasid = ctxt_open.pasid;
	(*ctxt_out)->status = (struct ctxt_status *)ctxt_status;
	(*ctxt_out)->desc_ring = (struct sdxi_desc *)desc_ring;
	(*ctxt_out)->write_idx = write_index;
	(*ctxt_out)->doorbell = doorbell;

	return err;
err_no_mmap:
	ctxt_unmap(ctxt_status, 4096);
	ctxt_unmap(desc_ring, ctxt_open.ring_entries * 64);
	ctxt_unmap(write_index, 4096);
	ctxt_unmap(doorbell, 8);
err_no_ctxt:
	close(sdxi_fd);
err_no_dev:
	free(*ctxt_out);

	return err;
}

void sdxi_ctxt_close(sdxi_ctxt_h ctxt)
{
	struct sdxi_ioctl_close_ctxt_args ctxt_close;

	if (!ctxt)
		return;

	/* unmap all memory region */
	ctxt_unmap(ctxt->status, 4096);
	ctxt_unmap(ctxt->desc_ring, ctxt->ring_entries * 64);
	ctxt_unmap(ctxt->write_idx, 4096);
	ctxt_unmap(ctxt->doorbell, 8);

	/* close context */
	ctxt_close.ctxt_id = ctxt->id;
	if (ioctl(ctxt->sdxi_fd, SDXI_IOCTL_CLOSE_CTXT, &ctxt_close)) {
		printf("failed to close context %d", ctxt_close.ctxt_id);
	}

	close(ctxt->sdxi_fd);
	free(ctxt);
}
