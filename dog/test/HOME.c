//  HOME: TOML config getter keyed by dotted path.
//
#include "dog/HOME.h"
#include "dog/DOG.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "abc/FILE.h"
#include "abc/PRO.h"
#include "abc/TEST.h"

static void seed_config(char const *root, char const *body) {
    char dir[256];
    snprintf(dir, sizeof(dir), "%s/" DOG_BE_NAME, root);
    mkdir(dir, 0755);
    char path[256];
    snprintf(path, sizeof(path), "%s/" DOG_BE_NAME "/" DOG_CONFIG_NAME, root);
    FILE *f = fopen(path, "w");
    fputs(body, f);
    fclose(f);
}

ok64 HOMETestGet() {
    sane(1);
    call(FILEInit);

    char tmp[] = "/tmp/dog-home-XXXXXX";
    want(mkdtemp(tmp) != NULL);

    seed_config(tmp,
        "# dogs config\n"
        "[user]\n"
        "name  = \"Ada Lovelace\"\n"
        "email = \"ada@example.com\"\n"
        "[remote]\n"
        "origin = \"ssh://host/repo\"\n"
        "[a.b]\n"
        "c = \"nested\"\n");

    a_cstr(root, tmp);
    home h = {};
    call(HOMEOpenAt, &h, root, NO);

    u8 vbuf[128];

    // --- hit: [user] name ---
    {
        u8s val = {vbuf, vbuf + sizeof(vbuf)};
        u8 *val_start = val[0];
        a_cstr(u, "user");
        a_cstr(n, "name");
        a_path(needle, u, n);
        call(HOMEGetConfig, &h, val, $path(needle));
        u8cs got = {val_start, val[0]};
        a_cstr(wantn, "Ada Lovelace");
        want($eq(got, wantn));
    }

    // --- hit: [remote] origin ---
    {
        u8s val = {vbuf, vbuf + sizeof(vbuf)};
        u8 *val_start = val[0];
        a_cstr(r, "remote");
        a_cstr(o, "origin");
        a_path(needle, r, o);
        call(HOMEGetConfig, &h, val, $path(needle));
        u8cs got = {val_start, val[0]};
        a_cstr(wanto, "ssh://host/repo");
        want($eq(got, wanto));
    }

    // --- hit: nested [a.b] c ---
    {
        u8s val = {vbuf, vbuf + sizeof(vbuf)};
        u8 *val_start = val[0];
        a_cstr(a, "a");
        a_cstr(b, "b");
        a_cstr(k, "c");
        a_path(needle, a, b, k);
        call(HOMEGetConfig, &h, val, $path(needle));
        u8cs got = {val_start, val[0]};
        a_cstr(wantn, "nested");
        want($eq(got, wantn));
    }

    // --- miss: wrong key ---
    {
        u8s val = {vbuf, vbuf + sizeof(vbuf)};
        a_cstr(u, "user");
        a_cstr(n, "nope");
        a_path(needle, u, n);
        want(HOMEGetConfig(&h, val, $path(needle)) == NOCONF);
    }

    // --- miss: wrong section ---
    {
        u8s val = {vbuf, vbuf + sizeof(vbuf)};
        a_cstr(s, "nope");
        a_cstr(n, "name");
        a_path(needle, s, n);
        want(HOMEGetConfig(&h, val, $path(needle)) == NOCONF);
    }

    HOMEClose(&h);
    char rm[512];
    snprintf(rm, sizeof(rm), "rm -rf %s", tmp);
    system(rm);
    done;
}

ok64 HOMETestMissingFile() {
    sane(1);
    call(FILEInit);

    char tmp[] = "/tmp/dog-home-XXXXXX";
    want(mkdtemp(tmp) != NULL);

    a_cstr(root, tmp);
    home h = {};
    call(HOMEOpenAt, &h, root, NO);

    u8 vbuf[64];
    u8s val = {vbuf, vbuf + sizeof(vbuf)};
    a_cstr(u, "user");
    a_cstr(n, "name");
    a_path(needle, u, n);
    want(HOMEGetConfig(&h, val, $path(needle)) == NOCONF);

    HOMEClose(&h);
    char rm[512];
    snprintf(rm, sizeof(rm), "rm -rf %s", tmp);
    system(rm);
    done;
}

static b8 slice_is(u8cs s, char const *lit) {
    size_t l = strlen(lit);
    if ((size_t)$len(s) != l) return NO;
    if (l == 0) return YES;
    return memcmp(s[0], lit, l) == 0;
}

ok64 HOMETestBranches() {
    sane(1);
    call(FILEInit);

    char tmp[] = "/tmp/dog-home-XXXXXX";
    want(mkdtemp(tmp) != NULL);

    a_cstr(root, tmp);
    home h = {};
    call(HOMEOpenAt, &h, root, NO);

    // 1. No branches yet → WriteBranch returns HOMENOBR.
    {
        u8cs w = {};
        want(HOMEWriteBranch(&h, w) == HOMENOBR);
    }

    // 2. Open trunk ro first.  WriteBranch HOMENOBR because the
    //    first open was ro (cur_rw=NO).
    {
        a_cstr(trunk, "main");
        want(HOMEOpenBranch(&h, trunk, NO) == OK);
        u8cs w = {};
        want(HOMEWriteBranch(&h, w) == HOMENOBR);
    }

    // 3. A later rw open re-targets cur (test/CLI pattern: open
    //    trunk, close keeper, open feature with rw).  Returns OK
    //    when the rw branch differs from cur; HOMEOPEN when it
    //    matches (idempotent promote).
    {
        a_cstr(feat, "heads/feature");
        want(HOMEOpenBranch(&h, feat, YES) == OK);
        u8cs w = {};
        want(HOMEWriteBranch(&h, w) == OK);
        want(slice_is(w, "heads/feature/"));
    }

    // 5. Dedup: reopening the same branch returns HOMEOPEN.
    {
        a_cstr(feat2, "heads/feature/");
        want(HOMEOpenBranch(&h, feat2, YES) == HOMEOPEN);
    }

    // 6. Second ro branch claims aux.  A third ro branch hits the
    //    aux pin and is refused with HOMEROBR.
    {
        a_cstr(other, "other/fix");
        want(HOMEOpenBranch(&h, other, NO) == OK);   // claims aux
        a_cstr(other2, "other/fix");
        want(HOMEOpenBranch(&h, other2, NO) == HOMEOPEN);  // idempotent
        a_cstr(third, "third/branch");
        want(HOMEOpenBranch(&h, third, NO) == HOMEROBR);   // aux pinned
    }

    // 7. HOMEBranchVisible — cur OR aux.  cur = "heads/feature/",
    //    aux = "other/fix/".
    {
        a_cstr(trunk_s, "");
        u8cs trunk_b = {trunk_s[0], trunk_s[1]};
        want(HOMEBranchVisible(&h, trunk_b) == YES);       // ancestor of all

        a_cstr(feat_s, "heads/feature/");
        u8cs feat_b = {feat_s[0], feat_s[1]};
        want(HOMEBranchVisible(&h, feat_b) == YES);        // exact match (cur)

        a_cstr(heads_s, "heads/");
        u8cs heads_b = {heads_s[0], heads_s[1]};
        want(HOMEBranchVisible(&h, heads_b) == YES);       // ancestor of cur

        a_cstr(other_s, "other/fix/");
        u8cs other_b = {other_s[0], other_s[1]};
        want(HOMEBranchVisible(&h, other_b) == YES);       // exact match (aux)

        a_cstr(stray_s, "nope/");
        u8cs stray_b = {stray_s[0], stray_s[1]};
        want(HOMEBranchVisible(&h, stray_b) == NO);
    }

    // 8. HOMESetCurBranch re-targets cur without touching aux.
    {
        a_cstr(other_root, "other");
        want(HOMESetCurBranch(&h, other_root) == OK);
        u8cs w = {};
        want(HOMEWriteBranch(&h, w) == OK);
        want(slice_is(w, "other/"));
    }

    HOMEClose(&h);
    char rm[512];
    snprintf(rm, sizeof(rm), "rm -rf %s", tmp);
    system(rm);
    done;
}

//  rw bootstrap: `HOMEOpen(rw=YES)` in a dir with no `.be/` anchor
//  must (1) succeed instead of failing NOHOME and (2) lay down the
//  canonical empty-state layout — `<cwd>/.be/{refs,wtlog}` — so the
//  next walk-up finds a well-formed anchor.  Table-driven over the
//  three input shapes the production code path actually exercises.
ok64 HOMETestRwBootstrap() {
    sane(1);
    call(FILEInit);

    struct {
        char const *name;
        b8          via_at;          // YES → HOMEOpenAt(root), NO → cwd-walk
        b8          preexisting_be;  // mkdir .be/ before open (no markers)
    } const cases[] = {
        {"cwd-walk, empty dir",   NO,  NO},
        {"cwd-walk, .be exists",  NO,  YES},
        {"HOMEOpenAt, empty dir", YES, NO},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char tmp[] = "/tmp/dog-home-rw-XXXXXX";
        want(mkdtemp(tmp) != NULL);
        fprintf(stderr, "  case: %s\n", cases[i].name);

        if (cases[i].preexisting_be) {
            char bedir[256];
            snprintf(bedir, sizeof(bedir), "%s/" DOG_BE_NAME, tmp);
            want(mkdir(bedir, 0755) == 0);
        }

        home h = {};
        if (cases[i].via_at) {
            a_cstr(root, tmp);
            call(HOMEOpenAt, &h, root, YES);
        } else {
            want(chdir(tmp) == 0);
            uri none = {};
            call(HOMEOpen, &h, &none, YES);
        }

        char p[512];
        snprintf(p, sizeof(p), "%s/" DOG_BE_NAME "/" DOG_REFS_NAME, tmp);
        struct stat st = {};
        want(stat(p, &st) == 0);
        want(S_ISREG(st.st_mode));
        snprintf(p, sizeof(p), "%s/" DOG_BE_NAME "/" DOG_WTLOG_NAME, tmp);
        want(stat(p, &st) == 0);
        want(S_ISREG(st.st_mode));

        HOMEClose(&h);
        char rm[512];
        snprintf(rm, sizeof(rm), "rm -rf %s", tmp);
        system(rm);
    }
    done;
}

//  rw=NO must NOT create anything — the historical NOHOME signal is
//  still needed by read-only callers (status, view projectors) so they
//  can distinguish "no repo here" from "repo open succeeded".
ok64 HOMETestRoNoBootstrap() {
    sane(1);
    call(FILEInit);

    char tmp[] = "/tmp/dog-home-ro-XXXXXX";
    want(mkdtemp(tmp) != NULL);
    want(chdir(tmp) == 0);

    home h = {};
    uri none = {};
    want(HOMEOpen(&h, &none, NO) == NOHOME);
    HOMEClose(&h);

    char p[512];
    snprintf(p, sizeof(p), "%s/" DOG_BE_NAME, tmp);
    struct stat st = {};
    want(stat(p, &st) != 0);   // .be must NOT have been created

    char rm[512];
    snprintf(rm, sizeof(rm), "rm -rf %s", tmp);
    system(rm);
    done;
}

//  Repo-vs-worktree taxonomy at the HOME layer:
//    * a `.be/` *dir* is a repo — opening it RO is legitimate even with
//      an empty wtlog (it may hold only config / packs).  The "empty
//      wtlog ⇒ no worktree" refusal lives in the worktree dog
//      (SNIFFOpen), covered by sniff/test/norepo.sh.
//    * a `.be` *file* is ONLY ever a secondary-wt anchor; its row 0
//      must be a valid `repo` URI.  Empty / invalid ⇒ NOHOME, the same
//      row-0 validation a primary wtlog gets, regardless of rw.
ok64 HOMETestRoEmptyAnchor() {
    sane(1);
    call(FILEInit);

    enum { K_DIR, K_FILE };
    struct {
        char const *name;
        int         kind;   // K_DIR: .be/ dir + empty wtlog; K_FILE: empty .be file
        b8          rw;
        ok64        want_ret;
    } const cases[] = {
        {"repo .be/ + empty wtlog, RO",  K_DIR,  NO,  OK},     // bare repo open is OK
        {"repo .be/ + empty wtlog, RW",  K_DIR,  YES, OK},     // rw bootstraps row 0
        {"secondary empty .be file, RO", K_FILE, NO,  NOTAWT}, // invalid anchor
        {"secondary empty .be file, RW", K_FILE, YES, NOTAWT}, // never bootstraps over it
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char tmp[] = "/tmp/dog-home-empty-XXXXXX";
        want(mkdtemp(tmp) != NULL);
        fprintf(stderr, "  case: %s\n", cases[i].name);

        char p[512];
        if (cases[i].kind == K_DIR) {
            snprintf(p, sizeof(p), "%s/" DOG_BE_NAME, tmp);
            want(mkdir(p, 0755) == 0);
            //  Lay down empty marker logs, exactly as a half-bootstrapped
            //  store would have them (home_ensure_markers shape).
            snprintf(p, sizeof(p), "%s/" DOG_BE_NAME "/" DOG_WTLOG_NAME, tmp);
            FILE *f = fopen(p, "w"); want(f != NULL); fclose(f);
            snprintf(p, sizeof(p), "%s/" DOG_BE_NAME "/" DOG_REFS_NAME, tmp);
            f = fopen(p, "w"); want(f != NULL); fclose(f);
        } else {
            //  Secondary anchor: `.be` is an empty regular file.
            snprintf(p, sizeof(p), "%s/" DOG_BE_NAME, tmp);
            FILE *f = fopen(p, "w"); want(f != NULL); fclose(f);
        }

        want(chdir(tmp) == 0);
        home h = {};
        uri none = {};
        ok64 got = HOMEOpen(&h, &none, cases[i].rw);
        want(got == cases[i].want_ret);
        HOMEClose(&h);

        char rm[512];
        snprintf(rm, sizeof(rm), "rm -rf %s", tmp);
        system(rm);
    }
    done;
}

ok64 maintest() {
    sane(1);
    fprintf(stderr, "HOMETestGet...\n");
    call(HOMETestGet);
    fprintf(stderr, "HOMETestMissingFile...\n");
    call(HOMETestMissingFile);
    fprintf(stderr, "HOMETestBranches...\n");
    call(HOMETestBranches);
    fprintf(stderr, "HOMETestRwBootstrap...\n");
    call(HOMETestRwBootstrap);
    fprintf(stderr, "HOMETestRoNoBootstrap...\n");
    call(HOMETestRoNoBootstrap);
    fprintf(stderr, "HOMETestRoEmptyAnchor...\n");
    call(HOMETestRoEmptyAnchor);
    fprintf(stderr, "all passed\n");
    done;
}

TEST(maintest)
