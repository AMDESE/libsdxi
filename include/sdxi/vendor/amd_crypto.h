// SPDX-License-Identifier: BSD-3-Clause
/**
 * DOC: libsdxi AMD crypto vendor dataplane helpers
 *
 * AMD Cryptography Extension helpers cover the S1 AES dataplane operations
 * advertised by the AMDCRYPTO_S1 OpGroup GUID and the S2 AES/SHA dataplane
 * operations advertised by the AMDCRYPTO_S2 OpGroup GUID.
 */

#ifndef SDXI_VENDOR_AMD_CRYPTO_H
#define SDXI_VENDOR_AMD_CRYPTO_H

#include <sdxi/types.h>

struct sdxi_dp_desc;

SDXI_BEGIN_DECLS

#define SDXI_AMD_S1_AES_KEY_ALIGN 64u
#define SDXI_AMD_S1_CRYPTO_STATE_ALIGN 256u
#define SDXI_AMD_S1_CRYPTO_STATE_BYTES 256u

struct sdxi_amd_s1_aes_key {
	uint8_t aes_key[32];
	uint8_t aes_key2[32];
} SDXI_ALIGNED(SDXI_AMD_S1_AES_KEY_ALIGN);

struct sdxi_amd_s1_crypto_state {
	uint32_t ingress_crc;
	uint32_t egress_crc;
	uint32_t aad_byte_cnt;
	uint32_t aes_byte_cnt;
	uint8_t aes_ctr_twk[16];
	uint8_t vendor_rsvd[8];
	uint64_t current_size;
	uint8_t hash_data[208];
} SDXI_ALIGNED(SDXI_AMD_S1_CRYPTO_STATE_ALIGN);

enum sdxi_amd_s1_aes_op {
	SDXI_AMD_S1_AES_OP_ENCRYPT = 0,
	SDXI_AMD_S1_AES_OP_DECRYPT = 1,
};

enum sdxi_amd_s1_aes_key_size {
	SDXI_AMD_S1_AES_KEY_SIZE_128 = 0,
	SDXI_AMD_S1_AES_KEY_SIZE_256 = 1,
};

enum sdxi_amd_s1_xts_block_size {
	SDXI_AMD_S1_XTS_BLOCK_SIZE_512B = 0,
	SDXI_AMD_S1_XTS_BLOCK_SIZE_4K = 1,
	SDXI_AMD_S1_XTS_BLOCK_SIZE_16K = 2,
	SDXI_AMD_S1_XTS_BLOCK_SIZE_INFINITE = 3,
};

struct sdxi_amd_s1_aes_xts_req {
	uint32_t size;

	uint16_t op_type;
	uint8_t aes_op;
	uint8_t aes_key_size;

	uint8_t xts_block_size;
	uint8_t attr_src;
	uint8_t attr_dst;
	uint8_t rsvd0;
	uint16_t akey_src;
	uint16_t akey_dst;

	const void *src;
	void *dst;
	size_t nbytes;

	const struct sdxi_amd_s1_aes_key *key;
	struct sdxi_amd_s1_crypto_state *state;

	uint32_t rsvd[8];
};

struct sdxi_amd_s1_aes_ecb_req {
	uint32_t size;

	uint16_t op_type;
	uint8_t aes_op;
	uint8_t aes_key_size;

	uint8_t attr_src;
	uint8_t attr_dst;
	uint16_t akey_src;
	uint16_t akey_dst;
	uint16_t rsvd0;

	const void *src;
	void *dst;
	size_t nbytes;

	const struct sdxi_amd_s1_aes_key *key;
	struct sdxi_amd_s1_crypto_state *state;

	uint32_t rsvd[8];
};

SDXI_EXPORT int sdxi_dp_desc_build_amd_s1_aes_xts(
	struct sdxi_dp_desc *out, const struct sdxi_amd_s1_aes_xts_req *req);
SDXI_EXPORT int sdxi_dp_desc_build_amd_s1_aes_ecb(
	struct sdxi_dp_desc *out, const struct sdxi_amd_s1_aes_ecb_req *req);

SDXI_EXPORT int sdxi_amd_s1_aes_xts(
	sdxi_cxt_t *cxt, const struct sdxi_amd_s1_aes_xts_req *req,
	uint32_t timeout_ms);
SDXI_EXPORT int sdxi_amd_s1_aes_ecb(
	sdxi_cxt_t *cxt, const struct sdxi_amd_s1_aes_ecb_req *req,
	uint32_t timeout_ms);

#define SDXI_AMD_S2_AES_KEY_ALIGN 64u
#define SDXI_AMD_S2_CRYPTO_STATE_ALIGN 256u
#define SDXI_AMD_S2_CRYPTO_STATE_BYTES 256u
#define SDXI_AMD_S2_GCM_TAG_BYTES 16u

struct sdxi_amd_s2_aes_key {
	uint8_t aes_key[32];
	uint8_t aes_key2[32];
} SDXI_ALIGNED(SDXI_AMD_S2_AES_KEY_ALIGN);

struct sdxi_amd_s2_crypto_state {
	uint32_t ingress_crc;
	uint32_t egress_crc;
	uint32_t aad_byte_cnt;
	uint32_t aes_byte_cnt;
	uint8_t aes_ctr_twk[16];
	uint8_t vendor_rsvd[8];
	uint64_t current_size;
	uint8_t hash_data[208];
} SDXI_ALIGNED(SDXI_AMD_S2_CRYPTO_STATE_ALIGN);

enum sdxi_amd_s2_aes_op {
	SDXI_AMD_S2_AES_OP_ENCRYPT = 0,
	SDXI_AMD_S2_AES_OP_DECRYPT = 1,
};

enum sdxi_amd_s2_aes_key_size {
	SDXI_AMD_S2_AES_KEY_SIZE_128 = 0,
	SDXI_AMD_S2_AES_KEY_SIZE_256 = 1,
};

enum sdxi_amd_s2_xts_block_size {
	SDXI_AMD_S2_XTS_BLOCK_SIZE_512B = 0,
	SDXI_AMD_S2_XTS_BLOCK_SIZE_4K = 1,
	SDXI_AMD_S2_XTS_BLOCK_SIZE_16K = 2,
	SDXI_AMD_S2_XTS_BLOCK_SIZE_INFINITE = 3,
};

enum sdxi_amd_s2_aes_ctr_iv_size {
	SDXI_AMD_S2_AES_CTR_IV_SIZE_12B = 0,
	SDXI_AMD_S2_AES_CTR_IV_SIZE_16B = 1,
};

enum sdxi_amd_s2_sha_mode {
	SDXI_AMD_S2_SHA_MODE_SHA2_224 = 0x0,
	SDXI_AMD_S2_SHA_MODE_SHA2_256 = 0x1,
	SDXI_AMD_S2_SHA_MODE_SHA2_384 = 0x2,
	SDXI_AMD_S2_SHA_MODE_SHA2_512 = 0x3,
	SDXI_AMD_S2_SHA_MODE_SHA3_224 = 0x4,
	SDXI_AMD_S2_SHA_MODE_SHA3_256 = 0x5,
	SDXI_AMD_S2_SHA_MODE_SHA3_384 = 0x6,
	SDXI_AMD_S2_SHA_MODE_SHA3_512 = 0x7,
	SDXI_AMD_S2_SHA_MODE_SHAKE128 = 0x8,
	SDXI_AMD_S2_SHA_MODE_SHAKE256 = 0x9,
};

enum sdxi_amd_s2_sha_src {
	SDXI_AMD_S2_SHA_SRC_INGRESS = 0,
	SDXI_AMD_S2_SHA_SRC_EGRESS = 1,
};

struct sdxi_amd_s2_sha_req {
	uint32_t size;

	uint16_t op_type;
	uint8_t sha_mode;
	uint8_t sha_src;

	uint8_t finalize;
	uint8_t zero_byte_req;
	uint8_t ingress_crc;
	uint8_t rsvd0;

	uint8_t attr_src;
	uint8_t rsvd1;
	uint16_t akey_src;

	const void *src;
	size_t nbytes;

	struct sdxi_amd_s2_crypto_state *state;

	uint32_t rsvd[8];
};

struct sdxi_amd_s2_sha_copy_req {
	uint32_t size;

	uint16_t op_type;
	uint8_t sha_mode;
	uint8_t finalize;

	uint8_t zero_byte_req;
	uint8_t ingress_crc;
	uint8_t egress_crc;
	uint8_t rsvd0;
	uint8_t attr_src;
	uint8_t attr_dst;
	uint16_t akey_src;
	uint16_t akey_dst;

	const void *src;
	void *dst;
	size_t nbytes;

	struct sdxi_amd_s2_crypto_state *state;

	uint32_t rsvd[8];
};

struct sdxi_amd_s2_aes_xts_req {
	uint32_t size;

	uint16_t op_type;
	uint8_t aes_op;
	uint8_t aes_key_size;

	uint8_t xts_block_size;
	uint8_t ingress_crc;
	uint8_t egress_crc;
	uint8_t rsvd0;
	uint8_t attr_src;
	uint8_t attr_dst;
	uint16_t akey_src;
	uint16_t akey_dst;
	uint16_t rsvd1;

	const void *src;
	void *dst;
	size_t nbytes;

	const struct sdxi_amd_s2_aes_key *key;
	struct sdxi_amd_s2_crypto_state *state;

	uint32_t rsvd[8];
};

struct sdxi_amd_s2_aes_ecb_req {
	uint32_t size;

	uint16_t op_type;
	uint8_t aes_op;
	uint8_t aes_key_size;

	uint8_t ingress_crc;
	uint8_t egress_crc;
	uint8_t attr_src;
	uint8_t attr_dst;
	uint16_t akey_src;
	uint16_t akey_dst;

	const void *src;
	void *dst;
	size_t nbytes;

	const struct sdxi_amd_s2_aes_key *key;
	struct sdxi_amd_s2_crypto_state *state;

	uint32_t rsvd[8];
};

struct sdxi_amd_s2_aes_gcm_req {
	uint32_t size;

	uint16_t op_type;
	uint8_t aes_op;
	uint8_t aes_key_size;

	uint8_t gcm_aad;
	uint8_t gcm_last_aad;
	uint8_t finalize;
	uint8_t zero_byte_req;

	uint8_t ingress_crc;
	uint8_t egress_crc;
	uint8_t attr_src;
	uint8_t attr_dst;
	uint16_t akey_src;
	uint16_t akey_dst;

	const void *src;
	void *dst;
	size_t nbytes;

	const struct sdxi_amd_s2_aes_key *key;
	struct sdxi_amd_s2_crypto_state *state;

	uint32_t rsvd[8];
};

struct sdxi_amd_s2_aes_ctr_req {
	uint32_t size;

	uint16_t op_type;
	uint8_t aes_op;
	uint8_t aes_key_size;

	uint8_t ctr_iv_size;
	uint8_t ingress_crc;
	uint8_t egress_crc;
	uint8_t rsvd0;
	uint8_t attr_src;
	uint8_t attr_dst;
	uint16_t akey_src;
	uint16_t akey_dst;
	uint16_t rsvd1;

	const void *src;
	void *dst;
	size_t nbytes;

	const struct sdxi_amd_s2_aes_key *key;
	struct sdxi_amd_s2_crypto_state *state;

	uint32_t rsvd[8];
};

struct sdxi_amd_s2_aes_ctr_sha_req {
	uint32_t size;

	uint16_t op_type;
	uint8_t aes_op;
	uint8_t aes_key_size;

	uint8_t ctr_iv_size;
	uint8_t sha_mode;
	uint8_t sha_src;
	uint8_t finalize;

	uint8_t zero_byte_req;
	uint8_t ingress_crc;
	uint8_t egress_crc;
	uint8_t attr_src;
	uint8_t attr_dst;
	uint8_t rsvd0;
	uint16_t akey_src;
	uint16_t akey_dst;
	uint16_t rsvd1;

	const void *src;
	void *dst;
	size_t nbytes;

	const struct sdxi_amd_s2_aes_key *key;
	struct sdxi_amd_s2_crypto_state *state;

	uint32_t rsvd[8];
};

struct sdxi_amd_s2_aes_xts_sha_req {
	uint32_t size;

	uint16_t op_type;
	uint8_t aes_op;
	uint8_t aes_key_size;

	uint8_t xts_block_size;
	uint8_t sha_mode;
	uint8_t sha_src;
	uint8_t finalize;

	uint8_t zero_byte_req;
	uint8_t ingress_crc;
	uint8_t egress_crc;
	uint8_t attr_src;
	uint8_t attr_dst;
	uint8_t rsvd0;
	uint16_t akey_src;
	uint16_t akey_dst;
	uint16_t rsvd1;

	const void *src;
	void *dst;
	size_t nbytes;

	const struct sdxi_amd_s2_aes_key *key;
	struct sdxi_amd_s2_crypto_state *state;

	uint32_t rsvd[8];
};

SDXI_EXPORT int sdxi_dp_desc_build_amd_s2_sha(
	struct sdxi_dp_desc *out, const struct sdxi_amd_s2_sha_req *req);
SDXI_EXPORT int sdxi_dp_desc_build_amd_s2_sha_copy(
	struct sdxi_dp_desc *out, const struct sdxi_amd_s2_sha_copy_req *req);
SDXI_EXPORT int sdxi_dp_desc_build_amd_s2_aes_xts_c(
	struct sdxi_dp_desc *out, const struct sdxi_amd_s2_aes_xts_req *req);
SDXI_EXPORT int sdxi_dp_desc_build_amd_s2_aes_ecb_c(
	struct sdxi_dp_desc *out, const struct sdxi_amd_s2_aes_ecb_req *req);
SDXI_EXPORT int sdxi_dp_desc_build_amd_s2_aes_gcm(
	struct sdxi_dp_desc *out, const struct sdxi_amd_s2_aes_gcm_req *req);
SDXI_EXPORT int sdxi_dp_desc_build_amd_s2_aes_ctr(
	struct sdxi_dp_desc *out, const struct sdxi_amd_s2_aes_ctr_req *req);
SDXI_EXPORT int sdxi_dp_desc_build_amd_s2_aes_ctr_sha(
	struct sdxi_dp_desc *out,
	const struct sdxi_amd_s2_aes_ctr_sha_req *req);
SDXI_EXPORT int sdxi_dp_desc_build_amd_s2_aes_xts_sha(
	struct sdxi_dp_desc *out,
	const struct sdxi_amd_s2_aes_xts_sha_req *req);

SDXI_EXPORT int sdxi_amd_s2_sha(sdxi_cxt_t *cxt,
				const struct sdxi_amd_s2_sha_req *req,
				uint32_t timeout_ms);
SDXI_EXPORT int sdxi_amd_s2_sha_copy(
	sdxi_cxt_t *cxt, const struct sdxi_amd_s2_sha_copy_req *req,
	uint32_t timeout_ms);
SDXI_EXPORT int sdxi_amd_s2_aes_xts_c(
	sdxi_cxt_t *cxt, const struct sdxi_amd_s2_aes_xts_req *req,
	uint32_t timeout_ms);
SDXI_EXPORT int sdxi_amd_s2_aes_ecb_c(
	sdxi_cxt_t *cxt, const struct sdxi_amd_s2_aes_ecb_req *req,
	uint32_t timeout_ms);
SDXI_EXPORT int sdxi_amd_s2_aes_gcm(
	sdxi_cxt_t *cxt, const struct sdxi_amd_s2_aes_gcm_req *req,
	uint32_t timeout_ms);
SDXI_EXPORT int sdxi_amd_s2_aes_ctr(
	sdxi_cxt_t *cxt, const struct sdxi_amd_s2_aes_ctr_req *req,
	uint32_t timeout_ms);
SDXI_EXPORT int sdxi_amd_s2_aes_ctr_sha(
	sdxi_cxt_t *cxt, const struct sdxi_amd_s2_aes_ctr_sha_req *req,
	uint32_t timeout_ms);
SDXI_EXPORT int sdxi_amd_s2_aes_xts_sha(
	sdxi_cxt_t *cxt, const struct sdxi_amd_s2_aes_xts_sha_req *req,
	uint32_t timeout_ms);

SDXI_END_DECLS

#endif /* SDXI_VENDOR_AMD_CRYPTO_H */
