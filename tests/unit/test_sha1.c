#include <stdlib.h>

#include "../../src/sha1.h"
#include "test.h"

static void check_vector(const char *input, const char *expected)
{
    char hex[SHA1_HEX_LEN + 1];
    sha1_hex(input, strlen(input), hex);
    CHECK_STR(hex, expected);
}

void suite_sha1(void)
{
    /* FIPS 180 / RFC 3174 test vectors. */
    check_vector("", "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    check_vector("abc", "a9993e364706816aba3e25717850c26c9cd0d89d");
    check_vector("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                 "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    check_vector("The quick brown fox jumps over the lazy dog",
                 "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");

    /* Lengths around the 55/56/64-byte padding boundaries must all hash. */
    char buf[130];
    memset(buf, 'a', sizeof buf);
    char a[SHA1_HEX_LEN + 1], b[SHA1_HEX_LEN + 1];
    sha1_hex(buf, 55, a);
    sha1_hex(buf, 56, b);
    CHECK(strcmp(a, b) != 0);
    sha1_hex(buf, 64, a);
    CHECK_STR(a, "0098ba824b5c16427bd7a1122a5a442a25ec644d");

    /* Incremental updates in odd-sized pieces equal one-shot hashing. */
    sha1_ctx ctx;
    uint8_t digest[SHA1_DIGEST_LEN];
    sha1_init(&ctx);
    for (size_t off = 0; off < sizeof buf; off += 7)
        sha1_update(&ctx, buf + off, off + 7 > sizeof buf ? sizeof buf - off : 7);
    sha1_final(&ctx, digest);
    sha1_to_hex(digest, a);
    sha1_hex(buf, sizeof buf, b);
    CHECK_STR(a, b);

    /* One million 'a's (RFC 3174 test 3). */
    char *big = malloc(1000000);
    memset(big, 'a', 1000000);
    sha1_hex(big, 1000000, a);
    CHECK_STR(a, "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
    free(big);
}
