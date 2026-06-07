#ifndef KEEPER_REFS_H
#define KEEPER_REFS_H

//  REFS: ULOG-backed ref/tip reflog for keeper.
//  See REF.md (next to this header) for the on-disk format.
//
//  Row shape: `<ron60-ts>\tset\t<ref-key>#?<40-hex-sha>\n` — a standard
//  dog/ULOG row where the verb is `set` and the URI's fragment carries
//  `?<sha>`.  REFSLoad re-serialises each row's URI via URIutf8Feed and
//  splits on `#` so callers still see `{key, val}` pairs (val = `?<sha>`).
//
//  Resolution: REFSResolve does two things in one reverse pass — host
//  substring match against the row's authority (so `//github?master`
//  finds `https://github.com/…?heads/master`) and refname equality /
//  heads|tags variant match against the row's query.  Most-recent row
//  wins on ambiguity; there is no separate alias file / alias verb.

#include "abc/INT.h"
#include "abc/URI.h"
#include "abc/RON.h"
#include "abc/FILE.h"
#include "abc/HEX.h"
#include "dog/DOG.h"   // DOGIsFullSha (REFSQueryKind pin test)

con ok64 REFSFAIL  = 0x6ce3dc3ca495;
con ok64 REFSNONE  = 0x6ce3dc5d85ce;
con ok64 REFSBAD   = 0x1b38f70b28d;
//  REFSCompareAndAppend: actual current value did not match expected_old.
//  Caller is expected to re-resolve and retry.
con ok64 REFSCAS   = 0x1b38f70c29c;
//  Stop-iteration sentinel: a REFSEach callback may return this to
//  short-circuit the walk without signalling an error.  REFSEach
//  swallows it and returns OK; real failures still surface as their
//  own non-OK codes.
con ok64 REFSSTOP  = 0x6ce3dc71d619;

#define REFS_FILE      "refs"
#define REFS_MAX_CHAIN 8
#define REFS_MAX_REFS  1024

//  Record kind.  Kept for REFADV's classification path; all rows emitted
//  by REFSAppend are REF_SHA today.
#define REF_SHA    2
#define REF_TAG    3
#define REF_BRANCH 4

typedef struct {
    ron60 time;
    u8cs  key;   // URI bytes up to '#'  (e.g. ?heads/main, //host?heads/main, ?HEAD)
    u8cs  val;   // URI fragment bytes   (`?<40-hex-sha>`)
    u8    type;  // REF_SHA (future: REF_TAG / REF_BRANCH)
} ref;

typedef ref *refp;
typedef ref const *refcp;

// Typed slices for ref arrays
typedef refp refs[2];    // mutable ref slice
typedef refcp refcs[2];  // const ref slice
typedef refp *refsp;
typedef refcp *refcsp;

// Match: key equality
fun b8 REFMatch(refcp a, u8csc key) {
    return u8csEq((u8c **)a->key, (u8c **)key);
}

// Compare by key (for dedup/sort)
fun int REFKeyCmp(refcp a, refcp b) {
    size_t al = u8csLen(a->key), bl = u8csLen(b->key);
    size_t ml = al < bl ? al : bl;
    int c = (ml == 0) ? 0 : memcmp(a->key[0], b->key[0], ml);
    if (c != 0) return c;
    return al < bl ? -1 : al > bl ? 1 : 0;
}

// --- Canonical-scope kind probe (URI-001 Stage 3) ---
//
// The funnel canonicalises a ref's SCOPE (the query) into one of three
// context-free shapes (revised 2026-06-07, see URI.mkd "Ref shapes").
// It does NOT pin a tip into a fragment — in argv URIs the fragment is
// verb payload (PATCH mode, GET `#~N`/`#sha`).  The resolved-sha pin
// lives in `--at` / REFS / display, never here:
//
//     ?/<project>             REFKIND_TRUNK     (trunk scope)
//     ?/<project>/<branch>    REFKIND_BRANCH    (branch scope, may nest)
//     ?/<project>/<full-sha>  REFKIND_DETACHED  (bare commit, no branch)
//
// DETACHED carries the sha IN the query — it is the ref's identity, with
// no branch to scope by; a full sha (`DOGIsFullSha`) distinguishes it
// from a branch named like a hash.
typedef enum {
    REFKIND_NONE     = 0,   // not a canonical scope (no leading /<project>)
    REFKIND_TRUNK    = 1,   // /<project>
    REFKIND_DETACHED = 2,   // /<project>/<full-sha>
    REFKIND_BRANCH   = 3,   // /<project>/<branch-path>
    REFKIND_TAG      = 4,   // reserved — REFSQueryKind never returns it
                            // (a tag waypoint needs a REFS lookup).
} refkind;

// Classify a canonical scope query by SHAPE only — no allocation, no
// REFS / pack lookup.  A leading `?` is tolerated; any `#<frag>` is
// ignored (the canonical scope carries none).  Requires a leading
// `/<project>`; a bareword / relative / unqualified query is
// REFKIND_NONE.  `canon` is not consumed.
fun refkind REFSQueryKind(u8csc canon) {
    a_dup(u8c, q, canon);
    if (!u8csEmpty(q) && *q[0] == '?') u8csUsed1(q);
    //  Defensive: drop any `#<frag>` — the canonical scope has none.
    {
        a_dup(u8c, s, q);
        if (u8csFind(s, '#') == OK) q[1] = s[0];
    }
    if (u8csEmpty(q) || *q[0] != '/') return REFKIND_NONE;
    u8csUsed1(q);                              // past leading '/'
    if (u8csEmpty(q)) return REFKIND_NONE;

    //  Project = up to the first '/'.  One segment ⇒ `/<project>` trunk.
    u8cs scan = {};
    u8csMv(scan, q);
    if (u8csFind(scan, '/') != OK) return REFKIND_TRUNK;
    u8cs project = {q[0], scan[0]};
    if (u8csEmpty(project)) return REFKIND_NONE;
    u8csUsed1(scan);                           // past the project '/'

    //  Rest = branch path or a detached full-sha.  A lone full-sha
    //  segment ⇒ DETACHED; anything else (incl. nested) ⇒ BRANCH.
    a_dup(u8c, rest, scan);
    if (u8csFind(rest, '/') == OK) return REFKIND_BRANCH;  // /<proj>/<a>/<b…>
    u8cs rs = {scan[0], scan[1]};
    if (DOGIsFullSha(rs)) return REFKIND_DETACHED;         // /<proj>/<sha>
    return REFKIND_BRANCH;                                  // /<proj>/<branch>
}

// --- Public API ---

//  Append one (ref-key, sha) pair with a monotonic timestamp.
//  `from_uri` is the canonical ref key (`?`, `?heads/<X>`,
//  `<peer-uri>?heads/<X>`, …); `to_uri` is bare 40-hex (or empty
//  for a deletion row).  Uses verb `get` — for remote observations
//  (the common case called by wire code).  Local-move writers
//  (sniff commit, keeper put, sniff checkout) use REFSAppendVerb
//  with REFSVerbPost.
ok64 REFSAppend(u8csc dir, u8csc from_uri, u8csc to_uri);

//  Append with an explicit verb — `REFSVerbGet()` for remote
//  observations, `REFSVerbPost()` for local moves.  See REF.md.
ok64 REFSAppendVerb(u8csc dir, ron60 verb, u8csc from_uri, u8csc to_uri);

//  Compare-and-swap append: append a new `post` row for `key` with
//  value `new` only when the current resolved value of `key` matches
//  `expected_old` (byte-equality on the bare 40-hex SHA).  Empty
//  `expected_old` means "key must be absent or tombstoned"; non-empty
//  means the current resolved value must match exactly.  On mismatch,
//  returns REFSCAS without writing; caller is expected to re-resolve
//  and retry.  Tombstoned keys are treated as absent (mirrors
//  REFSResolve which collapses zero-sha rows to REFSNONE).  This is a
//  best-effort serialization-of-intent — there is no extra lock, the
//  read-then-append window has the same race surface as any other
//  append in this module.
ok64 REFSCompareAndAppend(u8csc dir, u8csc key, u8csc expected_old, u8csc new_val);

//  Cached RON60 of the verbs REFS knows about.  `*Fail` rows are
//  journalled markers — recorded when an outbound op started but
//  failed before the corresponding success row could land (e.g.
//  fetch reached the peer but the post-fetch checkout crashed).
//  REFSResolve / REFSLoad ignore them; readers that care about
//  history (audit, recovery, debug) walk for them explicitly.
//
//  `delete` rows are the canonical tombstone shape: a row with
//  verb `delete` and an empty (or zero-sha) value.  REFSLoad /
//  REFSResolve walk in URI-key-only dedup order (via
//  `ULOGeachLatestKey`), so a `delete` row supersedes any earlier
//  `get`/`post` row for the same key regardless of verb.  The
//  legacy "post-with-zero-sha" tombstone shape is still recognised
//  for backward compat; new tombstone writers should use
//  `REFSVerbDelete`.
ron60 REFSVerbGet(void);
ron60 REFSVerbPost(void);
ron60 REFSVerbDelete(void);
ron60 REFSVerbGetFail(void);
ron60 REFSVerbPostFail(void);
ron60 REFSVerbSet(void);   //  legacy — only for reading old logs

//  Resolve a URI by reverse-scanning the ULOG.  Host-substring match +
//  refname/variant match; most-recent wins.  Fills `resolved`:
//    * query    — terminal 40-hex SHA (the matched row's `#fragment`)
//    * scheme/host/path — origin bytes of the matched row (for the
//      `//alias`-style transport-URI build done by keeper's get/post)
//    * fragment — matched row's `?query` (peer-side refname, e.g.
//      `heads/main`); lets `be post //host` recover the branch when
//      the input URI omits `?ref`
//  `arena` is a writable byte buffer that backs the filled slices; must
//  outlive the caller's use of `resolved`.
ok64 REFSResolve(urip resolved, u8bp arena, u8csc dir, u8csc uri);

//  Fill `out` with the source LOCATOR (scheme + authority + path, no
//  query/fragment) of this project's line-1 `get` row — its persisted
//  clone source (see Title.mkd "line-1 get row is the source").  The
//  first `get` row wins.  Slices point into `arena` — caller owns it
//  and must keep it alive while using `out`.  Returns REFSNONE when
//  the project has no `get` row (init-only / unreadable); callers that
//  gate submodule-recursion default treat that as no-recurse.
ok64 REFSSourceScheme(u8csc dir, urip out, u8bp arena);

//  Bulk append: each entry contributes one `set` row.  Timestamps are
//  assigned monotonically (the `time` field on input entries is
//  ignored — ULOG enforces strict monotonicity per file).
ok64 REFSSyncRecord(u8csc dir, refcp arr, u32 nrefs);

//  Load latest-per-key entries.  Key/val slices point into `arena` —
//  caller owns `arena` and must keep it alive until done with `arr`.
//  The ULOG file is closed before return.
ok64 REFSLoad(refp arr, u32p out_n, u32 max, u8b arena, u8csc dir);

//  Iterate latest (per key) entries.  The walk stops on the first
//  non-OK return from `cb`; `REFSSTOP` is treated as "stop, no
//  error" and converted to OK on return, so callbacks can use it to
//  short-circuit without polluting the iterator's error channel.
typedef ok64 (*refs_cb)(refcp r, void *ctx);
ok64 REFSEach(u8csc dir, refs_cb cb, void *ctx);

//  Iterate EVERY record (not deduped per key) in chronological order.
//  Yields the parsed URI components, timestamp, and verb directly so
//  callers can filter by host/path or replay history without losing
//  superseded rows.  `REFSSTOP` semantics match REFSEach.  Fail-marker
//  rows (`get_fail` / `post_fail`) are skipped; tombstones (`delete`)
//  and zero-sha rows are passed through — caller decides.
//
//  URI component slices passed to the callback point into the ULOG's
//  mmap and remain valid only for the duration of the callback (the
//  mmap is unmapped before REFSEachRecord returns).
typedef ok64 (*refs_record_cb)(uri const *u, ron60 ts, ron60 verb,
                               void *ctx);
ok64 REFSEachRecord(u8csc dir, refs_record_cb cb, void *ctx);

//  Compact: rewrite the ULOG keeping only the latest row per key.
ok64 REFSCompact(u8csc dir);

#endif
