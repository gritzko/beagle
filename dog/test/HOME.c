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

#include "dog/test/TESTBE.h"

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

    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);

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
    TESTBErmrf(tmp);
    done;
}

ok64 HOMETestMissingFile() {
    sane(1);
    call(FILEInit);

    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);

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
    TESTBErmrf(tmp);
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

    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);

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
    TESTBErmrf(tmp);
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
        char tmp[256];
        want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
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

        //  rw bootstrap lays down `.be/wtlog` (top-level, per-wt).  It no
        //  longer creates a top-level `.be/refs` — `refs` belongs to the
        //  project shard and is keeper's job (DIS-024; Store.mkd).
        char p[512];
        struct stat st = {};
        snprintf(p, sizeof(p), "%s/" DOG_BE_NAME "/" DOG_WTLOG_NAME, tmp);
        want(stat(p, &st) == 0);
        want(S_ISREG(st.st_mode));
        snprintf(p, sizeof(p), "%s/" DOG_BE_NAME "/" DOG_REFS_NAME, tmp);
        want(stat(p, &st) != 0);   //  no top-level refs

        HOMEClose(&h);
        TESTBErmrf(tmp);
    }
    done;
}

//  rw=NO must NOT create anything — the historical NOHOME signal is
//  still needed by read-only callers (status, view projectors) so they
//  can distinguish "no repo here" from "repo open succeeded".
ok64 HOMETestRoNoBootstrap() {
    sane(1);
    call(FILEInit);

    char tmp[256];
    want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
    want(chdir(tmp) == 0);

    home h = {};
    uri none = {};
    want(HOMEOpen(&h, &none, NO) == NOHOME);
    HOMEClose(&h);

    char p[512];
    snprintf(p, sizeof(p), "%s/" DOG_BE_NAME, tmp);
    struct stat st = {};
    want(stat(p, &st) != 0);   // .be must NOT have been created

    TESTBErmrf(tmp);
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
        char tmp[256];
        want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
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

        TESTBErmrf(tmp);
    }
    done;
}

//  Project derivation (DIS-024 step 1): a primary wt with no row-0
//  project anchor takes its project from the store's single
//  `<root>/.be/<project>/` shard — read FROM THE STORE, not the wt dir
//  basename, so a store copied into a differently-named wt keeps its
//  project (the `cp -r src/.be/. .be/` checkout pattern).  A flat store
//  (no shard) or an ambiguous multi-project store (>1 shard) leaves the
//  project empty.
ok64 HOMETestProjectDerive() {
    sane(1);
    call(FILEInit);

    struct {
        char const *name;
        char const *shards[2];   // shard dirs to create under .be/ (NULL pad)
        char const *want;        // expected project ("" = empty)
    } const cases[] = {
        {"flat primary -> empty",           {NULL,    NULL},   ""},
        {"single shard -> shard name",      {"alpha",  NULL},  "alpha"},
        {"two shards -> empty (ambiguous)", {"alpha",  "beta"}, ""},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char tmp[256];
        want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
        fprintf(stderr, "  case: %s\n", cases[i].name);

        char p[400];
        snprintf(p, sizeof p, "%s/" DOG_BE_NAME, tmp);
        want(mkdir(p, 0755) == 0);
        //  empty wtlog: home_anchor_resolve derives no project from row 0.
        snprintf(p, sizeof p, "%s/" DOG_BE_NAME "/" DOG_WTLOG_NAME, tmp);
        { FILE *f = fopen(p, "w"); want(f != NULL); fclose(f); }
        for (int s = 0; s < 2; s++) {
            if (cases[i].shards[s] == NULL) continue;
            snprintf(p, sizeof p, "%s/" DOG_BE_NAME "/%s", tmp,
                     cases[i].shards[s]);
            want(mkdir(p, 0755) == 0);
        }

        a_cstr(root, tmp);
        home h = {};
        call(HOMEOpenAt, &h, root, NO);

        if (cases[i].want[0] != 0) {
            a_dup(u8c, pj, u8bDataC(h.project));
            want(slice_is(pj, cases[i].want));
        } else {
            want(u8bEmpty(h.project));
        }

        HOMEClose(&h);
        TESTBErmrf(tmp);
    }
    done;
}

//  DIS-037: `?/<title>` resolves to the shard DIR whose recorded title
//  matches, not the dir whose NAME matches.  A store may name a shard
//  `dogs` on disk while its project title (refs line-1 `get` row URI,
//  `?/<title>` override or URL basename) is `beagle`.  Addressing by
//  TITLE (`be get …?/beagle`) must land on the `dogs/` shard; the
//  on-disk dir name is an implementation detail.  Dir-name still wins
//  as a fast path / migration fallback when no title matches.
ok64 HOMETestTitleResolve() {
    sane(1);
    call(FILEInit);

    struct {
        char const *name;
        char const *shard_dir;   // on-disk shard dir under .be/
        char const *refs_row0;   // shard's refs line-1 get row (NULL = none)
        char const *query;       // `?/<...>` typed by the user (no leading ?)
        char const *want_proj;   // expected resolved h->project (the dir)
    } const cases[] = {
        //  Title override in refs: dir `dogs`, title `beagle` → resolve.
        {"title override -> shard dir",
         "dogs", "26612AcUE7\tget\tssh://localhost/src/dogs?/beagle#0\n",
         "/beagle", "dogs"},
        //  Title from URL basename: dir `dogs`, source basename `dogs`,
        //  user types the dir name directly → direct dir match (fast path).
        {"dir name direct match",
         "dogs", "26612AcUE7\tget\tssh://localhost/src/dogs?#0\n",
         "/dogs", "dogs"},
        //  No title match anywhere: fall back to the typed name verbatim
        //  (lets a fresh clone mkdir it).
        {"no match -> verbatim fallback",
         "dogs", "26612AcUE7\tget\tssh://localhost/src/dogs?#0\n",
         "/nonesuch", "nonesuch"},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char tmp[256];
        want(TESTBEmkdtemp(tmp, sizeof tmp) == OK);
        fprintf(stderr, "  case: %s\n", cases[i].name);

        char p[400];
        snprintf(p, sizeof p, "%s/" DOG_BE_NAME, tmp);
        want(mkdir(p, 0755) == 0);
        //  empty wtlog: no row-0 project anchor.
        snprintf(p, sizeof p, "%s/" DOG_BE_NAME "/" DOG_WTLOG_NAME, tmp);
        { FILE *f = fopen(p, "w"); want(f != NULL); fclose(f); }
        //  the shard dir + its refs line-1 get row.
        snprintf(p, sizeof p, "%s/" DOG_BE_NAME "/%s", tmp, cases[i].shard_dir);
        want(mkdir(p, 0755) == 0);
        if (cases[i].refs_row0 != NULL) {
            snprintf(p, sizeof p, "%s/" DOG_BE_NAME "/%s/" DOG_REFS_NAME,
                     tmp, cases[i].shard_dir);
            FILE *f = fopen(p, "w"); want(f != NULL);
            fputs(cases[i].refs_row0, f);
            fclose(f);
        }

        //  Open with the typed `?/<title>` query (and the store root in
        //  the path slot) — exactly what `be get file:<store>?/<title>`
        //  forwards.
        a_cstr(root, tmp);
        a_cstr(q, cases[i].query);
        home h = {};
        uri at = {};
        at.path[0]  = root[0];  at.path[1]  = root[1];
        at.query[0] = q[0];     at.query[1] = q[1];
        call(HOMEOpen, &h, &at, NO);

        a_dup(u8c, pj, u8bDataC(h.project));
        want(slice_is(pj, cases[i].want_proj));

        HOMEClose(&h);
        TESTBErmrf(tmp);
    }
    done;
}

//  GET-010 Defect-2 — the HOME-escape.  `home_walk_up` is an unbounded
//  cwd→`/` ascent.  When a process runs under `$HOME` with no local `.be`
//  shield it used to keep climbing PAST `$HOME` and grab/create a store
//  sitting above it (on the dev box, the real `~/.be`) — breaking test
//  hermeticity.  The bound: if the walk STARTS under `$HOME` (canonical
//  prefix), `$HOME` is the ceiling and the ascent stops there; if the
//  start dir is NOT under `$HOME`, the walk stays UNBOUNDED (single-
//  project `~` anchors / stores outside `$HOME` keep working).
//
//  Each case lays a single-project STORE one level ABOVE a `home` dir
//  (`<base>/store/.be/proj`), with `<base>/store/home/work/deep` as cwd.
//  The `home` dir itself has NO `.be` shield, so the OLD walk escapes to
//  `<base>/store/.be`.  We then point `$HOME` at one of two places and
//  assert the walk's outcome.
ok64 HOMETestWalkHomeBound() {
    sane(1);
    call(FILEInit);

    enum { HOME_AT_HOMEDIR, HOME_ELSEWHERE };
    struct {
        char const *name;
        int         home_at;     // where $HOME points
        ok64        want_ret;     // expected HOMEFind result
    } const cases[] = {
        //  $HOME == the shield-less `home` dir, cwd under it: the ascent
        //  must NOT escape to the store above $HOME → NOHOME (bounded).
        {"started under $HOME -> bound at $HOME (no escape)",
         HOME_AT_HOMEDIR, NOHOME},
        //  $HOME points OUTSIDE the cwd's ancestry: walk stays unbounded
        //  and finds the store above cwd → OK (semantics preserved).
        {"started outside $HOME -> unbounded (finds store above cwd)",
         HOME_ELSEWHERE, OK},
    };

    //  Save and restore the inherited $HOME around the whole run.
    char const *home_saved = getenv("HOME");
    char home_save_buf[512] = {0};
    if (home_saved != NULL) snprintf(home_save_buf, sizeof home_save_buf,
                                     "%s", home_saved);

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char base_raw[256];
        want(TESTBEmkdtemp(base_raw, sizeof base_raw) == OK);
        //  Canonicalize: on macOS `/tmp` is a symlink to `/private/tmp`,
        //  so getcwd() inside the scratch returns `/private/tmp/...`
        //  while `$HOME` we set below would carry the un-resolved
        //  `/tmp/...` form — u8csEq would fail and the bounded walk
        //  would escape past `$HOME`.
        char base[512];
        if (realpath(base_raw, base) == NULL)
            snprintf(base, sizeof base, "%s", base_raw);
        fprintf(stderr, "  case: %s\n", cases[i].name);

        char p[512];
        //  store above $HOME: <base>/store/.be/proj (single-project,
        //  shieldlike — an anchor the old walk would adopt).
        snprintf(p, sizeof p, "%s/store", base);            want(mkdir(p, 0755) == 0);
        snprintf(p, sizeof p, "%s/store/" DOG_BE_NAME, base); want(mkdir(p, 0755) == 0);
        snprintf(p, sizeof p, "%s/store/" DOG_BE_NAME "/proj", base);
        want(mkdir(p, 0755) == 0);
        //  the `home` dir (NO `.be` of its own) and a deep cwd under it.
        snprintf(p, sizeof p, "%s/store/home", base);            want(mkdir(p, 0755) == 0);
        snprintf(p, sizeof p, "%s/store/home/work", base);       want(mkdir(p, 0755) == 0);
        snprintf(p, sizeof p, "%s/store/home/work/deep", base);  want(mkdir(p, 0755) == 0);
        //  the "elsewhere" HOME — outside the cwd's ancestry.
        snprintf(p, sizeof p, "%s/elsewhere", base);             want(mkdir(p, 0755) == 0);

        char homedir[512];
        if (cases[i].home_at == HOME_AT_HOMEDIR)
            snprintf(homedir, sizeof homedir, "%s/store/home", base);
        else
            snprintf(homedir, sizeof homedir, "%s/elsewhere", base);
        want(setenv("HOME", homedir, 1) == 0);

        char cwd[512];
        snprintf(cwd, sizeof cwd, "%s/store/home/work/deep", base);
        want(chdir(cwd) == 0);

        //  Drive the walk through HOMEOpen (rw=NO) — it allocates the
        //  h->wt / h->root buffers the walk's anchor-resolve writes into.
        //  A bare HOMEFind(&h={}) would hit those buffers unallocated.
        home h = {};
        uri none = {};
        ok64 got = HOMEOpen(&h, &none, NO);
        want(got == cases[i].want_ret);
        if (got == OK) {
            //  unbounded path discovered the store ABOVE cwd, not the
            //  shield-less home dir — confirm root lands on .../store.
            char wantroot[512];
            snprintf(wantroot, sizeof wantroot, "%s/store", base);
            a_dup(u8c, rt, u8bDataC(h.root));
            want(slice_is(rt, wantroot));
        }
        HOMEClose(&h);

        //  Step out of the scratch before removing it.
        want(chdir("/tmp") == 0);
        TESTBErmrf(base);
    }

    //  Restore the inherited HOME so later tests / teardown are unaffected.
    if (home_save_buf[0] != 0) setenv("HOME", home_save_buf, 1);
    else                       unsetenv("HOME");
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
    fprintf(stderr, "HOMETestProjectDerive...\n");
    call(HOMETestProjectDerive);
    fprintf(stderr, "HOMETestTitleResolve...\n");
    call(HOMETestTitleResolve);
    fprintf(stderr, "HOMETestWalkHomeBound...\n");
    call(HOMETestWalkHomeBound);
    fprintf(stderr, "all passed\n");
    done;
}

TEST(maintest)
