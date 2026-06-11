//
//  REBASE02 - Regression repro for the in-chain patch-id self-extend
//  bug in GRAFRebase (graf/REBASE.c).
//
//  Bug: the replay loop used to append each replayed commit's OWN
//  patch-id to the dedup set:
//      if (npids < REBASE_PIDS_MAX && pid != 0) pids[npids++] = pid;
//  Correct rebase dedup is ONLY against base_new's ancestor patch-ids
//  (the set built by rebase_collect_pids), never self-extended in-chain.
//
//  Repro chain (each commit touches f.txt = "v0\n" plus an optional x):
//      base_old : { f.txt }
//      A        : { f.txt, x="1" }      (adds x)
//      B        : { f.txt }             (reverts x; tree == base_old)
//      C        : { f.txt, x="1" }      (re-adds x, byte-identical to A)
//
//  A's diff (parent base_old, no x -> +x="1") and C's diff (parent B,
//  no x -> +x="1") are byte-identical, so GRAFPatchId(A) == GRAFPatchId(C).
//
//  Rebase the whole chain onto base_old (base_new == base_old, a root
//  commit whose collected pid set is effectively empty).  Expectation:
//      - ALL THREE commits replay  -> 3 COMMIT emits
//      - final head's tree contains x="1"
//  On the UNFIXED code C's pid collides with the self-extended A pid and
//  C is silently skipped: 2 commits, x absent in the final head -> RED.
//  After removing the self-extend: 3 commits, x present -> GREEN.
//

#include "graf/REBASE.h"

#include "graf/BLOB.h"
#include "graf/GRAF.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PRO.h"
#include "abc/TEST.h"
#include "dog/DOG.h"
#include "dog/HOME.h"
#include "dog/WHIFF.h"
#include "keeper/KEEP.h"

// --- Tiny test harness (mirrors REBASE01) ---------------------------------

static char g_tmp[256];
static home g_home;

static ok64 setup_repo(void) {
    sane(1);
    call(FILEInit);
    snprintf(g_tmp, sizeof(g_tmp), "/tmp/grafrebase2-XXXXXX");
    want(mkdtemp(g_tmp) != NULL);
    a_cstr(root, g_tmp);
    zero(g_home);
    call(HOMEOpenAt, &g_home, root, YES);
    call(KEEPOpen, &g_home, YES);
    call(GRAFOpen, &g_home, YES);
    done;
}

static void teardown_repo(void) {
    GRAFClose();
    KEEPClose();
    HOMEClose(&g_home);
    char cmd[300];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_tmp);
    system(cmd);
}

//  Build a single-leaf tree carrying one blob "<mode> <name>" -> bsha.
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

//  Build a two-leaf tree: entries (mode_name1->c1) then (mode_name2->c2).
//  Entry order must match git's sort (f.txt before x here).
static ok64 make_two_leaf_tree(keep_pack *p,
                               char const *mn1, char const *c1,
                               char const *mn2, char const *c2,
                               sha1 *tree_out) {
    sane(p && tree_out);
    sha1 b1 = {}, b2 = {};
    u8cs cb1 = {(u8cp)c1, (u8cp)c1 + strlen(c1)};
    u8cs cb2 = {(u8cp)c2, (u8cp)c2 + strlen(c2)};
    call(KEEPPackFeed, p, DOG_OBJ_BLOB, cb1, 0, &b1);
    call(KEEPPackFeed, p, DOG_OBJ_BLOB, cb2, 0, &b2);
    a_pad(u8, tb, 256);
    a_cstr(m1, mn1); call(u8bFeed, tb, m1); u8bFeed1(tb, 0);
    a_rawc(s1, b1);  call(u8bFeed, tb, s1);
    a_cstr(m2, mn2); call(u8bFeed, tb, m2); u8bFeed1(tb, 0);
    a_rawc(s2, b2);  call(u8bFeed, tb, s2);
    a_dup(u8c, tc, u8bData(tb));
    call(KEEPPackFeed, p, DOG_OBJ_TREE, tc, 0, tree_out);
    done;
}

//  Build a commit body and feed it; returns its sha.
static void make_commit_body(u8 *const *out,
                             sha1cp tree_sha, sha1cp parent_sha,
                             char const *author_id, long ts,
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

// --- Emit recorder: keeps every emitted (type, sha, body) so we can ----
//     persist them after the rebase and read back the final head.

#define R2_CAP 64
#define R2_BODY 4096
typedef struct {
    u8   type[R2_CAP];
    sha1 sha[R2_CAP];
    u8   body[R2_CAP][R2_BODY];
    u32  blen[R2_CAP];
    u32  n;
} r2_rec;

static ok64 r2_cb(void *ctx, u8 type, sha1cp sha, u8csc body) {
    r2_rec *r = (r2_rec *)ctx;
    if (r->n >= R2_CAP) return GRAFFAIL;
    u64 bl = (u64)$len(body);
    if (bl > R2_BODY) return GRAFFAIL;
    r->type[r->n] = type;
    r->sha[r->n] = *sha;
    if (bl) memcpy(r->body[r->n], body[0], bl);
    r->blen[r->n] = (u32)bl;
    r->n++;
    return OK;
}

ok64 test_revert_reapply(void) {
    sane(1);
    call(setup_repo);

    keep_pack p = {};
    call(KEEPPackOpen, &p);
    p.strict_order = NO;

    //  base_old: { f.txt = "v0\n" }
    sha1 b0 = {}, t0 = {};
    call(make_single_leaf_tree, &p, "100644 f.txt", "v0\n", &b0, &t0);
    sha1 base_old = {};
    call(feed_commit, &p, &t0, NULL, "alice", 100, "v0", &base_old);

    //  A: { f.txt = "v0\n", x = "1" }  (adds x)
    sha1 t_ax = {};
    call(make_two_leaf_tree, &p,
         "100644 f.txt", "v0\n",
         "100644 x",     "1",
         &t_ax);
    sha1 cA = {};
    call(feed_commit, &p, &t_ax, &base_old, "alice", 200, "add x", &cA);

    //  B: { f.txt = "v0\n" }  (reverts x -> tree == base_old's tree t0)
    sha1 cB = {};
    call(feed_commit, &p, &t0, &cA, "alice", 300, "revert x", &cB);

    //  C: { f.txt = "v0\n", x = "1" }  (re-adds x, byte-identical to A)
    //  Reuse t_ax: same content -> KEEP dedups to the same tree sha, so
    //  C's diff (parent B == base_old tree) is byte-identical to A's.
    sha1 cC = {};
    call(feed_commit, &p, &t_ax, &cB, "alice", 400, "re-add x", &cC);

    call(KEEPPackClose, &p);

    //  Sanity: GRAFPatchId(A) == GRAFPatchId(C), GRAFPatchId(B) differs.
    Bu8 ba = {}, bb = {}, bc = {};
    call(u8bAllocate, ba, 4096);
    call(u8bAllocate, bb, 4096);
    call(u8bAllocate, bc, 4096);
    u8 t = 0;
    call(KEEPGetExact, &cA, ba, &t);
    call(KEEPGetExact, &cB, bb, &t);
    call(KEEPGetExact, &cC, bc, &t);
    a_dup(u8c, ba_d, u8bData(ba));
    a_dup(u8c, bb_d, u8bData(bb));
    a_dup(u8c, bc_d, u8bData(bc));
    u64 idA = GRAFPatchId(ba_d);
    u64 idB = GRAFPatchId(bb_d);
    u64 idC = GRAFPatchId(bc_d);
    fprintf(stderr, "  idA=%016lx idB=%016lx idC=%016lx\n", idA, idB, idC);
    want(idA == idC);
    want(idA != idB);
    u8bFree(ba); u8bFree(bb); u8bFree(bc);

    //  Rebase the whole chain onto base_old (base_new == base_old).
    r2_rec *r = (r2_rec *)calloc(1, sizeof(r2_rec));
    want(r != NULL);
    ok64 o = GRAFRebase(&base_old, &base_old, &cC, r2_cb, r);
    if (o != OK) {
        fprintf(stderr, "  rebase: got 0x%lx, want OK\n", (unsigned long)o);
        free(r); fail(TESTFAIL);
    }

    u32 commits = 0, last_commit = R2_CAP;
    for (u32 i = 0; i < r->n; i++) {
        if (r->type[i] == DOG_OBJ_COMMIT) { commits++; last_commit = i; }
    }
    fprintf(stderr, "  replayed commits=%u (want 3) total emits=%u\n",
            commits, r->n);
    if (commits != 3) {
        free(r);
        fprintf(stderr, "  FAIL: revert->reapply lost the reapply\n");
        fail(TESTFAIL);
    }
    want(last_commit < R2_CAP);

    //  Persist every emitted object into keeper so we can read the final
    //  head's tree, then verify x == "1" survived to the final head.
    sha1 head_sha = r->sha[last_commit];
    keep_pack p2 = {};
    if (KEEPPackOpen(&p2) != OK) { free(r); fail(TESTFAIL); }
    p2.strict_order = NO;
    for (u32 i = 0; i < r->n; i++) {
        sha1 got = {};
        u8cs body = {r->body[i], r->body[i] + r->blen[i]};
        if (KEEPPackFeed(&p2, r->type[i], body, 0, &got) != OK) {
            KEEPPackClose(&p2); free(r); fail(TESTFAIL);
        }
    }
    if (KEEPPackClose(&p2) != OK) { free(r); fail(TESTFAIL); }

    Bu8 xb = {};
    call(u8bAllocate, xb, 256);
    a_cstr(xp, "x");
    u64 head_h = WHIFFHashlet60(&head_sha);
    ok64 bo = GRAFBlobAtCommit(xb, head_h, xp);
    if (bo != OK) {
        fprintf(stderr, "  FAIL: x absent in final head (0x%lx)\n",
                (unsigned long)bo);
        u8bFree(xb); free(r); fail(TESTFAIL);
    }
    a_dup(u8c, xd, u8bData(xb));
    a_cstr(want_x, "1");
    if ($len(xd) != $len(want_x) ||
        memcmp(xd[0], want_x[0], $len(xd)) != 0) {
        fprintf(stderr, "  FAIL: x=%.*s (want 1)\n",
                (int)$len(xd), (char const *)xd[0]);
        u8bFree(xb); free(r); fail(TESTFAIL);
    }
    u8bFree(xb);
    free(r);

    teardown_repo();
    fprintf(stderr, "  revert->reapply PASS (3 commits, x=1 in head)\n");
    done;
}

ok64 maintest(void) {
    sane(1);
    call(test_revert_reapply);
    done;
}

TEST(maintest)
