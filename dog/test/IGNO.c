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

//  Table-driven glob/match property test (PTR-002 lock): one pattern,
//  many paths.  Locks `*` / `**` / `?` / anchor / has_slash semantics
//  before/after the slice-API rewrite of IGNOGlob + the splitter.
typedef struct {
    char const *pat;    // raw .gitignore pattern line
    char const *path;   // relative path to test
    b8 is_dir;
    b8 want;            // expected IGNOMatch result
} igno_case;

static ok64 igno_one(igno_case const *tc) {
    sane(tc != NULL);
    igno ig;
    zero(ig);
    igno_pat *pat = &ig.patterns[0];
    a_cstr(line, tc->pat);
    a_dup(u8c, p, line);
    if (!u8csEmpty(p) && *u8csHead(p) == '!') { pat->negated = YES; u8csUsed1(p); }
    if (!u8csEmpty(p) && *u8csHead(p) == '/') pat->anchored = YES;
    if (!u8csEmpty(p) && *u8csLast(p) == '/') { pat->dir_only = YES; u8csShed1(p); }
    if (u8csLen(p) > 1) {
        a_rest(u8c, tail, p, 1);
        if (u8csFind(tail, '/') == OK) pat->has_slash = YES;
    }
    u8csDup(pat->pattern, p);
    ig.count = 1;
    a_cstr(path, tc->path);
    b8 got = IGNOMatch(&ig, path, tc->is_dir);
    want(got == tc->want);
    done;
}

ok64 IGNOtest4() {
    sane(1);
    static igno_case const cases[] = {
        {"*.o",      "foo.o",          NO,  YES},
        {"*.o",      "a/b/foo.o",      NO,  YES},   // basename match anywhere
        {"*.o",      "foo.c",          NO,  NO },
        {"/*.o",     "foo.o",          NO,  YES},   // anchored at root
        {"/*.o",     "a/foo.o",        NO,  NO },   // anchored: not nested
        {"build/",   "build",          YES, YES},
        {"build/",   "build",          NO,  NO },   // dir-only
        {"foo?bar",  "fooXbar",        NO,  YES},
        {"foo?bar",  "foo/bar",        NO,  NO },   // ? never spans '/'
        {"a/b",      "x/a/b",          NO,  YES},   // has_slash, unanchored
        {"a/b",      "a/b",            NO,  YES},
        {"**/x.c",   "p/q/x.c",        NO,  YES},   // ** spans dirs
        {"src/**",   "src/a/b.c",      NO,  YES},
        {"node_modules", "a/node_modules/x", YES, YES},  // dir prefix ignore
    };
    for (u64 i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        call(igno_one, &cases[i]);
    }
    done;
}

ok64 maintest() {
    sane(1);
    call(IGNOtest1);
    call(IGNOtest2);
    call(IGNOtest3);
    call(IGNOtest4);
    done;
}

TEST(maintest)
