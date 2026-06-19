//  WALK tests: tree walker on KEEP (eager / lazy / skip / stop).
//
#include "dog/git/GIT.h"
#include "keeper/KEEP.h"
#include "dog/git/SHA1.h"
#include "keeper/WALK.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/LSM.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/RON.h"
#include "abc/TEST.h"
#include "dog/DOG.h"
#include "dog/ULOG.h"

// ---- Test 1: WALKu8sModeKind table-driven ----

ok64 WALKtest1() {
    sane(1);

    typedef struct {
        char const *mode;
        u8          want_kind;
    } row;

    row cases[] = {
        {"40000",  WALK_KIND_DIR},
        {"100644", WALK_KIND_REG},
        {"100755", WALK_KIND_EXE},
        {"120000", WALK_KIND_LNK},
        {"160000", WALK_KIND_SUB},
        {"",       0},
        {"1",      0},               // too short after '1'
        {"2",      0},               // unknown first digit
        {"999999", 0},               // unknown first digit
        {"100000", WALK_KIND_REG},   // '0' at pos 3 → REG
        {"100700", WALK_KIND_EXE},   // '7' at pos 3 → EXE
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        u8cs m = {(u8cp)cases[i].mode,
                  (u8cp)cases[i].mode + strlen(cases[i].mode)};
        u8 got = WALKu8sModeKind(m);
        if (got != cases[i].want_kind) {
            fprintf(stderr, "mode='%s': want %u got %u\n",
                    cases[i].mode, cases[i].want_kind, got);
        }
        want(got == cases[i].want_kind);
    }

    done;
}

// ---- Test 2: WALKTree / WALKTreeLazy on a synthetic KEEP store ----
//
// Tree layout:
//   hello.txt       "hi\n"            (100644)
//   run.sh          "#!/bin/sh\n"     (100755)
//   sub/nested.txt  "deep\n"          (100644)
//
// Verifies: visit count, kind dispatch, eager-vs-lazy blob filling,
// WALKSKIP pruning a subtree.

typedef struct {
    u32 n_entries;
    u32 n_files;              // REG+EXE+LNK
    u32 n_dirs;
    u32 n_files_with_blob;
    char last_path[256];
    char dir_to_skip[64];     // if non-empty, skip the DIR entry matching
} w2_ctx;

static ok64 w2_visit(u8cs path, u8 kind, u8cp esha, u8cs blob,
                      void0p vctx) {
    (void)esha;
    w2_ctx *c = (w2_ctx *)vctx;
    c->n_entries++;

    size_t plen = $len(path);
    if (plen >= sizeof(c->last_path)) plen = sizeof(c->last_path) - 1;
    memcpy(c->last_path, path[0], plen);
    c->last_path[plen] = 0;

    if (kind == WALK_KIND_DIR) {
        c->n_dirs++;
        if (c->dir_to_skip[0]) {
            size_t dlen = strlen(c->dir_to_skip);
            if (plen == dlen &&
                memcmp(c->last_path, c->dir_to_skip, dlen) == 0)
                return WALKSKIP;
        }
    } else if (kind == WALK_KIND_REG || kind == WALK_KIND_EXE ||
               kind == WALK_KIND_LNK) {
        c->n_files++;
        if (!$empty(blob)) c->n_files_with_blob++;
    }
    return OK;
}

// Build one "leaf tree" — a single-entry tree containing a blob.
static ok64 build_leaf_tree(keeper *k, keep_pack *p,
                             u8cs mode_name, u8cs content,
                             sha1 *tree_out) {
    sane(k && p && tree_out);
    sha1 bsha = {};
    call(KEEPPackFeed, p, DOG_OBJ_BLOB, content, 0, &bsha);
    a_pad(u8, tb, 256);
    call(u8bFeed, tb, mode_name);
    u8bFeed1(tb, 0);
    a_rawc(ss, bsha);
    call(u8bFeed, tb, ss);
    a_dup(u8c, tc, u8bData(tb));
    call(KEEPPackFeed, p, DOG_OBJ_TREE, tc, 0, tree_out);
    done;
}

ok64 WALKtest2() {
    sane(1);
    call(FILEInit);

    char tmp[] = "/tmp/walktest-XXXXXX";
    want(mkdtemp(tmp) != NULL);
    a_cstr(root, tmp);
    home h = {};
    call(HOMEOpenAt, root, YES);

    
    call(KEEPOpen, YES);
    keep_pack p = {};
    call(KEEPPackOpen, &p);
    //  This test feeds blobs before trees to exercise WALK against a
    //  hand-rolled tree hierarchy; that's non-canonical but the walker
    //  doesn't care.  Drop the intra-pack ordering check for the pack.
    p.strict_order = NO;

    // Leaf blobs.
    a_cstr(hi_content, "hi\n");
    sha1 hi_sha = {};
    call(KEEPPackFeed, &p, DOG_OBJ_BLOB, hi_content, 0, &hi_sha);

    a_cstr(run_content, "#!/bin/sh\n");
    sha1 run_sha = {};
    call(KEEPPackFeed, &p, DOG_OBJ_BLOB, run_content, 0, &run_sha);

    // Inner "sub" tree.
    a_cstr(nested_mn, "100644 nested.txt");
    a_cstr(nested_content, "deep\n");
    sha1 sub_sha = {};
    call(build_leaf_tree, &KEEP, &p, nested_mn, nested_content, &sub_sha);

    // Root tree — git sort order: hello.txt < run.sh < sub.
    a_pad(u8, rtb, 512);
    a_cstr(e1, "100644 hello.txt");
    call(u8bFeed, rtb, e1);
    u8bFeed1(rtb, 0);
    a_rawc(hi_ss, hi_sha);
    call(u8bFeed, rtb, hi_ss);

    a_cstr(e2, "100755 run.sh");
    call(u8bFeed, rtb, e2);
    u8bFeed1(rtb, 0);
    a_rawc(run_ss, run_sha);
    call(u8bFeed, rtb, run_ss);

    a_cstr(e3, "40000 sub");
    call(u8bFeed, rtb, e3);
    u8bFeed1(rtb, 0);
    a_rawc(sub_ss, sub_sha);
    call(u8bFeed, rtb, sub_ss);

    a_dup(u8c, rtc, u8bData(rtb));
    sha1 root_sha = {};
    call(KEEPPackFeed, &p, DOG_OBJ_TREE, rtc, 0, &root_sha);

    call(KEEPPackClose, &p);

    // Eager walk.  One root DIR visit, three files, one sub DIR.
    {
        w2_ctx c = {};
        call(WALKTree, root_sha.data, w2_visit, &c);
        want(c.n_entries == 5);
        want(c.n_files == 3);
        want(c.n_dirs == 2);
        want(c.n_files_with_blob == 3);  // eager: all files carry blob
    }

    // Lazy walk — blobs empty.
    {
        w2_ctx c = {};
        call(WALKTreeLazy, root_sha.data, w2_visit, &c);
        want(c.n_entries == 5);
        want(c.n_files == 3);
        want(c.n_dirs == 2);
        want(c.n_files_with_blob == 0);
    }

    // Skip "sub" subtree — nested.txt must not be visited.
    {
        w2_ctx c = {};
        strcpy(c.dir_to_skip, "sub");
        call(WALKTreeLazy, root_sha.data, w2_visit, &c);
        want(c.n_entries == 4);   // root, hello.txt, run.sh, sub (skipped)
        want(c.n_files == 2);
        want(c.n_dirs == 2);
    }

    call(KEEPClose);
    HOMEClose();

    {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", tmp);
        system(cmd);
    }

    done;
}


// ---- Test 4: KEEPTreeULog round-trip via ULOGu8ssDrainHeap ----
//
// Same tree shape as WALKtest3.  Verifies:
//   * KEEPTreeULog emits 3 well-formed ULOG rows in lex-by-path order
//   * each row carries the right (mode, hex-sha) pair in (?query, #fragment)
//   * heap-drained back through ULOGu8ssDrainHeap with ULOGu8csZbyUri
//     yields the same 3 records in the same order

ok64 WALKtest4() {
    sane(1);
    call(FILEInit);

    char tmp[] = "/tmp/walktest4-XXXXXX";
    want(mkdtemp(tmp) != NULL);
    a_cstr(root, tmp);
    home h = {};
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);

    keep_pack p = {};
    call(KEEPPackOpen, &p);
    p.strict_order = NO;

    a_cstr(hi_content, "hi\n");
    sha1 hi_sha = {};
    call(KEEPPackFeed, &p, DOG_OBJ_BLOB, hi_content, 0,
         &hi_sha);

    a_cstr(run_content, "#!/bin/sh\n");
    sha1 run_sha = {};
    call(KEEPPackFeed, &p, DOG_OBJ_BLOB, run_content, 0,
         &run_sha);

    a_cstr(nested_mn, "100644 nested.txt");
    a_cstr(nested_content, "deep\n");
    sha1 sub_sha = {};
    call(build_leaf_tree, &KEEP, &p, nested_mn, nested_content, &sub_sha);

    sha1 nested_blob_sha = {};
    call(KEEPPackFeed, &p, DOG_OBJ_BLOB, nested_content, 0,
         &nested_blob_sha);

    a_pad(u8, rtb, 512);
    a_cstr(e1, "100644 hello.txt"); call(u8bFeed, rtb, e1);
    u8bFeed1(rtb, 0); a_rawc(hi_ss, hi_sha); call(u8bFeed, rtb, hi_ss);
    a_cstr(e2, "100755 run.sh"); call(u8bFeed, rtb, e2);
    u8bFeed1(rtb, 0); a_rawc(run_ss, run_sha); call(u8bFeed, rtb, run_ss);
    a_cstr(e3, "40000 sub"); call(u8bFeed, rtb, e3);
    u8bFeed1(rtb, 0); a_rawc(sub_ss, sub_sha); call(u8bFeed, rtb, sub_ss);

    a_dup(u8c, rtc, u8bData(rtb));
    sha1 root_sha = {};
    call(KEEPPackFeed, &p, DOG_OBJ_TREE, rtc, 0,
         &root_sha);

    call(KEEPPackClose, &p);

    Bu8 ulog_buf = {};
    call(u8bAllocate, ulog_buf, 1UL << 16);

    a_cstr(verb_name_s, "leaf");
    a_dup(u8c, verb_dup, verb_name_s);
    ron60 verb = 0;
    call(RONutf8sDrain, &verb, verb_dup);

    call(KEEPTreeULog, root_sha.data, 0, verb, ulog_buf);

    //  Drain back via the heap with one cursor.
    a_dup(u8c, view, u8bData(ulog_buf));
    a_pad(u8cs, ins, 1);
    u8cssFeed1(ins_idle, view);
    a_dup(u8cs, cursors, u8csbData(ins));
    u8cssHeapZ(cursors, ULOGu8csZbyUri);

    char const *expect_path[3] = {"hello.txt", "run.sh", "sub/nested.txt"};
    u8          expect_kind[3] = {RON_f,       RON_x,    RON_f};
    sha1 expect_sha[3] = {hi_sha, run_sha, nested_blob_sha};

    for (u32 i = 0; i < 3; i++) {
        ulogrec g = {};
        call(ULOGu8ssDrainHeap, cursors, ULOGu8csZbyUri, &g);
        want(ok64stem(g.verb) == verb);
        want(ok64Lit(g.verb, 0) == expect_kind[i]);

        size_t pl = strlen(expect_path[i]);
        want((size_t)u8csLen(g.uri.path) == pl);
        want(memcmp(g.uri.path[0], expect_path[i], pl) == 0);

        //  Mode no longer in query — query is empty for tree rows.
        want(u8csLen(g.uri.query) == 0);

        //  Decode the 40-hex fragment back to 20 raw bytes and compare.
        want(u8csLen(g.uri.fragment) == 40);
        sha1 got = {};
        a_dup(u8c, frag, g.uri.fragment);
        call(sha1FromHex, &got, frag);
        want(sha1Eq(&got, &expect_sha[i]));
    }

    //  Heap exhausted.
    {
        ulogrec g = {};
        want(ULOGu8ssDrainHeap(cursors, ULOGu8csZbyUri, &g) == ULOGNONE);
    }

    u8bFree(ulog_buf);

    call(KEEPClose);
    HOMEClose();
    {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", tmp);
        system(cmd);
    }
    done;
}

// ---- Test 5: KEEPTreeDescend over the WALKtest2 tree (CODE-005) ----
//
//  Same tree shape (hello.txt, run.sh, sub/nested.txt).  Exercises the
//  shared '/'-separated descent: empty → root DIR, leaf file, subtree
//  dir, nested file, leading/trailing '/', and not-found paths (with
//  the caller-chosen `notfound` code propagating).

ok64 WALKtest5() {
    sane(1);
    call(FILEInit);

    char tmp[] = "/tmp/walktest5-XXXXXX";
    want(mkdtemp(tmp) != NULL);
    a_cstr(root, tmp);
    home h = {};
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);

    keep_pack p = {};
    call(KEEPPackOpen, &p);
    p.strict_order = NO;

    a_cstr(hi_content, "hi\n");
    sha1 hi_sha = {};
    call(KEEPPackFeed, &p, DOG_OBJ_BLOB, hi_content, 0, &hi_sha);
    a_cstr(run_content, "#!/bin/sh\n");
    sha1 run_sha = {};
    call(KEEPPackFeed, &p, DOG_OBJ_BLOB, run_content, 0, &run_sha);
    a_cstr(nested_mn, "100644 nested.txt");
    a_cstr(nested_content, "deep\n");
    sha1 sub_sha = {};
    call(build_leaf_tree, &KEEP, &p, nested_mn, nested_content, &sub_sha);

    a_pad(u8, rtb, 512);
    a_cstr(e1, "100644 hello.txt"); call(u8bFeed, rtb, e1);
    u8bFeed1(rtb, 0); a_rawc(hi_ss, hi_sha); call(u8bFeed, rtb, hi_ss);
    a_cstr(e2, "100755 run.sh"); call(u8bFeed, rtb, e2);
    u8bFeed1(rtb, 0); a_rawc(run_ss, run_sha); call(u8bFeed, rtb, run_ss);
    a_cstr(e3, "40000 sub"); call(u8bFeed, rtb, e3);
    u8bFeed1(rtb, 0); a_rawc(sub_ss, sub_sha); call(u8bFeed, rtb, sub_ss);
    a_dup(u8c, rtc, u8bData(rtb));
    sha1 root_sha = {};
    call(KEEPPackFeed, &p, DOG_OBJ_TREE, rtc, 0, &root_sha);
    call(KEEPPackClose, &p);

    a_carve(u8, tbuf, 1UL << 20);

    typedef struct {
        char const *path;
        ok64        want_rc;   // OK or the notfound code
        u8          want_kind; // when OK
        char const *want_prefix;
    } row;
    row cases[] = {
        {"",               OK, WALK_KIND_DIR, ""},
        {"hello.txt",      OK, WALK_KIND_REG, "hello.txt"},
        {"run.sh",         OK, WALK_KIND_EXE, "run.sh"},
        {"sub",            OK, WALK_KIND_DIR, "sub"},
        {"sub/nested.txt", OK, WALK_KIND_REG, "sub/nested.txt"},
        {"/sub/nested.txt",OK, WALK_KIND_REG, "sub/nested.txt"}, // leading /
        {"sub/",           OK, WALK_KIND_DIR, "sub"},            // trailing /
        {"nope",           WALKNONE, 0, ""},                     // missing entry
        {"hello.txt/x",    WALKNONE, 0, ""},                     // descend into file
        {"sub/missing",    WALKNONE, 0, ""},
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        u8cs sp = {(u8cp)cases[i].path,
                   (u8cp)cases[i].path + strlen(cases[i].path)};
        a_pad(u8, pbuf, 1024);
        sha1 osha = {};
        u8 okind = 0;
        ok64 rc = KEEPTreeDescend(&root_sha, sp, pbuf, &osha, &okind,
                                  tbuf, WALKNONE);
        if (rc != cases[i].want_rc) {
            fprintf(stderr, "descend[%zu] '%s': rc %s want %s\n",
                    i, cases[i].path, ok64str(rc), ok64str(cases[i].want_rc));
            fail(TESTFAIL);
        }
        if (rc == OK) {
            want(okind == cases[i].want_kind);
            size_t pl = strlen(cases[i].want_prefix);
            want((size_t)u8bDataLen(pbuf) == pl);
            want(memcmp(u8bDataHead(pbuf), cases[i].want_prefix, pl) == 0);
        }
    }

    //  NULL pathbuf: descent still resolves, just skips the accounting.
    {
        a_cstr(pp, "sub/nested.txt");
        u8cs sp = {pp[0], pp[1]};
        sha1 osha = {}; u8 okind = 0;
        call(KEEPTreeDescend, &root_sha, sp, NULL, &osha, &okind,
             tbuf, WALKNONE);
        want(okind == WALK_KIND_REG);
    }

    call(KEEPClose);
    HOMEClose();
    {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", tmp);
        system(cmd);
    }
    done;
}

// ---- Test 6: wide-tree walk must not leak BASS (GET-020) ----
//
//  walk_tree_dive recurses once per directory, each level carving a
//  1 MiB `tbuf` off ABC_BASS.  Pre-fix the recursion was a bare C call,
//  so a parent's child carves were never rewound: every sibling
//  subtree's `tbuf` piled up across the whole walk.  A tree wider than
//  ~1024 directories therefore exhausted the 1 GiB arena and the next
//  `a_carve(tbuf)` returned BNOROOM MID-WALK — a clean tree-order
//  truncation (`be get file:<wt>` checked out only the first ~474 of
//  the tree's files and exited SNIFFFAIL).  The fix wraps each subtree
//  recursion in `try()` (PRO.h), which rewinds BASS per sibling so only
//  the current root-to-leaf chain stays live.
//
//  This builds a root tree with WIDE_N single-file sibling subdirs
//  (depth 2, so the live chain is tiny) and asserts the walk visits
//  EVERY directory + file and returns OK.  Pre-fix: BNOROOM after ~1000
//  dirs, a truncated visit count.  WIDE_N is set well past the 1 GiB /
//  1 MiB ≈ 1024-dir cliff.

#define WALK6_WIDE_N 1300

typedef struct {
    u32 n_dirs;
    u32 n_files;
} w6_ctx;

static ok64 w6_visit(u8cs path, u8 kind, u8cp esha, u8cs blob,
                     void0p vctx) {
    (void)path; (void)esha; (void)blob;
    w6_ctx *c = (w6_ctx *)vctx;
    if (kind == WALK_KIND_DIR) c->n_dirs++;
    else if (kind == WALK_KIND_REG || kind == WALK_KIND_EXE ||
             kind == WALK_KIND_LNK) c->n_files++;
    return OK;
}

ok64 WALKtest6() {
    sane(1);
    call(FILEInit);

    char tmp[] = "/tmp/walktest6-XXXXXX";
    want(mkdtemp(tmp) != NULL);
    a_cstr(root, tmp);
    home h = {};
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);

    keep_pack p = {};
    call(KEEPPackOpen, &p);
    p.strict_order = NO;

    //  One shared leaf blob, reused by every sibling's file entry.
    a_cstr(leaf_content, "leaf\n");
    sha1 leaf_sha = {};
    call(KEEPPackFeed, &p, DOG_OBJ_BLOB, leaf_content, 0, &leaf_sha);

    //  Build WIDE_N sibling subtrees, each `dXXXX/leaf.txt`, and feed
    //  their (mode-name, sha) entries into the root tree buffer.  Names
    //  are zero-padded so they stay in git's lex sort order.  4 MiB root
    //  tree buffer is plenty for ~1300 28-byte entries.
    Bu8 rtb = {};
    call(u8bMap, rtb, 1UL << 22);
    sha1 *subs = (sha1 *)malloc(sizeof(sha1) * WALK6_WIDE_N);
    want(subs != NULL);
    for (u32 i = 0; i < WALK6_WIDE_N; i++) {
        char mn[64];
        snprintf(mn, sizeof(mn), "100644 leaf.txt");
        u8cs mn_s = {(u8cp)mn, (u8cp)mn + strlen(mn)};
        try(build_leaf_tree, &KEEP, &p, mn_s, leaf_content, &subs[i]);
        nedo { free(subs); fail(__); }

        char dn[64];
        snprintf(dn, sizeof(dn), "40000 d%04u", i);
        u8cs dn_s = {(u8cp)dn, (u8cp)dn + strlen(dn)};
        try(u8bFeed, rtb, dn_s);          nedo { free(subs); fail(__); }
        u8bFeed1(rtb, 0);
        a_rawc(ss, subs[i]);
        try(u8bFeed, rtb, ss);            nedo { free(subs); fail(__); }
    }
    free(subs);

    a_dup(u8c, rtc, u8bData(rtb));
    sha1 root_sha = {};
    call(KEEPPackFeed, &p, DOG_OBJ_TREE, rtc, 0, &root_sha);
    call(KEEPPackClose, &p);
    u8bUnMap(rtb);

    //  Lazy walk (the `be get` checkout path).  Must visit every dir +
    //  file and finish OK — pre-fix it returned BNOROOM partway.
    {
        w6_ctx c = {};
        call(WALKTreeLazy, root_sha.data, w6_visit, &c);
        //  Root DIR + WIDE_N child dirs.
        want(c.n_dirs == 1 + WALK6_WIDE_N);
        want(c.n_files == WALK6_WIDE_N);
    }

    call(KEEPClose);
    HOMEClose();
    {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", tmp);
        system(cmd);
    }
    done;
}

// ---- Test 7: deep tree terminates cleanly, cap holds (KEEP-001) ----
//
//  Build a linear chain of nested single-subdir trees and assert
//  KEEPWalkTree terminates with a CLEAN error (no crash / C-stack
//  overflow) and a bounded visit count.  WALK_MAX_DEPTH (4096) is the
//  hard C-stack backstop; in practice the per-walk pathbuf (every level
//  appends "<name>/") binds first, so a genuinely deep tree returns
//  WALKNOROOM well before the cap.  Either is a clean truncation — the
//  point is no undefined behaviour.  A shallow chain walks fully OK.

typedef struct { u32 n_dirs; } w7_ctx;

static ok64 w7_visit(u8cs path, u8 kind, u8cp esha, u8cs blob,
                     void0p vctx) {
    (void)path; (void)esha; (void)blob;
    w7_ctx *c = (w7_ctx *)vctx;
    if (kind == WALK_KIND_DIR) c->n_dirs++;
    return OK;
}

//  Build `depth` nested trees: each holds one subdir `d` pointing at the
//  next level down; the deepest holds a single leaf file.  Returns the
//  top tree's sha.
static ok64 build_deep_chain(keeper *k, keep_pack *p, u32 depth,
                             sha1 *top_out) {
    sane(k && p && top_out);
    a_cstr(leaf_mn, "100644 leaf.txt");
    a_cstr(leaf_content, "x\n");
    sha1 cur = {};
    call(build_leaf_tree, k, p, leaf_mn, leaf_content, &cur);
    for (u32 i = 0; i < depth; i++) {
        a_pad(u8, tb, 64);
        a_cstr(dn, "40000 d");
        call(u8bFeed, tb, dn);
        u8bFeed1(tb, 0);
        a_rawc(ss, cur);
        call(u8bFeed, tb, ss);
        a_dup(u8c, tc, u8bData(tb));
        sha1 nx = {};
        call(KEEPPackFeed, p, DOG_OBJ_TREE, tc, 0, &nx);
        cur = nx;
    }
    *top_out = cur;
    done;
}

ok64 WALKtest7() {
    sane(1);
    call(FILEInit);

    char tmp[] = "/tmp/walktest7-XXXXXX";
    want(mkdtemp(tmp) != NULL);
    a_cstr(root, tmp);
    home h = {};
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);

    keep_pack p = {};
    call(KEEPPackOpen, &p);
    p.strict_order = NO;

    //  Over the cap: WALK_MAX_DEPTH+50 nested dirs → WALKBADFMT, no crash.
    sha1 deep_top = {};
    call(build_deep_chain, &KEEP, &p, WALK_MAX_DEPTH + 50, &deep_top);
    //  Under the cap: 100 nested dirs walk to the bottom cleanly.
    sha1 shallow_top = {};
    call(build_deep_chain, &KEEP, &p, 100, &shallow_top);
    call(KEEPPackClose, &p);

    {
        w7_ctx c = {};
        ok64 rc = KEEPWalkTree(deep_top.data, NO, w7_visit, &c);
        //  Clean truncation (WALKNOROOM/WALKBADFMT), never OK, no crash.
        want(rc != OK);
        want(rc == WALKNOROOM || rc == WALKBADFMT);
        //  Bounded: did not recurse unbounded into the C stack.
        want(c.n_dirs <= WALK_MAX_DEPTH + 1);
    }
    {
        w7_ctx c = {};
        call(KEEPWalkTree, shallow_top.data, NO, w7_visit, &c);
        //  Root DIR + 100 nested DIRs all visited.
        want(c.n_dirs == 1 + 100);
    }

    call(KEEPClose);
    HOMEClose();
    {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", tmp);
        system(cmd);
    }
    done;
}

ok64 maintest() {
    sane(1);
    call(WALKtest1);
    call(WALKtest2);
    call(WALKtest4);
    call(WALKtest5);
    call(WALKtest6);
    call(WALKtest7);
    done;
}

TEST(maintest)
