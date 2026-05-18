//  parent/main.c — entry point, v2 (uses util).

#include <stdio.h>

int util_double(int x);

int main(void) {
    printf("parent v2: %d\n", util_double(7));
    return 0;
}
