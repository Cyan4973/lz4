/* Copyright (c) 2026 noobpwnftw.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "lz4point_mono.h"
#include "lz4hc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            fprintf(stderr, "FAIL line %d\n", __LINE__);                                           \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static unsigned seed = 0x197a04d1U;
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
    unsigned char source[65536], dictionary[70000];
    char compressed[66000];
    unsigned trial;
    for (trial = 0; trial < 8000; ++trial) {
        static const int ds[] = {0, 1, 255, 1024, 65535, 65536, 70000};
        int const dictSize = ds[trial % 7];
        int const size = trial % 50 == 0 ? 65536 : (int)(rnd() % 4096) + 1;
        size_t const flatSize = LZ4_pointIndexBound(65536);
        void* flatMemory = malloc(flatSize);
        LZ4_pointIndex* flat = LZ4_pointIndexInit(flatMemory, flatSize, 65536);
        LZ4_pointMonoIndex* mono;
        unsigned char* memory;
        size_t bytes, guard;
        int i, n;
        CHECK(flat);
        for (i = 0; i < dictSize; ++i)
            dictionary[i] = (unsigned char)rnd();
        for (i = 0; i < size; ++i)
            source[i] = trial % 5 == 0   ? (unsigned char)rnd()
                        : trial % 5 == 1 ? (unsigned char)(rnd() % 5)
                        : trial % 5 == 2 ? 9
                        : trial % 5 == 3 ? (unsigned char)(i < 64 ? rnd() : source[i - 64])
                                         : (unsigned char)(i % 17);
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
        CHECK(n > 0 && LZ4_pointIndexBuild(flat, compressed, n, dictionary, dictSize) == size);
        bytes = LZ4_pointMonoSize(flat);
        CHECK(bytes > 0);
        memory = (unsigned char*)malloc(bytes + 16);
        CHECK(memory);
        memset(memory, 0xa5, bytes + 16);
        CHECK(!LZ4_pointMonoCopy(flat, memory, bytes - 1));
        CHECK(!LZ4_pointMonoCopy(flat, NULL, bytes));
        mono = LZ4_pointMonoCopy(flat, memory, bytes);
        CHECK(mono);
        memset(flatMemory, 0xcd, flatSize);
        free(flatMemory);
        for (i = 0; i < 64; ++i) {
            int const p = i == 0 ? 0 : i == 1 ? size - 1 : (int)(rnd() % size);
            CHECK(LZ4_pointMonoRead(mono, p) == source[p]);
        }
        CHECK(LZ4_pointMonoRead(mono, -1) < 0 && LZ4_pointMonoRead(mono, size) < 0);
        for (guard = 0; guard < 16; ++guard)
            CHECK(memory[bytes + guard] == 0xa5);
        free(memory);
    }
    {
        char const empty = 0;
        size_t const bytes = LZ4_pointIndexBound(0);
        void* flatMemory = malloc(bytes);
        LZ4_pointIndex* flat = LZ4_pointIndexInit(flatMemory, bytes, 0);
        void* memory;
        LZ4_pointMonoIndex* mono;
        CHECK(LZ4_pointMonoSize(NULL) == 0 && !LZ4_pointMonoCopy(NULL, NULL, 0));
        CHECK(LZ4_pointMonoRead(NULL, 0) < 0 && LZ4_pointMonoSize(flat) == 0);
        CHECK(LZ4_pointIndexBuild(flat, &empty, 1, NULL, 0) == 0);
        memory = malloc(LZ4_pointMonoSize(flat));
        CHECK(memory);
        mono = LZ4_pointMonoCopy(flat, memory, LZ4_pointMonoSize(flat));
        CHECK(mono);
        free(flatMemory);
        CHECK(LZ4_pointMonoRead(mono, 0) < 0);
        free(memory);
    }
    puts("Two-mono C cache: 8000 blocks, 512000 reads after releasing flat index, exact-size "
         "canaries, dictionary and empty cases: passed");
    return 0;
}
