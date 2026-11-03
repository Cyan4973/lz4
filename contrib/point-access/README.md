# RFC: indexed point reads with compact decode caches

> **TL;DR:** A reusable index costing about **0.11 bits per decoded byte** removes roughly **95% of the decode CPU cost of a random byte read**, compared with restarting partial decompression.

Measured medians on Silesia with 64 KiB blocks and a retained compact index. Construction is excluded; compressed input and dictionary storage are additional.

This prototype adds general-purpose random-access point reads over LZ4-compressed data. An application builds a reusable index, then reads individual decoded bytes without materializing the whole output. The index caches navigation information rather than decoded contents.

This is a standalone RFC under `contrib/`: it does not change the LZ4 format, the installed library, or its public API. Feedback on the use case, interface and possible home is welcome before library integration.

## Scope

The primitive operates on one [raw LZ4 block-format payload](../../doc/lz4_Block_format.md), with its compressed size, decoded-size bound and optional dictionary supplied by the caller. Locating blocks in a framed or linked stream and handling frame checksums remain the application's responsibility.

An index records a compressed offset and decoded offset every 256 output bytes. A read starts at a checkpoint and follows back-references until it reaches the requested literal or dictionary byte. Repeated periods in an overlapping match are folded in one step using 32-bit division. Completed indexes are immutable and shareable across readers.

Checkpoints shorten the scan to a decoded position; they do not bound reference-chain depth. A chain across many separate matches can make point reads slower than sequential decoding. Period folding collapses repetition within a match, not ancestry across matches; this is a performance worst case rather than a correctness failure.

## API sketch

```c
typedef struct LZ4_pointIndex_s LZ4_pointIndex;

size_t LZ4_pointIndexBound(int maxDecodedSize);
LZ4_pointIndex* LZ4_pointIndexInit(void* workspace, size_t workspaceSize,
                                int maxDecodedSize);
int LZ4_pointIndexBuild(LZ4_pointIndex* index, const void* src, int srcSize,
                      const void* dict, int dictSize);
int LZ4_pointRead(const LZ4_pointIndex* index, int offset);
int LZ4_pointReadRange(const LZ4_pointIndex* index, int offset, void* dst, int len);
```

The caller allocates malloc-aligned workspace using the returned bound. Building validates and indexes the complete block, returning its decoded length or a negative error. Reading returns a byte in 0–255 or a negative error. A failed build invalidates the previous index.

`LZ4_pointReadRange` copies a contiguous decoded range into caller-owned output and returns its length. Invalid arguments and ranges are rejected before writing; an empty range at the decoded end is valid. The destination must not overlap index workspace, compressed input or dictionary.

The range reader seeks to one checkpoint and walks forward like a decoder. Literals and dictionary data copy directly. Matches reuse already written output, with indexed point lookups only for sources before the requested window. The single-byte API retains its specialized fast path. The compact layout also exposes `LZ4_pointMonoReadRange` with the same contract.

The source and dictionary are borrowed: they must not overlap the workspace and must remain alive and unchanged until all readers finish. Building, reinitializing and releasing workspace require exclusive access. Read-only access needs no internal synchronization. Allocation, cache ownership and eviction remain with the application.

The index is complete before publication and does not grow on reads. An isolated byte request can already use the partial decompressor; index reuse amortizes the setup cost.

## Compact representation

Both checkpoint coordinates are nondecreasing. The [compact companion](lz4point_mono.c) stores them as two monotone vectors, each with a sampled base every 16 entries and bit-packed deltas:

```text
coordinate(i) = sample[i >> 4] + delta[i]
```

Each coordinate lookup needs two bit extractions and an addition. The scalar C90 code uses no architecture-specific intrinsics or external dependencies and [shares the resolver](lz4point_read.h) with the flat layout.

The optional helpers in [lz4point_mono.h](lz4point_mono.h) report the exact retained size, copy a completed flat index into caller-owned compact workspace, and read it. After copying, the flat construction workspace can be freed; the compact index borrows only the compressed data and dictionary. These helper names demonstrate the representation rather than commit to a second public API. Both layouts could sit behind one opaque interface.

## Measurements

<!-- SUMMARY:BEGIN -->
| Compression | Index speedup vs. partial | Build + reads speedup vs. partial |
|---|---:|---:|
| Fast, independent | 18.5× | 8.3× |
| Fast, dictionary | 29.7× | 10.0× |
| HC9, independent | 30.1× | 9.0× |
| HC9, dictionary | 66.9× | 12.4× |

Values are medians of per-file speedups over the 12 Silesia files, for 32 random requests per 64 KiB block. A retained index excludes its earlier build; the second column of speedups includes a complete build for every batch. Both indexed paths beat repeated partial decoding on all 12 files in each configuration.

The flat index uses **2,100 bytes** per block. In a separate paired benchmark, the two-vector representation used median **904–928 bytes** across configurations: **56–57% less index storage**, including metadata and read padding, with **5–8% longer lookup time**. Cached compact reads remained **19–69× faster** than repeated partial decodes. The compact helper allocates no internal memory.

Decoding once for a batch is also useful: the retained flat index beat that baseline on 6 of 12 fast/independent inputs. Retaining the decoded block gives the fastest reads when its memory cost is acceptable.
<!-- SUMMARY:END -->

These are CPU microbenchmarks on RAM-resident input. A decoded cache served requests faster on every input; its memory cost is a full decoded buffer. The index offers a smaller auxiliary cache representation for sparse reads. Full decompression remains preferable when the application needs the complete output.

The range extension is measured separately: 2-byte requests show roughly 6–20% median speedups over two point calls across the tested layouts/configurations, with larger gains for wider windows. Partial decoding can become faster as a contiguous window grows; see the range results for that tradeoff.

See [BENCHMARKS.md](BENCHMARKS.md) for the sampling method, baselines, construction costs, raw results and reproduction commands. No cache-hit-rate advantage is assumed in these measurements.

## Build and validate

From this directory:

```sh
make check
make check-large
make check-sanitize
```

`check` runs generated-data, dictionary, malformed-input, lifetime, guarded range and exact-workspace checks, plus C++ compilation. `check-large` exercises offsets across 2,113,929,216 decoded bytes, including 31-bit sample and delta values. Its construction is checked against a full reference decode at 64 KiB; it does not allocate a 2 GiB decoded buffer. Sanitizer tests instrument the prototype and reference codec.

The code remains experimental. Dedicated fuzzing, 32-bit execution and additional architectures are needed before production integration.

## Feedback sought

- Would this primitive be useful in LZ4, and would an experimental API or a separate component be the right home?
- Does caller-owned workspace with borrowed source/dictionary buffers fit the intended use?
- Does a byte plus contiguous-range interface cover the useful cases, or would sparse batched offsets be valuable?

## Origin and license

The checkpoint and monotone-vector schemes are adapted from the public-domain [point-access implementation](https://github.com/noobpwnftw/chesstb/blob/28d45d27ef22441bf844d3e9b4dd424eb032353f/src/util/compress.h) and [monotone integer vectors](https://github.com/noobpwnftw/chesstb/blob/28d45d27ef22441bf844d3e9b4dd424eb032353f/src/util/mono_uint_vec.h) in chesstb. The prototype is distributed under [BSD-2-Clause](LICENSE).
