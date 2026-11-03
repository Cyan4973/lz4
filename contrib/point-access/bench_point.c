/* Copyright (c) 2026 noobpwnftw.
 * SPDX-License-Identifier: BSD-2-Clause
 */
/* Point-access microbenchmark, designed for individual Silesia corpus files.
 * Timings exclude allocation/compression and use reusable, RAM-resident input.
 * See BENCHMARKS.md for exact methods and limitations. */
#include "lz4point.h"
#include "lz4.h"
#include "lz4hc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BLOCK_SIZE 65536
#define BLOCKS 16
#define QUERIES 256
#define ROUNDS 5
#define METHODS 11

typedef unsigned char Byte;
typedef struct {
    Byte* source;
    char* compressed;
    int compressedSize;
    Byte* dict;
    int dictSize;
    unsigned offsets[QUERIES];
    void* indexMemory;
    LZ4_pointIndex* index;
} Block;
typedef struct {
    Block b[BLOCKS];
    int count;
    Byte* output;
    void* scratchMemory;
    size_t indexSize;
    LZ4_pointIndex* scratchIndex;
} Corpus;
static volatile unsigned long long consume;
static unsigned state = 0x7c317b29U;
static unsigned rnd(void)
{
    unsigned x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            fprintf(stderr, "error at line %d\n", __LINE__);                                       \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static void setup(Corpus* c, const char* path, int hc, int withDict)
{
    FILE* f = fopen(path, "rb");
    long size;
    size_t fullBlocks;
    int i;
    CHECK(f != NULL);
    CHECK(fseek(f, 0, SEEK_END) == 0);
    size = ftell(f);
    CHECK(size >= BLOCK_SIZE);
    fullBlocks = (size_t)size / BLOCK_SIZE;
    CHECK(!withDict || fullBlocks > 1);
    fullBlocks -= withDict ? 1 : 0;
    c->count = fullBlocks < BLOCKS ? (int)fullBlocks : BLOCKS;
    c->indexSize = LZ4_pointIndexBound(BLOCK_SIZE);
    c->output = (Byte*)malloc(BLOCK_SIZE);
    c->scratchMemory = malloc(c->indexSize);
    c->scratchIndex = LZ4_pointIndexInit(c->scratchMemory, c->indexSize, BLOCK_SIZE);
    CHECK(c->output && c->scratchIndex);
    for (i = 0; i < c->count; ++i) {
        Block* b = &c->b[i];
        size_t const lo = (size_t)i * fullBlocks / c->count;
        size_t const hi = (size_t)(i + 1) * fullBlocks / c->count;
        size_t const bid = lo + rnd() % (hi - lo) + (withDict ? 1 : 0);
        int j;
        b->source = (Byte*)malloc(BLOCK_SIZE);
        b->compressed = (char*)malloc(LZ4_compressBound(BLOCK_SIZE));
        b->indexMemory = malloc(c->indexSize);
        b->index = LZ4_pointIndexInit(b->indexMemory, c->indexSize, BLOCK_SIZE);
        CHECK(b->source && b->compressed && b->index);
        b->dict = NULL;
        b->dictSize = withDict ? BLOCK_SIZE : 0;
        if (withDict) {
            b->dict = (Byte*)malloc(BLOCK_SIZE);
            CHECK(b->dict != NULL);
            CHECK(fseek(f, (long)((bid - 1) * BLOCK_SIZE), SEEK_SET) == 0);
            CHECK(fread(b->dict, 1, BLOCK_SIZE, f) == BLOCK_SIZE);
        }
        CHECK(fseek(f, (long)(bid * BLOCK_SIZE), SEEK_SET) == 0);
        CHECK(fread(b->source, 1, BLOCK_SIZE, f) == BLOCK_SIZE);
        if (withDict && hc) {
            LZ4_streamHC_t* stream = LZ4_createStreamHC();
            CHECK(stream != NULL);
            LZ4_resetStreamHC_fast(stream, 9);
            (void)LZ4_loadDictHC(stream, (const char*)b->dict, b->dictSize);
            b->compressedSize = LZ4_compress_HC_continue(stream, (char*)b->source, b->compressed,
                                                         BLOCK_SIZE, LZ4_compressBound(BLOCK_SIZE));
            LZ4_freeStreamHC(stream);
        } else if (withDict) {
            LZ4_stream_t* stream = LZ4_createStream();
            CHECK(stream != NULL);
            (void)LZ4_loadDict(stream, (const char*)b->dict, b->dictSize);
            b->compressedSize =
                LZ4_compress_fast_continue(stream, (char*)b->source, b->compressed, BLOCK_SIZE,
                                           LZ4_compressBound(BLOCK_SIZE), 1);
            LZ4_freeStream(stream);
        } else {
            b->compressedSize =
                hc ? LZ4_compress_HC((char*)b->source, b->compressed, BLOCK_SIZE,
                                     LZ4_compressBound(BLOCK_SIZE), 9)
                   : LZ4_compress_default((char*)b->source, b->compressed, BLOCK_SIZE,
                                          LZ4_compressBound(BLOCK_SIZE));
        }
        CHECK(b->compressedSize > 0);
        CHECK(LZ4_pointIndexBuild(b->index, b->compressed, b->compressedSize, b->dict,
                                  b->dictSize) == BLOCK_SIZE);
        CHECK(LZ4_decompress_safe_usingDict(b->compressed, (char*)c->output, b->compressedSize,
                                            BLOCK_SIZE, (const char*)b->dict,
                                            b->dictSize) == BLOCK_SIZE);
        CHECK(memcmp(c->output, b->source, BLOCK_SIZE) == 0);
        for (j = 0; j < QUERIES; ++j) {
            unsigned const p = rnd() % (unsigned)BLOCK_SIZE;
            b->offsets[j] = p;
            CHECK(LZ4_pointRead(b->index, (int)p) == b->source[p]);
        }
        /* Exhaustively check one block per source; all other blocks receive 256
         * differential point checks above. */
        if (i == 0)
            for (j = 0; j < BLOCK_SIZE; ++j)
                CHECK(LZ4_pointRead(b->index, j) == b->source[j]);
    }
    fclose(f);
}

static int fullDecode(const Block* b, Byte* output)
{
    return b->dictSize
               ? LZ4_decompress_safe_usingDict(b->compressed, (char*)output, b->compressedSize,
                                               BLOCK_SIZE, (const char*)b->dict, b->dictSize)
               : LZ4_decompress_safe(b->compressed, (char*)output, b->compressedSize, BLOCK_SIZE);
}
static int partialDecode(const Block* b, Byte* output, int target)
{
    return b->dictSize
               ? LZ4_decompress_safe_partial_usingDict(b->compressed, (char*)output,
                                                       b->compressedSize, target, BLOCK_SIZE,
                                                       (const char*)b->dict, b->dictSize)
               : LZ4_decompress_safe_partial(b->compressed, (char*)output, b->compressedSize,
                                             target, BLOCK_SIZE);
}

/* 0 index-build only, 1 index-build+read, 2 cached-index, 3 partial, 4 full;
 * 5 cached-index+32, 6 full-decode-per-read x32, 7 full-decode-once+32,
 * 8 partial-decode-per-read x32, 9 index-build+32, 10 retained-output+32. */
static unsigned long long operation(Corpus* c, int mode, size_t iteration)
{
    Block* b = &c->b[iteration % (size_t)c->count];
    size_t const group = (iteration / (size_t)c->count) % 8;
    unsigned const p = b->offsets[group * 32];
    unsigned long long sum = 0;
    int result;
    int j;
    switch (mode) {
    case 0:
        CHECK(LZ4_pointIndexBuild(c->scratchIndex, b->compressed, b->compressedSize, b->dict,
                                  b->dictSize) == BLOCK_SIZE);
        return 0;
    case 1:
        CHECK(LZ4_pointIndexBuild(c->scratchIndex, b->compressed, b->compressedSize, b->dict,
                                  b->dictSize) == BLOCK_SIZE);
        result = LZ4_pointRead(c->scratchIndex, (int)p);
        CHECK(result >= 0);
        return (unsigned)result;
    case 2:
        result = LZ4_pointRead(b->index, (int)p);
        CHECK(result >= 0);
        return (unsigned)result;
    case 3:
        CHECK(partialDecode(b, c->output, (int)p + 1) == (int)p + 1);
        return c->output[p];
    case 4:
        CHECK(fullDecode(b, c->output) == BLOCK_SIZE);
        return c->output[p];
    case 5:
        for (j = 0; j < 32; ++j) {
            result = LZ4_pointRead(b->index, (int)b->offsets[group * 32 + j]);
            CHECK(result >= 0);
            sum += (unsigned)result;
        }
        return sum;
    case 6:
        for (j = 0; j < 32; ++j) {
            unsigned const q = b->offsets[group * 32 + j];
            CHECK(fullDecode(b, c->output) == BLOCK_SIZE);
            sum += c->output[q];
        }
        return sum;
    case 7:
        CHECK(fullDecode(b, c->output) == BLOCK_SIZE);
        for (j = 0; j < 32; ++j)
            sum += c->output[b->offsets[group * 32 + j]];
        return sum;
    case 8:
        for (j = 0; j < 32; ++j) {
            unsigned const q = b->offsets[group * 32 + j];
            CHECK(partialDecode(b, c->output, (int)q + 1) == (int)q + 1);
            sum += c->output[q];
        }
        return sum;
    case 9:
        CHECK(LZ4_pointIndexBuild(c->scratchIndex, b->compressed, b->compressedSize, b->dict,
                                  b->dictSize) == BLOCK_SIZE);
        for (j = 0; j < 32; ++j) {
            result = LZ4_pointRead(c->scratchIndex, (int)b->offsets[group * 32 + j]);
            CHECK(result >= 0);
            sum += (unsigned)result;
        }
        return sum;
    default:
        /* Original bytes represent an already retained decoded block. */
        for (j = 0; j < 32; ++j)
            sum += b->source[b->offsets[group * 32 + j]];
        return sum;
    }
}

static double timed(Corpus* c, int mode, size_t iterations)
{
    clock_t const start = clock();
    unsigned long long sum = 0;
    size_t i;
    for (i = 0; i < iterations; ++i)
        sum += operation(c, mode, i);
    consume = sum;
    return 1e9 * (double)(clock() - start) / (double)CLOCKS_PER_SEC;
}
static int compareDouble(const void* a, const void* b)
{
    double const x = *(const double*)a, y = *(const double*)b;
    return (x > y) - (x < y);
}
static void measure(Corpus* c, double* results)
{
    size_t count[METHODS];
    double values[METHODS][ROUNDS];
    int mode, round;
    for (mode = 0; mode < METHODS; ++mode) {
        size_t n = (size_t)c->count * 8;
        double elapsed;
        do {
            elapsed = timed(c, mode, n);
            if (elapsed < 3e6)
                n *= 2;
        } while (elapsed < 3e6);
        count[mode] = n;
    }
    for (round = 0; round < ROUNDS; ++round) {
        int order[METHODS] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        for (mode = METHODS - 1; mode > 0; --mode) {
            int const j = (int)(rnd() % (unsigned)(mode + 1));
            int const v = order[j];
            order[j] = order[mode];
            order[mode] = v;
        }
        for (mode = 0; mode < METHODS; ++mode) {
            int const m = order[mode];
            values[m][round] = timed(c, m, count[m]) / count[m];
        }
    }
    for (mode = 0; mode < METHODS; ++mode) {
        qsort(values[mode], ROUNDS, sizeof(double), compareDouble);
        results[mode] = values[mode][ROUNDS / 2];
    }
}
static void measureFullPointScan(Corpus* c, double* point, double* full)
{
    double samples[2][3];
    int const count = c->count < 4 ? c->count : 4;
    int round;
    for (round = 0; round < 3; ++round) {
        int step;
        for (step = 0; step < 2; ++step) {
            int const mode = (step + round) & 1;
            clock_t elapsed = 0;
            int i;
            for (i = 0; i < count; ++i) {
                Block* b = &c->b[i];
                clock_t const start = clock();
                if (mode == 0) {
                    int p;
                    for (p = 0; p < BLOCK_SIZE; ++p) {
                        int const value = LZ4_pointRead(b->index, p);
                        CHECK(value >= 0);
                        c->output[p] = (Byte)value;
                    }
                } else {
                    int repeat;
                    for (repeat = 0; repeat < 128; ++repeat)
                        CHECK(fullDecode(b, c->output) == BLOCK_SIZE);
                }
                elapsed += clock() - start;
                CHECK(memcmp(c->output, b->source, BLOCK_SIZE) == 0);
            }
            samples[mode][round] =
                1e9 * (double)elapsed / ((double)CLOCKS_PER_SEC * count * (mode == 0 ? 1 : 128));
        }
    }
    qsort(samples[0], 3, sizeof(double), compareDouble);
    qsort(samples[1], 3, sizeof(double), compareDouble);
    *point = samples[0][1];
    *full = samples[1][1];
}
static void destroy(Corpus* c)
{
    int i;
    for (i = 0; i < c->count; ++i) {
        free(c->b[i].source);
        free(c->b[i].compressed);
        free(c->b[i].dict);
        free(c->b[i].indexMemory);
    }
    free(c->output);
    free(c->scratchMemory);
}
int main(int argc, char** argv)
{
    int hc = 0, withDict = 0, arg = 1;
    while (arg < argc) {
        if (strcmp(argv[arg], "--hc9") == 0)
            hc = 1;
        else if (strcmp(argv[arg], "--dict") == 0)
            withDict = 1;
        else
            break;
        ++arg;
    }
    if (arg == argc) {
        fprintf(stderr, "usage: %s [--hc9] [--dict] corpus_file...\n", argv[0]);
        return 2;
    }
    printf("file,compressor,blocks,ratio,index_bytes,build_index_ns,cold_index_ns,cached_index_ns,"
           "partial_ns,full_ns,cached_32_ns,redecode_32_ns,decode_once_32_ns,partial_32_ns,cold_"
           "index_32_ns,cached_output_32_ns,full_point_scan_ns,full_scan_baseline_ns\n");
    for (; arg < argc; ++arg) {
        Corpus corpus;
        double r[METHODS];
        double pointScan, fullScan;
        size_t compressed = 0;
        int i;
        const char* name = strrchr(argv[arg], '/');
        memset(&corpus, 0, sizeof(corpus));
        setup(&corpus, argv[arg], hc, withDict);
        for (i = 0; i < corpus.count; ++i)
            compressed += (size_t)corpus.b[i].compressedSize;
        measure(&corpus, r);
        printf("%s,%s,%d,%.5f,%lu", name ? name + 1 : argv[arg],
               withDict ? (hc ? "hc9-dict" : "fast-dict") : (hc ? "hc9" : "fast"), corpus.count,
               (double)compressed / ((double)corpus.count * BLOCK_SIZE),
               (unsigned long)corpus.indexSize);
        for (i = 0; i < METHODS; ++i)
            printf(",%.1f", r[i]);
        measureFullPointScan(&corpus, &pointScan, &fullScan);
        printf(",%.1f,%.1f", pointScan, fullScan);
        putchar('\n');
        fflush(stdout);
        destroy(&corpus);
    }
    return 0;
}
