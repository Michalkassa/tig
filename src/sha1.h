#ifndef SHA1_H
#define SHA1_H

#include <stddef.h>
#include <stdint.h>

#define SHA1_DIGEST_LEN 20
#define SHA1_HEX_LEN 40

typedef struct {
    uint32_t state[5];
    uint64_t count;   /* total bytes processed */
    uint8_t buf[64];
    size_t buflen;
} sha1_ctx;

void sha1_init(sha1_ctx *ctx);
void sha1_update(sha1_ctx *ctx, const void *data, size_t len);
void sha1_final(sha1_ctx *ctx, uint8_t out[SHA1_DIGEST_LEN]);

/* Convenience: hash a buffer straight to a 40-char hex string (+ NUL). */
void sha1_hex(const void *data, size_t len, char out[SHA1_HEX_LEN + 1]);
void sha1_to_hex(const uint8_t digest[SHA1_DIGEST_LEN], char out[SHA1_HEX_LEN + 1]);

#endif
