#   mark — module index

mark is the StrictMark renderer and linter for the WikiWeb: it turns a `.mkd` page into standalone HTML and enforces page structure and per-block budgets (see [README.mkd]). It does not parse — `dog/tok/MKDT.h` classifies blocks (`MKDTB` grammar), lexes inline tokens, and splits spans (`MKDTDecomposeSpan`); mark drives those and emits HTML via abc `u8b` helpers. mark's one ragel machine is `MARKE` (it escapes literals). `mark_*` helpers are file-local, not API.

##  Public API

###  MARK.h — render entry points, options, budgets

One document renderer, three generation helpers shared with the ragel units, two records, four error codes, and the WikiWeb budget macros. Parsing is MKDT's; here is presentation and validation.

 -  `MARKRenderDoc(out, src, title, opts)` — the one top-level call: render `src` into `u8bp out` as a full HTML document.
 -  `markopts` — render config: `strict` (a breach aborts with `MARKLIMIT`), `head`/`body` (raw HTML), `root`.
 -  `MARKu8bLit(out, s)` — append a C string verbatim; basis of every structural tag and the ragel escaper.
 -  `MARKu8bFeedEsc(out, text)` — HTML-escape `text` (`& < > "` → entities); the `MARKE` ragel machine.
 -  Inline span decomposition is `dog/tok`'s, not mark's: `mkdtspan` + `MKDTDecomposeSpan(g, tok)` (the `mkdtg` machine) split a `'G'` token into kind/text/label.
 -  `MARKLIMIT`/`MARKFAIL`/`MARKARG` — budget/structure breach (strict), internal failure, bad CLI arg; `MARKBAD` unraised.
 -  `MARK_LINE`(64) and `HEAD`/`BULLET`/`SUMM`/`OPEN_MAX` (1/2/4/8×64) — per-block codepoint budgets (`mark_budget`).

##  Translation units

 -  `MARK.c` — renderer body: 2 passes — `mark_collect_refs` (refs), `mark_blocks` (classify via MKDT, `<div>` per indent).
 -  `MARKE.c.rl` — mark's one ragel unit (the escaper); regenerate `ragel -C`, `.rl.c` is committed. Block/inline grammars are in `dog/tok` (`MKDTB`, `MKDT`).
 -  `MARK.cli.c` — the `MAIN` driver: parses `--strict`/`--head`/`--body`/`--root`, renders a file or every `*.mkd` in a dir.
 -  `test/MARK.c` — render/escape/link/budget table test + `MARKpathlinks` fixture; `fuzz/MARK.c` (`MARKfuzz`) only "no crash".

[README.mkd]: ./README.mkd
