/* SHA-1 as specified in FIPS 180-4. */
#include "sha1.h"

#include <stdio.h>
#include <string.h>

#define ROL(v, n) (((v) << (n)) | ((v) >> (32 - (n))))

static void sha1_transform(uint32_t s[5], const uint8_t block[64])
{
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = (uint32_t)block[i * 4] << 24 | (uint32_t)block[i * 4 + 1] << 16 |
               (uint32_t)block[i * 4 + 2] << 8 | (uint32_t)block[i * 4 + 3];
    }
    for (int i = 16; i < 80; i++)
        w[i] = ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    uint32_t a = s[0], b = s[1], c = s[2], d = s[3], e = s[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        uint32_t t = ROL(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = ROL(b, 30);
        b = a;
        a = t;
    }
    s[0] += a;
    s[1] += b;
    s[2] += c;
    s[3] += d;
    s[4] += e;
}

void sha1_init(sha1_ctx *ctx)
{
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count = 0;
    ctx->buflen = 0;
}

void sha1_update(sha1_ctx *ctx, const void *data, size_t len)
{
    const uint8_t *p = data;
    ctx->count += len;
    while (len > 0) {
        size_t n = 64 - ctx->buflen;
        if (n > len)
            n = len;
        memcpy(ctx->buf + ctx->buflen, p, n);
        ctx->buflen += n;
        p += n;
        len -= n;
        if (ctx->buflen == 64) {
            sha1_transform(ctx->state, ctx->buf);
            ctx->buflen = 0;
        }
    }
}

void sha1_final(sha1_ctx *ctx, uint8_t out[SHA1_DIGEST_LEN])
{
    uint64_t bits = ctx->count * 8;
    uint8_t pad = 0x80;
    sha1_update(ctx, &pad, 1);
    pad = 0;
    while (ctx->buflen != 56)
        sha1_update(ctx, &pad, 1);

    uint8_t len_be[8];
    for (int i = 0; i < 8; i++)
        len_be[i] = (uint8_t)(bits >> (56 - 8 * i));
    sha1_update(ctx, len_be, 8);

    for (int i = 0; i < 5; i++) {
        out[i * 4] = (uint8_t)(ctx->state[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        out[i * 4 + 3] = (uint8_t)ctx->state[i];
    }
}

void sha1_to_hex(const uint8_t digest[SHA1_DIGEST_LEN], char out[SHA1_HEX_LEN + 1])
{
    for (int i = 0; i < SHA1_DIGEST_LEN; i++)
        snprintf(out + i * 2, 3, "%02x", digest[i]);
}

void sha1_hex(const void *data, size_t len, char out[SHA1_HEX_LEN + 1])
{
    sha1_ctx ctx;
    uint8_t digest[SHA1_DIGEST_LEN];
    sha1_init(&ctx);
    sha1_update(&ctx, data, len);
    sha1_final(&ctx, digest);
    sha1_to_hex(digest, out);
}
