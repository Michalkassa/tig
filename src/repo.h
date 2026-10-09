#ifndef REPO_H
#define REPO_H

#include <time.h>

#include "sha1.h"
#include "snapshot.h"

#define TIG_DIR ".tig"
#define OBJECTS_DIR TIG_DIR "/objects"
#define COMMITS_DIR TIG_DIR "/commits"
#define REFS_DIR TIG_DIR "/refs"
#define INDEX_FILE TIG_DIR "/index"
#define HEAD_FILE TIG_DIR "/HEAD"
#define DEFAULT_BRANCH "main"

typedef struct {
    char hash[SHA1_HEX_LEN + 1];
    char parent[SHA1_HEX_LEN + 1]; /* empty string for the first commit */
    time_t time;
    char *message;
    snapshot files;
} commit;

/* Walks up from cwd to the directory containing .tig and chdirs there.
 * Returns the cwd relative to the root ("" when at the root); caller frees. */
char *repo_open(void);

/* Hashes a working-tree file. Returns 0 if it can't be read. */
int hash_worktree_file(const char *path, char out[SHA1_HEX_LEN + 1]);
/* Hashes a file and stores its content as .tig/objects/<hash>. */
void store_blob(const char *path, char out[SHA1_HEX_LEN + 1]);

void index_load(snapshot *s);
void index_save(const snapshot *s);

/* Reads a blob's content from .tig/objects. NULL if missing. */
char *blob_read(const char *hash, size_t *len);

/*
 * HEAD holds either "ref: <branch>" (on a branch) or a bare commit hash
 * (detached). Each branch is a file .tig/refs/<name> holding its tip commit.
 */

/* Commit HEAD resolves to. Returns 0 if there are no commits yet. */
int head_read(char out[SHA1_HEX_LEN + 1]);
/* Name of the current branch (caller frees), or NULL when detached. */
char *head_branch(void);
void head_set_branch(const char *name);
void head_set_detached(const char *hash);
/* Moves whatever HEAD points at (its branch, or HEAD itself) to hash. */
void head_advance(const char *hash);

int valid_branch_name(const char *name);
int ref_exists(const char *name);
int ref_read(const char *name, char out[SHA1_HEX_LEN + 1]);
void ref_write(const char *name, const char *hash);
int ref_delete(const char *name);
/* Sorted list of branch names; free with free_names. */
char **list_branches(size_t *count);
void free_names(char **names, size_t count);

int commit_load(const char *hash, commit *c);
void commit_free(commit *c);
/* Hashes the commit (parent + every file hash), writes it, fills c->hash. */
void commit_write(commit *c);

/* Snapshot of HEAD's files; empty if there are no commits yet. */
void head_snapshot(snapshot *s);

/* Every regular file under dir ("" = whole repo), hashed, skipping .tig and .git. */
void scan_worktree(const char *dir, snapshot *out);

#endif
