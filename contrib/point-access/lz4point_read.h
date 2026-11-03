/* Copyright (c) 2026 noobpwnftw.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <string.h>

/* Private resolver template; intentionally included for each index layout. */
int LZ4P_READ_NAME(const LZ4P_READ_TYPE* index, int offset)
{
    long long target;
    if (index == NULL || !index->valid || offset < 0 || (LZ4P_U32)offset >= index->decodedSize)
        return LZ4P_FAIL;
    target = offset;
    for (;;) {
        LZ4P_Checkpoint const start =
            LZ4P_READ_CHECKPOINT(index, (size_t)target >> LZ4P_STRIDE_LOG);
        const LZ4P_Byte* p = index->src + start.input;
        long long output = start.output;
        for (;;) {
            unsigned const token = *p++;
            size_t literals = token >> 4;
            const LZ4P_Byte* literalData;
            size_t match;
            unsigned distance;
            if (literals == 15) {
                unsigned extension;
                do {
                    extension = *p++;
                    literals += extension;
                } while (extension == 255);
            }
            literalData = p;
            p += literals;
            if (target < output + (long long)literals)
                return literalData[(size_t)(target - output)];
            output += (long long)literals;
            distance = (unsigned)p[0] | ((unsigned)p[1] << 8);
            p += 2;
            match = (token & 15) + 4;
            if ((token & 15) == 15) {
                unsigned extension;
                do {
                    extension = *p++;
                    match += extension;
                } while (extension == 255);
            }
            if (target < output + (long long)match) {
                /* Keep the period division 32-bit; widen only its result. */
                LZ4P_U32 const into = (LZ4P_U32)(target - output);
                target -=
                    into < distance ? distance : ((long long)(into / distance) + 1) * distance;
                if (target < 0)
                    return index->dict[(size_t)((long long)index->dictSize + target)];
                break;
            }
            output += (long long)match;
        }
    }
}

/* Walk forward from one checkpoint. Only match sources before the requested
 * window need point lookups; dictionary and already produced bytes copy directly. */
int LZ4P_RANGE_NAME(const LZ4P_READ_TYPE* index, int offset, void* dst, int len)
{
    LZ4P_Byte* const out = (LZ4P_Byte*)dst;
    LZ4P_Checkpoint start;
    const LZ4P_Byte* p;
    size_t output;
    size_t done = 0;
    if (index == NULL || !index->valid || offset < 0 || len < 0 ||
        (LZ4P_U32)offset > index->decodedSize ||
        (LZ4P_U32)len > index->decodedSize - (LZ4P_U32)offset || (len && dst == NULL))
        return LZ4P_FAIL;
    if (!len)
        return 0;
    start = LZ4P_READ_CHECKPOINT(index, (size_t)offset >> LZ4P_STRIDE_LOG);
    p = index->src + start.input;
    output = start.output;
    for (;;) {
        unsigned const token = *p++;
        size_t literals = token >> 4;
        size_t match;
        unsigned distance;
        if (literals == 15) {
            unsigned extension;
            do {
                extension = *p++;
                literals += extension;
            } while (extension == 255);
        }
        if ((size_t)offset + done < output + literals) {
            size_t n = output + literals - ((size_t)offset + done);
            if (n > (size_t)len - done)
                n = (size_t)len - done;
            memcpy(out + done, p + ((size_t)offset + done - output), n);
            done += n;
            if (done == (size_t)len)
                return len;
        }
        p += literals;
        output += literals;
        distance = (unsigned)p[0] | ((unsigned)p[1] << 8);
        p += 2;
        match = (token & 15) + 4;
        if ((token & 15) == 15) {
            unsigned extension;
            do {
                extension = *p++;
                match += extension;
            } while (extension == 255);
        }
        if ((size_t)offset + done < output + match) {
            long long source = (long long)offset + (long long)done - distance;
            size_t n = output + match - ((size_t)offset + done);
            if (n > (size_t)len - done)
                n = (size_t)len - done;
            while (n) {
                if (source < 0) {
                    size_t const count = n < (size_t)(-source) ? n : (size_t)(-source);
                    memcpy(out + done, index->dict + (size_t)((long long)index->dictSize + source),
                           count);
                    source += (long long)count;
                    done += count;
                    n -= count;
                } else if (source < offset) {
                    out[done++] = (LZ4P_Byte)LZ4P_READ_NAME(index, (int)source);
                    ++source;
                    --n;
                } else {
                    size_t const first = (size_t)(source - offset);
                    size_t written = 0;
                    size_t available = done - first;
                    /* Grow an overlapping match from its known period using
                     * nonoverlapping memcpy calls, doubling the available prefix. */
                    while (written < n) {
                        size_t const count = available < n - written ? available : n - written;
                        memcpy(out + done + written, out + first, count);
                        written += count;
                        available += count;
                    }
                    done += n;
                    n = 0;
                }
            }
            if (done == (size_t)len)
                return len;
        }
        output += match;
    }
}

#undef LZ4P_RANGE_NAME

#undef LZ4P_READ_NAME
#undef LZ4P_READ_TYPE
#undef LZ4P_READ_CHECKPOINT
