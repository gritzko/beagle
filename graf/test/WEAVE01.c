//
// WEAVE01 - Property tests for the single-weave token engine
// (`WEAVEApply`), 3-way concurrent-branch shape.
//
// Each case names a base blob `o` and two derived blobs `a`, `b`.  The
// whole little DAG is replayed into ONE weave (insert/delete in place,
// never a second weave to reconcile):
//   W = apply(o,   seq=0,    baseline=∅)        // root
//       apply(a,   seq=A,    baseline={0})      // ours,   off base
//       apply(b,   seq=B,    baseline={0})      // theirs, off base
// then asserts:
//   - RECOVERY (the engine's guarantee): the version visible to {0,A}
//     recovers `a` byte-for-byte, {0,B} recovers `b`, {0} recovers base.
//   - The merged alive view ({0,A,B}) equals `expected_merged` when
//     supplied, or contains both `contains_a` / `contains_b` substrings
//     (concurrent inserts at one locus keep both copies — collapsing
//     them is the merge-commit's diff-to-outcome step, not storage).
//

#include "WEAVE.h"

#include "abc/PRO.h"
#include "abc/RAP.h"
#include "abc/TEST.h"

#define WEAVE_TEST_BASE 0u
#define WEAVE_TEST_A    0xA0A0A0A0u
#define WEAVE_TEST_B    0xB0B0B0B0u

// --- Helpers ----------------------------------------------------------

#define WEAVE_CSV(name, lit)                                              \
    u8cs name = {(u8cp)(lit), (u8cp)(lit) + ((lit) ? strlen(lit) : 0)}

//  Dump a weave's tokens with inrm + bytes to stderr.  Used by
//  failure paths in test cases to localise WEAVE pipeline bugs.
static void weave_dump_tokens(char const *label, weave const *w) {
    fprintf(stderr, "  %s tokens:\n", label);
    weavecur c;
    WEAVECurInit(&c, w);
    u32 i = 0;
    while (WEAVECurNext(&c)) {
        fprintf(stderr, "    [%u] seq=%08x pos=%u rm={", i++, c.seq, c.pos);
        for (u32 k = 0; k < c.nr; k++)
            fprintf(stderr, "%08x%s", c.rset[k], k + 1 < c.nr ? "," : "");
        fprintf(stderr, "} \"");
        u8cp tp = (u8cp)c.text[0], te = (u8cp)c.text[1];
        for (u8cp q = tp; q < te && q < tp + 16; q++) {
            u8 ch = *q;
            if (ch >= 0x20 && ch < 0x7f) fputc(ch, stderr);
            else fprintf(stderr, "\\x%02x", ch);
        }
        fprintf(stderr, "\"\n");
    }
}

//  Substring search inside a u8s byte stream.
static b8 weave_contains(u8s const got, char const *needle) {
    size_t nlen = needle ? strlen(needle) : 0;
    if (nlen == 0) return YES;
    size_t glen = (size_t)$len(got);
    if (glen < nlen) return NO;
    for (size_t i = 0; i + nlen <= glen; i++) {
        if (memcmp(got[0] + i, needle, nlen) == 0) return YES;
    }
    return NO;
}

//  Find the first ALIVE token whose hashlet is `h`; return its inserter
//  seq (or 0xFFFFFFFF if not found).
static u32 weave_alive_seq(weave const *w, u64 h) {
    weavecur c;
    WEAVECurInit(&c, w);
    while (WEAVECurNext(&c)) {
        if (c.nr != 0) continue;
        if (RAPHash(c.text) == h) return c.seq;
    }
    return 0xFFFFFFFFu;
}

//  Slice byte-equality (works around the inability to assign u8cs).
static b8 weave_slice_eq_lit(u8s const got, char const *want) {
    size_t wlen = want ? strlen(want) : 0;
    size_t glen = (size_t)$len(got);
    if (glen != wlen) return NO;
    if (wlen == 0) return YES;
    return memcmp(got[0], want, wlen) == 0;
}

//  Count occurrences of `needle` in `got`.
static u32 weave_count(u8s const got, char const *needle) {
    size_t nlen = needle ? strlen(needle) : 0;
    if (nlen == 0) return 0;
    size_t glen = (size_t)$len(got);
    u32 n = 0;
    for (size_t i = 0; i + nlen <= glen; i++)
        if (memcmp(got[0] + i, needle, nlen) == 0) n++;
    return n;
}

// --- Single-weave replay primitives -----------------------------------

//  Membership predicate for a WEAVEApply baseline (a commit's parent
//  closure, here a small explicit seq set).
typedef struct { u32 const *set; u32 n; } w01_pred_ctx;
static b8 w01_pred(u32 seq, void *vctx) {
    w01_pred_ctx const *c = vctx;
    return WEAVESetHas(c->set, c->n, seq) ? YES : NO;
}

//  Apply one commit (`content` stamped `seq`) to `src`, producing `dst`.
//  `base`/`nbase` is the parent closure (NULL ⇒ root, empty baseline).
//  `nu` is a reusable scratch weave for the pre-tokenized content.
static ok64 w01_apply(weave *dst, weave const *src, weave *nu,
                      u8cs content, u8cs ext, u32 seq,
                      u32 const *base, u32 nbase) {
    sane(dst && src && nu);
    call(WEAVEFromBlob, nu, content, ext, seq);
    w01_pred_ctx pc = {base, nbase};
    call(WEAVEApply, dst, src, nu, seq, base ? w01_pred : NULL, &pc);
    done;
}

//  Recover the version visible to scope `set`: alive tokens whose
//  inserter is in `set` and that no in-scope commit removed, in weave
//  order, into `out` (reset on entry).
static ok64 weave_recover_scope(u8bp out, weave const *w,
                                u32 const *set, u32 nset) {
    sane(out && w);
    u8bReset(out);
    weavecur c;
    WEAVECurInit(&c, w);
    while (WEAVECurNext(&c)) {
        if (!WEAVESetHas(set, nset, c.seq)) continue;
        b8 dead = NO;
        for (u32 k = 0; k < c.nr; k++)
            if (WEAVESetHas(set, nset, c.rset[k])) { dead = YES; break; }
        if (!dead) call(u8bFeed, out, c.text);
    }
    if (c.bad) return WEAVEFAIL;
    done;
}

// --- Test case shape --------------------------------------------------

typedef struct {
    char const *name;
    char const *base;
    char const *a;
    char const *b;
    //  Two acceptance modes for the MERGED alive view, mutually
    //  exclusive:
    //    expected_merged != NULL : alive byte stream must equal it.
    //    contains_a, contains_b  : alive byte stream must contain both
    //                              substrings (a locus where both sides
    //                              keep divergent / duplicate tokens).
    char const *expected_merged;
    char const *contains_a;
    char const *contains_b;
} WEAVECase;

//  Build the single weave for a case (base → a → b) into `*W` (one of
//  `w0`/`w1`, ping-ponged), using `nu` as content scratch.
static ok64 weave_case_build(WEAVECase const *c, u8cs ext,
                             weave *w0, weave *w1, weave *nu, weave **W) {
    sane(c && W);
    WEAVE_CSV(base,  c->base);
    WEAVE_CSV(adata, c->a);
    WEAVE_CSV(bdata, c->b);
    weave *cur = w0, *nxt = w1;
    u32 bs[1] = {WEAVE_TEST_BASE};
    call(w01_apply, nxt, cur, nu, base,  ext, WEAVE_TEST_BASE, NULL, 0);
    { weave *t = cur; cur = nxt; nxt = t; }
    call(w01_apply, nxt, cur, nu, adata, ext, WEAVE_TEST_A, bs, 1);
    { weave *t = cur; cur = nxt; nxt = t; }
    call(w01_apply, nxt, cur, nu, bdata, ext, WEAVE_TEST_B, bs, 1);
    { weave *t = cur; cur = nxt; nxt = t; }
    *W = cur;
    done;
}

//  Per-case execution.
static ok64 weave_run(WEAVECase const *c) {
    sane(1);
    fprintf(stderr, "  %s...", c->name);

    WEAVE_CSV(ext, "c");
    weave w0 = {}, w1 = {}, nu = {};
    Bu8 outbuf = {};
    call(WEAVEInit, &w0);
    call(WEAVEInit, &w1);
    call(WEAVEInit, &nu);
    call(u8bMap, outbuf, 1UL << 16);

    weave *W = NULL;
    call(weave_case_build, c, ext, &w0, &w1, &nu, &W);

    //  RECOVERY: each side recovers its exact content from the one weave.
    u32 sb_base[1] = {WEAVE_TEST_BASE};
    u32 sa[2] = {WEAVE_TEST_BASE, WEAVE_TEST_A};
    u32 sb[2] = {WEAVE_TEST_BASE, WEAVE_TEST_B};
    struct { u32 const *set; u32 n; char const *want; char const *who; } recs[] = {
        {sb_base, 1, c->base, "base"},
        {sa,      2, c->a,    "ours"},
        {sb,      2, c->b,    "theirs"},
    };
    for (u32 r = 0; r < 3; r++) {
        call(weave_recover_scope, outbuf, W, recs[r].set, recs[r].n);
        u8s g = {u8bDataHead(outbuf), u8bDataHead(outbuf) + u8bDataLen(outbuf)};
        if (!weave_slice_eq_lit(g, recs[r].want)) {
            fprintf(stderr, "\n FAIL: %s recover %s\n  got:  '%.*s'\n  want: '%s'\n",
                    c->name, recs[r].who, (int)$len(g), (char *)g[0], recs[r].want);
            weave_dump_tokens("W", W);
            fail(TESTFAIL);
        }
    }

    //  MERGED view = alive under {0,A,B} (every seq present) = AliveBytes.
    call(WEAVEAliveBytes, W, outbuf);
    u8s got = {u8bDataHead(outbuf), u8bDataHead(outbuf) + u8bDataLen(outbuf)};
    if (c->expected_merged) {
        if (!weave_slice_eq_lit(got, c->expected_merged)) {
            fprintf(stderr, "\n FAIL: %s merged mismatch\n  got:  '%.*s'\n  want: '%s'\n",
                    c->name, (int)$len(got), (char *)got[0], c->expected_merged);
            weave_dump_tokens("W", W);
            fail(TESTFAIL);
        }
    } else if (c->contains_a || c->contains_b) {
        if (c->contains_a && !weave_contains(got, c->contains_a)) {
            fprintf(stderr, "\n FAIL: %s alive missing a-side '%s'\n  got: '%.*s'\n",
                    c->name, c->contains_a, (int)$len(got), (char *)got[0]);
            fail(TESTFAIL);
        }
        if (c->contains_b && !weave_contains(got, c->contains_b)) {
            fprintf(stderr, "\n FAIL: %s alive missing b-side '%s'\n  got: '%.*s'\n",
                    c->name, c->contains_b, (int)$len(got), (char *)got[0]);
            fail(TESTFAIL);
        }
    }

    u8bUnMap(outbuf);
    WEAVEFree(&w0);
    WEAVEFree(&w1);
    WEAVEFree(&nu);
    fprintf(stderr, " ok\n");
    done;
}

// --- Identity case ----------------------------------------------------
//
//  Re-applying identical content against the base adds nothing: the
//  weave is idempotent and its alive bytes stay equal to the base (the
//  single-weave analog of merge(W, W) == W).
static ok64 weave_test_self(char const *base_str) {
    sane(1);
    WEAVE_CSV(ext,  "c");
    WEAVE_CSV(base, base_str);

    weave w0 = {}, w1 = {}, nu = {};
    Bu8 outbuf = {};
    call(WEAVEInit, &w0);
    call(WEAVEInit, &w1);
    call(WEAVEInit, &nu);
    call(u8bMap, outbuf, 1UL << 16);

    weave *W = &w0, *Wn = &w1;
    u32 bs[1] = {WEAVE_TEST_BASE};
    call(w01_apply, Wn, W, &nu, base, ext, WEAVE_TEST_BASE, NULL, 0);
    { weave *t = W; W = Wn; Wn = t; }
    call(w01_apply, Wn, W, &nu, base, ext, WEAVE_TEST_A, bs, 1);
    { weave *t = W; W = Wn; Wn = t; }

    call(WEAVEAliveBytes, W, outbuf);
    u8s got = {u8bDataHead(outbuf), u8bDataHead(outbuf) + u8bDataLen(outbuf)};
    if (!weave_slice_eq_lit(got, base_str)) {
        fprintf(stderr, "WEAVE01 self: got '%.*s' want '%s'\n",
                (int)$len(got), (char *)got[0], base_str);
        fail(TESTFAIL);
    }

    u8bUnMap(outbuf);
    WEAVEFree(&w0);
    WEAVEFree(&w1);
    WEAVEFree(&nu);
    done;
}

// --- Shared-base reconcile --------------------------------------------
//
//  base spine is shared by both sides (applied once, seq 0); ours appends
//  a line, theirs prepends one.  The shared base line must appear EXACTLY
//  once in the merged view (it is inserted once and never re-derived),
//  and its spine token keeps the base seq.
static ok64 weave_test_pre_lca(void) {
    sane(1);
    fprintf(stderr, "  pre_lca_in_reconcile...");
    WEAVE_CSV(ext,  "c");

    WEAVECase c = {
        "pre_lca", "int x = 1;\n",
        "int x = 1;\nint y = 2;\n",   // ours appends y
        "int z = 0;\nint x = 1;\n",   // theirs prepends z
        NULL, NULL, NULL,
    };
    weave w0 = {}, w1 = {}, nu = {};
    Bu8 outbuf = {};
    call(WEAVEInit, &w0);
    call(WEAVEInit, &w1);
    call(WEAVEInit, &nu);
    call(u8bMap, outbuf, 1UL << 16);

    weave *W = NULL;
    call(weave_case_build, &c, ext, &w0, &w1, &nu, &W);

    call(WEAVEAliveBytes, W, outbuf);
    u8s got = {u8bDataHead(outbuf), u8bDataHead(outbuf) + u8bDataLen(outbuf)};

    if (weave_count(got, "int x = 1;") != 1 ||
        !weave_contains(got, "int y = 2;") ||
        !weave_contains(got, "int z = 0;")) {
        fprintf(stderr, " FAIL: base x%u (want 1) / y / z; got '%.*s'\n",
                weave_count(got, "int x = 1;"),
                (int)$len(got), (char *)got[0]);
        fail(TESTFAIL);
    }

    //  The shared spine 'x' carries the base seq (inserted once).
    WEAVE_CSV(xtok, "x");
    if (weave_alive_seq(W, RAPHash(xtok)) != WEAVE_TEST_BASE) {
        fprintf(stderr, " FAIL: spine 'x' seq must be base\n");
        fail(TESTFAIL);
    }

    u8bUnMap(outbuf);
    WEAVEFree(&w0);
    WEAVEFree(&w1);
    WEAVEFree(&nu);
    fprintf(stderr, " ok\n");
    done;
}

// --- Cases ------------------------------------------------------------

static WEAVECase cases[] = {
    {"identical_no_change",
     "int x = 1;\n",
     "int x = 1;\n",
     "int x = 1;\n",
     "int x = 1;\n", NULL, NULL},

    {"disjoint_inserts",
     "int x = 1;\n",
     "int x = 1;\nint y = 2;\n",       // a appends y
     "int w = 0;\nint x = 1;\n",       // b prepends w
     "int w = 0;\nint x = 1;\nint y = 2;\n", NULL, NULL},

    {"a_deletes_b_keeps",
     "int x = 1;\nint y = 2;\n",
     "int x = 1;\n",                   // a deletes y line
     "int x = 1;\nint y = 2;\n",       // b unchanged
     "int x = 1;\n", NULL, NULL},

    {"both_delete_same",
     "int x = 1;\nint y = 2;\n",
     "int x = 1;\n",
     "int x = 1;\n",
     "int x = 1;\n", NULL, NULL},

    {"a_only_change",
     "int x = 1;\n",
     "int x = 42;\n",
     "int x = 1;\n",
     "int x = 42;\n", NULL, NULL},

    {"b_only_change",
     "int x = 1;\n",
     "int x = 1;\n",
     "int x = 42;\n",
     "int x = 42;\n", NULL, NULL},

    //  Concurrent edits at the same locus.  The weave keeps both alive
    //  sides (a's "10" and b's "20"); we only assert both survive.
    {"both_alive_divergent",
     "int x = 1;\n",
     "int x = 10;\n",
     "int x = 20;\n",
     NULL, "10", "20"},

    //  Concurrent IDENTICAL insert: both sides add the same line off the
    //  base.  They carry distinct birth-ids (seq A vs B), so the weave
    //  honestly keeps both — collapsing to one is the merge commit's
    //  diff-to-outcome step, not storage.  Each side still recovers
    //  exactly; the merged view contains the inserted line.
    {"both_insert_same_at_same_slot",
     "int x = 1;\n",
     "int x = 1;\nint y = 2;\n",
     "int x = 1;\nint y = 2;\n",
     NULL, "int y = 2;", NULL},

    //  Repeated-token LCS ambiguity + theirs DEL-then-INS + ours
    //  tail-append.  Base has two `"0"` tokens; theirs replaces the
    //  second with `"f(0)"`; ours appends `"TAG\n"`.  Merged alive:
    //  theirs's replacement applied AND ours's tail preserved.
    {"del_ins_plus_tail_repeats",
     "a=0;\nb=0;\n",
     "a=0;\nb=0;\nTAG\n",
     "a=0;\nb=f(0);\n",
     "a=0;\nb=f(0);\nTAG\n", NULL, NULL},

    //  Same shape with the DEL-then-INS inside a function body and an
    //  extra base zone to broaden the LCS-ambiguity surface.
    {"del_ins_in_func_plus_tail_zones",
     "a=0;\nb=0;\nc=0;\nm(){return 0;}\n",
     "a=0;\nb=0;\nc=0;\nm(){return 0;}\nT\n",
     "a=0;\nb=0;\nc=0;\nm(){return f(0);}\n",
     "a=0;\nb=0;\nc=0;\nm(){return f(0);}\nT\n", NULL, NULL},

    {NULL, NULL, NULL, NULL, NULL, NULL, NULL},
};

// --- Multi-version ours-chain replay vs single-version theirs ---------
//
//  Regression guard for the patch-15 "transposed inserted line vs spine
//  blank" merge bug.  ours is a linear chain replayed into the weave (a
//  deliberately bad order that reverts mid-chain, leaving a dead-dup
//  region); theirs is a single edit off the same base.  Invariant: the
//  last inserted code line `d;` lands BEFORE the spine blank in the
//  merged alive stream — never transposed after it.  In the single weave,
//  inserts anchor to existing tokens and never move, so this holds by
//  construction.
static ok64 weave_test_replay_insert_order(void) {
    sane(1);
    fprintf(stderr, "  replay_insert_order...");
    WEAVE_CSV(ext, "c");

    //  ours-chain replay order is BAD on purpose: t0 -> +s -> revert(t0)
    //  -> +s,u,d  (the revert makes a dead-duplicate region).
    char const *vers[] = {
        "m;\n\ng;\n",            // v0 (base, seq 0)
        "m;\ns;\n\ng;\n",        // +s;
        "m;\n\ng;\n",            // revert -> dead-dup
        "m;\ns;\nu;\nd;\n\ng;\n",// +s;u;d;
    };
    u32 nvers = (u32)(sizeof(vers) / sizeof(vers[0]));
    char const *theirs   = "m;\n\nG;\n";              // g; -> G;
    char const *expected = "m;\ns;\nu;\nd;\n\nG;\n";  // d; before blank

    weave w0 = {}, w1 = {}, nu = {};
    Bu8 outbuf = {};
    call(WEAVEInit, &w0);
    call(WEAVEInit, &w1);
    call(WEAVEInit, &nu);
    call(u8bMap, outbuf, 1UL << 16);

    weave *W = &w0, *Wn = &w1;

    //  base (seq 0).
    { WEAVE_CSV(v0, vers[0]);
      call(w01_apply, Wn, W, &nu, v0, ext, WEAVE_TEST_BASE, NULL, 0); }
    { weave *t = W; W = Wn; Wn = t; }

    //  ours chain: each version applies against the cumulative ours
    //  closure {0, ours_1 .. ours_{i-1}}.
    u32 ours_set[8];
    u32 nours = 0;
    ours_set[nours++] = WEAVE_TEST_BASE;
    for (u32 i = 1; i < nvers; i++) {
        WEAVE_CSV(vd, vers[i]);
        u32 sc = 0x10000000u + i;
        call(w01_apply, Wn, W, &nu, vd, ext, sc, ours_set, nours);
        { weave *t = W; W = Wn; Wn = t; }
        ours_set[nours++] = sc;
    }

    //  theirs: single edit off the base {0}.
    { WEAVE_CSV(td, theirs);
      u32 bs[1] = {WEAVE_TEST_BASE};
      call(w01_apply, Wn, W, &nu, td, ext, WEAVE_TEST_B, bs, 1); }
    { weave *t = W; W = Wn; Wn = t; }

    call(WEAVEAliveBytes, W, outbuf);
    u8s got = {u8bDataHead(outbuf), u8bDataHead(outbuf) + u8bDataLen(outbuf)};
    if (!weave_slice_eq_lit(got, expected)) {
        fprintf(stderr,
                " FAIL: replay merge mismatch\n  got:  '%.*s'\n  want: '%s'\n",
                (int)$len(got), (char *)got[0], expected);
        weave_dump_tokens("W", W);
        fail(TESTFAIL);
    }
    //  Explicit transposition guard: `d;` immediately before the blank.
    if (!weave_contains(got, "d;\n\n")) {
        fprintf(stderr, " FAIL: inserted 'd;' not immediately before blank\n");
        fail(TESTFAIL);
    }

    u8bUnMap(outbuf);
    WEAVEFree(&w0);
    WEAVEFree(&w1);
    WEAVEFree(&nu);
    fprintf(stderr, " ok\n");
    done;
}

ok64 WEAVEtest(void) {
    sane(1);

    call(weave_test_self, "int x = 1;\nint y = 2;\n");
    call(weave_test_self, "");
    call(weave_test_pre_lca);
    call(weave_test_replay_insert_order);

    for (WEAVECase *c = cases; c->name != NULL; c++) {
        call(weave_run, c);
    }

    done;
}

TEST(WEAVEtest);
