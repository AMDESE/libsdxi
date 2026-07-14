// SPDX-License-Identifier: BSD-3-Clause
#include "libsdxi.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

const char *sdxi_err_name(int err)
{
	if (err > 0)
		err = -err;

	switch (err) {
	case 0:
		return "OK";
	case -EINVAL:
		return "EINVAL";
	case -ENOMEM:
		return "ENOMEM";
	case -ENODEV:
		return "ENODEV";
	case -ENOENT:
		return "ENOENT";
	case -EACCES:
		return "EACCES";
	case -EPERM:
		return "EPERM";
	case -EBUSY:
		return "EBUSY";
	case -ENOSPC:
		return "ENOSPC";
	case -EIO:
		return "EIO";
	case -ENOTSUP:
		return "ENOTSUP";
	case -EOVERFLOW:
		return "EOVERFLOW";
	case -EPROTONOSUPPORT:
		return "EPROTONOSUPPORT";
	case -ETIMEDOUT:
		return "ETIMEDOUT";
	default:
		return "UNKNOWN";
	}
}

static const char *sdxi_strerror_r_(int err, char *buf, size_t buflen)
{
#if defined(_GNU_SOURCE)
	return strerror_r(err, buf, buflen);
#else
	if (strerror_r(err, buf, buflen) == 0)
		return buf;
	snprintf(buf, buflen, "errno %d", err);
	return buf;
#endif
}

const char *sdxi_err_str(int err)
{
	static __thread char buf[128];

	if (err > 0)
		err = -err;
	if (err == 0)
		return "Success";

	return sdxi_strerror_r_(-err, buf, sizeof(buf));
}

const char *sdxi_grp_name(enum sdxi_grp_id grp)
{
	switch (grp) {
	case SDXI_GRP_UNKNOWN:
		return "UNKNOWN";
	case SDXI_GRP_DMA:
		return "DMA";
	case SDXI_GRP_ADMIN:
		return "ADMIN";
	case SDXI_GRP_ATOMIC:
		return "ATOMIC";
	case SDXI_GRP_INTR:
		return "INTR";
	case SDXI_GRP_CRCM1:
		return "CRCM1";
	case SDXI_GRP_XDMA:
		return "XDMA";
	case SDXI_GRP_DCOPY:
		return "DCOPY";
	case SDXI_GRP_ASQOSE:
		return "ASQOSE";
	case SDXI_GRP_AMDCRYPTO_S1:
		return "AMDCRYPTO_S1";
	case SDXI_GRP_AMDCRYPTO_S2:
		return "AMDCRYPTO_S2";
	default:
		return "UNKNOWN";
	}
}

/*
 * Per-attribute device-attribute selectors (mirrors enum sdxi_dev_attr_id).
 *
 * Centralising this table here keeps the public API and the CLI/tools in
 * sync without each consumer re-encoding the selector list.
 */
struct sdxi_dev_attr_desc {
	enum sdxi_dev_attr_id attr;
	const char *name;
	bool writable;
};

static const struct sdxi_dev_attr_desc sdxi_dev_attr_table[] = {
	{ SDXI_DEV_ATTR_ID_GSR,         "GSR",         true  },
	{ SDXI_DEV_ATTR_ID_GRP_ID,      "GRP_ID",      true  },
	{ SDXI_DEV_ATTR_ID_MAX_BUFFER,  "MAX_BUFFER",  true  },
	{ SDXI_DEV_ATTR_ID_MAX_AKEY_SZ, "MAX_AKEY_SZ", true  },
	{ SDXI_DEV_ATTR_ID_MAX_RKEY_SZ, "MAX_RKEY_SZ", false },
	{ SDXI_DEV_ATTR_ID_MAX_CXT,     "MAX_CXT",     true  },
};

static const struct sdxi_dev_attr_desc *
sdxi_dev_attr_desc_lookup(enum sdxi_dev_attr_id attr)
{
	size_t i;

	for (i = 0; i < sizeof(sdxi_dev_attr_table) / sizeof(sdxi_dev_attr_table[0]); i++) {
		if (sdxi_dev_attr_table[i].attr == attr)
			return &sdxi_dev_attr_table[i];
	}
	return NULL;
}

const char *sdxi_dev_attr_name(enum sdxi_dev_attr_id attr)
{
	const struct sdxi_dev_attr_desc *d = sdxi_dev_attr_desc_lookup(attr);

	return d ? d->name : NULL;
}

int sdxi_dev_attr_by_name(const char *name, enum sdxi_dev_attr_id *attr_out)
{
	const char *prefix = "SDXI_DEV_ATTR_";
	const size_t prefix_len = sizeof("SDXI_DEV_ATTR_") - 1;
	size_t i;

	if (!name || !attr_out)
		return -EINVAL;

	if (strncasecmp(name, prefix, prefix_len) == 0)
		name += prefix_len;

	for (i = 0; i < sizeof(sdxi_dev_attr_table) / sizeof(sdxi_dev_attr_table[0]); i++) {
		if (strcasecmp(name, sdxi_dev_attr_table[i].name) == 0) {
			*attr_out = sdxi_dev_attr_table[i].attr;
			return 0;
		}
	}
	return -EINVAL;
}

bool sdxi_dev_attr_is_writable(enum sdxi_dev_attr_id attr)
{
	const struct sdxi_dev_attr_desc *d = sdxi_dev_attr_desc_lookup(attr);

	return d ? d->writable : false;
}

void sdxi_bitmap_free(struct sdxi_bitmap *bm)
{
	if (!bm)
		return;
	free(bm->data);
	bm->data = NULL;
	bm->bytes = 0;
	bm->nentries = 0;
	bm->nvalid = 0;
}
