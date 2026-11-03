/* Copyright (c) 2026 noobpwnftw.
 * SPDX-License-Identifier: BSD-2-Clause
 */
/* Experimental point-access API for this RFC prototype. */
#ifndef LZ4POINT_DRAFT_H
#define LZ4POINT_DRAFT_H
#include <stddef.h>
#include "lz4.h"
#if defined(__cplusplus)
extern "C" {
#endif

typedef struct LZ4_pointIndex_s LZ4_pointIndex;

/* Returns required caller-owned workspace bytes, or 0 for an invalid bound.
 * The workspace must have malloc-compatible alignment and remain alive until
 * no thread is reading the index. */
LZ4LIB_API size_t LZ4_pointIndexBound(int maxDecodedSize);
LZ4LIB_API LZ4_pointIndex* LZ4_pointIndexInit(void* workspace, size_t workspaceSize,
                                              int maxDecodedSize);

/* Build a bounds-checked index over a raw LZ4 block. src and dict are borrowed:
 * they must remain alive and unchanged until the index is no longer used, and
 * must not overlap the index workspace.
 * @return decoded size (>= 0), or a negative value on malformed input,
 *         insufficient workspace, or an invalid argument. */
LZ4LIB_API int LZ4_pointIndexBuild(LZ4_pointIndex* index, const void* src, int srcSize,
                                   const void* dict, int dictSize);

/* Read one byte: returns 0..255, or a negative value for invalid arguments or
 * an out-of-range offset. Concurrent reads from a completed index are safe.
 * Wait for all readers before rebuilding, reinitializing or freeing workspace. */
LZ4LIB_API int LZ4_pointRead(const LZ4_pointIndex* index, int offset);

#if defined(__cplusplus)
}
#endif
#endif
