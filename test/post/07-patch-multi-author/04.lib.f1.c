#include <stdio.h>

// version: 1
int sum(int a, int b) {
    return a + b;
}

int diff(int a, int b) {
    return a - b;
}

int prod(int a, int b) {
    int r = 0;
    for (int i = 0; i < b; i++) r += a;
    return r;
}

int main(void) {
    printf("sum=%d\n", sum(3, 4));
    printf("diff=%d\n", diff(7, 2));
    return 0;
}
