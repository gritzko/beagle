//
//  REBASE01 — Property tests for the rebase primitives.
//
//  Tag matrix (matches the spec):
//    (a) PatchId same-body-twice           → equal ids
//    (b) PatchId different diffs           → different ids
//    (c) PatchId same diff, diff parent    → equal ids (rebase invariance)
//    (d) PatchId empty diff (= parent)     → 0
//    (e) MergeExplicit base==ours          → output = theirs
//    (f) MergeExplicit base==theirs        → output = ours
//    (g) MergeExplicit non-conflicting     → merged contains both edits
//    (h) MergeExplicit conflicting         → output has '<<<<' markers
//    (i) Rebase child_tip == base_old      → no emits, head = base_new
//    (j) Rebase single new commit          → 2 emits (tree, commit)
//    (k) Rebase patch-id collision         → second commit skipped
//    (l) Rebase conflict                   → GRAFCNFL, no emits past it
//
#include "graf/REBASE.h"

#include "graf/GRAF.h"
#include "graf/JOIN.h"

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

// --- Tiny test harness ----------------------------------------------------

static char g_tmp[256];

static ok64 setup_repo(void) {
    sane(1);
    call(FILEInit);
    snprintf(g_tmp, sizeof(g_tmp), "/tmp/grafrebase-XXXXXX");
    want(mkdtemp(g_tmp) != NULL);
    a_cstr(root, g_tmp);
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);
    call(GRAFOpen, YES);
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

//  Build a tiny single-leaf tree carrying one blob "<mode> <name>" → bsha.
static ok64 make_single_leaf_tree(keep_pack *p,
                                  char const *mode_name,
                                  char const *content,
                                  sha1 *blob_out, sha1 *tree_out) {
    sane(p && blob_out && tree_out);
    u8cs cb = {(u8cp)content, (u8cp)content + strlen(content)};
    call(KEEPPackFeed, p, DOG_OBJ_BLOB, cb, 0, blob_out);
    a_pad(u8, tb, 256);
    a_cstr(mn, mode_name);
    call(u8bFeed, tb, mn);
    u8bFeed1(tb, 0);
    a_rawc(ss, *blob_out);
    call(u8bFeed, tb, ss);
    a_dup(u8c, tc, u8bData(tb));
    call(KEEPPackFeed, p, DOG_OBJ_TREE, tc, 0, tree_out);
    done;
}

//  Build a commit body: "tree <hex>\n[parent <hex>\n]author ...\ncommitter ...\n\nmsg\n"
static void make_commit_body(u8 *const *out,
                             sha1cp tree_sha,
                             sha1cp parent_sha,        //  NULL for root
                             char const *author_id,
                             long ts,
                             char const *msg) {
    a_cstr(tl, "tree ");
    u8bFeed(out, tl);
    a_sha1hex(thx, tree_sha);
    u8bFeed(out, thx);
    u8bFeed1(out, '\n');

    if (parent_sha != NULL) {
        a_cstr(pl, "parent ");
        u8bFeed(out, pl);
        a_sha1hex(phx, parent_sha);
        u8bFeed(out, phx);
        u8bFeed1(out, '\n');
    }

    char hdr[256];
    int n = snprintf(hdr, sizeof(hdr),
                     "author %s %ld +0000\ncommitter %s %ld +0000\n\n%s\n",
                     author_id, ts, author_id, ts, msg);
    u8cs hs = {(u8cp)hdr, (u8cp)hdr + n};
    u8bFeed(out, hs);
}

//  Feed a synthetic commit to keeper with a given (tree, parent) pair,
//  return its sha.  Borrows `p` already opened by caller.
static ok64 feed_commit(keep_pack *p,
                        sha1cp tree_sha, sha1cp parent_sha,
                        char const *author_id, long ts, char const *msg,
                        sha1 *out_sha) {
    sane(p && out_sha);
    Bu8 cb = {};
    call(u8bAllocate, cb, 4096);
    make_commit_body(cb, tree_sha, parent_sha, author_id, ts, msg);
    a_dup(u8c, cd, u8bData(cb));
    call(KEEPPackFeed, p, DOG_OBJ_COMMIT, cd, 0, out_sha);
    u8bFree(cb);
    done;
}

//  Fetch a commit body into `out`.
static ok64 fetch_commit(sha1cp sha, u8 *const *out) {
    sane(out);
    u8 t = 0;
    call(KEEPGetExact, sha, out, &t);
    if (t != DOG_OBJ_COMMIT) fail(KEEPFAIL);
    done;
}

// --- Emit-callback recorder ----------------------------------------------

#define EMIT_CAP 32
typedef struct {
    u8 type[EMIT_CAP];
    sha1 sha[EMIT_CAP];
    u32 n;
    ok64 fail_after;   //  if > 0, the (n+1)-th call returns this
} rec_emit;

static ok64 rec_cb(void *ctx, u8 type, sha1cp sha, u8csc body) {
    (void)body;
    rec_emit *r = (rec_emit *)ctx;
    if (r->fail_after && r->n + 1 == r->fail_after) {
        return r->fail_after;
    }
    if (r->n < EMIT_CAP) {
        r->type[r->n] = type;
        r->sha[r->n] = *sha;
        r->n++;
    }
    return OK;
}

// --- Tests --------------------------------------------------------------

ok64 test_patchid(void) {
    sane(1);
    call(setup_repo);

    keep_pack p = {};
    call(KEEPPackOpen, &p);
    p.strict_order = NO;

    //  Two parents that share an identical tree: file `f.txt` = "old\n".
    //  Children that change `f.txt` to "new\n" against either parent
    //  should yield equal patch-ids (case (c)).
    sha1 b_old = {}, t_par_a = {}, t_par_b = {};
    call(make_single_leaf_tree, &p, "100644 f.txt", "old\n",
         &b_old, &t_par_a);
    sha1 b_old2 = {}, t_par_b_unused = {};
    call(make_single_leaf_tree, &p, "100644 f.txt", "old\n",
         &b_old2, &t_par_b);
    (void)b_old2; (void)t_par_b_unused;

    //  Two distinct parent commits referencing those (identical) trees.
    sha1 par_a = {}, par_b = {};
    call(feed_commit, &p, &t_par_a, NULL, "alice", 1000, "init A", &par_a);
    call(feed_commit, &p, &t_par_b, NULL, "bob",   2000, "init B", &par_b);

    //  Child trees: f.txt = "new\n".  Build twice to share a blob.
    sha1 b_new = {}, t_chi_a = {}, t_chi_b = {};
    call(make_single_leaf_tree, &p, "100644 f.txt", "new\n",
         &b_new, &t_chi_a);
    sha1 b_new2 = {};
    call(make_single_leaf_tree, &p, "100644 f.txt", "new\n",
         &b_new2, &t_chi_b);
    (void)b_new2;

    sha1 chi_a = {}, chi_b = {};
    call(feed_commit, &p, &t_chi_a, &par_a, "alice", 1100,
         "modify f", &chi_a);
    call(feed_commit, &p, &t_chi_b, &par_b, "bob",   2100,
         "modify f again", &chi_b);

    //  Also: a child that ADDS g.txt against par_a (different diff).
    sha1 b_g = {}, t_extra = {};
    {
        Bu8 tb = {};
        call(u8bAllocate, tb, 256);
        a_cstr(e1, "100644 f.txt");
        u8bFeed(tb, e1);
        u8bFeed1(tb, 0);
        a_rawc(o1, b_old);
        u8bFeed(tb, o1);
        a_cstr(e2, "100644 g.txt");
        u8bFeed(tb, e2);
        u8bFeed1(tb, 0);
        a_cstr(gc, "g\n");
        call(KEEPPackFeed, &p, DOG_OBJ_BLOB, gc, 0, &b_g);
        a_rawc(o2, b_g);
        u8bFeed(tb, o2);
        a_dup(u8c, td, u8bData(tb));
        call(KEEPPackFeed, &p, DOG_OBJ_TREE, td, 0, &t_extra);
        u8bFree(tb);
    }
    sha1 chi_extra = {};
    call(feed_commit, &p, &t_extra, &par_a, "alice", 1200,
         "add g", &chi_extra);

    //  A commit with no diff (tree == parent's tree).
    sha1 chi_nodiff = {};
    call(feed_commit, &p, &t_par_a, &par_a, "alice", 1300,
         "empty", &chi_nodiff);

    call(KEEPPackClose, &p);

    //  Fetch bodies and compute ids.
    Bu8 ba = {}, bb = {}, be = {}, bn = {};
    call(u8bAllocate, ba, 4096);
    call(u8bAllocate, bb, 4096);
    call(u8bAllocate, be, 4096);
    call(u8bAllocate, bn, 4096);
    call(fetch_commit, &chi_a,      ba);
    call(fetch_commit, &chi_b,      bb);
    call(fetch_commit, &chi_extra,  be);
    call(fetch_commit, &chi_nodiff, bn);

    a_dup(u8c, ba_d, u8bData(ba));
    a_dup(u8c, bb_d, u8bData(bb));
    a_dup(u8c, be_d, u8bData(be));
    a_dup(u8c, bn_d, u8bData(bn));

    u64 id_a    = GRAFPatchId(ba_d);
    u64 id_b    = GRAFPatchId(bb_d);
    u64 id_e    = GRAFPatchId(be_d);
    u64 id_none = GRAFPatchId(bn_d);

    //  (a) twice on the same body → same id.
    u64 id_a2 = GRAFPatchId(ba_d);
    want(id_a == id_a2);

    //  (c) rebase invariance: id_a == id_b.
    fprintf(stderr, "  id_a=%016lx id_b=%016lx id_e=%016lx id_none=%016lx\n",
            id_a, id_b, id_e, id_none);
    want(id_a == id_b);

    //  (b) different diffs → different ids.
    want(id_a != id_e);

    //  (d) empty diff → 0.
    want(id_none == 0);

    u8bFree(ba); u8bFree(bb); u8bFree(be); u8bFree(bn);

    teardown_repo();
    fprintf(stderr, "  patchid (a)PASS (b)PASS (c)PASS (d)PASS\n");
    done;
}

//  Build a small repo with this layout:
//
//      base_old → child1 → child2  (linear)
//      base_old → base_new          (alternate spine, 1-step ahead)
//
//  Then GRAFRebase(base_old, base_new, child2) replays both onto
//  base_new, optionally skipping child1 if its patch-id matches a
//  commit on base_new.
//
//  We keep tests deterministic by using fixed timestamps.
ok64 test_rebase(void) {
    sane(1);
    call(setup_repo);

    keep_pack p = {};
    call(KEEPPackOpen, &p);
    p.strict_order = NO;

    //  Initial blob+tree at base_old: f.txt = "v0\n"
    sha1 b0 = {}, t0 = {};
    call(make_single_leaf_tree, &p, "100644 f.txt", "v0\n", &b0, &t0);
    sha1 base_old = {};
    call(feed_commit, &p, &t0, NULL, "alice", 100, "v0", &base_old);

    //  base_new: tree has f.txt = "v0\n" + g.txt = "g\n"  (cherry-pickable add)
    sha1 g_blob = {}, t_bn = {};
    {
        a_cstr(gc, "g\n");
        call(KEEPPackFeed, &p, DOG_OBJ_BLOB, gc, 0, &g_blob);
        Bu8 tb = {};
        call(u8bAllocate, tb, 256);
        a_cstr(e1, "100644 f.txt"); u8bFeed(tb, e1);
        u8bFeed1(tb, 0); a_rawc(o1, b0); u8bFeed(tb, o1);
        a_cstr(e2, "100644 g.txt"); u8bFeed(tb, e2);
        u8bFeed1(tb, 0); a_rawc(o2, g_blob); u8bFeed(tb, o2);
        a_dup(u8c, td, u8bData(tb));
        call(KEEPPackFeed, &p, DOG_OBJ_TREE, td, 0, &t_bn);
        u8bFree(tb);
    }
    sha1 base_new = {};
    call(feed_commit, &p, &t_bn, &base_old, "alice", 200,
         "add g (on new spine)", &base_new);

    //  child1 on top of base_old: also adds g.txt = "g\n" — same diff!
    sha1 child1 = {};
    call(feed_commit, &p, &t_bn, &base_old, "alice", 300,
         "add g (on old spine)", &child1);

    //  child2 on top of child1: changes f.txt to "v2\n"
    sha1 b2 = {}, t_c2 = {};
    {
        a_cstr(gc, "g\n");
        sha1 g2 = {};  //  reuse same content; KEEP will dedup
        call(KEEPPackFeed, &p, DOG_OBJ_BLOB, gc, 0, &g2);
        a_cstr(c2, "v2\n");
        call(KEEPPackFeed, &p, DOG_OBJ_BLOB, c2, 0, &b2);
        Bu8 tb = {};
        call(u8bAllocate, tb, 256);
        a_cstr(e1, "100644 f.txt"); u8bFeed(tb, e1);
        u8bFeed1(tb, 0); a_rawc(o1, b2); u8bFeed(tb, o1);
        a_cstr(e2, "100644 g.txt"); u8bFeed(tb, e2);
        u8bFeed1(tb, 0); a_rawc(o2, g2); u8bFeed(tb, o2);
        a_dup(u8c, td, u8bData(tb));
        call(KEEPPackFeed, &p, DOG_OBJ_TREE, td, 0, &t_c2);
        u8bFree(tb);
    }
    sha1 child2 = {};
    call(feed_commit, &p, &t_c2, &child1, "alice", 400, "v2", &child2);

    call(KEEPPackClose, &p);

    //  (i) child_tip == base_old → no emits.
    {
        rec_emit r = {};
        ok64 o = GRAFRebase(&base_old, &base_new, &base_old, rec_cb, &r);
        want(o == OK);
        want(r.n == 0);
        fprintf(stderr, "  rebase (i)PASS\n");
    }

    //  (k) Two-commit child where the FIRST commit is patch-id-equiv
    //      to a commit on base_new — child1 should be skipped, child2
    //      replayed.  We expect at least one commit emit (child2's
    //      replay).
    {
        rec_emit r = {};
        ok64 o = GRAFRebase(&base_old, &base_new, &child2, rec_cb, &r);
        if (o != OK) {
            fprintf(stderr, "  rebase k: got 0x%lx, want OK\n",
                    (unsigned long)o);
            fail(TESTFAIL);
        }
        u32 commits = 0;
        for (u32 i = 0; i < r.n; i++) {
            if (r.type[i] == DOG_OBJ_COMMIT) commits++;
        }
        if (commits != 1) {
            fprintf(stderr, "  rebase k: %u commits emitted (want 1)\n",
                    commits);
            fail(TESTFAIL);
        }
        fprintf(stderr, "  rebase (k)PASS  (commits=%u total emits=%u)\n",
                commits, r.n);
    }

    //  (j) Single-commit child where there is no patch-id collision.
    //      Build base_alt = base_old (no extras), child_solo with a
    //      change unique to itself.  Replay onto base_alt.
    keep_pack p2 = {};
    call(KEEPPackOpen, &p2);
    p2.strict_order = NO;
    sha1 b_solo = {}, t_solo = {};
    call(make_single_leaf_tree, &p2, "100644 f.txt", "solo\n",
         &b_solo, &t_solo);
    sha1 child_solo = {};
    call(feed_commit, &p2, &t_solo, &base_old, "alice", 500,
         "solo edit", &child_solo);
    call(KEEPPackClose, &p2);
    {
        rec_emit r = {};
        ok64 o = GRAFRebase(&base_old, &base_old, &child_solo,
                            rec_cb, &r);
        if (o != OK) {
            fprintf(stderr, "  rebase j: got 0x%lx\n", (unsigned long)o);
            fail(TESTFAIL);
        }
        u32 commits = 0;
        for (u32 i = 0; i < r.n; i++) {
            if (r.type[i] == DOG_OBJ_COMMIT) commits++;
        }
        if (commits != 1) {
            fprintf(stderr, "  rebase j: commits=%u (want 1)\n", commits);
            fail(TESTFAIL);
        }
        fprintf(stderr, "  rebase (j)PASS  (commits=%u total emits=%u)\n",
                commits, r.n);
    }

    //  (l) Conflict.  Build a separate spine where:
    //      base_old has f.txt = "v0\n"
    //      base_new spine modifies f.txt → "head\n"
    //      child  spine modifies f.txt → "branch\n"
    //  Replaying child onto base_new requires merging "v0", "head",
    //  "branch" — concurrent edits, conflict.
    keep_pack p3 = {};
    call(KEEPPackOpen, &p3);
    p3.strict_order = NO;
    sha1 b_h = {}, t_h = {};
    call(make_single_leaf_tree, &p3, "100644 f.txt", "head\n",
         &b_h, &t_h);
    sha1 base_new_h = {};
    call(feed_commit, &p3, &t_h, &base_old, "alice", 600,
         "head edit", &base_new_h);

    sha1 b_b = {}, t_b = {};
    call(make_single_leaf_tree, &p3, "100644 f.txt", "branch\n",
         &b_b, &t_b);
    sha1 child_branch = {};
    call(feed_commit, &p3, &t_b, &base_old, "alice", 700,
         "branch edit", &child_branch);
    call(KEEPPackClose, &p3);
    {
        rec_emit r = {};
        ok64 o = GRAFRebase(&base_old, &base_new_h, &child_branch,
                            rec_cb, &r);
        if (o != GRAFCNFL) {
            fprintf(stderr, "  rebase l: got 0x%lx, want GRAFCNFL\n",
                    (unsigned long)o);
            fail(TESTFAIL);
        }
        //  No COMMIT emit for the conflicting commit.
        for (u32 i = 0; i < r.n; i++) {
            if (r.type[i] == DOG_OBJ_COMMIT) {
                fprintf(stderr,
                        "  rebase l: commit emit despite conflict\n");
                fail(TESTFAIL);
            }
        }
        fprintf(stderr, "  rebase (l)PASS  (emits before abort=%u)\n", r.n);
    }

    teardown_repo();
    done;
}

// --- (m) GRAFRebaseFileWeave -------------------------------------------
//
//  Linear chain of three commits modifying f.txt one line at a time:
//      C1: "alpha\nbeta\ngamma\n"
//      C2: "alpha\nBETA\ngamma\n"   (line 2 modified)
//      C3: "alpha\nBETA\nGAMMA\n"   (line 3 modified)
//
//  Expectations:
//    - per-step callback fires exactly 3 times, in chain order, with
//      `src_id` = low-32 of WHIFFHashlet40 and `commit_h` =
//      WHIFFHashlet60 for each commit;
//    - alive-byte concatenation of the resulting weave equals C3's
//      blob bytes verbatim.

#define FW_REC_CAP 16
typedef struct {
    u32  src[FW_REC_CAP];
    u64  h60[FW_REC_CAP];
    u32  n;
} fw_rec;

static ok64 fw_step_cb(u32 src_id, u64 commit_h, void *ctx) {
    fw_rec *r = (fw_rec *)ctx;
    if (r->n < FW_REC_CAP) {
        r->src[r->n] = src_id;
        r->h60[r->n] = commit_h;
        r->n++;
    }
    return OK;
}

ok64 test_rebase_file_weave(void) {
    sane(1);
    call(setup_repo);

    keep_pack p = {};
    call(KEEPPackOpen, &p);
    p.strict_order = NO;

    sha1 b1 = {}, t1 = {}, c1 = {};
    call(make_single_leaf_tree, &p, "100644 f.txt",
         "alpha\nbeta\ngamma\n", &b1, &t1);
    call(feed_commit, &p, &t1, NULL, "alice", 1000, "v1", &c1);

    sha1 b2 = {}, t2 = {}, c2 = {};
    call(make_single_leaf_tree, &p, "100644 f.txt",
         "alpha\nBETA\ngamma\n", &b2, &t2);
    call(feed_commit, &p, &t2, &c1, "alice", 1100, "v2", &c2);

    sha1 b3 = {}, t3 = {}, c3 = {};
    call(make_single_leaf_tree, &p, "100644 f.txt",
         "alpha\nBETA\nGAMMA\n", &b3, &t3);
    call(feed_commit, &p, &t3, &c2, "alice", 1200, "v3", &c3);

    call(KEEPPackClose, &p);

    sha1 chain[3] = {c1, c2, c3};
    weave wA = {}, wB = {}, wnu = {};
    call(WEAVEInit, &wA);
    call(WEAVEInit, &wB);
    call(WEAVEInit, &wnu);

    fw_rec rec = {};
    a_cstr(fp, "f.txt");
    weave *out = NULL;
    call(GRAFRebaseFileWeave, &wA, &wB, &wnu, &out,
         &KEEP, fp, chain, 3, fw_step_cb, &rec);
    want(out != NULL);

    if (rec.n != 3) {
        fprintf(stderr, "  fw: cb fired %u times (want 3)\n", rec.n);
        fail(TESTFAIL);
    }
    want(rec.h60[0] == WHIFFHashlet60(&c1));
    want(rec.h60[1] == WHIFFHashlet60(&c2));
    want(rec.h60[2] == WHIFFHashlet60(&c3));
    want(rec.src[0] == (u32)WHIFFHashlet40(&c1));
    want(rec.src[1] == (u32)WHIFFHashlet40(&c2));
    want(rec.src[2] == (u32)WHIFFHashlet40(&c3));

    //  Render alive bytes from the resulting weave.
    Bu8 ab = {};
    call(u8bMap, ab, 1UL << 16);

    call(WEAVEAliveBytes, out, ab);
    a_dup(u8c, alive, u8bData(ab));
    a_cstr(want_s, "alpha\nBETA\nGAMMA\n");
    if ($len(alive) != $len(want_s) ||
        memcmp(alive[0], want_s[0], $len(alive)) != 0) {
        fprintf(stderr, "  fw: alive=%.*s\n",
                (int)$len(alive), (char const *)alive[0]);
        u8bUnMap(ab);
        fail(TESTFAIL);
    }
    u8bUnMap(ab);

    WEAVEFree(&wA);
    WEAVEFree(&wB);
    WEAVEFree(&wnu);

    teardown_repo();
    fprintf(stderr, "  rebase_file_weave (m)PASS\n");
    done;
}

// --- (n)/(o) GRAFRebaseBlobMerge -----------------------------------------
//
//  WEAVE-merge step.  Two pre-built weaves (running, branch), each
//  bootstrapped from a shared ancestor blob (src=0) and extended by
//  one edit (src=R_h32 / src=B_h32).  Merge and render bytes.
//
//    (n) disjoint edits → no conflict, merged bytes contain both
//        sides' changes and equal the natural concatenated result;
//    (o) same-line edits → conflict flag set, marker bytes present.

typedef struct {
    u32 hs[8];
    u32 n;
} u32_set;

static b8 in_u32_set(u32 h32, void *ctx) {
    u32_set const *s = (u32_set const *)ctx;
    for (u32 i = 0; i < s->n; i++) if (s->hs[i] == h32) return YES;
    return NO;
}

//  Build a 2-version weave: bootstrap from `anc` (src=0), diff
//  toward `nu` with `src`.  Caller owns the three weaves and
//  must `WEAVEFree` them after.
static ok64 build_2v_weave(weave *out, weave *bs, weave *nu_w,
                           u8cs anc, u8cs nu_bytes, u8cs ext, u32 src) {
    sane(out && bs && nu_w);
    call(WEAVEFromBlob, bs, anc, ext, 0);
    call(WEAVEFromBlob, nu_w, nu_bytes, ext, src);
    call(WEAVEDiff, out, bs, nu_w, src);
    done;
}

ok64 test_rebase_blob_merge(void) {
    sane(1);

    a_cstr(ext, "txt");

    //  --- (n) disjoint edits ----------------------------------------
    {
        a_cstr(anc,    "x = 1\ny = 2\nz = 3\n");
        a_cstr(rbytes, "x = 10\ny = 2\nz = 3\n");
        a_cstr(bbytes, "x = 1\ny = 2\nz = 30\n");

        Bu8 out = {};
        call(u8bMap, out, 1UL << 16);
        b8 conflict = YES;
        call(GRAFRebaseBlobMerge, anc, rbytes, bbytes, ext, out, &conflict);

        if (conflict) {
            fprintf(stderr, "  bm n: unexpected conflict\n");
            u8bUnMap(out); fail(TESTFAIL);
        }
        a_dup(u8c, od, u8bData(out));
        a_cstr(want_s, "x = 10\ny = 2\nz = 30\n");
        if ($len(od) != $len(want_s) ||
            memcmp(od[0], want_s[0], $len(od)) != 0) {
            fprintf(stderr, "  bm n: got %.*s\n",
                    (int)$len(od), (char const *)od[0]);
            u8bUnMap(out); fail(TESTFAIL);
        }
        u8bUnMap(out);
    }

    //  --- (o) same-line conflict ------------------------------------
    {
        a_cstr(anc,    "v = old\n");
        a_cstr(rbytes, "v = new1\n");
        a_cstr(bbytes, "v = new2\n");

        Bu8 out = {};
        call(u8bMap, out, 1UL << 16);
        b8 conflict = NO;
        call(GRAFRebaseBlobMerge, anc, rbytes, bbytes, ext, out, &conflict);

        if (!conflict) {
            a_dup(u8c, od, u8bData(out));
            fprintf(stderr, "  bm o: expected conflict, got %.*s\n",
                    (int)$len(od), (char const *)od[0]);
            u8bUnMap(out); fail(TESTFAIL);
        }
        u8bUnMap(out);
    }

    fprintf(stderr, "  rebase_blob_merge (n)PASS (o)PASS\n");
    done;
}

// --- (p) BASS-bounded long-chain rebase (MEM-008) -----------------------
//
//  Repro for MEM-008: GRAFRebase replayed the chain calling the heavy
//  carving helpers (tm_merge_trees / tm_merge_blob / GRAFPatchId) by
//  plain assignment, so ABC_BASS was never rewound between iterations.
//  Each modify/modify leaf carves 4*REBASE_BLOB_MAX (=64MiB); over a
//  chain the arena grows unbounded and a_carve eventually returns
//  BNOROOM, aborting the rebase.
//
//  This test builds a chain of `CHAIN_N` commits that each modify the
//  SAME file f.txt divergently from a base_new spine that also modified
//  it — so every replay step is a real 3-way leaf merge (the 64MiB
//  path).  At each COMMIT emit we sample the BASS dispense offset; with
//  the bug it climbs ~tens of MiB per iteration (and at CHAIN_N=24 the
//  whole rebase aborts with BNOROOM before completing).  With the fix,
//  BASS is rewound per iteration so the offset stays flat and all
//  commits replay cleanly.

//  Long enough that pre-fix BASS (1GiB) overflows: ~66MiB/iter * 24 >> 1GiB.
#define BASS_CHAIN_N 24

typedef struct {
    keep_pack *p;       //  emitted objects persisted here
    u32 commits;
    size_t first_off;   //  BASS dispense offset at first commit emit
    size_t last_off;    //  ... at last commit emit
    size_t max_off;     //  peak across all emits
} bass_rec;

static size_t bass_dispensed(void) {
    //  IDLE head minus the arena base = bytes currently dispensed.
    return (size_t)(ABC_BASS[2] - ABC_BASS[0]);
}

//  Mirror sniff/POST.c::post_rebase_emit_cb: persist every emitted
//  object and checkpoint the pack on each commit so the next rebase
//  iteration can resolve the prior step's tree/blob via KEEPGetExact.
//  Also sample the BASS dispense offset at each commit emit — this is
//  what climbs unbounded with the MEM-008 bug.
static ok64 bass_cb(void *ctx, u8 type, sha1cp sha, u8csc body) {
    (void)sha;
    bass_rec *r = (bass_rec *)ctx;
    sha1 fed = {};
    ok64 fo = KEEPPackFeed(r->p, type, body, 0, &fed);
    if (fo != OK) return fo;
    if (type == DOG_OBJ_COMMIT) {
        size_t off = bass_dispensed();
        if (r->commits == 0) r->first_off = off;
        r->last_off = off;
        if (off > r->max_off) r->max_off = off;
        r->commits++;
        //  Checkpoint so the just-emitted objects index for the next
        //  iteration's KEEPGetExact lookups.
        ok64 cl = KEEPPackClose(r->p);
        if (cl != OK) return cl;
        zero(*r->p);
        ok64 op = KEEPPackOpen(r->p);
        if (op != OK) return op;
        r->p->strict_order = NO;
    }
    return OK;
}

//  Feed a tree carrying a single "100644 f.txt" leaf whose blob is the
//  joined `lines` (each already including its trailing '\n').
static ok64 feed_ftxt_tree(keep_pack *p, u8csc content,
                           sha1 *blob_out, sha1 *tree_out) {
    sane(p && blob_out && tree_out);
    call(KEEPPackFeed, p, DOG_OBJ_BLOB, content, 0, blob_out);
    a_pad(u8, tb, 256);
    a_cstr(mn, "100644 f.txt");
    call(u8bFeed, tb, mn);
    u8bFeed1(tb, 0);
    a_rawc(ss, *blob_out);
    call(u8bFeed, tb, ss);
    a_dup(u8c, tc, u8bData(tb));
    call(KEEPPackFeed, p, DOG_OBJ_TREE, tc, 0, tree_out);
    done;
}

ok64 test_rebase_bass_bounded(void) {
    sane(1);
    call(setup_repo);

    keep_pack p = {};
    call(KEEPPackOpen, &p);
    p.strict_order = NO;

    //  Base file: a block of distinct lines so chain edits to the tail
    //  and the base_new edit to the head never overlap → clean 3-way
    //  merges (no conflict), but a real modify/modify leaf each step.
    Bu8 buf = {};
    call(u8bMap, buf, 1UL << 16);
    a_cstr(base_line, "line 00 base\n");
    for (u32 i = 0; i < 40; i++) {
        char ln[32];
        int n = snprintf(ln, sizeof(ln), "line %02u base\n", i);
        u8cs s = {(u8cp)ln, (u8cp)ln + n};
        u8bFeed(buf, s);
    }
    (void)base_line;
    a_dup(u8c, base_content, u8bData(buf));

    sha1 b0 = {}, t0 = {};
    call(feed_ftxt_tree, &p, base_content, &b0, &t0);
    sha1 base_old = {};
    call(feed_commit, &p, &t0, NULL, "alice", 100, "v0", &base_old);

    //  base_new spine: modify the FIRST line only.
    u8bReset(buf);
    {
        a_cstr(hl, "line 00 HEAD\n");
        u8bFeed(buf, hl);
        for (u32 i = 1; i < 40; i++) {
            char ln[32];
            int n = snprintf(ln, sizeof(ln), "line %02u base\n", i);
            u8cs s = {(u8cp)ln, (u8cp)ln + n};
            u8bFeed(buf, s);
        }
    }
    a_dup(u8c, bn_content, u8bData(buf));
    sha1 b_bn = {}, t_bn = {};
    call(feed_ftxt_tree, &p, bn_content, &b_bn, &t_bn);
    sha1 base_new = {};
    call(feed_commit, &p, &t_bn, &base_old, "alice", 200, "head edit", &base_new);

    //  Chain off base_old: commit i modifies the LAST line uniquely.
    //  Each is a real modify/modify vs base_new (different region).
    sha1 chain_tip = base_old;
    long ts = 300;
    for (u32 c = 0; c < BASS_CHAIN_N; c++) {
        u8bReset(buf);
        for (u32 i = 0; i < 39; i++) {
            char ln[32];
            int n = snprintf(ln, sizeof(ln), "line %02u base\n", i);
            u8cs s = {(u8cp)ln, (u8cp)ln + n};
            u8bFeed(buf, s);
        }
        char last[40];
        int n = snprintf(last, sizeof(last), "line 39 chain step %02u\n", c);
        u8cs ls = {(u8cp)last, (u8cp)last + n};
        u8bFeed(buf, ls);
        a_dup(u8c, cc, u8bData(buf));
        sha1 bc = {}, tc = {};
        call(feed_ftxt_tree, &p, cc, &bc, &tc);
        sha1 cm = {};
        char msg[32];
        snprintf(msg, sizeof(msg), "step %02u", c);
        call(feed_commit, &p, &tc, &chain_tip, "alice", ts++, msg, &cm);
        chain_tip = cm;
    }
    u8bUnMap(buf);

    //  Close the chain-building pack so its objects are indexed, then
    //  open a fresh pack the rebase's emit cb feeds into.
    call(KEEPPackClose, &p);
    keep_pack rp = {};
    call(KEEPPackOpen, &rp);
    rp.strict_order = NO;

    //  Replay the whole chain onto base_new.  Pre-fix: each
    //  modify/modify leaf leaks 64MiB of BASS scratch, so the dispense
    //  offset climbs ~tens of MiB per commit and (at BASS_CHAIN_N=24)
    //  a_carve eventually returns BNOROOM, aborting mid-chain.
    //  Post-fix: OK + every commit emitted + a flat BASS offset.
    bass_rec r = {.p = &rp};
    ok64 o = GRAFRebase(&base_old, &base_new, &chain_tip, bass_cb, &r);
    KEEPPackClose(&rp);

    fprintf(stderr,
            "  bass: ret=0x%lx commits=%u first_off=%zu last_off=%zu max=%zu\n",
            (unsigned long)o, r.commits, r.first_off, r.last_off, r.max_off);

    if (o != OK) {
        fprintf(stderr,
                "  bass (p)FAIL: rebase aborted (BASS leak → NOROOM) "
                "after %u/%u commits\n", r.commits, BASS_CHAIN_N);
        fail(TESTFAIL);
    }
    if (r.commits != BASS_CHAIN_N) {
        fprintf(stderr, "  bass (p)FAIL: %u/%u commits replayed\n",
                r.commits, BASS_CHAIN_N);
        fail(TESTFAIL);
    }
    //  Bounded growth: across the whole chain the per-iteration BASS
    //  footprint must not creep.  With the bug the last emit sits tens
    //  of MiB above the first; with the fix it is essentially constant.
    //  Allow a generous 2MiB slack for benign one-time allocations.
    size_t spread = (r.last_off > r.first_off) ? r.last_off - r.first_off : 0;
    if (spread > (2UL << 20)) {
        fprintf(stderr,
                "  bass (p)FAIL: BASS grew %zu bytes across %u iterations "
                "(unbounded per-iteration scratch)\n", spread, r.commits);
        fail(TESTFAIL);
    }

    teardown_repo();
    fprintf(stderr, "  rebase_bass_bounded (p)PASS  (spread=%zu bytes)\n",
            spread);
    done;
}

ok64 maintest(void) {
    sane(1);
    call(test_patchid);
    call(test_rebase);
    call(test_rebase_file_weave);
    call(test_rebase_blob_merge);
    call(test_rebase_bass_bounded);
    done;
}

TEST(maintest)
