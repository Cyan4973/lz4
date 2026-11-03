/* Copyright (c) 2026 noobpwnftw.
 * SPDX-License-Identifier: BSD-2-Clause
 */
/* Optional compact-cache API for this RFC prototype. */
#ifndef LZ4POINT_MONO_H
#define LZ4POINT_MONO_H
#include "lz4point.h"
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct LZ4_pointMonoIndex_s LZ4_pointMonoIndex;
/* Exact final workspace size for two monotone vectors, or 0 for an invalid
 * source. The source must be a successfully built flat index. */
LZ4LIB_API size_t LZ4_pointMonoSize(const LZ4_pointIndex* source);
/* Encode into separate, malloc-aligned caller-owned workspace. No allocations.
 * Returns NULL on invalid arguments or insufficient workspace. The resulting
 * index borrows source's compressed block and dictionary, but not source itself:
 * the flat workspace can be released after copying. Borrowed buffers stay alive
 * and immutable until every reader finishes. Workspace must not overlap the flat index, compressed
 * block or dictionary. The source index must remain unchanged throughout the copy. This is an
 * in-memory cache representation, not a serialized format. */
LZ4LIB_API LZ4_pointMonoIndex* LZ4_pointMonoCopy(const LZ4_pointIndex* source, void* workspace,
                                                 size_t workspaceSize);
/* Returns a byte in 0..255, or a negative error. Concurrent reads are safe. */
LZ4LIB_API int LZ4_pointMonoRead(const LZ4_pointMonoIndex* index, int offset);
/* Same range and destination contract as LZ4_pointReadRange(). */
LZ4LIB_API int LZ4_pointMonoReadRange(const LZ4_pointMonoIndex* index, int offset, void* dst,
                                      int len);

#if defined(__cplusplus)
}
#endif
#endif
