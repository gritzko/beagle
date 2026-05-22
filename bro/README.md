# bro — syntax-highlighting pager and file viewer

`bro` is the display layer for the dog toolchain. It renders
syntax-highlighted code, search results from `spot`, and diffs
from `graf` in an interactive TUI pager.

## Usage

```
bro [URI...]
```

Arguments are URIs (see `dog/DOG.md`).  No args = pipe mode
(reads TLV hunks from stdin, used by spot/graf).

### View files

```
bro bro/BRO.c                       syntax-highlighted cat
bro bro/BRO.c#42                    open at line 42
bro bro/BRO.c#BROHandleKey          open, search for "BROHandleKey"
bro bro/BRO.c bro/BRO.h             view multiple files
```

### View directories

```
bro bro/                             list directory contents
bro .                                list current directory
```

### Pipe mode (used by spot and graf)

```
spot -g TODO .c                      spot forks bro automatically
spot --tlv -g TODO .c | bro          manual pipe
graf -d old.c new.c                  graf forks bro for diffs
```

## Keys

### Navigation

| Key | Action |
|-----|--------|
| `j` / `k` | line down / up |
| `d` / `u` | half page down / up |
| `space` / `f` | page down |
| `b` | page up |
| `g` / `G` | top / end |
| `]` `}` / `[` `{` | next / prev hunk |
| `(` / `)` | prev / next change (centered) |

### Search

| Key | Action |
|-----|--------|
| `/` or `'` | search in current view |
| `n` / `N` | next / prev match |
| `:` or `#` | URI prompt (see below) |

### File navigation

| Key | Action |
|-----|--------|
| `Enter` / `l` | open file at current hunk |
| `h` | go back |
| `q` | quit |
| `.` | list directory of current file |

### Other

| Key | Action |
|-----|--------|
| `m` | toggle mouse (wheel scroll, click to open) |
| `w` | toggle soft-wrap (off = clip long lines at the screen edge) |
| `Esc` | disable mouse mode |

## URI prompt (`:` or `#`)

The `:` prompt accepts GURI syntax.  The fragment determines
the search type:

```
:42                                  goto line 42
:bro/BRO.c                          open file
:bro/BRO.c#42                       open file at line 42
:bro/BRO.c#BROHandleKey             open file, search for token
:#TODO.c                             spot grep "TODO" in .c files
:#'ok64 o = OK'.c                   spot snippet search in .c
:#/u8s.*Feed/.h                     spot regex search in .h
```

The trailing `.ext` filters results by file type.  When omitted,
the extension of the current file is used as default.

Tab completion works in the prompt: type a prefix and press Tab
to cycle through matching identifiers from the current view.
Prefix match is tried first; if no match, substring match.

## Status bar

```
 --- bro/BRO.c :: BROHandleKey ---    42%  H3/12  C2/8
```

- position — `TOP` / `BOT` / `NN%` / `ALL` (fits on screen)
- `H` — current hunk / total hunks
- `C` — current change / total changes (only shown for diffs)

Long source lines soft-wrap at the terminal width.  Wrap is codepoint-based
(1 codepoint = 1 column); wide characters like CJK and emoji still count
as a single column for now.  Press `w` to toggle wrap off — long lines
are then clipped at the screen edge (less -S semantics); `w` again to
restore.

## Architecture

| File | Purpose |
|------|---------|
| `BRO.h` | Public API, `BROloc` URI parser, navigation primitives |
| `BRO.c` | Pager TUI, renderer, key handler, view stack, spot invocation |
| `BRO.cli.c` | CLI entry point (uses `dog/CLI.h`) |
| `MAUS.h/c` | SGR mouse tracking (enable/disable/parse) |

### Hunk format

Hunks arrive as TLV records (see `dog/HUNK.h`) with a URI field
encoding `path#symbol:line`.  The display title is formatted at
render time from the parsed URI.  Text is syntax-colored using
tok32 arrays with these tags:

| Tag | Meaning | Color |
|-----|---------|-------|
| `D` | Comment | gray |
| `G` | String | green |
| `L` | Number | cyan |
| `H` | Preproc/annotation | pink |
| `R` | Keyword | blue |
| `P` | Punctuation | gray |
| `S` | Default | — |
| `N` | Defined name | bold |
| `C` | Function call | bold |
| `F` | Filename/path | violet |
| `U` | URI (invisible click target — left-click on the preceding token navigates here) | — |

### View stack

Opening a file or directory pushes the current view onto a stack
(max 32 deep).  `h` pops back, `q` quits.  Each stacked view
preserves its scroll position.
