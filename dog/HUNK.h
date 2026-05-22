#ifndef DOG_HUNK_H
#define DOG_HUNK_H

#include "abc/RON.h"
#include "abc/TLV.h"
#include "dog/tok/TOK.h"

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

// Required by the Bx.h template. Order hunks by URI (path + location).
fun int hunkcmp(hunk const *a, hunk const *b) { return $cmp(a->uri, b->uri); }

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
} HUNKout;

extern HUNKout HUNKMode;

ok64 HUNKu8sFeedOut(u8s into, hunk const *hk);

// Serialize a hunk as a nested TLV record.  Advances into[0].
// `ts` and `verb` are emitted as fixed 8-byte LE records only when
// non-zero, before the URI/text/toks payload.
ok64 HUNKu8sFeed(u8s into, hunk const *hk);

// Deserialize a hunk from TLV.  Advances from[0].
// Slices in hk point into the original data (zero-copy).
ok64 HUNKu8sDrain(u8cs from, hunk *hk);

// Render a hunk as plain ASCII, no ANSI, git-diff-ish style:
//   - status hunk (ts||verb set, empty text): single
//     `<ron60-ts>\t<ron60-verb>\t<uri>\n` line (the ULOG wire shape).
//   - otherwise: formatted title, then each text line prefixed with
//     '+'/'-'/' ' based on per-token diff side bits in `toks`.
// If no token has a non-eq side, every line gets a leading space
// (grep/cat output).  A blank line is appended after the hunk.
// Advances into[0].
ok64 HUNKu8sFeedText(u8s into, hunk const *hk);

// Render a hunk with ANSI colors — same shape as `HUNKu8sFeedText`
// but with the date column in grey, the verb in its ULOGVerbColor,
// and (for diff hunks) '-' lines in red / '+' lines in green.
// Advances into[0].
ok64 HUNKu8sFeedColor(u8s into, hunk const *hk);

// Render a hunk as proper line-based unified diff: a line with mixed
// INS+DEL spans is emitted as a `-<old>` + `+<new>` pair where <old>
// reconstructs the line without INS bytes and <new> reconstructs it
// without DEL bytes.  Pure-INS lines are `+<line>`, pure-DEL are
// `-<line>`, untagged are ` <line>` (context).  Suitable for piping
// to `git apply` / `patch` / IDEs that expect classic unified diff —
// what `HUNKu8sFeedText` does within a single line is too token-level
// for that audience.  Advances into[0].
ok64 HUNKu8sFeedLineBased(u8s into, hunk const *hk);

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

#endif
