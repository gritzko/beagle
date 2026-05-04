#ifndef DOG_FRAG_H
#define DOG_FRAG_H

#include "abc/INT.h"

con ok64 FRAGFAIL = 0x3db2903ca495;
con ok64 FRAGBAD  = 0xf6ca40b28d;

// Fragment type after parsing.
//
// Search overload (#'spot'/`#name`/`#/regex/`) is gone — the verb-less
// projector schemes `spot:`, `grep:`, `regex:` carry search bodies now
// (VERBS.md §"View projectors").  FRAG only classifies the small set of
// in-band fragment shapes that survive: line jumps and ext filters.
#define FRAG_NONE  0
#define FRAG_LINE  2   // line number/range only

#define FRAG_MAX_EXTS 8

// Parsed URI fragment.
//
// Grammar (LINE + ext only):
//   fragment = line_or_range ext*
//            | ext+
//
//   line_or_range = NUMBER ('-' NUMBER)?
//   ext           = '.' [A-Za-z0-9]+
//
// Examples:
//   #42                 line
//   #10-20              range
//   #42.c               line + ext
//   #.c.h               ext-only
//
// Free-form fragment text (commit messages, symbol bodies emitted by
// HUNKu8sMakeURI for non-ident symbols) does NOT parse here — callers
// read u->fragment directly.  See bro/BRO.h:BROHunkLoc for the symbol
// extraction pattern.
typedef struct {
    u8cs body;          // unused; kept for ABI stability with old callers
    u32  line;          // line number (0 = not set)
    u32  line_end;      // range end (0 = not a range)
    u8cs exts[FRAG_MAX_EXTS];  // extension filters (sans dots)
    u8   nexts;         // count of ext filters
    u8   type;          // FRAG_NONE/FRAG_LINE
} frag;

typedef frag *fragp;
typedef frag const *fragcp;

// Parse a URI fragment string (the part after #, without the #).
// All slices point into the original input.
ok64 FRAGu8sDrain(u8cs input, fragp f);

// Percent-encode chars illegal in URI fragment (RFC 3986).
// Encodes: control chars (0x00-0x1F, 0x7F), non-ASCII (0x80+), '#', '%'.
// Everything else in printable ASCII passes through.
ok64 FRAGu8sEsc(u8s into, u8cs raw);

// Percent-decode %XX sequences.
ok64 FRAGu8sUnesc(u8s into, u8cs esc);

#endif
