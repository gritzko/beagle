#   bro — module index

bro is the shared terminal RENDERER and pager for the dog pack: it owns ALL presentation of hunks — banner, syntax highlight, soft-wrap, colour diff. Producers (graf/keeper/sniff/spot) only feed DATA — `hunk` records (`dog/HUNK.h`); they never decide header style, colour, or wrap. The byte-emitting `HUNKu8sFeed*` live in `dog/HUNK.h`; bro indexes hunks into rows and paints cells, delegating only the banner to dog. More in [README.md].

##  Pager state & lifecycle

###  BRO.h — control struct, staging, navigation, entry

The single public header. A `bro` holds the per-invocation arena and typed buffers; hunks are staged from a pipe or mapped files, then handed to the pager. Most non-lifecycle functions are exposed only so tests can drive them.

 -  `bro` — the control struct: `arena`, `hunks`, `toks`, `maps` (mmap'd fds), invocation flags; `BRO_NONE` = `UINT32_MAX`.
 -  `BROOpen`/`BROExec`/`BROUpdate`/`BROClose` — the DOG 4-fn lifecycle; `Update` is a no-op stub (bro never indexes).
 -  `BROloc`/`BROHunkLoc` — parse one hunk's `uri` into `{path, symbol, line}`, leading `/` stripped.
 -  `BRORun`/`BROPipeRun` — enter the pager over in-memory `hunkcs` (plain dump if not a tty) or a TLV-hunk pipe fd.
 -  `BROArenaInit`/`BROArenaCleanup`/`BRODefer` — reset staging; `BRODefer` returns `NOROOM` on a full maps table (MEM-020).
 -  `BROListDir`/`BROTokenize` — stage one hunk per dir entry (`'F'`), and tokenize a hunk's `text` by ext into `toks`.
 -  `BROAppendLines`/`BROCountLines` — build/size the line index: one `range32` per line (`lo`=hunk, `hi`=offset+pass).
 -  `BROHunkNextLine`/`PrevLine`/`Count`/`IndexAt` — row nav over hunks; movers return `BRO_NONE` past the ends.
 -  `BROHiliCount`/`IndexAt`/`NextLine`/`PrevLine` — change nav: `Count` counts non-eq side *runs*; rest seek nearest hili.

###  MAUS.h — SGR mouse tracking

Enable/disable mouse reporting and parse the SGR-1006 escape subset; the pager uses it for wheel scroll and click-to-open (`'m'` toggles). CSI parsing is borrowed from `abc/ANSI.h`.

 -  `MAUSevent` — a decoded event: 1-based `row`/`col`, a `type` (`PRESS`/`RELEASE`/`DRAG`/`WHEEL`) and a `button`.
 -  `MAUSEnable`/`MAUSDisable` — write the SGR 1000/1002/1006 on/off escape sequences to an fd.
 -  `MAUSParse` — decode one SGR mouse sequence; fills `*ev`, returns bytes consumed (0 if partial).

##  Rendering pipeline (in BRO.c, not a public header)

bro indexes hunks into rows then paints each cell itself, calling `dog/HUNK.h` only for the banner. The pieces below are file-static in `bro/BRO.c` — documented here because they ARE the renderer and the colour-diff policy, exporting no symbols.

 -  Row index — `BROAppendLines` walks text into `range32` rows; `hi` packs a 24-bit offset + 2-bit *pass* (NORMAL/RM/IN).
 -  Cell painting — `bro_cell_ansi` resolves each cell's SGR from the syntax tag + `(pass, side)`: in/rm wear the wash.
 -  Output sinks — colour TUI (`BRORender`), non-tty `BROPlain`, `--tlv` pass-through; `HUNKMode` picks TLV/Color/Plain.

##  Colour diff — line-OR-token by change volume

The mode that distinguishes bro from a dumb pager: each diff line is classified by how much changed, small edits staying inline, large ones splitting into two rows. The policy is `bro_classify` in `bro/BRO.c` — no separate header.

 -  Per-line classification — in/rm/eq counts → `EQ`/`PURE_IN`/`PURE_RM`/`MOD_INLINE`/`MOD_SPLIT` (the split is 25%).
 -  Inline mode (`< 25%`) — one NORMAL-pass row; rm red-washed, in green-washed *in place*, reads as one highlighted line.
 -  Split mode (`≥ 25%`) — two rows (RM-pass, IN-pass), opposite side hidden; merged text is INS-before-DEL (WEAVE canon).
 -  Verify added side in RAW bytes — judge diff-side correctness on the raw merged text + `tok32` sides, never ANSI-stripped.

[README.md]: ./README.md
