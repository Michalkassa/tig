/*
 * Branch visualisation.
 *
 * Every commit has at most one parent, so the history is a tree. Each child
 * subtree of a commit is drawn in its own lane, to the right of the previous
 * child's, which keeps the drawing free of crossing lines:
 *
 *   * 1a2b3c4 (HEAD -> main) third
 *   | * 9f8e7d6 (feature) feature work
 *   |/
 *   * 5a6b7c8 second
 *   * 0d1e2f3 first
 *
 * The child whose subtree holds the main branch goes first, so main stays a
 * straight line down the left; other children follow, most recent first.
 */
#include "graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "repo.h"
#include "util.h"

#define C_RESET "\033[0m"
#define C_HASH "\033[33m"
#define C_HEAD "\033[1;36m"
#define C_BRANCH "\033[1;32m"

static const char *lane_colors[] = {
    "\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m",
};
#define N_LANE_COLORS (sizeof lane_colors / sizeof lane_colors[0])

typedef struct {
    char hash[SHA1_HEX_LEN + 1];
    char parent[SHA1_HEX_LEN + 1];
    char *message;
    time_t time;
    int parent_idx;
    int *children;
    size_t nchildren;
    time_t newest; /* most recent commit time in this subtree */
    int has_main;  /* subtree contains the main branch's tip */
} gnode;

typedef struct {
    gnode *nodes;
    size_t len, cap;
    int color;
} graph;

int use_color(void)
{
    return isatty(STDOUT_FILENO) && !getenv("NO_COLOR");
}

/* ---- decorations ---- */

typedef struct {
    char *buf;
    size_t len, cap;
} strbuf;

static void sb_add(strbuf *sb, const char *s)
{
    size_t n = strlen(s);
    if (sb->len + n + 1 > sb->cap) {
        sb->cap = (sb->len + n + 1) * 2;
        sb->buf = xrealloc(sb->buf, sb->cap);
    }
    memcpy(sb->buf + sb->len, s, n + 1);
    sb->len += n;
}

char *ref_decorations(const char *hash, int color)
{
    strbuf sb = {0};
    sb_add(&sb, "");

    char *current = head_branch();
    char head[SHA1_HEX_LEN + 1];
    int detached_here = !current && head_read(head) && strcmp(head, hash) == 0;

    size_t n;
    char **branches = list_branches(&n);
    int first = 1;

#define SEP() (sb_add(&sb, first ? (color ? C_HASH " (" : " (") : (color ? C_HASH ", " : ", ")), first = 0)

    if (detached_here) {
        SEP();
        sb_add(&sb, color ? C_HEAD "HEAD" : "HEAD");
    }
    for (size_t i = 0; i < n; i++) {
        char tip[SHA1_HEX_LEN + 1];
        if (!ref_read(branches[i], tip) || strcmp(tip, hash) != 0)
            continue;
        SEP();
        if (current && strcmp(current, branches[i]) == 0)
            sb_add(&sb, color ? C_HEAD "HEAD -> " : "HEAD -> ");
        sb_add(&sb, color ? C_BRANCH : "");
        sb_add(&sb, branches[i]);
    }
    if (!first)
        sb_add(&sb, color ? C_HASH ")" C_RESET : ")");
#undef SEP

    free_names(branches, n);
    free(current);
    return sb.buf;
}

/* ---- building the tree ---- */

static int find_node(const graph *g, const char *hash)
{
    for (size_t i = 0; i < g->len; i++) {
        if (strcmp(g->nodes[i].hash, hash) == 0)
            return (int)i;
    }
    return -1;
}

/* Adds tip and all its ancestors not yet in the graph. */
static void add_history(graph *g, const char *tip)
{
    char hash[SHA1_HEX_LEN + 1];
    snprintf(hash, sizeof hash, "%s", tip);
    while (hash[0] && find_node(g, hash) < 0) {
        commit c;
        if (!commit_load(hash, &c))
            die("missing commit %s", hash);
        if (g->len == g->cap) {
            g->cap = g->cap ? g->cap * 2 : 32;
            g->nodes = xrealloc(g->nodes, g->cap * sizeof(gnode));
        }
        gnode *n = &g->nodes[g->len++];
        memset(n, 0, sizeof *n);
        memcpy(n->hash, c.hash, sizeof n->hash);
        memcpy(n->parent, c.parent, sizeof n->parent);
        n->message = c.message;
        c.message = NULL;
        n->time = c.time;
        commit_free(&c);
        snprintf(hash, sizeof hash, "%s", n->parent);
    }
}

static const graph *sort_graph;

static int cmp_children(const void *a, const void *b)
{
    const gnode *x = &sort_graph->nodes[*(const int *)a];
    const gnode *y = &sort_graph->nodes[*(const int *)b];
    if (x->has_main != y->has_main)
        return y->has_main - x->has_main;
    if (x->newest != y->newest)
        return x->newest > y->newest ? -1 : 1;
    return strcmp(x->hash, y->hash);
}

static void link_tree(graph *g)
{
    char maintip[SHA1_HEX_LEN + 1];
    int main_idx = ref_read(DEFAULT_BRANCH, maintip) ? find_node(g, maintip) : -1;

    for (size_t i = 0; i < g->len; i++) {
        gnode *n = &g->nodes[i];
        n->parent_idx = n->parent[0] ? find_node(g, n->parent) : -1;
        n->newest = n->time;
        n->children = xmalloc(g->len * sizeof(int));
    }
    for (size_t i = 0; i < g->len; i++) {
        int p = g->nodes[i].parent_idx;
        if (p >= 0) {
            gnode *pn = &g->nodes[p];
            pn->children[pn->nchildren++] = (int)i;
        }
        /* Propagate subtree facts up to every ancestor. */
        for (int j = (int)i; j >= 0; j = g->nodes[j].parent_idx) {
            if (g->nodes[i].time > g->nodes[j].newest)
                g->nodes[j].newest = g->nodes[i].time;
            if ((int)i == main_idx)
                g->nodes[j].has_main = 1;
        }
    }
    sort_graph = g;
    for (size_t i = 0; i < g->len; i++)
        qsort(g->nodes[i].children, g->nodes[i].nchildren, sizeof(int), cmp_children);
}

/* ---- drawing ---- */

static void put_lane(const graph *g, int lane, const char *s)
{
    if (g->color)
        printf("%s%s" C_RESET, lane_colors[lane % N_LANE_COLORS], s);
    else
        fputs(s, stdout);
}

static void draw_node(const graph *g, int idx, int lane)
{
    for (int k = 0; k < lane; k++)
        put_lane(g, k, "| ");
    fputs("* ", stdout);

    const gnode *n = &g->nodes[idx];
    char *deco = ref_decorations(n->hash, g->color);
    if (g->color)
        printf(C_HASH "%.7s" C_RESET "%s %s\n", n->hash, deco, n->message);
    else
        printf("%.7s%s %s\n", n->hash, deco, n->message);
    free(deco);
}

/* Draws the subtree rooted at idx; lanes 0..lane-1 are lines passing through. */
static void draw_subtree(const graph *g, int idx, int lane)
{
    const gnode *n = &g->nodes[idx];

    /* Child i ends up in lane (lane + i). */
    for (size_t i = 0; i < n->nchildren; i++)
        draw_subtree(g, n->children[i], lane + (int)i);

    /* Fold the extra child lanes back into this commit's lane, rightmost first. */
    for (int k = (int)n->nchildren - 1; k >= 1; k--) {
        for (int j = 0; j < lane + k - 1; j++)
            put_lane(g, j, "| ");
        put_lane(g, lane + k - 1, "|");
        put_lane(g, lane + k, "/");
        putchar('\n');
    }

    draw_node(g, idx, lane);
}

void print_graph(void)
{
    graph g = {0};
    g.color = use_color();

    char hash[SHA1_HEX_LEN + 1];
    if (head_read(hash))
        add_history(&g, hash);
    size_t nb;
    char **branches = list_branches(&nb);
    for (size_t i = 0; i < nb; i++) {
        if (ref_read(branches[i], hash))
            add_history(&g, hash);
    }
    free_names(branches, nb);

    if (g.len == 0) {
        puts("no commits yet");
        return;
    }

    link_tree(&g);

    /* Normally one root (the first commit); handle several just in case. */
    int *roots = xmalloc(g.len * sizeof(int));
    size_t nroots = 0;
    for (size_t i = 0; i < g.len; i++) {
        if (g.nodes[i].parent_idx < 0)
            roots[nroots++] = (int)i;
    }
    qsort(roots, nroots, sizeof(int), cmp_children);
    for (size_t r = 0; r < nroots; r++)
        draw_subtree(&g, roots[r], 0);
    free(roots);

    for (size_t i = 0; i < g.len; i++) {
        free(g.nodes[i].message);
        free(g.nodes[i].children);
    }
    free(g.nodes);
}
