/* Copyright (c) 2026 noobpwnftw.
 * SPDX-License-Identifier: BSD-2-Clause
 */
/* Bounds-checked experimental point reader. API and implementation are drafts. */
#include "lz4point.h"
#include <limits.h>
#include <stddef.h>

#include "lz4point_internal.h"

size_t LZ4_pointIndexBound(int maxDecodedSize)
{
    size_t slots;
    size_t bytes;
    if (maxDecodedSize < 0)
        return 0;
    slots = ((size_t)maxDecodedSize + 255) >> LZ4P_STRIDE_LOG;
    bytes = offsetof(LZ4_pointIndex, checkpoints) + slots * sizeof(LZ4P_Checkpoint);
    if (bytes < sizeof(LZ4_pointIndex))
        bytes = sizeof(LZ4_pointIndex);
    return bytes;
}

LZ4_pointIndex* LZ4_pointIndexInit(void* workspace, size_t workspaceSize, int maxDecodedSize)
{
    size_t const needed = LZ4_pointIndexBound(maxDecodedSize);
    LZ4_pointIndex* index;
    if (workspace == NULL || needed == 0 || workspaceSize < needed)
        return NULL;
    index = (LZ4_pointIndex*)workspace;
    index->src = NULL;
    index->dict = NULL;
    index->srcSize = 0;
    index->dictSize = 0;
    index->maxDecodedSize = (LZ4P_U32)maxDecodedSize;
    index->decodedSize = 0;
    index->slots = ((size_t)maxDecodedSize + 255) >> LZ4P_STRIDE_LOG;
    index->valid = 0;
    return index;
}

/* Read an LZ4 token length, checking source bounds and integer growth. */
static int LZ4P_length(const LZ4P_Byte** pp, const LZ4P_Byte* end, size_t initial, size_t* out)
{
    const LZ4P_Byte* p = *pp;
    size_t length = initial;
    if (initial == 15) {
        unsigned extension;
        do {
            if (p == end)
                return LZ4P_FAIL;
            extension = *p++;
            if (length > (size_t)INT_MAX - extension)
                return LZ4P_FAIL;
            length += extension;
        } while (extension == 255);
    }
    *pp = p;
    *out = length;
    return 0;
}

static void LZ4P_fill(LZ4_pointIndex* index, size_t* next, LZ4P_Checkpoint checkpoint, LZ4P_U32 end)
{
    while (*next < index->slots && (*next << LZ4P_STRIDE_LOG) < end) {
        index->checkpoints[(*next)++] = checkpoint;
    }
}

#define LZ4P_RANGE_NAME LZ4_pointReadRange
#define LZ4P_READ_NAME LZ4_pointRead
#define LZ4P_READ_TYPE LZ4_pointIndex
#define LZ4P_READ_CHECKPOINT(index, slot) ((index)->checkpoints[slot])
#include "lz4point_read.h"

/* Validate the complete block and build its immutable checkpoint index. */
static int LZ4P_parse(LZ4_pointIndex* index, LZ4P_U32 maxDecoded)
{
    const LZ4P_Byte* p = index->src;
    const LZ4P_Byte* const end = p + index->srcSize;
    LZ4P_U32 output = 0;
    LZ4P_U32 lastMatchStart = 0;
    size_t next = 0;
    int sawMatch = 0;
    while (p < end) {
        LZ4P_Checkpoint checkpoint;
        unsigned token;
        size_t literals;
        size_t match;
        unsigned distance;
        checkpoint.input = (LZ4P_U32)(p - index->src);
        checkpoint.output = output;
        token = *p++;
        if (LZ4P_length(&p, end, token >> 4, &literals) < 0 || literals > (size_t)(end - p) ||
            literals > (size_t)(maxDecoded - output))
            return LZ4P_FAIL;
        p += literals;
        output += (LZ4P_U32)literals;
        if (p == end) {
            if (sawMatch && (literals < 5 || output - lastMatchStart < 12))
                return LZ4P_FAIL;
            LZ4P_fill(index, &next, checkpoint, output);
            index->decodedSize = output;
            index->valid = 1;
            return (int)output;
        }
        if ((size_t)(end - p) < 2)
            return LZ4P_FAIL;
        distance = (unsigned)p[0] | ((unsigned)p[1] << 8);
        p += 2;
        if (distance == 0 || (size_t)distance > (size_t)output + index->dictSize)
            return LZ4P_FAIL;
        if (LZ4P_length(&p, end, token & 15, &match) < 0)
            return LZ4P_FAIL;
        /* The checked length is <= INT_MAX; +4 fits even 32-bit size_t.
         * The decoded-size check below also rejects values above INT_MAX. */
        match += 4;
        if (match > (size_t)(maxDecoded - output))
            return LZ4P_FAIL;
        lastMatchStart = output;
        sawMatch = 1;
        LZ4P_fill(index, &next, checkpoint, output + (LZ4P_U32)match);
        output += (LZ4P_U32)match;
    }
    return LZ4P_FAIL;
}

static int LZ4P_setup(LZ4_pointIndex* index, const void* src, int srcSize, const void* dict,
                      int dictSize)
{
    if (index == NULL)
        return LZ4P_FAIL;
    index->valid = 0;
    if (src == NULL || srcSize <= 0 || dictSize < 0 || (dictSize > 0 && dict == NULL))
        return LZ4P_FAIL;
    index->src = (const LZ4P_Byte*)src;
    index->srcSize = (size_t)srcSize;
    if ((unsigned)dictSize > LZ4P_HISTORY) {
        index->dict = (const LZ4P_Byte*)dict + (dictSize - LZ4P_HISTORY);
        index->dictSize = LZ4P_HISTORY;
    } else {
        index->dict = (const LZ4P_Byte*)dict;
        index->dictSize = (size_t)dictSize;
    }
    return 0;
}

int LZ4_pointIndexBuild(LZ4_pointIndex* index, const void* src, int srcSize, const void* dict,
                        int dictSize)
{
    if (LZ4P_setup(index, src, srcSize, dict, dictSize) < 0)
        return LZ4P_FAIL;
    return LZ4P_parse(index, index->maxDecodedSize);
}
