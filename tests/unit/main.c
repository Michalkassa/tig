/* mkdtemp is hidden on macOS under strict POSIX unless Darwin extensions are on. */
#define _DARWIN_C_SOURCE
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

#include "test.h"

int test_failures = 0;
int test_checks = 0;

static char temp_dir[PATH_MAX];
static char orig_dir[PATH_MAX];

const char *enter_temp_dir(void)
{
    const char *base = getenv("TMPDIR");
    snprintf(temp_dir, sizeof temp_dir, "%s/tig-unit-XXXXXX", base && base[0] ? base : "/tmp");
    if (!getcwd(orig_dir, sizeof orig_dir) || !mkdtemp(temp_dir) || chdir(temp_dir) != 0) {
        perror("enter_temp_dir");
        exit(2);
    }
    return temp_dir;
}

void leave_temp_dir(void)
{
    char cmd[PATH_MAX + 16];
    if (chdir(orig_dir) != 0) {
        perror("leave_temp_dir");
        exit(2);
    }
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", temp_dir);
    if (system(cmd) != 0)
        fprintf(stderr, "warning: could not remove %s\n", temp_dir);
}

static void run(const char *name, void (*suite)(void))
{
    int before = test_failures;
    suite();
    printf("%-10s %s\n", name, test_failures == before ? "ok" : "FAILED");
}

int main(void)
{
    run("sha1", suite_sha1);
    run("snapshot", suite_snapshot);
    run("paths", suite_paths);
    run("repo", suite_repo);
    printf("%d checks, %d failed\n", test_checks, test_failures);
    return test_failures ? 1 : 0;
}
