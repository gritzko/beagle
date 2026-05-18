#ifndef DOG_GIT_SUBS_H
#define DOG_GIT_SUBS_H

//  SUBS — git `.gitmodules` parser + synthesizer.
//
//  `.gitmodules` is gitconfig syntax with one `[submodule "<name>"]`
//  section per registered submodule.  Each section carries at least
//  `path =` and `url =`; other keys are tolerated and ignored.  The
//  parser builds on `dog/git/CFG`.
//
//  Pure data; no I/O, no allocations beyond caller-provided buffers.
//  Lifted out of `sniff/SUBS` so both keeper (`keeper/SUBS.h`) and
//  sniff can use it without sniff↔keeper dependency loops.

#include "abc/BUF.h"

con ok64 SUBSPARSE = 0x65bb73d4a495;  // malformed .gitmodules
con ok64 SUBSNOSEC = 0x65bb73d5d85ce; // no [submodule] for the queried path

//  Drain `.gitmodules` blob bytes; invoke `cb(path, url, ctx)` once
//  per `[submodule]` section that carries both `path` and `url`.
//  Slices passed to `cb` are valid for the lifetime of `blob`.
//  Sections missing either field are silently skipped — git tolerates
//  them and the parent tree's 160000 entry is the authoritative
//  mount, not the section.
//
//  Tolerant of: leading whitespace before keys, '\r' line endings,
//  '#'/';' comments, quoted section names (`[submodule "with space"]`),
//  surrounding whitespace around values.
//
//  Returns SUBSPARSE on a malformed section header (no closing `]`).
typedef ok64 (*subs_cb)(u8cs path, u8cs url, void *ctx);
ok64 SUBSu8sParse(u8cs blob, subs_cb cb, void *ctx);

//  Convenience: find the URL whose `[submodule]` section has
//  `path = <path>`.  `url_buf` is a caller-owned scratch buffer
//  (the URL bytes are copied into its DATA region).  On OK,
//  `*url_out` slices into `url_buf`'s DATA — valid for as long
//  as the caller's `url_buf` storage stays alive.
//  Returns SUBSNOSEC if no matching section, SUBSPARSE on parse
//  error, NOROOM if `url_buf` lacks capacity for the URL bytes.
ok64 SUBSu8sFind(u8cs blob, u8cs path, u8bp url_buf, u8csp url_out);

//  Emit a canonical `.gitmodules` blob into `out` (RESET on entry)
//  given two parallel newline-separated slice lists of equal length:
//  one of mount paths, one of URLs.  Mirrors `git submodule add`'s
//  output shape — one `[submodule "<path>"]` section per pair, with
//  `path = <path>` then `url = <url>`, tab-indented.  Empty input
//  produces an empty blob.  Returns SUBSPARSE on mismatched arity.
ok64 SUBSu8bSynth(u8bp out, u8cs paths, u8cs urls);

#endif
