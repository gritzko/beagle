#include "IGNO.h"

#include <string.h>

#include "abc/PRO.h"
#include "abc/TEST.h"

ok64 IGNOtest1() {
    sane(1);
    // Test basic pattern matching with an in-memory igno struct
    igno ig;
    zero(ig);

    // Add pattern: *.o
    con u8 pat1[] = "*.o";
    ig.patterns[0].pattern[0] = pat1;
    ig.patterns[0].pattern[1] = pat1 + 3;
    ig.patterns[0].negated = NO;
    ig.patterns[0].anchored = NO;
    ig.patterns[0].dir_only = NO;
    ig.patterns[0].has_slash = NO;
    ig.count = 1;

    // Should match .o files
    a_cstr(obj, "foo.o");
    want(IGNOMatch(&ig, obj, NO) == YES);

    // Should not match .c files
    a_cstr(src, "foo.c");
    want(IGNOMatch(&ig, src, NO) == NO);

    done;
}

ok64 IGNOtest2() {
    sane(1);
    igno ig;
    zero(ig);

    // Add pattern: build/
    con u8 pat1[] = "build";
    ig.patterns[0].pattern[0] = pat1;
    ig.patterns[0].pattern[1] = pat1 + 5;
    ig.patterns[0].negated = NO;
    ig.patterns[0].anchored = NO;
    ig.patterns[0].dir_only = YES;
    ig.patterns[0].has_slash = NO;
    ig.count = 1;

    // Should match directories
    a_cstr(dir, "build");
    want(IGNOMatch(&ig, dir, YES) == YES);

    // Should not match files (dir_only)
    want(IGNOMatch(&ig, dir, NO) == NO);

    done;
}

ok64 IGNOtest3() {
    sane(1);
    // Empty igno should match nothing
    igno ig;
    zero(ig);

    a_cstr(path, "anything");
    want(IGNOMatch(&ig, path, NO) == NO);
    want(IGNOMatch(&ig, path, YES) == NO);

    done;
}

ok64 maintest() {
    sane(1);
    call(IGNOtest1);
    call(IGNOtest2);
    call(IGNOtest3);
    done;
}

TEST(maintest)
