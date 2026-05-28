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
#include "dog/HOME.h"
#include "dog/THEME.h"
#include "dog/ULOG.h"
#include "graf/GRAF.h"            // GRAFResolveVersion (top-of-chain pass)
#include "keeper/KEEP.h"           // KEEPOpenBranch (resolver needs refs)
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

//  HTTP-verb dictionary (VERBS.md §"Verb semantics"): HEAD, GET, POST,
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
        "Verbs (VERBS.md):\n"
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
        "  //host  = cached remote-tracking refs only (no network)\n"
        "  ssh:    = open a wire (clone, fetch, push)\n"
        "\n"
        "Bare `be` = status (current branch, ahead/behind, dirty).\n"
    );
}

// --- Run a sibling tool ---

// Run a sibling tool.  `tool` is the dog name (also argv[0] in argv);
// resolved against this process's own argv[0] via HOMEResolveSibling.
static ok64 be_url_project(uricp u, u8csp out);
static ok64 be_sub_shard_setup(cli *c, uri *u);

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

// --- Run two tools as a producer → pager pipeline ---
//
//  Used to route view-projector output (e.g. `sniff ls --tlv`) into
//  `bro` for paging and coloring.  Spawns both children, pumps bytes
//  from producer's stdout into pager's stdin in the parent, then
//  reaps both.  Returns the worse of the two exit codes.
static ok64 BERunPipe(path8sc prod, u8css prod_argv,
                      path8sc pager, u8css pager_argv) {
    sane($ok(prod) && $ok(pager));

    //  Shell-style `producer | pager` pipeline: one pipe(2), dup2 it
    //  to producer's stdout and pager's stdin in the respective
    //  children, parent closes both ends and waitpids.  The kernel
    //  buffer handles flow control — no parent-side pump.  POSIX
    //  primitives only (Linux, BSD, macOS).
    //
    //  `FD_CLOEXEC` on the raw pipe fds covers the inherited-fd
    //  problem: each child gets a dup of *both* pipe ends, but only
    //  one of them is dup2'd onto stdin/stdout (which clears CLOEXEC
    //  on the target).  The other inherited copy auto-closes at exec,
    //  so pipe ref counts collapse correctly and EOF propagates the
    //  moment the producer exits.  Without CLOEXEC the unused end
    //  lingers in the sibling child and the reader hangs.
    int p[2] = {-1, -1};
    if (pipe(p) != 0) failc(BEFAIL);
    (void)fcntl(p[0], F_SETFD, FD_CLOEXEC);
    (void)fcntl(p[1], F_SETFD, FD_CLOEXEC);

    pid_t prod_pid = 0;
    ok64 ps = FILESpawnFds(prod, prod_argv, -1, p[1], &prod_pid);
    if (ps != OK) { close(p[0]); close(p[1]); return ps; }

    pid_t pager_pid = 0;
    ok64 gs = FILESpawnFds(pager, pager_argv, p[0], -1, &pager_pid);
    if (gs != OK) {
        //  Producer started but pager didn't.  Close the pipe ends
        //  so producer sees EPIPE and exits; reap it.
        close(p[0]); close(p[1]);
        int rc = 0; (void)FILEReap(prod_pid, &rc);
        return gs;
    }

    //  Parent's copies must go so the kernel pipe ref count drops to
    //  the children only (CLOEXEC handled the children's *unused*
    //  inherited copies above).
    close(p[0]);
    close(p[1]);

    int prod_rc = 0, pager_rc = 0;
    (void)FILEReap(prod_pid,  &prod_rc);
    (void)FILEReap(pager_pid, &pager_rc);
    if (prod_rc  != 0) return BEDOGEXIT;
    if (pager_rc != 0) return BEDOGEXIT;
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
    for (u32 j = 0; j < uribDataLen(c->uris); j++)
        u8csbFeed1(args, uribAtP(c->uris, j)->data);
}

//  `wt_uri_buf` is caller-owned scratch (≥ 64 B) backing the rewritten
//  `?<hashlet>` URI bytes — must outlive this frame so downstream argv
//  build can read `u->data` (CLAUDE.md §5).
ok64 BEGetWorktree(uri *u, u8b wt_uri_buf) {
    sane(1);
    if (u == NULL || !u8csEmpty(u->authority)) done;
    if (u8csEmpty(u->path)) done;

    // Primary candidate has to be an existing dir containing .be/.
    a_dup(u8c, prim_s, u->path);
    a_path(prim_be, prim_s, DOG_BE_S);
    if (FILEisdir($path(prim_be)) != OK) done;

    // Skip if cwd already has a .be (dir, symlink, or wtlog file).
    a_path(cwd);
    call(FILEGetCwd, cwd);
    a_path(cwd_be);
    a_dup(u8c, cwd_s, u8bDataC(cwd));
    call(PATHu8bFeed, cwd_be, cwd_s);
    call(PATHu8bPush, cwd_be, DOG_BE_S);
    {
        filestat fs = {};
        if (FILELStat(&fs, $path(cwd_be)) == OK) done;
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

    //  URI path: `<prim>/.be/[<proj>/]` with a trailing slash so the
    //  row-0 invariant (`file://…/.be/<proj>/`) holds.  Anchor write
    //  itself is sniff's job — same helper BEEnsureProjectRepo
    //  uses for the primary-wt layout.
    {
        a_path(repo_path, prim_s);
        call(PATHu8bPush, repo_path, DOG_BE_S);
        if (u8bDataLen(prim_proj) > 0) {
            a_dup(u8c, pp, u8bDataC(prim_proj));
            call(PATHu8bPush, repo_path, pp);
        }
        call(u8bFeed1, repo_path, '/');
        call(SNIFFWtRepoAnchor,
             u8bDataC(cwd_be), u8bDataC(repo_path));
    }

    fprintf(stderr, "be: worktree from %.*s\n",
            (int)$len(u->path), (char *)u->path[0]);

    // Resolve the primary's current commit via its wtlog.  Rewrite
    // this URI to "?<sha>" so downstream sniff checks out that commit
    // in the worktree.  NODATA is fine (primary never posted — leave
    // the URI alone and let sniff resolve from the primary's keeper);
    // real errors propagate.
    a_pad(u8, prim_at, FILE_PATH_MAX_LEN + 128);
    a_dup(u8c, prim_root, prim_s);
    mute(SNIFFAtTailOf(prim_root, prim_at), NODATA);
    if (u8bDataLen(prim_at) == 0) done;
    uri prim_uri = {};
    u8csMv(prim_uri.data, u8bDataC(prim_at));
    call(URILexer, &prim_uri);
    //  Hashlet: 6..40 hex chars (full sha1 = 40, prefix abbreviations
    //  shorter).  Anything outside that range is not a valid pin.
    size_t flen = u8csLen(prim_uri.fragment);
    if (flen < 6 || flen > 40) done;

    //  Compose "?<hashlet>" into the static buffer; expose data and
    //  query slices into it (other URI components stay empty).
    u8bReset(wt_uri_buf);
    call(u8bFeed1, wt_uri_buf, '?');
    call(u8bFeed,  wt_uri_buf, prim_uri.fragment);

    //  Downstream (`BEBuildArgv`) reads only `u->data` — `u->query`
    //  rewrite was dead code.  Point `data` at the buffer's bytes.
    zerop(u);
    u8csMv(u->data, u8bDataC(wt_uri_buf));
    done;
}

//  View-projector routing (VERBS.md §"View projectors").
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

    if (!tty) return BERun(dog_s, dargv, NO);

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
    return BERunPipe($path(dogpath), dargv, $path(bropath), bargv);
}


//  `be head <uri>` — peek/dry-run.  Per VERBS.md §"HEAD":
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
        //  Local recursion: forward parent's argv verbatim.
        u8css argv = {rc->argv_head, rc->argv_term};
        ok64 r = BERecurseInto(rc->wt_root, subpath, argv);
        if (r != OK) rc->worst = r;
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

    ok64 r = BERecurseInto(rc->wt_root, subpath, child_argv);
    if (r != OK) rc->worst = r;
    return OK;
}

//  HEAD's pre-order submodule recursion — extracted from the old
//  `BEHead` wrapper to fit the be_action signature.  Called once
//  per `be head` invocation (table marks the row as `once`).  The
//  local body (`graf head` / `keeper get` / spot+graf re-walk) is
//  driven by the preceding table rows; this function only walks
//  declared subs.  See VERBS.md / SUBS.plan.md §HEAD.
ok64 BEHeadSubs(cli *c) {
    sane(c);

    //  Need a wt root to enumerate / chdir from.
    if (!u8bHasData(c->repo)) done;
    a_dup(u8c, wt_root, $path(c->repo));

    //  Detect transport mode: parent invoked with `ssh://` etc.  In
    //  that case the recursion swaps the URL per-sub (each sub
    //  fetches its OWN remote) — see head/07.  Local mode
    //  (`?ref`, bare `?`, cached `//host`) just forwards verbatim.
    b8 transport = (uribDataLen(c->uris) > 0 && !u8csEmpty(uribAtP(c->uris, 0)->scheme));
    u8cs forwarded_query = {};
    if (transport) u8csMv(forwarded_query, uribAtP(c->uris, 0)->query);

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
        for (u32 j = 0; j < uribDataLen(c->uris); j++)
            u8csbFeed1(child_args, uribAtP(c->uris, j)->data);
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
//  (BEActKeeperGet etc.) read `uribAtP(c->uris, 0)->data`, which
//  BEGetWorktree may have repointed into this buffer.
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
    uri *u0 = (uribDataLen(c->uris) > 0) ? uribAtP(c->uris, 0) : NULL;
    if (u0 == NULL) done;
    u8bReset(beget_wt_uri_buf);
    return BEGetWorktree(u0, beget_wt_uri_buf);
}

ok64 BEActSingleFileGet(cli *c) {
    sane(c);
    //  Path+query (no authority) on the first URI is a one-file
    //  overwrite — bypass spot/graf/sniff-index and route only to
    //  sniff get, which fetches the blob via keeper and rewrites
    //  the wt file without appending a `get` row.  The aggregate
    //  gate already passed; double-check the first URI to skip
    //  edge-case multi-URI invocations (rare).
    uri *u0 = (uribDataLen(c->uris) > 0) ? uribAtP(c->uris, 0) : NULL;
    if (u0 == NULL)             done;
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
    } else if (uribDataLen(c->uris) > 0 && !u8csEmpty(uribAtP(c->uris, 0)->query)) {
        (void)u8bFeed(target_ref_buf, uribAtP(c->uris, 0)->query);
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
                                          (u8cs *)flag_view[1]);
        if (sub_worst != OK) worst = sub_worst;
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
    if (u8csEmpty(u->query)) return SUBSPARSE;
    a_dup(u8c, q, u->query);
    if (*q[0] != '/') return SUBSPARSE;
    u8csUsed1(q);
    u8c const *end = q[0];
    while (end < q[1] && *end != '/') end++;
    if (end == q[0]) return SUBSPARSE;
    u8cs proj_slice = {q[0], (u8c *)end};
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
    //  carries a host (`be get ssh://h/p?...` etc.).  STORE.md §"Repo
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
    call(SNIFFWtRepoAnchor, u8bDataC(wtlog_path), u8bDataC(repo_path));

    done;
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
    a_path(store_root_buf);
    {
        home rh = {};
        uri none = {};
        if (HOMEOpen(&rh, &none, NO) == OK && u8bHasData(rh.root)) {
            a_dup(u8c, rs, u8bDataC(rh.root));
            call(PATHu8bFeed, store_root_buf, rs);
        } else {
            call(PATHu8bFeed, store_root_buf, repo_s);
        }
        HOMEClose(&rh);
    }
    a_dup(u8c, store_root_s, u8bDataC(store_root_buf));

    //  mkdir <store>/.be/<basename>/  (flat: this dir IS the sub's
    //  store, same shape as <store>/.be/ itself)
    a_path(shard_be);
    call(PATHu8bFeed, shard_be, store_root_s);
    call(PATHu8bPush, shard_be, DOG_BE_S);
    a_dup(u8c, base_s, basename);
    call(PATHu8bPush, shard_be, base_s);
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
        a_path(sub_store);
        call(PATHu8bFeed, sub_store, store_root_s);
        call(PATHu8bPush, sub_store, DOG_BE_S);
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
//  PUT is the ref-writer (VERBS.md §"PUT"); per the URI's `//remote`
//  slot it doubles as the FF-push verb (keeper's old `post` entry).
//  Local shapes (label move, file staging, sha reset) stay in sniff
//  put.  DELETE is its mirror.

//  Ref-expecting verbs (post, patch) accept a path-shaped argument as
//  the ref — `be get feat/sub` → query="feat/sub".  Bareword promotion
//  is centralised in DOGPromoteBareword (per VERBS.md §"Bareword
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
//  scheme) → graf get (any remote) → sniff patch.  See VERBS.md
//  §PATCH for semantics.

//  `be post` — commit and/or fast-forward (never rebase; see VERBS.md
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
    for (u32 i = 0; i < uribDataLen(c->uris); i++) {
        uri *u = uribAtP(c->uris, i);
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
//  POST returns OK: BEActSniffPost then reads that row and emits the
//  gitlink ADD in the parent commit.  A clean sub (tip == baseline)
//  just gets re-stamped — no decision row, no parent bump.
static ok64 bepost_bump_sub(u8cs subpath) {
    sane($ok(subpath));

    //  Resolve self so the child's argv[0] is the full self path —
    //  HOMEResolveSibling needs that to find the sibling `sniff`,
    //  `keeper`, etc.  Matches BERecurseInto's convention.
    a_path(self_path);
    {
        char buf[FILE_PATH_MAX_LEN];
        ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
        if (n <= 0) {
            fprintf(stderr, "be: post: bump %.*s: cannot resolve self\n",
                    (int)$len(subpath), (char *)subpath[0]);
            return BEDOGEXIT;
        }
        buf[n] = 0;
        a_cstr(buf_s, buf);
        call(PATHu8bFeed, self_path, buf_s);
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
    //  HOMEResolveSibling finds the right bin dir.  Mirrors
    //  BERecurseInto's convention.
    a_path(self_path);
    {
        char buf[FILE_PATH_MAX_LEN];
        ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
        if (n <= 0) {
            fprintf(stderr, "be: post: %.*s: cannot resolve self\n",
                    (int)$len(subpath), (char *)subpath[0]);
            return BEDOGEXIT;
        }
        buf[n] = 0;
        a_cstr(buf_s, buf);
        call(PATHu8bFeed, self_path, buf_s);
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
    char *argv_c[MAX_ARGS + 1];
    for (u32 i = 0; i < nargs; i++) {
        argv_c[i] = (char *)(u8bDataHead(pool) + offs[i]);
    }
    argv_c[nargs] = NULL;
    char const *mount_cstr = (char const *)u8bDataHead(mount);

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "be: post: %.*s: fork failed: %s\n",
                (int)$len(subpath), (char *)subpath[0], strerror(errno));
        return BEDOGEXIT;
    }
    if (pid == 0) {
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

    int rc = 0;
    ok64 wo = FILEReap(pid, &rc);
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
    size_t nuris = uribDataLen(rc->c->uris);
    for (size_t j = 0; j < nuris; j++) {
        uri *u = uribAtP(rc->c->uris, j);
        b8 pure_frag = !u8csEmpty(u->fragment) &&
                       u8csEmpty(u->scheme) &&
                       u8csEmpty(u->authority) &&
                       u8csEmpty(u->path) &&
                       u8csEmpty(u->query);
        if (pure_frag) continue;
        u8csbFeed1(child_args, u->data);
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

    //  Sub returned OK with a real commit.  Fork `be put <subpath>` in
    //  the parent so the parent's wtlog gets a `put <subpath>#<40-hex>`
    //  row at the new tip; BEActSniffPost picks that up and emits the
    //  gitlink ADD in the parent commit.
    ok64 br = bepost_bump_sub(subpath);
    if (br != OK) rc->worst = br;
    return OK;
}



//  --- POST action bodies (Stage 5 migration) -----------------------

//  Pre-order submodule recursion: each sub commits before the
//  parent.  Self-gates on bare-status / transport / no-wt-root so
//  the recursion only fires when it makes sense.  Returns BESTOP
//  in --dry-run mode after emitting the parent's status, so the
//  subsequent BEActSniffPost / BEActKeeperPush / BEActReindex rows
//  don't run.
ok64 BEActSubsPost(cli *c) {
    sane(c);

    //  Detect transport / msg shape from URIs and -m flag.  No URI
    //  mutation here — BEActPromoteRef already ran above us in the
    //  table.
    b8 has_transport = NO;
    for (u32 i = 0; i < uribDataLen(c->uris); i++) {
        if (!u8csEmpty(uribAtP(c->uris, i)->scheme)) has_transport = YES;
    }
    b8 has_msg = NO;
    for (u32 i = 0; i < uribDataLen(c->uris); i++) {
        if (!u8csEmpty(uribAtP(c->uris, i)->fragment)) { has_msg = YES; break; }
    }
    if (!has_msg) {
        a_cstr(mf, "-m");
        for (u32 fi = 0; fi + 1 < u8csbDataLen(c->flags); fi += 2) {
            if ($eq((*u8csbAtP(c->flags, fi)), mf)) { has_msg = YES; break; }
        }
    }

    b8 dry_only   = CLIHas(c, "--dry-run") ? YES : NO;
    b8 bare_status = !has_msg && uribDataLen(c->uris) == 0;

    //  Skip recursion on transport (explicit URL → single project)
    //  or bare-status (no commit work to recurse into).  Dry-only
    //  always recurses (it's the audit pass).
    if (!dry_only && (bare_status || has_transport)) done;

    //  Need a wt root to enumerate / chdir from.
    if (!u8bHasData(c->repo)) done;
    a_dup(u8c, wt_root, $path(c->repo));

    //  Effective parent msg for sub argv composition.
    u8cs parent_msg = {};
    for (u32 i = 0; i < uribDataLen(c->uris); i++) {
        if (!u8csEmpty(uribAtP(c->uris, i)->fragment)) {
            u8csMv(parent_msg, uribAtP(c->uris, i)->fragment);
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
    for (u32 i = 0; i < uribDataLen(c->uris); i++) {
        if (!u8csEmpty(uribAtP(c->uris, i)->fragment)) { has_msg = YES; break; }
    }
    if (!has_msg) {
        a_cstr(mf, "-m");
        for (u32 fi = 0; fi + 1 < u8csbDataLen(c->flags); fi += 2) {
            if ($eq((*u8csbAtP(c->flags, fi)), mf)) { has_msg = YES; break; }
        }
    }
    b8 dry_run = !has_msg && uribDataLen(c->uris) == 0;
    if (dry_run) done;
    (void)be_reindex(c);
    done;
}

// --- Bare `be`: --update all dogs, then --status each ---

//  Canonize a user-typed file path slice into wt-relative form.
//  Handles cwd-relative inputs (`README`, `sub/file.c`, `./foo`,
//  `../sib`) and absolute inputs (`/home/me/proj/README`).
//
//  STORE.md §"URI structure" pins the path slot to "for all other
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

//  Bare `be` — overview of the working tree.  Forwards to bare
//  `sniff`, which lists Changed: and Untracked: against the baseline
//  tree (untracked-but-gitignored filtered).  spot / graf / keeper
//  dogs aren't surfaced here — they're index/storage layers without
//  user-relevant state to print.  Adding their summaries back is a
//  one-liner per dog if it ever matters.
static ok64 BEDefault(void) {
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
    return BERun(sniff_s, argv, NO);
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
    //  pipe through bro (BERunPipe) inherit env unchanged, so the
    //  pager picks up the same palette.  Unknown values → hard fail.
    char const *theme_name = NULL;
    if (CLIHas(c, "--16"))    theme_name = THEME_16;
    if (CLIHas(c, "--dark"))  theme_name = THEME_DARK;
    if (CLIHas(c, "--light")) theme_name = THEME_LIGHT;
    if (theme_name != NULL) call(THEMESelect, theme_name);

    //  Per-verb bareword default (VERBS.md §"Bareword defaults"):
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
            for (u32 i = 0; i < uribDataLen(c->uris); i++) {
                uri *u = uribAtP(c->uris, i);
                u8cs orig_path = {u->path[0], u->path[1]};

                //  GET path-sniff (VERBS.md §"Bareword defaults"): a
                //  bareword that names a file present on disk in the
                //  cwd stays in the path slot (single-file restore
                //  form, `be get file.c` ≡ `be get ./file.c`).
                //  Deleted-file restore needs the explicit `./` form
                //  since stat misses it.  Skips promotion entirely.
                if (is_get && !u8csEmpty(orig_path)) {
                    a_path(probe);
                    if (PATHu8bFeed(probe, orig_path) == OK) {
                        filestat fs = {};
                        if (FILEStat(&fs, $path(probe)) == OK) continue;
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
                u->data[0] = before;
                u->data[1] = after;
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
            uribDataLen(c->uris) > 0)
            (void)be_sub_shard_setup(c, uribAtP(c->uris, 0));
    }

    //  Top-of-chain version resolver pass.  Every user-supplied URI
    //  with a non-empty `?query` runs through `GRAFResolveVersion`
    //  (graf/GRAF.h) so downstream dogs see only the canonic
    //  `?/<project>.<hashlet>/<branch>/<sha-or-tag>` form
    //  (STORE.md §"URI structure").  Projector schemes own their
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
        && u8bHasData(c->repo) && uribDataLen(c->uris) > 0) {
        home rh = {};
        uri none = {};
        if (HOMEOpen(&rh, &none, NO) == OK) {
            static u8c const _zero = 0;
            u8cs trunk = {&_zero, &_zero};
            ok64 ko = KEEPOpenBranch(&rh, trunk, NO);
            if (ko == OK || ko == KEEPOPEN || ko == KEEPOPENRO) {
                ok64 go = GRAFOpen(&rh, NO);
                if (go == OK || go == GRAFOPEN || go == GRAFOPENRO) {
                    for (u32 i = 0; i < uribDataLen(c->uris); i++) {
                        uri *u = uribAtP(c->uris, i);
                        if (u8csEmpty(u->query)) continue;
                        if (!u8csEmpty(u->scheme) &&
                            DOGIsProjector(u->scheme)) continue;
                        //  Skip transport/remote URIs.  `ssh://h?ref`
                        //  expresses remote intent — the local REFS
                        //  row is a stale cache of last-fetched tip;
                        //  pinning here would prevent keeper from
                        //  re-contacting the wire.  Keeper has its own
                        //  freshness logic for transport URIs.
                        if (!u8csEmpty(u->authority)) continue;
                        u8c *before = *u8bIdle(resolve_scratch);
                        u8cs given = {};
                        u8csMv(given, u->data);
                        ok64 rr = GRAFResolveVersion(
                            u8bIdle(resolve_scratch), given);
                        if (rr != OK) continue;
                        u8c *after = *u8bIdle(resolve_scratch);
                        u->data[0] = before;
                        u->data[1] = after;
                    }
                    if (go == OK) (void)GRAFClose();
                }
                if (ko == OK) (void)KEEPClose();
            }
            HOMEClose(&rh);
        }
    }

    //  Top-of-chain path canonization pass.  Mirror of the query
    //  resolver above (STORE.md §"URI structure": every input path
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
    if (u8bHasData(c->repo) && uribDataLen(c->uris) > 0) {
        a_path(cwdbuf);
        if (FILEGetCwd(cwdbuf) == OK) {
            a_dup(u8c, cwd_s, u8bDataC(cwdbuf));
            a_dup(u8c, wt_s,  u8bDataC(c->repo));
            for (u32 i = 0; i < uribDataLen(c->uris); i++) {
                uri *u = uribAtP(c->uris, i);
                //  Re-lex `u->data` so component slices reflect the
                //  current (post-query-canon) bytes.  URILexer
                //  CONSUMES `data`, so capture component slices off
                //  the local `lexed` and leave `u->data` alone for
                //  now — we'll point it at the rewritten bytes below.
                uri lexed = {};
                u8csMv(lexed.data, u->data);
                if (URILexer(&lexed) != OK) continue;
                if (u8csEmpty(lexed.path))           continue;
                if (!u8csEmpty(lexed.authority))     continue;

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
                u->data[0] = before;
                u->data[1] = after;
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
    if ($empty(c->verb) && uribDataLen(c->uris) == 0 && u8csbDataLen(c->flags) == 0) {
        call(BEDefault);
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

    // Get first URI if available
    uri *u = (uribDataLen(c->uris) > 0) ? uribAtP(c->uris, 0) : NULL;

    //  `--seq` flag is reserved for the dispatch table's parallel-
    //  batch fan-out (DISPATCH.c BEExecute) once it lands.

    //  Projector URIs are pure views (VERBS.md Invariant 7).  Route
    //  them through BEProjector regardless of verb — `be get diff:f?r`
    //  must land in graf's diff machinery, not in BE_PLAN_GET's
    //  keeper+sniff checkout pipeline.  GET is the canonical projector verb per
    //  VERBS.md, but the table only specifies the read-only intent;
    //  any verb on a projector URI is treated as GET-equivalent here.
    if (u != NULL && DOGIsProjector(u->scheme)) {
        call(BEProjector, c, u);
        done;
    }

    //  Bareword-as-projector: `be <proj>` where the leading non-flag
    //  token names a known projector scheme (status, diff, log, ls,
    //  blame, cat, map, spot, grep, …) is shorthand for `be <proj>:`.
    //  Projections are not verbs (VERBS.md §"View projectors"); they
    //  never live in BE_VERB_NAMES, so CLIParse parks the bareword in
    //  `uribAtP(c->uris, 0)->path`.  We detect that shape here, synthesise the
    //  matching URI, and route through BEProjector — keeping the
    //  dispatch path single-sourced.  Examples:
    //      be status            → status:
    //      be log               → log:
    //      be diff foo.c?main   → diff:foo.c?main   (foo.c?main lands
    //                              in (*uribAtP(c->uris, 1)) and rides along)
    if ($empty(verb) && u != NULL && u8csEmpty(u->scheme) &&
        !u8csEmpty(u->path) && DOGIsProjector(u->path)) {
        a_pad(u8, syn_buf, 4096);
        a_dup(u8c, ps, u->path);
        (void)u8bFeed(syn_buf, ps);
        (void)u8bFeed1(syn_buf, ':');
        //  Trailing argv tokens after the projector name (`be log
        //  path/to/file?ref`) land in (*uribAtP(c->uris, 1..)) — fold the first
        //  one in as the projector body.
        if (uribDataLen(c->uris) > 1 && !$empty(uribAtP(c->uris, 1)->data)) {
            a_dup(u8c, us, uribAtP(c->uris, 1)->data);
            (void)u8bFeed(syn_buf, us);
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
    // `be regex:body` (VERBS.md §"View projectors").
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
            call(BEDefault);
        }
    } else if ($eq(verb, v_head)) {
        call(BEExecute, c, BE_PLAN_HEAD);
    } else if ($eq(verb, v_get)) {
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
    call(uribAlloc,  c.uris,  CLI_MAX_URIS);
    try(becli_inner, &c);
    ok64 ret = __;
    //  `-q` / `--quiet` swallows POSTNONE (a no-op signal, not a
    //  real error) so the outer `be post` recursion's
    //  bepost_spawn_sub passes `-q` to every sub-shard and gets
    //  clean output for shards with no changes.
    if (ret == POSTNONE &&
        (CLIHas(&c, "-q") || CLIHas(&c, "--quiet")))
        ret = OK;
    u8csbFree(c.flags);
    uribFree(c.uris);
    PATHu8bFree(c.repo);
    return ret;
}

MAIN(becli);
