// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2026 Advanced Micro Devices, Inc.
 *
 * SDXI library sample code - cross-function memory copy (2 processes / 2 funcs)
 *
 * This sample creates two processes (different mm):
 *   - process 1 (source): opens src SDXI function, allocates src buffer
 *   - process 2 (dest):   opens dst SDXI function, allocates dst buffer
 *
 * The src process programs a context AKey entry that targets the dst function.
 * The dst process programs a device RKey entry that authorizes the src function
 * and supplies the PASID used to access the dst buffer.
 *
 * Then the src process submits a DMA COPY descriptor using:
 *   - src akey: local user-memory AKey (default slot 1)
 *   - dst akey: the programmed cross-function AKey slot
 *
 * The dst process verifies that its dst buffer was updated.
 */

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "libsdxi.h"
#include "samples.h"

#define TEST_BUFFER_ALIGN 4096u
#define DEFAULT_SIZE_BYTES 4096u
#define DEFAULT_TIMEOUT_MS 5000u

#define XCOPY_MAGIC 0x58434f50u /* "XCOP" */

enum xcopy_msg_type {
	XCOPY_MSG_P2C_INIT = 1,
	XCOPY_MSG_C2P_INIT = 2,
	XCOPY_MSG_P2C_COPY_DONE = 3,
	XCOPY_MSG_C2P_VERIFY = 4,
};

struct xcopy_msg {
	uint32_t magic;
	uint32_t type;
	uint64_t v[6];
};

struct xcopy_args {
	const char *src_dev_path;
	const char *dst_dev_path;
	char src_dev_path_buf[PATH_MAX];
	char dst_dev_path_buf[PATH_MAX];
	size_t size;
	uint32_t timeout_ms;
};

static int xcopy_write_full(int fd, const void *buf, size_t len)
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

static int xcopy_read_full(int fd, void *buf, size_t len)
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

static int xcopy_send_msg(int fd, uint32_t type, const uint64_t v[6])
{
	struct xcopy_msg msg;

	memset(&msg, 0, sizeof(msg));
	msg.magic = XCOPY_MAGIC;
	msg.type = type;
	if (v)
		memcpy(msg.v, v, sizeof(msg.v));

	return xcopy_write_full(fd, &msg, sizeof(msg));
}

static int xcopy_recv_msg(int fd, struct xcopy_msg *msg_out)
{
	struct xcopy_msg msg;
	int ret;

	ret = xcopy_read_full(fd, &msg, sizeof(msg));
	if (ret)
		return ret;
	if (msg.magic != XCOPY_MAGIC)
		return -EBADMSG;

	*msg_out = msg;
	return 0;
}

static int xcopy_bitmap_test(const struct sdxi_bitmap *bm, uint32_t idx)
{
	uint32_t byte = idx / 8u;
	uint8_t mask = (uint8_t)(1u << (idx % 8u));

	if (!bm || !bm->data)
		return 0;
	if (idx >= bm->nentries)
		return 0;
	if (byte >= bm->bytes)
		return 0;
	return (bm->data[byte] & mask) != 0;
}

static int xcopy_find_free_key_index(const struct sdxi_bitmap *bm,
				     uint32_t start, uint32_t *idx_out)
{
	if (!bm || !bm->data || !idx_out)
		return -EINVAL;
	if (bm->nentries == 0)
		return -ENOSPC;

	for (uint32_t i = start; i < bm->nentries; i++) {
		if (!xcopy_bitmap_test(bm, i)) {
			*idx_out = i;
			return 0;
		}
	}
	return -ENOSPC;
}

static int xcopy_verify_byte(const void *buf, size_t size, uint8_t expected)
{
	const uint8_t *p = buf;

	for (size_t i = 0; i < size; i++) {
		if (p[i] != expected) {
			fprintf(stderr,
				"Verify mismatch at +0x%zx: got=0x%02x expected=0x%02x\n",
				i, p[i], expected);
			return -EIO;
		}
	}
	return 0;
}

static void xcopy_print_usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [options]\n", prog);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  --src-dev <path>     Source SDXI device (e.g., sdxi0, /dev/sdxi0)\n");
	fprintf(stderr, "  --dst-dev <path>     Destination SDXI device (e.g., sdxi1v0, /dev/sdxi1v0)\n");
	fprintf(stderr, "  --size <bytes>       Copy size (default: %u)\n", DEFAULT_SIZE_BYTES);
	fprintf(stderr, "  --timeout <ms>       Timeout (default: %u)\n", DEFAULT_TIMEOUT_MS);
	fprintf(stderr, "  -h, --help           Show this help message\n");
	fprintf(stderr, "\nIf devices are not specified, the first two from `/dev/sdxi*` are used.\n");
}

static int xcopy_parse_u64(const char *s, unsigned long long max,
			   unsigned long long *out)
{
	char *end = NULL;
	unsigned long long v;

	if (!s || !out)
		return -EINVAL;

	errno = 0;
	v = strtoull(s, &end, 0);
	if (errno || end == s || *end != '\0' || v == 0 || v > max)
		return -EINVAL;

	*out = v;
	return 0;
}

static int xcopy_parse_args(int argc, char **argv, struct xcopy_args *args)
{
	static const struct option long_opts[] = {
		{"src-dev", required_argument, 0, 's'},
		{"dst-dev", required_argument, 0, 't'},
		{"size", required_argument, 0, 'z'},
		{"timeout", required_argument, 0, 'T'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0},
	};
	int opt;
	int option_index = 0;

	memset(args, 0, sizeof(*args));
	args->size = DEFAULT_SIZE_BYTES;
	args->timeout_ms = DEFAULT_TIMEOUT_MS;

	/* Reset getopt */
	optind = 1;

	while (1) {
		opt = getopt_long(argc, argv, "s:t:z:T:h", long_opts,
				  &option_index);
		if (opt == -1)
			break;

		switch (opt) {
		case 's':
			if (sample_normalize_dev_path(
				    optarg, args->src_dev_path_buf,
				    sizeof(args->src_dev_path_buf),
				    &args->src_dev_path))
				return -EINVAL;
			break;
		case 't':
			if (sample_normalize_dev_path(
				    optarg, args->dst_dev_path_buf,
				    sizeof(args->dst_dev_path_buf),
				    &args->dst_dev_path))
				return -EINVAL;
			break;
		case 'z': {
			unsigned long long v;

			if (xcopy_parse_u64(optarg, SIZE_MAX, &v))
				return -EINVAL;
			args->size = (size_t)v;
			break;
		}
		case 'T': {
			unsigned long long v;

			if (xcopy_parse_u64(optarg, UINT32_MAX, &v))
				return -EINVAL;
			args->timeout_ms = (uint32_t)v;
			break;
		}
		case 'h':
			xcopy_print_usage(argv[0]);
			exit(0);
		default:
			return -EINVAL;
		}
	}

	return 0;
}

static int xcopy_pick_dev_pair(struct xcopy_args *args, char ***dev_list_out,
			       size_t *dev_count_out)
{
	char **dev_list = NULL;
	size_t dev_count = 0;
	int ret;

	ret = sdxi_list_devs(&dev_list, &dev_count);
	if (ret) {
		fprintf(stderr, "sdxi_list_devs: %s\n", sdxi_err_str(ret));
		return ret;
	}
	if (dev_count < 2) {
		fprintf(stderr, "Need >= 2 SDXI devices for xcopy (found %zu)\n",
			dev_count);
		sdxi_free_dev_list(dev_list, dev_count);
		return -ENODEV;
	}

	if (!args->src_dev_path && !args->dst_dev_path) {
		args->src_dev_path = dev_list[0];
		args->dst_dev_path = dev_list[1];
	} else if (args->src_dev_path && !args->dst_dev_path) {
		for (size_t i = 0; i < dev_count; i++) {
			if (strcmp(dev_list[i], args->src_dev_path) != 0) {
				args->dst_dev_path = dev_list[i];
				break;
			}
		}
	} else if (!args->src_dev_path && args->dst_dev_path) {
		for (size_t i = 0; i < dev_count; i++) {
			if (strcmp(dev_list[i], args->dst_dev_path) != 0) {
				args->src_dev_path = dev_list[i];
				break;
			}
		}
	}

	if (!args->src_dev_path || !args->dst_dev_path) {
		fprintf(stderr, "Unable to select src/dst SDXI devices\n");
		sdxi_free_dev_list(dev_list, dev_count);
		return -ENODEV;
	}
	if (strcmp(args->src_dev_path, args->dst_dev_path) == 0) {
		fprintf(stderr, "src-dev and dst-dev must be different\n");
		sdxi_free_dev_list(dev_list, dev_count);
		return -EINVAL;
	}

	*dev_list_out = dev_list;
	*dev_count_out = dev_count;
	return 0;
}

static int xcopy_open_and_create_cxt(const char *path,
				     sdxi_dev_t **dev_out, sdxi_cxt_t **cxt_out,
				     struct sdxi_dev_caps *caps_out,
				     uint32_t *fn_grp_id_out)
{
	sdxi_dev_t *dev = NULL;
	sdxi_cxt_t *cxt = NULL;
	int ret;

	ret = sdxi_dev_open(path, &dev);
	if (ret) {
		fprintf(stderr, "sdxi_dev_open(%s): %s\n", path, sdxi_err_str(ret));
		return ret;
	}

	if (caps_out) {
		ret = sdxi_dev_get_caps(dev, caps_out);
		if (ret) {
			fprintf(stderr, "sdxi_dev_get_caps(%s): %s\n", path,
				sdxi_err_str(ret));
			goto out;
		}
	}
	if (fn_grp_id_out) {
		uint64_t val = 0;

		ret = sdxi_dev_get_attr(dev, SDXI_DEV_ATTR_ID_GRP_ID, &val);
		if (ret) {
			fprintf(stderr,
				"sdxi_dev_get_attr(%s, GRP_ID): %s\n",
				path, sdxi_err_str(ret));
			goto out;
		}
		*fn_grp_id_out = (uint32_t)val;
	}

	struct sdxi_cxt_create_opts opts = {
		.size = sizeof(opts),
		.flags = 0,
		.ds_ring_entries = 0,
		.opb_000_enb = 0,
		.eventfd = -1,
	};

	ret = sdxi_cxt_create(dev, &opts, &cxt);
	if (ret) {
		fprintf(stderr, "sdxi_cxt_create(%s): %s\n", path, sdxi_err_str(ret));
		goto out;
	}

	*dev_out = dev;
	*cxt_out = cxt;
	return 0;

out:
	sdxi_dev_close(dev);
	return ret;
}

static int xcopy_program_rkey(sdxi_dev_t *dev, const struct sdxi_dev_caps *caps,
			      uint16_t req_sfunc, uint32_t pasid,
			      uint32_t timeout_ms, uint32_t *rkey_idx_out)
{
	struct sdxi_bitmap bm;
	uint32_t idx;
	struct sdxi_key_ent ent;
	int ret;

	memset(&bm, 0, sizeof(bm));
	ret = sdxi_dev_get_rkey_list(dev, &bm);
	if (ret) {
		fprintf(stderr, "sdxi_dev_get_rkey_list: %s\n", sdxi_err_str(ret));
		return ret;
	}

	ret = xcopy_find_free_key_index(&bm, 1, &idx);
	sdxi_bitmap_free(&bm);
	if (ret)
		return ret;
	if (idx > UINT16_MAX)
		return -ERANGE;

	memset(&ent, 0, sizeof(ent));
	ent.index = idx;
	ret = sdxi_rkey_payload_encode(
		&ent.payload, req_sfunc,
		(uint16_t)(SDXI_KEY_CTRL_VL | SDXI_KEY_CTRL_PV), 0, pasid,
		false, 0, 0);
	if (ret)
		return ret;

	{
		struct sdxi_key_set_result kres = {0};

		ret = sdxi_dev_set_rkeys(dev, &ent, 1, timeout_ms, &kres);
		if (ret) {
			fprintf(stderr, "sdxi_dev_set_rkeys(index=%u): %s\n",
				idx, sdxi_err_str(ret));
			return ret;
		}
		if (kres.commit != SDXI_KEY_COMMIT_ALL_E) {
			fprintf(stderr,
				"sdxi_dev_set_rkeys(index=%u): commit=%d first_failed=%u\n",
				idx, (int)kres.commit, kres.first_failed_index);
			return -EIO;
		}
	}

	/* SET_RKEYS already performs the matching hardware update + sync. */

	*rkey_idx_out = idx;
	(void)caps;
	return 0;
}

static int xcopy_program_remote_akey(sdxi_cxt_t *cxt, uint16_t tgt_sfunc,
				     uint16_t rkey_idx, uint32_t *akey_idx_out)
{
	uint32_t idx;
	struct sdxi_key_ent ent;
	int ret;

	ret = sdxi_cxt_alloc_akey_index(cxt, &idx);
	if (ret)
		return ret;
	if (idx > UINT16_MAX)
		return -ERANGE;

	memset(&ent, 0, sizeof(ent));
	ent.index = idx;
	ret = sdxi_akey_payload_encode(&ent.payload, tgt_sfunc,
				       SDXI_KEY_CTRL_VL, 0, 0, false, 0, 0,
				       rkey_idx);
	if (ret)
		return ret;

	ret = sdxi_cxt_set_akey(cxt, &ent);
	if (ret) {
		fprintf(stderr, "sdxi_cxt_set_akey(index=%u): %s\n", idx,
			sdxi_err_str(ret));
		return ret;
	}

	/* SET_AKEYS already performs the matching hardware update + sync. */

	*akey_idx_out = idx;
	return 0;
}

int main(int argc, char *argv[])
{
	struct xcopy_args args;
	char **dev_list = NULL;
	size_t dev_count = 0;
	int ret = 1;
	int sv[2] = {-1, -1};
	pid_t pid;

	ret = xcopy_parse_args(argc, argv, &args);
	if (ret) {
		xcopy_print_usage(argv[0]);
		return 1;
	}

	ret = xcopy_pick_dev_pair(&args, &dev_list, &dev_count);
	if (ret)
		return 1;

	printf("XCOPY:\n");
	printf("  src-dev: %s\n", args.src_dev_path);
	printf("  dst-dev: %s\n", args.dst_dev_path);
	printf("  size: %zu\n", args.size);

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
		perror("socketpair");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		perror("fork");
		goto out;
	}

	if (pid == 0) {
		/* Child: destination function owner */
		sdxi_dev_t *dev = NULL;
		sdxi_cxt_t *cxt = NULL;
		struct sdxi_dev_caps caps;
		uint32_t fn_grp_id = 0;
		struct xcopy_msg msg;
		uint16_t src_sfunc;
		uint32_t rkey_idx = 0;
		int rkey_programmed = 0;
		void *dst = NULL;
		int child_exit = 1;

		close(sv[0]);

		ret = xcopy_recv_msg(sv[1], &msg);
		if (ret) {
			fprintf(stderr, "child: failed to receive init: %s\n",
				sdxi_err_str(ret));
			goto child_out;
		}
		if (msg.type != XCOPY_MSG_P2C_INIT) {
			fprintf(stderr, "child: unexpected message type %u\n",
				msg.type);
			goto child_out;
		}
		src_sfunc = (uint16_t)msg.v[0];
		if (src_sfunc == 0) {
			fprintf(stderr, "child: invalid src sfunc=0\n");
			goto child_out;
		}

		ret = xcopy_open_and_create_cxt(args.dst_dev_path, &dev, &cxt,
						&caps, &fn_grp_id);
		if (ret)
			goto child_out;

		if (!caps.rkey_supported) {
			fprintf(stderr, "child: rkey not supported on %s\n",
				args.dst_dev_path);
			goto child_out;
		}
		if (caps.sfunc == 0) {
			fprintf(stderr, "child: invalid dst sfunc=0 on %s\n",
				args.dst_dev_path);
			goto child_out;
		}

		posix_memalign_check(&dst, TEST_BUFFER_ALIGN, args.size);
		memset(dst, 0xBE, args.size);

		ret = xcopy_program_rkey(dev, &caps, src_sfunc, sdxi_cxt_pasid(cxt),
					 args.timeout_ms, &rkey_idx);
		if (ret)
			goto child_out;
		rkey_programmed = 1;

		uint64_t v[6] = {0};
		v[0] = caps.sfunc;
		v[1] = fn_grp_id;
		v[2] = (uint16_t)rkey_idx;
		v[3] = (uint64_t)(uintptr_t)dst;
		v[4] = sdxi_cxt_pasid(cxt);
		ret = xcopy_send_msg(sv[1], XCOPY_MSG_C2P_INIT, v);
		if (ret)
			goto child_out;

		ret = xcopy_recv_msg(sv[1], &msg);
		if (ret) {
			fprintf(stderr, "child: failed to receive copy-done: %s\n",
				sdxi_err_str(ret));
			goto child_out;
		}
		if (msg.type != XCOPY_MSG_P2C_COPY_DONE) {
			fprintf(stderr, "child: unexpected message type %u\n",
				msg.type);
			goto child_out;
		}
		if ((int32_t)msg.v[0] != 0) {
			fprintf(stderr, "child: copy failed in parent: %s\n",
				sdxi_err_str((int32_t)msg.v[0]));
			goto child_out;
		}

		ret = xcopy_verify_byte(dst, args.size, 0xAF);
		uint64_t resv[6] = {0};
		resv[0] = (ret == 0) ? 1 : 0;
		(void)xcopy_send_msg(sv[1], XCOPY_MSG_C2P_VERIFY, resv);

		child_exit = ret ? 1 : 0;

child_out:
		/* Best-effort: invalidate the RKey slot we programmed. */
		if (rkey_programmed) {
			(void)sdxi_dev_invalidate_rkey(dev, rkey_idx,
						       args.timeout_ms, NULL);
		}

		(void)sdxi_cxt_destroy(cxt);
		sdxi_dev_close(dev);
		free(dst);
		close(sv[1]);
		_exit(child_exit);
	}

	/* Parent: source function owner */
	{
		sdxi_dev_t *dev = NULL;
		sdxi_cxt_t *cxt = NULL;
		struct sdxi_dev_caps caps;
		uint32_t fn_grp_id = 0;
		struct xcopy_msg msg;
		void *src = NULL;
		uint16_t dst_sfunc;
		uint16_t rkey_idx;
		uint64_t dst_addr;
		uint32_t akey_idx = 0;
		int child_status = 1;

		close(sv[1]);

		ret = xcopy_open_and_create_cxt(args.src_dev_path, &dev, &cxt,
						&caps, &fn_grp_id);
		if (ret)
			goto parent_out;

		if (!caps.rkey_supported) {
			fprintf(stderr, "parent: rkey not supported on %s\n",
				args.src_dev_path);
			goto parent_out;
		}
		if (caps.sfunc == 0) {
			fprintf(stderr, "parent: invalid src sfunc=0 on %s\n",
				args.src_dev_path);
			goto parent_out;
		}

		posix_memalign_check(&src, TEST_BUFFER_ALIGN, args.size);
		memset(src, 0xAF, args.size);

		uint64_t v[6] = {0};
		v[0] = caps.sfunc;
		v[1] = fn_grp_id;
		ret = xcopy_send_msg(sv[0], XCOPY_MSG_P2C_INIT, v);
		if (ret)
			goto parent_out;

		ret = xcopy_recv_msg(sv[0], &msg);
		if (ret) {
			fprintf(stderr, "parent: failed to receive dst init: %s\n",
				sdxi_err_str(ret));
			goto parent_out;
		}
		if (msg.type != XCOPY_MSG_C2P_INIT) {
			fprintf(stderr, "parent: unexpected message type %u\n",
				msg.type);
			goto parent_out;
		}

		dst_sfunc = (uint16_t)msg.v[0];
		if (dst_sfunc == 0) {
			fprintf(stderr, "parent: invalid dst sfunc=0\n");
			goto parent_out;
		}
		if ((uint32_t)msg.v[1] != fn_grp_id) {
			fprintf(stderr,
				"parent: function group mismatch (src=0x%08x dst=0x%08" PRIx64 ")\n",
				fn_grp_id, msg.v[1]);
			goto parent_out;
		}
		rkey_idx = (uint16_t)msg.v[2];
		dst_addr = msg.v[3];

		ret = xcopy_program_remote_akey(cxt, dst_sfunc, rkey_idx,
						&akey_idx);
		if (ret)
			goto parent_out;

		printf("src: sfunc=%u grp_id=0x%08x cxt_fd=%d pasid=%u\n",
		       caps.sfunc, fn_grp_id, sdxi_cxt_get_fd(cxt),
		       sdxi_cxt_pasid(cxt));
		printf("dst: sfunc=%u rkey=%u addr=0x%016" PRIx64 "\n",
		       dst_sfunc, rkey_idx, dst_addr);
		printf("xcopy: akey(remote dst)=%u\n", akey_idx);

		ret = sdxi_mem_copy_akeys(cxt, (void *)(uintptr_t)dst_addr,
					  (uint16_t)akey_idx, src, 0x1,
					  args.size, args.timeout_ms);
		if (ret)
			fprintf(stderr, "sdxi_mem_copy_akeys: %s\n", sdxi_err_str(ret));

		uint64_t donev[6] = {0};
		donev[0] = (int32_t)ret;
		(void)xcopy_send_msg(sv[0], XCOPY_MSG_P2C_COPY_DONE, donev);

		if (ret)
			goto parent_out;

		ret = xcopy_recv_msg(sv[0], &msg);
		if (ret) {
			fprintf(stderr, "parent: failed to receive verify: %s\n",
				sdxi_err_str(ret));
			goto parent_out;
		}
		if (msg.type != XCOPY_MSG_C2P_VERIFY) {
			fprintf(stderr, "parent: unexpected message type %u\n",
				msg.type);
			goto parent_out;
		}

		if (msg.v[0] == 1) {
			fprintf(stderr, "XCOPY ==> SUCCESS\n");
			ret = 0;
		} else {
			fprintf(stderr, "XCOPY ==> FAIL\n");
			ret = 1;
		}

parent_out:
		/* Best-effort: invalidate the AKey slot we programmed. */
		if (cxt && akey_idx != 0)
			(void)sdxi_cxt_invalidate_akey(cxt, akey_idx,
						       args.timeout_ms, NULL);
		if (sv[0] >= 0) {
			close(sv[0]);
			sv[0] = -1;
		}
		(void)waitpid(pid, &child_status, 0);
		(void)sdxi_cxt_destroy(cxt);
		sdxi_dev_close(dev);
		free(src);
	}

out:
	if (sv[0] >= 0)
		close(sv[0]);
	if (sv[1] >= 0)
		close(sv[1]);
	sdxi_free_dev_list(dev_list, dev_count);
	return ret ? 1 : 0;
}
