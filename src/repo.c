#include "repo.h"

#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util.h"

char *repo_open(void)
{
    char start[PATH_MAX], cur[PATH_MAX];
    if (!getcwd(start, sizeof start))
        die("cannot get current directory");

    while (!is_dir(TIG_DIR)) {
        if (!getcwd(cur, sizeof cur) || strcmp(cur, "/") == 0)
            die("not a tig repository (run 'tig init')");
        if (chdir("..") != 0)
            die("cannot leave '%s'", cur);
    }
    if (!getcwd(cur, sizeof cur))
        die("cannot get current directory");

    size_t rootlen = strlen(cur);
    if (strcmp(cur, "/") == 0)
        rootlen = 0;
    const char *rel = start + rootlen;
    while (*rel == '/')
        rel++;
    return xstrdup(rel);
}

int hash_worktree_file(const char *path, char out[SHA1_HEX_LEN + 1])
{
    size_t len;
    char *data = read_file(path, &len);
    if (!data)
        return 0;
    sha1_hex(data, len, out);
    free(data);
    return 1;
}

void store_blob(const char *path, char out[SHA1_HEX_LEN + 1])
{
    size_t len;
    char *data = read_file(path, &len);
    if (!data)
        die("cannot read '%s'", path);
    sha1_hex(data, len, out);

    /* Content-addressed: identical content is stored only once. */
    char obj[sizeof OBJECTS_DIR + SHA1_HEX_LEN + 2];
    snprintf(obj, sizeof obj, "%s/%s", OBJECTS_DIR, out);
    if (!path_exists(obj))
        write_file(obj, data, len);
    free(data);
}

void index_load(snapshot *s)
{
    snap_init(s);
    char *text = read_file(INDEX_FILE, NULL);
    if (text) {
        snap_parse(s, text);
        free(text);
    }
}

void index_save(const snapshot *s)
{
    size_t len;
    char *text = snap_serialize(s, &len);
    write_file(INDEX_FILE, text, len);
    free(text);
}

char *blob_read(const char *hash, size_t *len)
{
    char obj[sizeof OBJECTS_DIR + SHA1_HEX_LEN + 2];
    snprintf(obj, sizeof obj, "%s/%s", OBJECTS_DIR, hash);
    return read_file(obj, len);
}

/* Reads a file holding a hash on its first line. */
static int read_hash_file(const char *path, char out[SHA1_HEX_LEN + 1])
{
    char *text = read_file(path, NULL);
    if (!text)
        return 0;
    int ok = strlen(text) >= SHA1_HEX_LEN;
    if (ok) {
        memcpy(out, text, SHA1_HEX_LEN);
        out[SHA1_HEX_LEN] = '\0';
    }
    free(text);
    return ok;
}

static void write_line(const char *path, const char *prefix, const char *value)
{
    size_t len = strlen(prefix) + strlen(value) + 2;
    char *line = xmalloc(len);
    snprintf(line, len, "%s%s\n", prefix, value);
    write_file(path, line, strlen(line));
    free(line);
}

static char *ref_path(const char *name)
{
    size_t len = sizeof REFS_DIR + strlen(name) + 2;
    char *p = xmalloc(len);
    snprintf(p, len, "%s/%s", REFS_DIR, name);
    return p;
}

char *head_branch(void)
{
    char *text = read_file(HEAD_FILE, NULL);
    if (!text)
        die("missing %s", HEAD_FILE);
    if (strncmp(text, "ref: ", 5) != 0) {
        free(text);
        return NULL;
    }
    char *name = xstrdup(text + 5);
    free(text);
    name[strcspn(name, "\r\n")] = '\0';
    return name;
}

int head_read(char out[SHA1_HEX_LEN + 1])
{
    char *branch = head_branch();
    int ok = branch ? ref_read(branch, out) : read_hash_file(HEAD_FILE, out);
    free(branch);
    return ok;
}

void head_set_branch(const char *name)
{
    write_line(HEAD_FILE, "ref: ", name);
}

void head_set_detached(const char *hash)
{
    write_line(HEAD_FILE, "", hash);
}

void head_advance(const char *hash)
{
    char *branch = head_branch();
    if (branch)
        ref_write(branch, hash);
    else
        head_set_detached(hash);
    free(branch);
}

int valid_branch_name(const char *name)
{
    if (!name[0] || name[0] == '-' || name[0] == '.' || strcmp(name, "HEAD") == 0)
        return 0;
    for (const char *p = name; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '-' || *p == '_' || *p == '.'))
            return 0;
    }
    return 1;
}

int ref_exists(const char *name)
{
    char *p = ref_path(name);
    int ok = is_regular_file(p);
    free(p);
    return ok;
}

int ref_read(const char *name, char out[SHA1_HEX_LEN + 1])
{
    char *p = ref_path(name);
    int ok = read_hash_file(p, out);
    free(p);
    return ok;
}

void ref_write(const char *name, const char *hash)
{
    char *p = ref_path(name);
    write_line(p, "", hash);
    free(p);
}

int ref_delete(const char *name)
{
    char *p = ref_path(name);
    int ok = unlink(p) == 0;
    free(p);
    return ok;
}

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

char **list_branches(size_t *count)
{
    char **names = NULL;
    size_t n = 0, cap = 0;
    DIR *d = opendir(REFS_DIR);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d))) {
            if (!valid_branch_name(de->d_name) || !ref_exists(de->d_name))
                continue;
            if (n == cap) {
                cap = cap ? cap * 2 : 8;
                names = xrealloc(names, cap * sizeof *names);
            }
            names[n++] = xstrdup(de->d_name);
        }
        closedir(d);
    }
    if (n)
        qsort(names, n, sizeof *names, cmp_str);
    *count = n;
    return names;
}

void free_names(char **names, size_t count)
{
    for (size_t i = 0; i < count; i++)
        free(names[i]);
    free(names);
}

/*
 * Commit file format (.tig/commits/<hash>):
 *   parent <hash or "none">
 *   time <unix seconds>
 *   message <one line>
 *
 *   <hash> <path>
 *   ...
 */
int commit_load(const char *hash, commit *c)
{
    char path[sizeof COMMITS_DIR + SHA1_HEX_LEN + 2];
    snprintf(path, sizeof path, "%s/%s", COMMITS_DIR, hash);
    char *text = read_file(path, NULL);
    if (!text)
        return 0;

    memset(c, 0, sizeof *c);
    snprintf(c->hash, sizeof c->hash, "%s", hash);
    snap_init(&c->files);

    char *p = text;
    while (*p && *p != '\n') {
        char *nl = strchr(p, '\n');
        if (nl)
            *nl = '\0';
        if (strncmp(p, "parent ", 7) == 0) {
            if (strcmp(p + 7, "none") != 0)
                snprintf(c->parent, sizeof c->parent, "%s", p + 7);
        } else if (strncmp(p, "time ", 5) == 0) {
            c->time = (time_t)strtoll(p + 5, NULL, 10);
        } else if (strncmp(p, "message ", 8) == 0) {
            c->message = xstrdup(p + 8);
        }
        if (!nl) {
            p += strlen(p);
            break;
        }
        p = nl + 1;
    }
    if (*p == '\n')
        snap_parse(&c->files, p + 1);
    if (!c->message)
        c->message = xstrdup("");

    free(text);
    return 1;
}

void commit_free(commit *c)
{
    free(c->message);
    c->message = NULL;
    snap_free(&c->files);
}

void commit_write(commit *c)
{
    /* Commit hash = SHA-1 over the parent hash and every "<file hash> <path>". */
    size_t flen;
    char *files = snap_serialize(&c->files, &flen);

    sha1_ctx ctx;
    uint8_t digest[SHA1_DIGEST_LEN];
    sha1_init(&ctx);
    sha1_update(&ctx, "parent ", 7);
    sha1_update(&ctx, c->parent, strlen(c->parent));
    sha1_update(&ctx, "\n", 1);
    sha1_update(&ctx, files, flen);
    sha1_final(&ctx, digest);
    sha1_to_hex(digest, c->hash);

    size_t cap = flen + strlen(c->message) + 128;
    char *buf = xmalloc(cap);
    int n = snprintf(buf, cap, "parent %s\ntime %lld\nmessage %s\n\n%s",
                     c->parent[0] ? c->parent : "none", (long long)c->time,
                     c->message, files);

    char path[sizeof COMMITS_DIR + SHA1_HEX_LEN + 2];
    snprintf(path, sizeof path, "%s/%s", COMMITS_DIR, c->hash);
    /* Same parent + same files = same commit; keep the original record. */
    if (!path_exists(path))
        write_file(path, buf, (size_t)n);

    free(buf);
    free(files);
}

void head_snapshot(snapshot *s)
{
    snap_init(s);
    char hash[SHA1_HEX_LEN + 1];
    if (!head_read(hash))
        return;
    commit c;
    if (!commit_load(hash, &c))
        die("HEAD points to missing commit %s", hash);
    *s = c.files;
    snap_init(&c.files);
    commit_free(&c);
}

static void scan_dir(const char *dir, snapshot *out)
{
    DIR *d = opendir(dir[0] ? dir : ".");
    if (!d)
        return;

    struct dirent *de;
    while ((de = readdir(d))) {
        const char *name = de->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0 ||
            strcmp(name, TIG_DIR) == 0 || strcmp(name, ".git") == 0)
            continue;

        size_t len = strlen(dir) + strlen(name) + 2;
        char *path = xmalloc(len);
        if (dir[0])
            snprintf(path, len, "%s/%s", dir, name);
        else
            snprintf(path, len, "%s", name);

        /* lstat: symlinks are neither followed nor tracked. */
        struct stat st;
        if (lstat(path, &st) == 0) {
            if (S_ISREG(st.st_mode)) {
                char hash[SHA1_HEX_LEN + 1];
                if (hash_worktree_file(path, hash))
                    snap_set(out, path, hash);
            } else if (S_ISDIR(st.st_mode)) {
                scan_dir(path, out);
            }
        }
        free(path);
    }
    closedir(d);
}

void scan_worktree(const char *dir, snapshot *out)
{
    snap_init(out);
    scan_dir(dir, out);
}
