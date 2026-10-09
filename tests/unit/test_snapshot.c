#include <stdlib.h>

#include "../../src/snapshot.h"
#include "test.h"

#define H1 "1111111111111111111111111111111111111111"
#define H2 "2222222222222222222222222222222222222222"
#define H3 "3333333333333333333333333333333333333333"

void suite_snapshot(void)
{
    snapshot s;
    snap_init(&s);

    /* Insertion keeps entries sorted by path regardless of order. */
    snap_set(&s, "src/b.c", H2);
    snap_set(&s, "a.txt", H1);
    snap_set(&s, "src/a.c", H3);
    CHECK(s.len == 3);
    CHECK_STR(s.items[0].path, "a.txt");
    CHECK_STR(s.items[1].path, "src/a.c");
    CHECK_STR(s.items[2].path, "src/b.c");

    /* Setting an existing path replaces its hash instead of duplicating. */
    snap_set(&s, "a.txt", H2);
    CHECK(s.len == 3);
    CHECK_STR(snap_find(&s, "a.txt")->hash, H2);
    CHECK(snap_find(&s, "missing") == NULL);

    /* Serialize -> parse round trip, including a path with spaces. */
    snap_set(&s, "dir/with space.txt", H1);
    size_t len;
    char *text = snap_serialize(&s, &len);
    CHECK(len == strlen(text));
    CHECK(strstr(text, H1 " dir/with space.txt\n") != NULL);

    snapshot t;
    snap_init(&t);
    snap_parse(&t, text);
    CHECK(snap_equal(&s, &t));
    free(text);

    /* Equality looks at both paths and hashes. */
    snap_set(&t, "a.txt", H3);
    CHECK(!snap_equal(&s, &t));
    snap_set(&t, "a.txt", H2);
    CHECK(snap_equal(&s, &t));

    /* Removal. */
    CHECK(snap_remove(&t, "src/a.c"));
    CHECK(!snap_remove(&t, "src/a.c"));
    CHECK(t.len == 3);
    CHECK(!snap_equal(&s, &t));

    /* Garbage lines are ignored by the parser. */
    snapshot g;
    snap_init(&g);
    snap_parse(&g, "not a valid line\n" H1 " ok.txt\nshort x\n");
    CHECK(g.len == 1);
    CHECK_STR(g.items[0].path, "ok.txt");

    snap_free(&s);
    snap_free(&t);
    snap_free(&g);
    CHECK(s.len == 0 && s.items == NULL);
}
