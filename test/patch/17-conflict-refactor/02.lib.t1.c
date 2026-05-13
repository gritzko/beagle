#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    int j = atoi(argv[1]);
    j = j + 1;
    printf("%i\n", j);
    return 0;
}
