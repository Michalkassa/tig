#include "util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("tig: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(1);
}

void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p)
        die("out of memory");
    return p;
}

void *xrealloc(void *p, size_t n)
{
    p = realloc(p, n ? n : 1);
    if (!p)
        die("out of memory");
    return p;
}

char *xstrdup(const char *s)
{
    char *d = strdup(s);
    if (!d)
        die("out of memory");
    return d;
}

char *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    size_t cap = 4096, n = 0;
    char *buf = xmalloc(cap);
    size_t r;
    while ((r = fread(buf + n, 1, cap - n - 1, f)) > 0) {
        n += r;
        if (cap - n - 1 == 0) {
            cap *= 2;
            buf = xrealloc(buf, cap);
        }
    }
    if (ferror(f))
        die("cannot read '%s'", path);
    fclose(f);

    buf[n] = '\0';
    if (len)
        *len = n;
    return buf;
}

void write_file(const char *path, const void *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        die("cannot write '%s'", path);
    if (fwrite(data, 1, len, f) != len || fclose(f) != 0)
        die("cannot write '%s'", path);
}

int path_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int is_regular_file(const char *path)
{
    struct stat st;
    return lstat(path, &st) == 0 && S_ISREG(st.st_mode);
}
