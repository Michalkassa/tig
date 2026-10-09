#include <stdlib.h>
#include <sys/stat.h>

#include "../../src/repo.h"
#include "../../src/util.h"
#include "test.h"

#define H1 "1111111111111111111111111111111111111111"
#define H2 "2222222222222222222222222222222222222222"

static void make_repo(void)
{
    mkdir(TIG_DIR, 0755);
    mkdir(OBJECTS_DIR, 0755);
    mkdir(COMMITS_DIR, 0755);
    mkdir(REFS_DIR, 0755);
    write_file(INDEX_FILE, "", 0);
    head_set_branch(DEFAULT_BRANCH);
}

static void test_blobs(void)
{
    write_file("f.txt", "abc", 3);
    char hash[SHA1_HEX_LEN + 1];
    store_blob("f.txt", hash);
    CHECK_STR(hash, "a9993e364706816aba3e25717850c26c9cd0d89d");

    size_t len;
    char *data = blob_read(hash, &len);
    CHECK(data && len == 3 && memcmp(data, "abc", 3) == 0);
    free(data);
    CHECK(blob_read(H1, &len) == NULL);
}

static void test_branch_names(void)
{
    CHECK(valid_branch_name("main"));
    CHECK(valid_branch_name("feature-1.2_x"));
    CHECK(!valid_branch_name(""));
    CHECK(!valid_branch_name("HEAD"));
    CHECK(!valid_branch_name("-d"));
    CHECK(!valid_branch_name(".hidden"));
    CHECK(!valid_branch_name("a/b"));
    CHECK(!valid_branch_name("has space"));
}

static void test_head_and_refs(void)
{
    char hash[SHA1_HEX_LEN + 1];
    char *branch = head_branch();
    CHECK_STR(branch, DEFAULT_BRANCH);
    free(branch);
    CHECK(!head_read(hash)); /* no commits yet */

    /* Advancing HEAD while on a branch moves the branch. */
    head_advance(H1);
    CHECK(ref_read(DEFAULT_BRANCH, hash));
    CHECK_STR(hash, H1);
    CHECK(head_read(hash));
    CHECK_STR(hash, H1);

    ref_write("feature", H2);
    size_t n;
    char **names = list_branches(&n);
    CHECK(n == 2);
    CHECK(n == 2 && strcmp(names[0], "feature") == 0 && strcmp(names[1], "main") == 0);
    free_names(names, n);

    /* Detached HEAD: advancing moves HEAD itself, not any branch. */
    head_set_detached(H2);
    CHECK(head_branch() == NULL);
    head_advance(H1);
    CHECK(head_read(hash));
    CHECK_STR(hash, H1);
    CHECK(ref_read("feature", hash));
    CHECK_STR(hash, H2);

    CHECK(ref_delete("feature"));
    CHECK(!ref_exists("feature"));
    CHECK(!ref_delete("feature"));
    head_set_branch(DEFAULT_BRANCH);
}

static void test_commits(void)
{
    commit a = {0};
    snap_init(&a.files);
    snap_set(&a.files, "x.txt", H1);
    a.time = 1000;
    a.message = xstrdup("first");
    commit_write(&a);

    commit loaded;
    CHECK(commit_load(a.hash, &loaded));
    CHECK_STR(loaded.message, "first");
    CHECK(loaded.parent[0] == '\0');
    CHECK(loaded.time == 1000);
    CHECK(snap_equal(&loaded.files, &a.files));
    commit_free(&loaded);

    /* Commit hash depends only on parent + file hashes, not message or time. */
    commit same = {0};
    snap_init(&same.files);
    snap_set(&same.files, "x.txt", H1);
    same.time = 2000;
    same.message = xstrdup("different message");
    commit_write(&same);
    CHECK_STR(same.hash, a.hash);
    /* ...and the original record is kept rather than overwritten. */
    CHECK(commit_load(a.hash, &loaded));
    CHECK_STR(loaded.message, "first");
    commit_free(&loaded);

    /* A child with the same files but a parent gets a different hash. */
    commit child = {0};
    snap_init(&child.files);
    snap_set(&child.files, "x.txt", H1);
    memcpy(child.parent, a.hash, sizeof child.parent);
    child.message = xstrdup("child");
    commit_write(&child);
    CHECK(strcmp(child.hash, a.hash) != 0);
    CHECK(commit_load(child.hash, &loaded));
    CHECK_STR(loaded.parent, a.hash);
    commit_free(&loaded);

    /* Changing a file hash changes the commit hash. */
    commit changed = {0};
    snap_init(&changed.files);
    snap_set(&changed.files, "x.txt", H2);
    changed.message = xstrdup("changed");
    commit_write(&changed);
    CHECK(strcmp(changed.hash, a.hash) != 0);

    CHECK(!commit_load(H2, &loaded));

    commit_free(&a);
    commit_free(&same);
    commit_free(&child);
    commit_free(&changed);
}

static void test_scan(void)
{
    mkdir("dir", 0755);
    write_file("dir/a.txt", "abc", 3);
    mkdir(".git", 0755);
    write_file(".git/ignored", "x", 1);

    snapshot s;
    scan_worktree("", &s);
    CHECK(snap_find(&s, "dir/a.txt") != NULL);
    CHECK(snap_find(&s, "f.txt") != NULL);
    CHECK(snap_find(&s, ".git/ignored") == NULL);
    for (size_t i = 0; i < s.len; i++)
        CHECK(strncmp(s.items[i].path, ".tig", 4) != 0);
    snap_free(&s);

    scan_worktree("dir", &s);
    CHECK(s.len == 1);
    snap_free(&s);
}

void suite_repo(void)
{
    enter_temp_dir();
    make_repo();
    test_blobs();
    test_branch_names();
    test_head_and_refs();
    test_commits();
    test_scan();
    leave_temp_dir();
}
