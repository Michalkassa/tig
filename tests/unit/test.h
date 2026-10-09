/* Minimal unit test framework: CHECK records a failure and keeps going. */
#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <string.h>

extern int test_failures;
extern int test_checks;

#define CHECK(cond)                                                          \
    do {                                                                     \
        test_checks++;                                                       \
        if (!(cond)) {                                                       \
            test_failures++;                                                 \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                    \
    } while (0)

#define CHECK_STR(a, b)                                                       \
    do {                                                                      \
        const char *a_ = (a), *b_ = (b);                                      \
        test_checks++;                                                        \
        if (!a_ || !b_ || strcmp(a_, b_) != 0) {                              \
            test_failures++;                                                  \
            fprintf(stderr, "  FAIL %s:%d: %s == %s\n    got:      \"%s\"\n" \
                            "    expected: \"%s\"\n",                         \
                    __FILE__, __LINE__, #a, #b, a_ ? a_ : "(null)",           \
                    b_ ? b_ : "(null)");                                      \
        }                                                                     \
    } while (0)

/* Each suite file defines one of these. */
void suite_sha1(void);
void suite_snapshot(void);
void suite_paths(void);
void suite_repo(void);

/* Creates a fresh temp directory and chdirs into it; returns its path. */
const char *enter_temp_dir(void);
void leave_temp_dir(void);

#endif
