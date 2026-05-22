#ifndef SNIFF_CAT_H
#define SNIFF_CAT_H

//  CAT — `cat:<path>[?<ref>]` projector.  Emits the wt file at
//  `<path>` (live on-disk bytes) as one hunk with token-level diff
//  hili (`I` / `D` / `' '`) against a baseline blob:
//    - `?ref` empty  → baseline = sniff's last get/post/patch row's
//                      tree at this path (i.e. "what `be` would call
//                      mod against").
//    - `?<ref|sha>`   → baseline = `<ref>`'s tree's blob at this path.
//
//  The wt-on-disk bytes are always `to`; the baseline is `from`.
//  Untracked → all `I`.  Missing on disk → all `D` (still useful as
//  "what was there").  Lexer syntax tags ride alongside the hili
//  tags; bro paints both.  Always emits TLV — plain / color rendering
//  is dog/HUNK's job.

#include "abc/INT.h"
#include "abc/URI.h"

#include "SNIFF.h"

ok64 SNIFFCat(u8cs reporoot, uri const *u);

#endif
