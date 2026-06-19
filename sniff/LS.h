#ifndef SNIFF_LS_H
#define SNIFF_LS_H

//  LS — `ls:[<path>][?<ref>]` and `lsr:[<path>][?<ref>]` projectors.
//  Both list the wt (optionally scoped to `<path>/`) with one **status
//  hunk** per entry, identical status set to bare `be` (see
//  sniff/SNIFF.exe.c §"status_buckets"):
//
//      put | new | mov | mod | del | mis | unk | (eq is counted only)
//
//  `ls:`  — one-level listing.  Immediate file children get a normal
//           status row; every immediate subdirectory collapses into a
//           single `dir` row with uri `<sub>/`.  Subdir dedup relies
//           on `SNIFFClassify`'s sorted path order.
//  `lsr:` — recursive listing.  Every descendant under `<path>/` gets
//           its own status row; no `dir` rows.
//
//  `?ref` (default: sniff baseline from `.be/wtlog` last get/post/patch)
//  picks the *ours* tree used by the path classifier.  The wt is always
//  *theirs*.  No `?ref` → behaves identically to `be` (bare) on its
//  recursion mode.  An explicit `?ref` changes the meaning of
//  `mod`/`new`/`del`/`eq` (compared against that ref's tree);
//  `mov`/`mis`/`unk` are stamp-driven and don't move.
//
//  Each entry is emitted via `HUNKu8sFeedOut`: one `HUNK_TLV` record
//  carrying `V` = status verb (ron60 from `SNIFFAtVerbOf("put")` etc.),
//  `T` = the row's ts (0 for `dir` rows), `U` = the per-entry navigation
//  target (`cat:<path>` for files, `ls:<sub>/` for directories,
//  `cat:<dst>` for `mov` rows), `X` = the displayed path (with `<src>
//  -> <dst>` for moves).  Plain / color / TLV selection lives in
//  `HUNKMode` — dog/HUNK.h does that conversion; the projector never
//  branches on output mode.
//
//  Submodules are filtered upstream by `SNIFFClassify`.  Clean
//  baseline-equal paths roll into the trailing `eq` count summary
//  (one trailing hunk, no body) so the listing isn't drowned by
//  unchanged files; pass `--all` (TODO) to surface them.

#include "abc/INT.h"
#include "abc/URI.h"

#include "SNIFF.h"

ok64 SNIFFLs (u8cs reporoot, uri const *u);   // one-level
ok64 SNIFFLsr(u8cs reporoot, uri const *u);   // recursive

//  Acquire the `ls:` one-level dedup buffer (`dir_seen`); `lsr:` needs
//  none.  The row table's text/toks live in the HUNK table
//  (HUNKTableOpen owns + unwinds them, BRO-002).  Exposed for the
//  leak-repro test (sniff/test/SNIFF.c); not the projector's contract.
ok64 SNIFFLsBufsAcquire(Bu8 dir_seen, b8 recurse);

#endif
