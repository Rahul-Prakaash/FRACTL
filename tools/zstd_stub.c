#include <stddef.h>

static const size_t ZSTD_FAIL = (size_t)-1;

void  *ZSTD_createCCtx(void) { return NULL; }
size_t ZSTD_freeCCtx(void *c) { (void)c; return 0; }
size_t ZSTD_CCtx_setParameter(void *c, int p, int v) { (void)c; (void)p; (void)v; return ZSTD_FAIL; }
unsigned ZSTD_isError(size_t code) { return code == ZSTD_FAIL; }
size_t ZSTD_compressBound(size_t n) { return n + (n >> 8) + 64; }
size_t ZSTD_compress2(void *c, void *d, size_t cap, const void *s, size_t n) {
    (void)c; (void)d; (void)cap; (void)s; (void)n; return ZSTD_FAIL;
}
size_t ZSTD_decompress(void *d, size_t cap, const void *s, size_t n) {
    (void)d; (void)cap; (void)s; (void)n; return ZSTD_FAIL;
}
const char *ZSTD_getErrorName(size_t code) { (void)code; return "zstd is not included in this build"; }
