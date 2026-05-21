#ifndef KEEPER_RESOLVE_H
#define KEEPER_RESOLVE_H

//  RESOLVE — user-input → canonical commit sha.
//
//  The interpretation step happens ONCE per command, at the boundary
//  between user input and the be / sniff / graf / keeper internals.
//  Downstream code operates exclusively on full 40-hex shas and
//  absolute branch paths; KEEPResolveRef is the single funnel that
//  canonicalises everything else.
//
//  Token shapes accepted (matched in order):
//
//    * 40-hex full sha      → verify the object exists in keeper.
//    * 4..39 hex hashlet    → unique-prefix expand via the pack index.
//    * absolute branch path → REFSResolve (e.g. `feat`, `feat/sub`,
//                              `heads/main`, with the usual
//                              `refs/`/`heads/` strip-retry loop).
//    * relative branch path → resolved against `cur_branch` via
//                              dog/QURY (`./X`, `../X`, `..`), then
//                              REFSResolve.
//    * commit-message frag  → POSTPONED; today returns `RESOLVENONE`.
//                              Implementation (`keep_msg_search`)
//                              scans the recentmost commits across
//                              currently-open pack logs; lives in
//                              RESOLVE.c but isn't wired in yet.
//
//  All branch shapes pre-strip a leading `?` so the caller can pass
//  either `?feat` or `feat`.
//
//  `cur_branch` is the wt's current absolute branch path (empty for
//  trunk).  Pass an empty slice for non-wt contexts; relative refs
//  then return RESOLVEFAIL.

#include "abc/INT.h"
#include "dog/WHIFF.h"
#include "KEEP.h"

con ok64 RESLVNONE = 0x1b39c55f5d85ce;   // no match
con ok64 RESLVFAIL = 0x1b39c55f3ca495;  // malformed token / lookup error

//  Resolve `token` to a 40-byte commit sha.  Writes the canonical sha
//  into `*out` on success.  On miss returns RESOLVENONE; on hard
//  failure returns RESOLVEFAIL or the underlying keeper/REFS error.
ok64 KEEPResolveRef(sha1 *out, u8cs token, u8cs cur_branch);

//  Convenience wrapper for callers that take a user-typed hex token
//  (full 40-hex sha OR 6..39 hashlet prefix) and need the canonical
//  40-char ASCII form.  Composes KEEPResolveRef + sha1hexFromSha1;
//  passes empty cur_branch (hex tokens are not relative).  Returns
//  RESOLVENONE on miss, RESOLVEFAIL on malformed input.
ok64 KEEPResolveHex(sha1hex *out, u8cs token);

#endif
