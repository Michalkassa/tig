/*
 * tig - a tiny git.
 *
 * File life cycle:
 *   untracked  --add-->     staged
 *   unmodified --edit-->    modified
 *   unmodified --rm-->      untracked (after commit)
 *   modified   --add-->     staged
 *   staged     --commit-->  unmodified
 *
 * States come from comparing three snapshots: HEAD (last commit),
 * the index (what the next commit will contain) and the working tree.
 *
 * Branches: every commit points at its parent; a branch is a named pointer
 * to a commit, and HEAD points at the current branch (or a bare commit).
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "graph.h"
#include "paths.h"
#include "repo.h"
#include "snapshot.h"
#include "util.h"

static void usage(void) __attribute__((noreturn));
static void usage(void)
{
    fputs("usage: tig <command> [args]\n"
          "\n"
          "  init                 create an empty repository in the current directory\n"
          "  add <path>...        stage files (new or modified, or deletions)\n"
          "  rm <path>...         stop tracking files (they become untracked)\n"
          "  commit -m <message>  snapshot the staged files\n"
          "  status               show the state of every file\n"
          "  log                  show commit history\n"
          "  show [commit]        show a commit and its snapshot (default: HEAD)\n"
          "  branch               list branches\n"
          "  branch <name>        create a branch at the current commit\n"
          "  branch -d <name>     delete a branch\n"
          "  checkout <branch>    switch to a branch (or a commit, detaching HEAD)\n"
          "  checkout -b <name>   create a branch and switch to it\n"
          "  graph                draw all branches as a tree\n",
          stderr);
    exit(1);
}

static char *resolve_arg(const char *prefix, const char *arg)
{
    char *p = resolve_path(prefix, arg);
    if (!p)
        die("'%s': paths must be inside the repository and not use '..'", arg);
    return p;
}

static void short_hash(const char *hash, char out[8])
{
    snprintf(out, 8, "%.7s", hash);
}

/* ---- init ---- */

static int cmd_init(void)
{
    if (is_dir(TIG_DIR))
        die("repository already exists");
    if (mkdir(TIG_DIR, 0755) != 0 || mkdir(OBJECTS_DIR, 0755) != 0 ||
        mkdir(COMMITS_DIR, 0755) != 0 || mkdir(REFS_DIR, 0755) != 0)
        die("cannot create %s", TIG_DIR);
    write_file(INDEX_FILE, "", 0);
    head_set_branch(DEFAULT_BRANCH);
    puts("Initialized empty tig repository in " TIG_DIR "/");
    return 0;
}

/* ---- add ---- */

static int cmd_add(int argc, char **argv, const char *prefix)
{
    if (argc < 1)
        die("nothing specified, nothing added");

    snapshot index;
    index_load(&index);

    for (int a = 0; a < argc; a++) {
        char *path = resolve_arg(prefix, argv[a]);
        char hash[SHA1_HEX_LEN + 1];

        if (path[0] && is_regular_file(path)) {
            store_blob(path, hash);
            snap_set(&index, path, hash);
        } else if (!path[0] || is_dir(path)) {
            snapshot work;
            scan_worktree(path, &work);
            for (size_t i = 0; i < work.len; i++) {
                store_blob(work.items[i].path, hash);
                snap_set(&index, work.items[i].path, hash);
            }
            /* Tracked files under this dir that vanished from disk: stage removal. */
            for (size_t i = index.len; i-- > 0;) {
                if (is_under(index.items[i].path, path) &&
                    !snap_find(&work, index.items[i].path))
                    snap_remove(&index, index.items[i].path);
            }
            snap_free(&work);
        } else if (!snap_remove(&index, path)) {
            die("pathspec '%s' did not match any files", argv[a]);
        }
        free(path);
    }

    index_save(&index);
    snap_free(&index);
    return 0;
}

/* ---- rm ---- */

static int cmd_rm(int argc, char **argv, const char *prefix)
{
    if (argc < 1)
        die("nothing specified, nothing removed");

    snapshot index;
    index_load(&index);

    for (int a = 0; a < argc; a++) {
        char *path = resolve_arg(prefix, argv[a]);
        int removed = 0;

        if (path[0] && snap_remove(&index, path)) {
            removed = 1;
            printf("rm '%s'\n", path);
        } else {
            for (size_t i = index.len; i-- > 0;) {
                if (is_under(index.items[i].path, path)) {
                    printf("rm '%s'\n", index.items[i].path);
                    snap_remove(&index, index.items[i].path);
                    removed = 1;
                }
            }
        }
        if (!removed)
            die("pathspec '%s' is not tracked", argv[a]);
        free(path);
    }

    index_save(&index);
    snap_free(&index);
    return 0;
}

/* ---- commit ---- */

static int cmd_commit(int argc, char **argv)
{
    const char *message = NULL;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-m") == 0 && i + 1 < argc)
            message = argv[++i];
    }
    if (!message || !message[0])
        die("commit message required: tig commit -m <message>");

    commit c;
    memset(&c, 0, sizeof c);
    index_load(&c.files);

    snapshot head;
    head_snapshot(&head);
    int same = snap_equal(&head, &c.files);
    snap_free(&head);
    if (same) {
        puts("nothing to commit");
        snap_free(&c.files);
        return 1;
    }

    if (!head_read(c.parent))
        c.parent[0] = '\0';
    c.time = time(NULL);
    c.message = xstrdup(message);
    for (char *p = c.message; *p; p++) {
        if (*p == '\n' || *p == '\r')
            *p = ' ';
    }

    commit_write(&c);
    head_advance(c.hash);

    char sh[8];
    short_hash(c.hash, sh);
    char *branch = head_branch();
    printf("[%s %s] %s (%zu file%s)\n", branch ? branch : "detached HEAD", sh,
           c.message, c.files.len, c.files.len == 1 ? "" : "s");
    free(branch);
    commit_free(&c);
    return 0;
}

/* ---- status ---- */

enum { ST_STAGED, ST_MODIFIED, ST_UNTRACKED, ST_UNMODIFIED, ST_COUNT };

typedef struct {
    const char *path;
    const char *label;
} status_line;

static int cmd_status(void)
{
    snapshot head, index, work, all;
    head_snapshot(&head);
    index_load(&index);
    scan_worktree("", &work);

    /* Union of every path known to any of the three snapshots. */
    snap_init(&all);
    const snapshot *srcs[] = {&head, &index, &work};
    for (int s = 0; s < 3; s++)
        for (size_t i = 0; i < srcs[s]->len; i++)
            snap_set(&all, srcs[s]->items[i].path, srcs[s]->items[i].hash);

    status_line *lines[ST_COUNT];
    size_t counts[ST_COUNT] = {0};
    for (int s = 0; s < ST_COUNT; s++)
        lines[s] = xmalloc(all.len * sizeof(status_line));

#define PUSH(st, p, l) (lines[st][counts[st]++] = (status_line){(p), (l)})

    for (size_t k = 0; k < all.len; k++) {
        const char *p = all.items[k].path;
        entry *h = snap_find(&head, p);
        entry *i = snap_find(&index, p);
        entry *w = snap_find(&work, p);

        /* index vs HEAD: what the next commit changes */
        if (i && !h)
            PUSH(ST_STAGED, p, "new file:  ");
        else if (i && h && strcmp(i->hash, h->hash) != 0)
            PUSH(ST_STAGED, p, "modified:  ");
        else if (!i && h)
            PUSH(ST_STAGED, p, "removed:   ");

        /* working tree vs index: changes not yet staged */
        if (i && w && strcmp(i->hash, w->hash) != 0)
            PUSH(ST_MODIFIED, p, "modified:  ");
        else if (i && !w)
            PUSH(ST_MODIFIED, p, "deleted:   ");

        if (w && !i)
            PUSH(ST_UNTRACKED, p, "");

        if (h && i && w && strcmp(h->hash, i->hash) == 0 &&
            strcmp(i->hash, w->hash) == 0)
            PUSH(ST_UNMODIFIED, p, "");
    }
#undef PUSH

    char headhash[SHA1_HEX_LEN + 1];
    char *branch = head_branch();
    int has_commit = head_read(headhash);
    if (branch) {
        printf("On branch %s\n", branch);
    } else {
        char sh[8];
        short_hash(headhash, sh);
        printf("HEAD detached at %s\n", sh);
    }
    if (!has_commit)
        puts("No commits yet");
    free(branch);

    static const char *titles[ST_COUNT] = {
        "Staged (will be committed):",
        "Modified (not staged, use 'tig add'):",
        "Untracked (use 'tig add' to track):",
        "Unmodified:",
    };
    int any_change = 0;
    for (int s = 0; s < ST_COUNT; s++) {
        if (counts[s] == 0)
            continue;
        if (s != ST_UNMODIFIED)
            any_change = 1;
        printf("\n%s\n", titles[s]);
        for (size_t n = 0; n < counts[s]; n++)
            printf("    %s%s\n", lines[s][n].label, lines[s][n].path);
    }
    for (int s = 0; s < ST_COUNT; s++)
        free(lines[s]);
    if (!any_change)
        puts("\nnothing to commit, working tree clean");

    snap_free(&head);
    snap_free(&index);
    snap_free(&work);
    snap_free(&all);
    return 0;
}

/* ---- log / show ---- */

static void print_commit_header(const commit *c)
{
    char when[64];
    strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", localtime(&c->time));
    char *deco = ref_decorations(c->hash, use_color());
    printf("commit %s%s\n", c->hash, deco);
    free(deco);
    if (c->parent[0])
        printf("parent %s\n", c->parent);
    printf("date   %s\n\n    %s\n", when, c->message);
}

static int cmd_log(void)
{
    char hash[SHA1_HEX_LEN + 1];
    if (!head_read(hash)) {
        puts("no commits yet");
        return 0;
    }
    int first = 1;
    while (hash[0]) {
        commit c;
        if (!commit_load(hash, &c))
            die("missing commit %s", hash);
        if (!first)
            putchar('\n');
        first = 0;
        print_commit_header(&c);
        snprintf(hash, sizeof hash, "%s", c.parent);
        commit_free(&c);
    }
    return 0;
}

/* Accepts a full hash or a unique prefix of one. */
static void resolve_commit(const char *ref, char out[SHA1_HEX_LEN + 1])
{
    size_t n = strlen(ref);
    if (n < 4)
        die("commit hash '%s' too short (need at least 4 characters)", ref);

    DIR *d = opendir(COMMITS_DIR);
    if (!d)
        die("cannot open %s", COMMITS_DIR);
    int matches = 0;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (strlen(de->d_name) == SHA1_HEX_LEN && strncmp(de->d_name, ref, n) == 0) {
            snprintf(out, SHA1_HEX_LEN + 1, "%s", de->d_name);
            matches++;
        }
    }
    closedir(d);
    if (matches == 0)
        die("unknown commit '%s'", ref);
    if (matches > 1)
        die("ambiguous commit '%s'", ref);
}

static int cmd_show(int argc, char **argv)
{
    char hash[SHA1_HEX_LEN + 1];
    if (argc > 0)
        resolve_commit(argv[0], hash);
    else if (!head_read(hash))
        die("no commits yet");

    commit c, parent = {0};
    if (!commit_load(hash, &c))
        die("missing commit %s", hash);
    int has_parent = c.parent[0] && commit_load(c.parent, &parent);

    print_commit_header(&c);
    printf("\nsnapshot (%zu file%s):\n", c.files.len, c.files.len == 1 ? "" : "s");
    for (size_t i = 0; i < c.files.len; i++) {
        const entry *e = &c.files.items[i];
        const char *tag = "new      ";
        if (has_parent) {
            entry *pe = snap_find(&parent.files, e->path);
            if (pe)
                tag = strcmp(pe->hash, e->hash) == 0 ? "unchanged" : "modified ";
        }
        printf("    %s  %s  %s\n", e->hash, tag, e->path);
    }
    if (has_parent) {
        for (size_t i = 0; i < parent.files.len; i++) {
            if (!snap_find(&c.files, parent.files.items[i].path))
                printf("    %-40s  removed    %s\n", "", parent.files.items[i].path);
        }
        commit_free(&parent);
    }
    commit_free(&c);
    return 0;
}

/* ---- branch ---- */

static int cmd_branch(int argc, char **argv)
{
    char *current = head_branch();

    if (argc == 0) {
        size_t n;
        char **names = list_branches(&n);
        int current_listed = 0;
        for (size_t i = 0; i < n; i++) {
            int here = current && strcmp(current, names[i]) == 0;
            current_listed |= here;
            printf("%c %s\n", here ? '*' : ' ', names[i]);
        }
        if (current && !current_listed)
            printf("* %s (no commits yet)\n", current);
        if (!current) {
            char hash[SHA1_HEX_LEN + 1] = "", sh[8];
            head_read(hash);
            short_hash(hash, sh);
            printf("* (HEAD detached at %s)\n", sh);
        }
        free_names(names, n);
        free(current);
        return 0;
    }

    if (strcmp(argv[0], "-d") == 0) {
        if (argc < 2)
            die("usage: tig branch -d <name>");
        const char *name = argv[1];
        if (current && strcmp(current, name) == 0)
            die("cannot delete the branch you are on ('%s')", name);
        if (!valid_branch_name(name) || !ref_delete(name))
            die("branch '%s' not found", name);
        printf("Deleted branch %s\n", name);
        free(current);
        return 0;
    }

    const char *name = argv[0];
    if (!valid_branch_name(name))
        die("'%s' is not a valid branch name (use letters, digits, - _ .)", name);
    if (ref_exists(name))
        die("branch '%s' already exists", name);
    char hash[SHA1_HEX_LEN + 1];
    if (!head_read(hash))
        die("cannot create a branch before the first commit");
    ref_write(name, hash);
    free(current);
    return 0;
}

/* ---- checkout ---- */

/* Dies if switching would lose uncommitted work. */
static void ensure_clean(const snapshot *head, const snapshot *index, const snapshot *work)
{
    if (!snap_equal(head, index))
        die("you have staged changes; commit them before switching");
    for (size_t i = 0; i < index->len; i++) {
        entry *w = snap_find(work, index->items[i].path);
        if (!w || strcmp(w->hash, index->items[i].hash) != 0)
            die("'%s' has unstaged changes; commit them before switching",
                index->items[i].path);
    }
}

/* Makes the working tree and index match target, given they match current. */
static void switch_tree(const snapshot *current, const snapshot *target, const snapshot *work)
{
    for (size_t i = 0; i < target->len; i++) {
        const entry *t = &target->items[i];
        entry *w = snap_find(work, t->path);
        if (!snap_find(current, t->path) && w && strcmp(w->hash, t->hash) != 0)
            die("untracked file '%s' would be overwritten; move it first", t->path);
    }

    for (size_t i = 0; i < current->len; i++) {
        const char *p = current->items[i].path;
        if (!snap_find(target, p)) {
            unlink(p);
            remove_empty_parents(p);
        }
    }
    for (size_t i = 0; i < target->len; i++) {
        const entry *t = &target->items[i];
        entry *w = snap_find(work, t->path);
        if (w && strcmp(w->hash, t->hash) == 0)
            continue;
        size_t len;
        char *data = blob_read(t->hash, &len);
        if (!data)
            die("missing object %s for '%s'", t->hash, t->path);
        mkdir_parents(t->path);
        write_file(t->path, data, len);
        free(data);
    }
    index_save(target);
}

static int cmd_checkout(int argc, char **argv)
{
    if (argc < 1)
        die("usage: tig checkout <branch|commit> | -b <name>");

    if (strcmp(argv[0], "-b") == 0) {
        if (argc < 2)
            die("usage: tig checkout -b <name>");
        const char *name = argv[1];
        if (!valid_branch_name(name))
            die("'%s' is not a valid branch name (use letters, digits, - _ .)", name);
        if (ref_exists(name))
            die("branch '%s' already exists", name);
        char hash[SHA1_HEX_LEN + 1];
        if (head_read(hash))
            ref_write(name, hash);
        head_set_branch(name);
        printf("Switched to a new branch '%s'\n", name);
        return 0;
    }

    const char *target_name = argv[0];
    int is_branch = valid_branch_name(target_name) && ref_exists(target_name);
    char target_hash[SHA1_HEX_LEN + 1];
    if (is_branch)
        ref_read(target_name, target_hash);
    else
        resolve_commit(target_name, target_hash);

    char *current = head_branch();
    if (is_branch && current && strcmp(current, target_name) == 0) {
        printf("Already on '%s'\n", target_name);
        free(current);
        return 0;
    }
    free(current);

    snapshot head, index, work, target;
    head_snapshot(&head);
    index_load(&index);
    scan_worktree("", &work);
    ensure_clean(&head, &index, &work);

    commit c;
    if (!commit_load(target_hash, &c))
        die("missing commit %s", target_hash);
    target = c.files;
    snap_init(&c.files);
    commit_free(&c);

    switch_tree(&head, &target, &work);

    if (is_branch) {
        head_set_branch(target_name);
        printf("Switched to branch '%s'\n", target_name);
    } else {
        head_set_detached(target_hash);
        char sh[8];
        short_hash(target_hash, sh);
        printf("HEAD is now at %s (detached)\n", sh);
    }

    snap_free(&head);
    snap_free(&index);
    snap_free(&work);
    snap_free(&target);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        usage();
    const char *cmd = argv[1];

    if (strcmp(cmd, "init") == 0)
        return cmd_init();

    char *prefix = repo_open();
    int rc;
    if (strcmp(cmd, "add") == 0)
        rc = cmd_add(argc - 2, argv + 2, prefix);
    else if (strcmp(cmd, "rm") == 0)
        rc = cmd_rm(argc - 2, argv + 2, prefix);
    else if (strcmp(cmd, "commit") == 0)
        rc = cmd_commit(argc - 2, argv + 2);
    else if (strcmp(cmd, "status") == 0)
        rc = cmd_status();
    else if (strcmp(cmd, "log") == 0)
        rc = cmd_log();
    else if (strcmp(cmd, "show") == 0)
        rc = cmd_show(argc - 2, argv + 2);
    else if (strcmp(cmd, "branch") == 0)
        rc = cmd_branch(argc - 2, argv + 2);
    else if (strcmp(cmd, "checkout") == 0)
        rc = cmd_checkout(argc - 2, argv + 2);
    else if (strcmp(cmd, "graph") == 0) {
        print_graph();
        rc = 0;
    }
    else {
        usage();
    }
    free(prefix);
    return rc;
}
