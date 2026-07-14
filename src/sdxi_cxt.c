// SPDX-License-Identifier: BSD-3-Clause
#include "sdxi_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "sdxi_dp.h"

#define SDXI_CSB_FLAG_ER (1U << 31)

struct sdxi_csb {
	uint64_t signal;
	uint32_t flags;
	uint8_t rsvd0[20];
} __packed __aligned(32);

static size_t sdxi_csb_bytes(uint32_t entries)
{
	size_t bytes;

	if (entries == 0)
		return 0;

	bytes = (size_t)entries * sizeof(struct sdxi_csb);
	if (bytes / sizeof(struct sdxi_csb) != entries)
		return 0;

	return bytes;
}

static uint64_t sdxi_csb_signal(const struct sdxi_csb *csb)
{
	return __atomic_load_n(&csb->signal, __ATOMIC_ACQUIRE);
}

static uint32_t sdxi_csb_flags(const struct sdxi_csb *csb)
{
	return __atomic_load_n(&csb->flags, __ATOMIC_ACQUIRE);
}

static size_t sdxi_page_size(void)
{
	long v = sysconf(_SC_PAGESIZE);
	if (v <= 0)
		return 4096;
	return (size_t)v;
}

static void *sdxi_mmap_anon(size_t size)
{
	void *p;

	p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,
		 -1, 0);
	if (p == MAP_FAILED)
		return MAP_FAILED;

	if (mlock(p, size) == -1) {
		munmap(p, size);
		return MAP_FAILED;
	}

	return p;
}

static void sdxi_munmap_safe(void *p, size_t size)
{
	if (p && p != MAP_FAILED)
		(void)munmap(p, size);
}

static uint64_t sdxi_now_ns(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		return 0;
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void sdxi_relax(void)
{
	(void)sched_yield();
}

static uint8_t sdxi_cxt_state(const sdxi_cxt_t *cxt)
{
	const struct sdxi_cxt_sts *sts = cxt->mmaps.cxt_sts;

	if (!sts)
		return SDXI_CXT_STATE_ERR_FN;
	return sts->state & 0xF;
}

static int sdxi_cxt_state_check(const sdxi_cxt_t *cxt)
{
	switch (sdxi_cxt_state(cxt)) {
	case SDXI_CXT_STATE_RUN:
	case SDXI_CXT_STATE_STOPG_SW:
	case SDXI_CXT_STATE_STOPG_FN:
		return 0;
	case SDXI_CXT_STATE_STOP_SW:
	case SDXI_CXT_STATE_STOP_FN:
	case SDXI_CXT_STATE_ERR_FN:
	default:
		return -EIO;
	}
}

static uint64_t sdxi_cxt_read_idx(const sdxi_cxt_t *cxt)
{
	const struct sdxi_cxt_sts *sts = cxt->mmaps.cxt_sts;

	if (!sts)
		return 0;
	return __atomic_load_n(&sts->read_index, __ATOMIC_ACQUIRE);
}

static uint64_t *sdxi_cxt_write_idx_ptr(const sdxi_cxt_t *cxt)
{
	return (uint64_t *)cxt->mmaps.write_index;
}

static volatile uint64_t *sdxi_cxt_doorbell_ptr(const sdxi_cxt_t *cxt)
{
	return (volatile uint64_t *)cxt->mmaps.doorbell;
}

static int sdxi_cxt_wait_read_idx(sdxi_cxt_t *cxt, uint64_t target,
				  uint32_t timeout_ms)
{
	const uint64_t start_ns = sdxi_now_ns();
	const uint64_t deadline_ns =
		timeout_ms ? start_ns + (uint64_t)timeout_ms * 1000000ull : 0;

	for (;;) {
		if (sdxi_cxt_read_idx(cxt) >= target)
			return 0;
		if (sdxi_cxt_state_check(cxt))
			return -EIO;

		if (timeout_ms && sdxi_now_ns() >= deadline_ns)
			return -ETIMEDOUT;

		if (cxt->eventfd >= 0) {
			int timeout = -1;
			struct pollfd pfd = {
				.fd = cxt->eventfd,
				.events = POLLIN,
			};

			if (timeout_ms) {
				uint64_t now_ns = sdxi_now_ns();
				uint64_t remain_ns =
					(now_ns < deadline_ns) ? (deadline_ns - now_ns) : 0;
				uint64_t remain_ms = remain_ns / 1000000ull;
				timeout = (remain_ms > INT_MAX) ? INT_MAX
							       : (int)remain_ms;
			}

			(void)poll(&pfd, 1, timeout);
			if (pfd.revents & POLLIN) {
				uint64_t v;
				ssize_t n = read(cxt->eventfd, &v, sizeof(v));
				(void)n;
			}
			continue;
		}

		sdxi_relax();
	}
}

/*
 * Like sdxi_cxt_wait_{read_idx,csb}() but without draining the context eventfd.
 *
 * This is used by test/diagnostic flows (e.g. DSC_INTR) where the caller wants
 * to observe the eventfd notification directly.
 */
static int sdxi_cxt_wait_read_idx_nodrain(sdxi_cxt_t *cxt, uint64_t target,
					 uint32_t timeout_ms)
{
	const uint64_t start_ns = sdxi_now_ns();
	const uint64_t deadline_ns =
		timeout_ms ? start_ns + (uint64_t)timeout_ms * 1000000ull : 0;

	for (;;) {
		if (sdxi_cxt_read_idx(cxt) >= target)
			return 0;
		if (sdxi_cxt_state_check(cxt))
			return -EIO;

		if (timeout_ms && sdxi_now_ns() >= deadline_ns)
			return -ETIMEDOUT;

		sdxi_relax();
	}
}

static int sdxi_cxt_wait_csb(sdxi_cxt_t *cxt, const struct sdxi_csb *csb,
			     uint32_t timeout_ms)
{
	const uint64_t start_ns = sdxi_now_ns();
	const uint64_t deadline_ns =
		timeout_ms ? start_ns + (uint64_t)timeout_ms * 1000000ull : 0;

	if (!csb)
		return -EINVAL;

	for (;;) {
		uint32_t flags = sdxi_csb_flags(csb);

		if (flags & SDXI_CSB_FLAG_ER)
			return -EIO;
		if (sdxi_csb_signal(csb))
			return 0;
		if (sdxi_cxt_state_check(cxt))
			return -EIO;

		if (timeout_ms && sdxi_now_ns() >= deadline_ns)
			return -ETIMEDOUT;

		if (cxt->eventfd >= 0) {
			int timeout = -1;
			struct pollfd pfd = {
				.fd = cxt->eventfd,
				.events = POLLIN,
			};

			if (timeout_ms) {
				uint64_t now_ns = sdxi_now_ns();
				uint64_t remain_ns =
					(now_ns < deadline_ns) ? (deadline_ns - now_ns) : 0;
				uint64_t remain_ms = remain_ns / 1000000ull;
				timeout = (remain_ms > INT_MAX) ? INT_MAX : (int)remain_ms;
			}

			(void)poll(&pfd, 1, timeout);
			if (pfd.revents & POLLIN) {
				uint64_t v;
				ssize_t n = read(cxt->eventfd, &v, sizeof(v));
				(void)n;
			}
			continue;
		}

		sdxi_relax();
	}
}

static int sdxi_cxt_wait_csb_nodrain(sdxi_cxt_t *cxt, const struct sdxi_csb *csb,
				     uint32_t timeout_ms)
{
	const uint64_t start_ns = sdxi_now_ns();
	const uint64_t deadline_ns =
		timeout_ms ? start_ns + (uint64_t)timeout_ms * 1000000ull : 0;

	if (!csb)
		return -EINVAL;

	for (;;) {
		uint32_t flags = sdxi_csb_flags(csb);

		if (flags & SDXI_CSB_FLAG_ER)
			return -EIO;
		if (sdxi_csb_signal(csb))
			return 0;
		if (sdxi_cxt_state_check(cxt))
			return -EIO;

		if (timeout_ms && sdxi_now_ns() >= deadline_ns)
			return -ETIMEDOUT;

		sdxi_relax();
	}
}

static size_t sdxi_ring_bytes(uint32_t entries)
{
#if SIZE_MAX < (UINT64_C(0xffffffff) * SDXI_DESC_BYTES)
	if (entries > SIZE_MAX / (size_t)SDXI_DESC_BYTES)
		return 0;
#endif

	return (size_t)entries * (size_t)SDXI_DESC_BYTES;
}

static void sdxi_ring_copy_desc(void *dst_slot, const struct sdxi_desc *src)
{
	const uint8_t *s = (const uint8_t *)src;
	uint8_t *d = (uint8_t *)dst_slot;

	memcpy(d + 8, s + 8, SDXI_DESC_BYTES - 8);
	__atomic_thread_fence(__ATOMIC_RELEASE);
	memcpy(d, s, 8);
}

static void sdxi_desc_from_public(struct sdxi_desc *out,
				  const struct sdxi_dp_desc *in)
{
	memcpy(out, in->bytes, sizeof(*out));
}

static int sdxi_dp_flags_ok(uint32_t flags)
{
	if (flags & ~SDXI_DP_RESERVE_F_MULTI_PRODUCER)
		return -EINVAL;
	return 0;
}

static int sdxi_cxt_reserve(sdxi_cxt_t *cxt, uint32_t ndesc, uint32_t flags,
			    uint32_t timeout_ms, uint64_t *start_out)
{
	const uint64_t start_ns = sdxi_now_ns();
	const uint64_t deadline_ns =
		timeout_ms ? start_ns + (uint64_t)timeout_ms * 1000000ull : 0;
	int ret;

	ret = sdxi_dp_flags_ok(flags);
	if (ret)
		return ret;

	for (;;) {
		uint64_t read_idx = sdxi_cxt_read_idx(cxt);
		uint64_t reserve = __atomic_load_n(&cxt->prod_reserve, __ATOMIC_RELAXED);
		uint64_t new_reserve;

		if (ndesc == 0)
			return -EINVAL;
		new_reserve = reserve + ndesc;

		if (new_reserve - read_idx <= cxt->ds_ring_entries) {
			if (!(flags & SDXI_DP_RESERVE_F_MULTI_PRODUCER)) {
				__atomic_store_n(&cxt->prod_reserve, new_reserve,
						 __ATOMIC_RELAXED);
				*start_out = reserve;
				return 0;
			}

			if (__atomic_compare_exchange_n(&cxt->prod_reserve, &reserve,
							new_reserve, false,
							__ATOMIC_ACQ_REL,
							__ATOMIC_RELAXED)) {
				*start_out = reserve;
				return 0;
			}
			continue;
		}

		if (timeout_ms && sdxi_now_ns() >= deadline_ns)
			return -ETIMEDOUT;

		sdxi_relax();
	}
}

static int sdxi_dp_desc_store_slot(void *dst_slot, const struct sdxi_dp_desc *desc)
{
	const uint8_t *s;
	uint8_t *d;

	if (!dst_slot || !desc)
		return -EINVAL;

	s = desc->bytes;
	d = dst_slot;

	memcpy(d + 8, s + 8, SDXI_DP_DESC_BYTES - 8);
	__atomic_thread_fence(__ATOMIC_RELEASE);
	memcpy(d, s, 8);
	return 0;
}

static int sdxi_dp_write_idx_store(uint64_t *write_index, uint64_t value)
{
	if (!write_index)
		return -EINVAL;

	__atomic_store_n(write_index, value, __ATOMIC_RELEASE);
	return 0;
}

static int sdxi_dp_ring_doorbell(volatile uint64_t *doorbell, uint64_t value)
{
	if (!doorbell)
		return -EINVAL;

	__atomic_thread_fence(__ATOMIC_RELEASE);
	*doorbell = value;
	return 0;
}

static int sdxi_cxt_commit(sdxi_cxt_t *cxt, uint64_t start, uint32_t ndesc,
			   uint32_t flags, uint32_t timeout_ms,
			   uint64_t *commit_out)
{
	const uint64_t start_ns = sdxi_now_ns();
	const uint64_t deadline_ns =
		timeout_ms ? start_ns + (uint64_t)timeout_ms * 1000000ull : 0;
	int ret;

	ret = sdxi_dp_flags_ok(flags);
	if (ret)
		return ret;

	for (;;) {
		uint64_t new_commit = start + ndesc;

		if (!(flags & SDXI_DP_RESERVE_F_MULTI_PRODUCER)) {
			uint64_t expected =
				__atomic_load_n(&cxt->prod_commit, __ATOMIC_RELAXED);

			if (expected == start) {
				__atomic_store_n(&cxt->prod_commit, new_commit,
						 __ATOMIC_RELAXED);
				ret = sdxi_dp_write_idx_store(
					sdxi_cxt_write_idx_ptr(cxt), new_commit);
				if (ret)
					return ret;
				ret = sdxi_dp_ring_doorbell(sdxi_cxt_doorbell_ptr(cxt),
							    new_commit);
				if (ret)
					return ret;
				if (commit_out)
					*commit_out = new_commit;
				return 0;
			}
		} else {
			uint64_t expected = start;

			if (__atomic_compare_exchange_n(&cxt->prod_commit, &expected,
							new_commit, false,
							__ATOMIC_ACQ_REL,
							__ATOMIC_RELAXED)) {
				ret = sdxi_dp_write_idx_store(
					sdxi_cxt_write_idx_ptr(cxt), new_commit);
				if (ret)
					return ret;
				ret = sdxi_dp_ring_doorbell(
					sdxi_cxt_doorbell_ptr(cxt), new_commit);
				if (ret)
					return ret;
				if (commit_out)
					*commit_out = new_commit;
				return 0;
			}
		}

		if (timeout_ms && sdxi_now_ns() >= deadline_ns)
			return -ETIMEDOUT;

		sdxi_relax();
	}
}

static int sdxi_cxt_desc_store(sdxi_cxt_t *cxt, uint64_t pos,
			       const struct sdxi_dp_desc *desc)
{
	uint8_t *ring;
	void *slot;

	if (!cxt || !desc)
		return -EINVAL;
	if (cxt->ds_ring_entries == 0 || !cxt->mmaps.ds_ring)
		return -EINVAL;

	ring = (uint8_t *)cxt->mmaps.ds_ring;
	slot = ring + (pos % cxt->ds_ring_entries) * SDXI_DESC_BYTES;

	return sdxi_dp_desc_store_slot(slot, desc);
}

int sdxi_dp_reserve(sdxi_cxt_t *cxt, const struct sdxi_dp_reserve_args *args,
		    struct sdxi_dp_reservation *out)
{
	uint64_t start;
	uint32_t ndesc;
	uint32_t timeout_ms;
	uint32_t flags;
	int ret;

	if (!cxt || !args || !out)
		return -EINVAL;
	if (args->size != 0 && args->size != sizeof(*args))
		return -EINVAL;
	for (size_t i = 0; i < sizeof(args->rsvd) / sizeof(args->rsvd[0]); i++) {
		if (args->rsvd[i])
			return -EINVAL;
	}

	ndesc = args->ndesc;
	timeout_ms = args->timeout_ms;
	flags = args->flags;

	if (cxt->ds_ring_entries == 0 || !cxt->mmaps.ds_ring)
		return -EINVAL;
	if (ndesc == 0 || ndesc > cxt->ds_ring_entries)
		return -EINVAL;

	ret = sdxi_cxt_reserve(cxt, ndesc, flags, timeout_ms, &start);
	if (ret)
		return ret;

	memset(out, 0, sizeof(*out));
	out->size = sizeof(*out);
	out->flags = flags;
	out->start = start;
	out->ndesc = ndesc;
	return 0;
}

int sdxi_dp_commit(sdxi_cxt_t *cxt, const struct sdxi_dp_reservation *resv,
		   uint32_t timeout_ms, uint64_t *write_idx_out)
{
	if (!cxt || !resv)
		return -EINVAL;
	if (resv->size != 0 && resv->size != sizeof(*resv))
		return -EINVAL;
	if (resv->rsvd0)
		return -EINVAL;
	for (size_t i = 0; i < sizeof(resv->rsvd) / sizeof(resv->rsvd[0]); i++) {
		if (resv->rsvd[i])
			return -EINVAL;
	}
	if (resv->ndesc == 0 || resv->ndesc > cxt->ds_ring_entries)
		return -EINVAL;

	return sdxi_cxt_commit(cxt, resv->start, resv->ndesc, resv->flags,
			       timeout_ms, write_idx_out);
}

int sdxi_dp_desc_store(sdxi_cxt_t *cxt, uint64_t pos,
		       const struct sdxi_dp_desc *desc)
{
	return sdxi_cxt_desc_store(cxt, pos, desc);
}

int sdxi_dp_wait_read_idx(sdxi_cxt_t *cxt, uint64_t target, uint32_t timeout_ms)
{
	if (!cxt)
		return -EINVAL;

	return sdxi_cxt_wait_read_idx(cxt, target, timeout_ms);
}

int sdxi_dp_wait_read_idx_poll(sdxi_cxt_t *cxt, uint64_t target,
			       uint32_t timeout_ms)
{
	if (!cxt)
		return -EINVAL;

	return sdxi_cxt_wait_read_idx_nodrain(cxt, target, timeout_ms);
}

int sdxi_dp_wait_csb(sdxi_cxt_t *cxt, const struct sdxi_dp_csb *csb,
		     uint32_t timeout_ms)
{
	if (!cxt || !csb)
		return -EINVAL;

	return sdxi_cxt_wait_csb(cxt, (const struct sdxi_csb *)csb, timeout_ms);
}

int sdxi_dp_wait_csb_poll(sdxi_cxt_t *cxt, const struct sdxi_dp_csb *csb,
			  uint32_t timeout_ms)
{
	if (!cxt || !csb)
		return -EINVAL;

	return sdxi_cxt_wait_csb_nodrain(cxt, (const struct sdxi_csb *)csb,
					 timeout_ms);
}

static int sdxi_cxt_submit_desc(sdxi_cxt_t *cxt, const struct sdxi_desc *desc,
				uint32_t timeout_ms, uint64_t *new_write_idx_out,
				struct sdxi_csb **csb_out)
{
	uint64_t start;
	uint64_t commit;
	uint8_t *ring;
	struct sdxi_desc desc_local;
	struct sdxi_csb *csb = NULL;
	int ret;

	if (!cxt || !desc)
		return -EINVAL;
	if (cxt->ds_ring_entries == 0)
		return -EINVAL;

	ret = sdxi_cxt_reserve(cxt, 1, 0, timeout_ms, &start);
	if (ret)
		return ret;

	{
		uint32_t slot = start % cxt->ds_ring_entries;

		if (!cxt->csbs || cxt->csbs == MAP_FAILED)
			return -EINVAL;

		csb = (struct sdxi_csb *)cxt->csbs + slot;
		memset(csb, 0, sizeof(*csb));
	}

	desc_local = *desc;
	if (csb) {
		desc_local.csr = 1;
		desc_local.comp_ptr = (uint64_t)(uintptr_t)csb;
	}
	if (csb_out)
		*csb_out = csb;

	ring = (uint8_t *)cxt->mmaps.ds_ring;
	sdxi_ring_copy_desc(ring + (start % cxt->ds_ring_entries) * SDXI_DESC_BYTES,
			    csb ? &desc_local : desc);

	ret = sdxi_cxt_commit(cxt, start, 1, 0, timeout_ms, &commit);
	if (ret)
		return ret;

	if (new_write_idx_out)
		*new_write_idx_out = commit;
	return 0;
}

int sdxi_cxt_create(sdxi_dev_t *dev, const struct sdxi_cxt_create_opts *opts,
		    sdxi_cxt_t **cxt_out)
{
	struct sdxi_dev_caps caps;
	struct sdxi_cxt_create_opts def;
	struct sdxi_abi_cxt_create_req req;
	struct sdxi_abi_cxt_create_resp resp;
	sdxi_cxt_t *cxt = NULL;
	size_t page_sz;
	size_t ring_bytes;
	int oflags;
	int ret;
	bool cxt_eventfd_supported;

	if (!dev || !cxt_out)
		return -EINVAL;

	*cxt_out = NULL;

	memset(&def, 0, sizeof(def));
	def.size = sizeof(def);
	def.flags = 0;
	def.ds_ring_entries = 0;
	def.opb_000_enb = 0;
	def.eventfd = -1;

	if (!opts)
		opts = &def;
	if (opts->size != 0 && opts->size != sizeof(*opts))
		return -EINVAL;
	if (opts->flags & ~SDXI_CXT_CREATE_F_EVENTFD)
		return -EINVAL;
	for (size_t i = 0; i < sizeof(opts->rsvd) / sizeof(opts->rsvd[0]); i++) {
		if (opts->rsvd[i])
			return -EINVAL;
	}

	oflags = fcntl(dev->fd, F_GETFL);
	if (oflags < 0)
		return -errno;
	if ((oflags & O_ACCMODE) == O_RDONLY)
		return -EACCES;

	ret = sdxi_dev_get_caps(dev, &caps);
	if (ret)
		return ret;
	cxt_eventfd_supported =
		sdxi_dev_has_uapi_cap(dev, SDXI_UAPI_INFO_CAP_CXT_EVENTFD);
	if ((opts->flags & SDXI_CXT_CREATE_F_EVENTFD) && !cxt_eventfd_supported)
		return -ENOTSUP;

	cxt = calloc(1, sizeof(*cxt));
	if (!cxt)
		return -ENOMEM;

	cxt->fd = -1;
	cxt->opb_000_enb = opts->opb_000_enb;
	cxt->eventfd = -1;

	page_sz = sdxi_page_size();

	if (opts->flags & SDXI_CXT_CREATE_F_EVENTFD) {
		if (opts->eventfd >= 0) {
			cxt->eventfd = opts->eventfd;
			cxt->eventfd_owned = false;
		} else {
			cxt->eventfd = eventfd(0, EFD_CLOEXEC);
			if (cxt->eventfd < 0) {
				ret = -errno;
				goto err;
			}
			cxt->eventfd_owned = true;
		}
	}

	memset(&req, 0, sizeof(req));
	req.opb_000_enb = opts->opb_000_enb;
	req.ds_ring_entries = opts->ds_ring_entries;

	{
		uint32_t requested = opts->ds_ring_entries ? opts->ds_ring_entries : 1024;

		if (requested > caps.max_ds_ring_entries) {
			ret = -EINVAL;
			goto err;
		}

		ring_bytes = sdxi_ring_bytes(requested);
		if (!ring_bytes) {
			ret = -EOVERFLOW;
			goto err;
		}

		cxt->mmaps.cxt_sts = sdxi_mmap_anon(page_sz);
		cxt->mmaps.write_index = sdxi_mmap_anon(page_sz);
		cxt->mmaps.ds_ring = sdxi_mmap_anon(ring_bytes);
		if (cxt->mmaps.cxt_sts == MAP_FAILED ||
		    cxt->mmaps.write_index == MAP_FAILED ||
		    cxt->mmaps.ds_ring == MAP_FAILED) {
			ret = -ENOMEM;
			goto err;
		}

		cxt->mmaps.cxt_sts_bytes = page_sz;
		cxt->mmaps.write_index_bytes = page_sz;
		cxt->mmaps.ds_ring_bytes = ring_bytes;

		req.ds_ring_entries = requested;
		req.ds_ring_base = (uint64_t)(uintptr_t)cxt->mmaps.ds_ring;
		req.cxt_sts_base = (uint64_t)(uintptr_t)cxt->mmaps.cxt_sts;
		req.write_index_base = (uint64_t)(uintptr_t)cxt->mmaps.write_index;
	}

	ret = sdxi_abi_create_cxt(dev->fd, &req, &resp);
	if (ret)
		goto err;

	cxt->fd = resp.cxt_fd;
	cxt->pasid = resp.pasid;
	cxt->created = true;
	cxt->ds_ring_entries = resp.ds_ring_entries;

	if (resp.ds_ring_entries > req.ds_ring_entries) {
		ret = -EINVAL;
		goto err;
	}

	if (cxt->eventfd >= 0) {
		ret = sdxi_abi_cxt_eventfd(cxt->fd, cxt->eventfd,
					 &cxt->intr_akey,
					 &cxt->intr_num);
		if (ret)
			goto err;
	}

	cxt->mmaps.doorbell_bytes = caps.db_stride_bytes;
	cxt->mmaps.doorbell = mmap(NULL, cxt->mmaps.doorbell_bytes, PROT_WRITE,
				   MAP_SHARED, cxt->fd,
				   (off_t)resp.doorbell_mmap_base);
	if (cxt->mmaps.doorbell == MAP_FAILED) {
		ret = -ENOMEM;
		goto err;
	}

	cxt->csbs_bytes = sdxi_csb_bytes(cxt->ds_ring_entries);
	if (!cxt->csbs_bytes) {
		ret = -EINVAL;
		goto err;
	}

	cxt->csbs = sdxi_mmap_anon(cxt->csbs_bytes);
	if (cxt->csbs == MAP_FAILED) {
		ret = -errno;
		goto err;
	}

	/* Initialize producer indices from the shared write_index memory. */
	cxt->prod_reserve = __atomic_load_n(sdxi_cxt_write_idx_ptr(cxt), __ATOMIC_RELAXED);
	cxt->prod_commit = cxt->prod_reserve;

	*cxt_out = cxt;
	return 0;

err:
	if (cxt) {
		(void)sdxi_cxt_destroy(cxt);
	}
	return ret;
}

int sdxi_cxt_quiesce(sdxi_cxt_t *cxt, uint32_t timeout_ms,
		     uint32_t *cst_flags_out)
{
	int ret;

	if (!cxt)
		return -EINVAL;

	if (cst_flags_out)
		*cst_flags_out = 0;

	if (!cxt->created || cxt->fd < 0)
		return 0;
	if (cxt->quiesced)
		return 0;

	ret = sdxi_abi_quiesce_cxt(cxt->fd, timeout_ms, cst_flags_out);
	if (ret)
		return ret;

	cxt->quiesced = true;
	return 0;
}

int sdxi_cxt_destroy(sdxi_cxt_t *cxt)
{
	int ret = 0;

	if (!cxt)
		return 0;

	/*
	 * The current context-fd lifetime model uses explicit quiesce as the
	 * synchronous stop boundary before userspace tears down mappings and
	 * closes the context handle.
	 */
	if (cxt->created)
		ret = sdxi_cxt_quiesce(cxt, SDXI_CXT_QUIESCE_TIMEOUT_DEFAULT,
				       NULL);

	sdxi_munmap_safe(cxt->csbs, cxt->csbs_bytes);
	sdxi_munmap_safe(cxt->mmaps.doorbell, cxt->mmaps.doorbell_bytes);
	sdxi_munmap_safe(cxt->mmaps.ds_ring, cxt->mmaps.ds_ring_bytes);
	sdxi_munmap_safe(cxt->mmaps.write_index, cxt->mmaps.write_index_bytes);
	sdxi_munmap_safe(cxt->mmaps.cxt_sts, cxt->mmaps.cxt_sts_bytes);

	if (cxt->eventfd_owned && cxt->eventfd >= 0)
		close(cxt->eventfd);

	if (cxt->fd >= 0 && close(cxt->fd) != 0 && ret == 0)
		ret = -errno;

	free(cxt);
	return ret;
}

int sdxi_cxt_get_fd(const sdxi_cxt_t *cxt)
{
	if (!cxt)
		return -EINVAL;
	return cxt->fd;
}

uint32_t sdxi_cxt_pasid(const sdxi_cxt_t *cxt)
{
	return cxt ? cxt->pasid : 0;
}

int sdxi_cxt_get_mmaps(const sdxi_cxt_t *cxt, struct sdxi_cxt_mmaps *out)
{
	if (!cxt || !out)
		return -EINVAL;
	*out = cxt->mmaps;
	return 0;
}

int sdxi_cxt_get_eventfd(const sdxi_cxt_t *cxt)
{
	if (!cxt)
		return -EINVAL;
	return cxt->eventfd;
}

int sdxi_cxt_get_intr_info(const sdxi_cxt_t *cxt, uint16_t *intr_akey_out,
			   uint32_t *intr_num_out)
{
	if (!cxt)
		return -EINVAL;
	if (intr_akey_out)
		*intr_akey_out = cxt->intr_akey;
	if (intr_num_out)
		*intr_num_out = cxt->intr_num;
	return 0;
}

int sdxi_cxt_get_akey_list(sdxi_cxt_t *cxt, struct sdxi_bitmap *out)
{
	if (!cxt || !out)
		return -EINVAL;
	return sdxi_abi_get_akey_list(cxt->fd, out);
}

int sdxi_cxt_get_akeys(sdxi_cxt_t *cxt, struct sdxi_key_ent *ents, size_t nents)
{
	if (!cxt)
		return -EINVAL;
	return sdxi_abi_get_akeys(cxt->fd, ents, nents);
}

int sdxi_cxt_set_akeys(sdxi_cxt_t *cxt, const struct sdxi_key_ent *ents,
		       size_t nents, uint32_t timeout_ms,
		       struct sdxi_key_set_result *res)
{
	struct sdxi_key_set_result local;
	struct sdxi_key_set_result *out = res ? res : &local;

	if (!cxt)
		return -EINVAL;

	out->commit = SDXI_KEY_COMMIT_NONE_E;
	out->first_failed_index = 0;
	out->cst_flags = 0;

	return sdxi_abi_set_akeys(cxt->fd, ents, nents, timeout_ms, out);
}

int sdxi_cxt_get_akey(sdxi_cxt_t *cxt, uint32_t index,
		      struct sdxi_key_ent *ent_out)
{
	struct sdxi_key_ent ent;
	int ret;

	if (!cxt || !ent_out)
		return -EINVAL;

	memset(&ent, 0, sizeof(ent));
	ent.index = index;

	ret = sdxi_cxt_get_akeys(cxt, &ent, 1);
	if (ret)
		return ret;

	*ent_out = ent;
	return 0;
}

int sdxi_cxt_set_akey(sdxi_cxt_t *cxt, const struct sdxi_key_ent *ent)
{
	struct sdxi_key_set_result res;
	int ret;

	if (!cxt || !ent)
		return -EINVAL;

	ret = sdxi_cxt_set_akeys(cxt, ent, 1, SDXI_KEY_TIMEOUT_DEFAULT, &res);
	if (ret)
		return ret;
	if (res.commit != SDXI_KEY_COMMIT_ALL_E)
		return -EIO;
	return 0;
}

int sdxi_cxt_set_akeys_resume(sdxi_cxt_t *cxt,
			      const struct sdxi_key_ent *ents, size_t nents,
			      uint32_t timeout_ms,
			      struct sdxi_key_set_result *res)
{
	struct sdxi_key_set_result local;
	struct sdxi_key_set_result *out = res ? res : &local;
	uint32_t off = 0;
	uint32_t budget = (uint32_t)nents + 1;
	int ret;

	if (!cxt)
		return -EINVAL;
	if (!ents && nents)
		return -EINVAL;
	if (nents > UINT32_MAX)
		return -EINVAL;
	if (nents == 0)
		return 0;

	out->commit = SDXI_KEY_COMMIT_NONE_E;
	out->first_failed_index = 0;
	out->cst_flags = 0;

	while (budget--) {
		ret = sdxi_abi_set_akeys(cxt->fd, ents + off,
					 (size_t)((uint32_t)nents - off),
					 timeout_ms, out);
		if (ret)
			return ret;

		if (out->commit == SDXI_KEY_COMMIT_ALL_E)
			return 0;
		if (out->commit != SDXI_KEY_COMMIT_PARTIAL_E)
			return 0;

		if (out->first_failed_index == 0)
			return -EIO;
		off += out->first_failed_index;
		if (off >= nents)
			return 0;
	}

	return -EIO;
}

int sdxi_cxt_alloc_akey_index(sdxi_cxt_t *cxt, uint32_t *index_out)
{
	struct sdxi_bitmap bm;
	uint32_t i;
	int ret;

	if (!cxt || !index_out)
		return -EINVAL;

	memset(&bm, 0, sizeof(bm));
	ret = sdxi_cxt_get_akey_list(cxt, &bm);
	if (ret)
		return ret;

	ret = -ENOSPC;
	for (i = 0; i < bm.nentries; i++) {
		uint8_t byte;

		if (i == 0 || i == 1 || i == cxt->intr_akey)
			continue;

		byte = bm.data ? bm.data[i / 8] : 0;
		if (!(byte & (uint8_t)(1u << (i % 8)))) {
			*index_out = i;
			ret = 0;
			break;
		}
	}

	sdxi_bitmap_free(&bm);
	return ret;
}

int sdxi_cxt_invalidate_akey(sdxi_cxt_t *cxt, uint32_t index,
			     uint32_t timeout_ms,
			     struct sdxi_key_set_result *res)
{
	struct sdxi_key_ent ent;

	if (!cxt)
		return -EINVAL;

	memset(&ent, 0, sizeof(ent));
	ent.index = index;
	sdxi_key_payload_clear(&ent.payload);
	return sdxi_cxt_set_akeys(cxt, &ent, 1, timeout_ms, res);
}

int sdxi_nop(sdxi_cxt_t *cxt, uint32_t timeout_ms)
{
	struct sdxi_dp_desc raw;
	struct sdxi_desc desc;
	struct sdxi_csb *csb = NULL;
	uint64_t new_write;
	int ret;

	if (!cxt)
		return -EINVAL;

	ret = sdxi_dp_desc_build_dma_nop(&raw);
	if (ret)
		return ret;
	sdxi_desc_from_public(&desc, &raw);

	ret = sdxi_cxt_submit_desc(cxt, &desc, timeout_ms, &new_write, &csb);
	if (ret)
		return ret;

	(void)new_write;
	return sdxi_cxt_wait_csb(cxt, csb, timeout_ms);
}

int sdxi_mem_copy(sdxi_cxt_t *cxt, void *dst, const void *src, size_t size,
				  uint32_t timeout_ms)
{
	return sdxi_mem_copy_akeys(cxt, dst, 0x1, src, 0x1, size, timeout_ms);
}

int sdxi_mem_copy_akeys(sdxi_cxt_t *cxt, void *dst, uint16_t dst_akey,
			const void *src, uint16_t src_akey, size_t size,
			uint32_t timeout_ms)
{
	struct sdxi_dp_dma_copy_req req;
	struct sdxi_dp_desc raw;
	struct sdxi_desc desc;
	struct sdxi_csb *csb = NULL;
	uint64_t new_write;
	int ret;

	if (!cxt || !dst || !src || size == 0 || size > UINT32_MAX)
		return -EINVAL;

	memset(&req, 0, sizeof(req));
	req.size = sizeof(req);
	req.src_akey = src_akey;
	req.dst_akey = dst_akey;
	req.addr_src = (uint64_t)(uintptr_t)src;
	req.addr_dst = (uint64_t)(uintptr_t)dst;
	req.nbytes = (uint32_t)size;

	ret = sdxi_dp_desc_build_dma_copy(&raw, &req);
	if (ret)
		return ret;
	sdxi_desc_from_public(&desc, &raw);

	ret = sdxi_cxt_submit_desc(cxt, &desc, timeout_ms, &new_write, &csb);
	if (ret)
		return ret;

	(void)new_write;
	return sdxi_cxt_wait_csb(cxt, csb, timeout_ms);
}

int sdxi_mem_rep_copy(sdxi_cxt_t *cxt, void *dst, const void *src,
			      size_t src_size, size_t repeat, uint32_t timeout_ms)
{
	struct sdxi_dp_dma_rep_copy_req req;
	struct sdxi_dp_desc raw;
	struct sdxi_desc desc;
	struct sdxi_csb *csb = NULL;
	uint64_t new_write;
	uint32_t n_4k;
	int ret;

	if (!cxt || !dst || !src || src_size == 0 || repeat == 0)
		return -EINVAL;
	if (src_size % 4096)
		return -EINVAL;
	if (((uintptr_t)src | (uintptr_t)dst) & 4095)
		return -EINVAL;
	if (repeat > (1ull << 20))
		return -EINVAL;

	n_4k = (uint32_t)(src_size / 4096);
	if (n_4k == 0)
		return -EINVAL;
	if (n_4k > 512)
		return -EINVAL;

	memset(&req, 0, sizeof(req));
	req.size = sizeof(req);
	req.src_akey = 0x1;
	req.dst_akey = 0x1;
	req.addr_src = (uint64_t)(uintptr_t)src;
	req.addr_dst = (uint64_t)(uintptr_t)dst;
	req.size_4k = n_4k;
	req.repeat = (uint32_t)repeat;

	ret = sdxi_dp_desc_build_dma_rep_copy(&raw, &req);
	if (ret)
		return ret;
	sdxi_desc_from_public(&desc, &raw);

	ret = sdxi_cxt_submit_desc(cxt, &desc, timeout_ms, &new_write, &csb);
	if (ret)
		return ret;

	(void)new_write;
	return sdxi_cxt_wait_csb(cxt, csb, timeout_ms);
}

int sdxi_write_imm(sdxi_cxt_t *cxt, void *dst, const void *data, size_t size,
			   uint32_t timeout_ms)
{
	struct sdxi_dp_dma_write_imm_req req;
	struct sdxi_dp_desc raw;
	struct sdxi_desc desc;
	struct sdxi_csb *csb = NULL;
	uint64_t new_write;
	int ret;

	if (!cxt || !dst || !data || size == 0 || size > 32)
		return -EINVAL;

	memset(&req, 0, sizeof(req));
	req.size = sizeof(req);
	req.dst_akey = 0x1;
	req.addr_dst = (uint64_t)(uintptr_t)dst;
	req.data_bytes = (uint32_t)size;
	memcpy(req.data, data, size);

	ret = sdxi_dp_desc_build_dma_write_imm(&raw, &req);
	if (ret)
		return ret;
	sdxi_desc_from_public(&desc, &raw);

	ret = sdxi_cxt_submit_desc(cxt, &desc, timeout_ms, &new_write, &csb);
	if (ret)
		return ret;

	(void)new_write;
	return sdxi_cxt_wait_csb(cxt, csb, timeout_ms);
}

int sdxi_atomic_uadd(sdxi_cxt_t *cxt, void *addr, uint64_t operand,
			     uint32_t timeout_ms)
{
	struct sdxi_dp_atomic_uadd_req req;
	struct sdxi_dp_desc raw;
	struct sdxi_desc desc;
	struct sdxi_csb *csb = NULL;
	uint64_t new_write;
	int ret;

	if (!cxt || !addr)
		return -EINVAL;

	memset(&req, 0, sizeof(req));
	req.size = sizeof(req);
	req.dst_akey = 0x1;
	req.operand_bytes = sizeof(operand);
	req.addr_dst = (uint64_t)(uintptr_t)addr;
	req.operand = operand;

	ret = sdxi_dp_desc_build_atomic_uadd(&raw, &req);
	if (ret)
		return ret;
	sdxi_desc_from_public(&desc, &raw);

	ret = sdxi_cxt_submit_desc(cxt, &desc, timeout_ms, &new_write, &csb);
	if (ret)
		return ret;

	(void)new_write;
	return sdxi_cxt_wait_csb(cxt, csb, timeout_ms);
}

int sdxi_intr(sdxi_cxt_t *cxt, uint32_t timeout_ms)
{
	struct sdxi_dp_intr_req req;
	struct sdxi_dp_desc raw;
	struct sdxi_desc desc;
	struct sdxi_csb *csb = NULL;
	uint64_t new_write;
	int ret;

	if (!cxt)
		return -EINVAL;
	if (cxt->eventfd < 0)
		return -ENOTSUP;

	memset(&req, 0, sizeof(req));
	req.size = sizeof(req);
	req.akey = cxt->intr_akey;

	ret = sdxi_dp_desc_build_intr(&raw, &req);
	if (ret)
		return ret;
	sdxi_desc_from_public(&desc, &raw);

	ret = sdxi_cxt_submit_desc(cxt, &desc, timeout_ms, &new_write, &csb);
	if (ret)
		return ret;

	(void)new_write;
	return sdxi_cxt_wait_csb_nodrain(cxt, csb, timeout_ms);
}

int sdxi_crc_m1(sdxi_cxt_t *cxt, const struct sdxi_crc_m1_req *req,
		uint32_t timeout_ms)
{
	struct sdxi_dp_desc raw;
	struct sdxi_desc desc;
	struct sdxi_csb *csb = NULL;
	uint64_t new_write;
	int ret;

	if (!cxt || !req)
		return -EINVAL;

	ret = sdxi_dp_desc_build_crc_m1(&raw, req);
	if (ret)
		return ret;
	sdxi_desc_from_public(&desc, &raw);

	ret = sdxi_cxt_submit_desc(cxt, &desc, timeout_ms, &new_write, &csb);
	if (ret)
		return ret;

	(void)new_write;
	return sdxi_cxt_wait_csb(cxt, csb, timeout_ms);
}

int sdxi_cxt_submit_public_desc(sdxi_cxt_t *cxt,
				const struct sdxi_dp_desc *raw,
				uint32_t timeout_ms)
{
	struct sdxi_desc desc;
	struct sdxi_csb *csb = NULL;
	uint64_t new_write;
	int ret;

	if (!cxt || !raw)
		return -EINVAL;

	sdxi_desc_from_public(&desc, raw);

	ret = sdxi_cxt_submit_desc(cxt, &desc, timeout_ms, &new_write, &csb);
	if (ret)
		return ret;

	(void)new_write;
	return sdxi_cxt_wait_csb(cxt, csb, timeout_ms);
}
