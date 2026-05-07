#include <stdio.h>

int add(int a, int b) {
    return a + b;
}

int mul(int a, int b) {
    return a * b;
}

int sub(int a, int b) {
    return a - b;
}

void greet(const char *name) {
    printf("trunk: %s\n", name);
}

int main(void) {
    int x = add(10, 20);
    int y = sub(5, 3);
    greet("world");
    int diff = x - y;
    fputs("trunk done\n", stdout);
    return diff;
}
