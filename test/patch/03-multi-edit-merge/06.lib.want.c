#include <stdio.h>

/* math */
int add(int x, int y) { return x + y; }
int sub(int x, int y) { int r = x - y; return r; }
int mul(int x, int y) { return x * y; }

/* string */
const char *greet = "Hello";
const char *bye = "Goodbye";

/* io */
void info(const char *m) { printf("%s\n", m); }
void debug_(const char *m) { printf("[dbg] %s\n", m); }

int main(void) {
    info("hi");
    return 0;
}
