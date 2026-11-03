/* Copyright (c) 2026 noobpwnftw.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "lz4point_mono.h"
#include "lz4.h"
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
static int sequence(unsigned char* dst, int at, unsigned char literal, unsigned length)
{
    unsigned remaining = length - 19;
    dst[at++] = 0x1f;
    dst[at++] = literal;
    dst[at++] = 1;
    dst[at++] = 0;
    while (remaining >= 255) {
        dst[at++] = 255;
        remaining -= 255;
    }
    dst[at++] = (unsigned char)remaining;
    return at;
}
static int makeBlock(unsigned char* dst, int size, int cut)
{
    int at = sequence(dst, 0, 'Q', (unsigned)cut - 1);
    at = sequence(dst, at, 'R', (unsigned)(size - cut - 6));
    dst[at++] = 0x50;
    memcpy(dst + at, "END!!", 5);
    return at + 5;
}
int main(void)
{
    int const size = LZ4_MAX_INPUT_SIZE;
    int const cut = size - size / 4 + 123;
    size_t const sourceCapacity = (size_t)size / 255 + 100;
    unsigned char* source = (unsigned char*)malloc(sourceCapacity);
    size_t const flatBytes = LZ4_pointIndexBound(size);
    void* flatMemory = malloc(flatBytes);
    LZ4_pointIndex* flat = LZ4_pointIndexInit(flatMemory, flatBytes, size);
    LZ4_pointMonoIndex* mono;
    unsigned char small[65536];
    unsigned char range[16];
    unsigned char* memory;
    size_t bytes, i;
    int n;
    CHECK(source && flat);
    n = makeBlock(source, 65536, 65536 - 65536 / 4 + 123);
    CHECK(LZ4_decompress_safe((char*)source, (char*)small, n, sizeof(small)) == 65536);
    for (i = 0; i < 65536; ++i)
        CHECK(small[i] == (i < 65536 - 65536 / 4 + 123 ? 'Q'
                           : i < 65531                 ? 'R'
                                                       : "END!!"[i - 65531]));
    n = makeBlock(source, size, cut);
    CHECK((size_t)n < sourceCapacity);
    CHECK(LZ4_pointIndexBuild(flat, source, n, NULL, 0) == size);
    bytes = LZ4_pointMonoSize(flat);
    memory = (unsigned char*)malloc(bytes + 16);
    CHECK(memory);
    memset(memory + bytes, 0xa5, 16);
    mono = LZ4_pointMonoCopy(flat, memory, bytes);
    CHECK(mono);
    memset(flatMemory, 0xcd, flatBytes);
    free(flatMemory);
    CHECK(LZ4_pointMonoRead(mono, 0) == 'Q' && LZ4_pointMonoRead(mono, cut - 1) == 'Q');
    CHECK(LZ4_pointMonoRead(mono, cut) == 'R' && LZ4_pointMonoRead(mono, size - 6) == 'R');
    CHECK(LZ4_pointMonoRead(mono, size - 5) == 'E' && LZ4_pointMonoRead(mono, size - 1) == '!');
    CHECK(LZ4_pointMonoRead(mono, size) < 0);
    CHECK(LZ4_pointMonoReadRange(mono, cut - 4, range, 8) == 8);
    CHECK(!memcmp(range, "QQQQRRRR", 8));
    CHECK(LZ4_pointMonoReadRange(mono, size - 8, range, 8) == 8);
    CHECK(!memcmp(range, "RRREND!!", 8));
    CHECK(LZ4_pointMonoReadRange(mono, size, NULL, 0) == 0);

    for (i = 0; i < 16; ++i)
        CHECK(memory[bytes + i] == 0xa5);
    printf("%d decoded bytes, 31-bit sample/delta values, flat %lu B, compact %lu B, reads after "
           "freeing flat index: passed\n",
           size, (unsigned long)flatBytes, (unsigned long)bytes);
    free(memory);
    free(source);
    return 0;
}
