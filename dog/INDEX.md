# tok/ — Lightweight ragel-based tokenizers

Produces the same tag classification as ast/ tree-sitter parsers
(D=comment, G=string, L=number, R=keyword, P=punct, H=preproc, S=default)
but without the tree-sitter dependency.

## Core headers

| Header  | Purpose |
|---------|---------|
| DOG.h   | Shared dog primitives: URI parse/normalize/canonicalize (`DOGParseURI`, `DOGNormalizeArg`, `DOGCanonURI`/`DOGCanonURIFeed`), `/.be/`-anchor splitters (`DOGRepoFromBe`/`DOGProjectFromBe`/`DOGBranchFromBe`), path-hash, puppy-stack, and the **URI-002 bang factor**: bit byte `DOG_BANG_VERB`/`_PATH`/`_QUERY`/`_FRAG` (one per URI component, dog-side only — abc's `uri` stays pristine), plus the single uniform debanger every parser funnels through. `DOGDebangSlice(u8cs)` tail-sheds at most ONE trailing `!` (idempotent, present-but-empty preserved); `DOGDebang(uri*, u8*)` debangs all components and ORs the bits; `DOGDebangFeed(u8b*, bang, bit)` re-emits `!` for a set bit (used by `be`, the sole canonicalizer, so a bang survives path/branch resolution). Transport rule: URIs forward as full `u8cs` text (`!` rides along); every parser debangs locally. |
| TOK.h   | Common callback typedef `TOKcb`, dispatch API `TOKLexer()`, `TOKSplitText()` |
| BRCT.h  | Bracket matching and region detection on tokenized files |
| DEF.h   | Mark symbol definitions (S→N) via enrichment + NFA patterns, see [DEF.md](DEF.md) |
| HUNK.h  | Three-mode hunk renderer: `HUNKu8sFeed` (TLV wire), `HUNKu8sFeedText` (plain ASCII), `HUNKu8sFeedColor` (ANSI). Dispatch via module-global `HUNKMode` + `HUNKu8sFeedOut`. `diff:`-scheme hunks render as unified diff (LineBased) internally from FeedText. Plus `HUNKu8sDrain`, `HUNKu32sClip`, `HUNKu32bTokenize`, `HUNKu8sMakeURI`, `HUNKcb` |
| FREE.h  | Reusable "free text" scanner for natural-language slices (comment bodies, docstrings, markdown paragraphs).  Splits into UTF-8 words / numbers / punctuation / whitespace; fuses issue keys like `ABC-123` into one word token; emits `\n` as a standalone W token (never fused).  No strict UTF-8 validation — any `0x80..0xff` byte runs as a word char.  `FREELexer` emits native tags (`S`/`L`/`P`/`W`); `FREEu8sFeed(overlay, slice, cb, ctx)` wraps it with an overlay tag (e.g. pass `'D'` from a comment callback to retag every chunk).  Drop-in replacement for `TOKSplitText('D', …)` used by `CTonComment`, `MDTonComment`. |
| HOME.h  | Workspace finder + branch-sharding scaffolding: `HOMEFind` walks up to the nearest `.be` directory, `HOMEResolveSibling` finds tools next to the running binary, `HOMEOpenBranch`/`HOMEWriteBranch`/`HOMEBranchVisible` track the process-wide open-branch stack (slot 0 = writable, frozen at first rw open). Branch paths canonicalised via `dog/DPATH` helpers `DPATHBranchNormFeed`/`DPATHBranchAncestor`. Project derive (DIS-024): when the row-0 anchor names no project, `HOMEOpen` adopts the store's single `.be/<project>/` shard (`home_single_shard`, copy-safe); the walk's `home_dir_shieldlike` anchors a `.be/` with ≤1 subdir (single-project store) and only keeps ascending past a multi-project (>1 shard) store. |
| ULOG.h  | Append-only `<ts>\t<verb>\t<uri>\n` event log + persistent `wh128` sidecar index (`<dir>/.<base>.idx`). Index entries pack ts (60b key) and offset (40b) + verb-hash (20b prefilter) (val); tail sentinel records `(log_mtime, log_size)` — on Open, sidecar is trusted as-is only when both match `fstat` AND the last indexed row spans to the log's content end (`ulog_idx_spans_log`, DIS-033 false-fresh guard), rebuilt otherwise. RO+missing sidecar quietly falls back to anonymous mmap. Torn-log guard (ULOG-001): a NUL before real content (interrupted/ENOSPC/SIGKILL/page-cache-lost write) returns `ULOGTORN` on open instead of being trusted as empty and `FILETrimBook`-zeroed; original on-disk size is restored, live bytes left for recovery. |
| WHIFF.h | `wh64` packed words (off40 \| id20 \| type4) and `wh128` (key, val) pairs. Used by ULOG idx, keeper LSM index, graf entries. SHA-1 hashlet helpers (40-bit / 60-bit). |
| git/CFG.h | gitconfig-family lexer + iterative key/value drain (`~/.gitconfig`, `.git/config`, `.gitmodules`). `CFGLexer` ragel scanner emits low-level tokens (D comment, G quoted string, S bareword, P punct, W ws) for syntax-highlighting. `CFGu8sDrain(ini, key_buf, val)` consumes one kv at a time: the full dotted key (`submodule.abc.path`) lands in `key_buf`'s DATA — read it via `u8bDataC` — and `val` slices into `ini` (quotes stripped, trailing ws + inline `#`/`;` comments trimmed). Section prefix stays sticky in `key_buf` across calls; the next call strips back to the last `.` automatically. Returns NODATA at EOF, CFGBAD on parse error. |

## Tag mapping

| Tag | Meaning | Color |
|-----|---------|-------|
| D   | Comment | gray  |
| G   | String  | green |
| L   | Number  | cyan  |
| H   | Preproc/annotation/pragma | pink |
| R   | Keyword | red   |
| P   | Punctuation | gray |
| S   | Default (identifier, whitespace) | default |
| N   | Defined name (from DEF pass) | — |
| C   | Function call (from DEF pass) | — |
| F   | Filename or path | #490d49 |
| U   | URI (invisible click target; preceding token navigates here) | — |

## Language tokenizers

| Header  | Language | Extensions |
|---------|----------|------------|
| CT.h    | C | c h |
| CPPT.h  | C++ | cpp cc cxx hpp hh hxx |
| GOT.h   | Go | go |
| PYT.h   | Python | py |
| JST.h   | JavaScript | js jsx mjs |
| TST.h   | TypeScript | ts tsx |
| RST.h   | Rust | rs |
| JAT.h   | Java | java |
| KTT.h   | Kotlin | kt kts |
| SCLT.h  | Scala | scala sc |
| CST.h   | C# | cs |
| FSHT.h  | F# | fs fsi fsx |
| SWFT.h  | Swift | swift |
| DARTT.h | Dart | dart |
| DT.h    | D | d |
| ZIGT.h  | Zig | zig |
| HTMT.h  | HTML | html htm |
| CSST.h  | CSS | css |
| SCSST.h | SCSS | scss |
| JSONT.h | JSON | json |
| YMLT.h  | YAML | yml yaml |
| TOMLT.h | TOML | toml |
| SHT.h   | Bash | sh bash |
| RBT.h   | Ruby | rb |
| LUAT.h  | Lua | lua |
| PRLT.h  | Perl | pl pm |
| RT.h    | R | r R |
| ELXT.h  | Elixir | ex exs |
| ERLT.h  | Erlang | erl hrl |
| HST.h   | Haskell | hs |
| MLT.h   | OCaml | ml mli |
| JLT.h   | Julia | jl |
| NIMT.h  | Nim | nim nims |
| PHPT.h  | PHP | php |
| CLJT.h  | Clojure | clj cljs cljc edn |
| NIXT.h  | Nix | nix |
| SQLT.h  | SQL | sql |
| GQLT.h  | GraphQL | graphql gql |
| PRTT.h  | Protobuf | proto |
| HCLT.h  | HCL/Terraform | hcl tf |
| LAXT.h  | LaTeX | tex sty cls |
| VIMT.h  | VimL | vim |
| CMKT.h  | CMake | cmake |
| DKFT.h  | Dockerfile | dockerfile |
| MAKT.h  | Makefile | mk |
| FORT.h  | Fortran | f90 f95 f03 f08 |
| GLST.h  | GLSL | glsl vert frag geom comp |
| GLMT.h  | Gleam | gleam |
| ODNT.h  | Odin | odin |
| PWST.h  | PowerShell | ps1 psm1 psd1 |
| SOLT.h  | Solidity | sol |
| TYST.h  | Typst | typ |
| MDT.h   | Markdown (block+inline two-pass) | md markdown |
| AGDT.h  | Agda | agda |
| VERT.h  | Verilog | v sv |

## Regenerating

```sh
cd tok/
ragel -C XX.c.rl -o XX.rl.c -L
```

where XX is the module prefix (CT, GOT, PYT, JST, RST, etc).
