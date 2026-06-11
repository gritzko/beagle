#include "abc/URI.h"
#include "dog/CLI.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/RON.h"
#include "dog/DOG.h"
#include "dog/DPATH.h"
#include "dog/HOME.h"
#include "dog/THEME.h"
#include "dog/ULOG.h"
#include "graf/BLOB.h"            // GRAFPathDescend (bareword tracked probe)
#include "graf/GRAF.h"            // GRAFResolveVersion (top-of-chain pass)
#include "dog/git/GIT.h"          // GITu8sCommitTree (baseline-tree probe)
#include "dog/WHIFF.h"            // sha1hexFromHex / sha1FromSha1hex
#include "keeper/KEEP.h"           // KEEPOpenBranch (resolver needs refs)
#include "keeper/RESOLVE.h"        // KEEPResolveRef (entry-point resolver)
#include "keeper/REFS.h"
#include "sniff/AT.h"
#include "sniff/SNIFF.h"          // POSTNONE  (low-byte exit signal)
#include "sniff/SUBS.h"

#include "SUBS.h"           // beagle/SUBS.h — BESubsHere / BERecurseInto
#include "DISPATCH.h"       // beagle/DISPATCH.h — exposed helpers + BEExecute

// Distinct codes so the MAIN-wrapper's `Error: <code>` line tells you
// what kind of failure stopped the pipeline — a dog exited non-zero
// (BEDOGEXIT) or died from a signal (BEDOGSIG). Generic BEFAIL is
// reserved for be's own internal slips. RON60 caps names at ~10 chars
// (60-bit base64 encoding) — verify with `abc/ok64 NAME`.
con ok64 BEFAIL    = 0x2ce3ca495;
con ok64 BEDOGEXIT = 0xb38d6103a149d;
con ok64 BEDOGSIG  = 0x2ce35841c490;
//  `be get ?/proj/...` (or any URI carrying an explicit project
//  segment): the project shard dir already exists on disk and
//  there's no row-0 anchor pinning this wt to it yet — a fresh-
//  init would clobber the existing shard's seed files.  User must
//  either drop the shard first or use a different project name.
con ok64 BEPRJDUP  = 0x2ce65b4cd799;

// --- Verb table ---

//  HTTP-verb dictionary (https://replicated.wiki/html/wiki/Verbs.html §"Verb semantics"): HEAD, GET, POST,
//  PUT, DELETE, PATCH.  Everything else (diff, status, log, ls, blame,
//  cat, map, …) is a *projection* — surfaced via `be <proj>:` (URI
//  form) or `be <proj>` (bareword shorthand — see the dispatcher's
//  bareword-projector branch).  No projection name lives in this
//  table; if it did, CLIParse would consume it as a verb and the
//  bareword shortcut would never fire.
static char const *const BE_VERB_NAMES[] = {
    "head", "get", "post", "put", "delete", "patch",
    NULL
};

static void BEUsage(void) {
    fprintf(stderr,
        "Usage: be [verb] [--flags] [URI...]\n"
        "\n"
        "Verbs (https://replicated.wiki/html/wiki/Verbs.html):\n"
        "  head [uri]           peek/dry-run; fetch refs from remote;\n"
        "                       show ahead/behind cur vs the target\n"
        "  get  [uri]           switch wt+cur (mkdir/cd model)\n"
        "  post [#msg|?br|//r]  commit on cur; rebase upstream;\n"
        "                       no commits land on non-cur branches\n"
        "  put  [files|?br|//r] stage files / mint label / FF-push\n"
        "  delete [files|?br]   unlink files / drop branch\n"
        "  patch [uri]          weave-merge another branch into wt\n"
        "\n"
        "URI format: [scheme:][//host][path][?ref][#frag]\n"
        "  //host       = cached remote-tracking refs only (no network)\n"
        "  ssh:         = open a git wire (clone, fetch, push)\n"
        "  be://, file: = open a beagle wire (clone, fetch, push)\n"
        "\n"
        "Bare `be` = status (current branch, ahead/behind, dirty).\n"
    );
}

// --- Run a sibling tool ---

// Run a sibling tool.  `tool` is the dog name (also argv[0] in argv);
// resolved against this process's own argv[0] via HOMEResolveSibling.
static ok64 be_url_project(uricp u, u8csp out);
static ok64 be_sub_shard_setup(cli *c, uri *u);
static ok64 BEProjectorSubs(cli *c, uri *u);

//  Every `*NONE` ok64 (POSTNONE, KEEPNONE, GRAFNONE, SPOTNONE, …)
//  shares the same low byte — `NONE & 0xFFu = 0xCE` — because they
//  all suffix-match `NONE = 0x5d85ce` (see `abc/OK.h::ok64is`).  POSIX
//  truncates exit codes to that single byte, so the wire alone can't
//  tell us which *NONE the child meant — just "some *NONE".  Return
//  the base `NONE`; callers gate on `ok64is(r, NONE)` (which matches
//  every flavour) and switch on the tool name themselves when they
//  need to distinguish.
#define BE_NONE_LOW_BYTE  ((int)(NONE & 0xFFu))

//  Non-static so beagle/SUBS.c can spawn `sniff sub-mount` etc.
ok64 BERun(u8csc tool, u8css argv, b8 bg) {
    sane($ok(tool) && !$empty(tool));
    a_path(path);
    a$rg(a0, 0);
    HOMEResolveSibling(NULL, path, tool, a0);
    pid_t pid = 0;
    call(FILESpawn, $path(path), argv, NULL, NULL, &pid);
    if (bg) done;
    int rc = 0;
    ok64 r = FILEReap(pid, &rc);
    if (r == FILESIGNAL) {
        char const *sname = strsignal(rc);
        fprintf(stderr, "be: " U8SFMT " killed by signal %d (%s)\n",
                u8sFmt(tool), rc, sname ? sname : "?");
        return BEDOGSIG;
    }
    if (r != OK) return r;
    if (rc == BE_NONE_LOW_BYTE) return NONE;
    if (rc != 0) return BEDOGEXIT;
    done;
}

//  Spawn a sibling tool without waiting; caller reaps later via
//  BEReap.  Used by `be get` to run spot/graf/sniff in parallel
//  after keeper completes (DOG.md §10a).
ok64 BESpawn(u8csc tool, u8css argv, pid_t *out_pid) {
    sane($ok(tool) && !$empty(tool) && out_pid);
    a_path(path);
    a$rg(a0, 0);
    HOMEResolveSibling(NULL, path, tool, a0);
    return FILESpawn($path(path), argv, NULL, NULL, out_pid);
}

//  Wait on a previously-spawned child and translate its exit into
//  the BEDOG* code surface that `BERun` uses.
ok64 BEReap(pid_t pid, u8csc tool) {
    int rc = 0;
    ok64 r = FILEReap(pid, &rc);
    if (r == FILESIGNAL) {
        char const *sname = strsignal(rc);
        fprintf(stderr, "be: " U8SFMT " killed by signal %d (%s)\n",
                u8sFmt(tool), rc, sname ? sname : "?");
        return BEDOGSIG;
    }
    if (r != OK) return r;
    //  Same *NONE pass-through as BERun (see comment there); BEReap
    //  is the parallel-reap path for fan-out actions.
    if (rc == BE_NONE_LOW_BYTE) return NONE;
    if (rc != 0) return BEDOGEXIT;
    return OK;
}

// --- Producer → pager pipeline, with the sub fan-out folded in -------
//
//  DIFF-002: a whole-tree projector (`diff:` / `tree:` / `refs:`) whose
//  only change lives inside a mounted submodule must show that sub diff
//  IN THE PAGER, not dump it to the bare terminal after the pager quits.
//
//  A plain `producer | pager` pipeline would only carry the
//  PARENT dog's hunks; the sub relays (BEProjectorSubs → BERelaySub →
//  write(STDOUT_FILENO, …)) used to land on the inherited terminal and
//  surface only after `bro` restored the screen on `q`.  Here the pager
//  reads one pipe, and BOTH sides feed it: the parent producer's stdout
//  is dup2'd onto the write end, then — after the producer is reaped —
//  this parent process points its OWN STDOUT_FILENO at the same write
//  end so the in-process sub fan-out's writes go to the pager too.  One
//  sequential stream, parent-then-subs, paged together; when the parent
//  diff is empty but a sub diff exists the pager shows the sub diff,
//  never "nothing!".
static ok64 BERunPipeSubs(path8sc prod, u8css prod_argv,
                          path8sc pager, u8css pager_argv,
                          cli *c, uri *u) {
    sane($ok(prod) && $ok(pager) && c && u);

    int p[2] = {-1, -1};
    if (pipe(p) != 0) failc(BEFAIL);
    //  CLOEXEC on both ends: each child gets a dup of both, but only the
    //  one dup2'd onto stdin/stdout survives exec — the other inherited
    //  copy auto-closes so pipe ref counts collapse and EOF propagates.
    (void)fcntl(p[0], F_SETFD, FD_CLOEXEC);
    (void)fcntl(p[1], F_SETFD, FD_CLOEXEC);

    //  Pager first: it reads the pipe read end; its stdout inherits the
    //  terminal (bro opens /dev/tty for keystrokes).
    pid_t pager_pid = 0;
    ok64 gs = FILESpawnFds(pager, pager_argv, p[0], -1, &pager_pid);
    if (gs != OK) { close(p[0]); close(p[1]); return gs; }

    //  Parent producer writes its hunks into the pipe write end; reap it
    //  so its whole report lands before the sub fan-out appends.
    pid_t prod_pid = 0;
    ok64 ps = FILESpawnFds(prod, prod_argv, -1, p[1], &prod_pid);
    if (ps != OK) {
        close(p[0]); close(p[1]);
        int rc = 0; (void)FILEReap(pager_pid, &rc);
        return ps;
    }
    close(p[0]);                         //  parent never reads
    int prod_rc = 0;
    (void)FILEReap(prod_pid, &prod_rc);

    //  Fold the sub fan-out into the SAME pipe: save the real stdout,
    //  redirect STDOUT_FILENO → pipe write end, run the relays (they
    //  write to STDOUT_FILENO), then restore.  Clear CLOEXEC on the
    //  redirected fd first so the dup target survives any exec the relay
    //  performs.
    //
    //  bro drains *TLV* from its stdin (BROPipeRun) — the parent dog was
    //  spawned with `--tlv` for exactly that.  But BERelaySub re-emits
    //  the captured sub report in the process-global `HUNKMode`, which
    //  is `HUNKOutColor` on a TTY — that would feed bro pre-rendered
    //  ANSI it can't index, leaving it on "nothing!".  Force `HUNKOutTLV`
    //  for the folded relay so the sub hunks reach bro as TLV (paged and
    //  rendered exactly like the parent's), then restore the mode.
    ok64 sr = OK;
    int saved = dup(STDOUT_FILENO);
    if (saved >= 0) {
        (void)fcntl(p[1], F_SETFD, 0);   //  the dup target must survive
        fflush(stdout);
        if (dup2(p[1], STDOUT_FILENO) >= 0) {
            HUNKout saved_mode = HUNKMode;
            HUNKMode = HUNKOutTLV;
            sr = BEProjectorSubs(c, u);
            HUNKMode = saved_mode;
            fflush(stdout);
            (void)dup2(saved, STDOUT_FILENO);
        }
        close(saved);
    } else {
        //  dup failed: fall back to the unfolded behaviour rather than
        //  drop the sub diff entirely.
        sr = BEProjectorSubs(c, u);
    }

    close(p[1]);                         //  EOF → pager drains + exits
    int pager_rc = 0;
    (void)FILEReap(pager_pid, &pager_rc);

    if (prod_rc  != 0 && prod_rc != BE_NONE_LOW_BYTE) return BEDOGEXIT;
    if (sr != OK)                                     return sr;
    if (pager_rc != 0)                                return BEDOGEXIT;
    done;
}

// --- Verb dispatch: forward URI to dogs in order ---
//
// Each dog parses the URI and handles its part:
//   get:    keeper (fetch + own index) → spot+graf+sniff in parallel
//             (each pulls from keeper's read APIs and updates its own
//              state — see DOG.md §10a "Indexing").
//   put:    sniff (stage tree)            [local only]
//   delete: sniff (stage removal)         [local only]
//   post:   sniff (commit, HEAD move) → keeper (push ref)
//
// Other verbs (post/put/delete/patch/diff) keep their historical
// shape for now; the get-style fork-keeper-then-parallel pattern is
// expected to generalise but is committed only for `get`.

//  Process-wide buffer for the `--at <root>?<branch>#<sha>` URI text
//  forwarded to every sub-dog argv.  Populated once at the top of
//  `becli` from `<cwd>/.be/wtlog` via `SNIFFAtTailOf`; empty when no
//  `.be/wtlog` is present (fresh dir / pre-clone bootstrap), in which
//  case sub-dogs fall back to their own cwd-walk.
static u8 be_at_buf_storage[FILE_PATH_MAX_LEN + 128];
static Bu8 be_at_buf = {
    be_at_buf_storage, be_at_buf_storage,
    be_at_buf_storage,
    be_at_buf_storage + sizeof(be_at_buf_storage)
};
static u8c const be_at_flag_lit[] = "--at";
static u8cs const be_at_flag = {
    be_at_flag_lit, be_at_flag_lit + sizeof(be_at_flag_lit) - 1
};

//  Build a `<dog> <verb> [--at <uri>] [flags...] [URIs...]` argv
//  slice into `args`.  Caller-owned: `args` must be a u8cs Bbuf with
//  space for `4 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS` slots.
void BEBuildArgv(u8csb args, u8csc dog, u8csc verb, cli *c) {
    a_dup(u8c, ldog,  dog);
    a_dup(u8c, lverb, verb);
    u8csbFeed1(args, ldog);
    u8csbFeed1(args, lverb);
    if (u8bDataLen(be_at_buf) > 0) {
        a_dup(u8c, at_flag, be_at_flag);
        a_dup(u8c, at_val,  u8bData(be_at_buf));
        u8csbFeed1(args, at_flag);
        u8csbFeed1(args, at_val);
    }
    //  Flags come as {flag, val} pairs; val is the empty-string
    //  sentinel for booleans.  Forward the flag name always; only
    //  forward its value if it's genuinely non-empty, otherwise the
    //  callee's CLIParse would pick it up as a spurious URI.
    for (u32 j = 0; j + 1 < u8csbDataLen(c->flags); j += 2) {
        u8csbFeed1(args, (*u8csbAtP(c->flags, j)));
        if (!u8csEmpty((*u8csbAtP(c->flags, j + 1))))
            u8csbFeed1(args, (*u8csbAtP(c->flags, j + 1)));
    }
    for (u32 j = 0; j < CLIUriLen(c); j++)
        u8csbFeed1(args, (*u8csbAtP(c->uris, j)));
}

//  YES iff dir `path` looks like a git repo — a worktree
//  (`<path>/.git/objects`) or a bare repo (`<path>/objects` +
//  `<path>/refs`).  Mirrors keeper/WIRECLI.c::wcli_path_is_git_layout
//  so be's `file:` routing agrees with the transport spawn's choice of
//  git-upload-pack vs keeper upload-pack.
static b8 be_path_is_git(u8csc path) {
    if (u8csEmpty(path)) return NO;
    a_cstr(dotgit_s,  ".git");
    a_cstr(objects_s, "objects");
    a_cstr(refs_s,    "refs");
    //  Worktree layout: <path>/.git/objects/
    a_path(wtobj, path, dotgit_s, objects_s);
    if (FILEisdir($path(wtobj)) == OK) return YES;
    //  Bare layout: <path>/objects/ + <path>/refs/
    a_path(objp, path, objects_s);
    a_path(refp, path, refs_s);
    if (FILEisdir($path(objp)) == OK && FILEisdir($path(refp)) == OK)
        return YES;
    return NO;
}

//  Backing store for the rewritten `file:///abs` URI text — must
//  outlive be_file_get_route so the later BEExecute → BEBuildArgv
//  hand-off can forward `u->data` to keeper / sniff (CLAUDE.md §5).
static u8 befile_uri_storage[FILE_PATH_MAX_LEN + 32];
static Bu8 befile_uri_buf = {
    befile_uri_storage, befile_uri_storage,
    befile_uri_storage,
    befile_uri_storage + sizeof(befile_uri_storage)
};

//  `be get file:<path>` routing.  The keeper side already serves a
//  local `file:` git repo by spawning git-upload-pack (KEEP.exe.c +
//  keeper/WIRECLI.c), but both keeper and sniff key transport routing
//  off the `file://` (authority-present) form: a single-slash
//  `file:/abs` reads as a bare path, so be sends it down the local
//  sibling-worktree plan and sniff then tries to *restore* the path.
//  When the target is a git repo, rewrite `file:/abs` →
//  `file:///abs` (preserving any ?query / #fragment) and re-lex, so
//  URIPattern reports URI_AUTHORITY and the forwarded text routes the
//  clone exactly like ssh://.  A `file:` beagle store (no git layout)
//  is left untouched and stays a sibling worktree (https://replicated.wiki/html/wiki/Verbs.html
//  §"Worktree management" Example 2).
static void be_file_get_route(cli *c) {
    if (c == NULL || CLIUriLen(c) == 0) return;
    uri u = {};
    if (CLIUriAt(&u, c, 0) != OK) return;
    a_cstr(file_sch, "file");
    if (!u8csEq(u.scheme, file_sch)) return;   // file: scheme only
    if (u.authority[0] != NULL)      return;   // already file:///abs form
    if (u8csEmpty(u.path))           return;
    a_dup(u8c, p, u.path);
    if (!be_path_is_git(p))          return;   // beagle store → worktree

    //  Rebuild `file://` + <path> [+ ?query] [+ #frag].  The path is
    //  rooted (leading '/'), so `file://` + `/abs` = `file:///abs`.
    u8bReset(befile_uri_buf);
    a_cstr(pfx, "file://");
    if (u8bFeed(befile_uri_buf, pfx)    != OK) return;
    if (u8bFeed(befile_uri_buf, u.path) != OK) return;
    if (!u8csEmpty(u.query)) {
        if (u8bFeed1(befile_uri_buf, '?')     != OK) return;
        if (u8bFeed(befile_uri_buf, u.query)  != OK) return;
    }
    if (!u8csEmpty(u.fragment)) {
        if (u8bFeed1(befile_uri_buf, '#')       != OK) return;
        if (u8bFeed(befile_uri_buf, u.fragment) != OK) return;
    }
    //  Store the triple-slash form back as the raw arg text so the
    //  downstream parse-on-demand reports URI_AUTHORITY and BEBuildArgv
    //  forwards the rewritten bytes verbatim to keeper / sniff.  The
    //  rewrite lives in the static befile_uri_buf, so the slice survives.
    a_dup(u8c, newtext, u8bDataC(befile_uri_buf));
    CLIUriSetRaw(c, 0, newtext);
}

//  `wt_uri_buf` is caller-owned scratch (≥ 64 B) backing the rewritten
//  `?<hashlet>` URI bytes — must outlive this frame so downstream argv
//  build can read `u->data` (CLAUDE.md §5).
ok64 BEGetWorktree(uri *u, u8b wt_uri_buf, b8 *attached_out) {
    sane(1);
    //  GET-012: report whether a store-backed worktree was actually
    //  attached (anchor written / branch switched on an existing wt) so
    //  the caller can suppress the remote-fetch rows.  Default NO — a
    //  pre-existing `.be` at cwd that we did NOT touch is not an attach.
    if (attached_out) *attached_out = NO;
    //  GET-012: skip only a genuine remote (`be://host`, non-empty host).
    //  A local `file://<store>?/proj` has an empty host (bare `//`) and
    //  must proceed so the store-dir form attaches a worktree.
    if (u == NULL || !u8csEmpty(u->host)) done;
    if (u8csEmpty(u->path)) done;

    // Source candidate: either an existing dir containing `.be/` (a
    // worktree, primary-wt layout) or the `.be` store dir itself (a
    // bare store with no default worktree — point `file:` straight at
    // `<store>/.be`).  `path_is_store` selects the branch below.
    a_dup(u8c, prim_s, u->path);
    a_path(prim_path, prim_s);
    a_path(prim_be, prim_s, DOG_BE_S);
    b8 path_is_store = NO;
    if (FILEisdir($path(prim_be)) != OK) {
        //  Not `<wt>/.be` — accept `u->path` when it IS the `.be`
        //  store dir (basename `.be`, an existing dir).
        u8cs base = {};
        PATHu8sBase(base, prim_s);
        if (FILEisdir($path(prim_path)) == OK && u8csEq(base, DOG_BE_S))
            path_is_store = YES;
        else
            done;
    }

    //  GET-012: a worktree-DIR source reached via `file://` (authority
    //  present) — a beagle peer being cloned (be-post-38) — must become
    //  an INDEPENDENT clone, not a secondary worktree of the peer.  Only
    //  the store-dir form (path_is_store) attaches a worktree here; the
    //  bare-path `file:<wt>` form (no authority) still makes a secondary
    //  wt.  Defer to keeper get for the clone.
    if (!path_is_store && !u8csEmpty(u->authority)) done;

    // Skip if cwd already has a .be (dir, symlink, or wtlog file).
    a_path(cwd);
    call(FILEGetCwd, cwd);
    a_path(cwd_be);
    a_dup(u8c, cwd_s, u8bDataC(cwd));
    call(PATHu8bFeed, cwd_be, cwd_s);
    call(PATHu8bPush, cwd_be, DOG_BE_S);
    {
        filestat fs = {};
        if (FILELStat(&fs, $path(cwd_be)) == OK) {
            //  GET-012: cwd already has a `.be`.  A regular FILE is an
            //  existing worktree anchor → local switch below.  A
            //  DIRECTORY is a clone-destination store (the `mkdir .be`
            //  convention, be-post-37/head-20 B-side) → NOT a worktree;
            //  defer to keeper get for an independent clone.
            if (FILEisdir($path(cwd_be)) == OK) done;
            //  cwd is already a worktree.  For the store-direct form,
            //  rewrite the `file:<store>?<ref>` URI to its bare branch
            //  ref so a re-run is a clean local switch — otherwise the
            //  unresolvable `file:<store>` falls through to keeper get
            //  (KEEPNONE).  The worktree-layout source keeps its prior
            //  no-op behavior.
            if (path_is_store && !u8csEmpty(u->query)) {
                u8bReset(wt_uri_buf);
                call(u8bFeed1, wt_uri_buf, '?');
                call(u8bFeed,  wt_uri_buf, u->query);
                zerop(u);
                u8csMv(u->data, u8bDataC(wt_uri_buf));
                //  Local switch on the existing store-backed wt — no clone.
                if (attached_out) *attached_out = YES;
            }
            done;
        }
    }

    // Worktree layout: secondary wt has `<wt>/.be` as a REGULAR FILE
    // (= its own wtlog).  Row 0's `repo` URI names the primary's
    // project shard `<prim>/.be/<project>/`, so keeper/graf/spot
    // reach the shared store via that anchor.  We seed the file with
    // a single `repo` row pointing at the primary, then sniff's row-0
    // read on the next open redirects h->root automatically.
    //
    // Project derivation: peek the primary's own `<prim>/.be/wtlog`
    // row 0 and run DOGProjectFromBe on its URI path.  Empty means
    // the primary is still on the legacy elided layout — fall back
    // to bare `<prim>/.be/`.
    a_path(prim_proj);
    uri prim_uri = {};
    //  Backs prim_uri.{data,query} — must outlive the title/branch
    //  derivation below, so it lives at function scope.
    a_pad(u8, prim_at, FILE_PATH_MAX_LEN + 128);
    if (path_is_store) {
        //  Bare store, no default worktree: there is no primary tip to
        //  read.  Take the project/branch straight from the request
        //  query (`?/<title>/<branch>`) and leave the tip unresolved —
        //  the downstream sniff get resolves the branch tip from the
        //  shared store's refs.
        u8csMv(prim_uri.query, u->query);
    } else {
        {
            a_cstr(wtlog_s, DOG_WTLOG_NAME);
            a_path(prim_wtlog, prim_s);
            call(PATHu8bPush, prim_wtlog, DOG_BE_S);
            call(PATHu8bPush, prim_wtlog, wtlog_s);
            //  Drain row 0 via the ULOG API instead of hand-walking tabs.
            u8bp mapped = NULL;
            if (FILEMapRO(&mapped, $path(prim_wtlog)) == OK) {
                a_dup(u8c, scan, u8bDataC(mapped));
                ulogrec row0 = {};
                if (ULOGu8sDrain(scan, &row0) == OK
                    && !u8csEmpty(row0.uri.path))
                    DOGProjectFromBe(row0.uri.path, prim_proj);
                FILEUnMap(mapped);
            }
        }

        //  Resolve the primary's current tip + branch up front:
        //  SNIFFAtTailOf yields `<prim_root>?/<title>/<branch>#<sha>`.
        //  We carry (title, branch, hash) into the secondary's row-0
        //  anchor (sha-bearing shape, DIS-001) AND reuse the tip to pin
        //  the downstream checkout.  NODATA (primary never posted) is
        //  fine — write a tip-less anchor and let sniff resolve from the
        //  primary's keeper.
        a_dup(u8c, prim_root, prim_s);
        mute(SNIFFAtTailOf(prim_root, prim_at), NODATA);
        if (u8bDataLen(prim_at) > 0) {
            u8csMv(prim_uri.data, u8bDataC(prim_at));
            call(URILexer, &prim_uri);
        }
    }

    //  Title: prefer the resolved query's project segment, else the
    //  primary wtlog's path-encoded project (prim_proj), else empty
    //  (legacy elided primary → bare anchor).
    a_path(title_buf);
    {
        a_dup(u8c, q, prim_uri.query);
        u8cs qproj = {};
        DOGQueryProject(q, qproj);
        if (!u8csEmpty(qproj)) {
            call(u8bFeed, title_buf, qproj);
        } else if (u8bDataLen(prim_proj) > 0) {
            a_dup(u8c, pp, u8bDataC(prim_proj));
            call(u8bFeed, title_buf, pp);
        }
    }
    //  Branch within the project (empty = trunk).
    a_path(branch_buf);
    {
        a_dup(u8c, q2, prim_uri.query);
        DOGQueryStripProject(q2);
        if (!u8csEmpty(q2)) call(u8bFeed, branch_buf, q2);
    }
    //  Hash: the primary tip, when a valid hashlet (6..40 hex).
    u8cs hash = {};
    size_t flen = u8csLen(prim_uri.fragment);
    if (flen >= 6 && flen <= 40) u8csMv(hash, prim_uri.fragment);

    //  Write the secondary-wt row-0 anchor.  Path is the primary store's
    //  `.be/` (store root; title+branch now live in the query, not the
    //  path), plus `?/<title>/<branch>#<hash>`.  Anchor write itself is
    //  sniff's job — same helper the primary-wt layout uses.
    {
        a_path(repo_path, prim_s);
        //  `prim_s` is the store `.be/` itself when path_is_store; only
        //  the worktree layout needs the `.be` segment appended.
        if (!path_is_store) call(PATHu8bPush, repo_path, DOG_BE_S);
        call(u8bFeed1, repo_path, '/');
        a_dup(u8c, tt, u8bDataC(title_buf));
        a_dup(u8c, bb, u8bDataC(branch_buf));
        call(SNIFFWtRepoAnchor, u8bDataC(cwd_be), u8bDataC(repo_path),
             tt, bb, hash);
    }
    //  The `.be` anchor is now written — a store-backed worktree is
    //  attached from here on; the remote-fetch rows must be suppressed.
    if (attached_out) *attached_out = YES;

    fprintf(stderr, "be: worktree from %.*s\n",
            (int)$len(u->path), (char *)u->path[0]);

    //  Set the downstream checkout target (data slot — `BEBuildArgv`
    //  reads only `u->data`).
    if (u8csEmpty(hash)) {
        //  No tip resolved up front.  For the worktree layout (primary
        //  never posted) leave `u` as-is.  For the store-direct layout
        //  the file: URI carries the branch ref in its query — rewrite
        //  to the bare `?<query>` so the downstream sniff get switches
        //  to that branch and resolves its tip from the shared store.
        if (!path_is_store || u8csEmpty(u->query)) done;
        u8bReset(wt_uri_buf);
        call(u8bFeed1, wt_uri_buf, '?');
        call(u8bFeed,  wt_uri_buf, u->query);
        zerop(u);
        u8csMv(u->data, u8bDataC(wt_uri_buf));
        done;
    }
    //  GET-002: the row-0 anchor names the branch (DIS-001), so when a
    //  title resolved the secondary wt must land ATTACHED — rewrite to
    //  the absolute-branch form `?/<title>[/<branch>]` (mirrors the
    //  store-direct arm above) so the downstream sniff get switches to
    //  that branch and a follow-up POST is allowed.  Without a resolved
    //  title (legacy elided primary) fall back to pinning the tip
    //  `?<hashlet>` — detached, as before.
    {
        a_dup(u8c, tt2, u8bDataC(title_buf));
        if (!u8csEmpty(tt2)) {
            a_dup(u8c, bb2, u8bDataC(branch_buf));
            u8bReset(wt_uri_buf);
            call(u8bFeed1, wt_uri_buf, '?');
            call(u8bFeed1, wt_uri_buf, '/');
            call(u8bFeed,  wt_uri_buf, tt2);
            if (!u8csEmpty(bb2)) {
                call(u8bFeed1, wt_uri_buf, '/');
                call(u8bFeed,  wt_uri_buf, bb2);
            }
            zerop(u);
            u8csMv(u->data, u8bDataC(wt_uri_buf));
            done;
        }
    }
    u8bReset(wt_uri_buf);
    call(u8bFeed1, wt_uri_buf, '?');
    call(u8bFeed,  wt_uri_buf, hash);
    zerop(u);
    u8csMv(u->data, u8bDataC(wt_uri_buf));
    done;
}

//  --- Projector submodule recursion (SUBS-011 / SUBS-012) ----------
//
//  A repo-wide read-only projector (search: grep/spot/regex; whole-
//  tree views: diff/tree/refs) must descend into every mounted sub,
//  run the SAME projector there, and merge each sub's hits into the
//  parent's stream path-prefixed under the mount — exactly the
//  HUNKu8sRelay pattern the verbs use (BERelaySub).  Path-scoped
//  projectors that already land inside a mount (`grep:vendor/sub/#…`,
//  `blob:vendor/sub/core.c?…`) are routed by the dog itself and don't
//  need this fan-out; whole-tree forms do.  `--nosub` opts out.

//  YES iff `scheme` names a projector that fans out into subs.  Search
//  projectors (SUBS-011) merge per-sub hits; whole-tree views
//  (SUBS-012: diff, tree, refs) merge per-sub path-prefixed reports.
//  Object/history projectors that resolve a single path (sha1, blob,
//  commit, size, type, log, blame) are NOT here — a sub path on those
//  is handled by the dog's own in-mount resolution, not a fan-out.
static b8 be_projector_recurses(u8cs scheme) {
    a_cstr(s_grep,  "grep");
    a_cstr(s_spot,  "spot");
    a_cstr(s_regex, "regex");
    a_cstr(s_diff,  "diff");
    a_cstr(s_tree,  "tree");
    a_cstr(s_refs,  "refs");
    return $eq(scheme, s_grep) || $eq(scheme, s_spot)
        || $eq(scheme, s_regex) || $eq(scheme, s_diff)
        || $eq(scheme, s_tree)  || $eq(scheme, s_refs);
}

//  Per-sub closure for the projector fan-out.  Carries the parent's
//  projector argv prefix (`be [flags] <proj-uri>` minus the leading
//  `be`) so each mounted sub re-runs the identical projector, with
//  BERelaySub path-prefixing the sub's hunk URIs under the mount.
typedef struct {
    u8cs  wt_root;
    u8cs *argv_head;
    u8cs *argv_term;
    ok64  worst;
} beproj_recurse_ctx;

static ok64 beproj_recurse_cb(besub const *s, void *vctx) {
    sane(s && vctx);
    beproj_recurse_ctx *rc = (beproj_recurse_ctx *)vctx;
    if (!s->mounted) return OK;   //  declared-not-mounted: nothing to query
    u8cs subpath = {};
    u8csMv(subpath, s->path);
    u8css argv = {rc->argv_head, rc->argv_term};
    ok64 r = BERelaySub(rc->wt_root, subpath, argv);
    //  A clean sub (no hits / empty report) returns *NONE — a no-op,
    //  not a failure of the aggregation.  Only real errors stick.
    if (r != OK && !ok64is(r, NONE)) rc->worst = r;
    return OK;
}

//  Fan the projector `u` (its full `u->data` URI) out into every
//  mounted sub, path-prefixed.  Runs AFTER the parent projector has
//  emitted its own hunks, so the merged stream is parent-then-subs in
//  one sequential HUNK stream.  Honours `--nosub`.  Returns the worst
//  sub exit (OK when every sub was clean / a no-op).
static ok64 BEProjectorSubs(cli *c, uri *u) {
    sane(c && u);
    if (CLIHas(c, "--nosub"))        done;
    if (!be_projector_recurses(u->scheme)) done;
    if (!u8bHasData(c->repo))        done;
    a_dup(u8c, wt_root, $path(c->repo));

    //  Child argv = the projector URI plus the forwarded flags (drop
    //  `--at`: each sub resolves its own tip from its mount wtlog, and
    //  BERelaySub appends `--tlv` itself).  Mirror BEHeadSubs.
    a_pad(u8cs, child_args, 4 + CLI_MAX_FLAGS * 2);
    for (u32 j = 0; j + 1 < u8csbDataLen(c->flags); j += 2) {
        if ($eq((*u8csbAtP(c->flags, j)), be_at_flag)) continue;
        u8csbFeed1(child_args, (*u8csbAtP(c->flags, j)));
        if (!u8csEmpty((*u8csbAtP(c->flags, j + 1))))
            u8csbFeed1(child_args, (*u8csbAtP(c->flags, j + 1)));
    }
    a_dup(u8c, proj_uri, u->data);
    u8csbFeed1(child_args, proj_uri);
    a_dup(u8cs, child_argv, u8csbData(child_args));

    beproj_recurse_ctx rc = {
        .argv_head = (u8cs *)child_argv[0],
        .argv_term = (u8cs *)child_argv[1],
        .worst     = OK,
    };
    u8csMv(rc.wt_root, wt_root);
    (void)BESubsHere(wt_root, beproj_recurse_cb, &rc);
    return rc.worst;
}

//  --- Projector path-in-mount routing (SUBS-012) -------------------
//
//  A path-bearing projector (`blob:vendor/sub/core.c?ref`,
//  `tree:vendor/sub?ref`, `log:vendor/sub/core.c`, `blame:…`) whose
//  path lands inside a mounted sub must resolve in the SUB shard, not
//  the parent — the parent tree holds only a `160000` gitlink there,
//  so keeper/graf return KEEPNONE / PROJFAIL.  Route it: rebuild the
//  projector URI with the path made mount-relative and relay it into
//  the sub (BERelaySub re-prefixes the hunk URIs under the mount).

//  YES iff `path` == `sub` or `path` starts with `sub/` — i.e. `path`
//  lands at or inside the mount `sub`.  On a match, `*rel_out` is the
//  mount-relative remainder (empty when `path`==`sub`).
static b8 be_path_in_mount(u8csc path, u8csc sub, u8csp rel_out) {
    if (u8csEmpty(path) || u8csEmpty(sub)) return NO;
    u32 pl = (u32)$len(path), sl = (u32)$len(sub);
    if (pl < sl) return NO;
    u8cs head = {path[0], path[0] + sl};
    if (!u8csEq(head, sub)) return NO;
    if (pl == sl) { rel_out[0] = rel_out[1] = NULL; return YES; }
    if (path[0][sl] != '/') return NO;     //  `vendorX` must not match `vendor`
    rel_out[0] = path[0] + sl + 1;
    rel_out[1] = path[1];
    return YES;
}

typedef struct {
    cli  *c;
    uri  *u;
    u8cs  wt_root;
    b8    handled;
    ok64  rc;
} beproj_route_ctx;

static ok64 beproj_route_cb(besub const *s, void *vctx) {
    sane(s && vctx);
    beproj_route_ctx *rc = (beproj_route_ctx *)vctx;
    if (rc->handled) return OK;            //  first matching mount wins
    if (!s->mounted) return OK;

    u8cs rel = {};
    if (!be_path_in_mount(rc->u->path, s->path, rel)) return OK;

    //  Rebuild `<scheme>:<rel>?<query>#<frag>` for the child.  URIMake
    //  serialises a generic URI; prepend `<scheme>:` so the child's
    //  BEProjector routes it (the scheme is the projector selector).
    a_pad(u8, uri_buf, MAX_URI_LEN);
    a_dup(u8c, sch, rc->u->scheme);
    call(u8bFeed,  uri_buf, sch);
    call(u8bFeed1, uri_buf, ':');
    if (!u8csEmpty(rel)) { a_dup(u8c, r, rel); call(u8bFeed, uri_buf, r); }
    if (rc->u->query[0] != NULL) {
        call(u8bFeed1, uri_buf, '?');
        if (!u8csEmpty(rc->u->query)) {
            a_dup(u8c, q, rc->u->query); call(u8bFeed, uri_buf, q);
        }
    }
    if (rc->u->fragment[0] != NULL) {
        call(u8bFeed1, uri_buf, '#');
        if (!u8csEmpty(rc->u->fragment)) {
            a_dup(u8c, f, rc->u->fragment); call(u8bFeed, uri_buf, f);
        }
    }
    a_dup(u8c, child_uri, u8bDataC(uri_buf));

    //  argv = forwarded flags (drop `--at`) + the rewritten projector.
    a_pad(u8cs, child_args, 4 + CLI_MAX_FLAGS * 2);
    for (u32 j = 0; j + 1 < u8csbDataLen(rc->c->flags); j += 2) {
        if ($eq((*u8csbAtP(rc->c->flags, j)), be_at_flag)) continue;
        u8csbFeed1(child_args, (*u8csbAtP(rc->c->flags, j)));
        if (!u8csEmpty((*u8csbAtP(rc->c->flags, j + 1))))
            u8csbFeed1(child_args, (*u8csbAtP(rc->c->flags, j + 1)));
    }
    u8csbFeed1(child_args, child_uri);
    a_dup(u8cs, child_argv, u8csbData(child_args));

    u8cs subpath = {};
    u8csMv(subpath, s->path);
    u8css argv = {(u8cs *)child_argv[0], (u8cs *)child_argv[1]};
    rc->rc = BERelaySub(rc->wt_root, subpath, argv);
    rc->handled = YES;
    return OK;
}

//  If the projector's path lands inside a mounted sub, run it in the
//  sub shard (path-prefixed) and set `*handled`.  Otherwise leave
//  `*handled` NO and let the caller run the parent projector.  Honours
//  `--nosub` (a sub-path projector under `--nosub` stays parent-bound,
//  which is the documented opt-out — it then fails as before).
static ok64 BEProjectorRouteToMount(cli *c, uri *u, b8 *handled) {
    sane(c && u && handled);
    *handled = NO;
    if (CLIHas(c, "--nosub"))   done;
    if (u8csEmpty(u->path))     done;   //  whole-tree form → fan-out, not route
    if (!u8bHasData(c->repo))   done;
    a_dup(u8c, wt_root, $path(c->repo));

    beproj_route_ctx rc = { .c = c, .u = u, .handled = NO, .rc = OK };
    u8csMv(rc.wt_root, wt_root);
    (void)BESubsHere(wt_root, beproj_route_cb, &rc);
    *handled = rc.handled;
    return rc.rc;
}

//  View-projector routing (https://replicated.wiki/html/wiki/Projector.html §"View projectors").
//
//  Invocation: `be <scheme>:<URI>` — no verb.  `be` is a scheme
//  router; the dog that owns the scheme receives the URI verbatim
//  (scheme and all) and dispatches internally.  The scheme→dog map
//  lives in dog/DOG.c (`DOG_PROJECTORS`) — adding a projector = one
//  row there + the producing dog's internal branch.
//
//  Output: always via `dog/HUNK` — TLV to a bro pipe on TTY, plain
//  ASCII via `HUNKu8sFeedText` otherwise.  No dog needs its own
//  color / pager / renderer code.
static ok64 BEProjector(cli *c, uri *u) {
    sane(c && u);

    char const *dog_cstr = DOGProjectorDog(u->scheme);
    if (dog_cstr == NULL) {
        fprintf(stderr, "be: unknown projector '%.*s:'\n",
                (int)$len(u->scheme), (char *)u->scheme[0]);
        fail(BEFAIL);
    }
    a_cstr(dog_s, dog_cstr);

    //  SUBS-012: a path-bearing projector whose path lands inside a
    //  mounted sub resolves in the SUB shard (the parent tree holds a
    //  bare gitlink there).  Route + relay it; if handled, we're done —
    //  the parent dog would only return KEEPNONE / PROJFAIL.
    {
        b8 routed = NO;
        ok64 rr = BEProjectorRouteToMount(c, u, &routed);
        if (routed) return rr;
    }

    //  Universal three-mode rule: `HUNKMode` (set in main via
    //  `CLISetHUNKMode`) already encodes --tlv / --color / --plain /
    //  ANSIIsTTY().  Map it onto bro-pager spawn and the dog's
    //  mode-flag:
    //    COLOR → spawn bro, feed it TLV (bro renders ANSI), pass
    //            `--color` to bro so it paints even on a piped stdout.
    //    TLV   → run dog with `--tlv`, no inner bro fork.  Used by an
    //            outer bro that opens `be` itself to navigate a URI.
    //    PLAIN → run dog with no `--tlv`, dog writes plain text.
    b8 tty      = (HUNKMode == HUNKOutColor);
    b8 emit_tlv = (HUNKMode != HUNKOutPlain);

    a_path(dogpath);
    a$rg(a0, 0);
    HOMEResolveSibling(NULL, dogpath, dog_s, a0);


    //  Verbless: dog argv is `<dog> [--at <uri>] [mode-flag] <URI>`.
    //  Forward the mode explicitly so the child's CLISetHUNKMode picks
    //  the same shape `be` resolved — without an explicit flag, a child
    //  that inherits a TTY stdout would default back to Color even when
    //  `be --plain` had forced Plain.  `--at` carries the wt's tip so
    //  the projector (graf map / log "you are here", etc.) doesn't need
    //  to poke at `.be/wtlog` itself.
    a_cstr(tlv_flag,   "--tlv");
    a_cstr(plain_flag, "--plain");
    a_cstr(color_flag, "--color");
    b8 have_at = u8bDataLen(be_at_buf) > 0;
    a_pad(u8cs, dargs, 6);
    u8csbFeed1(dargs, dog_s);
    if (have_at) {
        a_dup(u8c, at_flag, be_at_flag);
        a_dup(u8c, at_val,  u8bData(be_at_buf));
        u8csbFeed1(dargs, at_flag);
        u8csbFeed1(dargs, at_val);
    }
    if      (emit_tlv)                   u8csbFeed1(dargs, tlv_flag);
    else if (HUNKMode == HUNKOutColor)   u8csbFeed1(dargs, color_flag);
    else if (HUNKMode == HUNKOutPlain)   u8csbFeed1(dargs, plain_flag);
    u8csbFeed1(dargs, u->data);
    a_dup(u8cs, dargv, u8csbData(dargs));

    if (!tty) {
        //  Run the parent projector (writes its own hunks straight to
        //  stdout in the resolved mode), then fan out into mounted subs
        //  (SUBS-011 / SUBS-012): each sub's hits are captured and
        //  re-emitted path-prefixed in the same stream.  A sub-only or
        //  whole-tree projector that recurses appends after the
        //  parent's output; a non-recursing one is a no-op fan-out.
        ok64 pr = BERun(dog_s, dargv, NO);
        //  Parent *NONE (no hits) must not suppress the sub descent —
        //  the sub may still have hits.  Keep the parent code as the
        //  floor but let a successful sub relay surface OK.
        ok64 sr = BEProjectorSubs(c, u);
        if (pr != OK && !ok64is(pr, NONE)) return pr;
        if (sr != OK) return sr;
        return pr;
    }

    //  TTY: pipe through bro.  Bro drains HUNK TLV from stdin (see
    //  bro/BRO.c §BROPipeRun) and opens /dev/tty for keystrokes.
    //  Forward `--color` so bro renders ANSI without relying on its
    //  own ANSIIsTTY (its stdout is /dev/tty here, but being explicit
    //  is one less surprise across NO_COLOR / piped-stdout edge cases).
    a_path(bropath);
    a_cstr(bro_name, "bro");
    HOMEResolveSibling(NULL, bropath, bro_name, a0);
    a_pad(u8cs, bargs, 2);
    u8csbFeed1(bargs, bro_name);
    u8csbFeed1(bargs, color_flag);
    a_dup(u8cs, bargv, u8csbData(bargs));
    //  DIFF-002: TTY fan-out folded into one bro stream.  The parent's
    //  hunks AND the sub relays (BERelaySub, re-emitted in the parent's
    //  Color HUNKMode) feed bro's single stdin, so the sub diff pages
    //  with the parent instead of dumping to the bare terminal after
    //  bro quits.  An empty parent diff with a non-empty sub diff now
    //  shows the sub diff in the pager, never "nothing!".
    ok64 pr = BERunPipeSubs($path(dogpath), dargv,
                            $path(bropath), bargv, c, u);
    if (pr != OK && !ok64is(pr, NONE)) return pr;
    return pr;
}



//  `be head <uri>` — peek/dry-run.  Per https://replicated.wiki/html/wiki/HEAD.html §"HEAD":
//    - `?br` (local)              — ahead/behind cur vs ?br
//    - `//host` (cached)          — diff cur vs cached origin tracking
//    - `ssh://host` (transport)   — fetch refs+pack, update .be/refs,
//                                    print diff cur vs origin
//    - `#frag`                    — commit-msg search; diff cur vs match
//
//  Implementation lives in the BE_PLAN_HEAD action table
//  (DISPATCH.c): keeper-get + spot+graf re-walk on an authority
//  URI, graf-head on a no-authority URI, then BEHeadSubs once at
//  the end of the table.  HEAD never modifies a branch's history
//  or the wt.

//  HEAD recursion context (SUBS.plan.md §HEAD).  Carries the
//  child-process argv built once for the whole forest walk plus the
//  outer wt root (needed to compose mount paths in BERecurseInto)
//  and a sticky `worst` exit code so the walk continues across sub
//  failures (compiler-style).  `outer_emitted` tracks whether the
//  outer-project marker has been printed yet — it fires lazily on
//  the first sub entry so single-project repos (no `.gitmodules`)
//  keep their pre-recursion stderr contract (empty on success).
typedef struct {
    u8cs  wt_root;
    u8cs *argv_head;    //  raw [head, term) over a u8css storage
    u8cs *argv_term;
    //  When the parent URI carries a transport scheme (ssh:// etc.),
    //  the recursion replaces it per-sub with that sub's own URL +
    //  the original query.  argv_head/argv_term then point at the
    //  `head` + flags prefix only; the cb appends `<sub-url>?<query>`
    //  per call.  When transport_url is NO, argv_head/argv_term cover
    //  the full child argv unchanged.
    b8    transport_mode;
    u8cs  forwarded_query;   //  parent's query slot, e.g. `master` or `*`
    b8    outer_emitted;
    ok64  worst;
} behead_recurse_ctx;

static void behead_emit_outer(behead_recurse_ctx *rc) {
    if (rc->outer_emitted) return;
    fprintf(stderr, "be: head .\n");
    rc->outer_emitted = YES;
}

static ok64 behead_recurse_cb(besub const *s, void *vctx) {
    sane(s && vctx);
    behead_recurse_ctx *rc = (behead_recurse_ctx *)vctx;

    //  Declared but not mounted on disk — report and continue.  Do
    //  not recurse (nothing to chdir into).
    if (!s->mounted) {
        behead_emit_outer(rc);
        fprintf(stderr, "be: head %.*s: declared, not mounted\n",
                (int)$len(s->path), (char *)s->path[0]);
        return OK;
    }

    behead_emit_outer(rc);
    fprintf(stderr, "be: head %.*s\n",
            (int)$len(s->path), (char *)s->path[0]);

    u8cs subpath = {};
    u8csMv(subpath, s->path);

    if (!rc->transport_mode) {
        //  Local recursion: forward parent's argv verbatim, capturing
        //  the sub's report and relaying it with the sub's path prefix
        //  so the parent's stream lists the sub's affected files too.
        u8css argv = {rc->argv_head, rc->argv_term};
        ok64 r = BERelaySub(rc->wt_root, subpath, argv);
        if (r != OK && !ok64is(r, NONE)) rc->worst = r;   //  NONE = no-op

        //  SUBS-007: the `be head ?` relay above is the committed
        //  pin-vs-trunk ahead/behind only — it is byte-identical whether
        //  or not a sub file is dirty (Submodules.mkd line 20 promises
        //  "dirty state per sub").  Also relay the sub's bare status
        //  (empty argv re-enters BEDefault → `sniff` in the mount, the
        //  same path bare `be` already uses to surface `mod core.c`), so
        //  a dirtied sub working-tree file shows up in HEAD's per-sub
        //  report.  Read-only: `sniff` status writes no wtlog rows, so
        //  the head/03 read-only invariant holds.  A clean sub returns
        //  *NONE (no-op); never an error.
        u8css status_argv = {NULL, NULL};
        ok64 sr = BERelaySub(rc->wt_root, subpath, status_argv);
        if (sr != OK && !ok64is(sr, NONE)) rc->worst = sr;   //  NONE = no-op
        return OK;
    }

    //  Transport mode: append `<sub-url>?<query>` per-sub so the
    //  child fetches the sub's own remote (not the parent's URL).
    //  Build a fresh argv = (caller's prefix: `head` + flags) +
    //  this sub's URI.
    a_pad(u8, uri_buf, MAX_URI_LEN);
    a_dup(u8c, sub_url, s->url);
    call(u8bFeed, uri_buf, sub_url);
    if (!u8csEmpty(rc->forwarded_query)) {
        call(u8bFeed1, uri_buf, '?');
        call(u8bFeed,  uri_buf, rc->forwarded_query);
    }
    a_dup(u8c, uri_view, u8bData(uri_buf));

    a_pad(u8cs, child_args, 4 + CLI_MAX_FLAGS * 2);
    for (u8cs *fp = rc->argv_head; fp < rc->argv_term; fp++) {
        u8csbFeed1(child_args, *fp);
    }
    u8csbFeed1(child_args, uri_view);
    a_dup(u8cs, child_argv, u8csbData(child_args));

    ok64 r = BERelaySub(rc->wt_root, subpath, child_argv);
    if (r != OK && !ok64is(r, NONE)) rc->worst = r;   //  NONE = no-op
    return OK;
}

//  HEAD's pre-order submodule recursion — extracted from the old
//  `BEHead` wrapper to fit the be_action signature.  Called once
//  per `be head` invocation (table marks the row as `once`).  The
//  local body (`graf head` / `keeper get` / spot+graf re-walk) is
//  driven by the preceding table rows; this function only walks
//  declared subs.  See https://replicated.wiki/html/wiki/Verbs.html / SUBS.plan.md §HEAD.
ok64 BEHeadSubs(cli *c) {
    sane(c);

    //  Local sub status is unconditional — every mounted sub is a
    //  beagle worktree, so HEAD always reports it.  Only the per-sub
    //  REMOTE ahead/behind (transport mode below) can't recurse into a
    //  git peer; that is handled by swapping each sub's own URL.
    //  `--nosub` is the explicit opt-out.
    if (CLIHas(c, "--nosub")) done;

    //  Need a wt root to enumerate / chdir from.
    if (!u8bHasData(c->repo)) done;
    a_dup(u8c, wt_root, $path(c->repo));

    //  Detect transport mode: parent invoked with `ssh://` etc.  In
    //  that case the recursion swaps the URL per-sub (each sub
    //  fetches its OWN remote) — see head/07.  Local mode
    //  (`?ref`, bare `?`, cached `//host`) just forwards verbatim.
    uri u0 = {};
    if (CLIUriLen(c) > 0) (void)CLIUriAt(&u0, c, 0);
    b8 transport = (CLIUriLen(c) > 0 && !u8csEmpty(u0.scheme));
    u8cs forwarded_query = {};
    if (transport) u8csMv(forwarded_query, u0.query);

    //  Build the child argv prefix.  Local mode: include the parent's
    //  URIs (child runs against `?<same-ref>` in its own scope).
    //  Transport mode: stop at flags — the cb appends the sub URI per
    //  call.
    a_pad(u8cs, child_args, 4 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS);
    a_cstr(head_lit, "head");
    a_dup(u8c, head_d, head_lit);
    u8csbFeed1(child_args, head_d);
    for (u32 j = 0; j + 1 < u8csbDataLen(c->flags); j += 2) {
        if ($eq((*u8csbAtP(c->flags, j)), be_at_flag)) continue;
        u8csbFeed1(child_args, (*u8csbAtP(c->flags, j)));
        if (!u8csEmpty((*u8csbAtP(c->flags, j + 1))))
            u8csbFeed1(child_args, (*u8csbAtP(c->flags, j + 1)));
    }
    if (!transport) {
        for (u32 j = 0; j < CLIUriLen(c); j++)
            u8csbFeed1(child_args, (*u8csbAtP(c->uris, j)));
    }

    a_dup(u8cs, child_argv, u8csbData(child_args));
    behead_recurse_ctx rc = {
        .argv_head      = (u8cs *)child_argv[0],
        .argv_term      = (u8cs *)child_argv[1],
        .transport_mode = transport,
        .outer_emitted  = NO,
        .worst          = OK,
    };
    u8csMv(rc.wt_root, wt_root);
    u8csMv(rc.forwarded_query, forwarded_query);

    (void)BESubsHere(wt_root, behead_recurse_cb, &rc);
    return rc.worst;
}

//  --- GET action bodies (Stage 4 migration) ------------------------
//
//  Three actions live here because they need access to BE.cli.c
//  internals: `BEGetWorktree` (rewrites first URI for secondary-wt
//  setup), the BEGet* subs helpers in beagle/SUBS.c, and a small
//  amount of cross-action state captured pre-checkout.

//  Cross-action state populated by BEActGetBaseline (runs before the
//  table mutates the wt) and consumed by BEActSubsGet (runs after).
//  Holds the 40-hex sha of the pre-get wtlog tail; empty when no
//  baseline (fresh dir, no wtlog, no row with a sha).  One static
//  per process — `be` is one-shot so no reset is needed across
//  invocations.
static u8 beget_baseline_sha_storage[64];
static Bu8 beget_baseline_sha = {
    beget_baseline_sha_storage, beget_baseline_sha_storage,
    beget_baseline_sha_storage,
    beget_baseline_sha_storage + sizeof(beget_baseline_sha_storage)
};

//  Scratch buffer for BEGetWorktree's `?<hashlet>` URI rewrite.
//  Must outlive the rest of BE_PLAN_GET because downstream rows
//  (BEActKeeperGet etc.) read `c->uris[0]` (the raw arg text), which
//  BEActWorktreeAnchor may have repointed into this buffer.
static u8 beget_wt_uri_buf_storage[64];
static Bu8 beget_wt_uri_buf = {
    beget_wt_uri_buf_storage, beget_wt_uri_buf_storage,
    beget_wt_uri_buf_storage,
    beget_wt_uri_buf_storage + sizeof(beget_wt_uri_buf_storage)
};

ok64 BEActGetBaseline(cli *c) {
    sane(c);
    u8bReset(beget_baseline_sha);
    if (!u8bHasData(c->repo)) done;
    a_pad(u8, at_buf, FILE_PATH_MAX_LEN + 128);
    if (SNIFFAtTailOf($path(c->repo), at_buf) != OK) done;
    uri bt = {};
    u8csMv(bt.data, u8bDataC(at_buf));
    URILexer(&bt);
    if (u8csLen(bt.fragment) != 40) done;
    call(u8bFeed, beget_baseline_sha, bt.fragment);
    done;
}

ok64 BEActWorktreeAnchor(cli *c) {
    sane(c);
    if (CLIUriLen(c) == 0) done;
    uri u0v = {};
    call(CLIUriAt, &u0v, c, 0);
    uri *u0 = &u0v;
    u8bReset(beget_wt_uri_buf);
    b8 attached = NO;
    call(BEGetWorktree, u0, beget_wt_uri_buf, &attached);
    //  GET-012: only a genuine attach (anchor written / branch switched)
    //  suppresses the remote-fetch rows — NOT a pre-existing `.be` at cwd
    //  that BEGetWorktree left untouched (e.g. a clone destination store).
    if (attached) c->wt_attached = YES;
    //  BEGetWorktree may have rewritten the checkout target into
    //  beget_wt_uri_buf (`?<hashlet>` / `?/<title>` etc.).  Persist the
    //  rewritten bytes back as the raw arg text so downstream rows read
    //  the new target (the buffer is static, so the slice survives).
    CLIUriSetRaw(c, 0, u0->data);

    //  GET-006: BEGetWorktree may have just wired a NEW secondary
    //  worktree at cwd (`<cwd>/.be` written as a row-0 anchor FILE)
    //  when the destination dir had no `.be` of its own.  CLIParse's
    //  earlier cwd-walk resolved `c->repo` (and, downstream, the
    //  `--at` URI in `be_at_buf`) to an ANCESTOR store — running from
    //  a subdir of someone else's worktree.  Left unfixed, the
    //  downstream `sniff get` opens that ancestor rw and appends its
    //  `get` row into the ancestor's `.be/wtlog`, poisoning it.
    //  Mirror be_sub_shard_setup's re-anchor: now that cwd owns a
    //  `.be`, re-target c->repo at cwd so the be_at_buf fill (and any
    //  later c->repo reader) sees THIS worktree, never the ancestor.
    a_path(cwd);
    call(FILEGetCwd, cwd);
    a_dup(u8c, cwd_s, u8bDataC(cwd));
    a_path(cwd_be);
    call(PATHu8bFeed, cwd_be, cwd_s);
    call(PATHu8bPush, cwd_be, DOG_BE_S);
    filestat fs = {};
    if (FILELStat(&fs, $path(cwd_be)) != OK) done;   // no cwd anchor → leave as-is
    a_dup(u8c, repo_s, u8bDataC(c->repo));
    b8 was_cwd = u8csEq(cwd_s, repo_s);
    if (!was_cwd) {
        u8bReset(c->repo);
        call(PATHu8bFeed, c->repo, cwd_s);
    }
    //  The `--at` URI was composed from the (then-ancestor) c->repo
    //  before this plan ran (BEExecute's caller fills be_at_buf at the
    //  top of the call chain).  Re-derive it from the now-cwd-anchored
    //  worktree so the downstream sniff/keeper get open THIS wt's store
    //  (the secondary anchor redirects h->root to the shared store; the
    //  `get` row lands in cwd's own `.be`, never the ancestor's wtlog).
    u8bReset(be_at_buf);
    (void)SNIFFAtTailOf(cwd_s, be_at_buf);
    done;
}

ok64 BEActSingleFileGet(cli *c) {
    sane(c);
    //  Path+query (no authority) on the first URI is a one-file
    //  overwrite — bypass spot/graf/sniff-index and route only to
    //  sniff get, which fetches the blob via keeper and rewrites
    //  the wt file without appending a `get` row.  The aggregate
    //  gate already passed; double-check the first URI to skip
    //  edge-case multi-URI invocations (rare).
    if (CLIUriLen(c) == 0)      done;
    uri u0v = {};
    call(CLIUriAt, &u0v, c, 0);
    uri *u0 = &u0v;
    if ($empty(u0->path))       done;
    if ($empty(u0->query))      done;
    if (!$empty(u0->authority)) done;

    a_cstr(sniff_s, "sniff");
    a_cstr(get_s,   "get");
    a_dup(u8c, sniff_d, sniff_s);
    a_dup(u8c, get_d,   get_s);
    a_pad(u8cs, args, 4 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS);
    BEBuildArgv(args, sniff_d, get_d, c);
    a_dup(u8cs, argv, u8csbData(args));
    call(BERun, sniff_d, argv, NO);
    return BESTOP;
}

ok64 BEActSubsGet(cli *c) {
    sane(c);

    //  `--nosub`: leave declared subs unmounted, emit marker, stop.
    if (CLIHas(c, "--nosub")) {
        fprintf(stderr, "be: submodule(s) skipped (--nosub)\n");
        done;
    }
    //  Otherwise recurse unconditionally: mounting each declared sub is
    //  the default for every source.  A sub is fetched from its OWN
    //  `.gitmodules` URL (its own remote, not the parent's), or skipped
    //  when its pin is already present in the local shard (offline-safe);
    //  recursion is never gated on the parent's source scheme.

    //  Re-resolve the wt root + active branch after the local body
    //  (sniff get) ran — a fresh-clone path may have populated
    //  `c->repo` only just now.
    a_pad(u8, post_get_branch, 256);
    {
        home rh = {};
        uri none = {};
        if (HOMEOpen(&rh, &none, NO) == OK) {
            if (!u8bHasData(c->repo))
                (void)PATHu8bFeed(c->repo, u8bDataC(rh.root));
            if (u8bDataLen(rh.cur_branch) > 0) {
                a_dup(u8c, rb, u8bDataC(rh.cur_branch));
                u8bFeed(post_get_branch, rb);
            }
        }
        HOMEClose(&rh);
    }
    if (!u8bHasData(c->repo)) done;
    a_dup(u8c, wt_root, $path(c->repo));

    //  GET-011: the in-flight `be get` SOURCE URI — the remote on the
    //  command line we are actually cloning from (`c->uris[0]->data`,
    //  forwarded verbatim downstream, see the BEGetWorktree note above).
    //  A transport source (scheme present) is the remote each sub must be
    //  fetched from; pass it down so `sniff sub-mount --source` builds the
    //  PRIMARY sub-fetch candidate against the SAME store.  Local/no-URI
    //  forms leave it empty → git's own recursion / declared URL.
    u8cs src_uri = {};
    //  SUBS-018: the requested path scope (empty = whole-tree get).  A
    //  path-scoped get recurses only into the sub(s) the path reaches.
    //  ONLY a LOCAL bareword path (`be get p.txt`) is a within-wt scope;
    //  a transport URI's path is the clone SOURCE (`file:<src>`), never a
    //  scope — so set it only when scheme AND authority are both absent.
    u8cs scope_path = {};
    if (CLIUriLen(c) > 0) {
        uri su = {};
        (void)CLIUriAt(&su, c, 0);
        if (!u8csEmpty(su.scheme)) u8csMv(src_uri, su.data);
        if (u8csEmpty(su.scheme) && u8csEmpty(su.authority))
            u8csMv(scope_path, su.path);
    }

    //  Compose target_ref from the wtlog tail (post-checkout).
    a_pad(u8, target_ref_buf, 128);
    a_pad(u8, target_at_buf, FILE_PATH_MAX_LEN + 128);
    uri tt = {};
    b8 have_tail = (SNIFFAtTailOf(wt_root, target_at_buf) == OK);
    if (have_tail) {
        u8csMv(tt.data, u8bDataC(target_at_buf));
        URILexer(&tt);
    }
    if (have_tail && u8csLen(tt.fragment) == 40) {
        if (u8bDataLen(post_get_branch) > 0) {
            (void)u8bFeed(target_ref_buf, u8bDataC(post_get_branch));
        } else if (!u8csEmpty(tt.query)) {
            (void)u8bFeed(target_ref_buf, tt.query);
        }
        (void)u8bFeed1(target_ref_buf, '#');
        (void)u8bFeed(target_ref_buf, tt.fragment);
    } else if (CLIUriLen(c) > 0) {
        uri q0 = {};
        (void)CLIUriAt(&q0, c, 0);
        if (!u8csEmpty(q0.query))
            (void)u8bFeed(target_ref_buf, q0.query);
    }
    if (u8bDataLen(target_ref_buf) == 0) done;
    u8cs target_ref = {};
    u8csMv(target_ref, u8bDataC(target_ref_buf));

    //  Enumerate target's subs.
    Bu8 target_subs = {};
    call(u8bAllocate, target_subs, 1UL << 16);
    ok64 ke = BEGetKeeperSubs(target_ref, target_subs);
    if (ke != OK) {
        u8bFree(target_subs);
        done;
    }

    //  Enumerate baseline's subs (best-effort) when baseline_sha was
    //  captured and differs from target's fragment.  Baseline_ref
    //  shape mirrors target: `<branch>#<sha>` if we know the
    //  branch, else `#<sha>`.
    Bu8 baseline_subs = {};
    call(u8bAllocate, baseline_subs, 1UL << 16);
    if (u8bDataLen(beget_baseline_sha) == 40) {
        a_pad(u8, baseline_ref_buf, 128);
        if (u8bDataLen(post_get_branch) > 0) {
            (void)u8bFeed(baseline_ref_buf,
                          u8bDataC(post_get_branch));
        }
        (void)u8bFeed1(baseline_ref_buf, '#');
        (void)u8bFeed(baseline_ref_buf,
                      u8bDataC(beget_baseline_sha));
        a_dup(u8c, baseline_ref, u8bDataC(baseline_ref_buf));
        if (!u8csEq(baseline_ref, target_ref)) {
            (void)BEGetKeeperSubs(baseline_ref, baseline_subs);
        }
    }

    //  Flags tail (sans --at).
    a_pad(u8cs, flags_buf, CLI_MAX_FLAGS * 2);
    for (u32 j = 0; j + 1 < u8csbDataLen(c->flags); j += 2) {
        if ($eq((*u8csbAtP(c->flags, j)), be_at_flag)) continue;
        u8csbFeed1(flags_buf, (*u8csbAtP(c->flags, j)));
        if (!u8csEmpty((*u8csbAtP(c->flags, j + 1))))
            u8csbFeed1(flags_buf, (*u8csbAtP(c->flags, j + 1)));
    }
    a_dup(u8cs, flag_view, u8csbData(flags_buf));

    ok64 worst = OK;

    //  Unmount removed subs first (rename case lays cleanly).
    if (u8bDataLen(baseline_subs) > 0 && u8bDataLen(target_subs) > 0) {
        a_dup(u8c, base_view, u8bData(baseline_subs));
        a_dup(u8c, tgt_view,  u8bData(target_subs));
        ok64 rr = BEGetDrainRemoved(wt_root, base_view, tgt_view);
        if (rr != OK) worst = rr;
    } else if (u8bDataLen(baseline_subs) > 0 &&
               u8bDataLen(target_subs) == 0) {
        u8cs empty = {NULL, NULL};
        a_dup(u8c, base_view, u8bData(baseline_subs));
        ok64 rr = BEGetDrainRemoved(wt_root, base_view, empty);
        if (rr != OK) worst = rr;
    }

    //  Mount/recurse target's subs.
    if (u8bDataLen(target_subs) > 0) {
        a_dup(u8c, tgt_view, u8bData(target_subs));
        ok64 sub_worst = BEGetDrainSubs(wt_root, tgt_view,
                                          (u8cs *)flag_view[0],
                                          (u8cs *)flag_view[1], src_uri,
                                          scope_path);
        if (sub_worst != OK) worst = sub_worst;
    }

    u8bFree(target_subs);
    u8bFree(baseline_subs);
    return worst;
}

//  PATCH submodule recursion (https://replicated.wiki/html/wiki/Verbs.html / Submodules.mkd): gitlink-diff
//  driven.  A patch that absorbs `?branch` may move a sub's `160000`
//  pin; where the source (applied) pin differs from cur's base pin, the
//  sub is checked out at the new pin and its report relayed.  We reuse
//  GET's subs machinery: enumerate the source branch's subs (target)
//  and cur's subs (baseline), unmount removed, then mount + relay-`get`
//  each target sub.  Unchanged subs are safe — GET no-ops a clean sub
//  already at its pin and refuses a dirty one, so only moved pins
//  actually update.  Path-scoped / transport patches don't recurse
//  (not a whole-tree absorption).
ok64 BEActSubsPatch(cli *c) {
    sane(c);
    if (CLIHas(c, "--nosub")) done;
    //  Local absorb is unconditional — patching a mounted sub at a
    //  moved pin is local work, never gated on the parent's source.
    if (!u8bHasData(c->repo)) done;
    a_dup(u8c, wt_root, $path(c->repo));

    if (CLIUriLen(c) == 0) done;
    uri uv = {};
    call(CLIUriAt, &uv, c, 0);
    uri *u = &uv;
    if (!u8csEmpty(u->scheme)) done;    //  transport — fetch+absorb, no recurse
    if (!u8csEmpty(u->path))   done;    //  path-scoped — partial, no recurse
    if (u8csEmpty(u->query))   done;    //  no source branch — nothing to diff

    //  Base ref = cur's tip (the pre-patch gitlink pins); the tail also
    //  yields cur's branch, against which a relative source query is
    //  absolutised below.
    a_pad(u8, base_at_buf, FILE_PATH_MAX_LEN + 128);
    u8cs base_ref = {};
    u8cs cur_branch = {};
    if (SNIFFAtTailOf(wt_root, base_at_buf) == OK) {
        uri tt = {};
        u8csMv(tt.data, u8bDataC(base_at_buf));
        URILexer(&tt);
        if (u8csLen(tt.fragment) == 40) u8csMv(base_ref, tt.fragment);
        u8csMv(cur_branch, tt.query);
    }

    //  Source (applied) ref = the patched branch.  SUBS-002: a relative
    //  query (`?./feat`, `?../sib`, `?..`) is meaningless to `keeper subs`
    //  (it has no cur-branch context and REFSResolve rejects the `./`
    //  prefix → REFSNONE).  Absolutise it against cur's branch first, the
    //  same way sniff PATCH resolves its own target.
    u8cs source_ref = {};
    u8csMv(source_ref, u->query);
    a_path(src_qbuf);
    {
        b8 was_rel = NO;
        if (DPATHBranchResolveRel(src_qbuf, cur_branch, u->query,
                                  &was_rel) == OK && was_rel)
            u8csMv(source_ref, $path(src_qbuf));
    }

    //  Enumerate source (target) and base subs.
    Bu8 target_subs = {};
    call(u8bAllocate, target_subs, 1UL << 16);
    ok64 ke = BEGetKeeperSubs(source_ref, target_subs);
    if (ke != OK) { u8bFree(target_subs); done; }

    Bu8 baseline_subs = {};
    call(u8bAllocate, baseline_subs, 1UL << 16);
    if (!u8csEmpty(base_ref))
        (void)BEGetKeeperSubs(base_ref, baseline_subs);

    //  Flags tail (sans --at) to forward to each sub `get`.
    a_pad(u8cs, flags_buf, CLI_MAX_FLAGS * 2);
    for (u32 j = 0; j + 1 < u8csbDataLen(c->flags); j += 2) {
        if ($eq((*u8csbAtP(c->flags, j)), be_at_flag)) continue;
        u8csbFeed1(flags_buf, (*u8csbAtP(c->flags, j)));
        if (!u8csEmpty((*u8csbAtP(c->flags, j + 1))))
            u8csbFeed1(flags_buf, (*u8csbAtP(c->flags, j + 1)));
    }
    a_dup(u8cs, flag_view, u8csbData(flags_buf));

    ok64 worst = OK;
    if (u8bDataLen(baseline_subs) > 0 && u8bDataLen(target_subs) > 0) {
        a_dup(u8c, base_view, u8bData(baseline_subs));
        a_dup(u8c, tgt_view,  u8bData(target_subs));
        ok64 rr = BEGetDrainRemoved(wt_root, base_view, tgt_view);
        if (rr != OK) worst = rr;
    }
    if (u8bDataLen(target_subs) > 0) {
        a_dup(u8c, tgt_view, u8bData(target_subs));
        //  PATCH has no in-flight clone source — the sub's gitlink moves
        //  via a local tree-diff absorb, not a remote fetch.  Empty
        //  src_uri → sniff keeps the declared `.gitmodules` URL.
        u8cs no_src = {NULL, NULL};
        //  PATCH only reaches here for a whole-tree patch (the path-scoped
        //  form returned earlier), so no path scope — recurse every sub.
        u8cs no_scope = {NULL, NULL};
        ok64 sr = BEGetDrainSubs(wt_root, tgt_view,
                                 (u8cs *)flag_view[0], (u8cs *)flag_view[1],
                                 no_src, no_scope);
        if (sr != OK) worst = sr;
    }

    u8bFree(target_subs);
    u8bFree(baseline_subs);
    return worst;
}

//  Fork spot + graf in parallel against the worktree's current tip
//  (via `--at`).  Used by verbs that move a ref locally — post,
//  patch — so the user-facing indexes stay current without a manual
//  `be get` step.  `be_at_buf` is refreshed from `.be/wtlog` first
//  (the calling verb may have just moved the tip).
static ok64 be_reindex(cli *c) {
    sane(c);
    //  Re-derive the wt root: a post that just bootstrapped a fresh
    //  `.be/` (`be post` in an empty dir) leaves `c->repo` empty
    //  because CLIParse's cwd-walk happened before the post created
    //  the store.  Walk again here so SNIFFAtTailOf can read `.be/wtlog`.
    u8cs repo = {};
    if (u8bHasData(c->repo)) {
        u8csMv(repo, $path(c->repo));
    } else {
        home rh = {};
        uri none = {};
        if (HOMEOpen(&rh, &none, NO) == OK) {
            //  HOMEOpen owns its `rh` storage; feed it into the cli's
            //  path8b (NUL-terminated by construction) for re-export.
            (void)PATHu8bFeed(c->repo, u8bDataC(rh.root));
            if (u8bHasData(c->repo)) u8csMv(repo, $path(c->repo));
        }
        HOMEClose(&rh);
    }
    if (!u8csEmpty(repo)) {
        u8bReset(be_at_buf);
        (void)SNIFFAtTailOf(repo, be_at_buf);
    }
    static u8c const spot_lit[] = "spot";
    static u8c const graf_lit[] = "graf";
    u8cs const dogs[2] = {
        {spot_lit, spot_lit + 4},
        {graf_lit, graf_lit + 4},
    };
    a_cstr(get_s, "get");

    pid_t pids[2] = {0};
    ok64  spawn_err[2] = {OK, OK};
    for (int i = 0; i < 2; i++) {
        a_pad(u8cs, args, 4);
        a_dup(u8c, dog_d, dogs[i]);
        a_dup(u8c, get_d, get_s);
        u8csbFeed1(args, dog_d);
        u8csbFeed1(args, get_d);
        if (u8bDataLen(be_at_buf) > 0) {
            a_dup(u8c, at_flag, be_at_flag);
            a_dup(u8c, at_val,  u8bData(be_at_buf));
            u8csbFeed1(args, at_flag);
            u8csbFeed1(args, at_val);
        }
        a_dup(u8cs, argv, u8csbData(args));
        spawn_err[i] = BESpawn(dog_d, argv, &pids[i]);
    }
    ok64 worst = OK;
    for (int i = 0; i < 2; i++) {
        if (spawn_err[i] != OK) { worst = spawn_err[i]; continue; }
        a_dup(u8c, dog_d, dogs[i]);
        ok64 r = BEReap(pids[i], dog_d);
        if (r != OK) worst = r;
    }
    return worst;
}

//  Derive the project segment from a URI.  Currently only the
//  explicit absolute-query form is honored:
//    * `?/<project>/...` — project is the first path segment after
//      the leading `/`.  Works for any scheme (`be://host?/proj/main`,
//      `file:///path?/proj/main`, bare `?/proj/main`).
//  URL-basename derivation (e.g. inferring "beagle" from
//  `https://github.com/gritzko/beagle.git`) is intentionally deferred
//  until the test suite is migrated to expect the project-sharded
//  on-disk layout for fresh clones — switching the default today
//  would silently flip the layout for every file:// clone in the
//  suite (test/get/08, test/post/04, etc.).
//
//  Feeds the project bytes into `out` (a slice borrowing from the
//  URI; no allocation).  Returns SUBSPARSE when the URI doesn't
//  carry an absolute-query project.
static ok64 be_url_project(uricp u, u8csp out) {
    if (!u) return SUBSPARSE;
    //  Canonical extractor (dog/DOG.h): `?/<title>[/<branch>]` → title.
    a_dup(u8c, q, u->query);
    u8cs proj_slice = {};
    DOGQueryProject(q, proj_slice);
    if (u8csEmpty(proj_slice)) return SUBSPARSE;
    u8csMv(out, proj_slice);
    return OK;
}

//  Lay down `<cwd>/.be/<project>/{refs,wtlog}` plus a `<cwd>/.be/wtlog`
//  row-0 `repo file:<cwd>/.be/<project>/` anchor that pins this wt to
//  the project shard.  Project name derivation, in order:
//    1. `?/<proj>/...` query slot on `u` (explicit URI form).
//    2. URL basename via `SNIFFSubBasename` (clone-URL form).
//    3. `basename($PWD)` (fresh-init in an empty dir with no URL).
//  Skipped iff an anchor is already present in the wt (`h->project`
//  populated after HOMEOpen) — established projects aren't
//  re-initialised; the existing anchor wins.  Refuses with `BEFAIL`
//  when every derivation arm yields empty (e.g. `cwd == /`).
ok64 BEEnsureProjectRepo(uri *u) {
    sane(1);
    //  Probe for an existing anchor.  An empty `h->project` after
    //  HOMEOpen means either no `.be/` exists in any parent OR a
    //  `.be/` exists but its wtlog carries no row-0 `repo` anchor
    //  (legacy uninitialised shape).  Either way fall through to
    //  fresh-init.
    {
        home probe = {};
        uri none = {};
        ok64 ho = HOMEOpen(&probe, &none, NO);
        b8 anchored = (ho == OK && u8bDataLen(probe.project) > 0);
        HOMEClose(&probe);
        if (anchored) done;
    }

    a_path(cwd);
    call(FILEGetCwd, cwd);
    a_dup(u8c, cwd_s, u8bDataC(cwd));

    //  Secondary worktree: `<cwd>/.be` is a regular FILE (the row-0
    //  `repo` anchor), not a store dir.  Its store lives at the primary
    //  it points to, which may use the legacy elided layout (empty
    //  project) — so the anchored-gate above doesn't fire.  A project
    //  shard must never be created here: `mkdir <cwd>/.be/<proj>` would
    //  ENOTDIR against the wtlog file.  BEGetWorktree already wired the
    //  anchor; there is nothing to init.
    {
        a_path(cwd_be);
        call(PATHu8bFeed, cwd_be, cwd_s);
        call(PATHu8bPush, cwd_be, DOG_BE_S);
        filestat fs = {};
        if (FILELStat(&fs, $path(cwd_be)) == OK
            && fs.kind == FILE_KIND_REG)
            done;
    }

    //  Project name derivation chain.
    u8cs proj = {};
    //  (1) Explicit `?/<proj>/...` query slot.
    if (u) (void)be_url_project(u, proj);
    //  (2) URL basename via SNIFFSubBasename — only for URL-shaped
    //      URIs (have scheme or authority).  Local file path args
    //      like `be put a.txt` would otherwise anchor the store at
    //      `.be/a.txt/`, breaking subsequent `keeper subs` /
    //      project-relative lookups.
    if (u8csEmpty(proj) && u &&
        (!u8csEmpty(u->scheme) || !u8csEmpty(u->authority))) {
        u8cs candidate = {NULL, NULL};
        if (!u8csEmpty(u->data))      u8csMv(candidate, u->data);
        else if (!u8csEmpty(u->path)) u8csMv(candidate, u->path);
        if (!u8csEmpty(candidate))
            (void)SNIFFSubBasename(candidate, proj);
    }
    //  (3) `basename($PWD)` — fresh-init in a named directory.
    if (u8csEmpty(proj)) PATHu8sBase(proj, cwd_s);

    if (u8csEmpty(proj)) {
        fprintf(stderr, "be: cannot derive project name; "
                "pass `be put ?/<project>` or run from a named "
                "directory\n");
        return BEFAIL;
    }
    //  GET-004: never let a derived project be literally `.be` — that
    //  would mint a `<cwd>/.be/.be` doubled-store shard.  `.be` is the
    //  store dir name, never a project.
    {
        a_cstr(be_nm, DOG_BE_NAME);
        if (u8csEq(proj, be_nm)) {
            fprintf(stderr, "be: refusing `.be` as a project name\n");
            return BEFAIL;
        }
    }

    //  Refuse if the shard already exists on disk: an explicit
    //  `?/<proj>` URI must not silently clobber a half-initialised
    //  shard.  (Fully-initialised anchored case is handled by the
    //  anchored-gate above as a no-op.)
    a_path(shard_dir);
    call(PATHu8bFeed, shard_dir, cwd_s);
    call(PATHu8bPush, shard_dir, DOG_BE_S);
    call(PATHu8bPush, shard_dir, proj);
    {
        filestat fs = {};
        if (FILEStat(&fs, $path(shard_dir)) == OK) {
            fprintf(stderr,
                "be: project shard already exists at .be/%.*s\n",
                (int)$len(proj), (char *)proj[0]);
            return BEPRJDUP;
        }
    }

    //  Storage skeleton (mkdir + empty refs/wtlog inside the shard)
    //  belongs to keeper; row-0 anchor file at `<cwd>/.be/wtlog`
    //  belongs to sniff.  BE only orchestrates.
    a_path(store_root);
    call(PATHu8bFeed, store_root, cwd_s);
    call(PATHu8bPush, store_root, DOG_BE_S);
    call(KEEPInitShard, u8bDataC(store_root), proj);

    //  Remote shard alongside the project shard when the seed URI
    //  carries a host (`be get ssh://h/p?...` etc.).  https://replicated.wiki/html/wiki/Store.html §"Repo
    //  dir layout": `<store>/<project>/remotes/<host>/refs` lives in
    //  a `remotes/` class dir parallel to branch dirs.  Cache-only —
    //  no wtlog seed.  Skipped silently when the URI is local-only.
    if (u && !u8csEmpty(u->host)) {
        a_dup(u8c, host_s, u->host);
        call(KEEPInitRemoteShard,
             u8bDataC(store_root), proj, host_s);
    }

    a_path(repo_path);
    call(PATHu8bFeed, repo_path, u8bDataC(shard_dir));
    call(u8bFeed1,    repo_path, '/');
    a_path(wtlog_path);
    call(PATHu8bFeed, wtlog_path, u8bDataC(store_root));
    call(PATHu8bPush, wtlog_path, DOG_WTLOG_S);
    //  Bootstrap anchor is tip-less here (clone has not fetched yet);
    //  the combined `?/<title>/<branch>#<hash>` row is written after
    //  the keeper get resolves the tip (step 5, DIS-001).
    u8cs empty_q = {};
    call(SNIFFWtRepoAnchor, u8bDataC(wtlog_path), u8bDataC(repo_path),
         empty_q, empty_q, empty_q);

    done;
}

//  YES iff the URI names a remote the wt ALREADY knows — a transport
//  row matching its host (or, for a host-less `file:` URL, its path)
//  exists in the project's REFS.  A known remote means `be get` from a
//  subdir targets the WHOLE wt (a normal fetch), not a new submodule
//  at cwd; only an UNKNOWN remote in a subdir mounts as a submodule.
typedef struct { u8cs host; u8cs path; b8 known; } be_known_ctx;

static ok64 be_known_cb(uri const *r, ron60 ts, ron60 verb, void *vc) {
    (void)ts; (void)verb;
    be_known_ctx *k = (be_known_ctx *)vc;
    u8cs rhost = {r->host[0],   r->host[1]};
    u8cs rsch  = {r->scheme[0], r->scheme[1]};
    u8cs rpath = {r->path[0],   r->path[1]};
    //  Only transport rows (scheme or host present) name a remote.
    if (u8csEmpty(rsch) && u8csEmpty(rhost)) return OK;
    //  Same remote ⇒ BOTH host and path match.  Same host but a
    //  DIFFERENT path is a different repo (e.g. parent.git vs sub.git
    //  on one server) → NOT known, so it mounts as a submodule.
    //  Host-less transports (`file:`) match on path alone.
    b8 host_ok = u8csEmpty(k->host) ? u8csEmpty(rhost)
                                    : u8csEq(rhost, k->host);
    b8 path_ok = u8csEmpty(k->path) ? u8csEmpty(rpath)
                                    : (!u8csEmpty(rpath) && u8csEq(rpath, k->path));
    if (host_ok && path_ok) { k->known = YES; return REFSSTOP; }
    return OK;
}

static b8 be_remote_is_known(uri *u, home *rh) {
    a_path(keepdir);
    if (HOMEBranchDir(rh, keepdir, NULL) != OK) return NO;
    be_known_ctx ctx = {};
    ctx.host[0] = u->host[0]; ctx.host[1] = u->host[1];
    ctx.path[0] = u->path[0]; ctx.path[1] = u->path[1];
    (void)REFSEachRecord($path(keepdir), be_known_cb, &ctx);
    return ctx.known;
}

//  Subdir-of-existing-repo + remote clone = treat cwd as a submodule
//  worktree of a fresh shard under the ancestor's `.be/`.
//
//  Layout (flat: `<parent>/.be/<basename>/` IS the sub's store, with
//  the same file shape as the parent's trunk `<parent>/.be/`):
//
//      <parent_root>/.be/                          parent's trunk store
//      <parent_root>/.be/<basename>/               sub's store (branch dir)
//          ├── refs                                (empty seed)
//          └── wtlog                               (empty seed)
//      <cwd>/.be                                   regular FILE anchor
//          └── <ts>\trepo\tfile:<parent>/.be/<basename>/\n
//
//  Walk-up from any path inside cwd then resolves to the sub's store
//  (via the anchor file's row-0), so keeper / sniff / graf / spot all
//  open the shard cleanly without colliding with the parent's keeper.
//
//  After setup we rewrite c->repo to cwd so the be_at_buf fill below
//  reads the freshly-minted (empty) shard wtlog, not the parent's.
static ok64 be_sub_shard_setup(cli *c, uri *u) {
    sane(c && u);

    //  Only fires when CLIParse walked up to an ancestor (c->repo
    //  non-empty) and cwd is a strict subdir of it.  Pure remote
    //  clones only — `?ref` / `#sha` / projector forms are handled
    //  by the normal in-repo path.
    if (!u8bHasData(c->repo)) done;
    if (u8csEmpty(u->authority)) done;

    a_path(cwd);
    call(FILEGetCwd, cwd);
    a_dup(u8c, cwd_s,  u8bDataC(cwd));
    a_dup(u8c, repo_s, u8bDataC(c->repo));
    if (u8csEq(cwd_s, repo_s)) done;             // not a subdir
    if (u8csLen(cwd_s) <= u8csLen(repo_s)) done;
    if (!u8csHasPrefix(cwd_s, repo_s)) done;
    //  Boundary check: cwd must continue into repo with a '/' so a
    //  cousin named `<repo>x/…` doesn't masquerade as a subdir.
    if (cwd_s[0][u8csLen(repo_s)] != '/') done;

    //  Refuse to clobber an existing `<cwd>/.be` — either anchor file
    //  or store dir.  A stale one means the user already mounted here
    //  and a fresh shard would orphan the previous state.
    a_path(cwd_be);
    call(PATHu8bFeed, cwd_be, cwd_s);
    call(PATHu8bPush, cwd_be, DOG_BE_S);
    {
        filestat fs = {};
        if (FILELStat(&fs, $path(cwd_be)) == OK) done;
    }

    //  A KNOWN remote (already recorded in the wt's REFS) targets the
    //  WHOLE wt — `be get <known-remote>` from a subdir is a normal
    //  fetch, not a new submodule.  Only an UNKNOWN remote mounts here.
    {
        home rh = {};
        uri none = {};
        b8 known = NO;
        if (HOMEOpen(&rh, &none, NO) == OK)
            known = be_remote_is_known(u, &rh);
        HOMEClose(&rh);
        if (known) done;
    }

    //  Derive shard name from the URL basename — same rule the
    //  .gitmodules-driven sub-mount uses (sniff/SUBS.c).
    a_dup(u8c, url_d, u->data);
    u8cs basename = {};
    call(SNIFFSubBasename, url_d, basename);

    //  Resolve the actual STORE root via HOMEOpen.  c->repo holds the
    //  worktree path (h->wt); for a wt that's already a secondary
    //  (its `.be` is a file anchor redirecting to another store) the
    //  new shard must land at `<store>/<basename>/`, not under the
    //  secondary's wt dir.  HOMEOpen follows the row-0 anchor and
    //  fills h->root with the redirected store root; we use that.
    //  If HOMEOpen fails (e.g. the anchor URI is corrupted) we fall
    //  back to the wt dir's own `.be/` — the legacy behavior.
    //
    //  GET-004: compose BOTH `<store>/.be[/seg]` paths through the single
    //  HOME composer (handles *.be-is-store + drops a `.be` basename)
    //  while rh is open, then close rh in this same block so no early
    //  return between open and close can leak the home.  rh stays rw=NO
    //  so HOMEBeDir is pure compose — the sub shard is mkdir'd explicitly
    //  below (it must always exist).  `shard_be` / `sub_store` outlive the
    //  block; `store_root_buf` carries the resolved root for the logging.
    a_path(store_root_buf);
    a_path(shard_be);
    a_path(sub_store);
    b8 sub_store_set = NO;
    {
        home rh = {};
        uri none = {};
        b8 have_home = (HOMEOpen(&rh, &none, NO) == OK && u8bHasData(rh.root));
        ok64 ro = OK;
        if (have_home) {
            a_dup(u8c, rs, u8bDataC(rh.root));
            ro = PATHu8bFeed(store_root_buf, rs);
            if (ro == OK) ro = HOMEBeDir(&rh, basename, shard_be);
            if (ro == OK && !u8csEmpty(u->host)) {
                u8cs noseg = {};
                ro = HOMEBeDir(&rh, noseg, sub_store);
                if (ro == OK) sub_store_set = YES;
            }
        } else {
            ro = PATHu8bFeed(store_root_buf, repo_s);
        }
        HOMEClose(&rh);
        if (ro != OK) fail(ro);
    }
    a_dup(u8c, store_root_s, u8bDataC(store_root_buf));

    //  mkdir <store>/.be/<basename>/  (flat: this dir IS the sub's
    //  store, same shape as <store>/.be/ itself).  Fall back to the
    //  guarded manual compose only when the anchor was unreadable (no
    //  home) — HOMEBeDir already ran above when a home was available.
    if (u8bEmpty(shard_be)) {
        call(PATHu8bFeed, shard_be, store_root_s);
        a_cstr(be_suf, DOG_BE_NAME);
        if (!u8csHasSuffix(store_root_s, be_suf))
            call(PATHu8bPush, shard_be, DOG_BE_S);
        a_dup(u8c, base_s, basename);
        call(PATHu8bPush, shard_be, base_s);
    }
    call(FILEMakeDirP, $path(shard_be));

    //  Seed empty refs + wtlog so HOME walk-up finds a well-formed
    //  store on first open.
    {
        a_path(p);
        a_dup(u8c, s, u8bDataC(shard_be));
        call(PATHu8bFeed, p, s);
        call(PATHu8bPush, p, DOG_REFS_S);
        int fd = -1;
        call(FILECreate, &fd, $path(p));
        call(FILEClose, &fd);
    }
    {
        a_path(p);
        a_dup(u8c, s, u8bDataC(shard_be));
        call(PATHu8bFeed, p, s);
        call(PATHu8bPush, p, DOG_WTLOG_S);
        int fd = -1;
        call(FILECreate, &fd, $path(p));
        call(FILEClose, &fd);
    }

    //  Per-host remote shard parallel to the sub's branch dirs.  Sub
    //  bootstrap only fires for remote-bearing URIs (the early-out at
    //  the top of this function gates on `u->authority`), so `u->host`
    //  is almost always populated — but stay defensive.
    if (!u8csEmpty(u->host)) {
        if (!sub_store_set) {
            call(PATHu8bFeed, sub_store, store_root_s);
            call(PATHu8bPush, sub_store, DOG_BE_S);
        }
        a_dup(u8c, host_s, u->host);
        call(KEEPInitRemoteShard,
             u8bDataC(sub_store), basename, host_s);
    }

    //  Compose row-0 URI: `file:<parent>/.be/<basename>/`.  Routed
    //  through URIutf8Feed so the bytes match `sniff_write_repo_row`'s
    //  output shape (single slash after `file:`, trailing slash on path).
    a_path(uri_path);
    a_dup(u8c, sr_s, u8bDataC(shard_be));
    call(PATHu8bFeed, uri_path, sr_s);
    call(u8bFeed1, uri_path, '/');
    call(PATHu8bTerm, uri_path);

    uri urow = {};
    a_cstr(scheme_s, "file");
    urow.scheme[0] = scheme_s[0];
    urow.scheme[1] = scheme_s[1];
    {
        a_dup(u8c, p, u8bData(uri_path));
        urow.path[0] = p[0];
        urow.path[1] = p[1];
    }

    a_pad(u8, row, 1024);
    ron60 ts = RONNow();
    call(RONutf8sFeed, u8bIdle(row), ts);
    call(u8bFeed1, row, '\t');
    a_cstr(repo_verb, "repo");
    call(u8bFeed, row, repo_verb);
    call(u8bFeed1, row, '\t');
    call(URIutf8Feed, u8bIdle(row), &urow);
    call(u8bFeed1, row, '\n');

    int fd = -1;
    call(FILECreate, &fd, $path(cwd_be));
    a_dup(u8c, body, u8bData(row));
    call(FILEFeedAll, fd, body);
    FILEClose(&fd);

    //  Re-anchor c->repo at cwd so the be_at_buf fill (and any later
    //  c->repo readers) see the sub, not the parent.
    u8bReset(c->repo);
    call(PATHu8bFeed, c->repo, cwd_s);

    fprintf(stderr, "be: subdir clone — shard at %.*s/.be/%.*s\n",
            (int)$len(store_root_s), (char *)store_root_s[0],
            (int)$len(basename),     (char *)basename[0]);
    done;
}

//  PUT / DELETE bodies live in DISPATCH.c (BE_PLAN_PUT, BE_PLAN_DELETE).
//  PUT is the ref-writer (https://replicated.wiki/html/wiki/PUT.html §"PUT"); per the URI's `//remote`
//  slot it doubles as the FF-push verb (keeper's old `post` entry).
//  Local shapes (label move, file staging, sha reset) stay in sniff
//  put.  DELETE is its mirror.

//  Ref-expecting verbs (post, patch) accept a path-shaped argument as
//  the ref — `be get feat/sub` → query="feat/sub".  Bareword promotion
//  is centralised in DOGPromoteBareword (per https://replicated.wiki/html/wiki/Verbs.html §"Bareword
//  defaults"); this is the safety net for paths that URILexer already
//  parked in `u->path`.  Skip when the path looks like a filesystem
//  path (leading `/`) or rides a non-empty scheme (`file:`, `ssh:`,
//  etc.) — both shapes go to keeper / sniff as paths, not refs.
//  Returns YES if anything moved.
b8 BEPromoteRef(uri *u) {
    if (!$empty(u->query)) return NO;
    if ($empty(u->path)) return NO;
    if (!$empty(u->scheme)) return NO;
    if (u->path[0][0] == '/') return NO;
    u8cs s = {u->path[0], u->path[1]};
    u8csMv(u->query, s);
    u->path[0] = u->path[1] = NULL;
    return YES;
}

//  PATCH body lives in DISPATCH.c (BE_PLAN_PATCH).  3-way merge
//  flow: promote_ref → bootstrap (local) → keeper get (transport
//  scheme) → graf get (any remote) → sniff patch.  See https://replicated.wiki/html/wiki/Verbs.html
//  §PATCH for semantics.

//  `be post` — commit and/or fast-forward (never rebase; see https://replicated.wiki/html/wiki/Verbs.html
//  §POST).  Rebase is `be patch ?br#` + `be post`, looped.
//    <free-form tail> → fragment carries the commit message; sniff
//                       commits on cur.  (Legacy `-m <msg>` flag still
//                       works as a fallback.)
//    ?<branch>        → FF-advance ?branch to cur's (post-commit) tip;
//                       refused (POSTNOFF) if cur isn't a descendant.
//    //<host>[?<ref>] → keeper FF-pushes cur's tip to the remote's
//                       counterpart; refused if not a fast-forward.
//    bare             → sniff prints the dry-run change-set; no commit.
//
//  Path-form URIs (`be post abc/foo`, `be post .`, `be post x.txt`)
//  are spec-illegal: POST takes only branch URIs.  Refuse early with a
//  hint to use `be put` first.
//
//  Detection: the token is path-form when its raw bytes (u->data) carry
//  no `?` sigil AND either contain a `/` or `.` OR have an explicit
//  uri.path slice (URILexer-classified path).  This catches both
//  `be post abc/foo` (uri.path) and `be post x.txt` (DOGNormalizeArg
//  classified as ref_safe-bare-token, query-only no path).  Pure-letter
//  branch tokens (`?feat`, `feat`) and remote URIs (`//host?ref`) skip
//  the check.
static b8 be_post_is_path_form(uri *u) {
    if (!$empty(u->authority)) return NO;
    if (!$empty(u->path)) return YES;
    //  Bare token classified as query-only: refuse if it looks like a
    //  filesystem path (contains `.` or `/` and is not preceded by `?`
    //  in the raw bytes).
    u8cs raw = {u->data[0], u->data[1]};
    if ($empty(raw)) return NO;
    if (raw[0][0] == '?') return NO;
    $for(u8c, p, raw) {
        if (*p == '/' || *p == '.') return YES;
    }
    return NO;
}

//  POST action: refuse path-form URIs early with a hint to use `be
//  put` first.  Pure-fragment URIs (commit-message tokens) are
//  skipped — they're message-only and have no path.
ok64 BEActPathFormCheck(cli *c) {
    sane(c);
    for (u32 i = 0; i < CLIUriLen(c); i++) {
        uri uv = {};
        call(CLIUriAt, &uv, c, i);
        uri *u = &uv;
        //  Pure-fragment URIs are message-only; skip the check.
        if ($empty(u->path) && $empty(u->query) && $empty(u->authority) &&
            !$empty(u->fragment)) continue;
        if (be_post_is_path_form(u)) {
            fprintf(stderr,
                "be: post: path-form URI `%.*s` not allowed — use "
                "`be put %.*s` first, then `be post <msg>`\n",
                (int)u8csLen(u->data), (char *)u->data[0],
                (int)u8csLen(u->data), (char *)u->data[0]);
            fail(BEFAIL);
        }
    }
    done;
}


//  Spawn `be put <subpath>` in the parent wt to stage a `put
//  <subpath>#<40-hex>` row reflecting the sub's current tip
//  (SNIFFSubReadTip).  Used by BEActSubsPost after a sub's recursive
//  POST returns OK *and* the parent has commit intent: BEActSniffPost
//  then reads that row and emits the gitlink ADD in the parent
//  commit.  Gated on parent_msg in the caller (Bug 4): pure-push
//  shapes (`be post //host` no #frag) skip the bump so the parent
//  isn't tricked into a selective-mode commit on cur.
static ok64 bepost_bump_sub(u8cs subpath) {
    sane($ok(subpath));

    //  Resolve self so the child's argv[0] is the full self path —
    //  HOMEResolveSibling needs that to find the sibling `sniff`,
    //  `keeper`, etc.  argv[0] + PATH is portable across Linux and
    //  FreeBSD; /proc/self/exe is Linux-only.
    a_path(self_path);
    {
        a$rg(a0, 0);
        a_cstr(self_name, "be");
        (void)HOMEResolveSibling(NULL, self_path, self_name, a0);
        if (!u8bHasData(self_path)) {
            fprintf(stderr, "be: post: bump %.*s: cannot resolve self\n",
                    (int)$len(subpath), (char *)subpath[0]);
            return BEDOGEXIT;
        }
    }

    a_pad(u8cs, args, 4);
    a_dup(u8c, self_view, u8bDataC(self_path));
    a_cstr(put_s, "put");
    a_dup(u8c, put_d, put_s);
    a_dup(u8c, sub_d, subpath);
    u8csbFeed1(args, self_view);
    u8csbFeed1(args, put_d);
    u8csbFeed1(args, sub_d);
    a_dup(u8cs, argv, u8csbData(args));

    pid_t pid = 0;
    ok64 sp = FILESpawn($path(self_path), argv, NULL, NULL, &pid);
    if (sp != OK) {
        fprintf(stderr, "be: post: bump %.*s: spawn failed: %s\n",
                (int)$len(subpath), (char *)subpath[0], ok64str(sp));
        return BEDOGEXIT;
    }
    int rc = 0;
    ok64 rr = FILEReap(pid, &rc);
    if (rr == FILESIGNAL) return BEDOGSIG;
    if (rr != OK)         return rr;
    if (rc != 0)          return BEDOGEXIT;
    done;
}

//  POST-recursion fork: fork + chdir(<wt>/<subpath>) + execvp the
//  resolved self path.  Child stderr is inherited from the parent
//  (no piping/teeing), so the child's diagnostic lines flow
//  straight to the user.
//
//  *NONE detection rides on the exit code.  abc/PRO.h::MAIN returns
//  the full ok64 from main(), but the kernel truncates to the low
//  byte via WEXITSTATUS.  Every *NONE shares the low byte 0xCE (see
//  `BE_NONE_LOW_BYTE` near BERun) — `ok64is(NONE, …)` reads that as
//  "no match / no-op".  See plan §POST: "a sub with no dirty paths
//  just no-ops; parent doesn't bump that gitlink".
//
//  Returns OK on (child exit 0) OR (child exit *NONE-low-byte
//  residue, with *postnone_out = YES).  BEDOGEXIT / BEDOGSIG
//  otherwise.

static ok64 bepost_spawn_sub(u8cs wt_root, u8cs subpath,
                              u8css argv, b8 *postnone_out) {
    sane($ok(wt_root) && $ok(subpath) && postnone_out);
    *postnone_out = NO;

    //  Resolve self (absolute path); use as argv[0] so the child's
    //  HOMEResolveSibling finds the right bin dir.  argv[0] + PATH is
    //  portable across Linux and FreeBSD; /proc/self/exe is Linux-only.
    a_path(self_path);
    {
        a$rg(a0, 0);
        a_cstr(self_name, "be");
        (void)HOMEResolveSibling(NULL, self_path, self_name, a0);
        if (!u8bHasData(self_path)) {
            fprintf(stderr, "be: post: %.*s: cannot resolve self\n",
                    (int)$len(subpath), (char *)subpath[0]);
            return BEDOGEXIT;
        }
    }

    //  Compose mount path: <wt_root>/<subpath>.
    a_path(mount);
    call(PATHu8bFeed, mount, wt_root);
    call(PATHu8bAdd,  mount, subpath);

    //  Pack argv with self_path as argv[0].
    enum { POOL_BYTES = 8192, MAX_ARGS = 4 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS };
    a_pad(u8, pool, POOL_BYTES);
    size_t offs[MAX_ARGS];
    u32    nargs = 0;
    a_dup(u8c, self_view, u8bDataC(self_path));
    {
        size_t need = (size_t)u8csLen(self_view) + 1;
        if ((size_t)u8bIdleLen(pool) < need) return BEDOGEXIT;
        offs[nargs++] = (size_t)(u8bIdleHead(pool) - u8bDataHead(pool));
        if (!u8csEmpty(self_view)) call(u8bFeed, pool, self_view);
        call(u8bFeed1, pool, 0);
    }
    $for(u8cs, ap, argv) {
        if (nargs >= MAX_ARGS) return BEDOGEXIT;
        size_t need = (size_t)u8csLen(*ap) + 1;
        if ((size_t)u8bIdleLen(pool) < need) return BEDOGEXIT;
        offs[nargs] = (size_t)(u8bIdleHead(pool) - u8bDataHead(pool));
        if (!u8csEmpty(*ap)) call(u8bFeed, pool, *ap);
        call(u8bFeed1, pool, 0);
        nargs++;
    }
    //  Force `--tlv` so the child routes its commit report to stdout as
    //  a capturable TLV hunk stream (sniff/POST.c step 16 gates on TLV);
    //  we relay it below with the sub's path prefix.
    {
        a_cstr(tlv_lit, "--tlv");
        a_dup(u8c, tlv_v, tlv_lit);
        if (nargs >= MAX_ARGS) return BEDOGEXIT;
        size_t need = (size_t)u8csLen(tlv_v) + 1;
        if ((size_t)u8bIdleLen(pool) < need) return BEDOGEXIT;
        offs[nargs] = (size_t)(u8bIdleHead(pool) - u8bDataHead(pool));
        call(u8bFeed, pool, tlv_v);
        call(u8bFeed1, pool, 0);
        nargs++;
    }

    char *argv_c[MAX_ARGS + 1];
    for (u32 i = 0; i < nargs; i++) {
        argv_c[i] = (char *)(u8bDataHead(pool) + offs[i]);
    }
    argv_c[nargs] = NULL;
    char const *mount_cstr = (char const *)u8bDataHead(mount);

    int pfd[2];
    if (pipe(pfd) != 0) {
        fprintf(stderr, "be: post: %.*s: pipe failed: %s\n",
                (int)$len(subpath), (char *)subpath[0], strerror(errno));
        return BEDOGEXIT;
    }
    pid_t pid = fork();
    if (pid < 0) {
        close(pfd[0]); close(pfd[1]);
        fprintf(stderr, "be: post: %.*s: fork failed: %s\n",
                (int)$len(subpath), (char *)subpath[0], strerror(errno));
        return BEDOGEXIT;
    }
    if (pid == 0) {
        close(pfd[0]);
        if (dup2(pfd[1], STDOUT_FILENO) < 0) _exit(127);
        close(pfd[1]);
        if (chdir(mount_cstr) != 0) {
            fprintf(stderr, "be: post: chdir %s: %s\n",
                    mount_cstr, strerror(errno));
            _exit(127);
        }
        execvp(argv_c[0], argv_c);
        fprintf(stderr, "be: post: execvp %s: %s\n",
                argv_c[0], strerror(errno));
        _exit(127);
    }

    //  Capture the child's TLV commit report to EOF (before reaping so
    //  the report is complete even on a non-zero exit), then relay it
    //  path-prefixed to the parent's stdout in the parent's HUNKMode.
    close(pfd[1]);
    a_carve(u8, capbuf, 1UL << 20);
    for (;;) {
        if (u8bIdleLen(capbuf) == 0) break;
        u8 *idle = u8bIdleHead(capbuf);
        size_t cap = (size_t)u8bIdleLen(capbuf);
        ssize_t n = read(pfd[0], idle, cap);
        if (n < 0) { if (errno == EINTR) continue; break; }
        if (n == 0) break;
        u8bFed(capbuf, (u32)n);
    }
    close(pfd[0]);

    int rc = 0;
    ok64 wo = FILEReap(pid, &rc);

    a_dup(u8c, captured, u8bData(capbuf));
    if (!$empty(captured)) {
        a_carve(u8, obuf, 1UL << 20);
        (void)HUNKu8sRelay(u8bIdle(obuf), subpath, captured);
        a_dup(u8c, relayed, u8bData(obuf));
        if (!$empty(relayed)) {
            fflush(stdout);
            (void)write(STDOUT_FILENO, relayed[0], (size_t)$len(relayed));
        }
    }

    if (wo == FILESIGNAL) return BEDOGSIG;
    if (wo != OK)         return wo;
    if (rc == 0)          done;
    //  Child exited non-zero.  POSTNONE's low-byte residue is a
    //  no-op signal per plan §POST.
    if (rc == BE_NONE_LOW_BYTE) { *postnone_out = YES; done; }
    return BEDOGEXIT;
}

//  POST recursion context (SUBS.plan.md §POST).  The wrapper carries
//  the parent's parsed cli (for flag iteration) plus the precomputed
//  parent_msg (from `#frag` or `-m`); the cb composes a fresh child
//  argv per sub so it can substitute the per-sub message (either a
//  `--sub-msg <subpath>=<msg>` override or the decorated default
//  `<parent-msg> [<subpath>]`).
//
//  dry_only flips the wrapper into status-aggregation mode: child
//  argv strips the parent's msg (so each level dry-runs) and the
//  bump-after-recurse step is skipped (sub tips don't move, so the
//  parent must not stage gitlink rows that would otherwise pollute
//  the wt's pd boundary).
typedef struct {
    u8cs   wt_root;
    cli   *c;            //  parent's cli — for flag / URI access
    u8cs   parent_msg;   //  effective msg from #frag or -m (may be empty)
    b8     dry_only;     //  status-only walk (no msg forwarded, no bump)
    b8     outer_emitted;
    ok64   worst;
} bepost_recurse_ctx;

//  Match `--sub-msg <subpath>=<msg>` flag entries against `subpath`.
//  Returns YES + writes msg into out_msg if found.
static b8 bepost_find_sub_msg(cli const *c, u8cs subpath, u8csp out_msg) {
    out_msg[0] = NULL; out_msg[1] = NULL;
    a_cstr(sm_flag, "--sub-msg");
    for (u32 i = 0; i + 1 < u8csbDataLen(c->flags); i += 2) {
        if (!$eq((*u8csbAtP(c->flags, i)), sm_flag)) continue;
        u8cs val = {(*u8csbAtP(c->flags, i + 1))[0], (*u8csbAtP(c->flags, i + 1))[1]};
        //  Split val on the first '='.
        u8c *eq = NULL;
        $for(u8c, p, val) { if (*p == '=') { eq = p; break; } }
        if (eq == NULL) continue;
        u8cs key = {val[0], eq};
        u8cs msg = {eq + 1, val[1]};
        if (u8csEq(key, subpath)) {
            out_msg[0] = msg[0];
            out_msg[1] = msg[1];
            return YES;
        }
    }
    return NO;
}

static void bepost_emit_outer(bepost_recurse_ctx *rc) {
    if (rc->outer_emitted) return;
    fprintf(stderr, "be: post .\n");
    rc->outer_emitted = YES;
}

//  Compose the synthetic commit target for a beagle submodule POST
//  (SUBS.plan.md §"detached-sub commit"; wiki design): a mounted sub
//  is detached at the gitlink pin, so a parent-driven POST commits it
//  onto a synthetic branch `?/<subproj>/.<parentproj>[/<parentbranch>]`
//  in the SHARED store.  The branch is a real REFS tip → the new
//  commit stays GC-reachable in the sub's self-contained shard, where
//  a ref-less detached commit would be dropped at epoch recompaction.
//
//  THIS level's coordinate (parentproj, parentbranch) comes from the
//  explicit branch-query URI the invocation carries (a parent's
//  target, including a synthetic coord injected by an OUTER recursion
//  — this is what makes the prefix nest `?/leaf/.sub/.proj/br`), else
//  from the wt's own tail (a bare top-level post on an attached
//  branch).  `subproj` is read from the sub mount's row-0 anchor.
//  Returns NONE (→ caller skips injection, sub refuses if detached)
//  when no real parent project resolves (e.g. detached top-level wt
//  with no target).
//  Read a worktree's project TITLE from its row-0 anchor, handling
//  BOTH anchor encodings: project in the PATH (`file:<store>/.be/<proj>/`
//  — primary / colocated wt) and project in the QUERY (`file:<store>/.be/
//  ?/<proj>` — secondary-wt / submodule anchor, DIS-001).  SNIFFAtTailOf
//  only surfaces the path form, so we read the anchor directly here.
static ok64 bepost_wt_project(u8cs wt_root, u8bp out) {
    sane($ok(wt_root) && out);
    u8bReset(out);
    a_path(be);
    call(PATHu8bFeed, be, wt_root);
    call(PATHu8bPush, be, DOG_BE_S);
    a_path(anchor);
    {
        filestat fs = {};
        if (FILELStat(&fs, $path(be)) != OK) return NONE;
        a_dup(u8c, bes, u8bDataC(be));
        call(PATHu8bFeed, anchor, bes);
        if (fs.kind == FILE_KIND_DIR) {
            a_cstr(wl, "wtlog");
            call(PATHu8bPush, anchor, wl);
        }
    }
    u8bp mapped = NULL;
    if (FILEMapRO(&mapped, $path(anchor)) != OK) return NONE;
    a_dup(u8c, scan, u8bDataC(mapped));
    ulogrec row0 = {};
    ok64 ret = NONE;
    if (ULOGu8sDrain(scan, &row0) == OK) {
        a_path(pp);
        DOGProjectFromBe(row0.uri.path, pp);     //  path form
        if (u8bDataLen(pp) > 0) {
            a_dup(u8c, ppv, u8bDataC(pp));
            u8bFeed(out, ppv); ret = OK;
        } else {
            u8cs qp = {};
            DOGQueryProject(row0.uri.query, qp); //  query form
            if (!u8csEmpty(qp)) { u8bFeed(out, qp); ret = OK; }
        }
    }
    FILEUnMap(mapped);
    return ret;
}

static ok64 bepost_synth_child_uri(cli *c, u8cs wt_root, u8cs subpath,
                                   u8bp out) {
    sane(c && $ok(wt_root) && $ok(subpath) && out);
    u8bReset(out);

    a_pad(u8, pproj_buf, 128);
    a_pad(u8, pbr_buf,   256);
    {
        //  Prefer an explicit local branch-query URI on this invocation
        //  (a parent's target, incl. an outer recursion's synthetic
        //  coord — this is what nests the prefix).
        u8cs lq = {};
        for (u32 i = 0; i < CLIUriLen(c); i++) {
            uri uv = {};
            call(CLIUriAt, &uv, c, i);
            if (u8csEmpty(uv.scheme) && u8csEmpty(uv.authority) &&
                !u8csEmpty(uv.query)) { u8csMv(lq, uv.query); break; }
        }
        if (!u8csEmpty(lq)) {
            u8cs qp = {};
            DOGQueryProject(lq, qp);             //  absolute → project
            if (!u8csEmpty(qp)) u8bFeed(pproj_buf, qp);
            a_dup(u8c, lq2, lq);
            DOGQueryStripProject(lq2);           //  → <branch> (relative: unchanged)
            if (!u8csEmpty(lq2)) u8bFeed(pbr_buf, lq2);
        } else {
            //  Bare top-level post: branch from the wt's tail.
            a_pad(u8, tail, FILE_PATH_MAX_LEN + 128);
            if (SNIFFAtTailOf(wt_root, tail) == OK) {
                uri tu = {};
                u8csMv(tu.data, u8bDataC(tail));
                if (URILexer(&tu) == OK) {
                    a_dup(u8c, tq, tu.query);
                    DOGQueryStripProject(tq);
                    if (!u8csEmpty(tq)) u8bFeed(pbr_buf, tq);
                }
            }
        }
    }
    //  Project fallback: this wt's own title (covers project-relative
    //  targets `?feat` and path-encoded anchors alike).
    if (u8bDataLen(pproj_buf) == 0) {
        a_pad(u8, wp, 128);
        if (bepost_wt_project(wt_root, wp) == OK && u8bDataLen(wp) > 0) {
            a_dup(u8c, wpv, u8bDataC(wp));
            u8bFeed(pproj_buf, wpv);
        }
    }
    if (u8bDataLen(pproj_buf) == 0) return NONE;

    //  subproj from the sub mount's row-0 anchor (path OR query form).
    a_path(submount);
    call(PATHu8bFeed, submount, wt_root);
    call(PATHu8bAdd,  submount, subpath);
    a_pad(u8, subproj_buf, 128);
    {
        a_dup(u8c, sroot, u8bDataC(submount));
        if (bepost_wt_project(sroot, subproj_buf) != OK ||
            u8bDataLen(subproj_buf) == 0)
            return NONE;
    }

    //  Build `?/<subproj>/.<pproj>[/<pbranch>]`.
    call(u8bFeed1, out, '?');
    call(u8bFeed1, out, '/');
    { a_dup(u8c, sp, u8bDataC(subproj_buf)); call(u8bFeed, out, sp); }
    call(u8bFeed1, out, '/');
    call(u8bFeed1, out, '.');
    { a_dup(u8c, pp, u8bDataC(pproj_buf)); call(u8bFeed, out, pp); }
    if (u8bDataLen(pbr_buf) > 0) {
        call(u8bFeed1, out, '/');
        a_dup(u8c, pb, u8bDataC(pbr_buf));
        call(u8bFeed, out, pb);
    }
    done;
}

static ok64 bepost_recurse_cb(besub const *s, void *vctx) {
    sane(s && vctx);
    bepost_recurse_ctx *rc = (bepost_recurse_ctx *)vctx;

    //  Declared but not mounted on disk — report and skip.  Nothing to
    //  recurse into and no SubReadTip to bump.
    if (!s->mounted) {
        bepost_emit_outer(rc);
        fprintf(stderr, "be: post %.*s: declared, not mounted\n",
                (int)$len(s->path), (char *)s->path[0]);
        return OK;
    }

    bepost_emit_outer(rc);
    fprintf(stderr, "be: post %.*s\n",
            (int)$len(s->path), (char *)s->path[0]);

    u8cs subpath = {};
    u8csMv(subpath, s->path);

    //  Compose the per-sub effective msg.  Resolution order:
    //    1. dry_only → empty (no msg forwarded; child dry-runs).
    //    2. `--sub-msg <subpath>=<msg>` override → use <msg> verbatim.
    //    3. parent_msg non-empty → "<parent_msg> [<subpath>]" (default
    //       decoration so the sub's commit body identifies the level).
    //    4. else: empty — child gets no `#frag` URI and may auto-resolve
    //       from its own put/patch rows (POSTNONE is treated as no-op
    //       by bepost_spawn_sub).
    a_pad(u8, msg_uri, MAX_URI_LEN);
    if (!rc->dry_only) {
        u8cs override = {};
        b8 have_override = bepost_find_sub_msg(rc->c, subpath, override);
        if (have_override) {
            if (!u8csEmpty(override)) {
                (void)u8bFeed1(msg_uri, '#');
                (void)u8bFeed (msg_uri, override);
            }
        } else if (!u8csEmpty(rc->parent_msg)) {
            (void)u8bFeed1(msg_uri, '#');
            (void)u8bFeed (msg_uri, rc->parent_msg);
            (void)u8bFeed1(msg_uri, ' ');
            (void)u8bFeed1(msg_uri, '[');
            (void)u8bFeed (msg_uri, subpath);
            (void)u8bFeed1(msg_uri, ']');
        }
    }

    //  Build child argv: `post -q` + flags-sans-{--at,-m,--sub-msg,-q,
    //  --quiet} + non-fragment URIs + the fresh `#<sub-msg>` URI (if
    //  any).  `-q` silences POSTNONE chatter in every sibling shard
    //  with no changes — the cli wrapper converts POSTNONE to OK
    //  there, so the bump runs unconditionally (idempotent on a no-
    //  op sub).
    a_pad(u8cs, child_args, 5 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS);
    a_cstr(post_lit,  "post");
    a_cstr(q_lit,     "-q");
    a_cstr(qlong_lit, "--quiet");
    a_cstr(mf_lit,    "-m");
    a_cstr(sm_lit,    "--sub-msg");
    a_dup(u8c, post_d, post_lit);
    a_dup(u8c, q_d,    q_lit);
    u8csbFeed1(child_args, post_d);
    u8csbFeed1(child_args, q_d);
    size_t nflags = u8csbDataLen(rc->c->flags);
    for (size_t j = 0; j + 1 < nflags; j += 2) {
        u8cs *flag = u8csbAtP(rc->c->flags, j);
        u8cs *val  = u8csbAtP(rc->c->flags, j + 1);
        if ($eq(*flag, be_at_flag)) continue;
        if ($eq(*flag, mf_lit))     continue;
        if ($eq(*flag, sm_lit))     continue;
        if ($eq(*flag, q_lit))      continue;
        if ($eq(*flag, qlong_lit))  continue;
        u8csbFeed1(child_args, *flag);
        if (!u8csEmpty(*val)) u8csbFeed1(child_args, *val);
    }
    //  Compute the sub's synthetic commit target up front so we can
    //  both (a) skip forwarding THIS level's branch-query URI (the
    //  child must commit onto its OWN synthetic branch, not the
    //  parent's) and (b) append the freshly-composed target below.
    a_pad(u8, synth_uri, MAX_URI_LEN);
    b8 have_synth = (bepost_synth_child_uri(rc->c, rc->wt_root, subpath,
                                            synth_uri) == OK)
                    && u8bDataLen(synth_uri) > 0;

    size_t nuris = CLIUriLen(rc->c);
    for (size_t j = 0; j < nuris; j++) {
        uri uv = {};
        call(CLIUriAt, &uv, rc->c, j);
        uri *u = &uv;
        b8 pure_frag = !u8csEmpty(u->fragment) &&
                       u8csEmpty(u->scheme) &&
                       u8csEmpty(u->authority) &&
                       u8csEmpty(u->path) &&
                       u8csEmpty(u->query);
        if (pure_frag) continue;
        //  Transport URI = the PARENT's remote.  Never forward it to a
        //  sub: the sub commits LOCALLY only here (a git remote can't
        //  take a beagle sub; a keeper remote is pushed by the separate
        //  push-recursion path).  The commit message still reaches the
        //  sub via parent_msg → msg_uri, composed above.
        if (!u8csEmpty(u->scheme)) continue;
        //  Local branch-query URI = this level's commit target; do not
        //  forward verbatim (project belongs to THIS level, not the
        //  sub).  Its coordinate is already folded into synth_uri.
        b8 local_branch = !u8csEmpty(u->query) &&
                          u8csEmpty(u->scheme) &&
                          u8csEmpty(u->authority) &&
                          u8csEmpty(u->path);
        if (local_branch && have_synth) continue;
        u8csbFeed1(child_args, u->data);
    }
    if (have_synth) {
        a_dup(u8c, synth_view, u8bDataC(synth_uri));
        u8csbFeed1(child_args, synth_view);
    }
    if (u8bDataLen(msg_uri) > 0) {
        a_dup(u8c, msg_view, u8bData(msg_uri));
        u8csbFeed1(child_args, msg_view);
    }
    a_dup(u8cs, child_argv, u8csbData(child_args));

    //  Child runs its own `be post`: depth recursion is its
    //  responsibility.  bepost_spawn_sub returns OK with `postnone=YES`
    //  when the sub had nothing to commit; we skip the bump in that
    //  case so the parent doesn't move that gitlink (plan §POST).
    b8 postnone = NO;
    ok64 r = bepost_spawn_sub(rc->wt_root, subpath, child_argv, &postnone);
    if (r != OK) { rc->worst = r; return OK; }
    if (rc->dry_only) return OK;       //  status-only walk; no bump.
    if (postnone) return OK;

    //  Pure-push gate (Bug 4): if the PARENT has no commit intent
    //  (no #frag from the user, no -m flag), we're in a pure-push
    //  shape — `be post //host` whose only effect should be to
    //  push existing tips, not mint a new commit.  Skipping the
    //  bump here prevents the synthesised put row from tripping
    //  sniff into a selective-mode commit on cur.  Gitlink lag is
    //  intentional in that shape; the user runs `be post '#bump
    //  sub' //host` to actually commit the gitlink update.
    if (u8csEmpty(rc->parent_msg)) return OK;

    //  Parent has commit intent + sub returned OK with a real commit.
    //  Fork `be put <subpath>` so the parent's wtlog gets a `put
    //  <subpath>#<40-hex>` row at the new tip; BEActSniffPost picks
    //  that up and emits the gitlink ADD in the parent commit.
    ok64 br = bepost_bump_sub(subpath);
    if (br != OK) rc->worst = br;
    return OK;
}



//  --- POST action bodies (Stage 5 migration) -----------------------

//  Is `u` a BEAGLE (keeper-protocol) push target?  Mirrors the
//  keeper-vs-git split in keeper/WIRECLI.c wcli_spawn: keeper:// /
//  be:// (and file:// / local store paths that aren't git repos)
//  speak the keeper protocol and carry submodules as sibling shards
//  we recurse into on get AND post; plain ssh:// / //host / *.git are
//  vanilla git peers the user manages themselves (no recursion — see
//  post/16, which drives that push order by hand).
static b8 be_post_target_is_keeper(uri const *u) {
    a_cstr(keeper_s, "keeper");
    a_cstr(be_s,     "be");
    a_cstr(file_s,   "file");
    if (u8csEq(u->scheme, keeper_s) || u8csEq(u->scheme, be_s)) return YES;
    //  `.git` suffix marks a git repo regardless of scheme.
    a_cstr(git_dot, ".git");
    if (u8csLen(u->path) >= u8csLen(git_dot)) {
        u8cs tail = {u->path[1] - u8csLen(git_dot), u->path[1]};
        if (u8csEq(tail, git_dot)) return NO;
    }
    if (u8csEq(u->scheme, file_s)) return YES;
    //  Local form: no scheme, no authority → a local keeper store.
    if (u8csEmpty(u->scheme) && u8csEmpty(u->authority)) return YES;
    //  ssh://host, //host, http(s):// → git peer.
    return NO;
}

//  Transport-push recursion state: forward the parent's transport
//  LOCATOR (scheme + authority + path, project query dropped) to each
//  mounted sub.  The locator alone is multi-project; per-sub the
//  callback re-attaches the sub's OWN project selector `?/<subproj>`
//  (SUBS-017) so the sub pushes to its OWN sibling shard on the same
//  peer, never the synthetic dot-coordinate or the parent's default
//  project shard.  Mirrors the GET-side `subs_candidate_from_source`
//  (sniff/SUBS.c) which re-targets the FETCH the same way.
typedef struct {
    cli  *c;
    u8cs  wt_root;
    u8cs  fwd_uri;          //  parent's transport LOCATOR (no ?/proj)
    b8    outer_emitted;
    ok64  worst;
} bepush_recurse_ctx;

//  Build the per-sub push URI = `<locator>?/<subproj>` (SUBS-017).
//  `subproj` is read from the sub mount's row-0 anchor (path OR query
//  form) via bepost_wt_project.  When the sub's project can't be
//  resolved (a non-beagle-store mount, or a malformed anchor) the
//  locator is emitted verbatim so the prior behavior is preserved for
//  callers that relied on the peer's row-0 default.  Returns OK with
//  `out` populated; NONE when even the locator copy fails.
static ok64 bepush_sub_uri(u8cs wt_root, u8cs subpath, u8cs locator,
                           u8bp out) {
    sane($ok(wt_root) && $ok(subpath) && $ok(locator) && out);
    u8bReset(out);
    if (u8csEmpty(locator)) return NONE;
    call(u8bFeed, out, locator);

    //  subproj from the sub mount's row-0 anchor (path OR query form).
    a_path(submount);
    call(PATHu8bFeed, submount, wt_root);
    call(PATHu8bAdd,  submount, subpath);
    a_pad(u8, subproj_buf, 128);
    {
        a_dup(u8c, sroot, u8bDataC(submount));
        if (bepost_wt_project(sroot, subproj_buf) != OK ||
            u8bDataLen(subproj_buf) == 0)
            done;                       //  locator-only fallback
    }
    //  Append the absolute `?/<subproj>` shard selector.  keeper's
    //  keeper_remote_uri preserves it onto the served path, so the peer
    //  routes to the sub's own shard (keeper_served_at → HOMEOpen).
    call(u8bFeed1, out, '?');
    call(u8bFeed1, out, '/');
    a_dup(u8c, sp, u8bDataC(subproj_buf));
    call(u8bFeed, out, sp);
    done;
}

static void bepush_emit_outer(bepush_recurse_ctx *rc) {
    if (rc->outer_emitted) return;
    fprintf(stderr, "be: post .\n");
    rc->outer_emitted = YES;
}

static ok64 bepush_recurse_cb(besub const *s, void *vctx) {
    sane(s && vctx);
    bepush_recurse_ctx *rc = (bepush_recurse_ctx *)vctx;

    if (!s->mounted) {
        bepush_emit_outer(rc);
        fprintf(stderr, "be: post %.*s: declared, not mounted\n",
                (int)$len(s->path), (char *)s->path[0]);
        return OK;
    }
    bepush_emit_outer(rc);
    fprintf(stderr, "be: post %.*s\n",
            (int)$len(s->path), (char *)s->path[0]);

    u8cs subpath = {};
    u8csMv(subpath, s->path);

    //  child argv: `post` + flags(sans --at) + the per-sub transport
    //  URI.  The sub runs its own `be post <locator>?/<subproj>`
    //  (recursing into its own subs first), pushing its OWN project's
    //  tip to the SAME peer's sibling shard (SUBS-017).  Without the
    //  `?/<subproj>` selector the sub's commit lands on the peer's
    //  row-0 default project (the parent's shard) under the synthetic
    //  dot-branch — the wrong target that aborted the push WIRECLFL.
    //  Runs BEFORE the parent's BEActKeeperPush, so a fresh `--recurse`
    //  clone never sees a parent tree pointing at a sub sha the peer
    //  lacks.
    a_pad(u8, sub_uri_buf, FILE_PATH_MAX_LEN + 128);
    u8cs sub_uri = {};
    if (bepush_sub_uri(rc->wt_root, subpath, rc->fwd_uri, sub_uri_buf) == OK
        && u8bDataLen(sub_uri_buf) > 0) {
        a_dup(u8c, sv, u8bDataC(sub_uri_buf));
        u8csMv(sub_uri, sv);
    } else {
        u8csMv(sub_uri, rc->fwd_uri);   //  locator-only fallback
    }

    a_pad(u8cs, child_args, 4 + CLI_MAX_FLAGS * 2);
    a_cstr(post_lit, "post");
    a_dup(u8c, post_d, post_lit);
    u8csbFeed1(child_args, post_d);
    for (size_t j = 0; j + 1 < u8csbDataLen(rc->c->flags); j += 2) {
        u8cs *flag = u8csbAtP(rc->c->flags, j);
        u8cs *val  = u8csbAtP(rc->c->flags, j + 1);
        if ($eq(*flag, be_at_flag)) continue;
        u8csbFeed1(child_args, *flag);
        if (!u8csEmpty(*val)) u8csbFeed1(child_args, *val);
    }
    u8csbFeed1(child_args, sub_uri);
    a_dup(u8cs, child_argv, u8csbData(child_args));

    ok64 r = BERelaySub(rc->wt_root, subpath, child_argv);
    if (r != OK && !ok64is(r, NONE)) rc->worst = r;   //  NONE = no-op
    return OK;
}

//  Pre-order submodule recursion: each sub commits before the
//  parent.  Self-gates on bare-status / transport / no-wt-root so
//  the recursion only fires when it makes sense.  Returns BESTOP
//  in --dry-run mode after emitting the parent's status, so the
//  subsequent BEActSniffPost / BEActKeeperPush / BEActReindex rows
//  don't run.
ok64 BEActSubsPost(cli *c) {
    sane(c);

    //  `--nosub`: skip all submodule recursion for this invocation,
    //  mirroring BEActSubsGet / BEActSubsPatch / BEActSubsRelay.
    if (CLIHas(c, "--nosub")) done;

    //  Detect transport / msg shape from URIs and -m flag.  No URI
    //  mutation here — BEActPromoteRef already ran above us in the
    //  table.
    b8 has_transport = NO;
    b8 has_msg = NO;
    for (u32 i = 0; i < CLIUriLen(c); i++) {
        uri uv = {};
        call(CLIUriAt, &uv, c, i);
        if (!u8csEmpty(uv.scheme))   has_transport = YES;
        if (!u8csEmpty(uv.fragment)) has_msg = YES;
    }
    if (!has_msg) {
        a_cstr(mf, "-m");
        for (u32 fi = 0; fi + 1 < u8csbDataLen(c->flags); fi += 2) {
            if ($eq((*u8csbAtP(c->flags, fi)), mf)) { has_msg = YES; break; }
        }
    }

    b8 dry_only   = CLIHas(c, "--dry-run") ? YES : NO;
    b8 bare_status = !has_msg && CLIUriLen(c) == 0;

    //  Transport push.  For a BEAGLE peer the sub shards live on the
    //  same peer, so recurse the push into each mounted sub (post-
    //  order: subs land before the parent tree that references them).
    //  For a git peer, do nothing — the user manages git submodules
    //  (post/16 drives that order by hand).  Dry-only skips this and
    //  falls through to the audit walk below.
    if (!dry_only && has_transport) {
        uri tuv = {};
        uri *tu = NULL;
        for (u32 i = 0; i < CLIUriLen(c); i++) {
            uri cand = {};
            call(CLIUriAt, &cand, c, i);
            if (!u8csEmpty(cand.scheme)) {
                tuv = cand;
                tu  = &tuv;
                break;
            }
        }
        if (tu && be_post_target_is_keeper(tu) && u8bHasData(c->repo)) {
            a_dup(u8c, push_root, $path(c->repo));
            //  Forward the store LOCATOR (scheme + authority + path),
            //  dropping the parent's project/branch query: a beagle
            //  store is multi-project, so the SAME locator hosts the
            //  sub's shard.  bepush_sub_uri (SUBS-017) re-attaches each
            //  sub's OWN `?/<subproj>` selector so the sub pushes its
            //  own sibling shard.  Keeping the path is essential for
            //  file:/// (store lives in the path, authority empty) and
            //  be://host/store alike.  Compose via URIMake — the manual
            //  `scheme + "://" + authority` form double-emitted the `//`
            //  for a `file:///abs` URI (authority lexes as `//`),
            //  yielding `file://///abs` which broke the per-sub query
            //  re-attach (WIRECLFL).
            a_pad(u8, fwd_buf, FILE_PATH_MAX_LEN + 64);
            {
                u8cs none = {NULL, NULL};
                u8cs sch  = {tu->scheme[0],    tu->scheme[1]};
                u8cs auth = {tu->authority[0], tu->authority[1]};
                u8cs pth  = {tu->path[0],      tu->path[1]};
                (void)URIMake(u8bIdle(fwd_buf), sch, auth, pth, none, none);
            }
            a_dup(u8c, fwd_view, u8bData(fwd_buf));
            bepush_recurse_ctx prc = {
                .c = c, .outer_emitted = NO, .worst = OK,
            };
            u8csMv(prc.wt_root, push_root);
            u8csMv(prc.fwd_uri, fwd_view);
            (void)BESubsHere(push_root, bepush_recurse_cb, &prc);
            return prc.worst;   //  OK → plan proceeds to parent push
        }
        //  Git peer: the REMOTE push can't recurse into a git submodule,
        //  but the LOCAL commits still MUST happen — fall through to the
        //  local commit recursion below.  It forwards a local `be post`
        //  (transport URI stripped) into each mounted sub, so a dirty
        //  sub commits + bumps its gitlink BEFORE the parent's own git
        //  push (BEActKeeperPush) runs.  No `done` here — local sub work
        //  is unconditional; only the sub PUSH is suppressed for git.
    }

    //  Local commit path: ALWAYS recurse.  Every mounted sub is a beagle
    //  worktree — there is no git locally — so a commit must descend into
    //  subs regardless of the parent's remote (a git remote only governs
    //  whether the PUSH recurses, handled above).  Reached for both
    //  no-transport posts and git-peer transport posts.  `--nosub`
    //  already short-circuited at the top of this function.
    if (CLIHas(c, "--nosub")) done;

    //  Skip recursion on bare-status (no commit work to recurse into).
    //  Dry-only always recurses (it's the audit pass).
    if (!dry_only && bare_status) done;

    //  Need a wt root to enumerate / chdir from.
    if (!u8bHasData(c->repo)) done;
    a_dup(u8c, wt_root, $path(c->repo));

    //  Effective parent msg for sub argv composition.
    u8cs parent_msg = {};
    for (u32 i = 0; i < CLIUriLen(c); i++) {
        uri uv = {};
        call(CLIUriAt, &uv, c, i);
        if (!u8csEmpty(uv.fragment)) {
            u8csMv(parent_msg, uv.fragment);
            break;
        }
    }
    if (u8csEmpty(parent_msg)) {
        a_cstr(mf_lit, "-m");
        for (u32 fi = 0; fi + 1 < u8csbDataLen(c->flags); fi += 2) {
            if ($eq((*u8csbAtP(c->flags, fi)), mf_lit)) {
                u8csMv(parent_msg, (*u8csbAtP(c->flags, fi + 1)));
                break;
            }
        }
    }

    bepost_recurse_ctx rc = {
        .c             = c,
        .dry_only      = dry_only,
        .outer_emitted = NO,
        .worst         = OK,
    };
    u8csMv(rc.wt_root,    wt_root);
    u8csMv(rc.parent_msg, parent_msg);

    (void)BESubsHere(wt_root, bepost_recurse_cb, &rc);

    if (dry_only) {
        //  Parent's status report: spawn a bare `sniff post` (no
        //  msg, no URIs) so the dirty-paths / "0 changes" line
        //  prints uniformly.  Then short-circuit the rest of the
        //  plan — actual commits never fire in dry-only mode.
        a_pad(u8cs, args, 4);
        a_cstr(sniff_s, "sniff");
        a_cstr(post_s,  "post");
        a_dup(u8c, sniff_d, sniff_s);
        a_dup(u8c, post_d,  post_s);
        u8csbFeed1(args, sniff_d);
        u8csbFeed1(args, post_d);
        if (u8bDataLen(be_at_buf) > 0) {
            a_dup(u8c, at_flag, be_at_flag);
            a_dup(u8c, at_val,  u8bData(be_at_buf));
            u8csbFeed1(args, at_flag);
            u8csbFeed1(args, at_val);
        }
        a_dup(u8cs, argv, u8csbData(args));
        (void)BERun(sniff_d, argv, NO);
        return BESTOP;
    }

    //  Sub failure → abort parent commit.  Committing on top of a
    //  half-applied forest lands a tree that misses the failed
    //  sub's gitlink bump.
    if (rc.worst != OK) {
        fprintf(stderr,
                "be: post: aborting parent commit — sub recursion "
                "failed (worst=%s).  Resolve the sub and retry.\n",
                ok64str(rc.worst));
        return rc.worst;
    }
    done;
}

//  POST post-pass: refresh spot+graf against the new tip.  Self-
//  gates on dry-run (no msg + no URIs → nothing committed → nothing
//  to reindex).
ok64 BEActReindex(cli *c) {
    sane(c);
    b8 has_msg = NO;
    for (u32 i = 0; i < CLIUriLen(c); i++) {
        uri uv = {};
        call(CLIUriAt, &uv, c, i);
        if (!u8csEmpty(uv.fragment)) { has_msg = YES; break; }
    }
    if (!has_msg) {
        a_cstr(mf, "-m");
        for (u32 fi = 0; fi + 1 < u8csbDataLen(c->flags); fi += 2) {
            if ($eq((*u8csbAtP(c->flags, fi)), mf)) { has_msg = YES; break; }
        }
    }
    b8 dry_run = !has_msg && CLIUriLen(c) == 0;
    if (dry_run) done;
    (void)be_reindex(c);
    done;
}

// --- Bare `be`: --update all dogs, then --status each ---

//  Canonize a user-typed file path slice into wt-relative form.
//  Handles cwd-relative inputs (`README`, `sub/file.c`, `./foo`,
//  `../sib`) and absolute inputs (`/home/me/proj/README`).
//
//  https://replicated.wiki/html/wiki/URI.html §"URI structure" pins the path slot to "for all other
//  cases: relative path within a project".  Beagle is the only
//  normalization point — sub-dogs receive canonic paths and never
//  re-resolve cwd.
//
//  Returns OK on success; `out` holds the wt-rel bytes (no leading
//  '/', trailing '/' preserved iff the user typed it that way — the
//  dir-form signal sniff PUT / DELETE use).  Returns PATHBAD when
//  the resolved absolute path lies outside `wt_root`.
static ok64 be_canon_path(path8b out, u8cs cwd, u8cs wt_root,
                          u8cs user_path) {
    sane(u8bOK(out));
    u8bReset(out);
    if (u8csEmpty(user_path)) done;

    b8 dir_form = (*u8csLast(user_path) == '/');

    a_path(absbuf);
    if (PATHu8sIsAbsolute(user_path)) {
        call(PATHu8bNorm, absbuf, user_path);
    } else {
        call(PATHu8bAbs, absbuf, cwd, user_path);
    }

    a_dup(u8c, abs, u8bDataC(absbuf));
    a_dup(u8c, base, wt_root);
    if (!u8csEmpty(base) && *u8csLast(base) == '/') u8csShed1(base);

    if (u8csEq(abs, base)) done;             //  the wt root itself

    a_dup(u8c, rel, abs);
    if (!u8csHasPrefix(rel, base))           return PATHBAD;
    u8csUsed(rel, u8csLen(base));
    if (u8csEmpty(rel) || *rel[0] != '/')    return PATHBAD;
    u8csUsed1(rel);

    call(u8bFeed, out, rel);
    if (dir_form && *u8csLast(u8bDataC(out)) != '/')
        call(u8bFeed1, out, '/');
    call(PATHu8bTerm, out);
    done;
}

//  Generic submodule-report aggregation for verbs that recurse: for
//  each mounted sub under `c`'s wt, relay the sub's report for the
//  given child `argv` (the child runs `be <argv> --tlv` in the mount,
//  emitting its own per-file report plus any deeper subs).  Relayed
//  hunks land in the parent's stream with each path prefixed by the
//  sub's mount path.  No-op when there's no wt or no mounted subs.
typedef struct {
    u8cs   wt_root;
    u8cs  *argv_head;   // child argv as head/term (u8css can't be a member)
    u8cs  *argv_term;
    ok64   worst;
    b8     did_work;    // any sub returned OK (staged/removed real work)?
} be_relay_ctx;

static ok64 be_relay_subs_cb(besub const *s, void *vctx) {
    sane(s && vctx);
    be_relay_ctx *rc = (be_relay_ctx *)vctx;
    if (!s->mounted) return OK;   // declared-not-mounted → nothing to relay
    u8cs subpath = {};
    u8csMv(subpath, s->path);
    u8css argv = {rc->argv_head, rc->argv_term};
    ok64 r = BERelaySub(rc->wt_root, subpath, argv);
    //  OK  → the sub staged / removed real work (verb did something).
    //  NONE→ the sub was clean (no-op); never marks did_work.
    //  else→ a real failure; record as worst.
    if (r == OK)                     rc->did_work = YES;
    else if (!ok64is(r, NONE))       rc->worst    = r;
    return OK;                     // keep enumerating other subs
}

//  Relay the given child argv into every mounted sub.  Returns:
//    *NONE-class via *worst untouched aside,
//  but the real signal is *out_did_work — set YES iff at least one sub
//  did real work — which the put/delete relay turns into an OK exit
//  even when the parent side was *NONE (SUBS-004).  `out_did_work` may
//  be NULL for callers (status) that only relay reports.
static ok64 be_relay_subs(cli *c, u8css argv, b8 *out_did_work) {
    sane(c);
    if (out_did_work) *out_did_work = NO;
    if (!u8bHasData(c->repo)) done;
    a_dup(u8c, wt_root, $path(c->repo));
    be_relay_ctx rc = {
        .argv_head = (u8cs *)argv[0],
        .argv_term = (u8cs *)argv[1],
        .worst     = OK,
        .did_work  = NO,
    };
    u8csMv(rc.wt_root, wt_root);
    (void)BESubsHere(wt_root, be_relay_subs_cb, &rc);
    if (out_did_work) *out_did_work = rc.did_work;
    return rc.worst;
}

//  PUT / DELETE submodule recursion: the bare (stage-all / delete-all)
//  form recurses the same verb into each mounted sub so dirty / missing
//  files inside subs are staged / removed and listed too — the
//  "commit recursively with submodules" workflow.  Path-scoped or
//  remote forms don't recurse (they target specific parent paths).
ok64 BEActSubsRelay(cli *c) {
    sane(c);
    if (CLIHas(c, "--nosub"))      done;
    //  Bare stage-all / delete-all recurses into mounted subs
    //  unconditionally — staging is local work, never gated on source.
    //  Path-scoped / remote forms target the parent and stop here.
    if (CLIUriLen(c) > 0)          done;   //  bare form only
    if (u8csEmpty(c->verb))        done;
    if (!u8bHasData(c->repo))      done;

    a_pad(u8cs, cargs, 5 + CLI_MAX_FLAGS * 2);
    u8csbFeed1(cargs, c->verb);
    //  No `-q` here: a clean sub exits `*NONE`, which be_recurse_capture
    //  surfaces as the NONE class so be_relay_subs can tell a clean sub
    //  (no-op) from one that staged real work.  The NONE is treated as a
    //  no-op below (it never marks did_work), so a clean submodule still
    //  doesn't turn a bare `be put` / `be delete` into an error.
    for (u32 j = 0; j + 1 < u8csbDataLen(c->flags); j += 2) {
        if ($eq((*u8csbAtP(c->flags, j)), be_at_flag)) continue;
        u8csbFeed1(cargs, (*u8csbAtP(c->flags, j)));
        if (!u8csEmpty((*u8csbAtP(c->flags, j + 1))))
            u8csbFeed1(cargs, (*u8csbAtP(c->flags, j + 1)));
    }
    a_dup(u8cs, cargv, u8csbData(cargs));
    b8 did_work = NO;
    try(be_relay_subs, c, cargv, &did_work);
    ok64 worst = __;
    if (worst != OK) return worst;
    //  SUBS-004: a sub staged / removed real work even though the parent
    //  side was empty (`*NONE`).  Return BESTOP so the executor exits OK,
    //  clearing the parent's stale last-NONE.  If every sub was clean
    //  (and no real failure), fall through to OK so the parent's `*NONE`
    //  still surfaces (truly-empty bare put → PUTNONE preserved).
    if (did_work) return BESTOP;
    done;
}

//  PUT-001: path-scoped `be put <sub>/<path>` relay.  PUT resolves
//  paths against the PARENT tree only; a `<sub>/<path>` argument names
//  a file INSIDE a mounted submodule (the parent holds a bare gitlink
//  there), so sniff put classifies it BASE_ONLY/not-seen and reports
//  "does not exist — skipped" → PUTNONE.  POST/GET/DELETE descend into
//  mounted subs (BEActSubsRelay / the projector route); PUT was the odd
//  verb out.  This action routes each path argument that lands inside a
//  mounted sub to the sub's own `be put` (BERelaySub, mirroring
//  beproj_route_cb), then rebuilds `c->uris` to keep only the
//  parent-bound arguments so the downstream BEActSniffPut row stages
//  those against the parent tree.  Honours `--nosub` (the documented
//  opt-out — a sub-path then stays parent-bound and fails as before).
//
//  Runs BEFORE BEActSniffPut.  Returns:
//    BESTOP  every argument was a sub-path AND at least one sub staged
//            real work — clean exit, no parent put (and the bare
//            BEActSubsRelay below stays a no-op since URIs are present).
//    OK      mixed / nothing relayed — fall through to BEActSniffPut for
//            the remaining (parent-bound) arguments.
typedef struct {
    cli  *c;
    uri  *u;
    u8cs  wt_root;
    b8    routed;       //  this URI landed in a mount and was relayed
    b8    did_work;     //  the relayed sub staged real work (not *NONE)
    ok64  worst;
} be_put_route_ctx;

static ok64 be_put_route_cb(besub const *s, void *vctx) {
    sane(s && vctx);
    be_put_route_ctx *rc = (be_put_route_ctx *)vctx;
    if (rc->routed)    return OK;          //  first matching mount wins
    if (!s->mounted)   return OK;

    u8cs rel = {};
    if (!be_path_in_mount(rc->u->path, s->path, rel)) return OK;
    //  `be put <sub>` (no interior path) is the gitlink-bump form sniff
    //  put already handles via its sub-mount short-circuit; leave it for
    //  the parent.  Only an interior `<sub>/<path>` relays here.
    if (u8csEmpty(rel)) return OK;

    //  child argv = `put` + forwarded flags (drop `--at`) + the
    //  mount-relative path.  BERelaySub appends `--tlv` itself.
    a_pad(u8cs, child_args, 4 + CLI_MAX_FLAGS * 2);
    a_cstr(put_lit, "put");
    a_dup(u8c, put_d, put_lit);
    u8csbFeed1(child_args, put_d);
    for (u32 j = 0; j + 1 < u8csbDataLen(rc->c->flags); j += 2) {
        if ($eq((*u8csbAtP(rc->c->flags, j)), be_at_flag)) continue;
        u8csbFeed1(child_args, (*u8csbAtP(rc->c->flags, j)));
        if (!u8csEmpty((*u8csbAtP(rc->c->flags, j + 1))))
            u8csbFeed1(child_args, (*u8csbAtP(rc->c->flags, j + 1)));
    }
    a_dup(u8c, rel_d, rel);
    u8csbFeed1(child_args, rel_d);
    a_dup(u8cs, child_argv, u8csbData(child_args));

    u8cs subpath = {};
    u8csMv(subpath, s->path);
    u8css argv = {(u8cs *)child_argv[0], (u8cs *)child_argv[1]};
    ok64 r = BERelaySub(rc->wt_root, subpath, argv);
    rc->routed = YES;
    //  OK → the sub staged real work; *NONE → the sub had nothing to do
    //  (e.g. the path is already settled); else → a real failure.
    if (r == OK)               rc->did_work = YES;
    else if (!ok64is(r, NONE)) rc->worst    = r;
    return OK;
}

ok64 BEActSubsPut(cli *c) {
    sane(c);
    if (CLIHas(c, "--nosub"))   done;      //  documented opt-out
    if (CLIUriLen(c) == 0)      done;      //  bare form → BEActSubsRelay
    if (!u8bHasData(c->repo))   done;
    a_dup(u8c, wt_root, $path(c->repo));

    //  Walk every requested URI: relay each path that lands inside a
    //  mounted sub, and keep the parent-bound ones for sniff put.  Kept
    //  entries (raw text slices) are compacted forward in `c->uris` IN
    //  PLACE (the write index never exceeds the read index, so the
    //  borrowed argv views are preserved); the routed tail is shed
    //  afterwards.  Each entry is parsed on demand (URI-004).
    u32  n          = (u32)CLIUriLen(c);
    u32  nkeep      = 0;
    b8   any_routed = NO;
    b8   any_work   = NO;
    ok64 worst      = OK;

    for (u32 i = 0; i < n; i++) {
        u8cs raw = {};
        CLIUriRawAt(raw, c, i);            //  copy slice before compaction
        uri ucur = {};
        call(CLIUriParse, &ucur, raw);
        uri *u   = &ucur;
        //  Only plain path arguments route into subs.  Scheme-bearing
        //  (`file:`, `ssh:`) or authority/query/fragment forms are PUT's
        //  remote-push / ref-write / move shapes — leave them parent-bound.
        b8 plain_path = !u8csEmpty(u->path) && u8csEmpty(u->scheme)
                      && u8csEmpty(u->authority) && u8csEmpty(u->query)
                      && u8csEmpty(u->fragment);
        b8 keep_it = YES;
        if (plain_path) {
            be_put_route_ctx rc = {
                .c = c, .u = u, .routed = NO, .did_work = NO, .worst = OK,
            };
            u8csMv(rc.wt_root, wt_root);
            (void)BESubsHere(wt_root, be_put_route_cb, &rc);
            if (rc.routed) {
                keep_it    = NO;           //  relayed → drop from parent set
                any_routed = YES;
                if (rc.did_work)    any_work = YES;
                if (rc.worst != OK) worst    = rc.worst;
            }
        }
        if (keep_it) {
            CLIUriSetRaw(c, nkeep, raw);   //  forward copy (w <= i)
            nkeep++;
        }
    }

    if (!any_routed) done;                 //  nothing relayed → unchanged
    if (worst != OK) return worst;         //  a sub failed for real

    //  Drop the now-stale tail entries so only the kept (parent-bound)
    //  args remain visible to BEActSniffPut / BEBuildArgv.
    (void)u8csbShed(c->uris, (size_t)(n - nkeep));

    //  Every argument was a relayed sub-path: there is no parent-bound
    //  path left, so the downstream BEActSniffPut MUST NOT run (an empty
    //  c->uris would make it a bare stage-all of the parent).  Short-
    //  circuit here:
    //    any sub staged real work → BESTOP (clean OK exit, clears the
    //                               executor's stale last-NONE).
    //    every sub was a no-op     → PUTNONE (the subs' own empty result;
    //                               preserve the natural "nothing to do").
    if (nkeep == 0) return any_work ? BESTOP : PUTNONE;
    //  Mixed: parent-bound paths remain — fall through to BEActSniffPut,
    //  whose result (OK if any stages, else PUTNONE) becomes the exit.
    //  A relayed sub having done work does NOT override a parent PUTNONE
    //  here (explicitly-listed parent paths all-unchanged is honest
    //  feedback); only the all-sub-paths case above clears it.
    done;
}

//  Bare `be` — overview of the working tree.  Forwards to bare
//  `sniff`, which lists Changed: and Untracked: against the baseline
//  tree (untracked-but-gitignored filtered).  spot / graf / keeper
//  dogs aren't surfaced here — they're index/storage layers without
//  user-relevant state to print.  Adding their summaries back is a
//  one-liner per dog if it ever matters.  After the local status, each
//  mounted submodule's status is relayed (path-prefixed) so bare `be`
//  lists affected files across the whole tree.
static ok64 BEDefault(cli *c) {
    sane(1);
    a_cstr(sniff_s,    "sniff");
    a_cstr(tlv_flag,   "--tlv");
    a_cstr(plain_flag, "--plain");
    a_cstr(color_flag, "--color");
    a_pad(u8cs, args, 2);
    u8csbFeed1(args, sniff_s);
    //  Forward the resolved mode explicitly — sniff inherits stdout
    //  but its TTY detection would override `be --plain` / `be --tlv`
    //  if stdout happens to be a terminal.
    if      (HUNKMode == HUNKOutTLV)   u8csbFeed1(args, tlv_flag);
    else if (HUNKMode == HUNKOutColor) u8csbFeed1(args, color_flag);
    else if (HUNKMode == HUNKOutPlain) u8csbFeed1(args, plain_flag);
    a_dup(u8cs, argv, u8csbData(args));
    try(BERun, sniff_s, argv, NO);
    ok64 sniff_rc = __;

    //  Relay each mounted sub's bare-status report.  Child argv is
    //  empty — bare `be` (BERelaySub forces `--tlv`), which re-enters
    //  this same path inside the mount.
    u8css empty_argv = {NULL, NULL};
    (void)be_relay_subs(c, empty_argv, NULL);
    return sniff_rc;
}

//  DIS-017: bareword GET classification probe.  Returns YES iff
//  `rel` (a project-relative path slice) names a TRACKED entry in
//  cur's baseline version tree — the tip the wt is currently
//  anchored to — regardless of whether it exists on disk.  This is
//  what decides whether a bareword `be get file.c` stays in the path
//  slot (single-file restore, resurrecting a deleted file — GET.mkd
//  §"resurrecting if deleted") or falls through to a `?branch`
//  switch (Verbs.mkd §"Bareword defaults": "path if it names a
//  tracked file, else query branch").
//
//  Mechanics mirror the resolver pass below (HOMEOpen →
//  KEEPOpenBranch → GRAFOpen), then read the baseline commit sha
//  from the wtlog tail (`SNIFFAtTailOf`, same standalone peek
//  `BEActGetBaseline` uses), decode it, fetch the commit object,
//  extract its root tree, and descend `rel` segment-by-segment via
//  GRAFPathDescend.  An OK descent (file OR dir entry) → tracked.
//  NO on any missing store / missing baseline / absent path — the
//  caller then routes the bareword to `?branch`, preserving the
//  real-branch-switch case.
static b8 be_bareword_tracked_in_baseline(cli *c, u8cs rel) {
    if (!u8bHasData(c->repo)) return NO;
    if (u8csEmpty(rel))       return NO;

    //  Baseline commit sha from the wtlog tail's `@<at>#<sha>`.
    a_pad(u8, at_buf, FILE_PATH_MAX_LEN + 128);
    if (SNIFFAtTailOf($path(c->repo), at_buf) != OK) return NO;
    uri bt = {};
    u8csMv(bt.data, u8bDataC(at_buf));
    URILexer(&bt);
    u8cs frag = {bt.fragment[0], bt.fragment[1]};
    if (u8csLen(frag) != 40) return NO;
    sha1 csha = {};
    if (sha1FromHex(&csha, frag) != OK) return NO;

    b8 tracked = NO;
    home rh = {};
    uri none = {};
    if (HOMEOpen(&rh, &none, NO) != OK) return NO;
    static u8c const _zero = 0;
    u8cs trunk = {&_zero, &_zero};
    ok64 ko = KEEPOpenBranch(&rh, trunk, NO);
    if (ko == OK || ko == KEEPOPEN || ko == KEEPOPENRO) {
        ok64 go = GRAFOpen(&rh, NO);
        if (go == OK || go == GRAFOPEN || go == GRAFOPENRO) {
            //  Commit object → root tree sha → descend the path.
            Bu8 *cbuf = &GRAF.obj_buf;
            u8bReset(*cbuf);
            u8 ct = 0;
            if (KEEPGetExact(&csha, *cbuf, &ct) == OK
                && ct == DOG_OBJ_COMMIT) {
                sha1 cur = {};
                if (GITu8sCommitTree(u8bDataC(*cbuf), cur.data) == OK
                    && GRAFPathDescend(&cur, rel) == OK)
                    tracked = YES;
            }
            if (go == OK) (void)GRAFClose();
        }
        if (ko == OK) (void)KEEPClose();
    }
    HOMEClose(&rh);
    return tracked;
}

//  Entry-point URI resolver (URI.mkd §"Resolution boundary").  `be` is
//  the only component with context, so it resolves every local,
//  query-bearing URI to the single canonical context-free form
//  `?/<project>/<branch>/<full-hash>` here, before sniff / keeper / the
//  sub-recursion ever see it.  A query already ending in a 40-hex pin
//  (DOGCanonQueryParse) is resolved — left untouched (idempotent).
//  Remote URIs (scheme/authority) are handled by BEActResolveRemote;
//  search / magic shapes KEEPResolveRef can't pin are left for the
//  downstream verb (staged: full coverage tracked in URI.mkd).
ok64 BEActResolveRef(cli *c) {
    sane(c);
    if (CLIUriLen(c) == 0) done;

    //  cur's branch (`/<project>/<branch>`) from the wtlog tail — the
    //  same standalone peek the bareword/baseline paths use.  (`be`'s own
    //  c->flags has no `--at`; that flag is only composed onto child argv.)
    a_pad(u8, cur_at_buf, FILE_PATH_MAX_LEN + 128);
    u8cs cur_branch = {};
    if (u8bHasData(c->repo) &&
        SNIFFAtTailOf($path(c->repo), cur_at_buf) == OK) {
        uri bt = {};
        u8csMv(bt.data, u8bDataC(cur_at_buf));
        URILexer(&bt);
        u8csMv(cur_branch, bt.query);
    }

    //  Anything to do?  (a local, query-bearing, not-yet-canonical URI)
    b8 any = NO;
    for (u32 i = 0; i < CLIUriLen(c); i++) {
        uri uv = {};
        call(CLIUriAt, &uv, c, i);
        uri *u = &uv;
        if (!u8csEmpty(u->scheme) || !u8csEmpty(u->authority)) continue;
        if (u8csEmpty(u->query)) continue;
        u8cs pr = {}, br = {}, pn = {};
        if (DOGCanonQueryParse(u->query, pr, br, pn)) continue;
        any = YES; break;
    }
    if (!any) done;

    home rh = {};
    uri hat = {};
    if (u8bHasData(c->repo)) u8csMv(hat.path, $path(c->repo));
    if (!u8csEmpty(cur_branch)) u8csMv(hat.query, cur_branch);  // project=parent
    if (HOMEOpen(&rh, &hat, NO) != OK) done;
    //  A colocated/default wt leaves h->project empty (HOME defers it to
    //  the caller's get/post-row scan; https://replicated.wiki/html/wiki/Store.html: never the top-level
    //  `.be/refs`, which is meaningless).  Take cur's project as default.
    if (!u8bHasData(rh.project)) {
        u8cs proj = {};
        DOGQueryProject(cur_branch, proj);
        if (!u8csEmpty(proj)) {
            u8bReset(rh.project);
            (void)u8bFeed(rh.project, proj);
        }
    }
    //  Open keeper at CUR's leaf (not trunk): KEEPOpenBranch walks
    //  trunk→…→leaf and registers each shard, so cur's children (the
    //  usual `?./feat` patch source) are reachable by REFS — a trunk-only
    //  open leaves them RESLVNONE.  Branch-only form (project stripped).
    u8cs cur_leaf = {};
    u8csMv(cur_leaf, cur_branch);
    DOGQueryStripProject(cur_leaf);     // `/parent/master` → `master`
    static u8c const _zero = 0;
    u8cs open_branch = {&_zero, &_zero};
    if (!u8csEmpty(cur_leaf)) u8csMv(open_branch, cur_leaf);
    ok64 ko = KEEPOpenBranch(&rh, open_branch, NO);
    if (!(ko == OK || ko == KEEPOPEN || ko == KEEPOPENRO)) {
        HOMEClose(&rh);
        done;
    }

    //  REFSResolveURI reads the funnel's project + cur leaf from `rh`.
    //  HOMEOpen / KEEPOpenBranch leave rh.cur_branch as the un-stripped
    //  `<project>/<branch>`, so pin it to the project-stripped leaf for
    //  relative refs (`?./x`, `?..`).  Set the LITERAL leaf bytes — NOT
    //  via HOMESetCurBranch, whose DPATHBranchNormFeed collapses a
    //  trunk-named leaf (`master`/`main`) to "".  The branch tree stores
    //  trunk-named segments literally (children live under `master/…`),
    //  so `?./fix` from cur=`master` must resolve to `master/fix`, not
    //  `fix` — matching the prior be_abs_branch behaviour.
    u8bReset(rh.cur_branch);
    (void)u8bFeed(rh.cur_branch, cur_leaf);

    //  Resolved URIs must outlive this plan frame (BEBuildArgv forwards
    //  `u->data`), so compose into a process-scoped scratch buffer.
    static u8 _resolve_scratch[CLI_MAX_URIS * 96];
    u8b scratch = {_resolve_scratch, _resolve_scratch, _resolve_scratch,
                   _resolve_scratch + sizeof(_resolve_scratch)};

    for (u32 i = 0; i < CLIUriLen(c); i++) {
        uri uv = {};
        call(CLIUriAt, &uv, c, i);
        uri *u = &uv;
        if (!u8csEmpty(u->scheme) || !u8csEmpty(u->authority)) continue;
        if (u8csEmpty(u->query)) continue;
        //  Rebase-one (`?br#`, present-but-empty fragment): the source is a
        //  branch to WALK (foster = next-unreplayed commit, NOT the tip), so
        //  pinning it to the tip-hash here is wrong until sniff's rebase-one
        //  walks from the resolved pin.  Leave it relative for sniff.
        //  (Staged — full per-shape coverage tracked in URI.mkd.)
        if (u->fragment[0] != NULL && u8csEmpty(u->fragment)) continue;

        //  URI-002 St.3: `be` is the SOLE canonicalizer.  Debang the
        //  query (`?feat!`) BEFORE resolution so the funnel sees the bare
        //  ref `feat`, then re-emit the `!` onto the rewritten canonical
        //  query so the whole-branch scope survives `be`'s resolve all
        //  the way to sniff PATCH.  The `!` is shed off a LOCAL copy of
        //  the query slice — `u->query` itself is left intact so the
        //  `DOGCanonQueryParse` check and the resolver both run on the
        //  same bare bytes.
        u8 bang = 0;
        a_dup(u8c, qbare, u->query);
        if (DOGDebangSlice(qbare)) bang |= DOG_BANG_QUERY;
        {
            u8cs pr = {}, br = {}, pn = {};
            if (DOGCanonQueryParse(qbare, pr, br, pn)) continue;
        }

        //  Ref arm: the keeper funnel produces the canonical query with
        //  the correct trunk (single-slash `/<proj>/<sha>`) / detached
        //  (double-slash `/<proj>//<sha>`) / branch (`/<proj>/<br>/<sha>`)
        //  shape, resolving the pin against rh's project + cur leaf.  The
        //  original fragment is preserved verbatim.
        u8c *before = u8bIdleHead(scratch);
        {
            u8 _qpad[320];
            u8s qw = {_qpad, _qpad + sizeof _qpad};
            if (REFSResolveURI(&rh, qw, qbare) != OK) continue;
            //  URI-001 Stage 3: the funnel canonicalises the SCOPE only
            //  (`?/proj/branch`, or `?/proj/<sha>` detached) — it never
            //  pins a tip into the fragment.  The original fragment is
            //  the verb payload (PATCH mode, GET `#~N`, …) and is
            //  preserved verbatim below.
            u8cs cq = {_qpad, qw[0]};        //  `?<scope>` canonical query
            if (u8bFeed(scratch, cq) != OK) continue;
            //  URI-002 St.3: re-emit the query-bang onto the canonical
            //  query so `?feat!` → `?/proj/feat!` carries the
            //  whole-branch modifier downstream to sniff PATCH.
            DOGDebangFeed(scratch, bang, DOG_BANG_QUERY);
        }
        if (u->fragment[0] != NULL) {
            if (u8bFeed1(scratch, '#') != OK) continue;
            u8cs frag_save = {u->fragment[0], u->fragment[1]};
            if (!u8csEmpty(frag_save) &&
                u8bFeed(scratch, frag_save) != OK) continue;
        }
        u8c *after = u8bIdleHead(scratch);
        u8cs full  = {before, after};

        //  Persist the canonical `?/<proj>/<branch>[/<sha>][!][#frag]` text
        //  back as the raw arg (URI-004); the scratch buffer outlives this
        //  plan frame so downstream parse-on-demand + BEBuildArgv see it.
        CLIUriSetRaw(c, i, full);
    }

    if (ko == OK) (void)KEEPClose();
    HOMEClose(&rh);
    done;
}

// --- Main ---

static ok64 becli_inner(cli *c) {
    sane(c);
    call(FILEInit);

    //  -m / --author take a following value (legacy commit-message
    //  flag — the new convention is to fold trailing words into the
    //  URI fragment, but `-m` remains accepted).
    call(CLIParse, c, BE_VERB_NAMES, "-m\0--author\0--sub-msg\0");
    CLISetHUNKMode(c);

    if (CLIHas(c, "-h") || CLIHas(c, "--help")) {
        BEUsage();
        done;
    }

    //  Theme forwarding: `be --light <args>` propagates to every dog
    //  spawn via $BRO_THEME (THEMESelect setenv's).  All sub-tools we
    //  pipe through bro (BERunPipeSubs) inherit env unchanged, so the
    //  pager picks up the same palette.  Unknown values → hard fail.
    char const *theme_name = NULL;
    if (CLIHas(c, "--16"))    theme_name = THEME_16;
    if (CLIHas(c, "--dark"))  theme_name = THEME_DARK;
    if (CLIHas(c, "--light")) theme_name = THEME_LIGHT;
    if (theme_name != NULL) call(THEMESelect, theme_name);

    //  Per-verb bareword default (https://replicated.wiki/html/wiki/Verbs.html §"Bareword defaults"):
    //  promote a bareword sitting in u->path into the verb's natural
    //  slot.  POST → fragment (commit msg); GET / HEAD / PATCH →
    //  query (branch); PUT / DELETE / verbless → path (no-op).  When
    //  a promotion fires we also rewrite u->data with a leading `?`
    //  or `#` so BEBuildArgv forwards a URI shape that sub-dogs
    //  re-parse the same way (no second round of bareword promotion
    //  at the sub-dog layer).  Bareword bytes get packed into one
    //  scratch buffer that lives for becli's full frame (covers
    //  the later BEExecute → BEBuildArgv hand-off).
    a_pad(u8, bareword_scratch, CLI_MAX_URIS * 65);
    {
        u8 def = 'p';
        if (!$empty(c->verb)) {
            a_cstr(_v_post,  "post");
            a_cstr(_v_get,   "get");
            a_cstr(_v_head,  "head");
            a_cstr(_v_patch, "patch");
            if      ($eq(c->verb, _v_post))  def = 'f';
            else if ($eq(c->verb, _v_get))   def = 'q';
            else if ($eq(c->verb, _v_head))  def = 'q';
            else if ($eq(c->verb, _v_patch)) def = 'q';
        }
        if (def != 'p') {
            a_cstr(_v_get, "get");
            b8 is_get = !$empty(c->verb) && $eq(c->verb, _v_get);
            for (u32 i = 0; i < CLIUriLen(c); i++) {
                uri uv = {};
                call(CLIUriAt, &uv, c, i);
                uri *u = &uv;
                u8cs orig_path = {u->path[0], u->path[1]};

                //  GET bareword classification (DIS-017; Verbs.mkd
                //  §"Bareword defaults", GET.mkd §"resurrecting if
                //  deleted"): a bareword that names a TRACKED entry in
                //  cur's baseline tree stays in the path slot (single-
                //  file restore — resurrecting it even when deleted on
                //  disk), else it falls through to a `?branch` switch.
                //  Keying on tracked-in-baseline status (not on-disk
                //  stat) is what lets `be get file.c` resurrect a
                //  deleted tracked file and what stops an UNTRACKED
                //  on-disk file from hijacking a branch name.  The
                //  bareword is cwd-relative as typed; canonicalize to
                //  the project-relative key the baseline tree uses.
                if (is_get && !u8csEmpty(orig_path)) {
                    a_path(cwdbuf);
                    if (FILEGetCwd(cwdbuf) == OK) {
                        a_dup(u8c, cwd_s, u8bDataC(cwdbuf));
                        a_dup(u8c, wt_s,  u8bDataC(c->repo));
                        a_path(rel);
                        if (be_canon_path(rel, cwd_s, wt_s, orig_path)
                                == OK) {
                            a_dup(u8c, rel_s, u8bDataC(rel));
                            if (be_bareword_tracked_in_baseline(c, rel_s))
                                continue;
                        }
                    }
                }

                ok64 pr = DOGPromoteBareword(u, def);
                if (pr != OK) continue;
                if (u->path[0] != NULL) continue;        // not promoted
                if (u8csEmpty(orig_path)) continue;
                u8c *before = *u8bIdle(bareword_scratch);
                if (u8bFeed1(bareword_scratch,
                             (def == 'q') ? '?' : '#') != OK) continue;
                if (u8bFeed(bareword_scratch, orig_path) != OK) continue;
                u8c *after = *u8bIdle(bareword_scratch);
                //  Persist the promoted `?<word>` / `#<word>` text back as
                //  the raw arg (URI-004) so downstream parse-on-demand and
                //  BEBuildArgv forward the promoted URI shape.
                u8cs promoted = {before, after};
                CLIUriSetRaw(c, i, promoted);
            }
        }
    }

    //  Subdir-of-existing-repo + remote `be get`: drop a fresh shard
    //  under the ancestor's `.be/` and anchor cwd there before the
    //  --at flag is composed.  Without this the sub-dogs would walk
    //  up to the parent and pollute its keeper.
    {
        a_cstr(v_get_s, "get");
        if (!u8csEmpty(c->verb) && u8csEq(c->verb, v_get_s) &&
            CLIUriLen(c) > 0) {
            uri u0 = {};
            (void)CLIUriAt(&u0, c, 0);
            (void)be_sub_shard_setup(c, &u0);
        }
    }

    //  Top-of-chain version resolver pass.  Every user-supplied URI
    //  with a non-empty `?query` runs through `GRAFResolveVersion`
    //  (graf/GRAF.h) so downstream dogs see only the canonic
    //  `?/<project>.<hashlet>/<branch>/<sha-or-tag>` form
    //  (https://replicated.wiki/html/wiki/URI.html §"URI structure").  Projector schemes own their
    //  own grammar (`sha1:`, `log:`, `diff:`, ...) — skipped here;
    //  the projector dog parses its URI itself.  Per-URI failure
    //  isolates: a single GRAFNONE leaves that URI as-is so
    //  downstream can surface the diagnosis in context.
    //
    //  Currently a pass-through stub — wiring lands first so future
    //  resolution arms (magic refs, project-relative paths,
    //  hashlets, commit-msg search) can ship behind a stable be-side
    //  surface.
    //
    //  Scratch buffer lives at function scope so the rewritten
    //  `u->data` slices outlive the resolution if-block (CLAUDE.md §5
    //  — resource lifetime).
    //  Verb gate: only read-shaped verbs (GET / HEAD / PATCH)
    //  benefit from a pinned source sha.  Write verbs (POST / PUT /
    //  DELETE) operate on ref names and would only get confused by
    //  receiving a canonic `?/<project>/<branch>/<sha>` they then
    //  have to parse back out.  Resolver is a no-op for them.
    a_pad(u8, resolve_scratch, MAX_URI_LEN * CLI_MAX_URIS);
    b8 verb_wants_resolve = NO;
    {
        a_cstr(v_get_s,   "get");
        a_cstr(v_head_s,  "head");
        a_cstr(v_patch_s, "patch");
        if ($eq(c->verb, v_get_s) || $eq(c->verb, v_head_s)
                                  || $eq(c->verb, v_patch_s))
            verb_wants_resolve = YES;
    }
    if (verb_wants_resolve
        && u8bHasData(c->repo) && CLIUriLen(c) > 0) {
        home rh = {};
        uri none = {};
        if (HOMEOpen(&rh, &none, NO) == OK) {
            static u8c const _zero = 0;
            u8cs trunk = {&_zero, &_zero};
            ok64 ko = KEEPOpenBranch(&rh, trunk, NO);
            if (ko == OK || ko == KEEPOPEN || ko == KEEPOPENRO) {
                ok64 go = GRAFOpen(&rh, NO);
                if (go == OK || go == GRAFOPEN || go == GRAFOPENRO) {
                    for (u32 i = 0; i < CLIUriLen(c); i++) {
                        uri uv = {};
                        (void)CLIUriAt(&uv, c, i);
                        uri *u = &uv;
                        if (u8csEmpty(u->query)) continue;
                        if (!u8csEmpty(u->scheme) &&
                            DOGIsProjector(u->scheme)) continue;
                        //  Skip transport/remote URIs.  `ssh://h?ref`
                        //  expresses remote intent — the local REFS
                        //  row is a stale cache of last-fetched tip;
                        //  pinning here would prevent keeper from
                        //  re-contacting the wire.  Keeper has its own
                        //  freshness logic for transport URIs.  This
                        //  also covers the SCHEME-ONLY form (`ssh:` /
                        //  `file:` — authority absent, GET-002 part 2):
                        //  its `?ref` is the REMOTE want, not a local
                        //  branch to pin to a `/project/branch/sha`.
                        if (!u8csEmpty(u->authority)) continue;
                        if (!u8csEmpty(u->scheme) &&
                            DOGIsTransport(u->scheme)) continue;
                        u8c *before = *u8bIdle(resolve_scratch);
                        u8cs given = {};
                        u8csMv(given, u->data);
                        ok64 rr = GRAFResolveVersion(
                            u8bIdle(resolve_scratch), given);
                        if (rr != OK) continue;
                        u8c *after = *u8bIdle(resolve_scratch);
                        //  Persist the resolved text back as the raw arg
                        //  (URI-004); resolve_scratch outlives this frame.
                        u8cs resolved = {before, after};
                        CLIUriSetRaw(c, i, resolved);
                    }
                    if (go == OK) (void)GRAFClose();
                }
                if (ko == OK) (void)KEEPClose();
            }
            HOMEClose(&rh);
        }
    }

    //  Top-of-chain path canonization pass.  Mirror of the query
    //  resolver above (https://replicated.wiki/html/wiki/URI.html §"URI structure": every input path
    //  shape resolves to a project-relative form).  Sub-dogs receive
    //  canonic paths and never re-walk cwd.
    //
    //  Skipped for URIs with an authority (`ssh://host/path` — path
    //  is server-side) and for inputs whose lex doesn't even find a
    //  path slot.  Projector schemes (`tree:`, `blob:`, …) are NOT
    //  skipped: their path is wt-rel just like the verb forms.
    //
    //  Buffer is sized for the worst case — every URI fully rewritten.
    //  The rewritten bytes live in `path_scratch` for becli's full
    //  frame (covers BEExecute → BEBuildArgv hand-off).
    a_pad(u8, path_scratch, MAX_URI_LEN * CLI_MAX_URIS);
    if (u8bHasData(c->repo) && CLIUriLen(c) > 0) {
        a_path(cwdbuf);
        if (FILEGetCwd(cwdbuf) == OK) {
            a_dup(u8c, cwd_s, u8bDataC(cwdbuf));
            a_dup(u8c, wt_s,  u8bDataC(c->repo));
            for (u32 i = 0; i < CLIUriLen(c); i++) {
                //  Lex the raw arg text so component slices reflect the
                //  current (post-query-canon) bytes.  URILexer CONSUMES
                //  `data`, so capture component slices off the local
                //  `lexed`; we point the entry at the rewritten bytes.
                u8cs raw = {};
                CLIUriRawAt(raw, c, i);
                uri lexed = {};
                u8csMv(lexed.data, raw);
                if (URILexer(&lexed) != OK) continue;
                if (u8csEmpty(lexed.path))           continue;
                if (!u8csEmpty(lexed.authority))     continue;
                //  A TRANSPORT-scheme URI's path is the store / remote
                //  location (`file:<store>`, `ssh:host/path`), NOT a
                //  wt-relative file to canonicalize — leave it intact so
                //  the transport/clone plan reads the full path.  (Projector
                //  schemes like `tree:`/`blob:` keep their wt-relative path
                //  and ARE canonicalized, per the note above.)
                if (!u8csEmpty(lexed.scheme)
                    && DOGIsTransport(lexed.scheme)) continue;

                a_path(rel);
                if (be_canon_path(rel, cwd_s, wt_s, lexed.path) != OK)
                    continue;
                a_dup(u8c, rel_s, u8bDataC(rel));
                if (u8csEq(rel_s, lexed.path)) continue;  // unchanged

                //  Splice the new path back into the URI shape; keep
                //  every other component.  Sub-dogs re-lex from data.
                u8csMv(lexed.path, rel_s);
                u8c *before = *u8bIdle(path_scratch);
                if (URIutf8Feed(u8bIdle(path_scratch), &lexed) != OK)
                    continue;
                u8c *after = *u8bIdle(path_scratch);
                //  Persist the canonical path text back (URI-004);
                //  path_scratch outlives this frame.
                u8cs canon = {before, after};
                CLIUriSetRaw(c, i, canon);
            }
        }
    }

    //  Read the wt's tip URI (`<root>?<branch>#<sha>`) once, here at
    //  the top of the call chain, and stash it for `BEBuildArgv` to
    //  forward to every sub-dog as `--at <uri>`.  Sub-dogs that need
    //  to know the worktree's current branch / commit (sniff bare
    //  `get` resume, keeper `get //origin` default branch, graf `log`
    //  / `map` "you are here") read it back via `CLIAtURI` from
    //  their own cli.flags — no more sub-dog poking at `.be/wtlog`.
    //  `c.repo` is the cwd-walked wt root resolved by `CLIParse`.
    //  Absent / empty `.be/wtlog` (fresh dir, pre-clone bootstrap) →
    //  buffer stays empty and no `--at` flag is forwarded.
    if (u8bHasData(c->repo)) {
        u8bReset(be_at_buf);
        (void)SNIFFAtTailOf($path(c->repo), be_at_buf);
    }

    // No args → default
    if ($empty(c->verb) && CLIUriLen(c) == 0 && u8csbDataLen(c->flags) == 0) {
        call(BEDefault, c);
        done;
    }

    // Classify verb — HTTP verbs only.  Projections (diff, status, log,
    // ls, blame, …) take the bareword-projector route below.
    a_cstr(v_head,   "head");
    a_cstr(v_get,    "get");    a_cstr(v_put,    "put");
    a_cstr(v_post,   "post");   a_cstr(v_delete, "delete");
    a_cstr(v_patch,  "patch");

    u8cs verb = {};
    $mv(verb, c->verb);

    // Get first URI if available (parse-on-demand; the transient lives
    // at function scope so `u` is valid through the routing below — its
    // component slices view into c->uris's stable raw text).
    uri u0v = {};
    uri *u = NULL;
    if (CLIUriLen(c) > 0) {
        (void)CLIUriAt(&u0v, c, 0);
        u = &u0v;
    }

    //  `--seq` flag is reserved for the dispatch table's parallel-
    //  batch fan-out (DISPATCH.c BEExecute) once it lands.

    //  Projector URIs are pure views (https://replicated.wiki/html/wiki/Invariants.html Invariant 7).  Route
    //  them through BEProjector regardless of verb — `be get diff:f?r`
    //  must land in graf's diff machinery, not in BE_PLAN_GET's
    //  keeper+sniff checkout pipeline.  GET is the canonical projector verb per
    //  https://replicated.wiki/html/wiki/Verbs.html, but the table only specifies the read-only intent;
    //  any verb on a projector URI is treated as GET-equivalent here.
    if (u != NULL && DOGIsProjector(u->scheme)) {
        call(BEProjector, c, u);
        done;
    }

    //  Bareword-as-projector: `be <proj>` where the leading non-flag
    //  token names a known projector scheme (status, diff, log, ls,
    //  blame, cat, map, spot, grep, …) is shorthand for `be <proj>:`.
    //  Projections are not verbs (https://replicated.wiki/html/wiki/Projector.html §"View projectors"); they
    //  never live in BE_VERB_NAMES, so CLIParse parks the bareword as
    //  the first raw URI arg (parsed on demand into `u->path`).  We
    //  detect that shape here, synthesise the matching URI, and route
    //  through BEProjector — keeping the dispatch path single-sourced.
    //  Examples:
    //      be status            → status:
    //      be log               → log:
    //      be diff foo.c?main   → diff:foo.c?main   (foo.c?main is the
    //                              second raw URI arg and rides along)
    if ($empty(verb) && u != NULL && u8csEmpty(u->scheme) &&
        !u8csEmpty(u->path) && DOGIsProjector(u->path)) {
        a_pad(u8, syn_buf, 4096);
        a_dup(u8c, ps, u->path);
        (void)u8bFeed(syn_buf, ps);
        (void)u8bFeed1(syn_buf, ':');
        //  Trailing argv tokens after the projector name (`be log
        //  path/to/file?ref`) are the 2nd+ raw URI args — fold the first
        //  one in as the projector body.
        if (CLIUriLen(c) > 1) {
            u8cs raw1 = {};
            CLIUriRawAt(raw1, c, 1);
            if (!$empty(raw1)) {
                a_dup(u8c, us, raw1);
                (void)u8bFeed(syn_buf, us);
            }
        }
        uri synthetic = {};
        a_dup(u8c, syn_text, u8bDataC(syn_buf));
        call(DOGParseURI, &synthetic, syn_text);
        //  URILexer consumes `data` while parsing — repoint it at the
        //  full URI bytes so BEProjector can forward the original text
        //  to the sub-dog (it uses u->data as the argv URI slot).
        u8csMv(synthetic.data, syn_text);
        call(BEProjector, c, &synthetic);
        done;
    }

    // No verb → view/file mode.  Projector schemes (spot:, grep:, regex:,
    // ls:, tree:, …) were already routed through BEProjector above; here
    // a bare path-shaped URI displays the file via bro.  Search has no
    // implicit `#frag` form anymore — use `be spot:body`, `be grep:body`,
    // `be regex:body` (https://replicated.wiki/html/wiki/Projector.html §"View projectors").
    if ($empty(verb)) {
        u8cs bro  = u8slit("bro");
        if (u != NULL && !$empty(u->path)) {
            //  Mirror BEProjector: forward the resolved mode flag so
            //  bro picks the same shape `be` resolved (no env hack;
            //  --color / --plain / --tlv straight on bro's argv).
            a_cstr(tlv_flag_s,   "--tlv");
            a_cstr(color_flag_s, "--color");
            a_cstr(plain_flag_s, "--plain");
            u8cs tlv_flag   = {tlv_flag_s[0],   tlv_flag_s[1]};
            u8cs color_flag = {color_flag_s[0], color_flag_s[1]};
            u8cs plain_flag = {plain_flag_s[0], plain_flag_s[1]};
            a_pad(u8cs, args, 3);
            u8csbFeed1(args, bro);
            if      (HUNKMode == HUNKOutTLV)   u8csbFeed1(args, tlv_flag);
            else if (HUNKMode == HUNKOutColor) u8csbFeed1(args, color_flag);
            else if (HUNKMode == HUNKOutPlain) u8csbFeed1(args, plain_flag);
            u8csbFeed1(args, u->data);
            a_dup(u8cs, argv, u8csbData(args));
            call(BERun, bro, argv, NO);
        } else {
            call(BEDefault, c);
        }
    } else if ($eq(verb, v_head)) {
        call(BEExecute, c, BE_PLAN_HEAD);
    } else if ($eq(verb, v_get)) {
        //  Route `be get file:<git-repo>` through the transport/clone
        //  plan (like ssh://) before the pattern is computed; a `file:`
        //  beagle store stays on the sibling-worktree plan.
        be_file_get_route(c);
        call(BEExecute, c, BE_PLAN_GET);
    } else if ($eq(verb, v_post)) {
        call(BEExecute, c, BE_PLAN_POST);
    } else if ($eq(verb, v_put)) {
        call(BEExecute, c, BE_PLAN_PUT);
    } else if ($eq(verb, v_delete)) {
        call(BEExecute, c, BE_PLAN_DELETE);
    } else if ($eq(verb, v_patch)) {
        call(BEExecute, c, BE_PLAN_PATCH);
    } else {
        fprintf(stderr, "be: verb '" U8SFMT "' not yet implemented\n",
                u8sFmt(verb));
    }

    done;
}

ok64 becli() {
    sane(1);
    cli c = {};
    call(PATHu8bAlloc, c.repo);
    call(u8csbAlloc, c.flags, CLI_MAX_FLAGS * 2);
    call(u8csbAlloc, c.uris,  CLI_MAX_URIS);
    try(becli_inner, &c);
    ok64 ret = __;
    //  `-q` / `--quiet` swallows any `*NONE` (POSTNONE / PUTNONE /
    //  DELNONE — all suffix-match NONE), a no-op signal rather than a
    //  real error, so the submodule recursion (bepost_spawn_sub,
    //  BEActSubsRelay) passes `-q` to every sub-shard and gets clean
    //  output / exit 0 for shards with no changes.
    if (ok64is(ret, NONE) &&
        (CLIHas(&c, "-q") || CLIHas(&c, "--quiet")))
        ret = OK;
    u8csbFree(c.flags);
    u8csbFree(c.uris);
    PATHu8bFree(c.repo);
    return ret;
}

MAIN(becli);
