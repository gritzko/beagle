#include <stdio.h>

int add(int a, int b) {
    return a + b;
}

int sub(int a, int b) {
    return a - b;
}

int neg(int a) {
    return -a;
}

void greet(const char *name) {
    printf("hi %s\n", name);
}

int main(void) {
    int x = add(1, 2);
    int y = sub(50, 30);
    greet("world");
    while (y > 0) {
        y = y - x;
    }
    fprintf(stderr, "fix loop done\n");
    return 0;
}
