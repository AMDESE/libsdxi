// SPDX-License-Identifier: BSD-3-Clause
/**
 * DOC: libsdxi advanced local dataplane API
 *
 * Dataplane helpers expose the current local-only raw context accessors, a
 * byte-exact descriptor/completion-builder surface, and the higher-level
 * submission helpers built on top of that local state.
 *
 * The raw context fd, ring/status/write-index mappings, and doorbell belong
 * here because they are explicit advanced dataplane details. They are not
 * transport-neutral and do not define a cross-backend contract.
 *
 * Compiler-dependent internal descriptor bitfields remain private to the
 * library. The installed API exposes byte-exact descriptor buffers and POD
 * request structs instead. Vendor-specific typed dataplane helpers live in
 * explicit headers under `sdxi/vendor/`.
 */

#ifndef SDXI_DP_H
#define SDXI_DP_H

#include <sdxi/types.h>

SDXI_BEGIN_DECLS

struct sdxi_cxt_mmaps {
	void *ds_ring;
	size_t ds_ring_bytes;

	void *cxt_sts;
	size_t cxt_sts_bytes;

	void *write_index;
	size_t write_index_bytes;

	void *doorbell;
	size_t doorbell_bytes;
};

SDXI_EXPORT int sdxi_cxt_get_fd(const sdxi_cxt_t *cxt);
SDXI_EXPORT int sdxi_cxt_get_mmaps(const sdxi_cxt_t *cxt,
				   struct sdxi_cxt_mmaps *out);

#define SDXI_DP_DESC_BYTES 64u
#define SDXI_DP_DESC_ALIGN 64u
#define SDXI_DP_CSB_BYTES 32u
#define SDXI_DP_CSB_ALIGN 32u
#define SDXI_DP_WRITE_IMM_MAX_BYTES 32u

#if defined(__GNUC__) || defined(__clang__)
#define SDXI_DP_ALIGNED(x) __attribute__((aligned(x)))
#else
#define SDXI_DP_ALIGNED(x)
#endif

struct sdxi_dp_desc {
	uint8_t bytes[SDXI_DP_DESC_BYTES];
} SDXI_DP_ALIGNED(SDXI_DP_DESC_ALIGN);

#define SDXI_DP_CSB_F_ERROR (1U << 31)
#define SDXI_DP_RESERVE_F_MULTI_PRODUCER (1U << 0)

struct sdxi_dp_csb {
	uint64_t signal;
	uint32_t flags;
	uint8_t rsvd0[20];
} SDXI_DP_ALIGNED(SDXI_DP_CSB_ALIGN);

struct sdxi_dp_reserve_args {
	uint32_t size;
	uint32_t flags;
	uint32_t ndesc;
	uint32_t timeout_ms;
	uint32_t rsvd[8];
};

struct sdxi_dp_reservation {
	uint32_t size;
	uint32_t flags;
	uint64_t start;
	uint32_t ndesc;
	uint32_t rsvd0;
	uint32_t rsvd[8];
};

SDXI_EXPORT void sdxi_dp_csb_clear(struct sdxi_dp_csb *csb);
SDXI_EXPORT uint64_t sdxi_dp_csb_signal(const struct sdxi_dp_csb *csb);
SDXI_EXPORT uint32_t sdxi_dp_csb_flags(const struct sdxi_dp_csb *csb);
SDXI_EXPORT bool sdxi_dp_csb_done(const struct sdxi_dp_csb *csb);
SDXI_EXPORT bool sdxi_dp_csb_error(const struct sdxi_dp_csb *csb);
SDXI_EXPORT int sdxi_dp_reserve(sdxi_cxt_t *cxt,
				const struct sdxi_dp_reserve_args *args,
				struct sdxi_dp_reservation *out);
SDXI_EXPORT int sdxi_dp_commit(sdxi_cxt_t *cxt,
			       const struct sdxi_dp_reservation *resv,
			       uint32_t timeout_ms, uint64_t *write_idx_out);
SDXI_EXPORT int sdxi_dp_desc_store(sdxi_cxt_t *cxt, uint64_t pos,
				   const struct sdxi_dp_desc *desc);
SDXI_EXPORT int sdxi_dp_wait_read_idx(sdxi_cxt_t *cxt, uint64_t target,
				      uint32_t timeout_ms);
SDXI_EXPORT int sdxi_dp_wait_read_idx_poll(sdxi_cxt_t *cxt, uint64_t target,
					   uint32_t timeout_ms);
SDXI_EXPORT int sdxi_dp_wait_csb(sdxi_cxt_t *cxt,
				 const struct sdxi_dp_csb *csb,
				 uint32_t timeout_ms);
SDXI_EXPORT int sdxi_dp_wait_csb_poll(sdxi_cxt_t *cxt,
				      const struct sdxi_dp_csb *csb,
				      uint32_t timeout_ms);
SDXI_EXPORT int sdxi_dp_desc_set_csb(struct sdxi_dp_desc *desc,
				     struct sdxi_dp_csb *csb);
SDXI_EXPORT int sdxi_dp_desc_clear_csb(struct sdxi_dp_desc *desc);
SDXI_EXPORT int sdxi_dp_desc_set_read_barrier(struct sdxi_dp_desc *desc,
					      bool enable);

struct sdxi_dp_dma_copy_req {
	uint32_t size;

	uint8_t attr_src;
	uint8_t attr_dst;
	uint16_t src_akey;
	uint16_t dst_akey;
	uint16_t rsvd0;

	uint64_t addr_src;
	uint64_t addr_dst;
	uint32_t nbytes;

	uint32_t rsvd[8];
};

struct sdxi_dp_dma_rep_copy_req {
	uint32_t size;

	uint8_t attr_src;
	uint8_t attr_dst;
	uint16_t src_akey;
	uint16_t dst_akey;
	uint16_t rsvd0;

	uint64_t addr_src;
	uint64_t addr_dst;
	uint32_t size_4k;
	uint32_t repeat;

	uint32_t rsvd[8];
};

struct sdxi_dp_dma_write_imm_req {
	uint32_t size;

	uint8_t attr_dst;
	uint8_t rsvd0[3];
	uint16_t dst_akey;
	uint16_t rsvd1;

	uint64_t addr_dst;
	uint32_t data_bytes;
	uint8_t data[SDXI_DP_WRITE_IMM_MAX_BYTES];

	uint32_t rsvd[8];
};

struct sdxi_dp_atomic_uadd_req {
	uint32_t size;

	uint8_t attr_dst;
	uint8_t operand_bytes;
	uint16_t dst_akey;

	uint64_t addr_dst;
	uint64_t operand;
	uint64_t ret_data_addr;

	uint32_t rsvd[8];
};

struct sdxi_dp_intr_req {
	uint32_t size;

	uint16_t akey;
	uint16_t rsvd0;

	uint32_t rsvd[8];
};

enum sdxi_crc_m1_subtype {
	SDXI_CRC_M1_SUBTYPE_COPY = 0x0,
	SDXI_CRC_M1_SUBTYPE_CALC = 0x1,
	SDXI_CRC_M1_SUBTYPE_COPY_BLK = 0x2,
	SDXI_CRC_M1_SUBTYPE_CALC_BLK = 0x3,
};

enum sdxi_crc_m1_blksz {
	SDXI_CRC_M1_BLKSZ_512B_NO_MD = 0x0,
	SDXI_CRC_M1_BLKSZ_512B_8B_MD = 0x1,
	SDXI_CRC_M1_BLKSZ_4096B_NO_MD = 0x2,
	SDXI_CRC_M1_BLKSZ_4096B_16B_MD = 0x3,
	SDXI_CRC_M1_BLKSZ_4096B_8B_MD = 0x4,
};

enum sdxi_crc_m1_seedloc {
	SDXI_CRC_M1_SEEDLOC_COMMON = 0,
	SDXI_CRC_M1_SEEDLOC_PTR = 1,
};

enum sdxi_crc_m1_crcsel {
	SDXI_CRC_M1_CRCSEL_CRC16_8BB7 = 0x0,
	SDXI_CRC_M1_CRCSEL_CRC32C_1EDC6F41 = 0x1,
	SDXI_CRC_M1_CRCSEL_CRC32_04C11DB7 = 0x2,
	SDXI_CRC_M1_CRCSEL_CRC64_AD93D23594C93659 = 0x3,
};

enum sdxi_crc_m1_reflect {
	SDXI_CRC_M1_REFLECT_NONE = 0x0,
	SDXI_CRC_M1_REFLECT_DATA = 0x1,
	SDXI_CRC_M1_REFLECT_SEED_CRC = 0x2,
	SDXI_CRC_M1_REFLECT_DATA_SEED_CRC = 0x3,
};

enum sdxi_crc_m1_invert {
	SDXI_CRC_M1_INVERT_NONE = 0x0,
	SDXI_CRC_M1_INVERT_SEED = 0x1,
	SDXI_CRC_M1_INVERT_CRC = 0x2,
	SDXI_CRC_M1_INVERT_SEED_CRC = 0x3,
};

struct sdxi_crc_m1_req {
	uint32_t size;

	uint16_t op_type;
	uint8_t sub_type;

	uint8_t blksz;
	uint8_t meta_src;
	uint8_t meta_dst;

	uint8_t seed_loc;
	uint8_t crcsel;
	uint8_t reflect;
	uint8_t invert;

	uint8_t attr_src;
	uint8_t attr_dst;
	uint16_t akey_src;
	uint16_t akey_dst;

	const void *src;
	void *dst;
	size_t nbytes;

	uint64_t seed;

	void *crc_dst;
	size_t crc_dst_bytes;

	uint32_t rsvd[8];
};

SDXI_EXPORT int sdxi_crc_m1(sdxi_cxt_t *cxt,
			    const struct sdxi_crc_m1_req *req,
			    uint32_t timeout_ms);

SDXI_EXPORT int sdxi_dp_desc_build_dma_nop(struct sdxi_dp_desc *out);
SDXI_EXPORT int sdxi_dp_desc_build_dma_copy(
	struct sdxi_dp_desc *out, const struct sdxi_dp_dma_copy_req *req);
SDXI_EXPORT int sdxi_dp_desc_build_dma_rep_copy(
	struct sdxi_dp_desc *out, const struct sdxi_dp_dma_rep_copy_req *req);
SDXI_EXPORT int sdxi_dp_desc_build_dma_write_imm(
	struct sdxi_dp_desc *out, const struct sdxi_dp_dma_write_imm_req *req);
SDXI_EXPORT int sdxi_dp_desc_build_atomic_uadd(
	struct sdxi_dp_desc *out, const struct sdxi_dp_atomic_uadd_req *req);
SDXI_EXPORT int sdxi_dp_desc_build_intr(
	struct sdxi_dp_desc *out, const struct sdxi_dp_intr_req *req);
SDXI_EXPORT int sdxi_dp_desc_build_crc_m1(
	struct sdxi_dp_desc *out, const struct sdxi_crc_m1_req *req);

/*
 * These helpers are layered above the explicit byte-exact builders. After
 * context setup, the current high-level dataplane hot path does not perform
 * hidden heap allocation.
 */
SDXI_EXPORT int sdxi_nop(sdxi_cxt_t *cxt, uint32_t timeout_ms);
SDXI_EXPORT int sdxi_mem_copy(sdxi_cxt_t *cxt, void *dst, const void *src,
			      size_t size, uint32_t timeout_ms);
SDXI_EXPORT int sdxi_mem_copy_akeys(sdxi_cxt_t *cxt, void *dst,
				    uint16_t dst_akey, const void *src,
				    uint16_t src_akey, size_t size,
				    uint32_t timeout_ms);
SDXI_EXPORT int sdxi_mem_rep_copy(sdxi_cxt_t *cxt, void *dst, const void *src,
				  size_t src_size, size_t repeat,
				  uint32_t timeout_ms);
SDXI_EXPORT int sdxi_write_imm(sdxi_cxt_t *cxt, void *dst, const void *data,
			       size_t size, uint32_t timeout_ms);
SDXI_EXPORT int sdxi_atomic_uadd(sdxi_cxt_t *cxt, void *addr,
				 uint64_t operand, uint32_t timeout_ms);
SDXI_EXPORT int sdxi_intr(sdxi_cxt_t *cxt, uint32_t timeout_ms);

#undef SDXI_DP_ALIGNED

SDXI_END_DECLS

#endif /* SDXI_DP_H */
