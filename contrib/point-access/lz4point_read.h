/* Copyright (c) 2026 noobpwnftw.
 * SPDX-License-Identifier: BSD-2-Clause
 */
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

#undef LZ4P_READ_NAME
#undef LZ4P_READ_TYPE
#undef LZ4P_READ_CHECKPOINT
