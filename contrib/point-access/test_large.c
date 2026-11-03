/* Copyright (c) 2026 noobpwnftw.
 * SPDX-License-Identifier: BSD-2-Clause
 */
/* Large-offset check without allocating a 2 GiB decoded output buffer. */
#include "lz4point.h"
#include "lz4.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            fprintf(stderr, "FAIL line %d\n", __LINE__);                                           \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int makeRun(unsigned char* dst, int outputSize)
{
    int const matchLength = outputSize - 6;
    unsigned remaining = (unsigned)matchLength - 19U;
    int pos = 0;
    dst[pos++] = 0x1f; /* one literal followed by a long match */
    dst[pos++] = 'Q';
    dst[pos++] = 1;
    dst[pos++] = 0; /* repeat with offset 1 */
    while (remaining >= 255U) {
        dst[pos++] = 255;
        remaining -= 255U;
    }
    dst[pos++] = (unsigned char)remaining;
    dst[pos++] = 0x50; /* five final literal bytes */
    memcpy(dst + pos, "END!!", 5);
    return pos + 5;
}
int main(void)
{
    int const size = LZ4_MAX_INPUT_SIZE;
    size_t const compressedCapacity = (size_t)size / 255 + 100;
    size_t const indexSize = LZ4_pointIndexBound(size);
    unsigned char* compressed = (unsigned char*)malloc(compressedCapacity);
    void* indexMemory = malloc(indexSize);
    unsigned char small[65536];
    unsigned char range[16];
    LZ4_pointIndex* index;
    int compressedSize;
    CHECK(compressed && indexMemory);
    /* Establish format equivalence with a full reference decode at 64 KiB. */
    compressedSize = makeRun(compressed, 65536);
    CHECK(LZ4_decompress_safe((const char*)compressed, (char*)small, compressedSize, 65536) ==
          65536);
    CHECK(small[0] == 'Q' && small[65530] == 'Q' && memcmp(small + 65531, "END!!", 5) == 0);
    compressedSize = makeRun(compressed, size);
    CHECK((size_t)compressedSize < compressedCapacity);
    index = LZ4_pointIndexInit(indexMemory, indexSize, size);
    CHECK(index != NULL);
    CHECK(LZ4_pointIndexBuild(index, compressed, compressedSize, NULL, 0) == size);
    CHECK(LZ4_pointRead(index, 0) == 'Q');
    CHECK(LZ4_pointRead(index, size / 2) == 'Q');
    CHECK(LZ4_pointRead(index, size - 6) == 'Q');
    CHECK(LZ4_pointRead(index, size - 5) == 'E');
    CHECK(LZ4_pointRead(index, size - 1) == '!');
    CHECK(LZ4_pointReadRange(index, size / 2, range, 16) == 16);
    CHECK(!memcmp(range, "QQQQQQQQQQQQQQQQ", 16));
    CHECK(LZ4_pointReadRange(index, size - 8, range, 8) == 8);
    CHECK(!memcmp(range, "QQQEND!!", 8));
    CHECK(LZ4_pointReadRange(index, size, NULL, 0) == 0);

    printf("%d-byte decoded block, %d-byte source, %lu-byte index: passed\n", size, compressedSize,
           (unsigned long)indexSize);
    free(compressed);
    free(indexMemory);
    return 0;
}
