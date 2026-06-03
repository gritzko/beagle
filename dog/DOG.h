#ifndef DOG_DOG_H
#define DOG_DOG_H

#include "abc/HEX.h"
#include "abc/KV.h"
#include "abc/PATH.h"
#include "abc/RAP.h"
#include "abc/RON.h"
#include "abc/URI.h"

// Git object types (from packfile format)
#define DOG_OBJ_COMMIT 1
#define DOG_OBJ_TREE   2
#define DOG_OBJ_BLOB   3
#define DOG_OBJ_TAG    4

// --- Path hash ---
//
// A 64-bit positional identifier for a tree node.  The root hash is
// the ron60 encoding of the literal "ROOT"; every child's hash is
// RAPHashSeed(name, parent_hash), where `name` is just the leaf
// segment — no slashes, no parent prefix.  
con ok64 ROOT	= 0x6d861d;

// One step: child of `parent` named `name` (leaf, no slashes).
fun u64 DOGChildPathHash(u8csc name, u64 parent) {
    return RAPHashSeed(name, parent);
}

// --- Canonical on-disk layout ----------------------------------------
//
// First level under `.be/` is always a project — one repository
// identity, name typically derived from the clone URL basename (see
// `SNIFFSubBasename`).  A store may hold any number of project shards
// side-by-side; each is self-contained (REF_DELTA bases cannot cross
// project boundaries).  Below the project, the directory tree mirrors
// branch paths verbatim, with the project's trunk sitting at the
// project root itself.
//
// Primary / colocated worktree (one project alongside the wt):
//   <wt>/.be/                                — store directory
//   <wt>/.be/config                          — store-wide TOML config
//   <wt>/.be/wtlog                           — wt command ULOG; row 0
//                                              = `repo file:<wt>/.be/<project>`
//                                              pins which project this
//                                              wt is on
//   <wt>/.be/<project>/                      — project shard (== its
//                                              trunk branch)
//   <wt>/.be/<project>/refs                  — project-scoped ref
//                                              ULOG: trunk tip plus
//                                              entries meant to
//                                              outlive a branch-dir
//                                              drop (host aliases,
//                                              project-scoped tags)
//   <wt>/.be/<project>/<seqno>.keeper        — trunk keeper packs
//   <wt>/.be/<project>/<seqno>.keeper.idx    — trunk keeper LSM index
//   <wt>/.be/<project>/<seqno>.graf.idx      — trunk graf commit-graph
//   <wt>/.be/<project>/<seqno>.spot.idx      — trunk spot index
//   <wt>/.be/<project>/<branch>/refs         — branch-scoped ref ULOG:
//                                              this branch's tip +
//                                              branch-local
//                                              remote-tracking
//                                              (`be delete ?branch`
//                                              is a whole-dir drop)
//   <wt>/.be/<project>/<branch>/<seqno>.<ext>
//                                            — per-branch packs + idx
//                                              puppies, one sibling
//                                              dir per leaf branch
//
// Secondary worktree (wt sits elsewhere; `.be` is a regular file =
// the wtlog; row 0's anchor URI pins the project):
//   <wt>/.be                                 — regular file = the wtlog
//   row 0 = `get file:<store>/.be/<project>` (anchor; legacy `repo`)
//
// The branch the wt is on is NOT carried in the anchor URI; it is the
// latest `get`/`post` row's `?branch` in the wtlog.  Switching
// branches appends a new row, never rewrites the anchor.
//
// Central store ("$HOME/.be/" pattern): the store holds many project
// shards side-by-side; every wt is a secondary anchored at one of
// them.
//
// Migration note: the older single-project layout (no `<project>/`
// segment — `refs`, packs, and branches sitting directly under
// `<wt>/.be/`) is still recognised.  `DOGProjectFromBe` returns ""
// for that shape; callers may treat the empty project as "the
// implicit single project" during the transition.
//
// Single source of truth for the layout names.  Use the macros directly
// where a C-string is needed; use the `DOGa_*` `a_cstr`-style helpers
// where a `u8cs` slice is wanted at point-of-use.
#define DOG_BE_NAME     ".be"
#define DOG_REFS_NAME   "refs"
#define DOG_WTLOG_NAME  "wtlog"
#define DOG_CONFIG_NAME "config"

#define DOGa_be(n)     a_cstr(n, DOG_BE_NAME)
#define DOGa_refs(n)   a_cstr(n, DOG_REFS_NAME)
#define DOGa_wtlog(n)  a_cstr(n, DOG_WTLOG_NAME)
#define DOGa_config(n) a_cstr(n, DOG_CONFIG_NAME)

// Process-lifetime `u8cs` slices over the layout names (created via
// `a_cstr(...)` in dog/DOG.c) — pass directly where a slice is needed,
// no local `a_cstr` required.
extern u8 const *DOG_BE_S[2];
extern u8 const *DOG_REFS_S[2];
extern u8 const *DOG_WTLOG_S[2];
extern u8 const *DOG_CONFIG_S[2];

// Whole path: walk non-empty segments from the root, folding each
// one through DOGChildPathHash.  Empty path returns ROOT.
fun u64 DOGPathHash(path8s path) {
    u64 h = ROOT;
    $eachseg(seg, path) h = DOGChildPathHash(seg, h);
    return h;
}

// Given the path of a `.be/[<branch>/]` directory (the row-0 `repo`-
// anchor URI path; with or without trailing slash), feed the wt-root
// path into `out`.  Splits on the first `/.be/` separator:
//     /abs/path/.be/          → /abs/path
//     /abs/path/.be/sub/      → /abs/path        (branch = "sub")
//     /abs/path/.be/a/b/      → /abs/path        (branch = "a/b")
// Falls back to the trailing-`.be`-strip logic when `/.be/` is not
// present (so already-stripped paths and other legacy callers keep
// working).  Caller's `out` buffer is reset before the feed.
fun void DOGRepoFromBe(u8cs in, u8bp out) {
    a_dup(u8c, p, in);
    if (!u8csEmpty(p) && *u8csLast(p) == '/') u8csShed1(p);
    //  First pass: split on `/.be/` if present.
    DOGa_be(be);
    if (u8csLen(p) > u8csLen(be) + 1) {
        u8c const *scan = p[0];
        u8c const *end  = p[1] - u8csLen(be) - 1;  // last possible start
        for (u8c const *q = scan; q <= end; q++) {
            if (q[0] == '/' && q[1] == '.' && q[2] == 'b' && q[3] == 'e'
                && q[4] == '/') {
                p[1] = q;                          //  truncate at sep
                u8bReset(out);
                u8bFeed(out, p);
                return;
            }
        }
    }
    //  Fallback: strip a trailing `.be` (legacy / already-stripped).
    if (u8csHasSuffix(p, be))
        for (size_t i = 0; i < u8csLen(be); i++) u8csShed1(p);
    while (u8csLen(p) > 1 && *u8csLast(p) == '/') u8csShed1(p);
    u8bReset(out);
    u8bFeed(out, p);
}

// Sibling of `DOGRepoFromBe` / `DOGBranchFromBe`: extract the project
// segment — the first path component AFTER `/.be/` — from a row-0
// anchor URI path.  Empty when no `/.be/` separator is present, or
// when the URI ends exactly at `/.be/` (legacy single-project anchor
// with the project name elided).  Trailing `/` on the input is
// tolerated.  Caller's `out` buffer is reset before the feed.
//
//     /abs/path/.be/                  → ""
//     /abs/path/.be/beagle            → "beagle"
//     /abs/path/.be/beagle/           → "beagle"
//     /abs/path/.be/beagle/feat       → "beagle"
//     /abs/path/.be/beagle/feat/x     → "beagle"
fun void DOGProjectFromBe(u8cs in, u8bp out) {
    u8bReset(out);
    a_dup(u8c, p, in);
    if (!u8csEmpty(p) && *u8csLast(p) == '/') u8csShed1(p);
    DOGa_be(be);
    if (u8csLen(p) <= u8csLen(be) + 1) return;
    u8c const *scan = p[0];
    u8c const *end  = p[1] - u8csLen(be) - 1;
    for (u8c const *q = scan; q <= end; q++) {
        if (q[0] == '/' && q[1] == '.' && q[2] == 'b' && q[3] == 'e'
            && q[4] == '/') {
            u8c const *seg_start = q + 5;
            u8c const *seg_end = seg_start;
            while (seg_end < p[1] && *seg_end != '/') seg_end++;
            u8cs proj = {(u8 *)seg_start, (u8 *)seg_end};
            if (!u8csEmpty(proj)) u8bFeed(out, proj);
            return;
        }
    }
}

// Sibling of `DOGRepoFromBe`: extract the path that lives AFTER
// `/.be/` in a row-0 anchor URI path.  Empty when the URI ends at
// `/.be/` itself, or when no `/.be/` separator is present.  Caller's
// `out` buffer is reset before the feed.
//
// Project-sharded layout: the returned slice carries `<project>[/<branch>]`
// — the project segment first.  Callers that need just the branch
// should pair this with `DOGProjectFromBe` and strip the leading
// project segment.  The legacy single-project layout (project elided)
// returns the bare `<branch>` here.
fun void DOGBranchFromBe(u8cs in, u8bp out) {
    u8bReset(out);
    a_dup(u8c, p, in);
    if (!u8csEmpty(p) && *u8csLast(p) == '/') u8csShed1(p);
    DOGa_be(be);
    if (u8csLen(p) <= u8csLen(be) + 1) return;
    u8c const *scan = p[0];
    u8c const *end  = p[1] - u8csLen(be) - 1;
    for (u8c const *q = scan; q <= end; q++) {
        if (q[0] == '/' && q[1] == '.' && q[2] == 'b' && q[3] == 'e'
            && q[4] == '/') {
            u8cs br = {(u8 *)(q + 5), (u8 *)p[1]};
            if (!u8csEmpty(br)) u8bFeed(out, br);
            return;
        }
    }
}

// Shared error code for branch-scoped Open entry points that receive
// a branch path outside the supported set.  Phase 0 accepts only the
// trunk (canonical form = empty slice); later phases widen this.
con ok64 DOGNOBR = 0xd6105d82db;

// --- View-projector schemes (VERBS.md §"View projectors") ---
//
// One shared table for the whole repo.  DOGParseURI uses it to skip
// the scheme→authority promotion for projector schemes.  BE uses it
// to dispatch `be <scheme>:<URI>` to the dog that produces that
// projection.  Each dog's CLI recognises the schemes it owns and
// dispatches internally.  Adding a projector = one row here + the
// producing dog's internal dispatch branch; no further wiring.

typedef struct {
    char const *scheme;   // "ls", "tree", "sha1", ...
    char const *dog;      // "sniff" | "keeper" | "graf"
} DOGProjRoute;

// YES iff `scheme` names a registered view-projector scheme.
b8 DOGIsProjector(u8cs scheme);

// Dog name that handles `scheme:` projections, or NULL if `scheme`
// isn't a projector.  The returned cstr has static lifetime.
char const *DOGProjectorDog(u8cs scheme);

// YES iff `scheme` names a known transport (`ssh`, `https`, `file`,
// `be`, …).  Used by the CLI tokenizer to decide whether `<word>:`
// at the start of an arg is a URI scheme or a prose colon.
b8 DOGIsTransport(u8cs scheme);

// Parse a URI string with dog-specific normalization:
//   1. Invoke abc/URILexer for strict RFC 3986 parsing.
//   2. If the parsed URI has a scheme but no authority and its
//      path has no leading slash (i.e. the text is `word:path...`
//      rather than `proto://host/path`), treat the scheme as a
//      remote alias: move scheme → authority.  View-projector
//      schemes (`sha1:`, `blob:`, `tree:`, `commit:`, `log:`,
//      `refs:`, `diff:`, `size:`, `type:`, `ls:` — see VERBS.md)
//      are exempt: they stay as the scheme so `ls:subdir` and
//      `tree:src/?heads/feat` round-trip intact.
//   3. Non-numeric "ports" (`ssh://host:src/...` — `src` isn't a
//      port) get glued back onto the front of the path.
//   4. If the path's head-segment contains `@` (`user@host/rest`),
//      promote that prefix to authority/host/user.
//
// Rationale: users routinely type `localhost:src/git/protocol.h`
// or `origin:docs/README.md`.  Per RFC 3986 this parses as
// scheme="localhost", path="src/git/protocol.h".  For the dogs,
// the first token before a bare `:` almost always names a remote,
// not a protocol.
//
// True URIs with protocol schemes (`https://`, `file:///`) are
// unaffected — they have leading-`/` paths or populated authority.
//
// Path convention for remote transports (ssh/https/etc.): the
// path is always treated as **relative to the user's home on the
// remote**.  `ssh://host:src/repo` means `~/src/repo` on `host`.
// No `~` expansion in the grammar; no `/absolute/path` escape.
// Absolute local paths must use `file:///abs/path`.
ok64 DOGParseURI(urip uri, u8csc text);

// Canonicalise a URI in place — the single chokepoint every
// ULOG/REFS writer must go through so the on-disk form never
// carries redundant or ambiguous spellings.
//
//   Query (slice mutated on `u`):
//     - opaque hierarchical local branch path (no `refs/`, no `heads/`
//       prefix — those are git wire conventions handled by
//       keeper/GIT.h GITParseRef/GITFeedRef, not by us)
//     - trunk = empty query (`?`); bare `?/` folds to `?`
//     - everything else left alone
//
//   Fragment (slice mutated on `u`):
//     - strip a single leading `?` (value is bare 40-hex SHA or empty)
//     - empty fragment means deletion / tombstone
//
//   Presence is preserved: a query/fragment that was in the input
//   stays present-but-empty (non-NULL zero-length slice) rather than
//   reverting to absent, so `?#<sha>` (trunk move) and `?branch#`
//   (deletion) round-trip through the canonicaliser unchanged.
//
// Shape-only transform: slices are shrunk or pointed at their tail;
// no reallocation, no validation of contents.  `u->data` is not
// rewritten and will be out of sync with the mutated components.
ok64 DOGCanonURI(urip u);

// Emit the canonical byte form of a URI to `out`.  Thin wrapper:
// calls DOGCanonURI to canonicalise in place, then serialises.
// Transport schemes (ssh, https, git) are dropped as fungible;
// `file:` is preserved.  Present-but-empty query/fragment emit a
// bare `?` / `#` so `?#<sha>` (trunk move) and `?branch#` (deletion)
// round-trip.  This is the single entry point every ULOG/REFS
// writer goes through.
ok64 DOGCanonURIFeed(u8bp out, urip u);

// Split a branch ref like `feat/sub/abc1234` into branch + commit pin.
// When the LAST '/'-separated segment is 6..40 hex chars, it's read
// as a commit hashlet pinning the branch's lineage at that commit:
//
//     feat                  → branch = "feat",     pin = ""
//     feat/sub              → branch = "feat/sub", pin = ""
//     feat/abc1234          → branch = "feat",     pin = "abc1234"
//     abc1234               → branch = "",         pin = "abc1234"
//     stable/v1.2.3         → branch = "stable/v1.2.3", pin = ""  (v1.2.3
//                              is non-hex; whole path stays as branch)
//
// Pin slices are 0 (no pin) or 6..40 bytes inclusive (a sha1 hashlet).
// `query` may be the URI query body (no leading `?`) or a path-form
// ref string — same rule.  Slices in (branch_out, pin_out) point
// into `query`; valid as long as `query`'s storage lives.  Empty
// `query` → both outputs empty.  Caller-owned outputs.
void DOGRefSplitPin(u8cs query, u8csp branch_out, u8csp pin_out);

// YES iff `s` is 6..40 bytes of [0-9a-fA-F].  Used by DOGRefSplitPin
// to classify the trailing segment; exposed for callers that need
// the same hashlet test (sniff URI normalisers, ref parsers).
b8   DOGIsHashlet(u8cs s);

// Consume one `&`-separated chunk from a multi-ref query body
// (`A&B&C` shape — `graf get` blob/tree merges, `sniff` baseline
// rows that store `<branch>&<sha>`).  Advances `q[0]` past the
// chunk and any trailing `&`; `out` holds the chunk slice (possibly
// empty when the input started with `&`).  Callers loop on `$empty(q)`.
//
// Each chunk is a path-shaped branch ref — classify with
// `DOGIsHashlet` (sha prefix vs branch path); relative `./X` / `../X`
// / `..` resolve against `cur_branch` via `abc/PATH::PATHu8bAbs`.
fun void DOGRefDrain(u8cs q, u8cs out) {
    out[0] = NULL; out[1] = NULL;
    if (q[0] == NULL || q[0] >= q[1]) return;
    u8cp p = q[0];
    u8cp e = q[1];
    while (p < e && *p != '&') p++;
    out[0] = q[0];
    out[1] = p;
    q[0] = (p < e) ? p + 1 : e;
}

// Extract the first non-sha (i.e. branch-path) chunk from a query
// `<branch>(&<sha>)*` per dog/QURY.  Skips empty chunks and 40-hex
// hashlet chunks.  `out` slice points into `query`; lifetime matches
// the caller's slice.  Sets `out` empty on a query with no branch
// chunk (pure sha query, or empty input).
fun void DOGQueryBranchOnly(u8cs query, u8cs out) {
    out[0] = NULL; out[1] = NULL;
    a_dup(u8c, q, query);
    while (!u8csEmpty(q)) {
        u8cs chunk = {};
        DOGRefDrain(q, chunk);
        if ($empty(chunk)) continue;
        if (u8csLen(chunk) == 40 && DOGIsHashlet(chunk)) continue;
        out[0] = chunk[0];
        out[1] = chunk[1];
        return;
    }
}

// Strip the absolute-form project prefix from a query slice in
// place.  Per VERBS.md §"Ref resolution":
//   `?/<project>/<branch>` (leading `/`) → `<branch>` slice
//   `?/<project>`           (no branch)   → empty (= trunk)
//   `?<branch>`             (no leading `/`)→ left unchanged
//   `?`                     (empty)        → left unchanged
// The query slice's bounds are rewritten so downstream branch /
// REFS-lookup code sees just the branch portion.  Project info is
// local-side state (already consumed by home_open_inner /
// be_ensure_project_repo).
fun void DOGQueryStripProject(u8cs query) {
    if (u8csEmpty(query) || query[0][0] != '/') return;
    u8csUsed1(query);
    u8c const *p = query[0];
    while (p < query[1] && *p != '/') p++;
    if (p < query[1]) query[0] = (u8c *)(p + 1);
    else              query[0] = query[1];
}

// Read-only project-segment extractor — the non-consuming sibling of
// `DOGQueryStripProject`.  Absolute `?/<title>` or `?/<title>/<branch>…`
// → `<title>`; a non-absolute (no leading `/`) or empty query → empty.
// `out` slice points into `query` (no copy).
fun void DOGQueryProject(u8csc query, u8cs out) {
    out[0] = NULL; out[1] = NULL;
    if (query[0] == NULL || query[0] >= query[1] || query[0][0] != '/')
        return;
    u8c const *s = query[0] + 1;            // past the leading '/'
    u8c const *p = s;
    while (p < query[1] && *p != '/') p++;
    out[0] = (u8c *)s;
    out[1] = (u8c *)p;
}

// Canonical project TITLE from a parsed source/clone URI (wiki
// Title.mkd: "title IS the project segment").  Three-step precedence:
//   1. `?/<title>` query segment (the manual override)        → title
//   2. `/.be/<seg>/` in the path (a local-shard anchor URI)    → seg
//   3. URL path basename, trailing `/` + `.git` stripped       → base
// Step 3 makes this a DERIVATION function — right for naming a shard
// from a fetch source, but NOT for anchor readback, where an empty
// result must stay empty (legacy elided single-project): use
// `DOGProjectFromBe` (== step 2 alone) there.  `SNIFFSubBasename` is
// the raw-string (unparsed, SCP-aware) sibling of step 3.  `out` is
// reset before the feed.
fun void DOGTitleFromUri(uricp u, u8bp out) {
    u8bReset(out);
    //  1. Override: the query's absolute project segment wins.
    a_dup(u8c, q, u->query);
    u8cs qproj = {};
    DOGQueryProject(q, qproj);
    if (!u8csEmpty(qproj)) { u8bFeed(out, qproj); return; }
    //  2. Local-shard anchor: first segment after `/.be/`.
    a_dup(u8c, apath, u->path);
    DOGProjectFromBe(apath, out);
    if (u8bDataLen(out) > 0) return;
    //  3. Default: basename of the URI path, sans trailing `/` + `.git`.
    a_dup(u8c, base, u->path);
    while (!u8csEmpty(base) && *(base[1] - 1) == '/') u8csShed1(base);
    u8c const *slash = NULL;
    for (u8c const *c = base[0]; c < base[1]; c++)
        if (*c == '/') slash = c;
    if (slash) base[0] = (u8c *)(slash + 1);
    if (u8csLen(base) >= 4) {
        u8c const *suf = base[1] - 4;
        if (suf[0] == '.' && suf[1] == 'g' && suf[2] == 'i' && suf[3] == 't')
            base[1] -= 4;
    }
    if (!u8csEmpty(base)) u8bFeed(out, base);
}

// Detect and split the canonic resolved query form (STORE.md
// §"URI structure" → "Every input query shape resolves to a single
// canonical form"):
//
//     /<project>/<branch-path>/<pin>
//
// Leading `/` is mandatory; at least three '/'-separated segments
// total (project + ≥1 branch segment + pin).  Pin is a 40-hex sha
// (resolved tip); tag-like pins land in a later phase.
//
// On a canonic match this populates the three out slices to alias
// slices of `query` (no copy) and returns YES.  On miss it returns
// NO and leaves the out slices untouched — callers fall through to
// their existing query-as-branch parsing.
fun b8 DOGCanonQueryParse(u8csc query, u8cs project,
                          u8cs branch, u8cs pin) {
    if (u8csLen(query) < 5)        return NO;   // "/a/b/c" minimum
    if (query[0][0] != '/')        return NO;

    u8cs q = {};
    u8csMv(q, query);
    u8csUsed1(q);                  // step past the leading '/'

    u8cs head_scan = {};
    u8csMv(head_scan, q);
    if (u8csFind(head_scan, '/') != OK) return NO;

    u8cs tail_scan = {};
    u8csMv(tail_scan, q);
    if (u8csRevFind(tail_scan, '/') != OK) return NO;

    //  After RevFind, tail_scan[1] is one past the trailing '/' —
    //  i.e. exactly the start of the pin.  Pin must be 40-hex.
    u8cs pin_local = {tail_scan[1], q[1]};
    if (u8csLen(pin_local) != 40)        return NO;
    if (!HEXu8sValid(pin_local))         return NO;

    //  Project = head segment up to the first '/'.  head_scan[0]
    //  points AT that '/' after Find.
    u8cs proj_local = {q[0], head_scan[0]};
    if (u8csEmpty(proj_local))           return NO;

    //  Branch = bytes between first and last '/', exclusive.  When
    //  there's a single segment in the middle the slice is non-empty;
    //  zero-segment (e.g. "/proj//sha") is rejected as malformed.
    u8c const *branch_start = head_scan[0] + 1;
    u8c const *branch_end   = tail_scan[1] - 1;
    if (branch_start >= branch_end)      return NO;
    u8cs branch_local = {(u8c *)branch_start, (u8c *)branch_end};

    u8csMv(project, proj_local);
    u8csMv(branch,  branch_local);
    u8csMv(pin,     pin_local);
    return YES;
}

// Classify a *new* ref name as branch (dir ref) vs tag (file ref).
// For refs that already exist in REFS, callers must check the existing
// kind first and override; this helper only fixes the default for a
// fresh ref so PUT/POST know whether to materialise a dir shard.
//
// Rule (VERBS.md §"Ref kinds"):
//     empty           → BRANCH (trunk)
//     contains '/'    → BRANCH (`feat/fix`, `feat/`, `./sub`, `../sib`)
//     is `.` or `..`  → BRANCH (bare relative anchor)
//     otherwise       → TAG    (`v1.2.3`, `feat`)
//
// Returns YES for branch, NO for tag.  Trailing-slash callers strip
// the slash before storing the canonical ref bytes; the slash on its
// own is the signal, not part of the name.
b8   DOGRefIsBranch(u8cs ref);

// --- Puppies: stack of `<seqno>.<ext>` files ---
//
// A "puppy" is one git-pack-style file (mmap'd, contents are bytes
// of fixed-size sorted records — wh128, u64, etc.).  Keeper's pack
// indexes, graf's DAG runs, and spot's posting runs are all stacks
// of puppies under their respective `.be/<dog>/` dirs, named
// `<seqno>.<ext>` where `<seqno>` is a 10-char zero-padded RON64
// integer and `<ext>` is dog-specific (`.keeper.idx`, `.graf.idx`,
// `.spot.idx`, `.keeper` for raw pack data).
//
// State is one `kv64b`: each entry is `(key=pup_key, val=fd)` with the
// mmap'd bytes living in `FILE_WANT_BUFS[fd]`.  Path and ext are
// caller-owned and re-passed per call so the same primitive serves
// multiple stacks per dog (keeper has both `.keeper` packs and
// `.keeper.idx` runs).  The typed merge during compaction stays
// caller-side (HIT*Compact); this API only does fs-level housekeeping.
//
// Pup keys are 60-bit ron60-style values (10-char RON64 filename).
// Sequential write callers pass a `keep_next_pup_key`-style globally
// monotonic value so two writers / collapsed sub-shards can't collide.
#define DOG_PUP_SEQNO_W 10

con ok64 DOGPUPFAIL = 0x3584197993ca495;

// Scan `dir` for files matching `<10-RON64><ext>`, sort by pup_key,
// FILEMapRO each, push (pup_key, fd) into `pups`.  Empty dir → OK with
// pups left empty.  Caller must have allocated `pups` first.
// Not idempotent: every file in `dir` becomes a new entry, even if a
// matching pup_key already sits in PAST/DATA — same basename in a
// different dir is a different file.  Avoiding double-scans is the
// caller's responsibility (keeper / graf compare the target branch
// against `home->cur_branch` and skip the shared LCA prefix).
ok64 DOGPupOpenAll(kv64b pups, path8sc dir, u8csc ext);

// Cross-branch load: flip the current DATA into PAST (collapse the
// previously-loaded branch into the read-only context), then open
// `dir`'s pup files into the now-empty DATA.  Mirror of "open first
// branch, then `pups[1]=pups[2]`, then open the second" — the call
// sequence becomes one helper.  The active leaf becomes the dir just
// loaded; subsequent KEEPPackOpen / DOGPupCreate writes target this
// new DATA.  Like DOGPupOpenAll, this primitive does not dedup against
// already-loaded entries.
ok64 DOGPupOpenAside(kv64b pups, path8sc dir, u8csc ext);

// Atomically write `bytes` to `<max(now_ron60, max(DATA)+1)>.<ext>`
// (tmp+rename), FILEMapRO, push (new_pup_key, fd) onto `pups`.
// New key picks `RONNow()` (60-bit ron60) when it dominates the local
// max, else `max(DATA)+1` — same monotonic invariant the keeper's
// `keep_next_pup_key` enforces, so independent writers across sub-
// shards converge to non-overlapping key ranges.
ok64 DOGPupCreate(kv64b pups, path8s dir, u8cs ext, u8cs bytes);

// Same as DOGPupCreate but the caller picks the pup_key explicitly.
// Refuses with DOGPUPFAIL when a live entry with the same pup_key
// already sits in DATA (caller should DOGPupThinTail first when
// replacing the tail).  Used by keeper to keep `.keeper.idx` and
// `.keeper` file-ids in lockstep across branches.
ok64 DOGPupCreateAt(kv64b pups, path8s dir, u8cs ext, u8cs bytes,
                    u64 pup_key);

// Unmap and unlink the youngest `m` puppies; trim `pups` by `m`.
ok64 DOGPupThinTail(kv64b pups, path8s dir, u8cs ext, u32 m);

// Unmap every puppy and free `pups` itself.
ok64 DOGPupClose(kv64b pups);

// Number of live puppies in the stack — DATA only (the active leaf,
// when callers use the PAST/DATA partition; see abc/Bx.h §PastDataS,
// keeper/KEEP.h "Branch-aware object store").  For "every loaded
// run" reads (cross-branch lookups, LSM merges), use `DOGPupCountAll`.
fun u32 DOGPupCount(kv64b pups) { return (u32)kv64bDataLen(pups); }
// Number of live puppies across PastData — every loaded run including
// inherited / read-only PAST entries.  Used by cross-branch readers
// (keeper's LSM lookups, graf's keeper-side idx scan, wire enumeration).
fun u32 DOGPupCountAll(kv64b pups) { return (u32)kv64bPastDataLen(pups); }

// Byte slice of the i-th puppy's contents (lookup via FILE_WANT_BUFS).
// Indexes into DATA only — see `DOGPupCount` caveats.  For an
// "every loaded run" indexer, use `DOGPupDataAll`.
// Writes [data_start, data_end) into `out`; out becomes empty when
// `i` is out-of-range or the file's mapping has been released.
void DOGPupData(u8csp out, kv64b pups, u32 i);
// Like DOGPupData but indexes into PastData — every loaded run.
void DOGPupDataAll(u8csp out, kv64b pups, u32 i);

// Fill `out` with one [data_start, data_end) slice per live puppy
// (oldest → newest, matching the kv64b's pup_key order from
// DOGPupOpenAll).  Resets `out` first.  Skips puppies whose slot has
// been released — the resulting slice count may be less than
// DOGPupCount.  Used by the LSM merge / lookup loops in keeper, graf,
// spot to view the whole stack as one `u8css`.
ok64 DOGPupAllData(u8csb out, kv64b pups);

// Pup key of the i-th puppy.  Returns 0 when out-of-range.
fun u64 DOGPupSeqno(kv64b pups, u32 i) {
    if (i >= kv64bDataLen(pups)) return 0;
    kv64 const *base = (kv64 const *)kv64bDataHead(pups);
    return base[i].key;
}

// Append a short human date for unix timestamp `ts` (seconds) into
// `into`, picking the coarsest representation that still resolves
// `now - ts` unambiguously:
//
//   < 12hr past, or same calendar day → "HH:MM"   ("12:34")
//   < 7 days past                     → "WdyDD"   ("Tue05")
//   < ~6 months past, or same year    → "DDMon"   ("01Jan")
//   else                              → "DDMonYY" ("01Jan25")
//   ts <= 0                           → "?"
//
// Output is always centred-padded to exactly 7 ASCII columns so a
// column of dates lines up cleanly.  `now` is caller-supplied
// (typically `time(NULL)`) so tests can pin a moment.
ok64 DOGutf8sFeedDate(u8s into, i64 ts, i64 now);

// Classify a CLI arg: parse it as a URI, and when the parse is
// degenerate (bare token, no structure), back-fill the URI's `query`
// or `fragment` slot from the raw text per this table:
//
//   contains whitespace  → fragment  (commit msg, search phrase)
//   40 hex chars         → query     (SHA)
//   ref-safe (alnum _-.) → query     (branch/tag shorthand)
//   starts with /,./,../ → leave as path
//   anything else        → fragment
//
// DOGParseURI is attempted first; args that already have scheme,
// authority, rooted path, `?`, or `#` pass through unchanged.
ok64 DOGNormalizeArg(urip u, u8csc arg);

// Move a bareword sitting in u->path (the default slot from
// DOGNormalizeArg) into the verb's natural default slot.  No-op
// when the URI has any other component populated, when path
// contains a '/' (path-shaped, not bareword), or when slot is 'p'.
//
// slot values:  'q' → query (?ref)
//               'f' → fragment (#frag)
//               'p' (or anything else) → leave as path
//
// Per-verb defaults (see VERBS.md §"Bareword defaults"):
//   POST    → 'f'   commit message
//   GET     → 'q'   branch
//   HEAD    → 'q'   branch
//   PATCH   → 'q'   branch
//   PUT     → 'p'   path (file staging)
//   DELETE  → 'p'   path (file unlink)
//   verbless→ 'p'   path (open in bro)
ok64 DOGPromoteBareword(urip u, u8 slot);

#endif
