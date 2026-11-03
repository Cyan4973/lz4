/* Copyright (c) 2026 noobpwnftw.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "lz4point_mono.h"
#define main original_benchmark_main
#include "bench_point.c"
#undef main

typedef struct {
    void* memory[BLOCKS];
    LZ4_pointMonoIndex* index[BLOCKS];
    unsigned char output[BLOCK_SIZE];
} RangeSet;
static double runRange(Corpus* c, RangeSet* set, int mode, int len, size_t iterations)
{
    clock_t const begin = clock();
    unsigned long long sum = 0;
    size_t i;
    for (i = 0; i < iterations; ++i) {
        size_t const bi = i % (size_t)c->count;
        Block* b = &c->b[bi];
        int const p =
            (int)(b->offsets[(i / (size_t)c->count) % QUERIES] % (unsigned)(BLOCK_SIZE - len + 1));
        int j;
        if (mode == 0)
            CHECK(LZ4_pointReadRange(b->index, p, set->output, len) == len);
        else if (mode == 1)
            for (j = 0; j < len; ++j) {
                int const value = LZ4_pointRead(b->index, p + j);
                CHECK(value >= 0);
                set->output[j] = (unsigned char)value;
            }
        else if (mode == 2)
            CHECK(LZ4_pointMonoReadRange(set->index[bi], p, set->output, len) == len);
        else if (mode == 3)
            for (j = 0; j < len; ++j) {
                int const value = LZ4_pointMonoRead(set->index[bi], p + j);
                CHECK(value >= 0);
                set->output[j] = (unsigned char)value;
            }
        else {
            CHECK(partialDecode(b, c->output, p + len) == p + len);
            memcpy(set->output, c->output + p, (size_t)len);
        }
        sum += set->output[0] + set->output[len - 1];
    }
    consume = sum;
    return 1e9 * (double)(clock() - begin) / CLOCKS_PER_SEC;
}
int main(int argc, char** argv)
{
    static const int lengths[] = {1, 2, 4, 8, 16, 32, 64, 256};
    int arg = 1, hc = 0, dict = 0;
    while (arg < argc) {
        if (strcmp(argv[arg], "--hc9") == 0)
            hc = 1;
        else if (strcmp(argv[arg], "--dict") == 0)
            dict = 1;
        else
            break;
        ++arg;
    }
    CHECK(arg < argc);
    puts("file,compressor,length,flat_range_ns,flat_points_ns,mono_range_ns,mono_points_ns,partial_"
         "ns");
    for (; arg < argc; ++arg) {
        Corpus c;
        RangeSet set;
        int bi, li;
        const char* file = strrchr(argv[arg], '/');
        memset(&c, 0, sizeof(c));
        memset(&set, 0, sizeof(set));
        setup(&c, argv[arg], hc, dict);
        for (bi = 0; bi < c.count; ++bi) {
            size_t const bytes = LZ4_pointMonoSize(c.b[bi].index);
            set.memory[bi] = malloc(bytes);
            CHECK(set.memory[bi]);
            set.index[bi] = LZ4_pointMonoCopy(c.b[bi].index, set.memory[bi], bytes);
            CHECK(set.index[bi]);
        }
        for (li = 0; li < (int)(sizeof(lengths) / sizeof(lengths[0])); ++li) {
            int const len = lengths[li];
            double values[5][5];
            size_t counts[5];
            int m, round;
            for (bi = 0; bi < c.count; ++bi) {
                int q;
                Block* b = &c.b[bi];
                for (q = 0; q < QUERIES; ++q) {
                    int const p = (int)(b->offsets[q] % (unsigned)(BLOCK_SIZE - len + 1));
                    CHECK(LZ4_pointReadRange(b->index, p, set.output, len) == len &&
                          !memcmp(set.output, b->source + p, (size_t)len));
                    CHECK(LZ4_pointMonoReadRange(set.index[bi], p, set.output, len) == len &&
                          !memcmp(set.output, b->source + p, (size_t)len));
                }
            }
            for (m = 0; m < 5; ++m) {
                size_t n = (size_t)c.count * 8;
                double elapsed;
                do {
                    elapsed = runRange(&c, &set, m, len, n);
                    if (elapsed < 5e6)
                        n *= 2;
                } while (elapsed < 5e6);
                counts[m] = n;
            }
            for (round = 0; round < 5; ++round) {
                int order[5] = {0, 1, 2, 3, 4};
                for (m = 4; m > 0; --m) {
                    int const q = (int)(rnd() % (unsigned)(m + 1));
                    int const t = order[q];
                    order[q] = order[m];
                    order[m] = t;
                }
                for (m = 0; m < 5; ++m) {
                    int const q = order[m];
                    values[q][round] = runRange(&c, &set, q, len, counts[q]) / counts[q];
                }
            }
            printf("%s,%s,%d", file ? file + 1 : argv[arg],
                   dict ? (hc ? "hc9-dict" : "fast-dict") : (hc ? "hc9" : "fast"), len);
            for (m = 0; m < 5; ++m) {
                qsort(values[m], 5, sizeof(double), compareDouble);
                printf(",%.2f", values[m][2]);
            }
            puts("");
            fflush(stdout);
        }
        for (bi = 0; bi < c.count; ++bi)
            free(set.memory[bi]);
        destroy(&c);
    }
    return 0;
}
