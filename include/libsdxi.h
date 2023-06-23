// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2022 AMD, Inc. - All Rights Reserved
 *
 * Wei Huang <wei.huang2@amd.com>
 *
 */

#ifndef __LIBSDXI_H
#define __LIBSDXI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

/* common */
#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

#ifndef __aligned
#define __aligned(x) __attribute__((aligned(x)))
#endif

typedef enum {
	SDXI_OK = 0,
	SDXI_INVAL,
	SDXI_NO_MEM,
	SDXI_NO_DEV,
	SDXI_NO_CTXT,
	SDXI_STATUS_COUNT,
	SDXI_UNKNOWN = 666,
} sdxi_status;

static const char * sdxi_status_strings[] = {
	[SDXI_OK] 	= "SDXI STATUS: OK",
	[SDXI_INVAL] 	= "SDXI STATUS: INVALID",
	[SDXI_NO_MEM] 	= "SDXI STATUS: NO MEMORY",
	[SDXI_NO_DEV] 	= "SDXI STATUS: NO DEVICE",
	[SDXI_NO_CTXT] 	= "SDXI STATUS: NO CONTEXT",
	[SDXI_UNKNOWN] 	= "SDXI STATUS: UNKNOWN",
};

static inline const char * sdxi_status_str(sdxi_status status) {
	if (status < 0 || status >= SDXI_STATUS_COUNT)
		status = SDXI_UNKNOWN;

	return sdxi_status_strings[status];
}


static inline void sdxi_status_ok_or_die(sdxi_status status, const char * msg) {
	if (status != SDXI_OK) {
		fprintf(stderr, "%s\n\t%s\n", msg, sdxi_status_str(status));
		exit(1);
	}
}

typedef void (*sdxi_cb_fn) (
	void *callback_tag,
	sdxi_status status
);

struct sdxi_addr {
	uint16_t func;
	uint32_t pasid;
	uint64_t addr;
};
typedef struct sdxi_addr sdxi_addr_t;
typedef sdxi_addr_t *sdxi_addr_h;

struct sdxi_ctxt {
	int sdxi_fd;

	/* properties */
	uint32_t ctxt_type;
	uint32_t ring_entries;
	uint32_t id;
	uint32_t pasid;

	/* context data structures */
	struct ctxt_status *status;
	struct sdxi_desc *desc_ring;
	uint64_t *write_idx;
	uint64_t *doorbell;
};
typedef struct sdxi_ctxt sdxi_ctxt_t;
typedef sdxi_ctxt_t *sdxi_ctxt_h;

/* Context API functions */
sdxi_status sdxi_ctxt_create(const char *name, sdxi_ctxt_h *ctxt);
void sdxi_ctxt_close(sdxi_ctxt_h ctxt);

/* DMA base group related API functions */
sdxi_status sdxi_nop(sdxi_ctxt_h ctxt);
sdxi_status sdxi_mem_copy(sdxi_ctxt_h ctxt, void *dest, const void *src,
			  size_t size);
sdxi_status sdxi_mem_rep_copy(sdxi_ctxt_h ctxt, void *dest, const void *src,
			      size_t size, size_t repeat);
sdxi_status sdxi_write_imm(sdxi_ctxt_h ctxt, void *dest, const void *data,
			   size_t size);

/* Atomic group related functions */
sdxi_status sdxi_atomic_uadd(sdxi_ctxt_h ctxt, void *addr, uint64_t operand);

#ifdef __cplusplus
}
#endif

#endif /* __LIBSDXI_H */
