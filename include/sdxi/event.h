// SPDX-License-Identifier: BSD-3-Clause
/**
 * DOC: libsdxi event helpers
 *
 * Event helpers cover context-side eventfd/introspection helpers for already
 * created contexts.
 */

#ifndef SDXI_EVENT_H
#define SDXI_EVENT_H

#include <sdxi/types.h>

SDXI_BEGIN_DECLS

SDXI_EXPORT int sdxi_cxt_get_eventfd(const sdxi_cxt_t *cxt);
SDXI_EXPORT int sdxi_cxt_get_intr_info(const sdxi_cxt_t *cxt,
				       uint16_t *intr_akey_out,
				       uint32_t *intr_num_out);

SDXI_END_DECLS

#endif /* SDXI_EVENT_H */
