#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    int i = atoi(argv[1]);
    i = i + 1;
    printf("%i\n", i);
    return 0;
}
