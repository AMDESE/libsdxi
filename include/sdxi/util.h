// SPDX-License-Identifier: BSD-3-Clause
/**
 * DOC: libsdxi utility helpers
 *
 * Small utility helpers that are shared across libsdxi callers and do not
 * belong to a single device/context subsystem.
 */

#ifndef SDXI_UTIL_H
#define SDXI_UTIL_H

#include <sdxi/types.h>

SDXI_BEGIN_DECLS

SDXI_EXPORT const char *sdxi_err_str(int err);
SDXI_EXPORT const char *sdxi_err_name(int err);

SDXI_END_DECLS

#endif /* SDXI_UTIL_H */
