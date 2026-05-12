#ifndef SNIFF_SUBS_H
#define SNIFF_SUBS_H

//  SUBS — submodule (gitlink + .gitmodules) plumbing.
//
//  See MODULES.plan.md.  A submodule is a secondary worktree mounted
//  at a path inside the parent's wt; the parent's tree records a
//  `160000` gitlink at that path and a `.gitmodules` blob at the root
//  carries the upstream URL.  This module owns the small, pure helpers
//  needed by GET (read .gitmodules, derive the on-disk store dir from
//  a URL) and POST (synthesise .gitmodules from the live mount table).
//
//  No singleton state.  No I/O.  All consumers are pure-slice.

#include "abc/BUF.h"

con ok64 SUBSPARSE = 0x65bb73d4a495;  // .gitmodules INI did not parse
con ok64 SUBSNOSEC = 0x65bb73d5d85ce; // no [submodule] section for path

//  Extract the project-basename from a clone URL.  Mirrors the
//  default sub-store keying rule of MODULES.plan.md §"Storage layout":
//  take the last path segment, strip a trailing ".git" suffix, strip a
//  trailing '/'.  Examples:
//
//      https://github.com/gritzko/libabc.git   ->  libabc
//      git@github.com:foo/proj.git             ->  proj
//      ssh://localhost/srv/repos/widgets/      ->  widgets
//      ssh://localhost/srv/repos/widgets       ->  widgets
//      file:///var/repos/proj.git              ->  proj
//
//  Doesn't allocate — `out` points into `url`.  Returns SUBSPARSE on
//  an unparseable URL or one whose basename is empty.
ok64 SNIFFSubBasename(u8cs url, u8csp out);

//  Parse a `.gitmodules` blob (INI format).  For each
//  `[submodule "<name>"]` section, accumulate `path` and `url` keys
//  and invoke `cb(path, url, ctx)` once the section ends (next section
//  header or EOF).  Sections missing either field are silently skipped
//  — git tolerates them and so do we, on the principle that the parent
//  tree's 160000 entry is the authoritative mount, not .gitmodules.
//
//  All slices passed to the callback point into `blob`; they are valid
//  for the lifetime of the caller's borrow on `blob`.  A non-OK return
//  from `cb` aborts the parse and is propagated.
//
//  Tolerant of:
//    * leading whitespace (tabs / spaces) before keys
//    * '\r' line endings (stripped before key/value split)
//    * '#' or ';' comments at start of line
//    * quoted section names: [submodule "name with spaces"]
//    * value surrounding whitespace
//  Rejects with SUBSPARSE:
//    * malformed section header (no closing ']')
typedef ok64 (*sniff_subs_cb)(u8cs path, u8cs url, void *ctx);
ok64 SNIFFSubsParse(u8cs blob, sniff_subs_cb cb, void *ctx);

//  Convenience: find the URL for the section whose `path = <path>`.
//  Equivalent to SubsParse + filter on path.  Returns SUBSNOSEC if no
//  matching section, SUBSPARSE on parse error.  On OK, `url_out`
//  points into `blob`.
ok64 SNIFFSubsParseFind(u8cs blob, u8cs path, u8csp url_out);

//  Emit a canonical `.gitmodules` blob into `out` (which is RESET on
//  entry) given two parallel slice arrays of equal length: one
//  newline-separated `paths`, one newline-separated `urls`.  Mirrors
//  `git submodule add`'s output: one `[submodule "<path>"]` section per
//  pair, with `path = <path>` then `url = <url>`, tab-indented.  The
//  section name is the mount path (git's de-facto convention when the
//  submodule has no other label).  Empty input → empty blob.
ok64 SNIFFSubsSynth(u8bp out, u8cs paths, u8cs urls);

//  Drive a single submodule mount.  Called by sniff GET once per
//  `WALK_KIND_SUB` entry it sees after the parent tree's WRITE pass
//  finishes.
//
//  Steps (see MODULES.plan.md §"Phase 3 — GET"):
//    1. Resolve the upstream URL from `.gitmodules` (`SubsParseFind`).
//    2. Compute the sub's project basename (`SubBasename`).
//    3. Mkdir `<parent_root>/.be/<basename>/`  (sub store dir, may
//       be empty for the smoke path — content goes into parent's
//       keeper trunk via the recursive `be get`).
//    4. Write `<wt>/<path>/.be` as a regular file: one ULOG row
//       `<ts>\trepo\tfile://<parent_root>/.be/\n` (secondary-wt
//       anchor — home walk-up redirects `h->root` to the parent's
//       store).
//    5. fork+chdir+execvp `be get <url>#<hex>` with cwd =
//       `<wt>/<path>`; waitpid in the parent.  The child reuses the
//       parent's keeper via the row-0 anchor.
//
//  Inputs:
//    reporoot     — parent worktree root (absolute path).
//    parent_root  — parent keeper's `.be/`-parent dir.  Normally equal
//                   to `reporoot` for colocated layouts; passed
//                   separately so secondary-wt scenarios still work.
//    path         — sub-mount path relative to `reporoot`.
//    hex_sha      — 40-byte hex of the pinned commit (the `160000`
//                   gitlink's target sha).
//    gitmodules   — bytes of the parent tree's `.gitmodules` blob.
//    argv0        — caller's argv[0], used by `HOMEResolveSibling` to
//                   find the sibling `be` binary.
//
//  Returns OK on success.  Per-sub failures (URL missing, fetch fail,
//  child non-zero exit) are propagated; the caller may choose to log
//  and continue.
ok64 SNIFFSubMount(u8cs reporoot, u8cs parent_root,
                   u8cs path, u8cs hex_sha,
                   u8cs gitmodules, u8cs argv0);

//  YES iff `<wt_root>/<subpath>/.be` exists as a regular file (the
//  secondary-wt anchor that GET writes when materialising a sub).
//  Per MODULES.plan.md §"Storage layout" — the file is the
//  ground-truth signal that a sub is mounted at this path.
b8 SNIFFSubIsMount(u8cs wt_root, u8cs subpath);

//  Read the sub-wt's current commit tip — the 40-byte hex sha that
//  POST should emit as the parent tree's `160000` gitlink target.
//  Opens `<wt_root>/<subpath>/.be` read-only via the existing
//  `SNIFFAtTailOf` plumbing (which composes the tail row's
//  `?<branch>#<sha>` view) and copies the fragment out into `out`
//  (must hold at least 40 bytes).  Returns OK on success, SUBSNOSEC
//  if no commit tip is recorded yet (fresh mount with row-0 anchor
//  only), or the upstream error code from the underlying read.
ok64 SNIFFSubReadTip(u8cs wt_root, u8cs subpath, u8s out);

#endif
