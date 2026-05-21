#ifndef KEEPER_SUBS_H
#define KEEPER_SUBS_H

//  KEEPSubsAt — one-level submodule enumeration off a tree sha.
//
//  Produces a ULOG-shaped buffer (one row per declared submodule
//  whose 160000 entry exists in the tree).  Same shape contract as
//  `KEEPTreeULog`: `<ron60-ts>\t<ron60-verb>\t<uri>\n` rows, caller
//  supplies `ts`/`verb`, `out` is RESET on entry, rows are emitted
//  in `.gitmodules` declaration order.
//
//  URI shape per row: the upstream URL is the URI in question, with
//  the gitlink pin as fragment and the mount path tucked into the
//  query slot so a single row carries the full mount triple.
//
//      <url>?<mount-path>#<pin-hex>
//        scheme/auth/path = decomposed upstream URL from `.gitmodules`
//                           (e.g. ssh, github.com, /user/repo.git)
//        query            = mount path relative to the tree root
//        fragment         = 40-hex of the 160000 gitlink at that path
//
//  Sections in `.gitmodules` whose mount path is missing from the
//  tree are silently skipped — the tree, not the blob, is
//  authoritative for the live mount set.
//
//  Returns OK on success (including the empty case — no
//  `.gitmodules`, or `.gitmodules` declares zero matching subs;
//  `out` is empty in either case).  SUBSPARSE on a malformed
//  `.gitmodules`, KEEPFAIL on tree object malformed, other keeper
//  errors propagated.

#include "abc/INT.h"
#include "abc/RON.h"
#include "abc/BUF.h"
#include "dog/git/SHA1.h"
#include "dog/git/SUBS.h"   // SUBSPARSE / SUBSNOSEC

ok64 KEEPSubsAt(sha1cp tree_sha, ron60 ts, ron60 verb, u8bp out);

#endif
