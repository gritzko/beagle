//  sub/core.c — counter library, v1 (initial).

#include <stddef.h>

static size_t sub_counter = 0;

void sub_reset(void) {
    sub_counter = 0;
}

size_t sub_get(void) {
    return sub_counter;
}
