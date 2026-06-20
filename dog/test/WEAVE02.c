//
//  WEAVE02 (DOG-004) — WEAVEEmit{Diff,Full,Merged} over scope bitmaps.
//
//  Builds weaves with WEAVENext / WEAVEMerge, derives scopes with
//  WEAVEScope, then asserts the EMITTED HUNK bytes (URI + body text +
//  per-token diff side) byte-for-byte against the graf oracle captured
//  by graf/test/ORACLE (DOG-004 ticket).  A >16 MiB case locks the
//  DIFF-007 coarse-fallback + `capped` status row.
//
#include "dog/WEAVE.h"

#include <stdio.h>
#include <string.h>

#include "abc/PRO.h"
#include "abc/TEST.h"
#include "dog/HUNK.h"

// --- a captured hunk: uri + text + a compact side-delta string -------
typedef struct {
    char uri[256];
    char text[4096];
    char sides[1024];   // "=1 =1 +1 -1 ..." (side char + offset delta)
    b8   has_verb;      // a status row (capped) carries a verb, empty body
} caphunk;

#define MAXCAP 16
typedef struct { caphunk h[MAXCAP]; u32 n; } capctx;

static ok64 cap_cb(hunkc *hk, void *vctx) {
    sane(hk != NULL);
    capctx *c = (capctx *)vctx;
    if (c->n >= MAXCAP) fail(TESTFAIL);
    caphunk *o = &c->h[c->n++];
    memset(o, 0, sizeof(*o));
    o->has_verb = (hk->verb != 0);
    u32 ul = (u32)$len(hk->uri);
    if (ul >= sizeof(o->uri)) ul = sizeof(o->uri) - 1;
    memcpy(o->uri, hk->uri[0], ul); o->uri[ul] = 0;
    u32 tl = (u32)$len(hk->text);
    if (tl >= sizeof(o->text)) tl = sizeof(o->text) - 1;
    memcpy(o->text, hk->text[0], tl); o->text[tl] = 0;
    char *s = o->sides; u32 cap = sizeof(o->sides), used = 0, prev = 0;
    $for(tok32c, t, hk->toks) {
        u8 sd = tok32Side(*t);
        char ch = sd == TOK_SIDE_IN ? '+' : sd == TOK_SIDE_RM ? '-' : '=';
        u32 off = tok32Offset(*t);
        int w = snprintf(s + used, cap - used, "%c%u ", ch, off - prev);
        if (w < 0 || (u32)w >= cap - used) break;
        used += (u32)w; prev = off;
    }
    done;
}

//  Fill an already-acquired scope `into` from a list of active commit
//  ids.  `into` must be acquired by the CALLER directly (never via call()
//  — call() rewinds BASS and frees a fresh acquisition; api.mkd hazard).
static ok64 fill_scope(u1b *into, weave const *w, u64 const *ids, u32 nid) {
    sane(into && w);
    a_carve(u64, act, (size_t)nid + 1);
    for (u32 k = 0; k < nid; k++) call(u64bFeed1, act, ids[k]);
    call(WEAVEScope, into, w, u64bDataC(act));
    done;
}

//  Acquire `into` DIRECTLY in the caller's frame, then fill it.  Wrapped
//  in a macro so the acquisition lands in the caller's BASS frame.
#define MK_SCOPE(SC, W, IDS, NID) do {                                    \
    ok64 _ar = u1bAcquire(ABC_BASS, (SC), (size_t)$len((W)->commits) + 1);\
    if (_ar != OK) return _ar;                                            \
    call(fill_scope, (SC), (W), (IDS), (NID)); } while (0)

static ok64 expect(char const *lbl, caphunk const *got, b8 has_verb,
                   char const *uri, char const *text, char const *sides) {
    sane(1);
    if (got->has_verb != has_verb) {
        fprintf(stderr, " %s verb mismatch (%d vs %d)\n", lbl, got->has_verb, has_verb);
        fail(TESTFAIL);
    }
    if (uri && strcmp(got->uri, uri)) {
        fprintf(stderr, " %s URI mismatch\n  got [%s]\n  want[%s]\n", lbl, got->uri, uri);
        fail(TESTFAIL);
    }
    if (text && strcmp(got->text, text)) {
        fprintf(stderr, " %s TEXT mismatch\n  got [%s]\n  want[%s]\n", lbl, got->text, text);
        fail(TESTFAIL);
    }
    if (sides && strcmp(got->sides, sides)) {
        fprintf(stderr, " %s SIDES mismatch\n  got [%s]\n  want[%s]\n", lbl, got->sides, sides);
        fail(TESTFAIL);
    }
    done;
}

//  v1 -> v2 single-line edit; scope from = {c1}, to = {c1,c2}.  Whole file
//  is one window (change within 3-line context), so Diff == Full body.
static ok64 emit_singlewin(void) {
    sane(1);
    fprintf(stderr, "  emit_singlewin ...");
    a_cstr(cext, "c"); a_cstr(name, "foo.c"); a_cstr(nav, "deadbeef");
    u8csc v1 = {(u8c *)"a\nb\nc\nd\ne\nf\ng\n", (u8c *)"a\nb\nc\nd\ne\nf\ng\n" + 14};
    u8csc v2 = {(u8c *)"a\nb\nc\nX\ne\nf\ng\n", (u8c *)"a\nb\nc\nX\ne\nf\ng\n" + 14};
    a_carve(u8, w1b, 1UL << 16); call(WEAVENext, u8bIdle(w1b), NULL, v1, cext, 1);
    weave w1 = {}; call(WEAVEParse, &w1, u8bDataC(w1b));
    a_carve(u8, w2b, 1UL << 16); call(WEAVENext, u8bIdle(w2b), &w1, v2, cext, 2);
    weave w2 = {}; call(WEAVEParse, &w2, u8bDataC(w2b));
    u1b from = {}, to = {};
    u64 idf[] = {1}, idt[] = {1, 2};
    MK_SCOPE(&from, &w2, idf, 1);
    MK_SCOPE(&to, &w2, idt, 2);

    char const *wantsides = "=1 =1 =1 =1 =1 =1 +1 -1 =1 =1 =1 =1 =1 =1 =1 ";
    char const *wanttext  = "a\nb\nc\nXd\ne\nf\ng\n";

    capctx cd = {};
    call(WEAVEEmitDiff, &w2, name, nav, u1bDataC(&from), u1bDataC(&to), cap_cb, &cd);
    if (cd.n != 1) { fprintf(stderr, " diff want 1 hunk got %u\n", cd.n); fail(TESTFAIL); }
    call(expect, "Diff", &cd.h[0], NO, "diff:foo.c?deadbeef#L1",
         "a\nb\nc\nXd\ne\nf\ng\n", wantsides);

    capctx cf = {};
    a_cstr(dsch, "diff:");
    call(WEAVEEmitFull, &w2, name, dsch, nav, u1bDataC(&from), u1bDataC(&to), cap_cb, &cf);
    if (cf.n != 1) { fprintf(stderr, " full want 1 hunk got %u\n", cf.n); fail(TESTFAIL); }
    call(expect, "Full-diff", &cf.h[0], NO, "diff:foo.c?deadbeef#L1", wanttext, wantsides);

    //  cat: (empty scheme) — no scheme prefix on the URI.
    capctx cc = {};
    u8cs nosch = {};
    call(WEAVEEmitFull, &w2, name, nosch, nav, u1bDataC(&from), u1bDataC(&to), cap_cb, &cc);
    if (cc.n != 1) { fprintf(stderr, " cat want 1 hunk got %u\n", cc.n); fail(TESTFAIL); }
    call(expect, "Full-cat", &cc.h[0], NO, "foo.c?deadbeef#L1", wanttext, wantsides);
    fprintf(stderr, " ok\n");
    done;
}

//  Two far-apart edits -> two windows -> two hunks at #L1 and #L8.
static ok64 emit_twowin(void) {
    sane(1);
    fprintf(stderr, "  emit_twowin ...");
    a_cstr(cext, "c"); a_cstr(name, "foo.c"); a_cstr(nav, "deadbeef");
    static char const b1[] = "l0\nl1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\nl11\nl12\nl13\n";
    static char const b2[] = "l0\nX1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\nX11\nl12\nl13\n";
    u8csc v1 = {(u8c *)b1, (u8c *)b1 + sizeof(b1) - 1};
    u8csc v2 = {(u8c *)b2, (u8c *)b2 + sizeof(b2) - 1};
    a_carve(u8, w1b, 1UL << 16); call(WEAVENext, u8bIdle(w1b), NULL, v1, cext, 1);
    weave w1 = {}; call(WEAVEParse, &w1, u8bDataC(w1b));
    a_carve(u8, w2b, 1UL << 16); call(WEAVENext, u8bIdle(w2b), &w1, v2, cext, 2);
    weave w2 = {}; call(WEAVEParse, &w2, u8bDataC(w2b));
    u1b from = {}, to = {};
    u64 idf[] = {1}, idt[] = {1, 2};
    MK_SCOPE(&from, &w2, idf, 1);
    MK_SCOPE(&to, &w2, idt, 2);

    capctx c = {};
    call(WEAVEEmitDiff, &w2, name, nav, u1bDataC(&from), u1bDataC(&to), cap_cb, &c);
    if (c.n != 2) { fprintf(stderr, " want 2 hunks got %u\n", c.n); fail(TESTFAIL); }
    call(expect, "2W-a", &c.h[0], NO, "diff:foo.c?deadbeef#L1",
         "l0\nX1l1\nl2\nl3\nl4\n", "=2 =1 +2 -2 =1 =2 =1 =2 =1 =2 =1 ");
    call(expect, "2W-b", &c.h[1], NO, "diff:foo.c?deadbeef#L8",
         "l7\nl8\nl9\nl10\nX11l11\nl12\nl13\n",
         "=2 =1 =2 =1 =2 =1 =3 =1 +3 -3 =1 =3 =1 =3 =1 ");
    fprintf(stderr, " ok\n");
    done;
}

//  WEAVEEmitMerged: ours/theirs both edit line 2 -> conflict frame.
//  Disjoint edits (line 1 vs line 3) -> no markers.  Built dog-style via
//  WEAVENext branches + WEAVEMerge; groups = {ours} / {theirs} closures.
static ok64 emit_merged(void) {
    sane(1);
    fprintf(stderr, "  emit_merged ...");
    a_cstr(cext, "c");
    //  conflict: base a/b/c, ours -> a/O/c, theirs -> a/T/c
    u8csc v0 = {(u8c *)"a\nb\nc\n", (u8c *)"a\nb\nc\n" + 6};
    u8csc vo = {(u8c *)"a\nO\nc\n", (u8c *)"a\nO\nc\n" + 6};
    u8csc vt = {(u8c *)"a\nT\nc\n", (u8c *)"a\nT\nc\n" + 6};
    a_carve(u8, w0b, 1UL << 16); call(WEAVENext, u8bIdle(w0b), NULL, v0, cext, 1);
    weave w0 = {}; call(WEAVEParse, &w0, u8bDataC(w0b));
    a_carve(u8, wob, 1UL << 16); call(WEAVENext, u8bIdle(wob), &w0, vo, cext, 2);
    weave wo = {}; call(WEAVEParse, &wo, u8bDataC(wob));
    a_carve(u8, wtb, 1UL << 16); call(WEAVENext, u8bIdle(wtb), &w0, vt, cext, 3);
    weave wt = {}; call(WEAVEParse, &wt, u8bDataC(wtb));
    a_carve(u8, wmb, 1UL << 16); call(WEAVEMerge, u8bIdle(wmb), &wo, &wt, 0);
    weave wm = {}; call(WEAVEParse, &wm, u8bDataC(wmb));

    u1b g_ours = {}, g_theirs = {};
    u64 ido[] = {1, 2}, idt[] = {1, 3};
    MK_SCOPE(&g_ours, &wm, ido, 2);
    MK_SCOPE(&g_theirs, &wm, idt, 2);
    u1cs groups[2] = {u1bDataC(&g_ours), u1bDataC(&g_theirs)};

    a_carve(u8, out, 1UL << 16);
    call(WEAVEEmitMerged, &wm, groups, 2, out);
    char gotc[256] = {}; u32 gl = (u32)u8bDataLen(out);
    if (gl >= sizeof(gotc)) gl = sizeof(gotc) - 1;
    memcpy(gotc, u8bDataHead(out), gl); gotc[gl] = 0;
    //  graf's oracle frames ours-then-theirs (`O||||T`, positional replay).
    //  dog's WEAVEMerge lays concurrent siblings in RGA order (commit-id
    //  DESC, DIS-043), so commit 3 (theirs/T) precedes commit 2 (ours/O):
    //  the FRAMING logic is byte-parity with graf, the group ORDER follows
    //  dog's deterministic weave order.
    char const *wantc = "a\n<<<<T||||O>>>>\nc\n";
    if (strcmp(gotc, wantc)) {
        fprintf(stderr, " conflict mismatch\n  got [%s]\n  want[%s]\n", gotc, wantc);
        fail(TESTFAIL);
    }

    //  disjoint: base a/b/c, ours -> A/b/c, theirs -> a/b/C  (no markers)
    u8csc dva = {(u8c *)"A\nb\nc\n", (u8c *)"A\nb\nc\n" + 6};
    u8csc dvb = {(u8c *)"a\nb\nC\n", (u8c *)"a\nb\nC\n" + 6};
    a_carve(u8, d0b, 1UL << 16); call(WEAVENext, u8bIdle(d0b), NULL, v0, cext, 1);
    weave d0 = {}; call(WEAVEParse, &d0, u8bDataC(d0b));
    a_carve(u8, dob, 1UL << 16); call(WEAVENext, u8bIdle(dob), &d0, dva, cext, 2);
    weave dao = {}; call(WEAVEParse, &dao, u8bDataC(dob));
    a_carve(u8, dtb, 1UL << 16); call(WEAVENext, u8bIdle(dtb), &d0, dvb, cext, 3);
    weave dat = {}; call(WEAVEParse, &dat, u8bDataC(dtb));
    a_carve(u8, dmb, 1UL << 16); call(WEAVEMerge, u8bIdle(dmb), &dao, &dat, 0);
    weave dm = {}; call(WEAVEParse, &dm, u8bDataC(dmb));
    u1b dg_o = {}, dg_t = {};
    MK_SCOPE(&dg_o, &dm, ido, 2);
    MK_SCOPE(&dg_t, &dm, idt, 2);
    u1cs dgroups[2] = {u1bDataC(&dg_o), u1bDataC(&dg_t)};
    a_carve(u8, dout, 1UL << 16);
    call(WEAVEEmitMerged, &dm, dgroups, 2, dout);
    char gotd[256] = {}; u32 dl = (u32)u8bDataLen(dout);
    if (dl >= sizeof(gotd)) dl = sizeof(gotd) - 1;
    memcpy(gotd, u8bDataHead(dout), dl); gotd[dl] = 0;
    char const *wantd = "A\nb\nC\n";
    if (strcmp(gotd, wantd)) {
        fprintf(stderr, " disjoint mismatch\n  got [%s]\n  want[%s]\n", gotd, wantd);
        fail(TESTFAIL);
    }
    fprintf(stderr, " ok\n");
    done;
}

//  DIFF-007: a >16 MiB weave-text file must NOT silently empty.  Build a
//  big from-blob weave (> WEAVE_TEXT_CAP), fold a one-line edit, then
//  EmitDiff must produce a coarse whole-file hunk (the to-scope bytes)
//  plus a `capped:` status row — never zero hunks.
static ok64 emit_capped(void) {
    sane(1);
    fprintf(stderr, "  emit_capped ...");
    a_cstr(cext, "txt"); a_cstr(name, "big.txt"); a_cstr(nav, "cafe");
    //  >16 MiB text but FEW tokens: ~17K lines of 1 KiB each (one lexer
    //  token per long line) keeps the serialized weave small while the
    //  text column trips the 16 MiB cap (WEAVE_TEXT_CAP).
    size_t linelen = 1024, nlines = (17UL << 20) / linelen + 1;
    a_carve(u8, blob, nlines * (linelen + 1) + 1);
    for (size_t k = 0; k < nlines; k++) {
        for (size_t b = 0; b < linelen; b++) call(u8bFeed1, blob, 'x');
        call(u8bFeed1, blob, '\n');
    }
    u8cs big = {}; u8csMv(big, u8bDataC(blob));
    //  weave = text (~17 MiB) + toks (~17K * 4) + anc (~17K * 8) + slack.
    a_carve(u8, w1b, (20UL << 20));
    call(WEAVENext, u8bIdle(w1b), NULL, big, cext, 1);
    weave w1 = {}; call(WEAVEParse, &w1, u8bDataC(w1b));
    if (u8csLen(w1.text) <= (16UL << 20)) {
        fprintf(stderr, " setup: text %zu not > 16 MiB\n", u8csLen(w1.text)); fail(TESTFAIL);
    }
    u1b from = {}, to = {};
    u64 idf[] = {1}, idt[] = {1};
    MK_SCOPE(&from, &w1, idf, 1);
    MK_SCOPE(&to, &w1, idt, 1);
    capctx c = {};
    call(WEAVEEmitDiff, &w1, name, nav, u1bDataC(&from), u1bDataC(&to), cap_cb, &c);
    if (c.n != 2) { fprintf(stderr, " capped want 2 hunks (body+status) got %u\n", c.n); fail(TESTFAIL); }
    //  hunk 0: coarse whole-file body (the alive bytes), no verb.
    if (c.h[0].has_verb) { fprintf(stderr, " capped: body hunk has verb\n"); fail(TESTFAIL); }
    if (strcmp(c.h[0].uri, "diff:big.txt?cafe#L1")) {
        fprintf(stderr, " capped body URI [%s]\n", c.h[0].uri); fail(TESTFAIL);
    }
    if (strlen(c.h[0].text) == 0) { fprintf(stderr, " capped: empty body!\n"); fail(TESTFAIL); }
    //  hunk 1: a `capped:` status row (verb set, empty body).
    if (!c.h[1].has_verb) { fprintf(stderr, " capped: status row missing verb\n"); fail(TESTFAIL); }
    if (strncmp(c.h[1].uri, "capped:big.txt", 14)) {
        fprintf(stderr, " capped status URI [%s]\n", c.h[1].uri); fail(TESTFAIL);
    }
    fprintf(stderr, " ok\n");
    done;
}

ok64 WEAVE02test() {
    sane(1);
    call(emit_singlewin);
    call(emit_twowin);
    call(emit_merged);
    call(emit_capped);
    done;
}

TEST(WEAVE02test);
