# test/spot — `spot` query integration cases

Each case stages a tiny workspace, commits via `be post` so the
worktree is present, then drives `spot` directly (the binary lives
next to `be` in `$BIN`) and matches the captured stdout/stderr.

`be post` with a real commit message (whitespace-folded fragment,
e.g. `'init msg'`) drives sniff's `keep_indexer_emit` →
`sniff_indexer_fanout` → `SPOTUpdate`, so the `.spot.idx` run is
populated by the time the case's spot query runs.  `be post 'init'`
(single token, no fold) is a dry-run and does *not* populate; cases
intentionally use the multi-word form.  08-status asserts the
non-zero entry count to catch regressions on this path.

## Cases

| Dir | Query mode |
|-----|-----------|
| 01-grep            | `spot -g <text>` substring grep |
| 02-grep-ext        | `spot -g <text> .ext` language filter |
| 03-grep-files      | `spot -g <text> file1 file2` explicit list |
| 04-pcre            | `spot -p <regex> .ext` Thompson NFA grep |
| 05-snippet         | `spot -s <pattern> .ext` structural search |
| 06-replace         | `spot -s <pat> -r <rep> .ext` rewrite in-place |
| 07-uri-fragment    | `be '#text.ext'` routes to spot via URI fragment |
| 08-status          | `spot status` reports index stack summary |
| 09-index-filter    | committed (filter-passing) **and** uncommitted file both surface — proves both ULOG spines run |

## Helpers

`lib/spot-case.sh` resolves `$SPOT` (the `spot` binary) on top of the
generic case helpers in `lib/case.sh`.
