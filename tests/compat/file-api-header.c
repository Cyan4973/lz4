/* Compile against installed headers, without the source tree on the include path. */
#include <lz4file.h>

LZ4F_errorCode_t lz4_file_api_header(LZ4_readFile_t* file);
LZ4F_errorCode_t lz4_file_api_header(LZ4_readFile_t* file)
{
    return LZ4F_readClose(file);
}
