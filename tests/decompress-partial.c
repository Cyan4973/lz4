#include "stdio.h"
#include "string.h"
#include "lz4.h"

const char source[] =
  "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod\n"
  "tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim\n"
  "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea\n"
  "commodo consequat. Duis aute irure dolor in reprehenderit in voluptate\n"
  "velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat\n"
  "cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id\n"
  "est laborum.\n"
  "\n"
  "Sed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium\n"
  "doloremque laudantium, totam rem aperiam, eaque ipsa quae ab illo inventore\n"
  "veritatis et quasi architecto beatae vitae dicta sunt explicabo. Nemo enim\n"
  "ipsam voluptatem quia voluptas sit aspernatur aut odit aut fugit, sed quia\n"
  "consequuntur magni dolores eos qui ratione voluptatem sequi nesciunt. Neque\n"
  "porro quisquam est, qui dolorem ipsum quia dolor sit amet, consectetur,\n"
  "adipisci velit, sed quia non numquam eius modi tempora incidunt ut labore\n"
  "et dolore magnam aliquam quaerat voluptatem. Ut enim ad minima veniam, quis\n"
  "nostrum exercitationem ullam corporis suscipit laboriosam, nisi ut aliquid\n"
  "ex ea commodi consequatur? Quis autem vel eum iure reprehenderit qui in ea\n"
  "voluptate velit esse quam nihil molestiae consequatur, vel illum qui\n"
  "dolorem eum fugiat quo voluptas nulla pariatur?\n";

#define BUFFER_SIZE 2048

int main()
{
  int srcLen = strlen(source);
  char cmpBuffer[BUFFER_SIZE];
  char outBuffer[BUFFER_SIZE];

  printf("Length of source: %d\n", srcLen);

  int cmpSize = LZ4_compress_default(source, cmpBuffer, srcLen, BUFFER_SIZE);
  printf("Compressed size: %d\n", cmpSize);

  for (int i = (cmpSize & ~3) - 8; i < (cmpSize & ~3) + 16; ++i) {
    printf("Trying input size %d:\n", i);
    int result1 = LZ4_decompress_safe(cmpBuffer, outBuffer, i, BUFFER_SIZE);
    printf("  decompress_safe -> %s (%d)", result1 < 0 ? "FAILED" : "OK", result1);
    if (result1 == srcLen) {
      printf(" matches input? %s", memcmp(source, outBuffer, srcLen) ? "NO" : "YES");
    }
    printf("\n");
    int result2 = LZ4_decompress_safe_partial(cmpBuffer, outBuffer, i, srcLen, BUFFER_SIZE);
    printf("  decompress_safe_partial -> %s (%d)", result2 < 0 ? "FAILED" : "OK", result2);
    if (result2 == srcLen) {
      printf(" matches input? %s", memcmp(source, outBuffer, srcLen) ? "NO" : "YES");
    }
    printf("\n");
  }
  return 0;
}
