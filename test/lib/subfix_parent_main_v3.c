//  parent/main.c — entry point, v3 (uses util + sub's counter).

#include <stddef.h>
#include <stdio.h>

int    util_double(int x);
int    util_square(int x);
void   sub_inc(void);
size_t sub_get(void);

int main(void) {
    sub_inc();
    sub_inc();
    printf("parent v3: counter=%zu, double=%d, square=%d\n",
           sub_get(), util_double(11), util_square(4));
    return 0;
}
