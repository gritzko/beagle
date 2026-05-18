#ifndef DOG_GIT_CFG_H
#define DOG_GIT_CFG_H

//  CFG — gitconfig-family structural parser.  Covers
//      ~/.gitconfig          (user-level)
//      /etc/gitconfig        (system-level)
//      <repo>/.git/config    (per-repo)
//      <wt>/.gitmodules      (submodule registry)
//
//  Pull-mode API.  Caller supplies the input slice and a scratch
//  buffer; each `CFGu8sFeed` call advances to the next event and
//  populates the state struct:
//
//      [name]            -> sec=name,  sub=empty,  key=empty
//      [name "subname"]  -> sec=name,  sub=subname, key=empty
//      key = value       -> sec/sub still hold the active section,
//                           key/value populated
//
//  sec/sub are sticky across assignment events (they live in the
//  buffer's PAST).  key/value are valid until the next `CFGu8sFeed`
//  call (they live in DATA and get reclaimed).
//
//  Quoted values get `\n` `\t` `\b` `\"` `\\` decoded; backslash-newline
//  inside a value is a line continuation that joins segments.  Bare
//  values strip trailing whitespace.  Comment tails (`# ...` / `; ...`)
//  are tolerated but not emitted.
//
//  `.gitattributes` is a DIFFERENT grammar — not this parser.

#include "abc/BUF.h"
#include "abc/PATH.h"

con ok64 CFGBAD	  = 0x30f40b28d;       // lex did not converge
con ok64 CFGFAIL  = 0xc3d03ca495;      // generic
con ok64 CFGNOSEC = 0x30f41761c38c;    // section not found (callers)
con ok64 CFGNOKEY = 0x30f4176143a2;    // key not found (callers)
con ok64 CFGNOBUF = 0x30f41760b78f;    // scratch buffer exhausted

typedef struct CFGstate CFGstate;

struct CFGstate {
    u8cs   data;       // input slice (head advances on each Feed)
    u8bp   buf;        // caller-owned scratch; sized to fit longest
                       // (sec + sub) plus (key + value).  No realloc;
                       // pointers into PAST/DATA must stay stable.
    u8cs   sec;        // active section name (into buf PAST)
    u8cs   sub;        // active subsection (into PAST; empty when none)
    u8cs   key;        // current assignment key (into buf DATA)
    u8cs   value;      // current assignment value (into buf DATA)
};

//  Advance to the next event.
//      OK     — event ready.  Distinguish by `key`:
//                 u8csEmpty(key) → section change (sec/sub updated)
//                 otherwise      → assignment (sec/sub still valid)
//      NODATA — input exhausted, no pending event.
//      CFGBAD — parse error.
//      CFGNOBUF — scratch buffer too small.
ok64 CFGu8sFeed (CFGstate *state);

//  Iterative drain: yield one (section, key, val) per call.
//
//    * `section` is a sticky `path8b` — caller resets it once before
//      the loop, then threads it across calls.  On OK return its DATA
//      holds the active section as a `/`-joined path: `submodule/abc`,
//      `core`, etc.  Read via `u8bDataC(section)`.
//    * `*key` and `*val` slice into `buf`, valid until the next call.
//    * `ini` is consumed (head advances).
//    * `buf` is the per-event scratch for decoded bytes.
//
//  Returns:
//    OK     — one (key, val) drained.
//    NODATA — `ini` exhausted (EOF).
//    CFGBAD — unparseable.
//    CFGNOBUF — `buf` too small.
ok64 CFGu8sDrain (u8cs ini, u8bp buf,
                  path8bp section, u8csp key, u8csp val);

#endif
