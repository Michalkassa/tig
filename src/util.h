#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

void die(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);

/* Reads a whole file into a NUL-terminated malloc'd buffer. NULL if missing. */
char *read_file(const char *path, size_t *len);
void write_file(const char *path, const void *data, size_t len);

int path_exists(const char *path);
int is_dir(const char *path);
int is_regular_file(const char *path);

#endif
