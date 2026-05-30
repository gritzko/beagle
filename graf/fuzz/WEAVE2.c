//
// WEAVE2 fuzz test — multi-revision DAG round-trip property of the
// WEAVE engine (WEAVEFromBlob / WEAVEDiff / WEAVEMerge).
//
// Input format (line-based, one revision per line):
//   <parent-refs><space><content>\n
//   <parent-refs> = zero or more RON base64 chars; each decoded to its
//                   0..63 value is a 0-based index of a PRECEDING line
//                   (must be < this line's index).  Zero chars = root.
//   <content>     = raw bytes; EACH BYTE is ONE token.
//
// Example DAG (5 revisions):
//   "" "abc"      line 0, root
//   "0" "abcd"    line 1, parent 0
//   "1" "bcde"    line 2, parent 1
//   "1" "abcde"   line 3, parent 1
//   "23" "bcde"   line 4, merge of 2 and 3
//
// Model — build per-line weaves w[0..N), input order is a valid topo
// order (parents always precede).  Each line i stamps its delta with
// insertion src = i (the line index; root = line 0 = 0):
//   * 0 parents: w[i] = blob(C_i, src=i).
//   * 1 parent p: w[i] = WEAVEDiff(w[p], blob(C_i, src=i), src=i).
//   * >=2 parents (p,q,...): wm = WEAVEMerge(w[p], w[q]) (fold extra
//     parents pairwise), then w[i] = WEAVEDiff(wm, blob(C_i, src=i),
//     src=i) so the explicit content is authoritative.
//
// After building w[i], for EACH ancestor a in anc*(i) (= {i} U
// transitive parents, through merges included), recover rev a from
// w[i] by walking tokens in weave order and keeping a token iff
//   in  in anc*(a)               (introduced no later than a), AND
//   rm == 0 || rm NOT in anc*(a) (not deleted as of a).
// The kept bytes must equal content_a byte-for-byte.  The provided
// input content IS the expected result — there is no separate
// reference; the weave must recover what was put in.
//
// Tokenization is done locally (one tok32 per input byte, hashlet =
// RAPHash of the single byte) so repeated letters collide on the LCS.
//

#include "WEAVE.h"

#include <string.h>

#include "abc/PRO.h"
#include "abc/RAP.h"
#include "abc/RON.h"
#include "abc/TEST.h"
#include "dog/tok/TOK.h"

#define W2_MAX_LINES   64u
#define W2_MAX_CONTENT 256u
#define W2_FUZZ_MAX    8192u

//  Per-line parsed record.  `src` for line i is just i (small, distinct,
//  never 0-as-NCA-bootstrap collides because anc* membership is by index).
typedef struct {
    u8cs content;            // raw content bytes (one token each)
    u32  par[W2_MAX_LINES];  // parent line indices
    u32  npar;               // number of parents
} w2_line;

// --- One-token-per-byte blob weave ---------------------------------
//
//  Mirror WEAVEFromBlob's array writes but force token granularity to
//  exactly one byte: tok32Pack('S', cumulative_end), hashlet =
//  RAPHash(single-byte slice), inrm = {in=src, rm=0}.

static ok64 w2_blob(weave *w, u8cs content, u32 src) {
    sane(w);
    weavebld b;
    WEAVEBldInit(&b, w);
    u32 n = (u32)$len(content);
    u32cs rs = {NULL, NULL};
    for (u32 i = 0; i < n; i++) {
        u8csc seg = {content[0] + i, content[0] + i + 1};
        call(WEAVEBldPut, &b, seg, src, i, rs);   // seq=src, pos=token index
    }
    done;
}

// --- Ancestor closure ----------------------------------------------
//
//  Mark anc[] (size N, zeroed by caller) with 1 for every index in
//  anc*(start) — {start} U transitive parents.  Parents always precede,
//  so a single descending sweep closes the set.

static void w2_closure(w2_line const *lines, u32 n, u32 start, u8 *anc) {
    anc[start] = 1;
    for (u32 i = start + 1; i-- > 0;) {
        if (!anc[i]) continue;
        for (u32 k = 0; k < lines[i].npar; k++) anc[lines[i].par[k]] = 1;
    }
}

// --- Normalize merge parents --------------------------------------
//
//  Real merge commits never list a parent that is an ancestor of another
//  parent (git simplifies them away).  Dedupe `par`, drop any parent that
//  is in another parent's ancestor closure, and topo-sort the rest
//  (ascending index = topo order since parents precede).  Folding a
//  minimal, ordered parent set avoids re-merging a weave with one whose
//  tokens it already subsumes.  Returns the count written to `mpar`.

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
            if (a[p]) anc_of_other = YES;   // p is an ancestor of uniq[j]
        }
        if (!anc_of_other) mpar[nm++] = p;
    }
    for (u32 i = 1; i < nm; i++) {          // insertion sort (topo = ascending)
        u32 v = mpar[i], k = i;
        while (k > 0 && mpar[k - 1] > v) { mpar[k] = mpar[k - 1]; k--; }
        mpar[k] = v;
    }
    return nm;
}

// --- Recover rev `a` from weave `w[i]` -----------------------------
//
//  Keep token t iff t.in in anc(a) AND (t.rm == 0 || t.rm not in anc(a)).
//  `in` is the line index that inserted the token (root = line 0 = 0);
//  `rm` is 0 when never removed, else the line index that removed it.
//  rm==0 is an unambiguous "not removed" sentinel: line 0 is the root,
//  which only inserts, so no real removal ever carries rm==0.
//  `anc` is the closure of `a` over indices [0..N).

static ok64 w2_recover(u8bp out, weave const *w, u8 const *anc, u32 n_idx) {
    sane(out && w);
    u8bReset(out);
    weavecur c;
    WEAVECurInit(&c, w);
    while (WEAVECurNext(&c)) {
        //  Inserted by a line in a's history.
        b8 in_ok = (c.seq < n_idx && anc[c.seq]);
        //  Alive unless removed by some line in a's history.
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

// --- Parse the line-based input ------------------------------------
//
//  Returns OK and fills lines[0..*nlines) on success; returns non-OK
//  (caller treats as "reject, done") on any malformed input.

static ok64 w2_parse(u8cs input, w2_line *lines, u32 *nlines) {
    sane($ok(input));
    u32  count = 0;
    u8cp p = input[0];
    u8cp e = input[1];
    while (p < e) {
        if (count >= W2_MAX_LINES) return WEAVEFAIL;
        u8cp eol = p;
        while (eol < e && *eol != '\n') eol++;
        //  Find the space separating refs from content.
        u8cp sp = p;
        while (sp < eol && *sp != ' ') sp++;
        if (sp == eol) return WEAVEFAIL;  // no space → malformed

        w2_line *ln = &lines[count];
        ln->npar = 0;
        for (u8cp r = p; r < sp; r++) {
            u8 v = RON64_REV[*r];
            if (v == 0xff) return WEAVEFAIL;        // not RON64
            if (v >= count) return WEAVEFAIL;       // ref >= line index
            if (ln->npar >= W2_MAX_LINES) return WEAVEFAIL;
            ln->par[ln->npar++] = v;
        }
        u8cp cb = sp + 1;
        if ((u32)(eol - cb) > W2_MAX_CONTENT) return WEAVEFAIL;
        //  Restrict content alphabet to RON64 chars only.
        for (u8cp r = cb; r < eol; r++)
            if (RON64_REV[*r] == 0xff) return WEAVEFAIL;
        ln->content[0] = cb;
        ln->content[1] = eol;
        count++;
        p = (eol < e) ? eol + 1 : eol;
    }
    if (count == 0) return WEAVEFAIL;
    *nlines = count;
    done;
}

// --- Parent-closure membership predicate (WEAVEApply baseline) -----

typedef struct { u8 const *set; u32 n; } w2_pred_ctx;

static b8 w2_pred(u32 seq, void *vctx) {
    w2_pred_ctx const *c = vctx;
    return (seq < c->n && c->set[seq]) ? YES : NO;
}

// --- Entry point ---------------------------------------------------

FUZZ(u8, WEAVE2fuzz) {
    sane(1);
    if ($empty(input) || $len(input) > W2_FUZZ_MAX) done;

    static _Thread_local w2_line lines[W2_MAX_LINES];
    u32 n = 0;
    if (w2_parse(input, lines, &n) != OK) done;

    //  ONE weave the whole DAG is replayed into (ping-pong buffers), plus
    //  a scratch weave holding each commit's pre-tokenized content.
    weave wa = {}, wb = {}, nu = {};
    Bu8   out = {};
    u8    panc[W2_MAX_LINES];   // parent closure (WEAVEApply baseline)
    u8    anc[W2_MAX_LINES];    // a rev's own closure (recovery)
    ok64  ret = OK;

    if ((ret = WEAVEInit(&wa)) != OK) goto out;
    if ((ret = WEAVEInit(&wb)) != OK) goto out;
    if ((ret = WEAVEInit(&nu)) != OK) goto out;
    if ((ret = u8bAlloc(out, (size_t)W2_MAX_LINES * W2_MAX_CONTENT + 16)) != OK)
        goto out;

    weave *W = &wa, *Wn = &wb;   // W = accumulated weave (starts empty)

    if (getenv("W2_DBG")) {
        fprintf(stderr, "\nDAG:");
        for (u32 z = 0; z < n; z++) {
            fprintf(stderr, " L%u(", z);
            for (u32 p = 0; p < lines[z].npar; p++)
                fputc(RON64_CHARS[lines[z].par[p] & 63], stderr);
            fprintf(stderr, ")=%.*s",
                    (int)$len(lines[z].content), lines[z].content[0]);
        }
        fprintf(stderr, "\n");
    }

    //  Replay every commit (topo order = input order) into the ONE weave:
    //  apply its content against its PARENT closure (union of all parents'
    //  closures) — insert/delete in place, never rebuilding a second weave
    //  to merge.  A root (no parents) applies against an empty baseline, so
    //  all of its content is inserted.
    for (u32 i = 0; i < n; i++) {
        w2_line *ln = &lines[i];
        ret = w2_blob(&nu, ln->content, i);   // content tokens (1/byte), seq=i
        if (ret != OK) must(0, "blob failed on valid input");

        memset(panc, 0, n);
        for (u32 k = 0; k < ln->npar; k++) panc[ln->par[k]] = 1;
        for (u32 j = i; j-- > 0;)
            if (panc[j])
                for (u32 k = 0; k < lines[j].npar; k++) panc[lines[j].par[k]] = 1;
        w2_pred_ctx pc = {panc, n};

        //  try() frees the op's BASS scratch (this driver is the call-chain
        //  top).  base = NULL for a root → empty baseline.
        try(WEAVEApply, Wn, W, &nu, i, ln->npar ? w2_pred : NULL, &pc);
        if (__ != OK) must(0, "WEAVEApply failed on valid input");

        weave *t = W; W = Wn; Wn = t;
    }

    if (getenv("W2_DBG")) {
        fprintf(stderr, "W:");
        weavecur dc; WEAVECurInit(&dc, W);
        while (WEAVECurNext(&dc)) {
            fprintf(stderr, " %.*s%c.%c/", (int)$len(dc.text), dc.text[0],
                    RON64_CHARS[dc.seq & 63], RON64_CHARS[dc.pos & 63]);
            for (u32 z = 0; z < dc.nr; z++) fputc(RON64_CHARS[dc.rset[z] & 63], stderr);
        }
        fprintf(stderr, "\n");
    }

    //  Every revision must recover its EXACT content from the ONE weave by
    //  filtering tokens through its own closure (inserter reachable, no
    //  remover reachable).  Later commits never disturb an earlier rev's
    //  view, so reading them all from the final weave is "as of a".  A
    //  mismatch is a real finding (the weave lost/corrupted a recoverable
    //  revision), not a property to weaken.
    for (u32 a = 0; a < n; a++) {
        memset(anc, 0, n);
        w2_closure(lines, n, a, anc);
        ret = w2_recover(out, W, anc, n);
        if (ret != OK) must(0, "recover failed on valid input");

        u8 **gd = u8bData(out);
        u32  glen = (u32)$len(gd);
        u8cp wlo = lines[a].content[0];
        u8cp whi = lines[a].content[1];
        u32  wlen = (u32)(whi - wlo);
        b8 okrec = glen == wlen &&
             (wlen == 0 || memcmp(gd[0], wlo, (size_t)wlen) == 0);
        if (!okrec && getenv("W2_DBG")) {
            fprintf(stderr, "\nFAIL recover rev a=%u: anc={", a);
            for (u32 z = 0; z < n; z++) if (anc[z]) fprintf(stderr, "%u", z);
            fprintf(stderr, "} want='%.*s' got='%.*s'\n",
                    (int)wlen, wlo, (int)glen, gd[0]);
        }
        must(okrec, "weave failed to recover a revision's content");
    }

out:
    u8bFree(out);
    WEAVEFree(&nu);
    WEAVEFree(&wa);
    WEAVEFree(&wb);
    if (ret != OK) fail(ret);
    done;
}
