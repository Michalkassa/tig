#include <stdlib.h>
#include <unistd.h>

#include "../../src/paths.h"
#include "../../src/util.h"
#include "test.h"

static void check_resolve(const char *prefix, const char *arg, const char *expected)
{
    char *got = resolve_path(prefix, arg);
    if (expected)
        CHECK_STR(got, expected);
    else
        CHECK(got == NULL);
    free(got);
}

void suite_paths(void)
{
    check_resolve("", "a.txt", "a.txt");
    check_resolve("", "./a.txt", "a.txt");
    check_resolve("", ".", "");
    check_resolve("", "dir/", "dir");
    check_resolve("sub", "a.txt", "sub/a.txt");
    check_resolve("sub", ".", "sub");
    check_resolve("sub/deep", "./x/", "sub/deep/x");
    check_resolve("", "../outside", NULL);
    check_resolve("", "/etc/passwd", NULL);

    CHECK(is_under("src/a.c", "src"));
    CHECK(is_under("src/deep/a.c", "src"));
    CHECK(is_under("anything", ""));
    CHECK(!is_under("srcfile", "src"));
    CHECK(!is_under("src", "src"));

    enter_temp_dir();
    mkdir_parents("a/b/c/file.txt");
    CHECK(is_dir("a/b/c"));
    write_file("a/b/c/file.txt", "x", 1);
    write_file("a/keep.txt", "y", 1);
    unlink("a/b/c/file.txt");
    remove_empty_parents("a/b/c/file.txt");
    CHECK(!path_exists("a/b"));  /* empty dirs pruned */
    CHECK(is_dir("a"));          /* stops at a non-empty dir */
    leave_temp_dir();
}
