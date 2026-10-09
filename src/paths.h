#ifndef PATHS_H
#define PATHS_H

/* Turns a user-supplied path (relative to cwd prefix) into a path relative to
 * the repo root: strips "./" and trailing "/", "." means the prefix itself.
 * Returns NULL for paths escaping the repo ("..", absolute). Caller frees. */
char *resolve_path(const char *prefix, const char *arg);

/* 1 if path lies inside dir ("" = repo root, which contains everything). */
int is_under(const char *path, const char *dir);

/* Creates every missing parent directory of a file path. */
void mkdir_parents(const char *path);
/* Removes now-empty parent directories of a deleted file, up to the root. */
void remove_empty_parents(const char *path);

#endif
