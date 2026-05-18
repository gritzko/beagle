//  sub/helper.c — driver for core's counter.  Added at sub C2.

#include <stddef.h>

void sub_inc(void);

void sub_bump_n(size_t n) {
    for (size_t i = 0; i < n; i++) {
        sub_inc();
    }
}
