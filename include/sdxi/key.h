// SPDX-License-Identifier: BSD-3-Clause
/**
 * DOC: libsdxi key-management API
 *
 * Key-management helpers share one userspace model across device-scoped
 * RKeys and context-scoped AKeys, including a raw payload type, pure
 * userspace payload encoders, validity bitmaps, get/set helpers, free-index
 * helpers, invalidation helpers, and shared commit-progress reporting.
 *
 * The installed encoder helpers live in this key-specific header because they
 * operate only on the public key-table payload layout. They do not expose raw
 * dataplane descriptor/ring details and they do not bake in driver-private
 * slot or privilege policy.
 */

#ifndef SDXI_KEY_H
#define SDXI_KEY_H

#include <sdxi/types.h>

SDXI_BEGIN_DECLS

#define SDXI_KEY_PAYLOAD_DWORDS 4U
#define SDXI_KEY_PAYLOAD_BYTES (SDXI_KEY_PAYLOAD_DWORDS * sizeof(uint32_t))

struct sdxi_key_payload {
	uint32_t dword[SDXI_KEY_PAYLOAD_DWORDS];
};

struct sdxi_key_ent {
	uint32_t index;
	struct sdxi_key_payload payload;
};

enum sdxi_key_ctrl {
	SDXI_KEY_CTRL_VL = 1U << 0,
	SDXI_KEY_CTRL_IV = 1U << 1,
	SDXI_KEY_CTRL_PV = 1U << 2,
	SDXI_KEY_CTRL_STE = 1U << 3,
};

SDXI_EXPORT void sdxi_key_payload_clear(struct sdxi_key_payload *payload);
SDXI_EXPORT int sdxi_rkey_payload_encode(struct sdxi_key_payload *payload,
					 uint16_t req_sfunc, uint16_t ctrl,
					 uint16_t intr_num, uint32_t pasid,
					 bool pr, uint8_t ph, uint16_t stag);
SDXI_EXPORT int sdxi_akey_payload_encode(struct sdxi_key_payload *payload,
					 uint16_t tgt_sfunc, uint16_t ctrl,
					 uint16_t intr_num, uint32_t pasid,
					 bool pr, uint8_t ph, uint16_t stag,
					 uint16_t rkey);

struct sdxi_bitmap {
	uint32_t nvalid;
	uint32_t nentries;
	size_t bytes;
	uint8_t *data;
};

SDXI_EXPORT void sdxi_bitmap_free(struct sdxi_bitmap *bm);

#define SDXI_KEY_TIMEOUT_DEFAULT 0U

enum sdxi_key_commit {
	SDXI_KEY_COMMIT_NONE_E = 0,
	SDXI_KEY_COMMIT_ALL_E = 1,
	SDXI_KEY_COMMIT_PARTIAL_E = 2,
};

struct sdxi_key_set_result {
	enum sdxi_key_commit commit;
	uint32_t first_failed_index;
	uint32_t cst_flags;
};

SDXI_EXPORT int sdxi_dev_get_rkey_list(sdxi_dev_t *dev,
				       struct sdxi_bitmap *out);
SDXI_EXPORT int sdxi_dev_get_rkeys(sdxi_dev_t *dev,
				   struct sdxi_key_ent *ents, size_t nents);
SDXI_EXPORT int sdxi_dev_set_rkeys(sdxi_dev_t *dev,
				   const struct sdxi_key_ent *ents,
				   size_t nents, uint32_t timeout_ms,
				   struct sdxi_key_set_result *res);
SDXI_EXPORT int sdxi_dev_get_rkey(sdxi_dev_t *dev, uint32_t index,
				  struct sdxi_key_ent *ent_out);
SDXI_EXPORT int sdxi_dev_set_rkey(sdxi_dev_t *dev,
				  const struct sdxi_key_ent *ent);
SDXI_EXPORT int sdxi_dev_set_rkeys_resume(sdxi_dev_t *dev,
					  const struct sdxi_key_ent *ents,
					  size_t nents, uint32_t timeout_ms,
					  struct sdxi_key_set_result *res);
SDXI_EXPORT int sdxi_dev_alloc_rkey_index(sdxi_dev_t *dev,
					  uint32_t *index_out);
SDXI_EXPORT int sdxi_dev_invalidate_rkey(sdxi_dev_t *dev, uint32_t index,
					 uint32_t timeout_ms,
					 struct sdxi_key_set_result *res);

SDXI_EXPORT int sdxi_cxt_get_akey_list(sdxi_cxt_t *cxt,
				       struct sdxi_bitmap *out);
SDXI_EXPORT int sdxi_cxt_get_akeys(sdxi_cxt_t *cxt,
				   struct sdxi_key_ent *ents, size_t nents);
SDXI_EXPORT int sdxi_cxt_set_akeys(sdxi_cxt_t *cxt,
				   const struct sdxi_key_ent *ents,
				   size_t nents, uint32_t timeout_ms,
				   struct sdxi_key_set_result *res);
SDXI_EXPORT int sdxi_cxt_get_akey(sdxi_cxt_t *cxt, uint32_t index,
				  struct sdxi_key_ent *ent_out);
SDXI_EXPORT int sdxi_cxt_set_akey(sdxi_cxt_t *cxt,
				  const struct sdxi_key_ent *ent);
SDXI_EXPORT int sdxi_cxt_set_akeys_resume(sdxi_cxt_t *cxt,
					  const struct sdxi_key_ent *ents,
					  size_t nents, uint32_t timeout_ms,
					  struct sdxi_key_set_result *res);
SDXI_EXPORT int sdxi_cxt_alloc_akey_index(sdxi_cxt_t *cxt,
					  uint32_t *index_out);
SDXI_EXPORT int sdxi_cxt_invalidate_akey(sdxi_cxt_t *cxt, uint32_t index,
					 uint32_t timeout_ms,
					 struct sdxi_key_set_result *res);

SDXI_END_DECLS

#endif /* SDXI_KEY_H */
