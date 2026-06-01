# git dogs — directory index

## Libraries

| Dir | What | README |
|-----|------|--------|
| `abc/` | Foundation C library: slices, buffers, hashing, TLV, diff, NFA, file I/O. No runtime, no GC. | [abc/INDEX.md](abc/INDEX.md) |
| `dog/` | Language dogenizers (ragel-based, 30+ languages), shared infra: HUNK wire format, HOME workspace finder, TOK dispatch. | [dog/INDEX.md](dog/INDEX.md) |

## Dogs (tools)

Each dog follows the [DOG API](dog/DOG.md): static lib + executable,
`--update`/`--status`/`--tlv` flags, state in `.be/name/`.

| Dog | Role | README |
|-----|------|--------|
| **spot** | Structural code search, grep, regex, replace. Trigram index in `.be/`. | [spot/README.md](spot/README.md) |
| **bro** | Interactive syntax-highlighted pager. Also `bro file.c` for colorful cat. | [bro/INDEX.md](bro/INDEX.md) |
| **graf** | Token-level diff, 3-way merge, git diff/merge driver. | [graf/INDEX.md](graf/INDEX.md) |
| **keeper** | Object store: packs, fetch, refs. Native replacement for git's object store. State in `.be/`. | [keeper/INDEX.md](keeper/INDEX.md) |
| **sniff** | Worktree management: checkout, status, stage, commit.  State is one append-only ULOG file at `<wt>/.be/wtlog`; no caches. | [sniff/INDEX.md](sniff/INDEX.md) |
| **beagle** | Dispatcher: ties the dogs together. URI-based CLI (`be verb URI`). | [beagle/README.md](beagle/README.md) |
| **mark** | StrictMark → HTML renderer for the wiki. Drives MKDT, enforces WikiWeb structure + size budgets (`--strict`). | [mark/README.mkd](mark/README.mkd) |

## Other

| Dir | What |
|-----|------|
| `js/` | JavaScriptCore bindings (optional, `WITH_JS`) |
| `scripts/` | CI, fuzzing, utilities (`ci-fast.sh`) |
| `build-*/` | Build directories (not tracked) |

## Architecture

```
abc          foundation (slices, buffers, crypto, diff, TLV)
 └── dog     tokenizers + shared infra (HUNK, HOME, TOK)
      ├── spot     search & index
      ├── bro      display & pager
      ├── graf     diff & merge
      ├── keeper   object store & fetch
      ├── sniff    worktree ops
      ├── beagle   dispatcher (be)
      └── mark     StrictMark → HTML wiki renderer
```

Dogs communicate via TLV hunks on pipes (`--tlv` flag) or through
static-library API and shared data.
`be` dispatches based on verb + [GURI](beagle/GURI.md) URI pattern.

## Key docs

| File | What |
|------|------|
| [README.md](README.md) | Project overview and quick start |
| [CLAUDE.md](CLAUDE.md) | Coding guidelines and ABC style |
| [DONT.md](DONT.md) | Anti-patterns to avoid |
| [dog/DOG.md](dog/DOG.md) | Dog API convention |
| [beagle/GURI.md](beagle/GURI.md) | Git URI addressing spec |
| [abc/S.md](abc/S.md) | Slice idioms |
| [abc/B.md](abc/B.md) | Buffer idioms |
