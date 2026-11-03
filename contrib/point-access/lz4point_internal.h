/* Copyright (c) 2026 noobpwnftw.
 * SPDX-License-Identifier: BSD-2-Clause
 */
/* Private index layout shared by the flat and compact prototypes. */
#ifndef LZ4POINT_INTERNAL_H
#define LZ4POINT_INTERNAL_H
#include "lz4point.h"
#include <limits.h>
#include <stddef.h>
#if UINT_MAX == 0xffffffffU
typedef unsigned int LZ4P_U32;
#elif ULONG_MAX == 0xffffffffUL
typedef unsigned long LZ4P_U32;
#else
#error "This draft requires a 32-bit integer type"
#endif

typedef unsigned char LZ4P_Byte;
typedef struct {
    LZ4P_U32 input;
    LZ4P_U32 output;
} LZ4P_Checkpoint;
struct LZ4_pointIndex_s {
    const LZ4P_Byte* src;
    const LZ4P_Byte* dict;
    size_t srcSize;
    size_t dictSize;
    LZ4P_U32 maxDecodedSize;
    LZ4P_U32 decodedSize;
    size_t slots;
    int valid;
    LZ4P_Checkpoint checkpoints[1];
};

#define LZ4P_STRIDE_LOG 8
#define LZ4P_HISTORY (64U * 1024U)
#define LZ4P_FAIL (-1)

#endif
