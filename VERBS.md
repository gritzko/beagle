# `be` HTTP-verb command syntax

The `be` dispatcher uses an HTTP-like verb vocabulary —
GET, POST, PUT, PATCH, DELETE — over the URI grammar from
`dog/DOG.md` and `beagle/GURI.md`.  The verb says direction and
intent; the URI picks the resource.

    be <verb> [--flags] [scheme:][//auth][path][?ref][#frag]

This document is the canonical reference for that mapping.  It
assumes the branch-sharded storage model from `keeper/README.md`
and the per-wt `.sniff` state from `sniff/AT.md`.

##  URI recap

    [scheme:] [//authority] [path] [?ref] [#fragment]

  - `scheme:`   — transport (`ssh:`, `https:`, `file:`, `be:`) or
                  a **view projector** (`sha1:`, `blob:`, `tree:`,
                  …).  See "Schemes" below.  No scheme = act
                  locally on the resource per the verb.
  - `//auth`    — remote host or alias (`//origin`, `//github`).
  - `path`      — file or directory inside the branch's tree.
                  With a `file:` scheme, the path is a filesystem
                  path to another store/wt on this host.
  - `?ref`      — branch path or sha/range.  Branch refs mirror
                  the on-disk tree: `?feature/fix1` ⇢
                  `<store>/feature/fix1/`.  Bare `?A` is absolute
                  (≡ `?/A`); `?./A` means a child of the current
                  branch, `?../A` a sibling.
  - `#frag`     — object hash (`#abc1234`), line jump
                  (`#42`, `#10-20`), or extension filter
                  (`#.c.h`).  See `dog/FRAG.h`.  Search bodies
                  live in their own projector schemes (`spot:`,
                  `grep:`, `regex:`), not in the fragment slot.

Branches form a tree; the **trunk** is the root.  "Create a
branch" = "create a dir under a parent branch", and that's
**POST's** job — GET never auto-creates.  Each branch is a
**linear** stack from its fork commit to its tip, and every
commit has **exactly one parent**.  POST is *with-history*
(ff or rebase) and never produces a merge commit; PATCH is
*history-erased* (absorbs another branch's delta into the wt as
a single-parent commit on the next POST).

##  Ref resolution

A `?ref` resolves in this order:

 1. Absolute path (`?A`, `?A/B`, `?/A/B`) ⇒ that dir under the
    store root.  Bare `?A` is **absolute**, not relative to the
    current branch.
 2. Relative path (`?./A`, `?../A`, `?..`) ⇒ rooted at the
    current branch dir.  Refused from a detached wt (ambiguous —
    pick an explicit branch first).
 3. Sha prefix (`?abc1234`) ⇒ object lookup; attaches as a
    detached wt.

Branch creation is shared between PUT and POST.  `be put ?./fix`
forks a label-only child at the current tip (no commit);
`be post ?./fix#msg` forks **and** commits in one step (advance
from zero).  `be put ?feat/new` or `be put ?feat/` creates a new
leaf under existing `feat`.  GET refuses on a missing ref (no
auto-fork on switch).

| URI | From branch `feature` |
|---|---|
| `?./fix`            | Child branch `feature/fix`; error if missing (POST creates). |
| `?../fix`           | Sibling branch `fix`; error if missing (POST creates). |
| `?..`               | Parent branch (the trunk if `feature` is top-level). |
| `?fix`              | Absolute lookup of root-level branch `fix`; error if missing. |
| `?feat/fix`         | Absolute path; same regardless of current branch. |
| `?feat/`            | Absolute path with **trailing slash**: in PUT, "create a leaf under `feat` reusing cur's basename"; elsewhere, error. |

##  Verb × URI aspect

Each URI component contributes one aspect; verbs combine them
orthogonally.  Pick the verb, then each populated URI part adds
its effect.

|        | `?branch`             | `./path`                | `//remote`                | `#frag`                          |
|--------|-----------------------|-------------------------|---------------------------|----------------------------------|
| POST   | advance branch        | —                       | advance remote (push)     | new commit (msg = frag)          |
| GET    | switch wt to branch   | restore file in wt      | fetch from remote         | sha pin / detach                 |
| PUT    | create branch         | stage file/dir          | register remote alias     | —                                |
| DELETE | drop branch           | unlink + stage delete   | drop alias / push delete  | —                                |
| PATCH  | absorb branch (3-way) | restrict merge to file  | fetch + absorb            | (search rewrites use `spot:` projector — see §"View projectors") |

The default for an empty `?branch` slot is **cur** (current
branch).  So `be post #msg` ≡ "advance cur with a new commit",
and `be post ?br#msg` ≡ "advance br with a new commit (parent =
cur.tip), cur untouched".  The verb only modifies the named ref;
cur is just the current-tip pointer that serves as the new
commit's parent.

Argument shape: every non-flag argv token becomes one URI via
`DOGNormalizeArg`.  Tokens with whitespace classify as fragment
(commit messages: `be post 'fix the typo'`).  Tokens with `?`,
`#`, `/`, or a known scheme go through the URI lexer.  Bare
tokens (`README`, `feat`, `v1.2.3`) are paths by default;
ref-expecting verbs (POST, PATCH, GET) promote a path to query
when the bytes match the QURY ref grammar.  Single-word commit
messages need explicit `#`: `be post '#fix'`.

##  Schemes

Two kinds.

### Transport schemes

Select **where** the resource lives.

| Scheme  | Meaning |
|---------|---------|
| *(none)*| Local store, current branch dir. |
| `ssh:`  | Remote host over ssh; use with `//host/path`. |
| `https:`| Remote host over https. |
| `be:`   | Peer dog over `keeper upload-pack` / `receive-pack` (`keeper/WIRE.md`). |
| `file:` | **Local sibling worktree/store** at the given path (see §Worktree management).  Equivalent to passing the path directly, but makes "this is a local repo, wire me as a worktree" explicit. |

### View projectors

Read-only verbs in their own right.  GET is repo→worktree
(clone / checkout / switch branch) and never appears with a
projector — the projector scheme **is** the verb.  Each one
emits a stream the bro/worker arch displays: the worker (graf,
keeper, …) emits `dog/HUNK` TLV, bro pages it.

| Scheme    | Emits                                            | Example |
|-----------|--------------------------------------------------|---------|
| `sha1:`   | 40-hex sha of the resource                       | `be sha1:?feat` |
| `blob:`   | raw bytes of a blob                              | `be blob:file.c?123abc` |
| `tree:`   | tree listing (mode, sha, name)                   | `be tree:src/?feat` |
| `commit:` | commit object body                               | `be commit:?123abc` |
| `log:`    | `REFS` tail, newest-first, one commit per line (`#N` = last N) | `be log:?feat#10` |
| `refs:`   | list refs under a dir (`**` = recursive)         | `be refs:?**` |
| `diff:`   | weave-based unified diff; right-hand side is *ours* (the changed state).  `diff:` and `diff:<path>` use the wt as *ours* and the sniff `--at` baseline as the from-side; `diff:?<branch>` and `diff:<path>?<branch>` use the baseline as *ours* and `<branch>` as the from-side; `diff:?<h1>..<h2>` is explicit (no baseline needed).  Refuses with `GRAFNOAT` when a baseline is needed but no `--at` is forwarded. | `be diff:file.c`, `be diff:?main`, `be diff:?v1..v2` |
| `size:`   | byte size of the resource                        | `be size:?#abc1234` |
| `type:`   | object type (`commit`/`tree`/`blob`/`tag`)       | `be type:?#abc1234` |
| `spot:`   | structural search; body in fragment, optional `.ext` filter | `be spot:#'u8sFeed( a, b )'.c` |
| `grep:`   | literal substring search                         | `be grep:#u8sFeed.c` |
| `regex:`  | PCRE search                                      | `be regex:#'u\d+sFeed'.c` |

Search projectors (`spot:`, `grep:`, `regex:`) replace the older
`#'body'` / `#name` / `#/regex/` fragment-search forms.  Each URI slot
carries one orthogonal aspect:

  - **scheme** — search backend (`spot` / `grep` / `regex`).
  - **fragment (`#…`)** — search body.  Surrounding `'…'` quotes get
    stripped so shell quoting applies once.  A trailing `.ext` on the
    fragment narrows by extension.
  - **path** — file/dir narrowing inside the worktree.
  - **`?ref`** — search at a specific revision.

So `be spot:/graf?feature#HASHu64Put` reads as "structural search for
`HASHu64Put` under `/graf` on branch `feature`".  All four slots
compose; any combination is valid.

Projectors are pure — they never mutate.  They compose with
`//auth` (`be sha1://origin?main` is a cheap reachability probe
that requests just the tip sha) and with `path+ref` (the path
says what, the ref says from where, the projector says in what
form).

##  Remote resolution is lazy

When a verb takes a remote:

  - `be get //origin?feat` — "feat from origin"; origin's
    concrete URL is resolved from `<store>/ALIAS`.
  - `be get ssh://host/path?feat` — explicit URL; on first use
    it is recorded as an alias (default name derived from host or
    user-supplied with `--as=origin`).
  - `be get //origin` — fast-forward the **current branch** from
    origin's counterpart.
  - `be get ?A` — **local**: resolve per §"Ref resolution"; no
    network.  Unresolved refs are errors (PUT or POST creates).

Alias lookup walks up the dir tree to the store root, same as
`<store>/ALIAS` in `keeper/REF.md`.

##  GET — repo → worktree

GET reads from the repo into the worktree: clone, checkout,
switch branch.  GET is **repo-read-only**: it never modifies a
branch's history.  Like git, GET refuses if any wt file with
local edits would be overwritten — abort up front, no partial
reset.  Projector views are **not** GET — see §"View projectors";
each one (`sha1:`, `blob:`, `log:`, …) is its own verb.

For the **remote** forms (`be get //origin`, `be get //origin?A`),
GET is also **fast-forward-only** on the *local branch's tip*:
it refuses if the local tip is not an ancestor of the incoming
remote tip.  This rule applies only when GET is syncing a local
branch from its remote counterpart, not to local branch switches.

| Form | Effect |
|---|---|
| `be get ?feat`                 | Switch the wt to branch `feat`, reset files from its tip.  Refuses on dirty overlap. |
| `be get ?./fix`                | Switch the wt to child branch `fix`; error if missing (PUT or POST creates). |
| `be get ?../fix`               | Switch the wt to sibling branch `fix`; error if missing (PUT or POST creates). |
| `be get ?abc1234`              | Detached checkout on a sha.  `post`/`patch` refuse until re-attached. |
| `be get file.c?feat`           | Overwrite one file in the wt from another branch's tip (no staging). |
| `be get //origin`              | Fast-forward the **current branch** from its `//origin` counterpart.  Refuses on divergence — resolve with `be patch //origin`. |
| `be get //origin?feat`         | Lazy-remote: fetch `feat` from origin (pack + REFS). |
| `be get ssh://host/path?feat`  | Explicit URL; same effect, registers an alias on first use. |
| `be get file:../proj?feat`     | **Local worktree**: wire this empty cwd as a wt sharing `../proj`'s store, reset files to `feat`'s tip. |
| `be get //origin?*`            | Fetch every branch origin advertises (opt-in bulk form). |

After a successful GET, `.sniff` records the new base as
`(branch, tip-sha)` and clears any pending PATCH parents.  Bare
`be get` (no ref, no remote) is a no-op status.

##  POST — advance / commit

POST has three orthogonal aspects (see §"Verb × URI aspect"):

  - **`#frag`** — make a new commit (msg = fragment).  No
    fragment ⇒ no commit.
  - **`?branch`** — the named branch advances (default: cur).
    Cur is not modified unless cur *is* the named branch.
  - **`//remote`** — also push the result to the remote.

The new commit's parent is always cur.tip.  POST never produces
a merge commit; every commit is single-parent.  When the named
branch and cur diverge, POST rebases cur's stack onto the named
branch's tip with patch-id dedup; cur's old SHAs are not
rewritten — the rebased copies live on the named branch as new
commits appended on top.  POST is **always append-only** on the
named target; descendants of either branch stay valid.

The CAS is on the named branch's expected tip — concurrent
posters that move it between fetch and post are rejected and
the client retries.  Empty POSTs (no commit, no ref move,
nothing to push) are refused with `POSTNONE`.

**POST advances linearly — possibly from zero.**  When the named
target doesn't exist, POST treats it as zero-tip and advances by
forking from cur: `be post ?./fix#msg` creates `fix` and lands one
commit on it in a single step.  PUT remains the **label-only**
fork (`be put ?./fix` makes the branch with no commit).
Force-rewrite is `be delete ?<branch>` then `be put ?<branch>`
(explicit).

### Per-file classification via stamps

Every row sniff writes (`get`, `post`, `patch`, `put`) stamps
each file it touched with the row's `ts` via `utimensat`.  At
POST time we read each on-disk file's mtime and look up the
owning row by ts:

| `mtime` lookup       | Fate (selective mode¹) | Fate (implicit mode²) |
|----------------------|------------------------|------------------------|
| `< last_get_ts`      | KEEP (unchanged since reset) | KEEP |
| `get` / `post` row   | KEEP (baseline content)      | KEEP |
| `patch` row          | REWRITE (merged bytes)       | REWRITE (merged bytes) |
| `put` row            | REWRITE (current bytes)      | REWRITE (current bytes) |
| ∉ stamp-set (edited) | ignore unless explicit `put` named it (warn if put exists; current bytes win) | REWRITE |

¹ Selective = at least one explicit `put` / `delete` row since
last post.  ² Implicit (a.k.a. commit-all) = none.

### Single-parent commits

The new commit's parent is always cur's previous tip — period.
PATCH-merged content contributes to the new tree but **not** to
the parent set; provenance is erased at PATCH time.  Cross-
branch deduplication (when cur eventually flows toward trunk)
relies on patch-id matching, not recorded parents.

### ULOG scope and boundaries

Two boundaries in `.sniff`, both anchored at the most recent `get`
row (a `get` is a hard reset of the world):

  * **pd boundary** — most recent `get` *or* `post`.  `put` /
    `delete` rows after this are in scope for the next POST.
  * **patch boundary** — most recent `get` *or* commit-all `post`.
    `patch` rows after this are in scope.

A `post` row classifies as commit-all iff no put/delete rows lie
between its own pd boundary and itself; determinable on the fly
from a single forward scan, so no new ULOG verb is needed.

### Wall-clock guard

Every command checks `now ≥ last_log_ts` on entry and refuses
with `CLOCKBAD` if the system clock has moved backwards.  One
`ts` is reserved per command, shared by every row + file stamp
written in that invocation.

Commit-message channel: a quoted whitespace-bearing arg
classifies as fragment automatically (`be post 'fix the typo'`).
Single-word messages need explicit `#`: `be post '#fix'`.
Legacy `-m "msg"` is still accepted.

| Form | Effect |
|---|---|
| `be post`                          | No-op (no commit, no ref move).  Status / dry-run. |
| `be post 'fix the typo'`           | Commit on cur (msg = "fix the typo"); cur advances. |
| `be post '#fix'`                   | Commit on cur (msg = "fix"); cur advances. |
| `be post ?feat`                    | Advance feat to cur.tip (label move; ff if linear, rebase otherwise).  Cur untouched. |
| `be post ?feat#msg`                | New commit (parent = cur.tip), advance feat to it.  Cur untouched. |
| `be post ?./stash#wip`             | One-liner stash: create child `stash` (advance from zero), commit dirty there with msg "wip"; cur clean. |
| `be post ?..`                      | Advance parent to cur.tip (ff or rebase).  Cur untouched. |
| `be post //origin`                 | Push cur to origin via `keeper receive-pack` (`keeper/WIRE.md`). |
| `be post //origin?feat`            | Advance feat locally to cur.tip and push to origin. |
| `be post //origin?feat#msg`        | New commit, advance feat, push. |

Each POST appends one or more entries to the target branch's
`REFS` — one for the new commit (when `#frag` is present), plus
rebased equivalents when cur's stack diverged from the target.
Every commit is single-parent.  When cur's stack is rebased over
existing commits on the target, **patch-id dedup** silently
skips replays whose normalized diff is already reachable from
the target tip.  No descendant invalidation: POST always
appends, never rewrites a branch's stack.

##  PUT — create / stage / register

PUT has three orthogonal aspects (see §"Verb × URI aspect"):

  - **`?branch`** — create the named branch as a label at
    cur.tip (no commit).
  - **`./path`** — stage that file or dir into the next commit.
  - **`//remote`** — register the URL as a remote alias.

Bare `be put` (no URI) stages every tracked-and-dirty file.
Each `put` row stamps its file via `utimensat` to the row's
ts, so the file's mtime points back to the put row that owns
its content.

| Form | Effect |
|---|---|
| `be put`                | Stage every tracked-and-dirty file (one `put` row per path).  Refuses with `PUTNONE` if no tracked file is dirty. |
| `be put file.c`         | Stage one file.  Refuses with `PUTNONE` if missing or already clean.  Re-stamps the file. |
| `be put src/`           | Stage a subtree.  Tracked dir: dirty files only.  Untracked: every non-ignored file under it. |
| `be put ?./fix`         | Create child branch `fix` at cur.tip (label move; no commit). |
| `be put ?../sib`        | Create sibling branch `sib` at the parent's tip. |
| `be put ?feat/new`      | Create absolute leaf `feat/new` under existing `feat`. |
| `be put ?feat/`         | Same; leaf name reused from cur's basename. |
| `be put ssh://host/path`| Register as a remote alias (name from host, or `--as=name`). |

PUT writes to `<wt>/.sniff` (the ULOG, for `./path` rows),
`.dogs/REFS` (for `?branch` create), or `.dogs/ALIAS` (for
`//remote`); no pack writes happen until POST.

A `put` on a clean baseline-stamped file is refused — re-stamping
it under a `put` row would shift its provenance from baseline to
put for no semantic gain.

##  DELETE — remove

DELETE's meaning depends on URI shape.  In-tree paths actually
unlink files **immediately** (after a dirty-safety check) and
append a `delete <path>` row; a ref URI drops the branch dir.

The dirty check refuses (`DELDIRTY`) if the file's mtime is out
of the stamp-set **and** its content differs from the baseline
sha (cheap mtime check, content-hash only on drift).  Already-
absent paths are an OK no-op.

| Form | Effect |
|---|---|
| `be delete file.c`                  | Unlink the file (refused as `DELDIRTY` if user-edited); append `delete file.c` row. |
| `be delete src/`                    | Atomic pre-flight: scan all descendants; refuse if **any** is dirty.  On pass, unlink all + append one `delete src/` row. |
| `be delete ?feat/fix1`              | **Drop a branch dir.**  Leaf-only by default; refused with `DELDESC` if descendants exist or any wt's `.sniff` records this branch as base.  Reclaims unreachable shards (current GC path).  See `keeper/README.md` §"Delta-dependency DAG" and `sniff/AT.md`. |
| `be delete -r ?feat` / `--force`    | Drop the branch *and* every descendant, depth-first (leaves first).  Still refused if any wt has it as base. |
| `be delete //origin?feat`           | Push a delete (`<old-sha> 000…0 refs/heads/feat`) via `keeper receive-pack` — same wire git uses for `git push -d`. |
| `be delete //origin`                | Drop the remote alias entry from `<store>/ALIAS`.  No network. |

##  PATCH — absorb, history erased

PATCH takes another branch as a whole — its full
`(fork_commit..tip)` stack — and absorbs the delta into cur's
wt as a single 3-way merge: `base = tree(arg.fork_commit)`,
`ours = tree(cur.tip)`, `theirs = tree(arg.tip)`.  The result
lands in the wt; PATCH does **not** commit.  The next POST
turns it into one new single-parent commit on cur.  No
multi-parent commit is ever produced and no provenance is
recorded — cross-branch dedup later relies on patch-id matching
or an explicit `cherry-picked-from` trailer in the commit
message.

PATCH refuses if any file it would touch is dirty in the wt.
For now, files modified by a previous PATCH count as dirty too,
so PATCH-on-PATCH only works on disjoint file sets.
Distinguishing "merge-result-clean" from "user-edited" is TODO.

Conflicts are marked **token-level** with 4-character delimiters
(`<<<<` / `>>>>`), not the line-level 7-char markers git uses.
Existing diff/merge UIs and `git mergetool` won't recognize
them; hand-edit, or use a `diff:?` projection to enumerate.

| Form | Effect |
|---|---|
| `be patch ?..`                      | Absorb parent's progress (parent's stack `fork..tip`) as a single squash on cur. |
| `be patch ?./fix`                   | Absorb child branch `fix` (its stack) into cur. |
| `be patch ?feat/fix`                | Absorb absolute branch `feat/fix` into cur as one commit. |
| `be patch ?trunk`                   | Absorb trunk's stack into cur (sync from trunk). |
| `be patch ?feat..feat2`             | Apply a range diff to the wt (replay another branch's delta between two named refs). |
| `be patch //origin?main`            | Fetch + absorb remote branch into wt.  ≈ `git pull --squash --no-commit`. |
| `be patch file.c?feat`              | Absorb one file's version from another branch into the wt. |
| `be patch spot:#'Old'->'New'.c`     | Delegated to spot: in-place structural rewrite across `.c` files. |

Multiple PATCHes compose into the wt; the next POST emits one
single-parent commit with the merged tree.  Branch identity of
the absorbed sources is **not** recorded — they survive on their
own branches with their own commit history, untouched by PATCH.

##  Worktree management

A **store** is the `.dogs/` directory holding packs, indexes,
REFS, and aliases.  A **worktree (wt)** is a checked-out tree on
disk; per-wt state — base branch, base tip, pending PATCH
parents — lives in `<wt>/.sniff` (see `sniff/AT.md`).  A
secondary wt shares the primary's store via a `.dogs` symlink.

Multiple wts may sit on the same branch; the ff-only POST rule
resolves write races.  Whichever POSTs first wins; the loser
must `be patch ?<branch>` to absorb the new tip before its own
POST can ff.

The guiding rule: **a machine only needs one store per upstream
repo**.  Every extra wt is just another dir with a `.dogs`
symlink back.

### Example 1 — same tree, flip between two named refs

```sh
mkdir proj && cd proj

# clone + checkout v1.2.3 into this tree
be get ssh://server/proj?v1.2.3

# fetch v1.2.4 from the same origin (lazy alias resolution)
be get //origin?v1.2.4

# flip this tree to v1.2.4
be get ?v1.2.4

# …inspect…
be get ?v1.2.3
```

### Example 2 — sibling worktrees on different branches

```sh
# primary store + wt
mkdir proj && cd proj
be get ssh://server/proj?v1.2.3
be get //origin?v1.2.4                 # populate v1.2.4 in the shared store

# spawn sibling wts — each gets its own dir
cd ..
mkdir v1.2.3 && (cd v1.2.3 && be get file:../proj?v1.2.3)
mkdir v1.2.4 && (cd v1.2.4 && be get file:../proj?v1.2.4)
```

Now `proj/`, `v1.2.3/`, and `v1.2.4/` each have a `.dogs`
symlink to the primary store and their own `.sniff` recording
the branch they sit on.

The `file:` scheme makes the "I want a worktree of that local
repo" intent explicit.  Without it, `be get ../proj?v1.2.3` does
the same thing by heuristic (path points at an existing store).

### Example 3 — feature branch workflow

```sh
# on trunk wt
cd proj
be put ?./feat              # PUT creates child branch `feat` at trunk's tip
be get ?./feat              # GET switches the wt to it
echo patch > new.c
be put . && be post '#feat stub'  # stage + commit on `feat`
be post //origin            # push the branch

# back on trunk
be get ?..                  # parent of `feat` is the trunk
be patch ?./feat            # absorb feat's delta into trunk's wt (history erased)
be post '#sync feat'        # single-parent commit on trunk with feat's content folded in
```

### Example 4 — close a worktree

```sh
cd ..
rm -rf proj-feat            # the .dogs symlink and .sniff go with the dir
```

Closing a wt is just removing its dir.  Branch dirs (packs,
REFS) stay put in the primary store.  Use `be delete ?feat` from
another wt to actually drop the branch.

### Example 5 — stash dirty wt and switch

```sh
# on cur with dirty wt; want to switch to otherbranch
be post ?./stash#wip        # one-liner: create stash + commit dirty; cur clean
be get ?otherbranch         # switch (wt now clean)

# come back later
be get ?./stash             # pick up the stashed work
```

The stash idiom rides two POST rules: (a) advance-from-zero
creates `?./stash` on first use, (b) with `?./stash` named, the
new commit lands on stash only — cur is the new commit's parent
but doesn't advance.

##  Common-task cheat sheet

| git | be |
|---|---|
| `git clone URL`                        | `be get //URL` |
| `git fetch`                            | `be get //origin?*` |
| `git pull --ff-only`                   | `be get //origin` |
| `git pull`                             | `be patch //origin && be post '#sync'` (single-parent absorb, not a merge commit) |
| `git checkout -b feat` (child of trunk)| `be put ?./feat && be get ?./feat` (PUT label-only fork, GET switches) |
| `git checkout feat`                    | `be get ?feat` |
| `git worktree add ../feat feat`        | `cd ../feat && be get file:../proj?feat` |
| `git add file && git commit -m`        | `be put ./file && be post '#msg'` |
| `git commit -am "…"`                   | `be put . && be post 'msg with space'` |
| `git rm file && commit`                | `be delete ./file && be post '#msg'` |
| `git branch -d feat`                   | `be delete ?feat` |
| `git merge trunk`                      | `be patch ?trunk && be post '#sync trunk'` (single-parent on cur; no merge commit) |
| `git cherry-pick <sha>`                | `be patch ?<sha>~..<sha>` |
| `git push`                             | `be post //origin` |
| `git push -d origin feat`              | `be delete //origin?feat` |
| `git rev-parse HEAD`                   | `be sha1:?` |
| `git cat-file -p <sha>:file.c`         | `be blob:file.c?<sha>` |
| `git log -n 20 feat`                   | `be log:?feat#20` |
| `git branch -a`                        | `be refs:?**` |
| `git ls-remote origin main`            | `be sha1://origin?main` |

##  Design invariants

 1. **Verb × URI shape is unambiguous.**  A ref-only URI targets
    the branch dir (create/drop/switch).  A path+ref URI targets
    a file in that branch.  `//auth` reaches out to a peer.  The
    projector scheme only reshapes the output.
 2. **Linear branches, single-parent commits.**  Each branch is
    a linear stack from its `fork_commit` to its `tip`; every
    commit has exactly one parent.  POST never produces a merge
    commit; PATCH absorbs without recording provenance.
 3. **GET is repo-read-only; POST is ff-or-rebase.**  GET
    refuses on dirty-overlap (all-or-nothing pre-flight) and, in
    its remote form, refuses non-ff (use `be patch //origin` to
    absorb divergence).  POST does fast-forward when the target
    is at the expected base, otherwise rebases cur's stack onto
    the new tip — but only the named target advances; cur is
    untouched unless cur *is* the target.  CAS on the expected
    tip; concurrent posters that move the target are rejected
    and retry.  Force-rewrite of a non-trunk branch is
    `be delete ?<branch>` then `be put ?<branch>`.  Empty POSTs
    are refused.
 4. **Tree-sharded branches; PUT labels, GET reads, POST advances (possibly from zero).**
    Sub-branch creation: `be put ?./A` (child), `be put ?../A`
    (sibling), `be put ?feat/new` or `be put ?feat/` (absolute
    under existing parent) — all label-only.  POST creates on
    miss when it would advance the named target (`be post ?./A#msg`
    forks A and commits in one step).  Bare `?A` is absolute
    (≡ `?/A`).  GET refuses on a missing ref.
 5. **One store per machine, many worktrees.**  Per-wt state
    lives in `<wt>/.sniff` (base branch, base tip, pending PATCH
    parents).  Secondary wts symlink `.dogs` back to the
    primary.  Multiple wts on the same branch are allowed; the
    ff rule resolves write races.
 6. **Detached mode is explicit** (`?<sha>` with no branch);
    `post`/`patch` refuse on detached wts.
 7. **Projector schemes are read-only.**  They never mutate —
    safe to compose with any verb and with `//auth` without side
    effects on the peer.
 8. **Remote operations are fast-forward only.**  Divergence is
    resolved client-side with PATCH + POST, never by the peer.
 9. **Remote resolution is lazy.**  `//origin` resolves through
    `<store>/ALIAS`; a bare URL registers an alias on first use.
10. **Git-peer interop: byte-faithful, topology-flat.**  Branch
    paths roundtrip as slashy ref names; the dogs branch tree
    collapses to a flat namespace on the git side.  Trunk maps
    to git's `main` (fallback `master`, or remote `HEAD`).
    Naming collisions (`feature` as a leaf vs `feature/fix` as
    a branch with a child) are git-side errors; we relay them
    unchanged.

##  Open edges

  - **`?./x` when the wt is detached** — refuse; detached +
    relative is ambiguous.
  - **`sha1:file.c` without a ref** — defined as the sha-1 of
    the wt's on-disk bytes (git-hash-object semantics).  The
    empty-ref form `sha1:file.c?` returns the tracked-blob sha
    via sniff's index.
  - **`be delete //origin?feat`** — uses git's standard
    delete-via-push (`<old> 000…0 refs/heads/feat`) over keeper's
    receive-pack; behaviour unchanged from vanilla git.
  - **Bulk fetch (`?*`)** — ordering rule:
    parents-before-children, per the delta-dependency DAG
    (`keeper/README.md`); the client walks the ancestor chain
    and runs N upload-pack sessions (`keeper/WIRE.md`).
  - **Projector + mutating verb** — `be post sha1:?feat` etc. is
    not a thing; projectors are their own verbs (read-only).
    "What would be committed" is a separate dry-run concern.
  - **PATCH-on-PATCH state.**  Today PATCH treats files
    previously merged by an earlier PATCH as "dirty," so multi-
    PATCH only works on disjoint file sets.  TODO: distinguish
    "merge-result-clean" from "user-edited" so an arbitrary
    chain of PATCHes can compose.
  - **Squashing / repacking.**  The current GC path is "delete
    branch" (drops shards reachable only from that branch dir).
    A real squash (consolidate a branch's REFS into a single
    commit without dropping the branch) is TODO.
  - **Trunk for git peers.**  First contact reads the remote's
    `HEAD`; if absent, prefer `main` then `master`.  Re-binding
    on a remote default-branch rename is TODO (today the alias
    snapshot wins).
