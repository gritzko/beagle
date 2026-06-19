//  CLOSE tests: object-closure walk + sorted sha set, with emphasis on
//  the merge-history dedup short-circuit (KEEP-001).  CLOSEWalkTree /
//  CLOSEWalkCommit route their tree descent through the one shared
//  keeper walker (KEEPWalkTree); the close_set short-circuit is the
//  visitor returning WALKSKIP so a subtree reachable through two paths
//  is collected exactly once.
//
#include "keeper/CLOSE.h"
#include "keeper/KEEP.h"
#include "keeper/WALK.h"
#include "dog/git/GIT.h"
#include "dog/git/SHA1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/TEST.h"
#include "dog/DOG.h"

//  Build a single-entry tree { <mode_name>\0<blob_sha> }.
static ok64 leaf_tree(keep_pack *p, u8cs mode_name, u8cs content,
                      sha1 *tree_out) {
    sane(p && tree_out);
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

//  Append "<mode> <name>\0<raw-sha>" to a tree buffer.
static ok64 tree_add(u8bp tb, char const *mode_name, sha1cp sha) {
    sane(tb && sha);
    u8cs mn = {(u8cp)mode_name, (u8cp)mode_name + strlen(mode_name)};
    call(u8bFeed, tb, mn);
    u8bFeed1(tb, 0);
    a_rawc(ss, *sha);
    call(u8bFeed, tb, ss);
    done;
}

//  Build a commit body { tree <hex>\n [parent <hex>\n]* author...\n\n<tag> }.
static ok64 build_commit(keep_pack *p, char tag, sha1cp tree,
                         sha1cp const *parents, u32 nparent, sha1 *out) {
    sane(p && tree && out);
    a_pad(u8, body, 1024);
    u8bFeed(body, GIT_FIELD_TREE);
    u8bFeed1(body, ' ');
    a_sha1hex(ths, tree);
    u8bFeed(body, ths);
    u8bFeed1(body, '\n');
    for (u32 i = 0; i < nparent; i++) {
        u8bFeed(body, GIT_FIELD_PARENT);
        u8bFeed1(body, ' ');
        a_sha1hex(phs, parents[i]);
        u8bFeed(body, phs);
        u8bFeed1(body, '\n');
    }
    a_cstr(au, "author x <x> 0 +0000\ncommitter x <x> 0 +0000\n\n");
    u8bFeed(body, au);
    u8bFeed1(body, tag);
    u8bFeed1(body, '\n');
    a_dup(u8c, bc, u8bData(body));
    call(KEEPPackFeed, p, DOG_OBJ_COMMIT, bc, 0, out);
    done;
}

static b8 sha_in(sha1 const *arr, u32 n, sha1cp q) {
    for (u32 i = 0; i < n; i++) if (sha1Eq(&arr[i], q)) return YES;
    return NO;
}

// ---- Test 1: close_set membership / sorted-dedup insert ----

ok64 CLOSEtest1() {
    sane(1);
    a_pad(sha1, items, 8);
    close_set s = {.items = items_idle[0], .n = 0, .cap = 8};

    sha1 a = {}, b = {}, c = {};
    a.data[0] = 1; b.data[0] = 2; c.data[0] = 3;
    want(!CLOSESetHas(&s, &b));
    CLOSESetAdd(&s, &c);
    CLOSESetAdd(&s, &a);
    CLOSESetAdd(&s, &b);
    CLOSESetAdd(&s, &b);          // dup: no-op
    want(s.n == 3);
    want(CLOSESetHas(&s, &a));
    want(CLOSESetHas(&s, &b));
    want(CLOSESetHas(&s, &c));
    sha1 d = {}; d.data[0] = 9;
    want(!CLOSESetHas(&s, &d));
    done;
}

// ---- Test 2: merge-history subtree collected once (the short-circuit) ----
//
//  Shape:  M ── parent ──▶ P1 ──▶ tree T1 { a/ → S, x.txt → bx }
//          └── parent ──▶ P2 ──▶ tree T2 { a/ → S, y.txt → by }
//  S is the SAME shared subtree under both parents (a merge that kept
//  one side's `a/` verbatim).  Walking M's full closure must list S (and
//  its blob) EXACTLY ONCE — the close_set dedup short-circuit.

ok64 CLOSEtest2() {
    sane(1);
    call(FILEInit);

    char tmp[] = "/tmp/closetest2-XXXXXX";
    want(mkdtemp(tmp) != NULL);
    a_cstr(root, tmp);
    home h = {};
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);

    keep_pack p = {};
    call(KEEPPackOpen, &p);
    p.strict_order = NO;

    //  Shared subtree S = { 100644 shared.txt → "S\n" }.
    a_cstr(s_mn, "100644 shared.txt");
    a_cstr(s_content, "S\n");
    sha1 S = {};
    call(leaf_tree, &p, s_mn, s_content, &S);

    //  Two distinct leaf blobs for the per-parent unique files.
    a_cstr(bx_content, "X\n");
    sha1 bx = {};
    call(KEEPPackFeed, &p, DOG_OBJ_BLOB, bx_content, 0, &bx);
    a_cstr(by_content, "Y\n");
    sha1 by = {};
    call(KEEPPackFeed, &p, DOG_OBJ_BLOB, by_content, 0, &by);

    //  T1 = { a → S, x.txt → bx } ; T2 = { a → S, y.txt → by }.
    //  git sort order: "a" (dir) sorts as "a/" so it precedes x.txt/y.txt.
    a_pad(u8, t1b, 256);
    call(tree_add, t1b, "40000 a", &S);
    call(tree_add, t1b, "100644 x.txt", &bx);
    a_dup(u8c, t1c, u8bData(t1b));
    sha1 T1 = {};
    call(KEEPPackFeed, &p, DOG_OBJ_TREE, t1c, 0, &T1);

    a_pad(u8, t2b, 256);
    call(tree_add, t2b, "40000 a", &S);
    call(tree_add, t2b, "100644 y.txt", &by);
    a_dup(u8c, t2c, u8bData(t2b));
    sha1 T2 = {};
    call(KEEPPackFeed, &p, DOG_OBJ_TREE, t2c, 0, &T2);

    //  Merge tree TM (M's own tree) reuses S too, so S is reachable
    //  from THREE trees — the harshest dedup case.
    a_pad(u8, tmb, 256);
    call(tree_add, tmb, "40000 a", &S);
    a_dup(u8c, tmc, u8bData(tmb));
    sha1 TM = {};
    call(KEEPPackFeed, &p, DOG_OBJ_TREE, tmc, 0, &TM);

    sha1 P1 = {}, P2 = {}, M = {};
    call(build_commit, &p, '1', &T1, NULL, 0, &P1);
    call(build_commit, &p, '2', &T2, NULL, 0, &P2);
    { sha1cp pp[2] = {&P1, &P2};
      call(build_commit, &p, 'M', &TM, pp, 2, &M); }

    call(KEEPPackClose, &p);

    //  Walk M's full closure (no have-set) with the dedup set active.
    #define CAP 64
    a_pad(sha1, out, CAP);
    a_pad(sha1, seen_items, CAP);
    sha1 *outp = out_idle[0];
    u32   n = 0;
    close_set seen = {.items = seen_items_idle[0], .n = 0, .cap = CAP};
    call(CLOSEWalkCommit, &M, outp, &n, CAP, NULL, &seen);

    //  Each object appears exactly once in `out`.
    for (u32 i = 0; i < n; i++)
        for (u32 j = i + 1; j < n; j++)
            want(!sha1Eq(&outp[i], &outp[j]));

    //  S and its blob present exactly once; all commits + trees present.
    u32 s_count = 0;
    for (u32 i = 0; i < n; i++) if (sha1Eq(&outp[i], &S)) s_count++;
    want(s_count == 1);
    want(sha_in(outp, n, &M));
    want(sha_in(outp, n, &P1));
    want(sha_in(outp, n, &P2));
    want(sha_in(outp, n, &T1));
    want(sha_in(outp, n, &T2));
    want(sha_in(outp, n, &TM));
    want(sha_in(outp, n, &S));
    want(sha_in(outp, n, &bx));
    want(sha_in(outp, n, &by));

    //  Expected unique objects: M,P1,P2 (3 commits) + TM,T1,T2,S (4 trees)
    //  + bx,by + S's shared.txt blob = 10.
    want(n == 10);

    //  have-set prune: re-walk with S pre-marked as had → S and its blob
    //  must be absent (short-circuit before descend).
    {
        a_pad(sha1, out2, CAP);
        a_pad(sha1, seen2_items, CAP);
        sha1 *out2p = out2_idle[0];
        u32   n2 = 0;
        close_set have = {.items = NULL, .n = 0, .cap = 0};
        a_pad(sha1, have_items, 1);
        have.items = have_items_idle[0]; have.cap = 1;
        CLOSESetAdd(&have, &S);
        close_set seen2 = {.items = seen2_items_idle[0], .n = 0, .cap = CAP};
        call(CLOSEWalkCommit, &M, out2p, &n2, CAP, &have, &seen2);
        want(!sha_in(out2p, n2, &S));
        //  S's blob is only reachable through S, so it is pruned too.
        for (u32 i = 0; i < n2; i++) want(!sha1Eq(&out2p[i], &S));
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
    call(CLOSEtest1);
    call(CLOSEtest2);
    done;
}

TEST(maintest)
