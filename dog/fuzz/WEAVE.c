//
// WEAVE fuzz (DOG-003) — multi-revision DAG content-recovery property of
// the columnar weave engine (WEAVENext / WEAVEMerge / WEAVEScope /
// WEAVEProduce).  Ported from graf/fuzz/WEAVE2 to the new identity-keyed
// API; this is where the DIS-003 "abac/aabc" holdout class is re-fuzzed.
//
// Input format (line-based, one revision per line):
//   <parent-refs><space><content>\n
//   <parent-refs> = zero or more RON64 chars; each decoded value is the
//                   0-based index of a PRECEDING line (< this line's idx).
//   <content>     = RON64 bytes, NON-EMPTY (small alphabet => repeated,
//                   hence identical, tokens — the positional stress).
//
// Restrictions vs graf/WEAVE2 (the new API has no apply-against-closure):
//   * SINGLE ROOT — only line 0 has no parents; every later line has >=1.
//     Multiple roots would be multiple file origins, but the from-blob
//     path stamps its own commits[0] spine, so distinct roots can't share
//     one; merging them is out of model.
//   * NON-EMPTY content — an empty revision makes WEAVENext take the
//     from-blob path on the CHILD (it sees an empty parent), which resets
//     the commit table and severs ancestry.  Empty revisions are a
//     separate concern; this property assumes every revision has content.
//
// Model — build one weave per line w[i] (each a serialized 'W' blob the
// next fold parses, all kept alive for the whole DAG):
//   * line 0:        w[0] = WEAVENext(NULL, lineform(C0), c, 0).
//   * 1 parent p:    w[i] = WEAVENext(w[p], lineform(Ci), c, i).
//   * >=2 parents:   fold-merge the (normalized) parents pairwise via
//                    WEAVEMerge, then WEAVENext(merged, lineform(Ci), c, i)
//                    so the explicit content is authoritative.
// commit-id = w2_cid(line index): an ARBITRARY, NON-monotonic 64-bit id
//   (SplitMix64 of the index) so a descendant may carry a SMALLER id than its
//   base — the real-hashlet order DIS-043's line-index ids accidentally hid
//   (DIS-044).  Line 0 still owns commits[0] (the spine slot) whatever its id.
//
// Each content byte b becomes the LINE "b\n" so the line-coherent
// tokenizer yields one token per byte and repeated bytes produce
// IDENTICAL tokens — the positional ambiguity that exposed the DIS-003
// holdout.  Recovery is compared against the same line form.
//
// DIS-045 TEETH: the content byte '_' lineforms to a BLANK line "\n"
// (see w2_lineform), so a blank line's bytes are IDENTICAL to every code
// line's trailing EOL.  Strings mixing '_' with other bytes place a
// re-stampable blank line next to a code-line EOL at insert/merge
// boundaries, making the insert-vs-blank mis-anchor class (DIS-003/045)
// reachable by the fuzzer.
//
// PROPERTY (content recovery): for every line i and every ancestor a in
// anc*(i), WEAVEProduce(w[i], scope(anc*(a))) must reproduce lineform(Ca)
// byte-for-byte.  The provided input content IS the oracle — a mismatch
// is a real finding (a recoverable revision was lost or corrupted), not a
// property to weaken.
//

#include "dog/WEAVE.h"

#include <string.h>

#include "abc/PRO.h"
#include "abc/RON.h"
#include "abc/TEST.h"

#define W2_MAX_LINES   64u
#define W2_MAX_CONTENT 256u
#define W2_FUZZ_MAX    8192u
#define W2_WEAVE_CAP   (1UL << 19)            // 512 KiB per per-line weave blob
#define W2_LINE_CAP    (2u * W2_MAX_CONTENT + 16u)

typedef struct {
    u8cs content;            // raw content bytes (one line each)
    u32  par[W2_MAX_LINES];  // parent line indices
    u32  npar;               // number of parents
} w2_line;

// --- commit-id per line: ARBITRARY, NON-monotonic (DIS-044) ---------
//  DIS-043's order key was the raw commit-id; the line-index id used here
//  was accidentally a valid causal order (parents precede, so id(parent) <
//  id(child)) and HID the bug.  Real 60-bit hashlets are arbitrary: a
//  descendant may carry a SMALLER id than its base (and the base/spine a
//  LARGER one than its edits — the exact stranding toggle).  Map the line
//  index through the SplitMix64 finalizer (a bijection on u64, so distinct
//  indices give distinct ids — no false identity collision) to get an
//  arbitrary order where the spine routinely outranks its descendants.  Line
//  0 (the single root) still owns commits[0]/WEAVE_SPINE whatever its id, so
//  no special id-0 case is needed; both the build and the verify/scope sides
//  call this, so commits[]-table membership still matches by value.
static u64 w2_cid(u32 i) {
    u64 x = (u64)i + 0x9E3779B97F4A7C15ULL;   // avoid the i==0 fixed point
    x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ULL;
    x ^= x >> 27; x *= 0x94D049BB133111EBULL;
    x ^= x >> 31;
    return x;                                 // 64-bit bijection: ids distinct
}

// --- ancestor closure: mark anc*(start) over [0..n) -----------------
//  Parents always precede, so one descending sweep closes the set.
static void w2_closure(w2_line const *lines, u32 n, u32 start, u8 *anc) {
    (void)n;
    anc[start] = 1;
    for (u32 i = start + 1; i-- > 0;) {
        if (!anc[i]) continue;
        for (u32 k = 0; k < lines[i].npar; k++) anc[lines[i].par[k]] = 1;
    }
}

// --- normalize merge parents --------------------------------------
//  Dedupe, drop any parent in another parent's ancestor closure, topo-sort
//  ascending.  Folding a minimal, ordered set avoids re-merging a weave
//  with one whose tokens it already subsumes.
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

// --- parse the line-based input ------------------------------------
//  OK + lines[0..*nlines) on success; non-OK (reject, done) on malformed.
static ok64 w2_parse(u8cs input, w2_line *lines, u32 *nlines) {
    sane($ok(input));
    u32  count = 0;
    u8cp p = input[0];
    u8cp e = input[1];
    while (p < e) {
        if (count >= W2_MAX_LINES) return WEAVEFAIL;
        u8cp eol = p;
        while (eol < e && *eol != '\n') eol++;
        u8cp sp = p;
        while (sp < eol && *sp != ' ') sp++;
        if (sp == eol) return WEAVEFAIL;            // no space -> malformed

        w2_line *ln = &lines[count];
        ln->npar = 0;
        for (u8cp r = p; r < sp; r++) {
            u8 v = RON64_REV[*r];
            if (v == 0xff) return WEAVEFAIL;        // not RON64
            if (v >= count) return WEAVEFAIL;       // ref >= this line index
            if (ln->npar >= W2_MAX_LINES) return WEAVEFAIL;
            ln->par[ln->npar++] = v;
        }
        u8cp cb = sp + 1;
        if ((u32)(eol - cb) > W2_MAX_CONTENT) return WEAVEFAIL;
        if (cb == eol) return WEAVEFAIL;            // empty content -> reject
        for (u8cp r = cb; r < eol; r++)             // RON64 alphabet only
            if (RON64_REV[*r] == 0xff) return WEAVEFAIL;
        if (count > 0 && ln->npar == 0) return WEAVEFAIL;  // single-root
        ln->content[0] = cb;
        ln->content[1] = eol;
        count++;
        p = (eol < e) ? eol + 1 : eol;
    }
    if (count == 0) return WEAVEFAIL;
    *nlines = count;
    done;
}

// --- lineform: each content byte b -> the line "b\n" ----------------
//  DIS-045 TEETH: the RON64 char '_' emits a BLANK line "\n" (no content
//  char) instead of "_\n".  Blank lines are then first-class content tokens
//  whose bytes ("\n") are IDENTICAL to every code line's trailing EOL, so a
//  string like "a_b" -> "a\n" "\n" "b\n" places a re-stampable blank next to
//  a code-line EOL at insert/merge boundaries — exactly the insert-vs-blank
//  ambiguity (DIS-003/DIS-045).  The oracle still holds: the SAME content
//  lineforms to the SAME bytes, so recovery is well-defined.
#define W2_BLANK '_'
static ok64 w2_lineform(u8b dst, u8csc content) {
    sane(1);
    u8bReset(dst);
    for (u8cp p = content[0]; p < content[1]; p++) {
        if (*p != (u8)W2_BLANK) call(u8bFeed1, dst, *p);
        call(u8bFeed1, dst, (u8)'\n');
    }
    done;
}

// --- recovery: every ancestor of `i` recovers its content from w[i] -
static ok64 w2_verify(weave const *w, w2_line const *lines, u32 n, u32 i) {
    sane(w);
    u8 anc_i[W2_MAX_LINES];
    memset(anc_i, 0, n);
    w2_closure(lines, n, i, anc_i);

    //  Acquire the scope bitmap DIRECTLY (never via call(): call() would
    //  snapshot+rewind BASS and free it — api.mkd hazard).  out/exp/active
    //  are reused across ancestors (Scope/Produce/lineform reset on entry).
    u1b sc = {};
    ok64 ar = u1bAcquire(ABC_BASS, &sc, (size_t)n + 1);
    if (ar != OK) return ar;
    a_carve(u8,  out, W2_LINE_CAP);
    a_carve(u8,  exp, W2_LINE_CAP);
    a_carve(u64, active, (size_t)n + 1);

    for (u32 a = 0; a < n; a++) {
        if (!anc_i[a]) continue;
        u8 anc_a[W2_MAX_LINES];
        memset(anc_a, 0, n);
        w2_closure(lines, n, a, anc_a);
        u64bReset(active);
        for (u32 j = 0; j < n; j++)
            if (anc_a[j]) call(u64bFeed1, active, w2_cid(j));
        call(WEAVEScope, &sc, w, u64bDataC(active));
        call(WEAVEProduce, w, u1bDataC(&sc), out);
        call(w2_lineform, exp, lines[a].content);
        size_t gl = u8bDataLen(out), wl = u8bDataLen(exp);
        b8 okrec = (gl == wl) &&
            (wl == 0 || memcmp(u8bDataHead(out), u8bDataHead(exp), wl) == 0);
        if (!okrec) {
            u8 *g = u8bDataHead(out), *x = u8bDataHead(exp);
            fprintf(stderr, "FAIL recover w[%u] rev=%u: want(%zu)='", i, a, wl);
            for (size_t z = 0; z < wl; z++)
                fputc(x[z] == '\n' ? '.' : x[z], stderr);
            fprintf(stderr, "' got(%zu)='", gl);
            for (size_t z = 0; z < gl; z++)
                fputc(g[z] == '\n' ? '.' : g[z], stderr);
            fprintf(stderr, "'\n");
            if (!getenv("WDBG")) must(0, "weave failed to recover a revision's content");
        }
    }
    done;
}

// --- build all per-line weaves, then verify recovery ----------------
static ok64 w2_run(w2_line *lines, u32 n) {
    sane(1);
    a_cstr(ext, "c");

    u8 *wbuf[W2_MAX_LINES][4] = {};   // one persistent 'W' blob per line
    weave W[W2_MAX_LINES] = {};       // parsed view of each wbuf[i]
    a_carve(u8, line, W2_LINE_CAP);   // reusable lineform scratch
    u8 *mtmp[2][4] = {};              // ping-pong merge-fold temporaries
    ok64 ar;
    if ((ar = u8bAcquire(ABC_BASS, mtmp[0], W2_WEAVE_CAP)) != OK) return ar;
    if ((ar = u8bAcquire(ABC_BASS, mtmp[1], W2_WEAVE_CAP)) != OK) return ar;

    b8 dbg = getenv("WDBG") != NULL;
    for (u32 i = 0; i < n; i++) {
        if ((ar = u8bAcquire(ABC_BASS, wbuf[i], W2_WEAVE_CAP)) != OK) return ar;
        call(w2_lineform, line, lines[i].content);
        if (dbg) fprintf(stderr, "build w[%u] npar=%u\n", i, lines[i].npar);
        u8csc lf = {u8bDataHead(line), u8bDataHead(line) + u8bDataLen(line)};

        if (lines[i].npar == 0) {                       // root: from-blob
            call(WEAVENext, u8bIdle(wbuf[i]), NULL, lf, ext, w2_cid(i));
        } else {
            u32 mpar[W2_MAX_LINES];
            u32 nm = w2_norm_parents(lines, n, lines[i].par, lines[i].npar, mpar);
            must(nm >= 1, "normalized parent set empty");
            if (nm == 1) {                              // linear fold
                call(WEAVENext, u8bIdle(wbuf[i]), &W[mpar[0]], lf, ext, w2_cid(i));
            } else {                                    // merge fold + content
                if (dbg) fprintf(stderr, "  merge %u + %u\n", mpar[0], mpar[1]);
                u8bReset(mtmp[0]);
                call(WEAVEMerge, u8bIdle(mtmp[0]), &W[mpar[0]], &W[mpar[1]], 0);
                weave wm = {};
                call(WEAVEParse, &wm, u8bDataC(mtmp[0]));
                if (dbg) fprintf(stderr, "  merged: ntoks=%u ncommits=%u\n",
                                 (u32)$len(wm.toks), (u32)$len(wm.commits));
                u32 d = 1;
                for (u32 k = 2; k < nm; k++) {
                    u8bReset(mtmp[d]);
                    call(WEAVEMerge, u8bIdle(mtmp[d]), &wm, &W[mpar[k]], 0);
                    call(WEAVEParse, &wm, u8bDataC(mtmp[d]));
                    d ^= 1;
                }
                if (dbg) fprintf(stderr, "  fold content into merge\n");
                call(WEAVENext, u8bIdle(wbuf[i]), &wm, lf, ext, w2_cid(i));
            }
        }
        call(WEAVEParse, &W[i], u8bDataC(wbuf[i]));
        if (dbg) fprintf(stderr, "  w[%u] built: ntoks=%u ncommits=%u\n", i,
                         (u32)$len(W[i].toks), (u32)$len(W[i].commits));
    }

    for (u32 i = 0; i < n; i++) {
        if (dbg) fprintf(stderr, "verify w[%u]\n", i);
        call(w2_verify, &W[i], lines, n, i);
    }
    done;
}

// --- entry ----------------------------------------------------------
FUZZ(u8, WEAVEfuzz) {
    sane(1);
    if ($empty(input) || $len(input) > W2_FUZZ_MAX) done;
    static _Thread_local w2_line lines[W2_MAX_LINES];
    u32 n = 0;
    if (w2_parse(input, lines, &n) != OK) done;
    call(w2_run, lines, n);
    done;
}
