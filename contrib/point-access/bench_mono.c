/* Copyright (c) 2026 noobpwnftw.
 * SPDX-License-Identifier: BSD-2-Clause
 */
/* Paired flat/compact C implementation, same data and requests in all modes. */
#include "lz4point_mono.h"
#define main original_benchmark_main
#include "bench_point.c"
#undef main

typedef struct {
    LZ4_pointMonoIndex* index[BLOCKS];
    void* memory[BLOCKS];
    size_t bytes[BLOCKS];
    void* scratch;
    size_t capacity;
} MonoSet;
static double runMono(Corpus* c, MonoSet* set, int mode, size_t n)
{
    clock_t const begin = clock();
    unsigned long long sum = 0;
    size_t i;
    for (i = 0; i < n; ++i) {
        size_t const bi = i % (size_t)c->count, group = (i / (size_t)c->count) % 8;
        Block* b = &c->b[bi];
        LZ4_pointMonoIndex* mono = set->index[bi];
        int j;
        if (mode == 2) {
            sum += LZ4_pointMonoSize(b->index);
            continue;
        }
        if (mode == 3) {
            CHECK(LZ4_pointMonoCopy(b->index, set->scratch, set->capacity));
            continue;
        }
        if (mode == 4) {
            CHECK(LZ4_pointIndexBuild(c->scratchIndex, b->compressed, b->compressedSize, b->dict,
                                      b->dictSize) == BLOCK_SIZE);
            mono = LZ4_pointMonoCopy(c->scratchIndex, set->scratch, set->capacity);
            CHECK(mono);
        }
        for (j = 0; j < 32; ++j) {
            int const p = (int)b->offsets[group * 32 + j];
            int value;
            if (mode == 0)
                value = LZ4_pointRead(b->index, p);
            else if (mode == 5) {
                CHECK(partialDecode(b, c->output, p + 1) == p + 1);
                value = c->output[p];
            } else
                value = LZ4_pointMonoRead(mono, p);
            CHECK(value >= 0);
            sum += (unsigned)value;
        }
    }
    consume = sum;
    return 1e9 * (double)(clock() - begin) / CLOCKS_PER_SEC;
}
int main(int argc, char** argv)
{
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
    puts("file,compressor,flat_bytes,mono_bytes,flat_32_ns,mono_32_ns,size_ns,copy_ns,build_copy_"
         "32_ns,partial_32_ns");
    for (; arg < argc; ++arg) {
        Corpus c;
        MonoSet set;
        int bi, j, m, r;
        double values[6][7];
        size_t counts[6];
        double bytes = 0;
        const char* file = strrchr(argv[arg], '/');
        memset(&c, 0, sizeof(c));
        memset(&set, 0, sizeof(set));
        setup(&c, argv[arg], hc, dict);
        for (bi = 0; bi < c.count; ++bi) {
            Block* b = &c.b[bi];
            set.bytes[bi] = LZ4_pointMonoSize(b->index);
            CHECK(set.bytes[bi]);
            set.memory[bi] = malloc(set.bytes[bi]);
            CHECK(set.memory[bi]);
            set.index[bi] = LZ4_pointMonoCopy(b->index, set.memory[bi], set.bytes[bi]);
            CHECK(set.index[bi]);
            bytes += set.bytes[bi];
            if (set.bytes[bi] > set.capacity)
                set.capacity = set.bytes[bi];
            for (j = 0; j < (bi == 0 ? BLOCK_SIZE : QUERIES); ++j) {
                int const p = bi == 0 ? j : (int)b->offsets[j];
                CHECK(LZ4_pointMonoRead(set.index[bi], p) == b->source[p]);
            }
        }
        set.scratch = malloc(set.capacity);
        CHECK(set.scratch);
        for (m = 0; m < 6; ++m) {
            size_t n = (size_t)c.count * 8;
            double t;
            do {
                t = runMono(&c, &set, m, n);
                if (t < 1e7)
                    n *= 2;
            } while (t < 1e7);
            counts[m] = n;
        }
        for (r = 0; r < 7; ++r) {
            int order[6] = {0, 1, 2, 3, 4, 5};
            for (m = 5; m > 0; --m) {
                int const k = (int)(rnd() % (unsigned)(m + 1));
                int const t = order[k];
                order[k] = order[m];
                order[m] = t;
            }
            for (m = 0; m < 6; ++m) {
                int const k = order[m];
                values[k][r] = runMono(&c, &set, k, counts[k]) / counts[k];
            }
        }
        printf("%s,%s,%lu,%.2f", file ? file + 1 : argv[arg],
               dict ? (hc ? "hc9-dict" : "fast-dict") : (hc ? "hc9" : "fast"),
               (unsigned long)c.indexSize, bytes / c.count);
        for (m = 0; m < 6; ++m) {
            qsort(values[m], 7, sizeof(double), compareDouble);
            printf(",%.2f", values[m][3]);
        }
        puts("");
        fflush(stdout);
        for (bi = 0; bi < c.count; ++bi)
            free(set.memory[bi]);
        free(set.scratch);
        destroy(&c);
    }
    return 0;
}
