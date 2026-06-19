#ifndef DOG_HUNK_H
#define DOG_HUNK_H

#include "abc/BUF.h"
#include "abc/RON.h"
#include "abc/TLV.h"
#include "dog/tok/TOK.h"

// "hunk" — the neutral file-fragment verb.  Search results, grep hits,
// cat-mode views, and other "this is a code snippet" hunks set this so
// bro's title bar gets the ULOG-shape row instead of the old violet
// `--- … ---` decoration.
con ron60 HUNK_VERB_HUNK = 0xb39caf;

// Drain-time validation failures for an untrusted token ('K') record.
//   HUNKTOKLEN — 'K' value length is not a whole multiple of sizeof(tok32).
//   HUNKTOKOOB — a tok32Offset exceeds the text length ($len(hk->text)).
// Both reject the hunk in HUNKu8sDrain so no renderer ever indexes
// hk->text past its end or aliases unaligned wire bytes as tok32.
con ok64 HUNKTOKLEN = 0x45e5d4758515397;
con ok64 HUNKTOKOOB = 0x45e5d475851860b;

// TLV type letters for hunk records
#define HUNK_TLV      'H'  // outer container
#define HUNK_TLV_TS   'T'  // ron60 timestamp (8 bytes LE)
#define HUNK_TLV_VRB  'V'  // ron60 verb       (8 bytes LE)
#define HUNK_TLV_URI  'U'  // hunk URI: path#symbol:line
#define HUNK_TLV_TXT  'X'  // source text bytes
#define HUNK_TLV_TOK  'K'  // tok32 array (packed u32 LE)

// A serializable code hunk.
// Location is a single URI: path#symbol:lineno (see dog/DOG.md).
// Display title is formatted at render time by parsing the URI.
//
// `ts` and `verb` carry ULOG-style event metadata when the hunk
// represents a per-file action (sniff GET status, sniff status,
// graf log row, ...).  Both default to 0 (= absent); they are
// emitted on the wire only when non-zero.  A "status hunk" is one
// where ts || verb is set and text/toks are empty — the renderer
// then emits a single `<date>\t<verb>\t<uri>\n` line in plain mode
// or the same with ANSI colors in color mode.
//
// Each `tok32` in `toks` carries a syntax tag (top 5 bits) and a 2-bit
// diff side (eq/in/rm) — see dog/TOK.h.  No separate hili stream.
typedef struct {
    ron60   ts;    // 0 = absent
    ron60   verb;  // 0 = absent
    u8cs    uri;   // e.g. "abc/MSET.h#MSETOpen:42"
    u8cs    text;  // source text bytes
    tok32cs toks;  // packed tok32: syntax fg + diff side
} hunk;

typedef hunk const hunkc;

// Order hunks by URI (path + location).
fun b8 hunkZ(hunk const *a, hunk const *b) { return u8csZ(&a->uri, &b->uri); }

// Generate hunks / hunkcs / hunkb / hunkcb / hunkbp etc.
// plus the usual bFeed/bFeed1/bDataLen/bDataHead... family.
#define X(M, name) M##hunk##name
#include "abc/Bx.h"
#undef X

// Producer callback: yields one hunk at a time.  Slices in `hk` are
// borrowed for the duration of the call (zero-copy into source buffers).
typedef ok64 (*HUNKcb)(hunkc *hk, void *ctx);

// Three output modes — chosen via the universal `--tlv` / `--color` /
// `--plain` CLI rule (`ANSIIsTTY()` picks COLOR vs PLAIN as default).
// `main()` resolves the mode once from the CLI flags and assigns it
// to the module-global `HUNKMode`; every emit site then calls
// `HUNKu8sFeedOut(into, hk)` and gets the right rendering with no
// extra plumbing.  Default (uninitialised process) is PLAIN.
typedef enum {
    HUNKOutTLV   = 0,
    HUNKOutColor = 1,
    HUNKOutPlain = 2,
    HUNKOutHtml  = 3,
} HUNKout;

extern HUNKout HUNKMode;

ok64 HUNKu8sFeedOut(u8s into, hunk const *hk);

// The ONE hunk-header drawer (BRO-002): renders the [BRO-001] banner —
// abbreviated date (only if `hk->ts`) + verb (only if set) + uri — for
// every hunk, status or content alike (there is no status-vs-content
// distinction).  No violet, no underline, no `--- … ---` dashes.  The
// output mode is the `mode` argument (the caller's own render mode, so
// HUNKu8sFeedColor always colours regardless of the process-global
// HUNKMode): Plain/TLV = `[<date> ][<verb> ]<uri>\n` (machine-parseable),
// Color = the THEME_BANNER black-on-pale-yellow SGR band, Html = the
// banner-coloured `<h3 class="banner">` row.  `cols > 0` space-fills the
// color band to the terminal edge (the width-aware bro layer passes its
// width); `cols == 0` frames just the content (piped color).  No-op on
// an empty URI with no ts/verb.  Advances into[0].
ok64 HUNKu8sFeedBanner(u8s into, hunk const *hk, HUNKout mode, u32 cols);

// Serialize a hunk as a nested TLV record.  Advances into[0].
// `ts` and `verb` are emitted as fixed 8-byte LE records only when
// non-zero, before the URI/text/toks payload.
ok64 HUNKu8sFeed(u8s into, hunk const *hk);

// Deserialize a hunk from TLV.  Advances from[0].
// Slices in hk point into the original data (zero-copy).
ok64 HUNKu8sDrain(u8cs from, hunk *hk);

// Rewrite a hunk URI by prefixing its path component with `prefix/`.
// Used to relay a submodule's report into the parent's stream: a child
// hunk for `src/foo.c` mounted at `vendor/sub` becomes
// `vendor/sub/src/foo.c`, with scheme/authority/query/fragment kept.
// An empty `prefix` copies the URI through unchanged.  Advances into[0].
ok64 HUNKu8sRebaseURI(u8s into, u8csc prefix, u8csc child_uri);

// Relay a child's TLV hunk stream (a `be --tlv` report — a sequence of
// HUNK 'H' records) into `into`, rendered via the current `HUNKMode`,
// with every hunk's URI path rebased under `prefix` (HUNKu8sRebaseURI).
// Hunks are emitted sequentially — never nested.  A clean end of stream
// (TLVNODATA) stops the relay; a malformed record propagates its error.
// Advances into[0].
ok64 HUNKu8sRelay(u8s into, u8csc prefix, u8csc child_tlv);

// Render a hunk as plain ASCII, no ANSI.  Two shapes:
//   - status hunk (ts||verb set, empty text): one ULOG-wire-shape
//     `<date>\t<verb>\t<uri>\n` line.
//   - content hunk (uri+text, optionally with diff sides in `toks`):
//     `--- <uri> ---\n` header (when URI is set), then body verbatim
//     with per-token '+'/'-'/' ' diff prefixes when sides are present,
//     plus a trailing blank line as a separator.  The `--- … ---`
//     dashes are the only visual cue between adjacent hunks in plain
//     mode — color mode replaces them with a title-tag color band.
// `U`-tagged URI bytes in the body stay invisible.  Advances into[0].
ok64 HUNKu8sFeedText(u8s into, hunk const *hk);

// Render a hunk with ANSI colors.  Same two shapes as
// `HUNKu8sFeedText`, but the content header is a bare `<uri>\n`
// painted in theme slot 'T' (title) — the color frames the hunk so the
// plain-mode dashes aren't needed.  Status lines get date in grey and
// verb in its `ULOGVerbColor`; diff bodies get `-`/`+` lines in red /
// green.  Advances into[0].
ok64 HUNKu8sFeedColor(u8s into, hunk const *hk);

// Render a hunk as HTML.  Three shapes match plain/color:
//   - status hunk (ts || verb set, empty text): one
//     `<div class="ulog"><span class="ts">…</span>
//      <span class="verb">…</span>
//      <span class="uri"><a href="…">…</a></span></div>` row.
//   - content hunk: `<div class="hunk"><h3>uri</h3>
//                    <pre>…tag-classed spans…</pre></div>`.
//     Each visible token becomes `<span class="t-X">bytes</span>` where
//     X is the syntax tag (`D`/`G`/`L`/`R`/`P`/`S`/`N`/`C`/`F`).
//     `U`-tagged tokens stay invisible (`.t-U { display: none }` in
//     the consumer's CSS).
//   - diff-marked hunks (per-token `inrm` side) wrap added tokens in
//     `<span class="diff-in">` and removed tokens in
//     `<span class="diff-rm">` — same pre-block, no separate table.
// All emitted bytes are HTML-safe (`<`/`>`/`&`/`"` escaped).
// Advances into[0].
ok64 HUNKu8sFeedHtml(u8s into, hunk const *hk);

// Proper line-based unified diff (`-<old>` / `+<new>` pairs with
// `@@ -L,C +L,C @@` headers, suitable for `git apply` / `patch`) is
// rendered internally whenever a hunk's URI scheme is `diff:`;
// `HUNKu8sFeedText` dispatches to it.  Producers opt in by prepending
// `diff:` to the hunk URI — see `graf/WEAVE.c`.  No standalone entry
// point: the three public renderers are `HUNKu8sFeed` (TLV),
// `HUNKu8sFeedText` (plain), and `HUNKu8sFeedColor` (ANSI).

// Clip file-level toks to [lo,hi), arena-write rebased entries.
// Output slice points into `arena` after this returns.
void HUNKu32sClip(Bu8 arena, u32cs out, u32cs toks, u32 lo, u32 hi);

// Tokenize source bytes via dog/TOK lexer dispatch.
// Strips a leading dot from `ext` if present.  Output is packed tok32
// (tag + end offset) appended to `toks`.
ok64 HUNKu32bTokenize(u32bp toks, u8csc source, u8csc ext);

// Maximum visible width of a formatted hunk title.
#define HUNK_TITLE_MAX 64

// Compose a hunk URI into `into`: path#symbol:Llineno  (`L`-prefix
// follows GitHub convention).  Any component may be empty/0 to omit.
ok64 HUNKu8sMakeURI(u8s into, u8csc path, u8csc symbol, u32 lineno);

// Best-effort extractor for the conventions HUNKu8sMakeURI emits.
// Returns the trailing line number (0 if absent).  When `out_sym` is
// non-NULL, it is populated with the symbol portion (fragment minus
// the trailing `:[L]?<digits>` suffix); surrounding `'…'` quotes are
// stripped.  Fragments are otherwise free-form — anything not matching
// these conventions stays in `out_sym` as the symbol body.
u32 HUNKu8sFragSplit(u8csc frag, u8cs out_sym);

// --- status/action row table (BE-007, ex dog/ROWS) ------------------
//
//  Beagle's verb reporters (`be status`, `ls:`, GET/PUT/DELETE/PATCH/
//  POST, keeper banners) all emit ONE output model (BRO-002): every
//  (sub)module emits ONE content hunk whose
//      uri  = the module address (`status:`, `ls:<prefix>`, wt root, …)
//      text = a per-row `<7-date> <3-verb> [<indent>]<path>[<mov><dst>]`
//             table plus an invisible per-row navigation URI
//      toks = per-column tok32 tags ('L' date, the verb's palette slot,
//             the path tag) + a trailing 'U' tok over the hidden nav URI
//  rendered through `HUNKu8sFeedOut` (Text/Color/Html/TLV per `HUNKMode`).
//  The module hunk header IS the BRO-001 banner (date + verb + uri).
//
//  This is plain serialization — NO row/table data types.  The active
//  table is a process-global accumulator (single-threaded per process),
//  armed by HUNKTableOpen and flushed by HUNKTableClose; the producer's
//  scattered emit sites just call HUNKTablePrintRow / HUNKTableSummary.
//
//  Flush is mode-keyed: TLV/relay BUFFERS every row → one table hunk at
//  Close (clean per-module grouping for the relay); direct-tty STREAMS
//  each row LIVE (line-by-line clone progress).  `batch=YES` always
//  buffers (whole-table consumers: `be status`, `ls:`).

//  Nav scheme for a row's hidden click-URI.  Plain `ron60` (the scheme
//  word, RON-encoded — `abc/ok64`), 0 = no nav URI emitted.
con ron60 HUNK_NAV_NONE   = 0;
con ron60 HUNK_NAV_CAT    = 0x27978;     // `cat:<target>`    open the file
con ron60 HUNK_NAV_DIFF   = 0xa2daaa;    // `diff:<target>`   what changed
con ron60 HUNK_NAV_LS     = 0xc37;       // `ls:<target>`     descend
con ron60 HUNK_NAV_COMMIT = 0x9f3c71b78; // `commit:?<query>` open commit

//  Arm the process-global active table.  `uri` is the module address
//  (borrowed — must outlive Close); `verb`/`ts` head the flushed hunk's
//  banner (0 = omit).  `batch=YES` always buffers one hunk (status/ls:);
//  `batch=NO` is mode-keyed (stream on a tty, buffer for the relay).
//  Pair every Open with exactly one Close.  The output sink defaults to
//  stdout; set `HUNKTableFd(fd)` after Open to retarget (stderr for
//  mutator streaming).
ok64 HUNKTableOpen(u8csc uri, ron60 verb, ron60 ts, b8 batch);

//  Retarget the active table's output sink (e.g. STDERR_FILENO).  No-op
//  with no active table.
void HUNKTableFd(int fd);

//  Append one event row to the active table (the ONE serializer).  Full
//  column control: `path` body, optional `mov_dst` (empty = no move),
//  `ts`/`verb` columns (0 ts → 7 blank date cols), `path_tag` tok over
//  the path column ('S' status / 'F' ls), `arrow` (YES → `<src> -> <dst>`
//  / NO → `<src>#<dst>`), `indent` left-pad cols, `nav` scheme +
//  `nav_target` bytes for the hidden click-URI.  Streams live or buffers
//  per the table's mode.  No-op with no active table.
ok64 HUNKu8sFeedRow(ron60 ts, ron60 verb, u8csc path, u8csc mov_dst,
                    u8 path_tag, b8 arrow, u32 indent,
                    ron60 nav, u8csc nav_target);

//  The ulogrec-taking conveniences (HUNKu8sFeedRec, HUNKTablePrintRow)
//  live in dog/ULOG.h — that header already includes this one, and
//  `ulogrec` cannot be referenced here without a circular include.

//  Append a trailing summary line (`staged N put row(s)`, `N change(s)`,
//  …) to the active table so the count rides the module hunk instead of
//  a bare stderr line (POST-018).  No date/verb/nav columns; the line
//  wears the neutral 'S' tag.  No-op with no active table.
ok64 HUNKTableSummary(u8csc text);

//  The active table's accumulating text / toks buffers — for a producer
//  rendering a rich multi-tok summary tail straight into the hunk (e.g.
//  `be status`'s per-bucket coloured count line).  NULL with no active
//  table.  Only valid for a BUFFERING (batch) table.
u8bp  HUNKTableText(void);
u32bp HUNKTableToks(void);

//  Flush + release the active table.  Buffering tables emit ONE table
//  hunk (banner header = uri/verb/ts) via HUNKu8sFeedOut + the sink;
//  streaming tables already pushed each row (an empty streaming result
//  still owes its state banner).  Resets the global; no-op when none is
//  open.
ok64 HUNKTableClose(void);

#endif
