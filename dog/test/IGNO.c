#include "IGNO.h"

#include <string.h>

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/TEST.h"

ok64 IGNOtest1() {
    sane(1);
    // Test basic pattern matching with an in-memory igno struct
    igno ig;
    zero(ig);
    igno_set *s0 = &ig.set[0];

    // Add pattern: *.o
    con u8 pat1[] = "*.o";
    s0->patterns[0].pattern[0] = pat1;
    s0->patterns[0].pattern[1] = pat1 + 3;
    s0->patterns[0].negated = NO;
    s0->patterns[0].anchored = NO;
    s0->patterns[0].dir_only = NO;
    s0->patterns[0].has_slash = NO;
    s0->count = 1;
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
    igno_set *s0 = &ig.set[0];

    // Add pattern: build/
    con u8 pat1[] = "build";
    s0->patterns[0].pattern[0] = pat1;
    s0->patterns[0].pattern[1] = pat1 + 5;
    s0->patterns[0].negated = NO;
    s0->patterns[0].anchored = NO;
    s0->patterns[0].dir_only = YES;
    s0->patterns[0].has_slash = NO;
    s0->count = 1;
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

//  --- shared single-pattern set builder ---------------------------------
//  Fill `set` from one raw .gitignore line (mirrors IGNOLoad's per-line
//  parse).  Returns nothing; on a `!`-only / empty line leaves count 0.
static void igno_fill_set(igno_set *set, char const *raw) {
    set->count = 0;
    a_cstr(line, raw);
    a_dup(u8c, p, line);
    igno_pat *pat = &set->patterns[0];
    zerop(pat);
    if (!u8csEmpty(p) && *u8csHead(p) == '!') { pat->negated = YES; u8csUsed1(p); }
    if (u8csEmpty(p)) return;
    if (!u8csEmpty(p) && *u8csHead(p) == '/') pat->anchored = YES;
    if (!u8csEmpty(p) && *u8csLast(p) == '/') { pat->dir_only = YES; u8csShed1(p); }
    if (u8csLen(p) > 1) {
        a_rest(u8c, tail, p, 1);
        if (u8csFind(tail, '/') == OK) pat->has_slash = YES;
    }
    u8csDup(pat->pattern, p);
    set->count = 1;
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
    igno_fill_set(&ig.set[0], tc->pat);
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

//  STATUS-002: hierarchical chain property test (in-memory).  Build a
//  2-level chain by hand: set[0] is the DEEPEST (anchor = the sub wt
//  root, empty prefix); set[1] is the parent one dir up (prefix
//  `<sub>`).  IGNOMatch must consult set[1] for paths the sub's own
//  (set[0]) doesn't cover — and let a deeper rule override a shallower
//  one (git precedence, incl. `!` negation).
typedef struct {
    char const *deep_pat;    // anchor (sub) .gitignore line ("" = none)
    char const *parent_pat;  // parent .gitignore line ("" = none)
    char const *prefix;      // anchor path relative to parent dir
    char const *path;        // anchor-relative path to classify
    b8 is_dir;
    b8 want;
} igno_chain_case;

static ok64 igno_chain_one(igno_chain_case const *tc) {
    sane(tc != NULL);
    igno ig;
    zero(ig);
    igno_fill_set(&ig.set[0], tc->deep_pat);       // empty prefix
    igno_fill_set(&ig.set[1], tc->parent_pat);
    a_cstr(pfx, tc->prefix);
    u8csDup(ig.set[1].prefix, pfx);
    ig.count = 2;
    a_cstr(path, tc->path);
    b8 got = IGNOMatch(&ig, path, tc->is_dir);
    want(got == tc->want);
    done;
}

ok64 IGNOtest5() {
    sane(1);
    static igno_chain_case const cases[] = {
        //  The repro: sub has NO ignore, parent lists build-*/ and *.o;
        //  classifying a sub-relative build path must be IGNORED via the
        //  parent set (prefix = the sub's name as seen from the parent).
        {"", "build-*/", "abc", "build-debug",              YES, YES},
        {"", "build-*/", "abc", "build-debug/CMakeCache.txt", NO, YES}, // under dir
        {"", "*.o",      "abc", "build-debug/foo.o",          NO, YES},
        {"", "*.o",      "abc", "core.c",                     NO, NO },  // not ignored
        //  Parent ignores build-*/, sub un-ignores it via `!` negation
        //  (deeper set overrides shallower) → NOT ignored.
        {"!build-debug/", "build-*/", "abc", "build-debug",   YES, NO },
        //  Deeper set IGNORES what parent leaves alone.
        {"secret",        "",         "abc", "secret",        NO, YES},
    };
    for (u64 i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        call(igno_chain_one, &cases[i]);
    }
    done;
}

//  STATUS-002: hermetic on-disk test.  Build a temp tree UNDER /tmp
//  (not $HOME, so the walk terminates at `/` without picking up the
//  real $HOME/.gitignore):
//
//    <tmp>/.gitignore          ->  build-*/, *.o, !keep.o
//    <tmp>/sub/                ->  the "submodule" wt root (no .gitignore)
//    <tmp>/sub/build-debug/x   ->  ignored via PARENT rule, crossing the
//                                  sub boundary (the bug)
//    <tmp>/sub/keep.o          ->  un-ignored via `!keep.o` (precedence)
//    <tmp>/sub/core.c          ->  not ignored
//
//  IGNOLoad anchors at <tmp>/sub and must compound the parent rule.
static ok64 igno_write_file(path8s path, char const *body) {
    sane(path[0] && body);
    int fd = 0;
    call(FILECreate, &fd, path);
    a_cstr(text, body);
    u8cs t = {text[0], text[1]};
    call(FILEFeed, fd, t);
    call(FILEClose, &fd);
    done;
}

ok64 IGNOtest6() {
    sane(1);

    //  Unique temp root under /tmp.
    a_path(root, $cstr("/tmp"));
    a_cstr(tmpl, "IGNOtest6_XXXXXX");
    call(PATHu8bAddTmp, root, tmpl);
    call(FILEMakeDir, $path(root));

    //  Parent .gitignore at the temp root.
    a_path(gi, u8bDataC(root), ((u8cs)u8slit(".gitignore")));
    call(igno_write_file, $path(gi), "build-*/\n*.o\n!keep.o\n");

    //  Sub wt root + a build dir, all WITHOUT a local .gitignore.
    a_path(sub, u8bDataC(root), ((u8cs)u8slit("sub")));
    call(FILEMakeDir, $path(sub));
    a_path(bd, u8bDataC(sub), ((u8cs)u8slit("build-debug")));
    call(FILEMakeDir, $path(bd));

    //  Anchor IGNOLoad at the SUB (as `be status:<sub>/` would).
    igno ig;
    zero(ig);
    a_dup(u8c, subdir, u8bDataC(sub));
    call(IGNOLoad, &ig, subdir);

    //  Parent rule crosses the sub boundary — the bug's fix.
    a_cstr(p_builddir, "build-debug");
    want(IGNOMatch(&ig, p_builddir, YES) == YES);
    a_cstr(p_buildfile, "build-debug/CMakeCache.txt");
    want(IGNOMatch(&ig, p_buildfile, NO) == YES);
    a_cstr(p_obj, "build-debug/foo.o");
    want(IGNOMatch(&ig, p_obj, NO) == YES);

    //  `!keep.o` negation (precedence) — *.o is ignored, keep.o is not.
    a_cstr(p_oo, "thing.o");
    want(IGNOMatch(&ig, p_oo, NO) == YES);
    a_cstr(p_keep, "keep.o");
    want(IGNOMatch(&ig, p_keep, NO) == NO);

    //  A normal source file is not ignored.
    a_cstr(p_src, "core.c");
    want(IGNOMatch(&ig, p_src, NO) == NO);

    IGNOFree(&ig);

    //  Cleanup (best-effort recursive rmdir).
    (void)FILERmDir($path(root), true);
    done;
}

ok64 maintest() {
    sane(1);
    call(IGNOtest1);
    call(IGNOtest2);
    call(IGNOtest3);
    call(IGNOtest4);
    call(IGNOtest5);
    call(IGNOtest6);
    done;
}

TEST(maintest)
