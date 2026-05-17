# DEF — symbol definition marking

`DEFMark()` takes a tokenized file and re-tags defined symbols from
`S` (default) to `N` (name definition), and call sites from `S` to
`C` (function call). Works in two steps:

1. **Enrichment**: map token array to a compact u8 byte stream
   (stripping whitespace/comments), where defining keywords become
   `f`, other keywords `r`, identifiers `s`, structural punctuation
   maps to itself (`( ) { } ; =`).

2. **Pattern matching**: run NFA regexes on the enriched stream.
   - C-family: `s[(][^(){};]*[)][{;]` finds function defs by
     checking that `s(...)` is followed by `{` or `;` with no `=`
     since the last statement boundary. `fs[{;]` catches struct/enum.
   - Keyword-prefix languages (Go, Python, Rust, JS): `fs` — a
     defining keyword followed by an identifier.
   - Calls (all languages): `s[(]` — any remaining identifier
     followed by `(`. Runs after definition marking, so definitions
     (already retagged `N`) are not affected.

Supported: c h cpp cc cxx hpp go py rs js jsx ts tsx java cs kt
swift dart zig.
