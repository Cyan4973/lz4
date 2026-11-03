/* Copyright (c) 2026 noobpwnftw.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "lz4point_mono.h"
#include "lz4hc.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            fprintf(stderr, "FAIL line %d seed %u\n", __LINE__, seed);                             \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static unsigned seed = 0x713a29b5U;
static unsigned rnd(void)
{
    unsigned x = seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return seed = x;
}
int main(void)
{
    unsigned char source[65536], dictionary[70000], decoded[65536], out[65568];
    char compressed[66000];
    unsigned trial;
    for (trial = 0; trial < 2000; ++trial) {
        static const int ds[] = {0, 1, 255, 1024, 65535, 65536, 70000};
        static const int lengths[] = {0,  1,  2,  3,   4,   7,   8,    16,
                                      31, 32, 64, 255, 256, 257, 1024, 4096};
        static const int periods[] = {1, 2, 3, 4, 7, 8, 15, 16, 17, 31, 255, 256, 4096};
        int const size = trial % 50 == 0 ? 65536 : (int)(rnd() % 8192) + 1;
        int const dictSize = ds[trial % 7], period = periods[trial % 13];
        size_t const flatBytes = LZ4_pointIndexBound(size);
        void* flatMemory = malloc(flatBytes);
        LZ4_pointIndex* flat = LZ4_pointIndexInit(flatMemory, flatBytes, size);
        LZ4_pointMonoIndex* mono;
        void* monoMemory;
        size_t monoBytes;
        int i, n, q;
        CHECK(flat);
        for (i = 0; i < dictSize; ++i)
            dictionary[i] = (unsigned char)rnd();
        for (i = 0; i < size; ++i) {
            if (trial % 5 == 0)
                source[i] = (unsigned char)rnd();
            else if (trial % 5 == 1)
                source[i] = (unsigned char)(rnd() % 5);
            else if (trial % 5 == 2)
                source[i] = (unsigned char)(i < period ? rnd() : source[i - period]);
            else if (trial % 5 == 3 && dictSize)
                source[i] = dictionary[i % dictSize];
            else
                source[i] = (unsigned char)((i % 4096) == 0 ? rnd() : 9);
        }
        if (dictSize) {
            LZ4_stream_t* stream = LZ4_createStream();
            CHECK(stream);
            LZ4_loadDict(stream, (char*)dictionary, dictSize);
            n = LZ4_compress_fast_continue(stream, (char*)source, compressed, size,
                                           sizeof(compressed), 1);
            LZ4_freeStream(stream);
        } else
            n = trial % 2
                    ? LZ4_compress_default((char*)source, compressed, size, sizeof(compressed))
                    : LZ4_compress_HC((char*)source, compressed, size, sizeof(compressed), 9);
        CHECK(n > 0 && LZ4_decompress_safe_usingDict(compressed, (char*)decoded, n, size,
                                                     (char*)dictionary, dictSize) == size);
        CHECK(!memcmp(source, decoded, size));
        CHECK(LZ4_pointIndexBuild(flat, compressed, n, dictionary, dictSize) == size);
        monoBytes = LZ4_pointMonoSize(flat);
        monoMemory = malloc(monoBytes);
        CHECK(monoMemory);
        mono = LZ4_pointMonoCopy(flat, monoMemory, monoBytes);
        CHECK(mono);
        for (q = 0; q < 32; ++q) {
            int offset = q == 0   ? 0
                         : q == 1 ? size
                         : q == 2 ? size - 1
                         : q < 6  ? (q - 3) * 256 + 255
                                  : (int)(rnd() % (unsigned)(size + 1));
            int len = lengths[q % 16], mode, j;
            if (offset > size)
                offset = size;
            if (q == 0)
                len = size;
            if (len > size - offset)
                len = size - offset;
            for (mode = 0; mode < 2; ++mode) {
                memset(out, 0xa5, (size_t)len + 32);
                CHECK((mode ? LZ4_pointMonoReadRange(mono, offset, out + 16, len)
                            : LZ4_pointReadRange(flat, offset, out + 16, len)) == len);
                CHECK(!memcmp(out + 16, decoded + offset, (size_t)len));
                for (j = 0; j < 16; ++j)
                    CHECK(out[j] == 0xa5 && out[16 + len + j] == 0xa5);
            }
        }
        memset(out, 0xa5, sizeof(out));
        CHECK(LZ4_pointReadRange(flat, -1, out, 1) < 0 &&
              LZ4_pointMonoReadRange(mono, -1, out, 1) < 0);
        CHECK(LZ4_pointReadRange(flat, 0, out, -1) < 0 &&
              LZ4_pointMonoReadRange(mono, 0, out, -1) < 0);
        CHECK(LZ4_pointReadRange(flat, size, out, 1) < 0 &&
              LZ4_pointMonoReadRange(mono, size, out, 1) < 0);
        CHECK(LZ4_pointReadRange(flat, INT_MAX, out, INT_MAX) < 0 &&
              LZ4_pointMonoReadRange(mono, INT_MAX, out, INT_MAX) < 0);
        CHECK(LZ4_pointReadRange(flat, 0, NULL, 1) < 0 &&
              LZ4_pointMonoReadRange(mono, 0, NULL, 1) < 0);
        CHECK(LZ4_pointReadRange(flat, size, NULL, 0) == 0 &&
              LZ4_pointMonoReadRange(mono, size, NULL, 0) == 0);
        CHECK(LZ4_pointReadRange(NULL, 0, out, 0) < 0 &&
              LZ4_pointMonoReadRange(NULL, 0, out, 0) < 0);
        for (i = 0; i < (int)sizeof(out); ++i)
            CHECK(out[i] == 0xa5);
        memset(flatMemory, 0xcd, flatBytes);
        free(flatMemory);
        CHECK(LZ4_pointMonoReadRange(mono, 0, out, size) == size && !memcmp(out, decoded, size));
        free(monoMemory);
    }
    {
        char const empty = 0;
        size_t const bytes = LZ4_pointIndexBound(0);
        void* memory = malloc(bytes);
        LZ4_pointIndex* flat = LZ4_pointIndexInit(memory, bytes, 0);
        CHECK(flat && LZ4_pointIndexBuild(flat, &empty, 1, NULL, 0) == 0);
        CHECK(LZ4_pointReadRange(flat, 0, NULL, 0) == 0);
        CHECK(LZ4_pointReadRange(flat, 0, out, 1) < 0);
        {
            size_t const monoBytes = LZ4_pointMonoSize(flat);
            void* packed = malloc(monoBytes);
            LZ4_pointMonoIndex* mono = LZ4_pointMonoCopy(flat, packed, monoBytes);
            CHECK(mono && LZ4_pointMonoReadRange(mono, 0, NULL, 0) == 0);
            CHECK(LZ4_pointMonoReadRange(mono, 0, out, 1) < 0);
            free(packed);
        }
        free(memory);
    }
    puts("Range reads: 2000 blocks, 128000 guarded flat/compact ranges, reference decode, periods, "
         "dictionaries, released flat workspace and invalid requests: passed");
    return 0;
}
