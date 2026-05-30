//
// WEAVE2 — deterministic, table-driven characterization test for the
// WEAVE engine (WEAVEFromBlob-equivalent / WEAVEDiff / WEAVEMerge).
//
// Unlike the RAPHash-driven WEAVE2 fuzzer (graf/fuzz/WEAVE2.c) this is a
// fixed table of small line-based revision DAGs.  For each row we build
// the per-line weaves, pick ONE weave w[check], and assert that EVERY
// revision in its closure recovers byte-for-byte from it — the core
// interleaved-delta property (any revision retrievable at any time).
// With set-valued provenance the exact per-token stamps are an
// implementation detail; recoverability is the invariant worth pinning,
// and it is unambiguous.
//
// INPUT FORMAT (one revision per line, lines joined by '\n'):
//   <parent-refs><space><content>
//   parent-refs : zero or more RON64 chars, each decoding (RON64_REV) to
//                 a 0-based index of a PRECEDING line; >1 = a merge.
//                 empty = root.
//   content     : one token PER CHAR, restricted to RON64 chars.
//
// MODEL (build per-line weaves w[0..N); input order is a valid topo order):
//   0 parents  : w[i] = w2_blob(content, src=i)         (one tok per char)
//   1 parent p : w[i] = WEAVEDiff(w[p], w2_blob(content,i), src=i)
//   >=2 parents: wm = WEAVEMerge(w[p],w[q]); fold extra parents pairwise;
//                w[i] = WEAVEDiff(wm, w2_blob(content,i), src=i)
//   src = the line index (root = line 0 = 0).  NOT i+1.
//

#include "WEAVE.h"

#include <string.h>

#include "abc/PRO.h"
#include "abc/RON.h"
#include "abc/TEST.h"

#define W2_MAX_LINES   32u
#define W2_MAX_CONTENT 64u

// --- Per-line parsed record ----------------------------------------

typedef struct {
    u8cs content;            // raw content bytes (one token each)
    u32  par[W2_MAX_LINES];  // parent line indices
    u32  npar;               // number of parents
} w2_line;

// --- One-token-per-byte blob weave ---------------------------------
//
//  Force token granularity to exactly one byte, stamped I={src}, R={}.

static ok64 w2_blob(weave *w, u8cs content, u32 src) {
    sane(w);
    weavebld b;
    WEAVEBldInit(&b, w);
    u32 n = (u32)$len(content);
    u32 one[1] = {src};
    u32cs is = {one, one + 1};
    u32cs rs = {NULL, NULL};
    for (u32 i = 0; i < n; i++) {
        u8csc seg = {content[0] + i, content[0] + i + 1};
        call(WEAVEBldPut, &b, seg, is, rs);
    }
    done;
}

// --- Parse the line-based input ------------------------------------

static ok64 w2_parse(u8cs input, w2_line *lines, u32 *nlines) {
    sane($ok(input));
    u32  count = 0;
    u8cp p = input[0];
    u8cp e = input[1];
    while (p < e) {
        if (count >= W2_MAX_LINES) fail(WEAVEFAIL);
        u8cp eol = p;
        while (eol < e && *eol != '\n') eol++;
        u8cp sp = p;
        while (sp < eol && *sp != ' ') sp++;
        if (sp == eol) fail(WEAVEFAIL);   // no space → malformed

        w2_line *ln = &lines[count];
        ln->npar = 0;
        for (u8cp r = p; r < sp; r++) {
            u8 v = RON64_REV[*r];
            if (v == 0xff) fail(WEAVEFAIL);     // not RON64
            if (v >= count) fail(WEAVEFAIL);    // ref >= line index
            if (ln->npar >= W2_MAX_LINES) fail(WEAVEFAIL);
            ln->par[ln->npar++] = v;
        }
        u8cp cb = sp + 1;
        if ((u32)(eol - cb) > W2_MAX_CONTENT) fail(WEAVEFAIL);
        for (u8cp r = cb; r < eol; r++)
            if (RON64_REV[*r] == 0xff) fail(WEAVEFAIL);
        ln->content[0] = cb;
        ln->content[1] = eol;
        count++;
        p = (eol < e) ? eol + 1 : eol;
    }
    if (count == 0) fail(WEAVEFAIL);
    *nlines = count;
    done;
}

// --- Ancestor closure ----------------------------------------------

static void w2_closure(w2_line const *lines, u32 n, u32 start, u8 *anc) {
    (void)n;
    anc[start] = 1;
    for (u32 i = start + 1; i-- > 0;) {
        if (!anc[i]) continue;
        for (u32 k = 0; k < lines[i].npar; k++) anc[lines[i].par[k]] = 1;
    }
}

// --- Normalize merge parents (dedupe, drop ancestors-of-other-parents,
//     topo-sort ascending) — real merge commits never list a parent that
//     is an ancestor of another, and folding a minimal ordered set avoids
//     re-merging a weave whose tokens another parent already subsumes.
static u32 w2_norm_parents(w2_line const *lines, u32 n,
                           u32 const *par, u32 npar, u32 *mpar) {
    u32 uniq[W2_MAX_LINES], nu = 0;
    for (u32 i = 0; i < npar; i++) {
        b8 dup = NO;
        for (u32 j = 0; j < nu; j++) if (uniq[j] == par[i]) { dup = YES; break; }
        if (!dup) uniq[nu++] = par[i];
    }
    u32 nm = 0;
    for (u32 i = 0; i < nu; i++) {
        u32 p = uniq[i];
        b8 anc_of_other = NO;
        for (u32 j = 0; j < nu && !anc_of_other; j++) {
            if (j == i) continue;
            u8 a[W2_MAX_LINES];
            memset(a, 0, n);
            w2_closure(lines, n, uniq[j], a);
            if (a[p]) anc_of_other = YES;
        }
        if (!anc_of_other) mpar[nm++] = p;
    }
    for (u32 i = 1; i < nm; i++) {
        u32 v = mpar[i], k = i;
        while (k > 0 && mpar[k - 1] > v) { mpar[k] = mpar[k - 1]; k--; }
        mpar[k] = v;
    }
    return nm;
}

// --- Recover rev `a` from weave `w` (keep token iff some inserter in
//     anc AND no remover in anc) ------------------------------------

static ok64 w2_recover(u8bp out, weave const *w, u8 const *anc, u32 n_idx) {
    sane(out && w);
    u8bReset(out);
    weavecur c;
    WEAVECurInit(&c, w);
    while (WEAVECurNext(&c)) {
        b8 in_ok = NO;
        for (u32 k = 0; k < c.ni; k++) {
            u32 in = c.iset[k];
            if (in < n_idx && anc[in]) { in_ok = YES; break; }
        }
        b8 rm_ok = YES;
        for (u32 k = 0; k < c.nr; k++) {
            u32 rm = c.rset[k];
            if (rm < n_idx && anc[rm]) { rm_ok = NO; break; }
        }
        if (in_ok && rm_ok) call(u8bFeed, out, c.text);
    }
    if (c.bad) return WEAVEFAIL;
    done;
}

// --- Test case shape -----------------------------------------------

typedef struct {
    char const *name;
    char const *input;     // multi-line DAG string
    u32         check;     // verify EVERY revision in this weave's closure
} W2Case;

//  Build all per-line weaves for one case; verify w[check] recovers each
//  revision in its closure byte-for-byte.
static ok64 w2_build(W2Case const *c) {
    sane(1);
    u8cs input = {(u8cp)c->input, (u8cp)c->input + strlen(c->input)};

    static _Thread_local w2_line lines[W2_MAX_LINES];
    u32 n = 0;
    call(w2_parse, input, lines, &n);

    weave w[W2_MAX_LINES] = {};
    weave nu = {}, wm = {};
    Bu8 out = {};
    for (u32 i = 0; i < n; i++) call(WEAVEInit, &w[i]);
    call(WEAVEInit, &nu);
    call(WEAVEInit, &wm);
    call(u8bMap, out, 1UL << 16);

    ok64 ret = OK;
    for (u32 i = 0; ret == OK && i < n; i++) {
        w2_line *ln = &lines[i];
        u32 s = i;
        u32 mpar[W2_MAX_LINES];
        u32 nm = (ln->npar == 0) ? 0
               : w2_norm_parents(lines, n, ln->par, ln->npar, mpar);
        if (ln->npar == 0) {
            ret = w2_blob(&w[i], ln->content, s);
        } else if (nm == 1) {
            ret = w2_blob(&nu, ln->content, s);
            if (ret == OK) ret = WEAVEDiff(&w[i], &w[mpar[0]], &nu, s);
        } else {
            ret = WEAVEMerge(&wm, &w[mpar[0]], &w[mpar[1]]);
            for (u32 k = 2; ret == OK && k < nm; k++) {
                ret = WEAVEMerge(&nu, &wm, &w[mpar[k]]);
                if (ret == OK) {
                    weave tmp;
                    memcpy(&tmp, &wm, sizeof(weave));
                    memcpy(&wm, &nu, sizeof(weave));
                    memcpy(&nu, &tmp, sizeof(weave));
                }
            }
            if (ret == OK) ret = w2_blob(&nu, ln->content, s);
            if (ret == OK) ret = WEAVEDiff(&w[i], &wm, &nu, s);
        }
    }

    //  Recover every revision in check's closure from w[check] — each
    //  rev `a` is recovered "as of a", i.e. using a's OWN closure.
    if (ret == OK) {
        u8 reach[W2_MAX_LINES];
        memset(reach, 0, n);
        w2_closure(lines, n, c->check, reach);
        for (u32 a = 0; ret == OK && a < n; a++) {
            if (!reach[a]) continue;
            u8 anc[W2_MAX_LINES];
            memset(anc, 0, n);
            w2_closure(lines, n, a, anc);
            ret = w2_recover(out, &w[c->check], anc, n);
            if (ret != OK) break;
            a_dup(u8c, got, u8bDataC(out));
            u8cs want = {};
            u8csMv(want, lines[a].content);
            b8 okrec = $len(got) == $len(want) &&
                       ($len(got) == 0 ||
                        memcmp(got[0], want[0], (size_t)$len(got)) == 0);
            if (!okrec) {
                fprintf(stderr,
                        "\n FAIL: %s w[%u] recover rev %u\n"
                        "  got:  '%.*s'\n  want: '%.*s'\n",
                        c->name, c->check, a,
                        (int)$len(got), (char *)got[0],
                        (int)$len(want), (char *)want[0]);
                ret = TESTFAIL;
            }
        }
    }

    u8bUnMap(out);
    for (u32 i = 0; i < n; i++) WEAVEFree(&w[i]);
    WEAVEFree(&nu);
    WEAVEFree(&wm);
    if (ret != OK) fail(ret);
    done;
}

static ok64 w2_run(W2Case const *c) {
    sane(1);
    fprintf(stderr, "  %s...", c->name);
    call(w2_build, c);
    fprintf(stderr, " ok\n");
    done;
}

// --- Cases ---------------------------------------------------------

static W2Case cases[] = {
    {"linear_root",            " abc\n0 abcd",                      0u},
    {"linear_child_insert",    " abc\n0 abcd",                      1u},
    {"linear_delete",          " abc\n0 ac",                       1u},
    {"self_merge_append",      " aZb\n00 aZbc",                     1u},
    //  Concurrent identical insert: lines 1,2 each append 'd' off the
    //  root, line 3 merges them.  Both inserters must be recoverable.
    {"dedup_merge_post_fork",  " ab\n0 abd\n0 abd\n12 abd",         3u},
    {"divergent_merge",        " abcd\n0 Xabcd\n0 abcdY\n12 XabcdY",3u},
    //  Dead token unique to one merge side must survive so rev0 ("c")
    //  recovers (graf/fuzz/WEAVE2; test/TRIANGLE.todo.md).
    {"merge_keeps_dead_token", " c\n Zb\n0 Z\n12 ",                 3u},
    //  Fuzz-found (graf/fuzz/WEAVE2 crash-cb902…): line1's tokens are
    //  deleted by line2 (in the {1,0} merge) AND by line3, so the merge
    //  of w[2] and w[3] sees the SAME line1 token dead on both sides with
    //  different removers.  They must reconcile to one token (removers
    //  unioned) — else rev1 recovers a duplicate ("cdec" vs "cde").
    {"merge_same_token_dead_both_sides",
     " ab0a\n cde\n10 bcde\n1 absde\n23 cd",                        4u},
    //  Fuzz-found (graf/fuzz/WEAVE2, crash-d924…, hand-minimized).
    //  L0,L1 both "a"; L2 merges them ("3" replacing the unioned a) so
    //  w[2] = [3·{2}, a·{0,1}/{2}] (3 BEFORE a).  L3 re-merges {1,2,2}:
    //  merge(w1,w2) reconciled w1's alive 'a' with w2's dead a{0,1} but
    //  emitted it at the OURS position, flipping the order to [a, 3] — so
    //  the re-fold against w2 (now transposed) couldn't align the second
    //  'a' and duplicated it → rev0 recovered "aa".  Fixed by emitting
    //  reconciled tokens at the THEIRS position (merge walks theirs in
    //  order; ours-only tokens lead).
    {"remerge_order_preserved",
     " a\n a\n01 3\n122 ",                                          3u},
    //  Fuzz-found (graf/fuzz/WEAVE2, crash-11511…, hand-minimized).
    //  w[2]="ab"=[a·{1}, b·{2}].  In merge(w[3],w[4]) the ours-only 'b'
    //  sits AFTER the reconciled 'a' in ours' order, but "ours-only
    //  first" emitted it ahead → rev2 recovered "ba".  Fixed by the
    //  anchor-segmented merge walk (reconciled tokens anchor the order;
    //  between anchors ours-only precede theirs-only).
    {"merge_order_ours_only_after_anchor",
     " a\n a\n1 ab\n02 \n1 \n34 ",                                  5u},
    {NULL, NULL, 0u},
};

ok64 WEAVE2test(void) {
    sane(1);
    for (W2Case *c = cases; c->name != NULL; c++)
        call(w2_run, c);
    done;
}

TEST(WEAVE2test);
