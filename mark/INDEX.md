# mark — StrictMark → HTML renderer

`mark` renders StrictMark wiki pages to standalone HTML and enforces the
WikiWeb page structure and size budgets. It drives the shared MKDT
dogenizer for parsing, generates HTML with `u8b` feed helpers, and uses
two small ragel machines of its own for sanitizing and inline decomposition.
The user-facing overview is [README.mkd](README.mkd) (itself WikiWeb-formatted).

## The one-paragraph model

A page is parsed by MKDT, never by mark. `mark_blocks` walks the source
line by line, classifying each line with MKDT's exposed block helpers
(`MKDTHeadingLevel`, `MKDTFenceOpen/Close`, `MKDTHRule`, `MKDTIndentDepth`,
`MKDTLineMarker`), and feeds the inline content of each block to
`MKDTInlineLexer`. The resulting token stream becomes HTML: structural
tags via `MARKu8bLit`, literal text via the `MARKE` ragel escaper, and
emphasis/link `G` tokens split by the `MARKG` ragel machine. A first pass
collects `[key]: url` reference definitions; links resolve against them and
a trailing `.mkd` target is rewritten to `.html` so the site is
self-contained.

## Files

| File | Role |
|------|------|
| `MARK.h` | Public API + error codes + WikiWeb budgets (`MARK_*_MAX`). |
| `MARK.c` | Block driver, inline callback, refdef collection, link rewrite, budget validation, document scaffold. Char counts go through abc `utf8CPLen`; slice eq/prefix/suffix through `u8csEq` / `u8csHasPrefix` / `u8csHasSuffix`. Leaf closing is one `mark_close(flag,tag)`; emphasis B/I/D one `mark_inline_wrap(text,open,close)`. |
| `MARKE.c.rl` / `MARKE.rl.c` | ragel HTML escaper (`MARKu8bFeedEsc`): `& < > "` → entities. |
| `MARKG.c.rl` / `MARKG.rl.c` | ragel inline decomposer (`MARKDecomposeG`): emphasis / link / image. |
| `MARK.cli.c` | `MAIN` entry: argv, `--strict`, file or whole-directory build. |
| `test/MARK.c` | Table-driven render + escape + link tests; budget-violation test; lexer-gap repros. |

## MKDT changes (shared dogenizer)

mark required three changes to `dog/tok/MKDT.*`, kept minimal and verified
against `dog/test/TOK01.c`:

- **De-static + declare** the block-line classifiers in `MKDT.h`
  (`MKDTFenceOpen`, `MKDTFenceClose`, `MKDTHeadingLevel`, `MKDTHRule`,
  `MKDTRefDef`, `MKDTIndentDepth`) plus a new `MKDTLineMarker` extracted from
  `MKDTLexer`'s marker logic (one source of truth). No grammar change.
- **Inline grammar:** added the shortcut link form `[page]` (keyed on its
  bracket text) alongside the existing explicit `[text][l]` (one-symbol label
  `l`). These are the only two link cases — no collapsed `[page][]`.
- **Inline grammar:** added an `any8 => on_punct` fallback so the lexer is
  total — prose bytes outside the original punct set (`?`, `%`, …) no longer
  yield `MKDTBAD`. Purely additive (only reclassifies bytes that matched
  nothing). Regenerate with `ragel -C MKDT.c.rl -o MKDT.rl.c -L`.

## Follow-up: refdef cross-dir reconcile

`mark_refdef` (MARK.c) still hand-rolls the `[key]: url` extraction that
`dog/tok/MKDT.c`'s `MKDTRefDef` already implements. CODE-014 deferred the
reconcile (reuse `MKDTRefDef`, drop the local copy) because `dog/` was
under concurrent edit; track it as a separate cross-dir ticket so the two
do not fork.

## Enforced structure & limits

`mark` doubles as the wiki linter. Budgets are counted in UTF-8
*characters*, not bytes (so `≤` / `—` don't inflate counts):

- header ≤ 64, bullet ≤ 128, summary ≤ 256, opener summary ≤ 512.
- exactly one H1 opener per page (one concept per page).
- Default: warn to stderr, render anyway. `--strict`: first breach →
  `MARKLIMIT`, non-zero exit (CI gate).

## CLI

```
mark [--strict] <file.mkd | dir>...
```

A file → sibling `.html`; a directory → every `*.mkd` in it (one bad page
warns and is skipped, the batch continues). Inter-page `.mkd` links are
rewritten to `.html`. Pair with a `.nojekyll` file to publish on GitHub
Pages verbatim.

## Tests

`test/MARK.c` (`ctest -R MARKtest`) is table-driven: each row renders a
snippet and asserts an HTML substring. It covers headings, paragraphs,
lists, emphasis, inline code, escaping, fenced code, shortcut + reference
links with `.mkd`→`.html`, and regression repros for the `?` / `%` /
em-dash inline bytes that previously tripped MKDT. A separate case asserts
that an over-budget header fails under `--strict` and only warns without it.

## Dependencies

`dog` (MKDT + TOK) and `abc-core` (slices, buffers, FILE, PATH). No network
modules. ragel 6.x to regenerate `*.rl.c` (the generated files are
committed).
