# bro/ — display layer for spot hunks

`bro` is the pager and file viewer for the `spot` toolchain. It owns
all syntax-highlighted display: the interactive TUI, the plain non-tty
fallback, and the cat-mode file viewer. It reads hunks either from
standard input (TLV via `dog/HUNK.h`) or by directly tokenizing files
passed as arguments.

## Modes

```
bro                  pager mode (read TLV hunks from stdin)
bro file.c [...]     cat mode (syntax-highlighted file display)
```

## Files

| File       | Purpose |
|------------|---------|
| `BRO.h`    | Public API: `BROhunk`, arena state, `BRORun`, `BROPipeRun`, `BROCat` |
| `BRO.c`    | Pager TUI (raw mode, render, search, status bar), plain dump, pipe consumer |
| `CAT.c`    | `BROCat` — tokenize files and stage hunks for the renderer |
| `BRO.cli.c`| `main()`: dispatch on argv (cat vs pager) and tty status |

## Pipe protocol

Hunks arrive as nested TLV records (`HUNK_TLV`) with sub-tags for title,
text, and toks (see `dog/HUNK.h`); each tok carries syntax tag plus a
2-bit diff side (eq/in/rm). `BROPipeRun` drains records incrementally,
copies fields into the bro arena, and renders.

## Spawning bro from spot

Spot resolves the bro binary at startup:
1. `dirname(/proc/self/exe) + "/bro"` (the build copies bro next to spot)
2. fall back to bare `"bro"` for `execvp` PATH lookup

When spot's stdout is a tty it forks bro as a pager and writes TLV
hunks to the pipe. When stdout is not a tty, spot bypasses bro and
writes plain ASCII via `HUNKu8sFeedText` directly.
