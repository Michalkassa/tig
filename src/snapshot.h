#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <stddef.h>

#include "sha1.h"

/* One tracked file: path relative to the repo root + SHA-1 of its content. */
typedef struct {
    char *path;
    char hash[SHA1_HEX_LEN + 1];
} entry;

/* A snapshot is a list of entries kept sorted by path.
 * Used both for the staging area (index) and for each commit's file list. */
typedef struct {
    entry *items;
    size_t len, cap;
} snapshot;

void snap_init(snapshot *s);
void snap_free(snapshot *s);

entry *snap_find(const snapshot *s, const char *path);
void snap_set(snapshot *s, const char *path, const char *hash);
int snap_remove(snapshot *s, const char *path);
int snap_equal(const snapshot *a, const snapshot *b);

/* Text form: one "<hash> <path>\n" line per entry. */
void snap_parse(snapshot *s, const char *text);
char *snap_serialize(const snapshot *s, size_t *len);

#endif
