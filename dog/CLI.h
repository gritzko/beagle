#ifndef DOG_CLI_H
#define DOG_CLI_H

#include "abc/FILE.h"
#include "abc/URI.h"
#include "dog/HOME.h"
#include "dog/HUNK.h"

//  CLI_MAX_URIS — caps the URI count per invocation.  Glob expansions
//  (`be put test/*/*/*.txt`) routinely produce dozens of paths, so 16
//  was too tight; bumped to 1024.  Used as the alloc size for the
//  uri buffer; the buffer is heap-backed (CLAUDE.md §5).
#define CLI_MAX_URIS  1024
#define CLI_MAX_FLAGS 32  // pairs: 16 flags max — sized in entries

// Parsed CLI state.
//
//   dog [verb] [--flags] [URI...]
//
// Slices borrow from argv (no per-slice allocation).  Every non-flag
// arg's raw text is appended verbatim to `uris`; decomposition into
// components (path / query / fragment / …) is deferred to a
// per-entry parse-on-demand (CLIUriAt → DOGNormalizeArg).  There is
// no multi-arg fragment joining.  Free-form text (commit messages,
// search strings) reaches the dogs as a single whitespace-bearing
// arg (which DOGNormalizeArg classifies as fragment) or via the
// explicit `#` sigil; legacy `-m <msg>` is still accepted.
//
// `flags` is interleaved: [flag0, val0, flag1, val1, …].  Boolean
// flags get an empty val.  Use $for(u8cs, e, u8csbData(c->flags))
// to walk, or u8csbDataLen(c->flags) for the entry count
// (always even: flag + val = 2 per pair).
//
// `uris` and `flags` are heap-allocated by the entry frame (e.g.
// becli_inner) via u8csbAlloc; CLIParse appends into them via
// u8csbFeed1.  `uris` carries the UNPARSED arg text (one u8cs per
// non-flag arg, borrowing argv); decompose a single entry on demand
// with CLIUriAt (parse-on-demand).  Walk count with CLIUriLen(c).
typedef struct {
    u8cs   verb;                     // first arg matching verb_names
    u8csb  flags;                    // interleaved [flag, val] u8cs entries
    u8csb  uris;                     // raw (unparsed) URI arg text
    path8b repo;                     // repo root path; heap-allocated by entry frame
    u8     bang;                     // URI-002 bang bits (DOG_BANG_VERB set here)
    b8     wt_attached;              // GET-012: WorktreeAnchor wired a store-backed
                                     // worktree at cwd → keeper/spot/graf get skip
                                     // the clone (the shared store already has it)
} cli;

// Number of URI args parsed onto `c->uris`.
fun size_t CLIUriLen(cli const *c) { return u8csbDataLen(c->uris); }

// Raw (unparsed) text of URI arg `i` — borrows c->uris storage.
fun void CLIUriRawAt(u8csp out, cli const *c, size_t i) {
    u8cs const *e = u8csbAtP(c->uris, i);
    $mv(out, (*e));
}

// Replace the raw text of URI arg `i` (URI rewriters: bareword
// promotion, ref/remote resolution).  `raw` must point into storage
// that outlives `c` (a persistent scratch buffer — see the BE plan
// actions).
fun void CLIUriSetRaw(cli *c, size_t i, u8cs raw) {
    u8cs *slot = u8csbAtP(c->uris, i);
    $mv((*slot), raw);
}

// Parse the raw URI text `raw` into the transient `*out`, mirroring
// the exact normalization CLIParse applies to each non-flag arg
// (DOGNormalizeArg + restore the full arg into out->data).  The
// component slices VIEW into `raw`, so `raw` must outlive `*out`.
ok64 CLIUriParse(uri *out, u8csc raw);

// Parse URI arg `i` of `c` into the transient `*out`.  Slices view
// into the cli's borrowed argv text, stable for the lifetime of `c`.
ok64 CLIUriAt(uri *out, cli const *c, size_t i);

// Parse $args into cli struct. verb_names is a NULL-terminated
// array of known verb strings (or NULL to disable verb detection).
// Flags start with '-'; everything else is a URI.
// Flags that take values: if the flag appears in val_flags
// (e.g. "-g\0-C\0-r\0"), the next arg is consumed as value.
// Otherwise value is set to an empty (non-NULL) sentinel.
// Caller pre-allocates `c->repo` (PATHu8bAlloc) and frees it
// (PATHu8bFree) — CLAUDE.md §5: alloc at the top of the call chain.
ok64 CLIParse(cli *c, char const *const *verb_names,
              char const *val_flags);

// Check if a flag is set. Returns the value slice (non-empty
// for value flags, empty-but-non-NULL for boolean flags).
// Returns a NULL slice if not found.
fun void CLIFlag(u8csp out, cli const *c, char const *flag) {
    out[0] = NULL; out[1] = NULL;
    a_cstr(fs, flag);
    //  Buffer layout: c->flags is `u8cs *const [4] = {past, data,
    //  idle, end}`.  Walk DATA in [flag, val] pairs; an empty `val`
    //  slot is still non-NULL (CLIParse uses an empty-but-non-NULL
    //  sentinel).
    u8cs *head = c->flags[1];
    u8cs *end  = c->flags[2];
    for (u8cs *p = head; p + 1 <= end; p += 2) {
        if ($eq(*p, fs)) { $mv(out, *(p + 1)); return; }
    }
}

// Check if a flag is present (boolean test).
fun b8 CLIHas(cli const *c, char const *flag) {
    u8cs v = {};
    CLIFlag(v, c, flag);
    return v[0] != NULL;
}

// Compose `at` URI for `HOMEOpen`.  When `--at <uri>` is in `c->flags`,
// URILexer it into `*at`; the slices borrow from the cli's flag
// storage (stable for the lifetime of `c`).  When the flag is absent,
// `*at` stays zero (HOMEOpen will then auto-detect via cwd-walk).
//
// The flag's payload follows the at-log row shape:
//   `<root>?<branch>#<sha>` — path = repo root, query = current
//   branch (empty == trunk), fragment = 40-hex commit sha.
// `be` (the verb owner) is the producer; sub-dogs read it here.
fun void CLIAtURI(uri *at, cli const *c) {
    u8cs v = {};
    CLIFlag(v, c, "--at");
    zerop(at);
    if (v[0] == NULL || u8csEmpty(v)) return;
    u8csMv(at->data, v);
    URILexer(at);
}

//  Resolve the global `HUNKMode` (dog/HUNK.h) from `c`'s flags per the
//  universal rule:
//      --tlv    → HUNKOutTLV
//      --color  → HUNKOutColor    (overrides ANSIIsTTY())
//      --plain  → HUNKOutPlain    (overrides ANSIIsTTY())
//      default  → ANSIIsTTY() ? HUNKOutColor : HUNKOutPlain
//  Same three flags for every dog.  Call once from `main()` after
//  `CLIParse` (or any time the flag set changes); every downstream
//  HUNK emitter then picks the right shape via `HUNKu8sFeedOut`.
void CLISetHUNKMode(cli const *c);

#endif
