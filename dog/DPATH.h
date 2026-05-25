#ifndef DOG_DPATH_H
#define DOG_DPATH_H

#include "abc/BUF.h"
#include "abc/INT.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/S.h"
#include "dog/DOG.h"

con ok64 DPATHFAIL = 0xd64a7513ca495;
con ok64 DPATHBAD  = 0x35929d44b28d;

// Validate and drain one path segment from a tree entry name.
//
// A segment is a single filename (no slashes — tree objects
// handle directories via nesting).  The ragel grammar accepts
// valid UTF-8 bytes excluding NUL, '/' and '\\', and rejects
// dangerous names:
//
//   .         (current dir)
//   ..        (parent dir escape)
//   .git      (case-insensitive)
//   .be     (case-insensitive)
//
// Returns OK and advances input past the segment.
// Returns DPATHBAD on dangerous names or invalid bytes.
// Returns DPATHFAIL on empty input.
ok64 DPATHu8sDrainSeg(u8cs input, u8cs out);

// Verify a tree entry name (single segment, no slashes).
// Returns OK if valid, DPATHBAD/DPATHFAIL otherwise.
fun ok64 DPATHVerify(u8csc name) {
    if ($empty(name)) return DPATHFAIL;
    a_dup(u8c, tmp, name);
    u8cs seg = {};
    ok64 o = DPATHu8sDrainSeg(tmp, seg);
    if (o != OK) return o;
    if (!$empty(tmp)) return DPATHBAD;
    return OK;
}

// --- Branch path normalization + ancestor test ---
//
// Canonical-form rules (for `.be/` sharding):
//   * trunk              = "" (empty slice)
//   * non-trunk branch   = path ending with '/', no leading '/'
//   * trunk aliases      = "", "main", "master", "trunk",
//                          "heads/main", "heads/master", "heads/trunk"
//                          (stripped of leading '/' and trailing '/')
//
// Callers feed the canonical form into an interning buffer and then
// compare by byte equality / prefix.

// Feed the canonical form of `in` into `out`.  No bytes are fed for
// trunk inputs.  Fails only if `out` runs out of room.
fun ok64 DPATHBranchNormFeed(u8b out, u8cs in) {
    a_dup(u8c, s, in);
    // Trim surrounding path separators using typed slice movers.
    while (!u8csEmpty(s) && *s[0] == '/') u8csUsed1(s);
    while (!u8csEmpty(s) && *$last(s) == '/') u8csShed1(s);
    if (u8csEmpty(s)) return OK;
    // Trunk aliases: any of these names (bare or with heads/ prefix)
    // normalize to the empty slice.
    a_cstr(a0, "main");
    a_cstr(a1, "master");
    a_cstr(a2, "trunk");
    a_cstr(a3, "heads/main");
    a_cstr(a4, "heads/master");
    a_cstr(a5, "heads/trunk");
    if (u8csEq(s, a0) || u8csEq(s, a1) || u8csEq(s, a2) ||
        u8csEq(s, a3) || u8csEq(s, a4) || u8csEq(s, a5))
        return OK;
    // Non-trunk: feed the stripped body plus a single trailing '/'.
    ok64 o = u8bFeed(out, s);
    if (o != OK) return o;
    return u8bFeed1(out, '/');
}

// YES iff `anc` is an ancestor of or equal to `des`, both already in
// canonical form (trunk = empty; non-trunk ends with '/').  Empty
// `anc` (trunk) is an ancestor of everything.
fun b8 DPATHBranchAncestor(u8cs anc, u8cs des) {
    if (u8csLen(anc) > u8csLen(des)) return NO;
    if (u8csEmpty(anc)) return YES;
    u8cs head = {des[0], des[0] + u8csLen(anc)};
    return u8csEq(anc, head);
}

// Longest shared '/'-bounded prefix of two canonical branch paths
// `a` and `b`, in bytes.  Returns the offset at which they first
// differ at a segment boundary; the slice [0, n) of either is the
// shared LCA.  Empty for trunk-only intersection.
//
// Examples:
//   ("feat/",     "other/")      → 0  (no shared segment)
//   ("feat/",     "feat/sub/")   → 5  (feat is ancestor of feat/sub)
//   ("feat/sub1/", "feat/sub2/") → 5  (feat/ shared)
//   ("feat/sub/",  "feat/sub/")  → 9  (identical)
//   ("",           "feat/")      → 0  (trunk LCA)
// Resolve a relative branch ref (`./X`, `../X`, `..`) against `current`
// (caller's current branch path, empty == trunk) into the path buffer
// `out`.  When `raw_query` is empty or its first chunk doesn't start
// with '.', `out` is left empty (caller treats raw_query as absolute).
// `was_relative_out` (optional) gets YES iff resolution ran.
//
// Branch semantics: popping past trunk yields trunk (no leading "..").
// Only the first chunk of `raw_query` (as drained by `DOGRefDrain`) is
// considered for the relative prefix; legacy `?<branch>&<sha>` queries
// route through the branch slot.
fun ok64 DPATHBranchResolveRel(u8b out, u8cs current, u8cs raw_query,
                               b8 *was_relative_out) {
    sane(out);
    u8bReset(out);
    if (was_relative_out) *was_relative_out = NO;
    if (u8csEmpty(raw_query)) done;

    a_dup(u8c, q_in, raw_query);
    u8cs first = {};
    DOGRefDrain(q_in, first);
    if ($empty(first) || first[0][0] != '.') done;
    if (was_relative_out) *was_relative_out = YES;

    if (!u8csEmpty(current)) call(PATHu8bFeed, out, current);
    u8cs rel = {first[0], first[1]};
    if ($len(rel) >= 2 && rel[0][0] == '.' && rel[0][1] == '/') {
        u8csUsed(rel, 2);
    } else if ($len(rel) >= 3 && rel[0][0] == '.' &&
               rel[0][1] == '.' && rel[0][2] == '/') {
        call(PATHu8bPop, out);
        u8csUsed(rel, 3);
    } else if ($len(rel) == 2 && rel[0][0] == '.' && rel[0][1] == '.') {
        call(PATHu8bPop, out);
        u8csUsed(rel, 2);
    }
    if (!$empty(rel)) call(PATHu8bPush, out, rel);
    done;
}

fun size_t DPATHBranchLcaLen(u8cs a, u8cs b) {
    size_t na = u8csLen(a), nb = u8csLen(b);
    size_t n = na < nb ? na : nb;
    size_t matched = 0;
    size_t last_slash = 0;
    for (; matched < n; matched++) {
        if (a[0][matched] != b[0][matched]) break;
        if (a[0][matched] == '/') last_slash = matched + 1;
    }
    if (matched == n) {
        if (na == nb) return na;
        u8cp longer_head = (na > nb) ? a[0] : b[0];
        if (longer_head[n] == '/') return n;
    }
    return last_slash;
}

#endif
