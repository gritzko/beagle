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
#include "dog/git/SUBS.h"

//  SUBSPARSE / SUBSNOSEC are now declared in dog/git/SUBS.h.  The
//  sniff-side names below are thin wrappers around the SUBSu8s*
//  functions there.

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
//  Wrapper around `SUBSu8sFind`; `url_buf` is a caller-owned scratch
//  that the URL bytes are copied into so `url_out` outlives the
//  parser's per-call stack frame.  Returns SUBSNOSEC if no matching
//  section, SUBSPARSE on parse error, NOROOM if the buffer is too
//  small.
ok64 SNIFFSubsParseFind(u8cs blob, u8cs path, u8bp url_buf,
                        u8csp url_out);

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
//    src_uri      — GET-011: the in-flight `be get` SOURCE URI we are
//                   actually cloning from (the remote on the command
//                   line, e.g. `file:///home/gritzko/.be?/dogs`).  Empty
//                   for a git-source parent.  When this names a beagle
//                   remote (scheme `file`/`be`/`keeper`), the PRIMARY
//                   fetch candidate for each sub is built from it —
//                   scheme+authority+store-path with the `?/<proj>`
//                   query replaced by the sub's path-basename — so the
//                   sub is sourced from the SAME store we are talking
//                   to, with the `.gitmodules` URL only a last fallback.
//
//  Returns OK on success.  Per-sub failures (URL missing, fetch fail,
//  child non-zero exit) are propagated; the caller may choose to log
//  and continue.
ok64 SNIFFSubMount(u8cs reporoot, u8cs parent_root,
                   u8cs path, u8cs hex_sha,
                   u8cs gitmodules, u8cs argv0, u8cs src_uri);

//  SUBS-020: YES iff the parent SOURCE URI's PATH ends in `.git` — the
//  git-parent discriminator shared by GET (fetch source) and POST (push
//  destination).  A `.git` parent (case 2) carries a canonical remote in
//  `.gitmodules`, so the official URL is used as-is (no path
//  computation).  A non-`.git` parent (case 3) resolves the sub URL
//  relative to the parent (SNIFFSubCandidateGitRel).  Empty / unlexable
//  `src_uri` → NO.
b8 SNIFFSubSrcEndsGit(u8cs src_uri);

//  SUBS-020 case 3: parent is a GIT repo whose URI does NOT end in
//  `.git`; compute the sub's URI (fetch source for GET, push
//  destination for POST) by resolving the declared `.gitmodules` `url`
//  (which may be relative, e.g. `../sub`) against the parent SOURCE
//  URI.  Git resolves a relative submodule URL with the SUPERPROJECT
//  URL treated as a DIRECTORY (so `../sub` off `…/subs/par` is
//  `…/subs/sub`).  Renders the resolved candidate into `out` (RESET on
//  entry).  Returns NONE when no useful computation applies — the
//  parent URI is empty / unlexable, or the resolved URI equals the
//  official `url` verbatim (an already-absolute declared URL needs no
//  separate candidate).  `url` is the raw `.gitmodules` value; the
//  caller always retains it as the final fallback.
ok64 SNIFFSubCandidateGitRel(u8bp out, u8cs src_uri, u8cs url);

//  GET-011 / HEAD-004: build the PRIMARY sub candidate from the in-flight
//  beagle SOURCE URI by replacing its `?/<project>` shard query with
//  `/<sub_proj>` (the sub's PATH-basename): `file://<store>?/dogs` + sub
//  `abc` → `file://<store>?/abc`.  Renders into `out` (RESET on entry).
//  Returns NONE unless `src_uri` is a beagle multi-project transport
//  (scheme `be`/`keeper`, or authority-bearing `file://…`) carrying a
//  `?/<project>` query — a git source / bare path keeps its own
//  resolution.  GET threads this into `sniff sub-mount --source`; HEAD's
//  transport recursion uses it to peek the sub via the parent, not the
//  declared `.gitmodules` URL.
ok64 SNIFFSubCandidateFromSource(u8bp out, u8cs src_uri, u8cs sub_proj);

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
