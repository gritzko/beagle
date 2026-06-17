#   mark — module index

mark is the StrictMark renderer and linter for the WikiWeb: it turns a `.mkd` page into standalone HTML and enforces page structure and per-block budgets (see [README.mkd]). It does not parse — `dog/tok/MKDT.h` classifies blocks and lexes inline tokens; mark drives those and emits HTML via abc `u8b` helpers. Two ragel machines do the leaf work: `MARKE` escapes literals, `MARKG` splits a span. `mark_*` helpers are file-local, not API.

##  Public API

###  MARK.h — render entry points, options, budgets

One document renderer, three generation helpers shared with the ragel units, two records, four error codes, and the WikiWeb budget macros. Parsing is MKDT's; here is presentation and validation.

 -  `MARKRenderDoc(out, src, title, opts)` — the one top-level call: render `src` into `u8bp out` as a full HTML document.
 -  `markopts` — render config: `strict` (a breach aborts with `MARKLIMIT`), `head`/`body` (raw HTML), `page`, `root`.
 -  `MARKu8bLit(out, s)` — append a C string verbatim; basis of every structural tag and the ragel escaper.
 -  `MARKu8bFeedEsc(out, text)` — HTML-escape `text` (`& < > "` → entities); the `MARKE` ragel machine.
 -  `markg` — the decomposed inline span: `kind` ∈ strong/em/del/link/image/none, with `text` and `label`.
 -  `MARKDecomposeG(g, tok)` — classify one MKDT `'G'` inline token into a `markg` (the `MARKG` machine); always `OK`.
 -  `MARKLIMIT`/`MARKFAIL`/`MARKARG` — budget/structure breach (strict), internal failure, bad CLI arg; `MARKBAD` unraised.
 -  `MARK_LINE`(64) and `HEAD`/`BULLET`/`SUMM`/`OPEN_MAX` (1/2/4/8×64) — per-block codepoint budgets (`mark_budget`).

##  Translation units

 -  `MARK.c` — renderer body: 2 passes — `mark_collect_refs` (refs), `mark_blocks` (classify via MKDT, `<div>` per indent).
 -  `MARKE.c.rl`/`MARKG.c.rl` — the two ragel units (escaper, span splitter); regenerate `ragel -C`, `.rl.c` is committed.
 -  `MARK.cli.c` — the `MAIN` driver: parses `--strict`/`--head`/`--body`/`--root`, renders a file or every `*.mkd` in a dir.
 -  `test/MARK.c` — render/escape/link/budget table test + `MARKpathlinks` fixture; `fuzz/MARK.c` (`MARKfuzz`) only "no crash".

[README.mkd]: ./README.mkd
