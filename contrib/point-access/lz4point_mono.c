/* Copyright (c) 2026 noobpwnftw.
 * SPDX-License-Identifier: BSD-2-Clause
 */
/* Compact cache: two sampled monotone vectors, 16 checkpoints per sample.
 * Based on chesstb's Mono_Uint_Vec scheme; bounded to 32-bit coordinates.
 * Scalar C90 implementation. Each packed region has eight bytes of read slack. */
#include "lz4point_mono.h"
#include "lz4point_internal.h"
#include <string.h>

typedef struct {
    const LZ4P_Byte* samples;
    const LZ4P_Byte* deltas;
    unsigned sampleBits, deltaBits;
} LZ4P_Mono;
typedef struct {
    unsigned sampleBits, deltaBits;
    size_t sampleBytes, bytes;
} LZ4P_Plan;
struct LZ4_pointMonoIndex_s {
    const LZ4P_Byte* src;
    const LZ4P_Byte* dict;
    size_t dictSize;
    LZ4P_U32 decodedSize;
    int valid;
    LZ4P_Mono coordinates[2];
    LZ4P_Byte data[1];
};

static unsigned LZ4P_width(LZ4P_U32 v)
{
    unsigned bits = 0;
    while (v) {
        ++bits;
        v >>= 1;
    }
    return bits;
}
/* Endian-independent unaligned word I/O; compilers fold the endian test. */
static unsigned long long LZ4P_load(const LZ4P_Byte* p)
{
    unsigned const one = 1;
    unsigned long long v = 0;
    unsigned i;
    if (*(const unsigned char*)&one) {
        memcpy(&v, p, 8);
        return v;
    }
    for (i = 0; i < 8; ++i)
        v |= (unsigned long long)p[i] << (8 * i);
    return v;
}
static void LZ4P_put(LZ4P_Byte* p, size_t bit, LZ4P_U32 v, unsigned width)
{
    unsigned const one = 1;
    unsigned long long word;
    unsigned i;
    if (!width)
        return;
    p += bit >> 3;
    word = LZ4P_load(p) | ((unsigned long long)v << (bit & 7));
    if (*(const unsigned char*)&one)
        memcpy(p, &word, 8);
    else
        for (i = 0; i < 8; ++i)
            p[i] = (LZ4P_Byte)(word >> (8 * i));
}
static LZ4P_U32 LZ4P_bits(const LZ4P_Byte* p, size_t bit, unsigned width)
{
    if (!width)
        return 0;
    return (LZ4P_U32)((LZ4P_load(p + (bit >> 3)) >> (bit & 7)) & ((1ULL << width) - 1));
}
static LZ4P_U32 LZ4P_coordinate(const LZ4_pointIndex* index, size_t i, unsigned axis)
{
    return axis ? index->checkpoints[i].output : index->checkpoints[i].input;
}
static LZ4P_Plan LZ4P_plan(const LZ4_pointIndex* index, size_t n, unsigned axis)
{
    LZ4P_Plan plan;
    LZ4P_U32 maxDelta = 0;
    size_t i;
    for (i = 0; i < n; ++i) {
        LZ4P_U32 const delta =
            LZ4P_coordinate(index, i, axis) - LZ4P_coordinate(index, i & ~(size_t)15, axis);
        if (delta > maxDelta)
            maxDelta = delta;
    }
    plan.sampleBits = LZ4P_width(n ? LZ4P_coordinate(index, n - 1, axis) : 0);
    plan.deltaBits = LZ4P_width(maxDelta);
    plan.sampleBytes = (((n + 15) / 16) * plan.sampleBits + 7) / 8;
    plan.bytes = plan.sampleBytes + (n * plan.deltaBits + 7) / 8 + 8;
    return plan;
}
size_t LZ4_pointMonoSize(const LZ4_pointIndex* source)
{
    size_t n;
    if (!source || !source->valid)
        return 0;
    n = ((size_t)source->decodedSize + 255) >> 8;
    return offsetof(LZ4_pointMonoIndex, data) + LZ4P_plan(source, n, 0).bytes +
           LZ4P_plan(source, n, 1).bytes;
}
LZ4_pointMonoIndex* LZ4_pointMonoCopy(const LZ4_pointIndex* source, void* workspace,
                                      size_t workspaceSize)
{
    size_t const required = LZ4_pointMonoSize(source);
    LZ4_pointMonoIndex* result;
    LZ4P_Byte* data;
    size_t n, i;
    unsigned axis;
    if (!required || !workspace || workspaceSize < required)
        return NULL;
    memset(workspace, 0, required);
    result = (LZ4_pointMonoIndex*)workspace;
    result->src = source->src;
    result->dict = source->dict;
    result->dictSize = source->dictSize;
    result->decodedSize = source->decodedSize;
    n = ((size_t)source->decodedSize + 255) >> 8;
    data = result->data;
    for (axis = 0; axis < 2; ++axis) {
        LZ4P_Plan const plan = LZ4P_plan(source, n, axis);
        result->coordinates[axis].samples = data;
        result->coordinates[axis].deltas = data + plan.sampleBytes;
        result->coordinates[axis].sampleBits = plan.sampleBits;
        result->coordinates[axis].deltaBits = plan.deltaBits;
        for (i = 0; i < n; ++i) {
            LZ4P_U32 const base = LZ4P_coordinate(source, i & ~(size_t)15, axis);
            if (!(i & 15))
                LZ4P_put(data, (i >> 4) * plan.sampleBits, base, plan.sampleBits);
            LZ4P_put(data + plan.sampleBytes, i * plan.deltaBits,
                     LZ4P_coordinate(source, i, axis) - base, plan.deltaBits);
        }
        data += plan.bytes;
    }
    result->valid = 1;
    return result;
}
static LZ4P_Checkpoint LZ4P_monoCheckpoint(const LZ4_pointMonoIndex* index, size_t slot)
{
    LZ4P_Checkpoint result;
    const LZ4P_Mono* a = &index->coordinates[0];
    const LZ4P_Mono* b = &index->coordinates[1];
    result.input = LZ4P_bits(a->samples, (slot >> 4) * a->sampleBits, a->sampleBits) +
                   LZ4P_bits(a->deltas, slot * a->deltaBits, a->deltaBits);
    result.output = LZ4P_bits(b->samples, (slot >> 4) * b->sampleBits, b->sampleBits) +
                    LZ4P_bits(b->deltas, slot * b->deltaBits, b->deltaBits);
    return result;
}
#define LZ4P_READ_NAME LZ4_pointMonoRead
#define LZ4P_READ_TYPE LZ4_pointMonoIndex
#define LZ4P_READ_CHECKPOINT(index, slot) LZ4P_monoCheckpoint(index, slot)
#include "lz4point_read.h"
