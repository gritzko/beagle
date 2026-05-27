#include <stdio.h>

/* tail: B rewrites cur's tip on top of an earlier commit;
 * the resulting tip is a non-FF replacement that PUT propagates. */
int main(void) {
    printf("rewritten tip from B (non-FF)\n");
    return 7;
}
