// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2026 Advanced Micro Devices, Inc.
 *
 * SDXI library sample code - kill a process that owns a live context
 *
 * This sample exercises the driver's file-release cleanup path:
 *   1) child opens a device, creates a context, and posts one or more DMA
 *      COPY descriptors directly to the descriptor ring without waiting
 *   2) parent receives the "submitted" notification and sends SIGKILL to the
 *      child before it calls sdxi_cxt_destroy()
 *   3) parent then verifies that the same device still accepts a fresh
 *      context and can complete a small memcopy
 *
 * The sample is a best-effort "killed while work is active" test. Increase
 * --size or reduce --kill-delay-ms if your device completes the copy too fast.
 */
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "libsdxi.h"
#include "samples.h"

/* Internal descriptor definitions used only by this sample. */
#include "../src/sdxi_dp.h"

#define TEST_BUFFER_ALIGN 4096u
#define VALIDATE_COPY_BYTES 4096u
#define DEFAULT_COPY_BYTES (128u * 1024u * 1024u)
#define SUBMIT_CHUNK_BYTES (32u * 1024u * 1024u)
#define DEFAULT_LOOPS 4u
#define DEFAULT_TIMEOUT_MS 5000u
#define DEFAULT_KILL_DELAY_MS 1u
#define RUNNING_TIMEOUT_MS 1000u
#define VALIDATE_RETRY_MS 50u
#define CHILD_GUARD_SECONDS 30u

#define KILL_CXT_MAGIC 0x4b435458u /* "KCTX" */

enum kill_cxt_msg_type {
	KILL_CXT_MSG_READY = 1,
	KILL_CXT_MSG_ERROR = 2,
};

struct kill_cxt_msg {
	uint32_t magic;
	uint32_t type;
	int32_t ret;
	int32_t cxt_fd;
	uint32_t pasid;
	uint32_t rsvd;
};

struct kill_cxt_args {
	const char *dev_path;
	char dev_path_buf[PATH_MAX];
	size_t copy_bytes;
	uint32_t loops;
	uint32_t timeout_ms;
	uint32_t kill_delay_ms;
};

static int kill_cxt_write_full(int fd, const void *buf, size_t len)
{
	const uint8_t *p = buf;

	while (len > 0) {
		ssize_t n = write(fd, p, len);

		if (n < 0) {
			if (errno == EINTR)
				continue;
			return -errno;
		}
		if (n == 0)
			return -EPIPE;
		p += (size_t)n;
		len -= (size_t)n;
	}
	return 0;
}

static int kill_cxt_read_full(int fd, void *buf, size_t len)
{
	uint8_t *p = buf;

	while (len > 0) {
		ssize_t n = read(fd, p, len);

		if (n < 0) {
			if (errno == EINTR)
				continue;
			return -errno;
		}
		if (n == 0)
			return -EPIPE;
		p += (size_t)n;
		len -= (size_t)n;
	}
	return 0;
}

static int kill_cxt_send_msg(int fd, uint32_t type, int32_t ret, int32_t cxt_fd,
			     uint32_t pasid)
{
	struct kill_cxt_msg msg;

	memset(&msg, 0, sizeof(msg));
	msg.magic = KILL_CXT_MAGIC;
	msg.type = type;
	msg.ret = ret;
	msg.cxt_fd = cxt_fd;
	msg.pasid = pasid;

	return kill_cxt_write_full(fd, &msg, sizeof(msg));
}

static int kill_cxt_recv_msg(int fd, struct kill_cxt_msg *msg_out)
{
	struct kill_cxt_msg msg;
	int ret;

	ret = kill_cxt_read_full(fd, &msg, sizeof(msg));
	if (ret)
		return ret;
	if (msg.magic != KILL_CXT_MAGIC)
		return -EBADMSG;

	*msg_out = msg;
	return 0;
}

static uint64_t kill_cxt_now_ns(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		return 0;
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void kill_cxt_sleep_ms(uint32_t ms)
{
	struct timespec req;

	req.tv_sec = ms / 1000u;
	req.tv_nsec = (long)(ms % 1000u) * 1000000l;

	while (nanosleep(&req, &req) != 0) {
		if (errno != EINTR)
			break;
	}
}

static void kill_cxt_ring_copy_desc(void *dst_slot, const struct sdxi_desc *src)
{
	const uint8_t *s = (const uint8_t *)src;
	uint8_t *d = (uint8_t *)dst_slot;

	memcpy(d + 8, s + 8, SDXI_DESC_BYTES - 8);
	__atomic_thread_fence(__ATOMIC_RELEASE);
	memcpy(d, s, 8);
}

static void kill_cxt_desc_set_hdr(struct sdxi_desc *desc, uint32_t type,
				  uint32_t sub_type)
{
	desc->vl = 1;
	desc->type = type;
	desc->sub_type = sub_type;
	desc->comp_ptr = 1;
}

static void kill_cxt_build_dma_nop(struct sdxi_desc *desc)
{
	memset(desc, 0, sizeof(*desc));
	desc->fe = 1;

	kill_cxt_desc_set_hdr(desc, SDXI_OP_TYPE_DMA, SDXI_OP_DMA_NOP);
}

static void kill_cxt_build_dma_copy(struct sdxi_desc *desc, uint64_t addr_src,
				    uint8_t attr_src, uint16_t src_akey,
				    uint64_t addr_dst, uint8_t attr_dst,
				    uint16_t dst_akey, uint32_t size)
{
	memset(desc, 0, sizeof(*desc));
	desc->fe = 1;

	desc->body.dmabasegrp.copy.size = size - 1;
	desc->body.dmabasegrp.copy.attr_src = attr_src;
	desc->body.dmabasegrp.copy.attr_dst = attr_dst;
	desc->body.dmabasegrp.copy.akey0 = src_akey;
	desc->body.dmabasegrp.copy.akey1 = dst_akey;
	desc->body.dmabasegrp.copy.addr0 = addr_src;
	desc->body.dmabasegrp.copy.addr1 = addr_dst;

	kill_cxt_desc_set_hdr(desc, SDXI_OP_TYPE_DMA, SDXI_OP_DMA_COPY);
}

static int kill_cxt_get_ring_mmaps(sdxi_cxt_t *cxt, struct sdxi_cxt_mmaps *mmaps_out,
				   uint32_t *ring_entries_out)
{
	struct sdxi_cxt_mmaps mmaps;
	uint32_t ring_entries;
	int ret;

	if (!cxt || !mmaps_out || !ring_entries_out)
		return -EINVAL;

	ret = sdxi_cxt_get_mmaps(cxt, &mmaps);
	if (ret)
		return ret;

	if (!mmaps.ds_ring || !mmaps.write_index || !mmaps.doorbell ||
	    !mmaps.cxt_sts)
		return -EINVAL;
	if (mmaps.ds_ring_bytes == 0 || (mmaps.ds_ring_bytes % SDXI_DESC_BYTES) != 0)
		return -EINVAL;
	if (mmaps.write_index_bytes < sizeof(uint64_t) ||
	    mmaps.cxt_sts_bytes < sizeof(struct sdxi_cxt_sts))
		return -EINVAL;

	ring_entries = (uint32_t)(mmaps.ds_ring_bytes / SDXI_DESC_BYTES);
	if (ring_entries == 0)
		return -EINVAL;

	*mmaps_out = mmaps;
	*ring_entries_out = ring_entries;
	return 0;
}

static uint8_t kill_cxt_state(const struct sdxi_cxt_sts *sts)
{
	return sts ? (uint8_t)(sts->state & 0xF) : SDXI_CXT_STATE_ERR;
}

static uint64_t kill_cxt_read_index(const struct sdxi_cxt_sts *sts)
{
	if (!sts)
		return 0;

	return __atomic_load_n(&sts->read_index, __ATOMIC_ACQUIRE);
}

static int kill_cxt_wait_running(sdxi_cxt_t *cxt, uint32_t timeout_ms)
{
	struct sdxi_cxt_mmaps mmaps;
	const struct sdxi_cxt_sts *sts;
	uint64_t deadline_ns;
	uint32_t ring_entries;
	int ret;

	ret = kill_cxt_get_ring_mmaps(cxt, &mmaps, &ring_entries);
	if (ret)
		return ret;

	sts = (const struct sdxi_cxt_sts *)mmaps.cxt_sts;
	deadline_ns = kill_cxt_now_ns() + (uint64_t)timeout_ms * 1000000ull;

	while (kill_cxt_state(sts) != SDXI_CXT_STATE_RUNNING) {
		if (kill_cxt_state(sts) == SDXI_CXT_STATE_ERR)
			return -EIO;
		if (kill_cxt_now_ns() >= deadline_ns)
			return -ETIMEDOUT;
		kill_cxt_sleep_ms(1);
	}

	return 0;
}

static int kill_cxt_submit_desc(sdxi_cxt_t *cxt, const struct sdxi_desc *desc,
				uint64_t *pos_out)
{
	struct sdxi_cxt_mmaps mmaps;
	const struct sdxi_cxt_sts *sts;
	uint64_t read_idx;
	uint64_t write_idx;
	uint64_t new_write;
	uint32_t ring_entries;
	uint8_t *ring;
	uint64_t *write_idx_ptr;
	volatile uint64_t *doorbell_ptr;
	int ret;

	if (!cxt || !desc)
		return -EINVAL;

	ret = kill_cxt_get_ring_mmaps(cxt, &mmaps, &ring_entries);
	if (ret)
		return ret;

	sts = (const struct sdxi_cxt_sts *)mmaps.cxt_sts;
	if (kill_cxt_state(sts) != SDXI_CXT_STATE_RUNNING)
		return -EIO;

	read_idx = kill_cxt_read_index(sts);
	__atomic_thread_fence(__ATOMIC_ACQ_REL);

	ring = (uint8_t *)mmaps.ds_ring;
	write_idx_ptr = (uint64_t *)mmaps.write_index;
	doorbell_ptr = (volatile uint64_t *)mmaps.doorbell;

	write_idx = __atomic_load_n(write_idx_ptr, __ATOMIC_RELAXED);
	if (read_idx > write_idx)
		return -EINVAL;

	new_write = write_idx + 1;
	if (new_write - read_idx > ring_entries)
		return -ENOSPC;

	__atomic_store_n(write_idx_ptr, new_write, __ATOMIC_RELEASE);
	__atomic_thread_fence(__ATOMIC_ACQ_REL);
	kill_cxt_ring_copy_desc(ring + (write_idx % ring_entries) * SDXI_DESC_BYTES,
				desc);
	*doorbell_ptr = new_write;

	if (pos_out)
		*pos_out = write_idx;
	return 0;
}

static int kill_cxt_submit_nop(sdxi_cxt_t *cxt, uint64_t *pos_out)
{
	struct sdxi_desc desc;

	kill_cxt_build_dma_nop(&desc);
	desc.csr = 1;

	return kill_cxt_submit_desc(cxt, &desc, pos_out);
}

static int kill_cxt_submit_copy_desc(sdxi_cxt_t *cxt, void *dst, const void *src,
				     size_t size, uint64_t *pos_out)
{
	struct sdxi_desc desc;

	if (!cxt || !dst || !src || size == 0 || size > UINT32_MAX)
		return -EINVAL;

	kill_cxt_build_dma_copy(&desc, (uint64_t)(uintptr_t)src, 0x0, 0x1,
				(uint64_t)(uintptr_t)dst, 0x0, 0x1,
				(uint32_t)size);
	desc.csr = 1;

	return kill_cxt_submit_desc(cxt, &desc, pos_out);
}

static int kill_cxt_force_read_index_advance(sdxi_cxt_t *cxt, uint64_t target,
					     uint32_t timeout_ms)
{
	struct sdxi_cxt_mmaps mmaps;
	const struct sdxi_cxt_sts *sts;
	uint64_t deadline_ns;
	uint32_t ring_entries;
	int ret;

	ret = kill_cxt_get_ring_mmaps(cxt, &mmaps, &ring_entries);
	if (ret)
		return ret;

	sts = (const struct sdxi_cxt_sts *)mmaps.cxt_sts;
	deadline_ns = kill_cxt_now_ns() + (uint64_t)timeout_ms * 1000000ull;

	for (;;) {
		int nop_ret;

		if (kill_cxt_read_index(sts) >= target)
			return 0;
		if (kill_cxt_state(sts) != SDXI_CXT_STATE_RUNNING)
			return -EIO;
		if (kill_cxt_now_ns() >= deadline_ns)
			return -ETIMEDOUT;

		nop_ret = kill_cxt_submit_nop(cxt, NULL);
		if (nop_ret && nop_ret != -ENOSPC)
			return nop_ret;

		kill_cxt_sleep_ms(1);
	}
}

static int kill_cxt_submit_sync_copy(sdxi_cxt_t *cxt, void *dst, const void *src,
				     size_t size, uint32_t timeout_ms)
{
	uint64_t pos;
	int ret;

	ret = kill_cxt_submit_copy_desc(cxt, dst, src, size, &pos);
	if (ret)
		return ret;

	return kill_cxt_force_read_index_advance(cxt, pos + 1, timeout_ms);
}

static int kill_cxt_submit_copy_workload(sdxi_cxt_t *cxt, void *dst, const void *src,
					 size_t total_bytes)
{
	const uint8_t *src_ptr = src;
	uint8_t *dst_ptr = dst;
	size_t remaining = total_bytes;

	while (remaining > 0) {
		size_t chunk = remaining;
		int ret;

		if (chunk > SUBMIT_CHUNK_BYTES)
			chunk = SUBMIT_CHUNK_BYTES;

		ret = kill_cxt_submit_copy_desc(cxt, dst_ptr, src_ptr, chunk, NULL);
		if (ret)
			return ret;

		src_ptr += chunk;
		dst_ptr += chunk;
		remaining -= chunk;
	}

	return 0;
}

static void kill_cxt_print_usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [options]\n", prog);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -d, --dev <path>          Specify SDXI device (e.g., sdxi0, /dev/sdxi0)\n");
	fprintf(stderr, "  --size <bytes>           Total COPY bytes submitted before kill (default: %u)\n",
		DEFAULT_COPY_BYTES);
	fprintf(stderr, "  --loops <n>              Number of kill/validate iterations (default: %u)\n",
		DEFAULT_LOOPS);
	fprintf(stderr, "  --timeout <ms>           Per-op timeout and recovery budget (default: %u)\n",
		DEFAULT_TIMEOUT_MS);
	fprintf(stderr, "  --kill-delay-ms <ms>     Delay between submit ack and SIGKILL (default: %u)\n",
		DEFAULT_KILL_DELAY_MS);
	fprintf(stderr, "  -h, --help               Show this help message\n");
	fprintf(stderr, "\nIf --dev is not specified, the first device from `/dev/sdxi*` is used.\n");
}

static int kill_cxt_parse_u32(const char *s, uint32_t *out)
{
	char *end = NULL;
	unsigned long long v;

	if (!s || !out)
		return -EINVAL;

	errno = 0;
	v = strtoull(s, &end, 0);
	if (errno || end == s || *end != '\0' || v > UINT32_MAX)
		return -EINVAL;

	*out = (uint32_t)v;
	return 0;
}

static int kill_cxt_parse_size(const char *s, size_t *out)
{
	char *end = NULL;
	unsigned long long v;

	if (!s || !out)
		return -EINVAL;

	errno = 0;
	v = strtoull(s, &end, 0);
	if (errno || end == s || *end != '\0' || v == 0 || v > UINT32_MAX)
		return -EINVAL;

	*out = (size_t)v;
	return 0;
}

static int kill_cxt_parse_args(int argc, char **argv, struct kill_cxt_args *args)
{
	static const struct option long_opts[] = {
		{"dev", required_argument, 0, 'd'},
		{"size", required_argument, 0, 'z'},
		{"loops", required_argument, 0, 'l'},
		{"timeout", required_argument, 0, 'T'},
		{"kill-delay-ms", required_argument, 0, 'k'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0},
	};
	int opt;
	int option_index = 0;
	int ret;

	memset(args, 0, sizeof(*args));
	args->copy_bytes = DEFAULT_COPY_BYTES;
	args->loops = DEFAULT_LOOPS;
	args->timeout_ms = DEFAULT_TIMEOUT_MS;
	args->kill_delay_ms = DEFAULT_KILL_DELAY_MS;

	optind = 1;

	while (1) {
		opt = getopt_long(argc, argv, "d:z:l:T:k:h", long_opts,
				  &option_index);
		if (opt == -1)
			break;

		switch (opt) {
		case 'd':
			ret = sample_normalize_dev_path(optarg, args->dev_path_buf,
							sizeof(args->dev_path_buf),
							&args->dev_path);
			if (ret)
				return ret;
			break;
		case 'z':
			ret = kill_cxt_parse_size(optarg, &args->copy_bytes);
			if (ret)
				return ret;
			break;
		case 'l':
			ret = kill_cxt_parse_u32(optarg, &args->loops);
			if (ret || args->loops == 0)
				return -EINVAL;
			break;
		case 'T':
			ret = kill_cxt_parse_u32(optarg, &args->timeout_ms);
			if (ret || args->timeout_ms == 0)
				return -EINVAL;
			break;
		case 'k':
			ret = kill_cxt_parse_u32(optarg, &args->kill_delay_ms);
			if (ret)
				return ret;
			break;
		case 'h':
			kill_cxt_print_usage(argv[0]);
			exit(0);
		default:
			return -EINVAL;
		}
	}

	/* Also accept the historical "-dev <path>" spelling used by samples. */
	optind = 1;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-dev") == 0 && i + 1 < argc) {
			ret = sample_normalize_dev_path(argv[i + 1],
							args->dev_path_buf,
							sizeof(args->dev_path_buf),
							&args->dev_path);
			if (ret)
				return ret;
			break;
		}
	}

	return 0;
}

static int kill_cxt_validate_once(const char *path, const struct kill_cxt_args *args)
{
	int ret = 1;
	sdxi_dev_t *dev = NULL;
	sdxi_cxt_t *cxt = NULL;
	void *src = NULL;
	void *dst = NULL;

	ret = sdxi_dev_open(path, &dev);
	if (ret)
		goto out;

	struct sdxi_cxt_create_opts opts = {
		.size = sizeof(opts),
		.flags = 0,
		.ds_ring_entries = 0,
		.opb_000_enb = 0,
		.eventfd = -1,
	};

	ret = sdxi_cxt_create(dev, &opts, &cxt);
	if (ret)
		goto out;

	ret = kill_cxt_wait_running(cxt, RUNNING_TIMEOUT_MS);
	if (ret)
		goto out;

	posix_memalign_check(&src, TEST_BUFFER_ALIGN, VALIDATE_COPY_BYTES);
	posix_memalign_check(&dst, TEST_BUFFER_ALIGN, VALIDATE_COPY_BYTES);

	memset(src, 0x5A, VALIDATE_COPY_BYTES);
	memset(dst, 0xA5, VALIDATE_COPY_BYTES);

	ret = kill_cxt_submit_sync_copy(cxt, dst, src, VALIDATE_COPY_BYTES,
					args->timeout_ms);
	if (ret)
		goto out;

	ret = memcmp(src, dst, VALIDATE_COPY_BYTES) == 0 ? 0 : -EIO;

out:
	(void)sdxi_cxt_destroy(cxt);
	sdxi_dev_close(dev);
	free(src);
	free(dst);
	return ret;
}

static int kill_cxt_validate_with_retry(const char *path,
					const struct kill_cxt_args *args)
{
	uint64_t start_ns = kill_cxt_now_ns();
	uint64_t deadline_ns = start_ns + (uint64_t)args->timeout_ms * 1000000ull;
	int last_ret;

	for (;;) {
		last_ret = kill_cxt_validate_once(path, args);
		if (last_ret == 0)
			return 0;

		if (kill_cxt_now_ns() >= deadline_ns)
			return last_ret;

		kill_cxt_sleep_ms(VALIDATE_RETRY_MS);
	}
}

static void kill_cxt_child(const char *path, const struct kill_cxt_args *args,
			   int notify_fd)
{
	int ret = 1;
	sdxi_dev_t *dev = NULL;
	sdxi_cxt_t *cxt = NULL;
	void *src = NULL;
	void *dst = NULL;

	ret = sdxi_dev_open(path, &dev);
	if (ret)
		goto err;

	struct sdxi_cxt_create_opts opts = {
		.size = sizeof(opts),
		.flags = 0,
		.ds_ring_entries = 0,
		.opb_000_enb = 0,
		.eventfd = -1,
	};

	ret = sdxi_cxt_create(dev, &opts, &cxt);
	if (ret)
		goto err;

	ret = kill_cxt_wait_running(cxt, RUNNING_TIMEOUT_MS);
	if (ret)
		goto err;

	posix_memalign_check(&src, TEST_BUFFER_ALIGN, args->copy_bytes);
	posix_memalign_check(&dst, TEST_BUFFER_ALIGN, args->copy_bytes);

	memset(src, 0xAF, args->copy_bytes);
	memset(dst, 0xBE, args->copy_bytes);

	ret = kill_cxt_submit_copy_workload(cxt, dst, src, args->copy_bytes);
	if (ret)
		goto err;

	ret = kill_cxt_send_msg(notify_fd, KILL_CXT_MSG_READY, 0, sdxi_cxt_get_fd(cxt),
				sdxi_cxt_pasid(cxt));
	if (ret)
		goto err;

	alarm(CHILD_GUARD_SECONDS);
	for (;;)
		pause();

err:
	(void)kill_cxt_send_msg(notify_fd, KILL_CXT_MSG_ERROR, ret, 0, 0);
	(void)sdxi_cxt_destroy(cxt);
	sdxi_dev_close(dev);
	free(src);
	free(dst);
	close(notify_fd);
	_exit(1);
}

int main(int argc, char *argv[])
{
	int ret = 1;
	struct kill_cxt_args args;
	char **dev_list = NULL;
	size_t dev_count = 0;
	const char *path;

	ret = kill_cxt_parse_args(argc, argv, &args);
	if (ret) {
		kill_cxt_print_usage(argv[0]);
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

	printf("SDXI kill-before-close context cleanup test ...\n");
	printf("  copy-bytes: %zu\n", args.copy_bytes);
	printf("  loops: %u\n", args.loops);
	printf("  timeout-ms: %u\n", args.timeout_ms);
	printf("  kill-delay-ms: %u\n", args.kill_delay_ms);

	for (uint32_t iter = 0; iter < args.loops; iter++) {
		struct kill_cxt_msg msg;
		int sv[2] = {-1, -1};
		pid_t pid;
		int status = 0;

		if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
			perror("socketpair");
			goto out;
		}

		pid = fork();
		if (pid < 0) {
			perror("fork");
			close(sv[0]);
			close(sv[1]);
			goto out;
		}

		if (pid == 0) {
			close(sv[0]);
			kill_cxt_child(path, &args, sv[1]);
		}

		close(sv[1]);
		sv[1] = -1;

		ret = kill_cxt_recv_msg(sv[0], &msg);
		if (ret) {
			fprintf(stderr, "iter %u: failed to receive child status: %s\n",
				iter + 1, sdxi_err_str(ret));
			(void)kill(pid, SIGKILL);
			(void)waitpid(pid, NULL, 0);
			close(sv[0]);
			goto out;
		}

		if (msg.type != KILL_CXT_MSG_READY) {
			fprintf(stderr, "iter %u: child setup failed: %s\n",
				iter + 1, sdxi_err_str(msg.ret));
			(void)waitpid(pid, NULL, 0);
			close(sv[0]);
			ret = msg.ret ? msg.ret : -EIO;
			goto out;
		}

		printf("iter %u/%u: child submitted copy (cxt_fd=%d pasid=%u), killing it\n",
		       iter + 1, args.loops, msg.cxt_fd, msg.pasid);

		if (args.kill_delay_ms)
			kill_cxt_sleep_ms(args.kill_delay_ms);

		if (kill(pid, SIGKILL) < 0 && errno != ESRCH) {
			fprintf(stderr, "iter %u: kill(SIGKILL): %s\n", iter + 1,
				strerror(errno));
			(void)waitpid(pid, NULL, 0);
			close(sv[0]);
			goto out;
		}

		if (waitpid(pid, &status, 0) < 0) {
			fprintf(stderr, "iter %u: waitpid: %s\n", iter + 1,
				strerror(errno));
			close(sv[0]);
			goto out;
		}

		if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGKILL) {
			fprintf(stderr,
				"iter %u: child did not die from SIGKILL (status=0x%x)\n",
				iter + 1, status);
			close(sv[0]);
			ret = -EIO;
			goto out;
		}

		ret = kill_cxt_validate_with_retry(path, &args);
		close(sv[0]);
		if (ret) {
			fprintf(stderr,
				"iter %u: post-kill validation failed: %s\n",
				iter + 1, sdxi_err_str(ret));
			goto out;
		}

		printf("iter %u/%u: validation after forced child exit ==> SUCCESS\n",
		       iter + 1, args.loops);
	}

	fprintf(stderr, "Kill-before-close cleanup ==> SUCCESS\n");
	ret = 0;

out:
	sdxi_free_dev_list(dev_list, dev_count);
	return ret ? 1 : 0;
}
