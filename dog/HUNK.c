#include "HUNK.h"

#include <stdio.h>
#include <time.h>

#include "abc/ANSI.h"
#include "abc/TTY.h"
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

// Resolve a ron60 wall-clock stamp `ts` to a unix `time_t` (as i64),
// falling back to `now` when `ts` is unset or unparsable.  `isdst` is
// passed straight to `tm.tm_isdst` before `mktime`: callers pass -1 so
// mktime resolves DST itself (a 0 would shift a summer stamp +1h into
// the future — SUBS-003 / test/get/05,07).
static i64 hunk_ron60_to_time(ron60 ts, i64 now, int isdst) {
    if (!ts) return now;
    struct tm tm = {};
    if (RONToTime(ts, &tm, NULL) != OK) return now;
    tm.tm_isdst = isdst;
    time_t t = mktime(&tm);
    return (t != (time_t)-1) ? (i64)t : now;
}

// BRO-002 subsumed the separate status-hunk shape: every hunk now
// carries its row table in (text, toks) and renders through the single
// content + header path below.  The `hunk_is_status` heuristic and the
// three `hunk_feed_status_{plain,color,html}` banner formatters are gone;
// the ONE header drawer (HUNKu8sFeedBanner) IS the BRO-001 banner.

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
    sane(u8sOK(into));
    if ($empty(raw)) done;
    //  Uppercase %XX per RFC 3986 §2.1.  abc/HEX only ships the
    //  lowercase BASE16 table, so the percent-encode path keeps an
    //  uppercase lookup; the byte emission rides u8sFeed1 (no pointer
    //  arithmetic, SNOROOM propagated).
    static const u8c HEXUP[16] = "0123456789ABCDEF";
    $for(u8c, p, raw) {
        u8 c = *p;
        b8 legal = (c >= 0x20 && c <= 0x7E && c != '#' && c != '%');
        if (legal) {
            call(u8sFeed1, into, c);
        } else {
            call(u8sFeed1, into, (u8)'%');
            call(u8sFeed1, into, (u8)HEXUP[(c >> 4) & 0xF]);
            call(u8sFeed1, into, (u8)HEXUP[c & 0xF]);
        }
    }
    done;
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

// Copy a raw 'K' (TOK) record's bytes into aligned BASS scratch and
// assign the typed slice to `hk->toks`.  The wire bytes arrive at an
// arbitrary byte boundary (the TLV header offset), so they must NOT be
// aliased as `tok32c *` directly — that is an unaligned load.  We read
// each token's 4 LE bytes by hand and feed it into an aligned tok32
// gauge, then seal it.  Rejects a `'K'` length that isn't a whole
// multiple of sizeof(tok32) (HUNKTOKLEN) — a partial tail can't form a
// token and silently dropping it (the old behaviour) hides corruption.
static ok64 hunk_drain_toks(u8cs val, hunk *hk) {
    sane(1);
    size_t blen = (size_t)$len(val);
    if (blen % sizeof(tok32) != 0) fail(HUNKTOKLEN);
    size_t n = blen / sizeof(tok32);
    //  Gauge feeds advance BASS in place — they must NOT be wrapped in
    //  call() (which snapshots+rewinds BASS and would undo the feed, see
    //  ABC.md §BASS).  Feed directly and propagate the rc by hand.
    a_lign(tok32, g);
    a_dup(u8c, src, val);
    for (size_t i = 0; i < n; i++) {
        u32 v = 0;
        for (int b = 0; b < 4; b++)
            v |= ((u32)u8csAt(src, (size_t)b)) << (8 * b);
        u8csUsed(src, 4);
        __ = tok32gFeed1(g, (tok32)v);
        if (__ != OK) return __;
    }
    a_cquire(tok32, toks);
    $mv(hk->toks, toks);
    done;
}

// Every renderer indexes `hk->text` by `tok32Offset(...)`; an offset
// past the text length drives an OOB read (MEM-005).  Token offsets are
// monotonic end-offsets, so the last one bounds them all — but check
// each so a non-monotonic / corrupt stream can't slip a large value in
// behind a small final token.
static b8 hunk_toks_valid(hunk const *hk) {
    u32 tlen = (u32)$len(hk->text);
    int n = (int)$len(hk->toks);
    for (int i = 0; i < n; i++)
        if (tok32Offset(hk->toks[0][i]) > tlen) return NO;
    return YES;
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
            //  Validated + aligned copy into BASS scratch; never alias
            //  raw wire bytes.  Invoked directly (not via call()) so the
            //  call()-boundary BASS rewind can't reclaim the carved toks
            //  before the caller renders them — same lifetime as the
            //  zero-copy URI/text slices above (see ABC.md §BASS).
            __ = hunk_drain_toks(val, hk);
            if (__ != OK) return __;
            break;
        default:
            break;
        }
    }
    //  Cross-record gate: TXT and TOK are drained independently, so the
    //  text length is only known once the whole body is consumed.  Bound
    //  every token offset to it here so all renderers inherit the
    //  invariant `tok32Offset(t) <= $len(hk->text)`.
    if (!hunk_toks_valid(hk)) fail(HUNKTOKOOB);
    done;
}

ok64 HUNKu8sRebaseURI(u8s into, u8csc prefix, u8csc child_uri) {
    sane(u8sOK(into));

    //  Empty prefix → copy the URI through unchanged.
    if ($empty(prefix)) {
        if (!$empty(child_uri)) { a_dup(u8c, u, child_uri); call(u8sFeed, into, u); }
        done;
    }

    //  Parse the child URI (RFC, no dog promotion) so only the path is
    //  prefixed and scheme/authority/query/fragment survive intact.
    //  URIutf8Drain consumes its input, so lex a stable copy.
    a_pad(u8, dbuf, MAX_URI_LEN);
    if (!$empty(child_uri)) { a_dup(u8c, src, child_uri); call(u8bFeed, dbuf, src); }
    uri u = {};
    a_dup(u8c, dview, u8bData(dbuf));
    ok64 pe = URIutf8Drain(dview, &u);

    //  Build the prefixed path: <prefix>/<path-without-leading-slash>.
    a_pad(u8, pbuf, MAX_URI_LEN);
    call(u8bFeed, pbuf, prefix);
    if (pe == OK) {
        u8cs path = {};
        $mv(path, u.path);
        if (!$empty(path) && *path[0] == '/') u8csUsed(path, 1);
        if (!$empty(path)) { call(u8bFeed1, pbuf, '/'); call(u8bFeed, pbuf, path); }
        a_dup(u8c, ppath, u8bData(pbuf));
        return URIMake(into, u.scheme, u.authority, ppath, u.query, u.fragment);
    }

    //  Unparseable URI → literal `<prefix>/<uri>` join (best effort).
    if (!$empty(child_uri)) {
        a_dup(u8c, raw, child_uri);
        call(u8bFeed1, pbuf, '/');
        call(u8bFeed,  pbuf, raw);
    }
    a_dup(u8c, lit, u8bData(pbuf));
    return u8sFeed(into, lit);
}

//  Forward decl — token-offset clamp lives below (after the renderers).
static u32 hunk_clamp(hunk const *hk, u32 off);

//  Is `tag` a path-column tag?  ROWS tags the visible path column 'S'
//  (status rows) or 'F' (ls: rows); the relay re-prefixes those so a
//  child's bare `core.c` row becomes `vendor/sub/core.c` under its mount
//  point.  Every other column tag ('L' date, the verb slot, the neutral
//  'S' separators) is single-line and must NOT be touched — the path
//  column is disambiguated from a separator 'S' by the trailing '\n' it
//  alone carries (see ROWS.c::rows_emit).
static b8 hunk_tag_is_path(u8 tag) { return tag == 'S' || tag == 'F'; }

//  Rebase a relayed child hunk's (text, toks) table under `prefix`:
//  every per-row path column and hidden nav URI gets the subpath prefix
//  inserted, the same way HUNKu8sRebaseURI rewrites the banner URI.  The
//  rewritten text + toks are carved into BASS scratch (caller's call()
//  frame owns them, same lifetime as the drained zero-copy slices) and
//  assigned back into `hk`.  Token end-offsets are recomputed against the
//  grown text.  Empty `prefix` or empty body → no-op.
static ok64 hunk_rebase_body(hunk *hk, u8csc prefix) {
    sane(hk != NULL);
    if ($empty(prefix) || $empty(hk->text) || $empty(hk->toks)) done;

    //  The grown body never more than doubles the child's (a `<prefix>/`
    //  per path column + a rebased nav URI per row); 8 MiB covers the
    //  4 MiB ROWS_TEXT_CAP body with ample headroom.  Both the grown text
    //  and the recomputed toks live in fixed carved buffers (NOT an
    //  in-flight a_lign gauge) so the per-token `call(HUNKu8sRebaseURI)`
    //  below — which snapshots+rewinds BASS — can't corrupt an open gauge
    //  (ABC.md §BASS).
    a_carve(u8, nt, 1UL << 23);            // grown text (8 MiB)
    a_carve(tok32, nk, 1UL << 18);         // recomputed toks (256K, = ROWS_TOKS_CAP)

    int n = (int)$len(hk->toks);
    u32 prev = 0;
    for (int i = 0; i < n; i++) {
        tok32 t   = hk->toks[0][i];
        u8    tag  = tok32Tag(t);
        u8    side = tok32Side(t);
        u32   off  = hunk_clamp(hk, tok32Offset(t));
        if (off < prev) off = prev;             // monotonic guard
        a_part(u8c, span, hk->text, prev, off - prev);
        prev = off;

        if (tag == 'U') {
            //  Nav URI (cat:/diff:/ls:/commit:) — rebase its path the
            //  same way the banner URI is rebased.  commit:?<sha> has no
            //  path component, so HUNKu8sRebaseURI passes it through.
            a_dup(u8c, navsrc, span);
            call(HUNKu8sRebaseURI, u8bIdle(nt), prefix, navsrc);
        } else if (hunk_tag_is_path(tag) && !$empty(span) &&
                   *$last(span) == '\n') {
            //  Path column `[<indent-spaces>]<path>[<mov>]\n`.  Keep the
            //  indent, then splice `<prefix>/` ahead of the path bytes.
            a_dup(u8c, body, span);
            while (!$empty(body) && *body[0] == ' ') {
                call(u8bFeed1, nt, ' ');
                u8csUsed(body, 1);
            }
            //  An EMPTY path column (body is just the `\n` — a uri-only
            //  summary / branch-tip status row) must NOT get a prefix:
            //  splicing `<prefix>/` into it glues onto the prior column
            //  (the `okabc/` garbage).  Copy verbatim instead.
            if ($len(body) <= 1) {
                call(u8bFeed, nt, body);
            } else {
                call(u8bFeed, nt, prefix);
                call(u8bFeed1, nt, '/');
                call(u8bFeed, nt, body);
            }
        } else {
            //  Date / verb / separator columns — copy verbatim.
            call(u8bFeed, nt, span);
        }
        call(tok32bFeed1, nk, tok32PackSide(tag, side, (u32)u8bDataLen(nt)));
    }
    //  Any trailing bytes past the last token (defensive — ROWS always
    //  closes a row on a token, so this is normally empty).
    if (prev < (u32)$len(hk->text)) {
        a_part(u8c, tail, hk->text, prev, (u32)$len(hk->text) - prev);
        call(u8bFeed, nt, tail);
    }

    a_dup(u8c, newtext, u8bDataC(nt));
    u8csMv(hk->text, newtext);
    a_dup(tok32c, newtoks, tok32bDataC(nk));
    tok32csMv(hk->toks, newtoks);
    done;
}

ok64 HUNKu8sRelay(u8s into, u8csc prefix, u8csc child_tlv) {
    sane(u8sOK(into));
    a_dup(u8c, scan, child_tlv);
    while (!$empty(scan)) {
        hunk hk = {};
        ok64 dr = HUNKu8sDrain(scan, &hk);
        if (dr == TLVNODATA || dr == NODATA) break;  // clean end of stream
        if (dr != OK) return dr;                      // malformed record

        a_pad(u8, ubuf, MAX_URI_LEN);
        a_dup(u8c, orig, hk.uri);
        call(HUNKu8sRebaseURI, u8bIdle(ubuf), prefix, orig);
        a_dup(u8c, newuri, u8bData(ubuf));
        $mv(hk.uri, newuri);

        //  Rebase the per-row path columns + nav URIs inside the table
        //  body too, so a relayed sub row reads `vendor/sub/core.c`, not
        //  the bare `core.c` (BRO-003).
        call(hunk_rebase_body, &hk, prefix);

        call(HUNKu8sFeedOut, into, &hk);
    }
    done;
}

// Belt-and-suspenders: clamp a (drain-validated) token offset to the
// text length before using it as a byte index.  HUNKu8sDrain already
// rejects any hunk with `tok32Offset > $len(text)` (HUNKTOKOOB), so in
// a correctly-drained hunk this is a no-op — but a renderer reached on
// a hunk built by some other path stays memory-safe regardless.
static u32 hunk_clamp(hunk const *hk, u32 off) {
    u32 tlen = (u32)$len(hk->text);
    return off > tlen ? tlen : off;
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

//  Forward decl — HTML escaper lives in the HTML renderer section below;
//  the one banner drawer (HUNKu8sFeedBanner) uses it for Html mode.
static ok64 hunk_html_escape(u8s into, u8cs src);

// ==  The ONE hunk-header drawer (BRO-002)  =========================
//
// Every hunk header — content, status, action-table — renders through
// this single function.  There is NO status-vs-content distinction:
// the [BRO-001] banner IS the header.  It draws, in order:
//   * abbreviated date  — ONLY when `hk->ts` is set (7-col DOGutf8sFeedDate)
//   * verb              — ONLY when `hk->verb` is set (RONutf8sFeed)
//   * uri               — the module / hunk address
// No violet `THEMEAt('T')`, no underline, no `--- … ---` dashes, no
// per-column colours.  The three output modes are handled INSIDE here,
// branching on the `mode` argument (the caller's own render mode — each
// of HUNKu8sFeedText/Color/Html passes its mode, so HUNKu8sFeedColor
// always colours regardless of the process-global HUNKMode):
//   * Plain — `[<date> ][<verb> ]<uri>\n`, machine-parseable (the
//             producer column grammar that `ULOGu8sDrain` round-trips).
//   * Color — the THEME_BANNER (black-on-pale-yellow) SGR band; when
//             `cols > 0` the band is space-filled to the terminal edge
//             (bro passes its width), else it frames just the content
//             (piped color, width-agnostic).
//   * Html  — the banner-coloured `<h3 class="banner">` row.
//   * TLV   — handled by HUNKu8sFeed (wire), never reaches here.
// No-op on an empty URI with no ts/verb (a header-less content hunk).
ok64 HUNKu8sFeedBanner(u8s into, hunk const *hk, HUNKout mode, u32 cols) {
    sane(u8sOK(into) && hk);
    if ($empty(hk->uri) && !hk->ts && !hk->verb) done;

    if (mode == HUNKOutPlain || mode == HUNKOutTLV) {
        if (hk->ts) {
            i64 now = (i64)time(NULL);
            //  -1: let mktime resolve DST (BRO-002 / SUBS-003); aligns
            //  with every other ron60→date conversion (ROWS).
            i64 ts = hunk_ron60_to_time(hk->ts, now, -1);
            call(DOGutf8sFeedDate, into, ts, now);
            call(u8sFeed1, into, ' ');
        }
        if (hk->verb) {
            call(RONutf8sFeed, into, hk->verb);
            call(u8sFeed1, into, ' ');
        }
        call(u8sFeed, into, hk->uri);
        call(u8sFeed1, into, '\n');
        done;
    }

    if (mode == HUNKOutHtml) {
        a_cstr(s_h0, "<h3 class=\"banner\">");
        a_cstr(s_h1, "</h3>");
        call(u8sFeed, into, s_h0);
        if (hk->ts) {
            i64 now = (i64)time(NULL);
            i64 ts = hunk_ron60_to_time(hk->ts, now, -1);
            call(DOGutf8sFeedDate, into, ts, now);
            call(u8sFeed1, into, ' ');
        }
        if (hk->verb) { call(RONutf8sFeed, into, hk->verb); call(u8sFeed1, into, ' '); }
        if (!$empty(hk->uri)) {
            a_dup(u8c, esc_uri, hk->uri);
            call(hunk_html_escape, into, esc_uri);
        }
        call(u8sFeed, into, s_h1);
        done;
    }

    //  Color: the THEME_BANNER band.  Count the visible width so a `cols`
    //  request can space-fill the band to the terminal edge (bro's job;
    //  piped color passes cols == 0 and stays width-agnostic).
    ansi64 band = THEME_BANNER;
    call(ANSIu8sFeedDelta, into, band, ANSI_DEFAULT);
    u32 used = 0;
    if (hk->ts) {
        i64 now = (i64)time(NULL);
        i64 ts  = hunk_ron60_to_time(hk->ts, now, -1);
        call(DOGutf8sFeedDate, into, ts, now);   // centre-pads to 7 cols
        used += 7;
        call(u8sFeed1, into, ' '); used++;
    }
    if (hk->verb) {
        a_pad(u8, vbuf, 16);
        call(RONutf8sFeed, vbuf_idle, hk->verb);
        a_dup(u8c, vd, u8bDataC(vbuf));
        call(u8sFeed, into, vd); used += (u32)u8csLen(vd);
        call(u8sFeed1, into, ' '); used++;
    }
    if (!$empty(hk->uri)) {
        call(u8sFeed, into, hk->uri); used += (u32)u8csLen(hk->uri);
    }
    //  Full-width fill (only when a width was supplied) so the band reaches
    //  the right edge; otherwise the band frames just the content.
    while (used < cols) { call(u8sFeed1, into, ' '); used++; }
    //  Close the band before the newline so the colour doesn't bleed past
    //  the line.
    call(ANSIu8sFeedReset, into, band);
    call(u8sFeed1, into, '\n');
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
        u32 end = hunk_clamp(hk, tok32Offset(hk->toks[0][i]));
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
        u32 end = hunk_clamp(hk, tok32Offset(hk->toks[0][i]));
        u32 tlo = prev;
        u32 thi = end;
        prev = end;
        if (thi <= lo) continue;
        if (tlo >= hi) break;
        if (tok32Tag(hk->toks[0][i]) != 'U') continue;
        if (tlo < lo) tlo = lo;
        if (thi > hi) thi = hi;
        if (emit_lo < tlo) {
            a_part(u8c, vis, hk->text, emit_lo, tlo - emit_lo);
            call(u8sFeed, into, vis);
        }
        emit_lo = thi;
    }
    if (emit_lo < hi) {
        a_part(u8c, vis, hk->text, emit_lo, hi - emit_lo);
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

    //  Diff-marked hunks render as proper unified diff so the output
    //  is `git apply` / `patch` friendly.  Detected via URI scheme so
    //  producers explicitly opt in (a content hunk with stray rm/in
    //  tokens still renders as cat).
    if (hunk_uri_is_diff(hk)) return HUNKu8sFeedLineBased(into, hk);

    call(HUNKu8sFeedBanner, into, hk, HUNKOutPlain, 0);

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

    call(HUNKu8sFeedBanner, into, hk, HUNKOutColor, 0);

    if ($empty(hk->text)) {
        if ($empty(hk->uri)) u8sFeed1(into, '\n');
        done;
    }

    if (!hunk_has_diff(hk)) {
        //  Content hunk (grep / search / cat / sniff status): emit each
        //  token's bytes wrapped in its tag's THEME color so per-column
        //  hili (`L` date, `F` path, verb-palette `Y`/`W`/`V`/…) shows
        //  up in direct --color mode the same way bro renders TLV via
        //  the toks stream.  'U'-tagged ranges stay hidden.  No toks →
        //  fall back to verbatim emission (legacy callers).
        u32 tlen = (u32)$len(hk->text);
        int n_toks = (int)$len(hk->toks);
        if (n_toks == 0) {
            call(hunk_feed_visible, into, hk, 0, tlen);
        } else {
            ansi64 prev = ANSI_DEFAULT;
            u32 lo = 0;
            for (int i = 0; i < n_toks; i++) {
                u32 hi = hunk_clamp(hk, tok32Offset(hk->toks[0][i]));
                u8 tag = tok32Tag(hk->toks[0][i]);
                if (tag == 'U') { lo = hi; continue; }   // invisible
                ansi64 want = THEMEAt(tag);
                if (want != prev) {
                    call(ANSIu8sFeedDelta, into, want, prev);
                    prev = want;
                }
                if (hi > lo) {
                    a_part(u8c, span, hk->text, lo, hi - lo);
                    call(u8sFeed, into, span);
                }
                lo = hi;
            }
            if (prev != ANSI_DEFAULT)
                call(ANSIu8sFeedReset, into, prev);
        }
        //  No trailing blank line — the underlined header at the top
        //  of the NEXT hunk is the visual divider.  Just make sure
        //  the body ends with a newline.
        if (tlen == 0 || hk->text[0][tlen - 1] != '\n')
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

// --- HTML renderer -------------------------------------------------

//  Escape `src` into `into`, replacing `<` `>` `&` `"` with their HTML
//  entities; every other byte passes through verbatim.  No allocation.
static ok64 hunk_html_escape(u8s into, u8cs src) {
    sane(1);
    if (src[0] == NULL) done;
    for (u8c *p = src[0]; p < src[1]; p++) {
        switch (*p) {
            case '<':  { a_cstr(e, "&lt;");   call(u8sFeed, into, e); break; }
            case '>':  { a_cstr(e, "&gt;");   call(u8sFeed, into, e); break; }
            case '&':  { a_cstr(e, "&amp;");  call(u8sFeed, into, e); break; }
            case '"':  { a_cstr(e, "&quot;"); call(u8sFeed, into, e); break; }
            default:   call(u8sFeed1, into, *p);
        }
    }
    done;
}

//  Map a syntax tag to its `<span class="t-X">` open / close pair.
//  The plain-default `S` and unknown tags emit no wrapper — saves
//  bytes on the heaviest token type.  `U`-tagged tokens never reach
//  here (caller skips them).
static b8 hunk_tag_wraps(u8 tag) {
    switch (tag) {
        case 'D': case 'G': case 'L': case 'R': case 'P':
        case 'N': case 'C': case 'F': case 'H':
            return YES;
        default:
            return NO;
    }
}

static ok64 hunk_feed_tag_open(u8s into, u8 tag) {
    sane(1);
    //  Wire form: `<span class="t-X">` = 18 bytes.  Stack buf must
    //  fit that or u8bFeed returns SNOROOM and the whole renderer
    //  aborts mid-hunk.
    a_pad(u8, buf, 32);
    call(u8bFeed, buf, ((u8cs){(u8c *)"<span class=\"t-",
                                (u8c *)"<span class=\"t-" + 15}));
    call(u8bFeed1, buf, tag);
    call(u8bFeed, buf, ((u8cs){(u8c *)"\">", (u8c *)"\">" + 2}));
    a_dup(u8c, out_s, u8bData(buf));
    call(u8sFeed, into, out_s);
    done;
}

static ok64 hunk_feed_tag_close(u8s into) {
    a_cstr(s, "</span>");
    return u8sFeed(into, s);
}

//  Per-token side wrapper for diff-marked hunks.  Mirrors the
//  `.diff-in` / `.diff-rm` classes in woof/style.css.
static b8 hunk_side_wraps(u8 side) {
    return side == TOK_SIDE_IN || side == TOK_SIDE_RM;
}

static ok64 hunk_feed_side_open(u8s into, u8 side) {
    if (side == TOK_SIDE_IN) {
        a_cstr(s, "<span class=\"diff-in\">");
        return u8sFeed(into, s);
    }
    if (side == TOK_SIDE_RM) {
        a_cstr(s, "<span class=\"diff-rm\">");
        return u8sFeed(into, s);
    }
    return OK;
}

ok64 HUNKu8sFeedHtml(u8s into, hunk const *hk) {
    sane(u8sOK(into) && hk != NULL);

    //  Header: `<div class="hunk">` + the ONE banner drawer's
    //  `<h3 class="banner">…</h3>` (date + verb + uri), then `<pre>`.
    //  Empty URI with no ts/verb → the banner is a no-op, but the `<div>`
    //  stays so per-hunk margins still apply.
    { a_cstr(s, "<div class=\"hunk\">"); call(u8sFeed, into, s); }
    call(HUNKu8sFeedBanner, into, hk, HUNKOutHtml, 0);
    { a_cstr(s, "<pre>"); call(u8sFeed, into, s); }

    if (!$empty(hk->text)) {
        u32 tlen = (u32)$len(hk->text);
        int n_toks = (int)$len(hk->toks);

        if (n_toks == 0) {
            //  No toks: dump the whole text escaped, verbatim.
            a_dup(u8c, esc, hk->text);
            call(hunk_html_escape, into, esc);
        } else {
            //  Walk tokens; wrap each visible span in the matching
            //  tag class (and diff side, if present).  `U` tokens
            //  are click-target URIs — emit them as `</span><a href=…>`
            //  wrapping the *following* visible token would be ideal,
            //  but for v1 we just hide them (CSS `.t-U { display:none }`
            //  in the consumer).
            u32 lo = 0;
            for (int i = 0; i < n_toks; i++) {
                u32 hi  = hunk_clamp(hk, tok32Offset(hk->toks[0][i]));
                u8  tag = tok32Tag   (hk->toks[0][i]);
                u8  side= tok32Side  (hk->toks[0][i]);
                if (tag == 'U') { lo = hi; continue; }
                if (hi <= lo)   continue;

                b8 want_tag  = hunk_tag_wraps(tag);
                b8 want_side = hunk_side_wraps(side);

                if (want_side) call(hunk_feed_side_open, into, side);
                if (want_tag)  call(hunk_feed_tag_open,  into, tag);

                a_part(u8c, span, hk->text, lo, hi - lo);
                call(hunk_html_escape, into, span);

                if (want_tag)  call(hunk_feed_tag_close, into);
                if (want_side) call(hunk_feed_tag_close, into);

                lo = hi;
            }
            //  Tail bytes past the last token (rare but legal).
            if (lo < tlen) {
                a_part(u8c, span, hk->text, lo, tlen - lo);
                call(hunk_html_escape, into, span);
            }
        }
    }

    { a_cstr(s, "</pre></div>\n"); call(u8sFeed, into, s); }
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
    case HUNKOutHtml:  return HUNKu8sFeedHtml (into, hk);
    }
    fail(FAILSANITY);
}

// Slurp one prefixed line (`-X\n` or `+X\n`) off the head of `scan`.
// Skips the prefix char, returns content bounds (excluding the '\n')
// in `line_out`, advances `scan` past the '\n'.  Returns YES on
// success, NO when `scan` is empty or has no trailing '\n'.
static b8 hk_take_line(u8cs scan, u8csp line_out) {
    if (u8csEmpty(scan)) return NO;
    a_dup(u8c, body, scan);
    u8csUsed1(body);                          // skip the `-`/`+` prefix char
    a_dup(u8c, nl, body);
    if (u8csFind(nl, '\n') != OK) return NO;  // no terminating '\n' → NO
    line_out[0] = body[0];                    // content start (past prefix)
    line_out[1] = nl[0];                      // up to (not incl.) the '\n'
    scan[0] = nl[0];
    u8csUsed1(scan);                          // step past the '\n'
    return YES;
}

// Common prefix (chars matching from start) + common suffix (chars
// matching from end, not overlapping the prefix).  Used to gauge how
// "small" the edit between two lines is — token-level diff routinely
// fails to pair small-edit lines (one token changed) into a single
// merged-text line, so the renderer pairs them at line level.
static u32 hk_shared(u8cs a, u8cs b) {
    u32 alen = (u32)u8csLen(a);
    u32 blen = (u32)u8csLen(b);
    u32 lim  = alen < blen ? alen : blen;
    u32 lcp = 0;
    while (lcp < lim && a[0][lcp] == b[0][lcp]) lcp++;
    u32 lcs = 0;
    a_dup(u8c, ar, a);
    a_dup(u8c, br, b);
    while (lcs < lim - lcp && *u8csLast(ar) == *u8csLast(br)) {
        u8csShed1(ar);
        u8csShed1(br);
        lcs++;
    }
    return lcp + lcs;
}

// Two lines are a "small edit" if shared bytes (LCP+LCS) cover at
// least 3/4 of the longer line — i.e. less than 1/4 of the line was
// touched.
static b8 hk_small_edit(u8cs a, u8cs b) {
    u32 alen = (u32)u8csLen(a);
    u32 blen = (u32)u8csLen(b);
    u32 mx   = alen > blen ? alen : blen;
    if (mx == 0) return YES;
    u32 sh = hk_shared(a, b);
    return (sh * 4 >= mx * 3) ? YES : NO;
}

#define HK_REGION_MAX 4096

// Release the five scratch buffers owned by HUNKu8sFeedLineBased.  Used
// both at the normal exit and as the `callsafe` cleanup so an early
// error return on a flush failure does not leak them.
static void hk_free5(Bu8 body, Bu8 dels, Bu8 adds, Bu8 oldb, Bu8 newb) {
    u8bFree(body);
    u8bFree(dels);
    u8bFree(adds);
    u8bFree(oldb);
    u8bFree(newb);
}

// Append `<prefix><content>\n` for a line slice.
static ok64 hk_emit_line(Bu8 body, u8 prefix, u8cs ln) {
    sane(u8bOK(body));
    call(u8bFeed1, body, prefix);
    call(u8bFeed, body, ln);
    call(u8bFeed1, body, '\n');
    done;
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
static ok64 hunk_flush_region(Bu8 body, Bu8 dels, Bu8 adds) {
    sane(u8bOK(body) && u8bOK(dels) && u8bOK(adds));
    u8c *dp = u8bDataHead(dels);
    u8c *de = dp + u8bDataLen(dels);
    u8c *ap = u8bDataHead(adds);
    u8c *ae = ap + u8bDataLen(adds);

    // 1. Exact-match prefix → context.
    {
        u8cs ds = {dp, de}, as = {ap, ae};
        while (!u8csEmpty(ds) && !u8csEmpty(as)) {
            u8cs dl0 = {}, al0 = {};
            a_dup(u8c, dpeek, ds);
            a_dup(u8c, apeek, as);
            if (!hk_take_line(dpeek, dl0) || !hk_take_line(apeek, al0)) break;
            if (!u8csEq(dl0, al0)) break;     // contents differ → stop
            call(u8bFeed1, body, ' ');
            u8csc ctx = {dl0[0], dpeek[0]};   // content + its '\n'
            call(u8bFeed, body, ctx);
            u8csMv(ds, dpeek);                // commit the consumed lines
            u8csMv(as, apeek);
        }
        dp = (u8c *)ds[0];
        ap = (u8c *)as[0];
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
    static u8cs dl[HK_REGION_MAX], al[HK_REGION_MAX];
    u32 nd = 0, na = 0;
    {
        u8cs cur = {dp, de_end};
        while (nd < HK_REGION_MAX && hk_take_line(cur, dl[nd])) nd++;
    }
    {
        u8cs cur = {ap, ae_end};
        while (na < HK_REGION_MAX && hk_take_line(cur, al[na])) na++;
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
        if (pair_a[j] >= 0) call(hk_emit_line, body, '-', dl[pair_a[j]]);
        call(hk_emit_line, body, '+', al[j]);
    }
    // Trailing unpaired `-D[i]`s, in their original order.
    for (u32 i = 0; i < nd; i++) {
        if (pair_d[i] < 0) call(hk_emit_line, body, '-', dl[i]);
    }

    // Emit suffix-matched lines (collected in step 2) in forward order.
    u8c *sp = de_end;
    while (sp < de) {
        u8c *snl = (u8c*)memchr(sp, '\n', (size_t)(de - sp));
        if (!snl) break;
        call(u8bFeed1, body, ' ');
        u8csc ctx = {sp + 1, snl + 1};
        call(u8bFeed, body, ctx);
        sp = snl + 1;
    }
    u8bReset(dels);
    u8bReset(adds);
    done;
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

    // For grep/cat-style hunks (no diff sides), emit the plain-mode
    // banner header and the verbatim text — not patchable, but
    // informative.  LineBased mode targets `git apply` / `patch`, which
    // are non-color tools, so it tracks plain mode's separator rule.
    if ($empty(hk->text)) {
        call(HUNKu8sFeedBanner, into, hk, HUNKOutPlain, 0);
        if ($empty(hk->uri)) u8sFeed1(into, '\n');
        done;
    }

    if (!hunk_has_diff(hk)) {
        call(HUNKu8sFeedBanner, into, hk, HUNKOutPlain, 0);
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
        u32 span_hi = hunk_clamp(hk, tok32Offset(hk->toks[0][i]));
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
                        callsafe(hunk_flush_region(body, dels, adds),
                                 hk_free5(body, dels, adds, oldb, newb));
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
                callsafe(hunk_flush_region(body, dels, adds),
                         hk_free5(body, dels, adds, oldb, newb));
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
        callsafe(hunk_flush_region(body, dels, adds),
                 hk_free5(body, dels, adds, oldb, newb));
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

    hk_free5(body, dels, adds, oldb, newb);
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
    u32gp g = u32bAlign(arena);
    for (int i = first; i < last; i++) {
        u8 tag = tok32Tag(toks[0][i]);
        u8 side = tok32Side(toks[0][i]);
        u32 off = tok32Offset(toks[0][i]);
        if (off > hi) off = hi;
        u32 rebased = off - lo;
        u32gFeed1(g, tok32PackSide(tag, side, rebased));
    }
    u32bAcq(arena, out);
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
 
