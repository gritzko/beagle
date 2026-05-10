#ifndef GRAF_TDIFF_H
#define GRAF_TDIFF_H

#include "dog/HUNK.h"

con ok64 TDIFFFAIL = 0x1d3523cf3ca495;

// Token-level diff between two source files.  Computes the LCS edit
// list (via abc/DIFFx.h instantiated for u64 hashes), runs NEIL
// semantic cleanup, walks change regions with surrounding context,
// and yields one hunk per change region via cb.
//
// Pure library: no globals, no IO.  All scratch space comes out of
// the caller-supplied `arena` (a Bu8 mapped to ~16MB or more, see
// each call site for sizing).  The arena is rewound when the diff
// finishes; the caller is responsible for u8bMap / u8bUnMap.
//
// `ext_nodot` is the file extension without a leading dot, used to
// pick the dog/TOK lexer.  `dispname` is the NUL-terminated path
// shown in hunk titles (e.g. "src/foo.c").
//
// hunk slices passed to cb are borrowed; their backing memory
// is in `arena` and remains valid only for the duration of the call.
ok64 DIFFu8cs(Bu8 arena,
              u8cs old_data, u8cs new_data,
              u8cs ext_nodot, u8csc dispname,
              HUNKcb cb, void *ctx);

#endif
