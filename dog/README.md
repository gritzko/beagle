# libdog — lightweight ragel-based dogenizers

Fast, dependency-free dogenizers for syntax highlighting. Each dogenizer is a
hand-written [Ragel](http://www.colm.net/open-source/ragel/) scanner that
classifies source bytes into tagged tokens: **D**=comment, **G**=string,
**L**=number, **R**=keyword, **P**=punct, **H**=preproc, **S**=default.

libdog is an order of magnitude faster than tree-sitter, *but* it does not
produce a tree, just a token stream.

## Supported languages

| Module | Language     | Extensions              |
|--------|-------------|-------------------------|
| CT     | C           | `.c` `.h`               |
| CPPT   | C++         | `.cpp` `.cc` `.cxx` `.hpp` `.hh` `.hxx` |
| GOT    | Go          | `.go`                   |
| PYT    | Python      | `.py`                   |
| JST    | JavaScript  | `.js` `.jsx` `.mjs`     |
| TST    | TypeScript  | `.ts` `.tsx`            |
| RST    | Rust        | `.rs`                   |
| JAT    | Java        | `.java`                 |
| KTT    | Kotlin      | `.kt` `.kts`            |
| SCLT   | Scala       | `.scala` `.sc`          |
| CST    | C#          | `.cs`                   |
| FSHT   | F#          | `.fs` `.fsi` `.fsx`     |
| SWFT   | Swift       | `.swift`                |
| DARTT  | Dart        | `.dart`                 |
| DT     | D           | `.d`                    |
| ZIGT   | Zig         | `.zig`                  |
| HTMT   | HTML        | `.html` `.htm`          |
| CSST   | CSS         | `.css`                  |
| SCSST  | SCSS        | `.scss`                 |
| JSONT  | JSON        | `.json`                 |
| YMLT   | YAML        | `.yml` `.yaml`          |
| TOMLT  | TOML        | `.toml`                 |
| SHT    | Bash        | `.sh` `.bash`           |
| RBT    | Ruby        | `.rb`                   |
| LUAT   | Lua         | `.lua`                  |
| PRLT   | Perl        | `.pl` `.pm`             |
| RT     | R           | `.r` `.R`               |
| ELXT   | Elixir      | `.ex` `.exs`            |
| ERLT   | Erlang      | `.erl` `.hrl`           |
| HST    | Haskell     | `.hs`                   |
| MLT    | OCaml       | `.ml` `.mli`            |
| JLT    | Julia       | `.jl`                   |
| NIMT   | Nim         | `.nim` `.nims`          |
| PHPT   | PHP         | `.php`                  |
| CLJT   | Clojure     | `.clj` `.cljs` `.cljc` `.edn` |
| NIXT   | Nix         | `.nix`                  |
| SQLT   | SQL         | `.sql`                  |
| GQLT   | GraphQL     | `.graphql` `.gql`       |
| PRTT   | Protobuf    | `.proto`                |
| HCLT   | HCL/Terraform | `.hcl` `.tf`          |
| LAXT   | LaTeX       | `.tex` `.sty` `.cls`    |
| VIMT   | VimL        | `.vim`                  |
| CMKT   | CMake       | `.cmake`                |
| DKFT   | Dockerfile  | `.dockerfile`           |
| MAKT   | Makefile    | `.mk`                   |
| FORT   | Fortran     | `.f90` `.f95` `.f03` `.f08` |
| GLST   | GLSL        | `.glsl` `.vert` `.frag` `.geom` `.comp` |
| GLMT   | Gleam       | `.gleam`                |
| ODNT   | Odin        | `.odin`                 |
| PWST   | PowerShell  | `.ps1` `.psm1` `.psd1`  |
| SOLT   | Solidity    | `.sol`                  |
| TYST   | Typst       | `.typ`                  |
| AGDT   | Agda        | `.agda`                 |
| VERT   | Verilog     | `.v` `.sv`              |

## Usage

### CLI tool

```sh
toktok file.go          # prints TAG\tTOKEN per line
toktok file.py
toktok file.rs
```

### C API

```c
#include "TOK.h"

ok64 my_callback(u8 tag, u8cs tok, void *ctx) {
    // tag: 'D','G','L','R','P','H','S'
    // tok: [start, end) byte slice
    return OK;
}

// Dispatch by extension
TOKstate state = {
    .data = {src_start, src_end},
    .cb = my_callback,
    .ctx = NULL,
};
u8csc ext = {(u8c*)"py", (u8c*)"py" + 2};
ok64 o = TOKLexer(&state, ext);

// Or call a specific lexer directly
GOTstate go = {
    .data = {src_start, src_end},
    .cb = my_callback,
    .ctx = NULL,
};
ok64 o = GOTLexer(&go);
```

## Architecture

Each language tokenizer follows the same pattern:

- **XX.h** — Public header: state typedef, lexer declaration, error codes
- **XX.c** — Keyword table + callback functions (tag mapping)
- **XX.c.rl** — Ragel scanner source (hand-written `|* ... *|` scanner)
- **XX.rl.c** — Generated C (do not edit; regenerate with `ragel -C XX.c.rl -o XX.rl.c -L`)

The dispatcher **TOK.c** routes file extensions to the appropriate lexer.

## Tag mapping

| Tag | Meaning                 | ANSI Color |
|-----|------------------------|------------|
| D   | Comment                | gray       |
| G   | String literal         | green      |
| L   | Number literal         | cyan       |
| H   | Preprocessor/annotation| pink       |
| R   | Keyword                | red        |
| P   | Punctuation            | gray       |
| S   | Default (id/whitespace)| default    |
| U   | URI (invisible click target) | —          |

## Regenerating ragel output

```sh
cd tok/
ragel -C CT.c.rl -o CT.rl.c -L
ragel -C GOT.c.rl -o GOT.rl.c -L
# etc.
```

## Testing

```sh
ninja -C build-debug tok/test/TOK01test
build-debug/tok/test/TOK01test
```

Compare with tree-sitter output:
```sh
build-debug/tok/toktok sample.go > /tmp/tok.out
build-debug/ast/astok sample.go > /tmp/ast.out
diff /tmp/tok.out /tmp/ast.out
```
