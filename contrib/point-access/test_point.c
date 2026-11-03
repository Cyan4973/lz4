/* Copyright (c) 2026 noobpwnftw.
 * SPDX-License-Identifier: BSD-2-Clause
 */
/* Deterministic differential and malformed-input checks for the prototype. */
#include "lz4point.h"
#include "lz4.h"
#include "lz4hc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned randomState = 0x13475a21U;
static unsigned nextRandom(void)
{
    unsigned x = randomState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    randomState = x;
    return x;
}
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            fprintf(stderr, "FAIL line %d seed=%u\n", __LINE__, randomState);                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void)
{
    int const maxSize = 65536;
    size_t const capacity = LZ4_pointIndexBound(maxSize);
    unsigned char* src = (unsigned char*)malloc(maxSize);
    unsigned char* dict = (unsigned char*)malloc(70000);
    char* encoded = (char*)malloc(LZ4_compressBound(maxSize));
    unsigned char* decoded = (unsigned char*)malloc(maxSize);
    unsigned char* memory = (unsigned char*)malloc(capacity + 16);
    LZ4_pointIndex* index = LZ4_pointIndexInit(memory, capacity, maxSize);
    unsigned test;
    CHECK(src && dict && encoded && decoded && memory && index);
    for (test = 0; test < 8000; ++test) {
        int const size = (test % 50 == 0) ? maxSize : (int)(nextRandom() % 4096) + 1;
        static const int dictSizes[] = {0, 1, 255, 1024, 65535, 65536, 70000};
        int const dictSize = dictSizes[test % (sizeof(dictSizes) / sizeof(dictSizes[0]))];
        int const kind = (int)(test % 5);
        size_t const bound = LZ4_pointIndexBound(size);
        size_t guard;
        int cSize;
        int i;
        unsigned q;
        for (i = 0; i < dictSize; ++i)
            dict[i] = (unsigned char)nextRandom();
        for (i = 0; i < size; ++i) {
            if (kind == 0)
                src[i] = (unsigned char)nextRandom();
            else if (kind == 1)
                src[i] = (unsigned char)(nextRandom() % 5);
            else if (kind == 2)
                src[i] = 7;
            else if (kind == 3)
                src[i] = (unsigned char)(i < 64 ? nextRandom() : src[i - 64]);
            else
                src[i] = dictSize ? dict[(i + 123) % dictSize] : (unsigned char)(i % 17);
        }
        if (dictSize) {
            LZ4_stream_t* state = LZ4_createStream();
            CHECK(state != NULL);
            LZ4_loadDict(state, (const char*)dict, dictSize);
            cSize = LZ4_compress_fast_continue(state, (const char*)src, encoded, size,
                                               LZ4_compressBound(size), 1);
            LZ4_freeStream(state);
        } else if (test % 2) {
            cSize = LZ4_compress_default((const char*)src, encoded, size, LZ4_compressBound(size));
        } else {
            cSize = LZ4_compress_HC((const char*)src, encoded, size, LZ4_compressBound(size), 9);
        }
        CHECK(cSize > 0);
        index = LZ4_pointIndexInit(memory, bound, size);
        CHECK(index != NULL);
        memset(memory + bound, 0xa5, 16);
        CHECK(LZ4_decompress_safe_usingDict(encoded, (char*)decoded, cSize, size, (const char*)dict,
                                            dictSize) == size);
        CHECK(memcmp(src, decoded, size) == 0);
        CHECK(LZ4_pointIndexBuild(index, encoded, cSize, dict, dictSize) == size);
        for (q = 0; q < 64; ++q) {
            int const p = q == 0 ? 0 : q == 1 ? size - 1 : (int)(nextRandom() % size);
            CHECK(LZ4_pointRead(index, p) == src[p]);
        }
        for (guard = 0; guard < 16; ++guard)
            CHECK(memory[bound + guard] == 0xa5);
        CHECK(LZ4_pointRead(index, -1) < 0 && LZ4_pointRead(index, size) < 0);
        CHECK(LZ4_pointIndexBuild(index, encoded, cSize, dict, dictSize) == size);
        CHECK(LZ4_pointIndexBuild(index, NULL, cSize, dict, dictSize) < 0);
        CHECK(LZ4_pointRead(index, 0) < 0); /* failed build invalidates old index */
    }
    index = LZ4_pointIndexInit(memory, capacity, maxSize);
    CHECK(index != NULL);
    /* A single token encoding empty output is valid; no offset is readable. */
    encoded[0] = 0;
    CHECK(LZ4_pointIndexBuild(index, encoded, 1, NULL, 0) == 0);
    CHECK(LZ4_pointRead(index, 0) < 0);
    /* Random malformed inputs exercise bounds under ASan/UBSan. Success is
     * allowed for structurally valid blocks, which may be noncanonical. */
    for (test = 0; test < 30000; ++test) {
        int const cSize = (int)(nextRandom() % 128) + 1;
        int i, result;
        for (i = 0; i < cSize; ++i)
            encoded[i] = (char)nextRandom();
        result = LZ4_pointIndexBuild(index, encoded, cSize, NULL, 0);
        if (result > 0)
            CHECK(LZ4_pointRead(index, (int)(nextRandom() % result)) >= 0);
    }
    free(src);
    free(dict);
    free(encoded);
    free(decoded);
    free(memory);
    puts("8000 valid blocks (fast/HC/dictionaries 0..70000 bytes), 512000 indexed point reads, "
         "30000 malformed inputs: passed");
    return 0;
}
