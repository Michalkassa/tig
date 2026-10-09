#include "snapshot.h"

#include <stdlib.h>
#include <string.h>

#include "util.h"

void snap_init(snapshot *s)
{
    s->items = NULL;
    s->len = s->cap = 0;
}

void snap_free(snapshot *s)
{
    for (size_t i = 0; i < s->len; i++)
        free(s->items[i].path);
    free(s->items);
    snap_init(s);
}

/* Binary search: returns the index of path, or where it would be inserted. */
static size_t snap_pos(const snapshot *s, const char *path, int *found)
{
    size_t lo = 0, hi = s->len;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int c = strcmp(s->items[mid].path, path);
        if (c == 0) {
            *found = 1;
            return mid;
        }
        if (c < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    *found = 0;
    return lo;
}

entry *snap_find(const snapshot *s, const char *path)
{
    int found;
    size_t i = snap_pos(s, path, &found);
    return found ? &s->items[i] : NULL;
}

void snap_set(snapshot *s, const char *path, const char *hash)
{
    int found;
    size_t i = snap_pos(s, path, &found);
    if (!found) {
        if (s->len == s->cap) {
            s->cap = s->cap ? s->cap * 2 : 16;
            s->items = xrealloc(s->items, s->cap * sizeof(entry));
        }
        memmove(&s->items[i + 1], &s->items[i], (s->len - i) * sizeof(entry));
        s->items[i].path = xstrdup(path);
        s->len++;
    }
    memcpy(s->items[i].hash, hash, SHA1_HEX_LEN + 1);
}

int snap_remove(snapshot *s, const char *path)
{
    int found;
    size_t i = snap_pos(s, path, &found);
    if (!found)
        return 0;
    free(s->items[i].path);
    memmove(&s->items[i], &s->items[i + 1], (s->len - i - 1) * sizeof(entry));
    s->len--;
    return 1;
}

int snap_equal(const snapshot *a, const snapshot *b)
{
    if (a->len != b->len)
        return 0;
    for (size_t i = 0; i < a->len; i++) {
        if (strcmp(a->items[i].path, b->items[i].path) != 0 ||
            strcmp(a->items[i].hash, b->items[i].hash) != 0)
            return 0;
    }
    return 1;
}

void snap_parse(snapshot *s, const char *text)
{
    const char *p = text;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t linelen = nl ? (size_t)(nl - p) : strlen(p);
        if (linelen > SHA1_HEX_LEN + 1 && p[SHA1_HEX_LEN] == ' ') {
            char hash[SHA1_HEX_LEN + 1];
            memcpy(hash, p, SHA1_HEX_LEN);
            hash[SHA1_HEX_LEN] = '\0';

            size_t plen = linelen - SHA1_HEX_LEN - 1;
            char *path = xmalloc(plen + 1);
            memcpy(path, p + SHA1_HEX_LEN + 1, plen);
            path[plen] = '\0';

            snap_set(s, path, hash);
            free(path);
        }
        if (!nl)
            break;
        p = nl + 1;
    }
}

char *snap_serialize(const snapshot *s, size_t *len)
{
    size_t total = 0;
    for (size_t i = 0; i < s->len; i++)
        total += SHA1_HEX_LEN + 1 + strlen(s->items[i].path) + 1;

    char *buf = xmalloc(total + 1), *p = buf;
    for (size_t i = 0; i < s->len; i++) {
        memcpy(p, s->items[i].hash, SHA1_HEX_LEN);
        p += SHA1_HEX_LEN;
        *p++ = ' ';
        size_t plen = strlen(s->items[i].path);
        memcpy(p, s->items[i].path, plen);
        p += plen;
        *p++ = '\n';
    }
    *p = '\0';
    if (len)
        *len = total;
    return buf;
}
