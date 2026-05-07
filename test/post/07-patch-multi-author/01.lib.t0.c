#include <stdio.h>

// version: 0
int sum(int a, int b) {
    return a + b;
}

int diff(int a, int b) {
    return a - b;
}

int main(void) {
    printf("sum=%d\n", sum(3, 4));
    printf("diff=%d\n", diff(7, 2));
    return 0;
}
