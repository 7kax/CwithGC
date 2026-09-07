#ifndef TEST_CHECK_H
#define TEST_CHECK_H

#include <stdio.h>
#include <stdlib.h>

static inline void test_check_failed(const char *condition, const char *file, int line) {
    fprintf(stderr, "Test check failed: %s (%s:%d)\n", condition, file, line);
    abort();
}

#define TEST_CHECK(condition)                                                                      \
    do {                                                                                           \
        if (!(condition))                                                                          \
            test_check_failed(#condition, __FILE__, __LINE__);                                     \
    } while (0)

#endif // TEST_CHECK_H
