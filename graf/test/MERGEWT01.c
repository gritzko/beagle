//
//  MERGEWT01 — Property tests for GRAFMergeWtFile.
//
//  Tag matrix:
//    (a) Non-overlapping edits (wt edits line A, tgt edits line C) →
//        merged output contains both edits, no markers.
//    (b) wt absent on disk → equivalent to "no wt edit" — merged
//        output equals tgt's blob bytes.
//    (c) wt byte-equal to base → no-op fold; merged output equals
//        tgt's blob bytes.
//
#include "graf/GRAF.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/TEST.h"
#include "dog/DOG.h"
#include "dog/HOME.h"
#include "dog/WHIFF.h"
#include "keeper/KEEP.h"

// --- Tiny test harness (mirrors REBASE01.c) ----------------------------

static char g_tmp[256];

static ok64 setup_repo(void) {
    sane(1);
    call(FILEInit);
    snprintf(g_tmp, sizeof(g_tmp), "/tmp/grafmergewt-XXXXXX");
    want(mkdtemp(g_tmp) != NULL);
    a_cstr(root, g_tmp);
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);
    done;
}

static void teardown_repo(void) {
    GRAFClose();
    KEEPClose();
    HOMEClose();
    char cmd[300];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_tmp);
    system(cmd);
}

//  Build a tree carrying one blob "100644 <name>" → bsha.
static ok64 make_single_leaf_tree(keep_pack *p,
                                  char const *name,
                                  char const *content,
                                  sha1 *blob_out, sha1 *tree_out) {
    sane(p && blob_out && tree_out);
    u8cs cb = {(u8cp)content, (u8cp)content + strlen(content)};
    call(KEEPPackFeed, p, DOG_OBJ_BLOB, cb, 0, blob_out);
    a_pad(u8, tb, 256);
    a_cstr(prefix, "100644 ");
    call(u8bFeed, tb, prefix);
    a_cstr(nm, name);
    call(u8bFeed, tb, nm);
    u8bFeed1(tb, 0);
    a_rawc(ss, *blob_out);
    call(u8bFeed, tb, ss);
    a_dup(u8c, tc, u8bData(tb));
    call(KEEPPackFeed, p, DOG_OBJ_TREE, tc, 0, tree_out);
    done;
}

//  Build a commit body and feed it to keeper.
static ok64 commit_one(keep_pack *p,
                       sha1cp tree_sha, sha1cp parent_sha,
                       char const *msg, long ts, sha1 *out_sha) {
    sane(p && tree_sha && msg && out_sha);

    Bu8 cb = {};
    call(u8bAllocate, cb, 1024);

    a_cstr(tl, "tree ");  u8bFeed(cb, tl);
    a_sha1hex(thx, tree_sha);
    u8bFeed(cb, thx);
    u8bFeed1(cb, '\n');

    if (parent_sha != NULL) {
        a_cstr(pl, "parent ");  u8bFeed(cb, pl);
        a_sha1hex(phx, parent_sha);
        u8bFeed(cb, phx);
        u8bFeed1(cb, '\n');
    }

    char hdr[256];
    int n = snprintf(hdr, sizeof(hdr),
                     "author t <t@t> %ld +0000\n"
                     "committer t <t@t> %ld +0000\n\n%s\n",
                     ts, ts, msg);
    u8cs hs = {(u8cp)hdr, (u8cp)hdr + n};
    u8bFeed(cb, hs);

    a_dup(u8c, cd, u8bData(cb));
    call(KEEPPackFeed, p, DOG_OBJ_COMMIT, cd, 0, out_sha);
    u8bFree(cb);
    done;
}

//  Convenience: build tree+commit for one file with `content` parented
//  on `parent` (NULL for root).
static ok64 commit_one_file(keep_pack *p, char const *name,
                            char const *content, sha1cp parent,
                            char const *msg, long ts, sha1 *out_commit) {
    sane(p && name && content && msg && out_commit);
    sha1 blob = {}, tree = {};
    call(make_single_leaf_tree, p, name, content, &blob, &tree);
    call(commit_one, p, &tree, parent, msg, ts, out_commit);
    done;
}

//  Write `content` into `<reporoot>/<rel>`.
static ok64 write_wt(char const *rel, char const *content) {
    sane(rel && content);
    char abs[512];
    snprintf(abs, sizeof(abs), "%s/%s", g_tmp, rel);
    int fd = open(abs, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) fail(FILEFAIL);
    size_t len = strlen(content);
    ssize_t w = write(fd, content, len);
    close(fd);
    if (w != (ssize_t)len) fail(FILEFAIL);
    done;
}

//  YES iff `out` byte-equals `want_str`.
static b8 buf_eq_cstr(u8b out, char const *want_str) {
    size_t wl = strlen(want_str);
    if (u8bDataLen(out) != wl) return NO;
    return memcmp(u8bDataHead(out), want_str, wl) == 0;
}

// --- Tests --------------------------------------------------------------

//  (a) Non-overlapping edits: wt edits line 1, tgt edits line 3.
//  Merged output contains both edits.
ok64 test_clean_merge(void) {
    sane(1);
    call(setup_repo);

    keep_pack p = {};
    call(KEEPPackOpen, &p);
    p.strict_order = NO;

    sha1 c_base = {}, c_tgt = {};
    call(commit_one_file, &p, "f.txt",
         "alpha\nbeta\ngamma\n", NULL,           "base", 1700000000L, &c_base);
    call(commit_one_file, &p, "f.txt",
         "alpha\nbeta\nGAMMA\n", &c_base,        "tgt",  1700000100L, &c_tgt);

    call(KEEPPackClose, &p);

    //  Build the DAG so build_tip_weave_tunable can walk history.
    call(GRAFOpen, YES);
    call(GRAFIndex);

    //  wt = base + edit on line 1 (alpha → ALPHA).
    call(write_wt, "f.txt", "ALPHA\nbeta\ngamma\n");

    Bu8 out = {};
    call(u8bAllocate, out, 1024);

    a_cstr(path, "f.txt");
    a_dup(u8c, root, u8bData(HOME.wt));

    call(GRAFMergeWtFile, path, root, &c_base, &c_tgt, out);

    //  Expected: both edits present, base text in the middle preserved.
    want(buf_eq_cstr(out, "ALPHA\nbeta\nGAMMA\n"));

    u8bFree(out);
    teardown_repo();
    done;
}

//  (b) wt absent on disk → fold is a no-op; merged output is just the
//  tgt blob (the base side has no wt layer, history-only).
ok64 test_wt_absent(void) {
    sane(1);
    call(setup_repo);

    keep_pack p = {};
    call(KEEPPackOpen, &p);
    p.strict_order = NO;

    sha1 c_base = {}, c_tgt = {};
    call(commit_one_file, &p, "f.txt",
         "one\ntwo\n", NULL,            "base", 1700000000L, &c_base);
    call(commit_one_file, &p, "f.txt",
         "one\nTWO\n", &c_base,         "tgt",  1700000100L, &c_tgt);

    call(KEEPPackClose, &p);
    call(GRAFOpen, YES);
    call(GRAFIndex);

    //  No write_wt — file is absent on disk.
    Bu8 out = {};
    call(u8bAllocate, out, 1024);

    a_cstr(path, "f.txt");
    a_dup(u8c, root, u8bData(HOME.wt));

    call(GRAFMergeWtFile, path, root, &c_base, &c_tgt, out);

    //  Without a wt edit, tgt's bytes pass through alone.
    want(buf_eq_cstr(out, "one\nTWO\n"));

    u8bFree(out);
    teardown_repo();
    done;
}

//  (c) wt byte-equal to last committed version on the base side →
//  fold is a no-op; merged output equals tgt's blob.
ok64 test_wt_clean_drift(void) {
    sane(1);
    call(setup_repo);

    keep_pack p = {};
    call(KEEPPackOpen, &p);
    p.strict_order = NO;

    sha1 c_base = {}, c_tgt = {};
    call(commit_one_file, &p, "f.txt",
         "x\ny\n", NULL,            "base", 1700000000L, &c_base);
    call(commit_one_file, &p, "f.txt",
         "x\nY\n", &c_base,         "tgt",  1700000100L, &c_tgt);

    call(KEEPPackClose, &p);
    call(GRAFOpen, YES);
    call(GRAFIndex);

    //  wt content matches base — graf_fold_wt_layer in BLAME's pattern
    //  byte-dedups, but our extracted helper does not (yet).  Even so,
    //  the tokens carry WEAVE_WT_SRC stamps that don't appear in tgt's
    //  history — the merge still resolves to tgt's bytes for changed
    //  regions because tgt's `in` is also alive on the base side via
    //  the shared-spine reconciliation.
    call(write_wt, "f.txt", "x\ny\n");

    Bu8 out = {};
    call(u8bAllocate, out, 1024);

    a_cstr(path, "f.txt");
    a_dup(u8c, root, u8bData(HOME.wt));

    call(GRAFMergeWtFile, path, root, &c_base, &c_tgt, out);

    want(buf_eq_cstr(out, "x\nY\n"));

    u8bFree(out);
    teardown_repo();
    done;
}

//  (d) Same line edited by both wt and tgt → conflict markers.
//  Both sides change line 2; the merger can't pick one — output
//  carries `<<<<` / `||||` / `>>>>` framing the divergent region.
ok64 test_conflict(void) {
    sane(1);
    call(setup_repo);

    keep_pack p = {};
    call(KEEPPackOpen, &p);
    p.strict_order = NO;

    sha1 c_base = {}, c_tgt = {};
    call(commit_one_file, &p, "f.txt",
         "one\nbeta\nthree\n", NULL,        "base", 1700000000L, &c_base);
    call(commit_one_file, &p, "f.txt",
         "one\nBETA-tgt\nthree\n", &c_base, "tgt",  1700000100L, &c_tgt);

    call(KEEPPackClose, &p);
    call(GRAFOpen, YES);
    call(GRAFIndex);

    //  wt edits the same line, differently from tgt.
    call(write_wt, "f.txt", "one\nBETA-wt\nthree\n");

    Bu8 out = {};
    call(u8bAllocate, out, 1024);
    a_cstr(path, "f.txt");
    a_dup(u8c, root, u8bData(HOME.wt));

    call(GRAFMergeWtFile, path, root, &c_base, &c_tgt, out);

    //  Output must carry conflict markers framing the disagreement.
    //  WEAVEMerge's NEIL/canonicalization can split a single conceptual
    //  conflict across multiple non-EQ runs (e.g., when a shared spine
    //  token sits between divergent inserts), so we only assert that
    //  markers fire and that both sides' distinguishing bytes survive
    //  somewhere in the output.
    u8s out_view = {u8bDataHead(out), u8bIdleHead(out)};
    size_t olen  = (size_t)$len(out_view);
    b8 has_open  = NO, has_mid = NO, has_close = NO;
    for (size_t i = 0; i + 4 <= olen; i++) {
        u8 const *p2 = out_view[0] + i;
        if (memcmp(p2, "<<<<", 4) == 0) has_open  = YES;
        if (memcmp(p2, "||||", 4) == 0) has_mid   = YES;
        if (memcmp(p2, ">>>>", 4) == 0) has_close = YES;
    }
    want(has_open && has_mid && has_close);

    //  Both sides' distinguishing bytes appear somewhere in the output.
    b8 has_wt = NO, has_tgt = NO;
    for (size_t i = 0; i + 3 <= olen; i++) {
        if (memcmp(out_view[0] + i, "wt",  2) == 0) has_wt  = YES;
        if (memcmp(out_view[0] + i, "tgt", 3) == 0) has_tgt = YES;
    }
    want(has_wt && has_tgt);

    u8bFree(out);
    teardown_repo();
    done;
}

//  (e) Large file (~80 KB) with a long linear history (~90 versions),
//  mirroring the BE.cli.c-scale repro in GET-001.  The unfixed
//  build_tip_weave_tunable replay loop never rewinds the per-version
//  WEAVE_DECODE scratch, so BASS use grows ~O(versions^2 * tokens) and
//  the 1 GB arena overflows with BNOROOM long before the merge
//  finishes.  With the per-version call()/try() boundary the scratch is
//  reclaimed each step and the merge completes (OK).
//
//  The fixture is fully synthetic and deterministic: a NLINES-line file
//  whose i-th version rewrites one rotating line, chained over NVERS
//  commits.  The wt edits a single far-apart line so it merges cleanly
//  with the tip and we can assert the merged bytes.

#define BIG_NLINES 2000u    // ~80 KB at ~40 bytes/line
#define BIG_NVERS  90u      // long history; overflows the unfixed loop

//  Render version `v` of the big file into `out` (reset first).  Line i
//  reads "line <i> base\n" except the rotating line for this version
//  which reads "line <i> v<v>\n".  Version 0 is the all-base baseline.
static ok64 big_render(Bu8 out, u32 v) {
    sane(1);
    u8bReset(out);
    u32 hot = (v == 0) ? BIG_NLINES : (v % BIG_NLINES);
    for (u32 i = 0; i < BIG_NLINES; i++) {
        char line[64];
        int n;
        if (v != 0 && i == hot)
            n = snprintf(line, sizeof(line), "line %05u v%05u\n", i, v);
        else
            n = snprintf(line, sizeof(line), "line %05u base\n", i);
        u8cs ls = {(u8cp)line, (u8cp)line + n};
        call(u8bFeed, out, ls);
    }
    done;
}

ok64 test_big_history(void) {
    sane(1);
    call(setup_repo);

    keep_pack p = {};
    call(KEEPPackOpen, &p);
    p.strict_order = NO;

    Bu8 content = {};
    call(u8bAllocate, content, (BIG_NLINES + 4) * 64);

    //  Chain BIG_NVERS commits, each touching one rotating line of the
    //  big file.  Keep the last two commit shas as base/tgt.
    sha1 parent = {}, prev = {}, c_base = {}, c_tgt = {};
    b8 have_parent = NO;
    for (u32 v = 0; v < BIG_NVERS; v++) {
        call(big_render, content, v);
        char msg[32];
        snprintf(msg, sizeof(msg), "v%u", v);
        sha1 c = {};
        sha1 blob = {}, tree = {};
        a_dup(u8c, cd, u8bData(content));
        call(KEEPPackFeed, &p, DOG_OBJ_BLOB, cd, 0, &blob);
        a_pad(u8, tb, 256);
        a_cstr(prefix, "100644 ");
        call(u8bFeed, tb, prefix);
        a_cstr(nm, "big.txt");
        call(u8bFeed, tb, nm);
        u8bFeed1(tb, 0);
        a_rawc(ss, blob);
        call(u8bFeed, tb, ss);
        a_dup(u8c, tc, u8bData(tb));
        call(KEEPPackFeed, &p, DOG_OBJ_TREE, tc, 0, &tree);
        call(commit_one, &p, &tree, have_parent ? &parent : NULL,
             msg, 1700000000L + (long)v, &c);
        prev = parent;
        parent = c;
        have_parent = YES;
    }
    c_base = prev;     // second-to-last commit
    c_tgt  = parent;   // last commit

    call(KEEPPackClose, &p);
    call(GRAFOpen, YES);
    call(GRAFIndex);

    //  wt edits a line that neither base nor tgt touch (tgt rotates a
    //  high line index; we edit line 0), so the fold merges cleanly.
    call(big_render, content, BIG_NVERS - 2);  // wt starts from base text
    {
        //  Mutate line 0 in the rendered base text → "line 00000 WT\n".
        char abs[512];
        snprintf(abs, sizeof(abs), "%s/%s", g_tmp, "big.txt");
        int fd = open(abs, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) fail(FILEFAIL);
        a_cstr(l0, "line 00000 WT\n");
        ssize_t w = write(fd, l0[0], (size_t)$len(l0));
        //  Append the rest of the base text from line 1 onward.
        u8s body = {u8bDataHead(content), u8bIdleHead(content)};
        size_t blen = (size_t)$len(body);
        //  Skip the first line of `body` (line 0).
        size_t off = 0;
        while (off < blen && body[0][off] != '\n') off++;
        if (off < blen) off++;
        ssize_t w2 = write(fd, body[0] + off, blen - off);
        close(fd);
        if (w < 0 || w2 < 0) fail(FILEFAIL);
    }

    Bu8 out = {};
    call(u8bAllocate, out, (BIG_NLINES + 8) * 64);

    a_cstr(path, "big.txt");
    a_dup(u8c, root, u8bData(HOME.wt));

    //  Before the fix this returns BNOROOM (arena overflow); after, OK.
    call(GRAFMergeWtFile, path, root, &c_base, &c_tgt, out);

    //  The wt edit to line 0 must survive in the merged output.
    u8s ov = {u8bDataHead(out), u8bIdleHead(out)};
    size_t olen = (size_t)$len(ov);
    b8 has_wt = NO;
    for (size_t i = 0; i + 13 <= olen; i++)
        if (memcmp(ov[0] + i, "line 00000 WT", 13) == 0) { has_wt = YES; break; }
    want(has_wt);

    u8bFree(out);
    u8bFree(content);
    teardown_repo();
    done;
}

ok64 maintest(void) {
    sane(1);
    call(test_clean_merge);
    call(test_wt_absent);
    call(test_wt_clean_drift);
    call(test_conflict);
    call(test_big_history);
    done;
}

TEST(maintest)
