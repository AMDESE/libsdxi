// SPDX-License-Identifier: BSD-3-Clause
/**
 * DOC: libsdxi base types and export annotations
 *
 * This header contains the declarations shared by all installed libsdxi
 * headers: version macros, public opaque handle types, the common
 * admin-completion POD, the return-code convention, and the export annotation
 * used when libsdxi is built with hidden visibility by default.
 */

#ifndef SDXI_TYPES_H
#define SDXI_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Public libsdxi package version macros. The 0.x series is public but not a
 * long-term ABI freeze; incompatible source or ABI changes before 1.0 must be
 * reflected by these macros and the shared-library versioning policy.
 */
#define SDXI_VERSION_MAJOR 0U
#define SDXI_VERSION_MINOR 1U
#define SDXI_VERSION_PATCH 0U
#define SDXI_VERSION_STRING "0.1.0"

#if defined(__GNUC__) || defined(__clang__)
#define SDXI_EXPORT __attribute__((visibility("default")))
#define SDXI_ALIGNED(x) __attribute__((aligned(x)))
#else
#define SDXI_EXPORT
#define SDXI_ALIGNED(x)
#endif

#ifdef __cplusplus
#define SDXI_BEGIN_DECLS extern "C" {
#define SDXI_END_DECLS }
#else
#define SDXI_BEGIN_DECLS
#define SDXI_END_DECLS
#endif

/*
 * Unless documented otherwise, libsdxi entry points return:
 *   - 0 on success
 *   - -errno on failure
 */

struct sdxi_root;
typedef struct sdxi_root sdxi_root_t;

struct sdxi_dev;
typedef struct sdxi_dev sdxi_dev_t;

struct sdxi_cxt;
typedef struct sdxi_cxt sdxi_cxt_t;

struct sdxi_admin_cpl {
	uint32_t cst_flags;
};

#endif /* SDXI_TYPES_H */
