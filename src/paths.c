#include "paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util.h"

char *resolve_path(const char *prefix, const char *arg)
{
    while (arg[0] == '.' && arg[1] == '/')
        arg += 2;
    if (strcmp(arg, ".") == 0)
        arg = "";
    if (strstr(arg, "..") || arg[0] == '/')
        return NULL;

    size_t len = strlen(prefix) + strlen(arg) + 2;
    char *p = xmalloc(len);
    if (prefix[0] && arg[0])
        snprintf(p, len, "%s/%s", prefix, arg);
    else
        snprintf(p, len, "%s", prefix[0] ? prefix : arg);

    size_t n = strlen(p);
    while (n > 0 && p[n - 1] == '/')
        p[--n] = '\0';
    return p;
}

int is_under(const char *path, const char *dir)
{
    size_t n = strlen(dir);
    return n == 0 || (strncmp(path, dir, n) == 0 && path[n] == '/');
}

void mkdir_parents(const char *path)
{
    char *p = xstrdup(path);
    for (char *s = strchr(p, '/'); s; s = strchr(s + 1, '/')) {
        *s = '\0';
        if (!is_dir(p) && mkdir(p, 0755) != 0)
            die("cannot create directory '%s'", p);
        *s = '/';
    }
    free(p);
}

void remove_empty_parents(const char *path)
{
    char *p = xstrdup(path);
    char *s;
    while ((s = strrchr(p, '/'))) {
        *s = '\0';
        if (rmdir(p) != 0)
            break;
    }
    free(p);
}
