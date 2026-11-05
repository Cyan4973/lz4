/* Check that the installed headers include all their dependencies. */
#include <lz4.h>
#include <lz4hc.h>
#include <lz4frame.h>
#include <lz4file.h>

int lz4_installed_headers(void);
int lz4_installed_headers(void) { return LZ4_VERSION_NUMBER; }
