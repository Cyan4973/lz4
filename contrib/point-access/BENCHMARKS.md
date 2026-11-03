# Point-access benchmarks

The benchmark uses all 12 files of the [Silesia corpus](https://sun.aei.polsl.pl/~sdeor/index.php?page=silesia), as used by LZ4's published benchmarks. Each file contributes 16 full 64 KiB blocks selected deterministically from across the file. Each block has 256 pseudo-random decoded offsets, grouped into batches of 32; offsets may repeat.

## Results

<!-- RESULTS:BEGIN -->
| Compression | Index speedup vs. partial | Build + reads speedup vs. partial |
|---|---:|---:|
| Fast, independent | 18.5× | 8.3× |
| Fast, dictionary | 29.7× | 10.0× |
| HC9, independent | 30.1× | 9.0× |
| HC9, dictionary | 66.9× | 12.4× |

Values are medians of per-file speedups over the 12 Silesia files, for 32 random requests per 64 KiB block. A retained index excludes its earlier build; the second column of speedups includes a complete build for every batch. Both indexed paths beat repeated partial decoding on all 12 files in each configuration.

The independently paired compact benchmark gives:

| Compression | Compact bytes | Compact / flat lookup time | Compact speedup vs. partial |
|---|---:|---:|---:|
| Fast, independent | 928.0 | 1.051× | 18.8× |
| Fast, dictionary | 926.9 | 1.063× | 28.9× |
| HC9, independent | 926.3 | 1.067× | 31.0× |
| HC9, dictionary | 904.4 | 1.083× | 68.6× |

Compact / flat values above 1 mean longer compact lookup time. Both timings exclude construction. Reported sizes average the 16 sampled blocks per file, then take the median across files.

### Fast, independent blocks: per-file CPU times

Microseconds per operation. All read columns serve the same 32 offsets; `Build` is construction alone. The last column reads already retained decoded bytes.

| File | Build | Cached index | Build + reads | Partial each | Full decode once | Cached decoded |
|---|---:|---:|---:|---:|---:|---:|
| dickens | 20.188 | 10.014 | 29.992 | 321.008 | 20.863 | 0.016 |
| mozilla | 10.818 | 16.535 | 27.250 | 181.148 | 11.596 | 0.016 |
| mr | 15.094 | 39.594 | 54.281 | 262.570 | 16.469 | 0.016 |
| nci | 8.680 | 15.090 | 24.297 | 103.281 | 6.732 | 0.016 |
| ooffice | 13.262 | 4.753 | 18.152 | 189.867 | 12.750 | 0.016 |
| osdb | 11.691 | 3.580 | 15.211 | 129.672 | 9.098 | 0.016 |
| reymont | 19.672 | 20.184 | 39.578 | 318.055 | 20.043 | 0.016 |
| samba | 11.348 | 10.596 | 22.496 | 163.781 | 10.457 | 0.016 |
| sao | 16.270 | 4.843 | 21.281 | 181.203 | 14.887 | 0.017 |
| webster | 16.203 | 10.982 | 27.164 | 233.039 | 15.453 | 0.016 |
| x-ray | 5.106 | 0.558 | 5.696 | 65.852 | 4.693 | 0.016 |
| xml | 9.342 | 12.762 | 23.430 | 121.148 | 7.664 | 0.016 |
<!-- RESULTS:END -->

## Cost per decoded byte

The compact index's median 904–928 bytes per 64 KiB decoded block corresponds to about **0.11 bits of index per decoded byte** (`928 * 8 / 65536 = 0.1133`). This includes context and read padding, but excludes compressed input and dictionary storage.

For the one-byte requests in the range benchmark (`length == 1`), the median per-file compact point-read speedup over a separate safe partial decode is **21.9×** for fast/independent blocks: about **95.4% less CPU time**. The corresponding median reductions are 97.3% for fast/dictionary, 97.1% for HC9/independent and 98.9% for HC9/dictionary. This is the basis of the short summary, with index construction excluded.

These are workload medians, not a bound on every request. Checkpoints limit local navigation but not match ancestry. Adversarial chains across separate matches can make point chasing more expensive than sequential decoding even with a retained index.

## Methods

The two programs measure different aspects of the same implementation:

- `bench_point.c`: index construction alone, construction plus a read, retained-index reads, safe partial/full decoding, 32-read batches, retained decoded output and a scalar scan of every output byte.
- `bench_mono.c`: paired flat/compact reads, exact compact-size planning, copying into preallocated compact workspace, a full flat build followed by compaction and 32 reads, and 32 separate partial decodes.

All compared methods within a program receive the same compressed blocks, dictionaries and decoded offsets. Method order is shuffled between rounds. The PRNG also controls sampling, so changing the method schedule changes samples for subsequent files. Ratios are calculated within each CSV; the two programs' absolute times should not be divided into one another.

The flat benchmark uses five interleaved rounds of at least 3 ms per timing sample; the compact benchmark uses seven rounds of at least 10 ms. Both report median process CPU time per operation using `clock()`. Allocation, compression and file I/O are excluded. Input, dictionaries and scratch buffers are RAM-resident.

Each configuration uses LZ4 fast compression or HC level 9, with independent blocks or an external dictionary containing the preceding 64 KiB. Dictionary sampling excludes the file's first block. The decoder and index receive the same dictionary. Benchmark setup checks every sampled result against original bytes and safe decompression, plus every byte of one block per file.

## Baselines and memory accounting

Partial decoding uses `LZ4_decompress_safe_partial` or its dictionary counterpart up to the requested byte. The benchmark supplies a preallocated 64 KiB destination, although only the prefix is needed. Full-decoding baselines use the corresponding safe API.

The 32-read comparisons distinguish repeated decoding, decoding once for the batch, retaining the decoded block, retaining an index, and building an index for the batch. Retained-index timings exclude the earlier build; retained-decoded timings exclude the earlier decode. The raw results include the cached-decoded baseline, which is faster than index lookups on every input.

The flat index occupies 2,100 bytes for a 64 KiB block, including context. Compact sizes include the complete context, both vector descriptors, packed samples/deltas and two eight-byte read-padding regions. The compact allocation actually replaces the flat one after copying. Simply packing inside a retained 2,100-byte allocation would not reclaim its capacity.

Both index sizes exclude allocator bookkeeping, caller cache containers and borrowed compressed input/dictionary storage. A decoded cache can release compressed input if its application permits it. Cache hit rates under an equal total memory budget are not measured.

Compaction temporarily retains both indexes. `copy_ns` includes size planning and encoding into preallocated workspace; the separate `size_ns` column measures planning alone. `build_copy_32_ns` includes a full flat build, compaction and all 32 reads, using buffers preallocated for the sampled sizes. Compact helpers allocate no internal memory.

The scalar full-output scan compares point reads with safe full decompression over the same four blocks. It uses three rounds; the reference decoder repeats 128 times per block to reduce timer-resolution effects. This measures the scalar interface, not an optimized batch/range API.

## Host and revision

Recorded on a Cortex-X925 core, pinned to CPU 5, with a maximum reported frequency of 3.9 GHz; Linux 7.0.0-1019-nvidia, AArch64. GCC 13.3.0, `-O3 -DNDEBUG -march=native`, C90. LZ4 base revision: `4fab7ab18e89095bda933ac2de6d261c5a009085` (development tree reporting 1.11.0).

Normal frequency variation and system noise still apply. These measurements establish the tradeoff for this prototype, sample set and host, not universal speedups. Further compiler/architecture measurements would help assess an upstream API.

## Reproduce

From `contrib/point-access`:

```sh
python3 fetch_silesia.py
make clean
make bench CFLAGS='-O3 -DNDEBUG -march=native' BENCH_PREFIX='taskset -c 5'
```

The fetcher downloads from the corpus author's site and checks the published MD5 digests. Corpus files are not included. Use a valid CPU for the host or omit `BENCH_PREFIX`; results are written under ignored `measurements/`, leaving the recorded `results/` snapshot intact. `SILESIA_DIR=/path/to/silesia` reuses an existing corpus directory. Build, test and benchmark code need only this directory and LZ4's `lib/` sources.

## Contiguous range reads

`LZ4_pointReadRange(index, offset, dst, len)` seeks once and walks the requested window forward. Literals and dictionaries copy directly, matches reuse already written output, and indexed point reads resolve sources before the window. The compact entrypoint follows the same shared resolver. Both allocate no internal memory and reject invalid ranges before writing.

These additional runs time **one contiguous request**, not the 32 independent requests in the earlier tables. Indexes are already cached. Widths are 1, 2, 4, 8, 16, 32, 64 and 256 bytes. Repeated-point baselines write each byte to the same destination; the partial-decode baseline decompresses once to the end of the window, then copies the requested bytes. Each configuration uses matching offsets across all methods, five rounds, and checked output against original bytes.

The table reports median per-file speedups of the range call over repeated point calls. Cells are **flat / compact**; values above 1 favor the range call.

| Compression | 2 bytes | 8 bytes | 32 bytes | 256 bytes |
|---|---:|---:|---:|---:|
| Fast, independent | 1.06× / 1.06× | 1.12× / 1.12× | 1.20× / 1.21× | 1.35× / 1.35× |
| Fast, dictionary | 1.08× / 1.09× | 1.17× / 1.18× | 1.28× / 1.28× | 1.56× / 1.58× |
| HC9, independent | 1.07× / 1.08× | 1.17× / 1.18× | 1.26× / 1.25× | 1.32× / 1.32× |
| HC9, dictionary | 1.19× / 1.20× | 1.49× / 1.52× | 1.65× / 1.69× | 1.78× / 1.79× |

Each timing uses five interleaved rounds of at least 5 ms. Each timing cycles through eight fixed offsets per block (every 32nd generated offset), mapped into the valid range for that width. Every adaptive iteration count is a multiple of that complete cycle, so all methods time identical requests. This is a separate query schedule from the original point-read benchmarks; ratios are calculated within each range CSV. Lower-level codec and host settings match the earlier runs.

Range calls reduce repeated navigation, but do not replace bulk decoding. In the fast/independent configuration, cached flat 2-byte reads were a median 13.3× faster than a partial decode, while a partial decode was faster than the indexed range at 32 and 256 bytes. Wider matches may require many external point lookups before enough output is available for direct copies. Keep the byte primitive for isolated bytes, range reads for small contiguous values/windows, and compare partial/full decoding when the requested window grows.

Validation adds 128,000 guarded flat/compact ranges over 2,000 generated blocks, dictionaries, short-period overlaps, checkpoint crossings, zero-length/end requests, invalid arguments and reads after freeing flat workspace. Large-offset checks also cross sequence boundaries near the 2.11 GB limit. ASan and UBSan cover the range cases.

Reproduce with:

```sh
make bench-ranges CFLAGS='-O3 -DNDEBUG -march=native' BENCH_PREFIX='taskset -c 5'
```

## Raw data

<!-- LINKS:BEGIN -->
- [Fast, independent: flat](results/flat-fast.csv), [compact](results/mono-fast.csv)
- [Fast, dictionary: flat](results/flat-fast-dict.csv), [compact](results/mono-fast-dict.csv)
- [HC9, independent: flat](results/flat-hc9.csv), [compact](results/mono-hc9.csv)
- [HC9, dictionary: flat](results/flat-hc9-dict.csv), [compact](results/mono-hc9-dict.csv)
<!-- LINKS:END -->

### Range results

- [Fast, independent](results/range-fast.csv)
- [Fast, dictionary](results/range-fast-dict.csv)
- [HC9, independent](results/range-hc9.csv)
- [HC9, dictionary](results/range-hc9-dict.csv)
