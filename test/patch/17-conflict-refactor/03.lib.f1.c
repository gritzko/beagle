#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    char *i = argv[1];
    printf("%s!\n", i);
    return 0;
}
