#include "HUNK.h"

#include <time.h>

#include "abc/ANSI.h"
#include "abc/PRO.h"
#include "abc/RON.h"
#include "abc/URI.h"
#include "dog/DOG.h"
#include "dog/THEME.h"
#include "dog/ULOG.h"
#include "dog/tok/TOK.h"

// Pack ron60 as a fixed 8-byte LE record under `tag`.
// Caller has already opened the outer container.
static ok64 hunk_feed_ron60(u8s inner, u8 tag, ron60 v) {
    sane(u8sOK(inner));
    a_pad(u8, buf, 8);
    for (int i = 0; i < 8; i++) call(u8sFeed1, buf_idle, (u8)(v >> (8 * i)));
    a_dup(u8c, val, u8bData(buf));
    call(TLVu8sFeed, inner, tag, val);
    done;
}

// Unpack a fixed 8-byte LE ron60.  Tolerant of short slices (treats
// them as 0) so legacy/forward-compat streams don't fail-hard.
static ron60 hunk_drain_ron60(u8cs val) {
    if ($len(val) != 8) return 0;
    u64 v = 0;
    for (int i = 0; i < 8; i++) v |= ((u64)val[0][i]) << (8 * i);
    return (ron60)v;
}

// A status hunk is a ULOG-style event row lifted into hunk shape:
// ts/verb populated, no source body.  Renders as one line.
static b8 hunk_is_status(hunk const *hk) {
    return (hk->ts || hk->verb) && $empty(hk->text) && $empty(hk->toks);
}

// Render `<pretty-date>\t<verb-token>\t<uri>\n` — human-readable but
// uncoloured.  `DOGutf8sFeedDate` produces HH:MM / weekday / date
// depending on age; `RONutf8sFeed` on a status verb prints the
// symbolic token ("put", "new", "mod", ...) since those verbs live
// in low ron60 codes.  For machine-parsing of the ts/verb fields
// the caller should pick TLV mode (which emits raw 8-byte LE ron60
// records under tags 'T'/'V').
static ok64 hunk_feed_status_plain(u8s into, hunk const *hk) {
    sane(u8sOK(into) && hk);
    if ($len(into) < 64) fail(BNOROOM);

    i64 now = (i64)time(NULL);
    i64 ts  = now;
    if (hk->ts) {
        struct tm tm = {};
        if (RONToTime(hk->ts, &tm, NULL) == OK) {
            time_t t = mktime(&tm);
            if (t != (time_t)-1) ts = (i64)t;
        }
    }
    call(DOGutf8sFeedDate, into, ts, now);
    call(u8sFeed1, into, '\t');
    call(RONutf8sFeed, into, hk->verb);
    call(u8sFeed1, into, '\t');
    if (!$empty(hk->uri)) call(u8sFeed, into, hk->uri);
    call(u8sFeed1, into, '\n');
    done;
}

// ANSI variant of the status line: date column in `unk` grey, verb in
// its palette colour.  Mirrors the old `ULOGFeedStatusLine` rendering.
#define HUNK_VERB_UNK_RON60 0x39caf  // "unk" — same constant ULOG used.
static ok64 hunk_feed_status_color(u8s into, hunk const *hk) {
    sane(u8sOK(into) && hk);
    if ($len(into) < 64) fail(BNOROOM);

    ansi64 c_unk  = ULOGVerbColor(HUNK_VERB_UNK_RON60);
    ansi64 c_verb = ULOGVerbColor(hk->verb);

    i64 now = (i64)time(NULL);
    i64 ts  = now;
    if (hk->ts) {
        struct tm tm = {};
        if (RONToTime(hk->ts, &tm, NULL) == OK) {
            time_t t = mktime(&tm);
            if (t != (time_t)-1) ts = (i64)t;
        }
    }

    call(ANSIu8sFeedDelta, into, c_unk, ANSI_DEFAULT);
    call(DOGutf8sFeedDate, into, ts, now);
    call(ANSIu8sFeedReset, into, c_unk);
    call(u8sFeed1, into, '\t');

    call(ANSIu8sFeedDelta, into, c_verb, ANSI_DEFAULT);
    call(RONutf8sFeed, into, hk->verb);
    call(ANSIu8sFeedReset, into, c_verb);
    call(u8sFeed1, into, '\t');

    if (!$empty(hk->uri)) call(u8sFeed, into, hk->uri);
    call(u8sFeed1, into, '\n');
    done;
}

// Fragments have no grammar — they carry whatever the producer wrote.
// The conventions HUNKu8sMakeURI emits are:
//   - bare line:        `#L42`
//   - symbol + line:    `#sym:L42`  (or `#'quoted body':L42`)
//   - symbol only:      `#sym`  / `#'quoted body'`
// HUNKu8sFragSplit is a best-effort extractor over those conventions;
// anything else is left to the caller to interpret.

// Percent-escape URI-illegal bytes (control, non-ASCII, '#', '%') into
// `into`.  Everything else in printable ASCII passes through.  Used by
// HUNKu8sMakeURI to wrap free-form symbol bodies safely.
static ok64 hunk_frag_esc(u8s into, u8cs raw) {
    if (into[0] == NULL || into[0] >= into[1]) return SNOROOM;
    if (raw[0] == NULL || raw[0] >= raw[1]) return OK;
    static const u8c HEX[16] = "0123456789ABCDEF";
    for (u8cp p = raw[0]; p < raw[1]; p++) {
        u8 c = *p;
        b8 legal = (c >= 0x20 && c <= 0x7E && c != '#' && c != '%');
        if (legal) {
            if (into[0] >= into[1]) return SNOROOM;
            *into[0]++ = c;
        } else {
            if (into[0] + 3 > into[1]) return SNOROOM;
            *into[0]++ = '%';
            *into[0]++ = HEX[(c >> 4) & 0xF];
            *into[0]++ = HEX[c & 0xF];
        }
    }
    return OK;
}

u32 HUNKu8sFragSplit(u8csc frag, u8cs out_sym) {
    if (out_sym) { out_sym[0] = NULL; out_sym[1] = NULL; }
    if ($empty(frag)) return 0;

    u8cp end = frag[1];
    u8cp p = end;
    while (p > frag[0] && p[-1] >= '0' && p[-1] <= '9') p--;
    u8cp digits = p;
    b8 has_digits = (digits < end);
    if (has_digits && p > frag[0] && p[-1] == 'L') p--;
    b8 is_bare    = has_digits && (p == frag[0]);
    b8 after_col  = has_digits && (p > frag[0]) && (p[-1] == ':');
    u32 line = 0;
    if (has_digits && (is_bare || after_col))
        for (u8cp d = digits; d < end; d++) line = line * 10 + (*d - '0');

    if (out_sym) {
        u8cp sym_lo = (u8cp)frag[0];
        u8cp sym_hi = (u8cp)end;
        if (after_col) sym_hi = (u8cp)(p - 1);   // drop trailing `:[L]?<digits>`
        else if (is_bare) sym_hi = sym_lo;        // no symbol
        if (sym_hi - sym_lo >= 2 &&
            *sym_lo == '\'' && *(sym_hi - 1) == '\'') {
            sym_lo++;
            sym_hi--;
        }
        out_sym[0] = sym_lo;
        out_sym[1] = sym_hi;
    }
    return line;
}

ok64 HUNKu8sFeed(u8s into, hunk const *hk) {
    sane(u8sOK(into) && hk != NULL);
    u8s inner = {};
    call(TLVu8sStart, into, inner, HUNK_TLV);
    if (hk->ts)   call(hunk_feed_ron60, inner, HUNK_TLV_TS,  hk->ts);
    if (hk->verb) call(hunk_feed_ron60, inner, HUNK_TLV_VRB, hk->verb);
    if (!$empty(hk->uri))
        call(TLVu8sFeed, inner, HUNK_TLV_URI, hk->uri);
    if (!$empty(hk->text))
        call(TLVu8sFeed, inner, HUNK_TLV_TXT, hk->text);
    if (!$empty(hk->toks)) {
        u8cs tkb = {(u8cp)hk->toks[0], (u8cp)hk->toks[1]};
        call(TLVu8sFeed, inner, HUNK_TLV_TOK, tkb);
    }
    call(TLVu8sEnd, into, inner, HUNK_TLV);
    done;
}

ok64 HUNKu8sDrain(u8cs from, hunk *hk) {
    sane($ok(from) && hk != NULL);
    u8 t = 0;
    u8cs body = {};
    call(TLVu8sDrain, from, &t, body);
    test(t == HUNK_TLV, TLVBADTYPE);
    *hk = (hunk){};
    while (!$empty(body)) {
        u8 st = 0;
        u8cs val = {};
        call(TLVu8sDrain, body, &st, val);
        switch (st) {
        case HUNK_TLV_TS:
            hk->ts = hunk_drain_ron60(val);
            break;
        case HUNK_TLV_VRB:
            hk->verb = hunk_drain_ron60(val);
            break;
        case HUNK_TLV_URI:
            $mv(hk->uri, val);
            break;
        case HUNK_TLV_TXT:
            $mv(hk->text, val);
            break;
        case HUNK_TLV_TOK:
            hk->toks[0] = (tok32c *)val[0];
            hk->toks[1] = (tok32c *)val[1];
            break;
        default:
            break;
        }
    }
    done;
}

// Does any token have a non-eq side?
static b8 hunk_has_diff(hunk const *hk) {
    int n = (int)$len(hk->toks);
    for (int i = 0; i < n; i++) {
        if (tok32Tag(hk->toks[0][i]) == 'U') continue;
        if (tok32Side(hk->toks[0][i]) != TOK_SIDE_EQ) return YES;
    }
    return NO;
}

// Plain-mode content-hunk header: `--- <uri> ---\n`.  The dashes are
// the only visual separator between adjacent hunks in non-color output
// (color mode gets visual separation from the title-tag color band —
// see hunk_feed_header_color below).  No-op on empty URI.
static ok64 hunk_feed_header_plain(u8s into, hunk const *hk) {
    sane(u8sOK(into) && hk);
    if ($empty(hk->uri)) done;
    a_cstr(pfx, "--- ");
    a_cstr(sfx, " ---\n");
    call(u8sFeed,  into, pfx);
    call(u8sFeed,  into, hk->uri);
    call(u8sFeed,  into, sfx);
    done;
}

// Color-mode content-hunk header: `<uri>\n`, painted in theme slot 'T'
// (title).  The color band visually frames the hunk so the dashes from
// plain mode aren't needed.  No-op on empty URI.
static ok64 hunk_feed_header_color(u8s into, hunk const *hk) {
    sane(u8sOK(into) && hk);
    if ($empty(hk->uri)) done;
    ansi64 c = THEMEAt('T');
    call(ANSIu8sFeedDelta, into, c, ANSI_DEFAULT);
    call(u8sFeed,          into, hk->uri);
    call(ANSIu8sFeedReset, into, c);
    call(u8sFeed1,         into, '\n');
    done;
}

// Walk tokens overlapping byte range [lo, hi) and OR-combine their sides.
// Returns a mask: bit 0 = saw IN, bit 1 = saw RM.  'U' tokens (click-
// target URIs) are skipped — they carry no diff side.
static u8 hunk_line_sides(hunk const *hk, u32 lo, u32 hi) {
    u8 mask = 0;
    int n = (int)$len(hk->toks);
    u32 prev = 0;
    for (int i = 0; i < n; i++) {
        u32 end = tok32Offset(hk->toks[0][i]);
        if (end > lo && prev < hi
            && tok32Tag(hk->toks[0][i]) != 'U') {
            u8 side = tok32Side(hk->toks[0][i]);
            if (side == TOK_SIDE_IN) mask |= 1;
            else if (side == TOK_SIDE_RM) mask |= 2;
        }
        prev = end;
        if (prev >= hi) break;
    }
    return mask;
}

// Emit hk->text[lo,hi) into `into`, skipping bytes covered by 'U'
// (URI) tokens.  URI bytes are invisible click-target metadata —
// see dog/tok/TOK.h.  No-op when lo>=hi.
static ok64 hunk_feed_visible(u8s into, hunk const *hk, u32 lo, u32 hi) {
    sane(u8sOK(into) && hk != NULL);
    if (lo >= hi) done;
    int n = (int)$len(hk->toks);
    u32 prev = 0;
    u32 emit_lo = lo;
    for (int i = 0; i < n; i++) {
        u32 end = tok32Offset(hk->toks[0][i]);
        u32 tlo = prev;
        u32 thi = end;
        prev = end;
        if (thi <= lo) continue;
        if (tlo >= hi) break;
        if (tok32Tag(hk->toks[0][i]) != 'U') continue;
        if (tlo < lo) tlo = lo;
        if (thi > hi) thi = hi;
        if (emit_lo < tlo) {
            a$part(u8c, vis, hk->text, emit_lo, tlo - emit_lo);
            call(u8sFeed, into, vis);
        }
        emit_lo = thi;
    }
    if (emit_lo < hi) {
        a$part(u8c, vis, hk->text, emit_lo, hi - emit_lo);
        call(u8sFeed, into, vis);
    }
    done;
}

//  Forward decl — line-based unified diff renderer lives below.  It
//  is internal-only (no header decl): callers reach it through
//  `HUNKu8sFeedText` for hunks carrying a `diff:` URI scheme.
static ok64 HUNKu8sFeedLineBased(u8s into, hunk const *hk);

//  `diff:` is the projector scheme producers use to mark a hunk as a
//  proper diff (vs. cat / search / log).  In plain mode such hunks
//  render as `git apply`-able unified diff via HUNKu8sFeedLineBased;
//  non-diff plain hunks keep the simple tokenised-line shape below.
static b8 hunk_uri_is_diff(hunk const *hk) {
    if ($empty(hk->uri)) return NO;
    uri u = {};
    u8csc text = {hk->uri[0], hk->uri[1]};
    if (DOGParseURI(&u, text) != OK) return NO;
    if ($empty(u.scheme)) return NO;
    a_cstr(s_diff, "diff");
    return u8csEq(u.scheme, s_diff);
}

ok64 HUNKu8sFeedText(u8s into, hunk const *hk) {
    sane(u8sOK(into) && hk != NULL);

    //  Status hunk (ULOG-row shape) → single `<ts>\t<verb>\t<uri>\n` line.
    if (hunk_is_status(hk)) return hunk_feed_status_plain(into, hk);

    //  Diff-marked hunks render as proper unified diff so the output
    //  is `git apply` / `patch` friendly.  Detected via URI scheme so
    //  producers explicitly opt in (a content hunk with stray rm/in
    //  tokens still renders as cat).
    if (hunk_uri_is_diff(hk)) return HUNKu8sFeedLineBased(into, hk);

    call(hunk_feed_header_plain, into, hk);

    if ($empty(hk->text)) {
        if ($empty(hk->uri)) u8sFeed1(into, '\n');
        done;
    }

    if (!hunk_has_diff(hk)) {
        //  Content hunk (grep / search / cat): emit text verbatim,
        //  minus any 'U'-tagged URI ranges, plus a trailing blank-line
        //  separator so the next hunk's header stands clear.
        u32 tlen = (u32)$len(hk->text);
        call(hunk_feed_visible, into, hk, 0, tlen);
        if (tlen > 0 && hk->text[0][tlen - 1] != '\n')
            u8sFeed1(into, '\n');
        u8sFeed1(into, '\n');
        done;
    }

    // Diff hunk: per-line '+' / '-' / ' ' prefix from tok side bits.
    u8c *base = hk->text[0];
    a_dup(u8 const, cur, hk->text);
    while (!$empty(cur)) {
        u32 line_lo = (u32)(cur[0] - base);
        u8cs scan = {cur[0], cur[1]};
        b8 had_nl = (u8csFind(scan, '\n') == OK);
        u8c *line_end = scan[0];
        u32 line_hi = (u32)(line_end - base);

        u8 sides = hunk_line_sides(hk, line_lo, line_hi);
        u8 prefix = ' ';
        if (sides & 1) prefix = '+';
        if (sides & 2) prefix = '-';
        u8sFeed1(into, prefix);
        call(hunk_feed_visible, into, hk, line_lo, line_hi);
        u8sFeed1(into, '\n');
        cur[0] = had_nl ? line_end + 1 : line_end;
    }
    u8sFeed1(into, '\n');
    done;
}

//  ANSI sequences for the diff-line colour band.  Re-emitted per line
//  so the renderer stays stateless (no carried `prev`); the trailing
//  `\033[0m` keeps the terminal clean once the hunk ends.
#define HUNK_ANSI_RED   "\033[31m"
#define HUNK_ANSI_GREEN "\033[32m"
#define HUNK_ANSI_RESET "\033[0m"

ok64 HUNKu8sFeedColor(u8s into, hunk const *hk) {
    sane(u8sOK(into) && hk != NULL);

    //  Status hunk: ULOG-style coloured line and we're done.
    if (hunk_is_status(hk)) return hunk_feed_status_color(into, hk);

    call(hunk_feed_header_color, into, hk);

    if ($empty(hk->text)) {
        if ($empty(hk->uri)) u8sFeed1(into, '\n');
        done;
    }

    if (!hunk_has_diff(hk)) {
        //  Content hunk (grep / search / cat): emit text verbatim, no
        //  colour layer here; 'U'-tagged URI bytes stay hidden.
        u32 tlen = (u32)$len(hk->text);
        call(hunk_feed_visible, into, hk, 0, tlen);
        if (tlen > 0 && hk->text[0][tlen - 1] != '\n')
            u8sFeed1(into, '\n');
        u8sFeed1(into, '\n');
        done;
    }

    //  Diff hunk: per-line side from tok bits, `-` lines red and `+`
    //  lines green; context lines uncoloured.  Mirrors HUNKu8sFeedText's
    //  line-walk; only the colour band differs.
    u8c *base = hk->text[0];
    a_dup(u8 const, cur, hk->text);
    while (!$empty(cur)) {
        u32 line_lo = (u32)(cur[0] - base);
        u8cs scan = {cur[0], cur[1]};
        b8 had_nl = (u8csFind(scan, '\n') == OK);
        u8c *line_end = scan[0];
        u32 line_hi = (u32)(line_end - base);

        u8 sides = hunk_line_sides(hk, line_lo, line_hi);
        u8 prefix = ' ';
        b8 col_add = NO, col_del = NO;
        if (sides & 1) { prefix = '+'; col_add = YES; }
        if (sides & 2) { prefix = '-'; col_del = YES; }

        if (col_add) { a_cstr(s, HUNK_ANSI_GREEN); u8sFeed(into, s); }
        if (col_del) { a_cstr(s, HUNK_ANSI_RED);   u8sFeed(into, s); }
        u8sFeed1(into, prefix);
        call(hunk_feed_visible, into, hk, line_lo, line_hi);
        if (col_add || col_del) {
            a_cstr(off, HUNK_ANSI_RESET); u8sFeed(into, off);
        }
        u8sFeed1(into, '\n');
        cur[0] = had_nl ? line_end + 1 : line_end;
    }
    u8sFeed1(into, '\n');
    done;
}

//  Module-global mode resolved at process entry (main / MAIN / TEST).
//  Default PLAIN matches the "stdout is a pipe" non-TTY case, which is
//  the safe choice for an uninitialised tool (e.g. a unit test that
//  forgets to set it).
HUNKout HUNKMode = HUNKOutPlain;

ok64 HUNKu8sFeedOut(u8s into, hunk const *hk) {
    sane(u8sOK(into) && hk != NULL);
    switch (HUNKMode) {
    case HUNKOutTLV:   return HUNKu8sFeed     (into, hk);
    case HUNKOutColor: return HUNKu8sFeedColor(into, hk);
    case HUNKOutPlain: return HUNKu8sFeedText (into, hk);
    }
    fail(FAILSANITY);
}

// One line slice — content between leading `-`/`+` prefix and trailing '\n'.
typedef struct { u8c *lo; u8c *hi; } hk_line;

// Slurp one prefixed line (`-X\n` or `+X\n`) starting at *cur.
// Skips the prefix char, returns content bounds, advances *cur past '\n'.
// Returns YES on success, NO on no '\n' found.
static b8 hk_take_line(u8c **cur, u8c *end, hk_line *out) {
    if (*cur >= end) return NO;
    u8c *nl = (u8c*)memchr(*cur, '\n', (size_t)(end - *cur));
    if (!nl) return NO;
    out->lo = *cur + 1;   // skip prefix char
    out->hi = nl;
    *cur = nl + 1;
    return YES;
}

// Common prefix (chars matching from start) + common suffix (chars
// matching from end, not overlapping the prefix).  Used to gauge how
// "small" the edit between two lines is — token-level diff routinely
// fails to pair small-edit lines (one token changed) into a single
// merged-text line, so the renderer pairs them at line level.
static u32 hk_shared(hk_line a, hk_line b) {
    u32 alen = (u32)(a.hi - a.lo);
    u32 blen = (u32)(b.hi - b.lo);
    u32 lim  = alen < blen ? alen : blen;
    u32 lcp = 0;
    while (lcp < lim && a.lo[lcp] == b.lo[lcp]) lcp++;
    u32 lcs = 0;
    while (lcs < lim - lcp && a.hi[-1 - (i32)lcs] == b.hi[-1 - (i32)lcs])
        lcs++;
    return lcp + lcs;
}

// Two lines are a "small edit" if shared bytes (LCP+LCS) cover at
// least 3/4 of the longer line — i.e. less than 1/4 of the line was
// touched.
static b8 hk_small_edit(hk_line a, hk_line b) {
    u32 alen = (u32)(a.hi - a.lo);
    u32 blen = (u32)(b.hi - b.lo);
    u32 mx   = alen > blen ? alen : blen;
    if (mx == 0) return YES;
    u32 sh = hk_shared(a, b);
    return (sh * 4 >= mx * 3) ? YES : NO;
}

#define HK_REGION_MAX 4096

// Append `<prefix><content>\n` for a line slice.
static void hk_emit_line(Bu8 body, u8 prefix, hk_line ln) {
    u8bFeed1(body, prefix);
    u8csc s = {ln.lo, ln.hi};
    u8bFeed(body, s);
    u8bFeed1(body, '\n');
}

// Flush a diff region's accumulated `-` lines and `+` lines into `body`.
// Three-stage cleanup:
//   1. exact-match prefix at line granularity → context
//   2. exact-match suffix at line granularity → context
//   3. similarity-based pairing in the middle: each `+` line is matched
//      to the most-similar unpaired `-` line (greedy max-shared); pairs
//      get emitted as `-X\n+Y\n` adjacent at the `+`'s position so a
//      one-token edit shows up as a `-/+` pair instead of a big DEL run
//      followed by a big INS run.
// Counts are unchanged regardless of the rearrangement.
static void hunk_flush_region(Bu8 body, Bu8 dels, Bu8 adds) {
    u8c *dp = u8bDataHead(dels);
    u8c *de = dp + u8bDataLen(dels);
    u8c *ap = u8bDataHead(adds);
    u8c *ae = ap + u8bDataLen(adds);

    // 1. Exact-match prefix → context.
    while (dp < de && ap < ae) {
        u8c *dnl = (u8c*)memchr(dp, '\n', (size_t)(de - dp));
        u8c *anl = (u8c*)memchr(ap, '\n', (size_t)(ae - ap));
        if (!dnl || !anl) break;
        size_t dlen = (size_t)(dnl - dp);
        size_t alen = (size_t)(anl - ap);
        if (dlen != alen || memcmp(dp + 1, ap + 1, dlen - 1) != 0) break;
        u8bFeed1(body, ' ');
        u8csc ctx = {dp + 1, dnl + 1};
        u8bFeed(body, ctx);
        dp = dnl + 1;
        ap = anl + 1;
    }

    // 2. Exact-match suffix → context.
    u8c *de_end = de;
    u8c *ae_end = ae;
    while (dp < de_end && ap < ae_end) {
        u8c *dlast = de_end - 1;
        if (*dlast != '\n') break;
        u8c *dline = dlast - 1;
        while (dline >= dp && *dline != '\n') dline--;
        dline++;
        u8c *alast = ae_end - 1;
        if (*alast != '\n') break;
        u8c *aline = alast - 1;
        while (aline >= ap && *aline != '\n') aline--;
        aline++;
        size_t dlen = (size_t)(dlast - dline);
        size_t alen = (size_t)(alast - aline);
        if (dlen != alen || memcmp(dline + 1, aline + 1, dlen - 1) != 0) break;
        de_end = dline;
        ae_end = aline;
    }

    // 3. Similarity-based pairing on the middle.
    static hk_line dl[HK_REGION_MAX], al[HK_REGION_MAX];
    u32 nd = 0, na = 0;
    {
        u8c *cur = dp;
        while (nd < HK_REGION_MAX && hk_take_line(&cur, de_end, &dl[nd])) nd++;
    }
    {
        u8c *cur = ap;
        while (na < HK_REGION_MAX && hk_take_line(&cur, ae_end, &al[na])) na++;
    }

    static i32 pair_a[HK_REGION_MAX];   // pair_a[j] = i if A[j]↔D[i], else -1
    static i32 pair_d[HK_REGION_MAX];
    for (u32 j = 0; j < na; j++) pair_a[j] = -1;
    for (u32 i = 0; i < nd; i++) pair_d[i] = -1;

    // Greedy 2D max-shared pairing: at each step pick the (i, j) with
    // the highest shared-byte count among unpaired pairs that pass the
    // small-edit threshold; pair them; repeat until no qualifying pair.
    for (;;) {
        u32 best_sh = 0;
        i32 best_i = -1, best_j = -1;
        for (u32 i = 0; i < nd; i++) {
            if (pair_d[i] >= 0) continue;
            for (u32 j = 0; j < na; j++) {
                if (pair_a[j] >= 0) continue;
                if (!hk_small_edit(dl[i], al[j])) continue;
                u32 sh = hk_shared(dl[i], al[j]);
                if (sh > best_sh) {
                    best_sh = sh;
                    best_i = (i32)i;
                    best_j = (i32)j;
                }
            }
        }
        if (best_i < 0) break;
        pair_d[best_i] = best_j;
        pair_a[best_j] = best_i;
    }

    // Emit in `adds` order.  A paired `+A[j]` is preceded by its
    // matched `-D[i]`; an unpaired `+A[j]` stands alone.
    for (u32 j = 0; j < na; j++) {
        if (pair_a[j] >= 0) hk_emit_line(body, '-', dl[pair_a[j]]);
        hk_emit_line(body, '+', al[j]);
    }
    // Trailing unpaired `-D[i]`s, in their original order.
    for (u32 i = 0; i < nd; i++) {
        if (pair_d[i] < 0) hk_emit_line(body, '-', dl[i]);
    }

    // Emit suffix-matched lines (collected in step 2) in forward order.
    u8c *sp = de_end;
    while (sp < de) {
        u8c *snl = (u8c*)memchr(sp, '\n', (size_t)(de - sp));
        if (!snl) break;
        u8bFeed1(body, ' ');
        u8csc ctx = {sp + 1, snl + 1};
        u8bFeed(body, ctx);
        sp = snl + 1;
    }
    u8bReset(dels);
    u8bReset(adds);
}

// Split a hunk URI into path slice + line number.  Path slice points
// into `uri_text`; line is 0 if not encoded.  The fragment has no
// grammar — only the trailing `[L]?<digits>` convention is recognised
// (see hunk_frag_split above).
static void hunk_loc(u8csc uri_text, u8cs out_path, u32 *out_line) {
    out_path[0] = NULL;
    out_path[1] = NULL;
    *out_line = 0;
    if ($empty(uri_text)) return;
    uri u = {};
    u8csc text = {uri_text[0], uri_text[1]};
    if (DOGParseURI(&u, text) != OK) return;
    if (!$empty(u.path)) {
        $mv(out_path, u.path);
        if (!$empty(out_path) && *out_path[0] == '/')
            u8csUsed(out_path, 1);
    }
    if (!$empty(u.fragment)) {
        u8csc fr = {u.fragment[0], u.fragment[1]};
        *out_line = HUNKu8sFragSplit(fr, NULL);
    }
}

static ok64 HUNKu8sFeedLineBased(u8s into, hunk const *hk) {
    sane(u8sOK(into) && hk != NULL);

    u8cs path = {};
    u32 line = 0;
    hunk_loc(hk->uri, path, &line);

    // For grep/cat-style hunks (no diff sides), emit a plain-mode-shaped
    // `--- URI ---` header and the verbatim text — not patchable, but
    // informative.  LineBased mode targets `git apply` / `patch`, which
    // are non-color tools, so it tracks plain mode's separator rule.
    if ($empty(hk->text)) {
        call(hunk_feed_header_plain, into, hk);
        if ($empty(hk->uri)) u8sFeed1(into, '\n');
        done;
    }

    if (!hunk_has_diff(hk)) {
        call(hunk_feed_header_plain, into, hk);
        u32 tlen = (u32)$len(hk->text);
        call(hunk_feed_visible, into, hk, 0, tlen);
        if (tlen > 0 && hk->text[0][tlen - 1] != '\n')
            u8sFeed1(into, '\n');
        u8sFeed1(into, '\n');
        done;
    }

    //  Build the unified-diff body in a side buffer, then emit standard
    //  headers (`--- a/path` / `+++ b/path` / `@@ -L,C +L,C @@`).
    //  Within each diff region (run of non-context lines), DEL lines
    //  are buffered into `dels` and INS into `adds`; at each context
    //  line (or end of hunk), `dels` are flushed before `adds` so the
    //  output is `patch`-applicable (token-level merging in TDIFF can
    //  produce merged-text bytes in INS-before-DEL order — the renderer
    //  reorders here to keep the `-` / `+` convention).
    //
    //  Per-line accumulation:
    //    oldb = bytes of the current OLD-side line in progress (eq+rm)
    //    newb = bytes of the current NEW-side line in progress (eq+in)
    //    old_dirty = oldb has any rm byte
    //    new_dirty = newb has any in byte
    u8c *base = hk->text[0];
    int  n_toks = (int)$len(hk->toks);

    Bu8 body = {};
    Bu8 dels = {}, adds = {};
    Bu8 oldb = {}, newb = {};
    call(u8bAllocate, body, 1UL << 16);
    call(u8bAllocate, dels, 1UL << 12);
    call(u8bAllocate, adds, 1UL << 12);
    call(u8bAllocate, oldb, 1UL << 12);
    call(u8bAllocate, newb, 1UL << 12);
    b8 old_dirty = NO, new_dirty = NO;
    u32 old_count = 0, new_count = 0;

    u32 prev = 0;
    for (int i = 0; i < n_toks; i++) {
        u32 span_hi = tok32Offset(hk->toks[0][i]);
        u8  side    = tok32Side(hk->toks[0][i]);
        u8  tag     = tok32Tag (hk->toks[0][i]);
        if (tag == 'U') { prev = span_hi; continue; }
        u8c *p = base + prev;
        u8c *e = base + span_hi;
        for (; p < e; p++) {
            u8 c = *p;
            b8 is_nl = (c == '\n');
            if (side == TOK_SIDE_RM) {
                if (is_nl) {
                    u8bFeed1(dels, '-');
                    a_dup(u8c, ob, u8bData(oldb));
                    u8bFeed(dels, ob);
                    u8bFeed1(dels, '\n');
                    u8bReset(oldb);
                    old_dirty = NO;
                    old_count++;
                } else {
                    u8bFeed1(oldb, c);
                    old_dirty = YES;
                }
            } else if (side == TOK_SIDE_IN) {
                if (is_nl) {
                    u8bFeed1(adds, '+');
                    a_dup(u8c, nb, u8bData(newb));
                    u8bFeed(adds, nb);
                    u8bFeed1(adds, '\n');
                    u8bReset(newb);
                    new_dirty = NO;
                    new_count++;
                } else {
                    u8bFeed1(newb, c);
                    new_dirty = YES;
                }
            } else { // EQ
                if (is_nl) {
                    b8 modified = old_dirty || new_dirty;
                    if (modified) {
                        u8bFeed1(dels, '-');
                        a_dup(u8c, ob, u8bData(oldb));
                        u8bFeed(dels, ob);
                        u8bFeed1(dels, '\n');
                        old_count++;
                        u8bFeed1(adds, '+');
                        a_dup(u8c, nb, u8bData(newb));
                        u8bFeed(adds, nb);
                        u8bFeed1(adds, '\n');
                        new_count++;
                    } else {
                        // Region break — flush dels/adds (with prefix
                        // and suffix line-matching to cancel token-
                        // level misalignments), then emit the context.
                        hunk_flush_region(body, dels, adds);
                        u8bFeed1(body, ' ');
                        a_dup(u8c, ob, u8bData(oldb));
                        u8bFeed(body, ob);
                        u8bFeed1(body, '\n');
                        old_count++;
                        new_count++;
                    }
                    u8bReset(oldb);
                    u8bReset(newb);
                    old_dirty = NO;
                    new_dirty = NO;
                } else {
                    u8bFeed1(oldb, c);
                    u8bFeed1(newb, c);
                }
            }
        }
        prev = span_hi;
    }

    //  Trailing partial line + final region flush.
    {
        b8 modified = old_dirty || new_dirty;
        if (u8bDataLen(oldb) > 0 || old_dirty) {
            if (modified) {
                u8bFeed1(dels, '-');
                a_dup(u8c, ob, u8bData(oldb));
                u8bFeed(dels, ob);
                u8bFeed1(dels, '\n');
                old_count++;
            } else {
                // Trailing context line (no terminating '\n').
                hunk_flush_region(body, dels, adds);
                u8bFeed1(body, ' ');
                a_dup(u8c, ob, u8bData(oldb));
                u8bFeed(body, ob);
                u8bFeed1(body, '\n');
                old_count++;
                new_count++;
            }
        }
        if (modified && u8bDataLen(newb) > 0) {
            u8bFeed1(adds, '+');
            a_dup(u8c, nb, u8bData(newb));
            u8bFeed(adds, nb);
            u8bFeed1(adds, '\n');
            new_count++;
        }
        hunk_flush_region(body, dels, adds);
    }

    //  Emit standard unified-diff headers.
    if (!$empty(path)) {
        a_cstr(amin, "--- a/");
        u8sFeed(into, amin);
        u8sFeed(into, path);
        u8sFeed1(into, '\n');
        a_cstr(aplu, "+++ b/");
        u8sFeed(into, aplu);
        u8sFeed(into, path);
        u8sFeed1(into, '\n');
    }
    {
        u32 ol_start = (line > 0) ? line : 1;
        u32 nu_start = ol_start;
        a_pad(u8, hh, 64);
        u8sFeed1(hh_idle, '@');
        u8sFeed1(hh_idle, '@');
        u8sFeed1(hh_idle, ' ');
        u8sFeed1(hh_idle, '-');
        utf8sFeed10(hh_idle, (u64)ol_start);
        u8sFeed1(hh_idle, ',');
        utf8sFeed10(hh_idle, (u64)old_count);
        u8sFeed1(hh_idle, ' ');
        u8sFeed1(hh_idle, '+');
        utf8sFeed10(hh_idle, (u64)nu_start);
        u8sFeed1(hh_idle, ',');
        utf8sFeed10(hh_idle, (u64)new_count);
        u8sFeed1(hh_idle, ' ');
        u8sFeed1(hh_idle, '@');
        u8sFeed1(hh_idle, '@');
        u8sFeed1(hh_idle, '\n');
        u8sFeed(into, u8bDataC(hh));
    }
    a_dup(u8c, bb, u8bData(body));
    u8sFeed(into, bb);

    u8bFree(body);
    u8bFree(dels);
    u8bFree(adds);
    u8bFree(oldb);
    u8bFree(newb);
    done;
}

void HUNKu32sClip(Bu8 arena, u32cs out, u32cs toks, u32 lo, u32 hi) {
    out[0] = NULL;
    out[1] = NULL;
    if ($empty(toks) || lo >= hi) return;
    int n = (int)$len(toks);
    // Find first tok overlapping [lo, hi)
    int first = 0;
    while (first < n && tok32Offset(toks[0][first]) <= lo) first++;
    // Find last tok overlapping [lo, hi)
    int last = first;
    while (last < n) {
        u32 tlo = (last > 0) ? tok32Offset(toks[0][last - 1]) : 0;
        if (tlo >= hi) break;
        last++;
    }
    if (first >= last) return;
    u32gp g = u32aOpen(arena);
    for (int i = first; i < last; i++) {
        u8 tag = tok32Tag(toks[0][i]);
        u8 side = tok32Side(toks[0][i]);
        u32 off = tok32Offset(toks[0][i]);
        if (off > hi) off = hi;
        u32 rebased = off - lo;
        u32gFeed1(g, tok32PackSide(tag, side, rebased));
    }
    u32aClose(arena, out);
}

// --- Source tokenizer wrapper around dog/TOK lexer ---

typedef struct {
    u32bp toks;
    u32   off;
} HUNKTokCtx;

static ok64 HUNKTokCB(u8 tag, u8cs tok, void *ctx) {
    sane(ctx != NULL);
    HUNKTokCtx *c = (HUNKTokCtx *)ctx;
    u32 end = c->off + (u32)$len(tok);
    u32 packed = tok32Pack(tag, end);
    call(u32bFeed1, c->toks, packed);
    c->off = end;
    return OK;
}

ok64 HUNKu32bTokenize(u32bp toks, u8csc source, u8csc ext) {
    sane(toks != NULL && $ok(source));
    if ($empty(source)) done;

    HUNKTokCtx ctx = {.toks = toks, .off = 0};

    u8cs ext_nodot = {};
    if (!$empty(ext) && ext[0][0] == '.') {
        ext_nodot[0] = ext[0] + 1;
        ext_nodot[1] = ext[1];
    } else {
        $mv(ext_nodot, ext);
    }

    TOKstate ts = {
        .data = {source[0], source[1]},
        .cb = HUNKTokCB,
        .ctx = &ctx,
    };
    call(TOKLexer, &ts, ext_nodot);
    done;
}

// Is `s` a plain identifier: [A-Za-z_][A-Za-z0-9_]* ?  When yes,
// HUNKu8sMakeURI emits it verbatim; otherwise it wraps the body in `'…'`
// and percent-escapes URI-illegal bytes via hunk_frag_esc.
static b8 hunk_is_ident(u8cs s) {
    if ($empty(s)) return NO;
    size_t n = (size_t)$len(s);
    u8 c = s[0][0];
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'))
        return NO;
    for (size_t i = 1; i < n; i++) {
        c = s[0][i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_'))
            return NO;
    }
    return YES;
}

ok64 HUNKu8sMakeURI(u8s into, u8csc path, u8csc symbol, u32 lineno) {
    sane(u8sOK(into));
    if (!$empty(path)) u8sFeed(into, path);
    b8 has_sym = !$empty(symbol);
    if (has_sym || lineno > 0)
        u8sFeed1(into, '#');
    if (has_sym) {
        u8cs sym = {symbol[0], symbol[1]};
        if (hunk_is_ident(sym)) {
            // Plain ident — emit verbatim
            u8sFeed(into, sym);
        } else {
            // Quote and percent-escape URI-illegal chars
            u8sFeed1(into, '\'');
            call(hunk_frag_esc, into, sym);
            u8sFeed1(into, '\'');
        }
        if (lineno > 0) {
            u8sFeed1(into, ':');
            u8sFeed1(into, 'L');
            utf8sFeed10(into, (u64)lineno);
        }
    } else if (lineno > 0) {
        u8sFeed1(into, 'L');
        utf8sFeed10(into, (u64)lineno);
    }
    done;
}
 
