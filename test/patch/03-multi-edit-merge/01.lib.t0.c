#include <stdio.h>

/* math */
int add(int x, int y) { return x + y; }
int sub(int x, int y) { return x - y; }
int placeholder_m(void) { return 0; }

/* string */
const char *greet_str = "hi";
const char *bye_str = "bye";
const char *placeholder_s = NULL;

/* io */
void log_msg(const char *m) { printf("%s\n", m); }
void placeholder_io(void) {}

int main(void) {
    return 0;
}
