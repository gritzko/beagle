#include "abc/URI.h"
#include "dog/CLI.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/RON.h"
#include "dog/DOG.h"
#include "dog/HOME.h"
#include "dog/QURY.h"
#include "dog/ULOG.h"
#include "keeper/REFS.h"
#include "sniff/AT.h"
#include "sniff/SNIFF.h"          // POSTNONE (low-byte exit signal)
#include "sniff/SUBS.h"

#include "SUBS.h"           // beagle/SUBS.h — BESubsHere / BERecurseInto

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

static char const *const BE_VERB_NAMES[] = {
    "head", "get", "post", "put", "delete", "patch",
    //  Legacy / read-only sub-verbs surfaced as projectors elsewhere
    //  but still parsed here for argv compat:
    "diff", "merge", "sync", "status",
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
static ok64 be_ensure_project_repo(uricp u);
static ok64 be_url_project(uricp u, u8csp out);
static ok64 be_sub_shard_setup(cli *c, uri *u);

static ok64 BERun(u8csc tool, u8css argv, b8 bg) {
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
    //  Plan §POST: sub-recursion treats POSTNONE as a no-op.  The
    //  exit byte 0xCE (POSTNONE's RON60 low byte) is the residue
    //  bepost_spawn_sub inspects one level up — pass it through
    //  instead of collapsing to BEDOGEXIT.  Scope the pass-through
    //  to the `sniff` tool because GRAFNONE happens to share the
    //  same low byte (RON60 encodes the "NONE" suffix identically),
    //  and graf-head's "no match" exit must keep mapping to
    //  BEDOGEXIT for upstream tests.
    if (rc == (int)(POSTNONE & 0xFFu)) {
        a_cstr(s_sniff, "sniff");
        if (u8csEq(tool, s_sniff)) return POSTNONE;
    }
    if (rc != 0) return BEDOGEXIT;
    done;
}

//  Spawn a sibling tool without waiting; caller reaps later via
//  BEReap.  Used by `be get` to run spot/graf/sniff in parallel
//  after keeper completes (DOG.md §10a).
static ok64 BESpawn(u8csc tool, u8css argv, pid_t *out_pid) {
    sane($ok(tool) && !$empty(tool) && out_pid);
    a_path(path);
    a$rg(a0, 0);
    HOMEResolveSibling(NULL, path, tool, a0);
    return FILESpawn($path(path), argv, NULL, NULL, out_pid);
}

//  Wait on a previously-spawned child and translate its exit into
//  the BEDOG* code surface that `BERun` uses.
static ok64 BEReap(pid_t pid, u8csc tool) {
    int rc = 0;
    ok64 r = FILEReap(pid, &rc);
    if (r == FILESIGNAL) {
        char const *sname = strsignal(rc);
        fprintf(stderr, "be: " U8SFMT " killed by signal %d (%s)\n",
                u8sFmt(tool), rc, sname ? sname : "?");
        return BEDOGSIG;
    }
    if (r != OK) return r;
    //  Same sniff-only POSTNONE pass-through as BERun (see comment
    //  there); BEReap is the parallel-reap path used by BEGet.
    if (rc == (int)(POSTNONE & 0xFFu)) {
        a_cstr(s_sniff, "sniff");
        if (u8csEq(tool, s_sniff)) return POSTNONE;
    }
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
    int to_pager_w = -1;
    pid_t pager_pid = 0;
    call(FILESpawn, pager, pager_argv, &to_pager_w, NULL, &pager_pid);

    int from_prod_r = -1;
    pid_t prod_pid = 0;
    ok64 so = FILESpawn(prod, prod_argv, NULL, &from_prod_r, &prod_pid);
    if (so != OK) {
        //  Bro started but we can't produce.  Close its stdin so it
        //  exits on EOF and we can reap cleanly.
        close(to_pager_w);
        int rc = 0; (void)FILEReap(pager_pid, &rc);
        return so;
    }

    //  Parent pump: drain producer → feed pager.  Shallow, no framing;
    //  HUNK TLV is self-framing so any chunk boundary is fine.
    u8 buf[8192];
    for (;;) {
        ssize_t n = read(from_prod_r, buf, sizeof buf);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(to_pager_w, buf + off, (size_t)(n - off));
            if (w < 0) {
                if (errno == EINTR) continue;
                goto drain_done;
            }
            off += w;
        }
    }
drain_done:
    close(from_prod_r);
    close(to_pager_w);

    int prod_rc = 0;
    int pager_rc = 0;
    (void)FILEReap(prod_pid, &prod_rc);
    (void)FILEReap(pager_pid, &pager_rc);
    if (prod_rc != 0) return BEDOGEXIT;
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

typedef struct {
    u8cs dog;
    u8cs verb;
    b8 bg;             // run in background (don't wait)
} dog_step;

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
static void be_build_argv(u8csb args, u8csc dog, u8csc verb, cli *c) {
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
    for (u32 j = 0; j + 1 < c->nflags; j += 2) {
        u8csbFeed1(args, c->flags[j]);
        if (!u8csEmpty(c->flags[j + 1]))
            u8csbFeed1(args, c->flags[j + 1]);
    }
    for (u32 j = 0; j < c->nuris; j++)
        u8csbFeed1(args, c->uris[j].data);
}

static ok64 BEDispatch(cli *c, dog_step const *steps, u32 nsteps,
                        b8 seq) {
    sane(c && steps);
    for (u32 i = 0; i < nsteps; i++) {
        a_pad(u8cs, args, 4 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS);
        be_build_argv(args, steps[i].dog, steps[i].verb, c);
        a_dup(u8cs, argv, u8csbData(args));
        call(BERun, steps[i].dog, argv, seq ? NO : steps[i].bg);
    }
    done;
}

//  `be get <local-dir>` creates a worktree in cwd that shares
//  keeper/graf/spot with the primary repo via symlinks; sniff is
//  real (per-worktree).  Returns OK after setup whether or not any
//  action was taken; only dies on a real error (mkdir/symlink fail).
//  Static storage for the rewritten URI after a worktree is wired up:
//  "?<6..40-hex-hashlet>" points every downstream dog at the primary's
//  HEAD.  Re-filled on each BEGetWorktree call.
static u8  wt_uri_storage[64];
static Bu8 wt_uri_buf = {
    wt_uri_storage, wt_uri_storage,
    wt_uri_storage,
    wt_uri_storage + sizeof(wt_uri_storage)
};

static b8 be_promote_to_ref(uri *u);

static ok64 BEGetWorktree(uri *u) {
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
        a_path(prim_wtlog);
        call(u8bFeed, prim_wtlog, prim_s);
        call(u8bFeed1, prim_wtlog, '/');
        call(u8bFeed, prim_wtlog, DOG_BE_S);
        call(u8bFeed1, prim_wtlog, '/');
        a_cstr(wtlog_s, DOG_WTLOG_NAME);
        call(u8bFeed, prim_wtlog, wtlog_s);
        call(PATHu8bTerm, prim_wtlog);
        u8bp mapped = NULL;
        if (FILEMapRO(&mapped, $path(prim_wtlog)) == OK) {
            a_dup(u8c, body, u8bDataC(mapped));
            //  Row 0 is `<ts>\trepo\t<uri>\n`; pick the URI between
            //  second tab and newline.
            u8c const *nl = body[0];
            while (nl < body[1] && *nl != '\n') nl++;
            u8c const *tab2 = body[0];
            int tabs = 0;
            while (tab2 < nl && tabs < 2) {
                if (*tab2 == '\t') tabs++;
                tab2++;
            }
            if (tabs == 2 && tab2 < nl) {
                u8cs r0_uri = {(u8c *)tab2, (u8c *)nl};
                //  URI path starts after `scheme:` — find the first
                //  `:` and skip a `//<auth>` if present.
                uri parsed = {};
                u8csMv(parsed.data, r0_uri);
                URILexer(&parsed);
                if (!u8csEmpty(parsed.path))
                    DOGProjectFromBe(parsed.path, prim_proj);
            }
            FILEUnMap(mapped);
        }
    }

    {
        int fd = FILE_CLOSED;
        ok64 co = FILECreate(&fd, $path(cwd_be));
        if (co != OK) return co;

        a_path(repo_uri);
        a_cstr(file_pref, "file://");
        call(u8bFeed, repo_uri, file_pref);
        call(u8bFeed, repo_uri, prim_s);
        call(u8bFeed1, repo_uri, '/');
        call(u8bFeed, repo_uri, DOG_BE_S);
        call(u8bFeed1, repo_uri, '/');
        if (u8bDataLen(prim_proj) > 0) {
            a_dup(u8c, pp, u8bDataC(prim_proj));
            call(u8bFeed, repo_uri, pp);
            call(u8bFeed1, repo_uri, '/');
        }

        //  Compose the row body: `<ts>\trepo\t<uri>\n`.  ts =
        //  RONNow(); verb = `repo`; uri = file:///<prim>/.be/.
        a_pad(u8, row, 1024);
        ron60 ts = RONNow();
        call(RONutf8sFeed, u8bIdle(row), ts);
        call(u8bFeed1, row, '\t');
        a_cstr(repo_verb, "repo");
        call(u8bFeed, row, repo_verb);
        call(u8bFeed1, row, '\t');
        call(u8bFeed, row, u8bDataC(repo_uri));
        call(u8bFeed1, row, '\n');

        a_dup(u8c, rowbytes, u8bData(row));
        (void)FILEFeedAll(fd, rowbytes);
        FILEClose(&fd);
    }

    fprintf(stderr, "be: worktree from %.*s\n",
            (int)$len(u->path), (char *)u->path[0]);

    // Resolve the primary's current commit via its wtlog.
    // Rewrite this URI to "?<sha>" so downstream sniff checks out
    // that commit in the worktree.
    a_pad(u8, prim_at, FILE_PATH_MAX_LEN + 128);
    a_dup(u8c, prim_root, prim_s);
    if (SNIFFAtTailOf(prim_root, prim_at) != OK) done;
    uri prim_uri = {};
    u8csMv(prim_uri.data, u8bDataC(prim_at));
    URILexer(&prim_uri);
    //  Hashlet: 6..40 hex chars (full sha1 = 40, prefix abbreviations
    //  shorter).  Anything outside that range is not a valid pin.
    size_t flen = u8csLen(prim_uri.fragment);
    if (flen < 6 || flen > 40) done;

    //  Compose "?<hashlet>" into the static buffer; expose data and
    //  query slices into it (other URI components stay empty).
    u8bReset(wt_uri_buf);
    call(u8bFeed1, wt_uri_buf, '?');
    call(u8bFeed,  wt_uri_buf, prim_uri.fragment);

    zerop(u);
    u8csMv(u->data,  u8bDataC(wt_uri_buf));
    u8csMv(u->query, u->data);
    u8csUsed1(u->query);  //  drop leading '?'
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

    //  `--color` (alias `--ansi`) forces the bro pager pipeline even
    //  when stdout is not a terminal — useful for capturing the
    //  ANSI-coloured renderer output (vs. the plain unified-diff text
    //  that graf emits when piped).  Bro reads BRO_COLOR=1 from the
    //  environment and uses BROPlain to one-shot dump ANSI when its
    //  own stdout is non-TTY.
    b8 force_color = CLIHas(c, "--color") || CLIHas(c, "--ansi");
    if (force_color) setenv("BRO_COLOR", "1", 1);
    b8 tty = (isatty(STDOUT_FILENO) || force_color) ? YES : NO;

    a_path(dogpath);
    a$rg(a0, 0);
    HOMEResolveSibling(NULL, dogpath, dog_s, a0);

    //  Verbless: dog argv is `<dog> [--at <uri>] [--tlv] <URI>`.  The
    //  dog sees the URI with its projector scheme intact and dispatches
    //  on u->scheme inside its own CLI.  `--at` carries the wt's tip
    //  so the projector (graf map / log "you are here", etc.) doesn't
    //  need to poke at `.be/wtlog` itself.
    a_cstr(tlv_flag, "--tlv");
    b8 have_at = u8bDataLen(be_at_buf) > 0;
    a_pad(u8cs, dargs, 5);
    u8csbFeed1(dargs, dog_s);
    if (have_at) {
        a_dup(u8c, at_flag, be_at_flag);
        a_dup(u8c, at_val,  u8bData(be_at_buf));
        u8csbFeed1(dargs, at_flag);
        u8csbFeed1(dargs, at_val);
    }
    if (tty) u8csbFeed1(dargs, tlv_flag);
    u8csbFeed1(dargs, u->data);
    a_dup(u8cs, dargv, u8csbData(dargs));

    if (!tty) return BERun(dog_s, dargv, NO);

    //  TTY: pipe through bro.  Bro drains HUNK TLV from stdin (see
    //  bro/BRO.c §BROPipeRun) and opens /dev/tty for keystrokes.
    a_path(bropath);
    a_cstr(bro_name, "bro");
    HOMEResolveSibling(NULL, bropath, bro_name, a0);
    a_pad(u8cs, bargs, 1);
    u8csbFeed1(bargs, bro_name);
    a_dup(u8cs, bargv, u8csbData(bargs));
    return BERunPipe($path(dogpath), dargv, $path(bropath), bargv);
}

//  `be get URI` (DOG.md §10a):
//
//    1. keeper get URI  — synchronous.  Fetches/clones (remote URI),
//       writes the pack to .be, builds keeper's own index.
//    2. spot get URI, graf get URI, sniff get URI — in parallel.
//       Each dog opens keeper read-only, walks the URI's tip(s), and
//       updates its own state (spot/graf indexes; sniff worktree).
//
//  `--seq` (debugging) collapses step 2 to sequential keeper-order
//  execution — same dispatch shape as the other verbs.
//
//  Pre-flight: URI normalisation (worktree wiring + fresh-clone
//  .be/ bootstrap so the downstream dogs have a place to land).
//
//  This is the per-project body; the public `BEGet` wraps this with
//  pre-order submodule recursion (SUBS.plan.md §GET).
static ok64 BEGetLocal(cli *c, b8 seq) {
    sane(c);
    //  GET is ref-expecting: promote bare `be get other/branch` to
    //  query=other/branch just like POST and PATCH.  Path views
    //  (`be VERBS.md`) are the verbless form, not GET.
    for (u32 i = 0; i < c->nuris; i++) be_promote_to_ref(&c->uris[i]);

    uri *u = (c->nuris > 0) ? &c->uris[0] : NULL;
    b8  remote = (u != NULL && !$empty(u->authority));

    //  Local file: URI → wire this cwd as a worktree of a sibling repo.
    call(BEGetWorktree, u);

    //  Single-file overwrite: `be get file.c?feat` (VERBS.md §GET).
    //  Path+query (no authority) is a one-file checkout — bypass the
    //  spot/graf parallel index pipeline and route only to sniff,
    //  which fetches the blob via keeper and overwrites the wt file
    //  without touching `.be/wtlog` (no `get`/`put` row appended).
    if (u != NULL && !$empty(u->path) && !$empty(u->query) &&
        $empty(u->authority)) {
        a_pad(u8cs, args, 4 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS);
        a_cstr(get_s,   "get");
        a_cstr(sniff_s, "sniff");
        a_dup(u8c, sniff_d, sniff_s);
        a_dup(u8c, get_d,   get_s);
        be_build_argv(args, sniff_d, get_d, c);
        a_dup(u8cs, argv, u8csbData(args));
        call(BERun, sniff_d, argv, NO);
        (void)seq;
        done;
    }

    //  Auto-bootstrap: GET is a writer (advances cur, stamps files,
    //  appends a `get` row), so it needs `.be/` markers like PUT/POST.
    //  Covers both the fresh-clone (remote) and the local
    //  `be get ?branch` on an empty dir cases.  For remote clones,
    //  derive the project name from the URL and lay down
    //  `<cwd>/.be/<project>/{refs,wtlog}` (project-sharded layout —
    //  see dog/DOG.h §"Canonical on-disk layout").
    call(be_ensure_project_repo, u);

    //  Step 1: keeper get URI — synchronous.  Only meaningful when
    //  the URI carries a remote authority (fetch/clone path); for a
    //  local-only checkout (`?ref`, `?<sha>`, bare `?`) keeper has
    //  nothing to do — its index is already current — so skip it.
    a_cstr(get_s,    "get");
    a_cstr(keeper_s, "keeper");
    if (remote) {
        a_pad(u8cs, args, 4 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS);
        a_dup(u8c, keeper_d, keeper_s);
        a_dup(u8c, get_d,    get_s);
        be_build_argv(args, keeper_d, get_d, c);
        a_dup(u8cs, argv, u8csbData(args));
        call(BERun, keeper_d, argv, NO);
    }

    //  Step 2: spot, graf, sniff in parallel.
    static u8c const spot_lit[]  = "spot";
    static u8c const graf_lit[]  = "graf";
    static u8c const sniff_lit[] = "sniff";
    u8cs const dogs[3] = {
        {spot_lit,  spot_lit  + 4},
        {graf_lit,  graf_lit  + 4},
        {sniff_lit, sniff_lit + 5},
    };

    if (seq) {
        for (int i = 0; i < 3; i++) {
            a_pad(u8cs, args, 4 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS);
            a_dup(u8c, dog_d, dogs[i]);
            a_dup(u8c, get_d, get_s);
            be_build_argv(args, dog_d, get_d, c);
            a_dup(u8cs, argv, u8csbData(args));
            call(BERun, dog_d, argv, NO);
        }
        done;
    }

    pid_t pids[3] = {0};
    ok64  spawn_err[3] = {OK, OK, OK};
    for (int i = 0; i < 3; i++) {
        a_pad(u8cs, args, 4 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS);
        a_dup(u8c, dog_d, dogs[i]);
        a_dup(u8c, get_d, get_s);
        be_build_argv(args, dog_d, get_d, c);
        a_dup(u8cs, argv, u8csbData(args));
        spawn_err[i] = BESpawn(dog_d, argv, &pids[i]);
        if (spawn_err[i] != OK) {
            fprintf(stderr, "be: spawn " U8SFMT ": %s\n",
                    u8sFmt(dog_d), ok64str(spawn_err[i]));
        }
    }
    ok64 worst = OK;
    for (int i = 0; i < 3; i++) {
        if (spawn_err[i] != OK) { worst = spawn_err[i]; continue; }
        a_dup(u8c, dog_d, dogs[i]);
        ok64 r = BEReap(pids[i], dog_d);
        if (r != OK) worst = r;
    }
    return worst;
}

//  GET recursion (SUBS.plan.md §GET).  Pre-order: parent's WRITE
//  pass (BEGetLocal above) materialises `.gitmodules` and gitlink
//  mount points first; then we ask keeper for the canonical
//  (URL, mount-path, pin) triple per declared submodule.
//
//  For each row:
//    * sub already mounted on disk → fork `be get [flags] ?<pin>`
//      with cwd = mount.  Idempotent if the sub's at the pin
//      already; otherwise switches it.
//    * sub declared in the tree but not on disk → fork `sniff
//      sub-mount <path>#<pin>` to do the first-time fetch + check-
//      out in a clean keeper state (no longer mid-parent-WALK, so
//      WIREFetchAll writes land in a stable shard — fixes get/12).
//
//  `be: get <relpath>` markers fire lazily on the first row so
//  single-project repos (no `.gitmodules`) keep their pre-recursion
//  stderr contract — same shape as BEHead.

//  Read child stdout to EOF into `out` (RESET on entry).  Reaps the
//  child.  Returns the child's translated exit code (OK / BEDOGEXIT
//  / BEDOGSIG).
static ok64 be_capture(u8csc tool, u8css argv, u8bp out) {
    sane($ok(tool) && out);
    u8bReset(out);

    a_path(path);
    a$rg(a0, 0);
    HOMEResolveSibling(NULL, path, tool, a0);

    int stdout_r = -1;
    pid_t pid = 0;
    call(FILESpawn, $path(path), argv, NULL, &stdout_r, &pid);

    for (;;) {
        if (u8bIdleLen(out) == 0) break;     // out full
        u8 *idle = u8bIdleHead(out);
        size_t cap = (size_t)u8bIdleLen(out);
        ssize_t n = read(stdout_r, idle, cap);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;
        u8bFed(out, (u32)n);
    }
    close(stdout_r);

    int rc = 0;
    ok64 r = FILEReap(pid, &rc);
    if (r == FILESIGNAL) return BEDOGSIG;
    if (r != OK)         return r;
    if (rc != 0)         return BEDOGEXIT;
    done;
}

//  Spawn `keeper subs ?<query>` and capture its ULOG output into
//  `out`.  Empty `out` on the no-sub case (still OK).
static ok64 beget_keeper_subs(u8cs query, u8bp out) {
    sane(out);
    a_pad(u8cs, args, 4);
    a_cstr(keeper_s, "keeper");
    a_cstr(subs_s,   "subs");
    a_dup(u8c, keeper_d, keeper_s);
    a_dup(u8c, subs_d,   subs_s);
    u8csbFeed1(args, keeper_d);
    u8csbFeed1(args, subs_d);

    a_pad(u8, qbuf, 256);
    call(u8bFeed1, qbuf, '?');
    call(u8bFeed,  qbuf, query);
    a_dup(u8c, qview, u8bData(qbuf));
    u8csbFeed1(args, qview);

    a_dup(u8cs, argv, u8csbData(args));
    return be_capture(keeper_d, argv, out);
}


//  Check whether `subpath` appears as the query slot of any ULOG row
//  in `ulog`.  Used to diff baseline-vs-target sub lists: a sub
//  present in baseline but not in target has been removed by the
//  target tree and should be unmounted.
static b8 beget_subs_has(u8cs ulog, u8cs subpath) {
    u8cs scan = {ulog[0], ulog[1]};
    for (;;) {
        ulogrec row = {};
        ok64 dr = ULOGu8sDrain(scan, &row);
        if (dr == NODATA) break;
        if (dr != OK) continue;
        if (u8csEq(row.uri.query, subpath)) return YES;
    }
    return NO;
}

//  Unmount a sub that's been removed by the target tree: unlink the
//  secondary-wt anchor file at `<wt>/<subpath>/.be`.  Leaves the
//  sub's wt files in place — they become plain untracked content
//  (caller can `rm -rf` separately if desired).  Returns OK whether
//  or not the anchor was actually there (idempotent for the
//  already-unmounted case).
static ok64 beget_sub_unmount(u8cs wt_root, u8cs subpath) {
    sane($ok(wt_root) && $ok(subpath));
    a_path(anchor);
    call(PATHu8bFeed, anchor, wt_root);
    call(PATHu8bAdd,  anchor, subpath);
    a_cstr(be_s, ".be");
    call(PATHu8bPush, anchor, be_s);
    (void)FILEUnLink($path(anchor));
    fprintf(stderr, "be: get %.*s: unmounted\n",
            (int)$len(subpath), (char *)subpath[0]);
    done;
}

//  Iterate baseline ULOG rows; for any sub not present in target,
//  unmount it.  Order is independent of target processing.
static ok64 beget_drain_removed(u8cs wt_root, u8cs baseline_ulog,
                                u8cs target_ulog) {
    sane($ok(wt_root));
    ok64 worst = OK;
    u8cs scan = {baseline_ulog[0], baseline_ulog[1]};
    for (;;) {
        ulogrec row = {};
        ok64 dr = ULOGu8sDrain(scan, &row);
        if (dr == NODATA) break;
        if (dr != OK) continue;

        u8cs path = {};
        u8csMv(path, row.uri.query);
        if (u8csEmpty(path)) continue;
        if (beget_subs_has(target_ulog, path)) continue;     // still declared

        if (!SNIFFSubIsMount(wt_root, path)) continue;       // already gone
        ok64 ur = beget_sub_unmount(wt_root, path);
        if (ur != OK) worst = ur;
    }
    return worst;
}

//  Spawn `sniff sub-mount ./<subpath>#<pin>` from the parent wt to
//  do a first-time mount (anchor + WIREFetchAll + checkout) in a
//  clean keeper state.  cwd inherits the parent process's cwd, which
//  is the parent wt at this point.
static ok64 beget_sub_mount(u8cs subpath, u8cs pin) {
    sane($ok(subpath) && $ok(pin));

    a_pad(u8, uri_buf, 1024);
    a_cstr(rel, "./");
    call(u8bFeed,  uri_buf, rel);
    call(u8bFeed,  uri_buf, subpath);
    call(u8bFeed1, uri_buf, '#');
    call(u8bFeed,  uri_buf, pin);
    a_dup(u8c, uri_view, u8bData(uri_buf));

    a_pad(u8cs, args, 4);
    a_cstr(sniff_s, "sniff");
    a_cstr(verb_s,  "sub-mount");
    a_dup(u8c, sniff_d, sniff_s);
    a_dup(u8c, verb_d,  verb_s);
    u8csbFeed1(args, sniff_d);
    u8csbFeed1(args, verb_d);
    u8csbFeed1(args, uri_view);
    a_dup(u8cs, argv, u8csbData(args));

    return BERun(sniff_d, argv, NO);
}

//  Iterate `keeper subs` ULOG rows via the standard ULOG drain API.
//  Per row: row's URI is `<url>?<mount-path>#<pin>` (the keeper SUBS
//  shape), so URILexer fills `uri.query` with the mount path and
//  `uri.fragment` with the 40-hex pin; URL components land in
//  scheme/authority/path slots.  If the sub is mounted on disk,
//  recurse via `be get [flags] ?<pin>` cwd=mount; else hand off to
//  `sniff sub-mount` for the first-time mount.  Outer `be: get .`
//  marker fires lazily on the first row.
static ok64 beget_drain_subs(u8cs wt_root, u8cs subs_ulog,
                             u8cs *flag_head, u8cs *flag_term) {
    sane($ok(wt_root));
    ok64 worst = OK;
    b8 outer_emitted = NO;

    u8cs scan = {subs_ulog[0], subs_ulog[1]};
    for (;;) {
        ulogrec row = {};
        ok64 dr = ULOGu8sDrain(scan, &row);
        if (dr == NODATA) break;
        if (dr != OK) continue;     //  ULOGBADFMT advances past the bad line

        u8cs path = {};
        u8csMv(path, row.uri.query);
        u8cs pin  = {};
        u8csMv(pin,  row.uri.fragment);
        if (u8csEmpty(path) || u8csLen(pin) != 40) continue;

        if (!outer_emitted) {
            fprintf(stderr, "be: get .\n");
            outer_emitted = YES;
        }
        fprintf(stderr, "be: get %.*s\n",
                (int)$len(path), (char *)path[0]);

        u8cs subpath_arg = {};
        u8csMv(subpath_arg, path);
        u8cs pin_arg = {};
        u8csMv(pin_arg, pin);

        //  Always run `sniff sub-mount`: it's idempotent and handles
        //  both first-time mount (write anchor, seed shard, fetch,
        //  initial checkout) and re-fetch on an existing mount (fetch
        //  new commits into the existing shard; the subsequent
        //  recurse below moves the wt to the new pin).  Skipping
        //  this on already-mounted subs leaves the sub-shard at the
        //  old pin and the recursive `be get ?<new_pin>` then fails
        //  with KEEPNONE.  See SUBS.c for the already_mounted handling.
        //  If the fetch/checkout itself fails, skip the recursion
        //  below — there's no usable mount to recurse into.
        b8 is_mounted = SNIFFSubIsMount(wt_root, subpath_arg);
        fprintf(stderr,
                "BE.dbg: sub path=" U8SFMT " pin=" U8SFMT
                " is_mounted=%s wt_root=" U8SFMT "\n",
                u8sFmt(subpath_arg), u8sFmt(pin_arg),
                is_mounted ? "YES" : "NO", u8sFmt(wt_root));
        {
            ok64 mr = beget_sub_mount(subpath_arg, pin_arg);
            fprintf(stderr,
                    "BE.dbg: beget_sub_mount path=" U8SFMT
                    " result=%s\n",
                    u8sFmt(subpath_arg), ok64str(mr));
            if (mr != OK) {
                worst = mr;
                continue;
            }
        }

        //  Recurse `be get [flags] ?<pin>` cwd=mount.  Idempotent if
        //  the sub is already at pin (no-op overlay); otherwise
        //  switches it.  Also kicks BEGet's wrapper inside the
        //  child, picking up any deeper sub-of-sub the sub's tree
        //  declares (head/06-sub-depth3).
        a_pad(u8, pin_uri, 64);
        call(u8bFeed1, pin_uri, '?');
        call(u8bFeed,  pin_uri, pin_arg);
        a_dup(u8c, pin_uri_view, u8bDataC(pin_uri));

        a_pad(u8cs, child_args, 4 + CLI_MAX_FLAGS * 2);
        a_cstr(get_lit, "get");
        a_dup(u8c, get_d, get_lit);
        u8csbFeed1(child_args, get_d);
        for (u8cs *fp = flag_head; fp < flag_term; fp++) {
            u8csbFeed1(child_args, *fp);
        }
        u8csbFeed1(child_args, pin_uri_view);
        a_dup(u8cs, child_argv, u8csbData(child_args));

        ok64 r = BERecurseInto(wt_root, subpath_arg, child_argv);
        if (r != OK) worst = r;
    }
    return worst;
}

//  Public BEGet wrapper: local body first (parent project), then
//  keeper-driven submodule orchestration.
static ok64 BEGet(cli *c, b8 seq) {
    sane(c);

    //  Snapshot the PRE-checkout tip BEFORE BEGetLocal runs.  After
    //  the local body, the wtlog's tail moves to the new tip; this
    //  saved value is the baseline that target_tip will be diffed
    //  against to find removed/renamed subs (get/13, get/17).
    u8cs baseline_ref = {};
    a_pad(u8, baseline_at_buf, FILE_PATH_MAX_LEN + 128);
    if (u8bHasData(c->repo)) {
        if (SNIFFAtTailOf($path(c->repo), baseline_at_buf) == OK) {
            uri bt = {};
            u8csMv(bt.data, u8bDataC(baseline_at_buf));
            URILexer(&bt);
            if (u8csLen(bt.fragment) == 40)
                u8csMv(baseline_ref, bt.fragment);
        }
    }

    //  `--nosub`: skip the submodule recursion entirely.  The user
    //  asked us to leave declared subs unmounted; print a stderr
    //  marker (VERBS.md §"GET --nosub") and bail out after the local
    //  body finishes.  Each level of `be get` honors this flag, so
    //  cascading clones (parent + sub + leaf) stop at this level.
    b8 nosub = CLIHas(c, "--nosub");

    ok64 worst = BEGetLocal(c, seq);

    if (nosub) {
        fprintf(stderr, "be: submodule(s) skipped (--nosub)\n");
        return worst;
    }

    //  Re-resolve the wt root after the local body.  Also grab the
    //  active leaf branch from the anchor (parked in h->cur_branch by
    //  HOMEOpen via DOGBranchFromBe) so beget_keeper_subs can pass it
    //  as the URI's branch slot — `?<branch>#<sha>` puts keeper_subs
    //  at the right leaf shard for sub enumeration.
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
    if (!u8bHasData(c->repo)) return worst;
    a_dup(u8c, wt_root, $path(c->repo));

    //  Target ref to pass to `keeper subs`: prefer the wtlog tail's
    //  full `?<branch>#<sha>` form so keeper opens the right leaf
    //  shard (branch slot) AND can resolve the commit directly via
    //  the fragment.  Falls back to URI query for truly fresh
    //  clones (no wtlog tail yet).
    a_pad(u8, target_ref_buf, 128);
    a_pad(u8, target_at_buf, FILE_PATH_MAX_LEN + 128);
    uri tt = {};
    b8 have_tail = (SNIFFAtTailOf(wt_root, target_at_buf) == OK);
    if (have_tail) {
        u8csMv(tt.data, u8bDataC(target_at_buf));
        URILexer(&tt);
    }
    if (have_tail && u8csLen(tt.fragment) == 40) {
        //  Compose `<branch>#<sha>` (query slot's body, no leading `?`
        //  — beget_keeper_subs prefixes `?` itself).  Prefer the
        //  home's cur_branch (set from the row-0 anchor for sub-shard
        //  / sub-clone contexts) over the wtlog row's query slot —
        //  the latter records the source URI's ref name (e.g.
        //  `?master` from `ssh://…?master`), not the local leaf.
        //  Branch may be empty for trunk; we still emit `#<sha>`
        //  so keeper_subs's fragment path kicks in.
        if (u8bDataLen(post_get_branch) > 0) {
            (void)u8bFeed(target_ref_buf, u8bDataC(post_get_branch));
        } else if (!u8csEmpty(tt.query)) {
            (void)u8bFeed(target_ref_buf, tt.query);
        }
        (void)u8bFeed1(target_ref_buf, '#');
        (void)u8bFeed(target_ref_buf, tt.fragment);
    } else if (c->nuris > 0 && !u8csEmpty(c->uris[0].query)) {
        (void)u8bFeed(target_ref_buf, c->uris[0].query);
    }
    if (u8bDataLen(target_ref_buf) == 0) return worst;
    u8cs target_ref = {};
    u8csMv(target_ref, u8bDataC(target_ref_buf));

    //  Enumerate target's subs (the post-checkout declarations).
    Bu8 target_subs = {};
    call(u8bAllocate, target_subs, 1UL << 16);
    ok64 ke = beget_keeper_subs(target_ref, target_subs);
    if (ke != OK) {
        u8bFree(target_subs);
        return worst;                          //  keeper failed; bail
    }

    //  Enumerate baseline's subs (best-effort).  Skipped if we
    //  didn't capture a baseline (initial clone) or it's the same
    //  as target (no checkout move).
    Bu8 baseline_subs = {};
    call(u8bAllocate, baseline_subs, 1UL << 16);
    if (!u8csEmpty(baseline_ref) && !u8csEq(baseline_ref, target_ref)) {
        (void)beget_keeper_subs(baseline_ref, baseline_subs);
    }

    //  Build flag tail (sans --at).
    a_pad(u8cs, flags_buf, CLI_MAX_FLAGS * 2);
    for (u32 j = 0; j + 1 < c->nflags; j += 2) {
        if ($eq(c->flags[j], be_at_flag)) continue;
        u8csbFeed1(flags_buf, c->flags[j]);
        if (!u8csEmpty(c->flags[j + 1]))
            u8csbFeed1(flags_buf, c->flags[j + 1]);
    }
    a_dup(u8cs, flag_view, u8csbData(flags_buf));

    //  Unmount removed subs (in baseline but not in target).  Runs
    //  BEFORE the target-driven mount/recurse so the "rename"
    //  case (path moves) lays down the new mount cleanly.
    if (u8bDataLen(baseline_subs) > 0 && u8bDataLen(target_subs) > 0) {
        a_dup(u8c, base_view, u8bData(baseline_subs));
        a_dup(u8c, tgt_view,  u8bData(target_subs));
        ok64 rr = beget_drain_removed(wt_root, base_view, tgt_view);
        if (rr != OK) worst = rr;
    } else if (u8bDataLen(baseline_subs) > 0 &&
               u8bDataLen(target_subs) == 0) {
        //  All subs removed.
        u8cs empty = {NULL, NULL};
        a_dup(u8c, base_view, u8bData(baseline_subs));
        ok64 rr = beget_drain_removed(wt_root, base_view, empty);
        if (rr != OK) worst = rr;
    }

    //  Mount/recurse target's subs.
    if (u8bDataLen(target_subs) > 0) {
        a_dup(u8c, tgt_view, u8bData(target_subs));
        ok64 sub_worst = beget_drain_subs(wt_root, tgt_view,
                                          (u8cs *)flag_view[0],
                                          (u8cs *)flag_view[1]);
        if (sub_worst != OK) worst = sub_worst;
    }

    u8bFree(target_subs);
    u8bFree(baseline_subs);
    return worst;
}

//  `be head <uri>` — peek/dry-run.  Per VERBS.md §"HEAD":
//    - `?br` (local)              — ahead/behind cur vs ?br
//    - `//host` (cached)          — diff cur vs cached origin tracking
//    - `ssh://host` (transport)   — fetch refs+pack, update .be/refs,
//                                    print diff cur vs origin
//    - `#frag`                    — commit-msg search; diff cur vs match
//
//  Implementation: thin orchestrator (DOG.md §10a "be is a thin
//  router; sub-dogs do the work"):
//    transport-scheme remote → keeper get URI (fetches; updates the
//                              local remote-tracking cache)
//    cached-or-local target  → no fetch step; sniff/graf print the
//                              diff against the named ref
//
//  HEAD never modifies a branch's history or the wt; the only side
//  effect on a transport-scheme URI is the cache refresh in
//  `.be/refs` and the pack data added to keeper.
//
//  Skeleton: today HEAD piggy-backs on `keeper get` for the fetch
//  half (which prints the fetched ref's sha to stderr — enough to
//  satisfy the canonical "rebase trunk on top of remote main" test's
//  cache-refresh assertion).  The diff-summary half is TODO once
//  the underlying graf/sniff "ahead/behind cur vs ref" entry point
//  lands.  See VERBS.todo.md §"HEAD".
//  Local body — the original BEHead implementation, unchanged.  The
//  per-project work for one level of the submodule forest.  Recursion
//  into mounted subs lives in the BEHead wrapper below.
static ok64 BEHeadLocal(cli *c, b8 seq) {
    sane(c);
    uri *u = (c->nuris > 0) ? &c->uris[0] : NULL;
    b8 transport = (u != NULL && !$empty(u->scheme));
    b8 cached    = (u != NULL && !transport && !$empty(u->authority));

    //  Transport scheme: forward to keeper get (fetches refs + pack),
    //  then spot get + graf get in parallel so the freshly-pulled
    //  commits + trees + blobs get indexed for downstream walks
    //  (`be log:`, `be patch`, `be spot ...`).  Per VERBS.md §HEAD,
    //  HEAD with a transport scheme updates `.be/refs` and pulls a
    //  pack; without the indexing chain, `be log:` on the fetched
    //  history would walk only as far as graf's DAG already knew.
    //
    //  `?*` wildcard query (`be head ssh://origin?*`) routes to keeper
    //  get just like a single-ref form — keeper detects the literal
    //  `*` and runs the bulk-fetch (advertise + multi-want) path.
    if (transport || cached) {
        a_cstr(get_s,    "get");

        //  Step 1: keeper get URI — synchronous.
        {
            a_pad(u8cs, args, 4 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS);
            a_cstr(keeper_s, "keeper");
            a_dup(u8c, keeper_d, keeper_s);
            a_dup(u8c, get_d,    get_s);
            be_build_argv(args, keeper_d, get_d, c);
            a_dup(u8cs, argv, u8csbData(args));
            call(BERun, keeper_d, argv, NO);
        }

        //  Step 2: graf + spot get URI in parallel.  HEAD is read-
        //  only so sniff is NOT included (no wt change, no get row).
        //  Mirrors BEGet's parallel pattern minus sniff.
        static u8c const graf_lit[] = "graf";
        static u8c const spot_lit[] = "spot";
        u8cs const dogs[2] = {
            {graf_lit, graf_lit + 4},
            {spot_lit, spot_lit + 4},
        };

        if (seq) {
            for (int i = 0; i < 2; i++) {
                a_pad(u8cs, args, 4 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS);
                a_dup(u8c, dog_d, dogs[i]);
                a_dup(u8c, get_d2, get_s);
                be_build_argv(args, dog_d, get_d2, c);
                a_dup(u8cs, argv, u8csbData(args));
                call(BERun, dog_d, argv, NO);
            }
            done;
        }

        pid_t pids[2] = {0};
        ok64  spawn_err[2] = {OK, OK};
        for (int i = 0; i < 2; i++) {
            a_pad(u8cs, args, 4 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS);
            a_dup(u8c, dog_d, dogs[i]);
            a_dup(u8c, get_d2, get_s);
            be_build_argv(args, dog_d, get_d2, c);
            a_dup(u8cs, argv, u8csbData(args));
            spawn_err[i] = BESpawn(dog_d, argv, &pids[i]);
            if (spawn_err[i] != OK) {
                fprintf(stderr, "be: spawn " U8SFMT ": %s\n",
                        u8sFmt(dog_d), ok64str(spawn_err[i]));
            }
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

    //  Local: hand off to `graf head`.  graf dispatches internally:
    //    fragment-only URI → commit-message substring search;
    //    `?br` / no URI    → ahead/behind cur vs target + tree diff.
    //  Bare `be` (no verb) is the spec's "current branch + ahead/
    //  behind + dirty list" combo and adds `sniff status` upstream;
    //  `be head` itself stays read-only and refuses to mix in wt
    //  status (per VERBS.md §HEAD: "never modifies a branch's
    //  history or the wt").
    {
        a_pad(u8cs, args, 4 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS);
        a_cstr(head_s, "head");
        a_cstr(graf_s, "graf");
        a_dup(u8c, graf_d, graf_s);
        a_dup(u8c, head_d, head_s);
        be_build_argv(args, graf_d, head_d, c);
        a_dup(u8cs, argv, u8csbData(args));
        call(BERun, graf_d, argv, NO);
    }
    (void)seq;
    done;
}

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
    a_pad(u8, uri_buf, 2048);
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

//  `be head <uri>` — pre-order recursion across mounted submodules.
//
//  Algorithm (SUBS.plan.md §HEAD):
//    1. Run BEHeadLocal (existing per-project body) for the outer.
//    2. Enumerate one level of subs declared in <wt>/.gitmodules.
//       If any subs are reported (mounted or declared-not-mounted),
//       emit `be: head .` marker on stderr before the first sub
//       marker — single-project repos with no `.gitmodules` keep
//       their pre-recursion clean-stderr contract.
//    3. For each mounted sub: emit `be: head <subpath>` marker,
//       fork-self into the mount with the same head argv (minus
//       --at, which each level rederives).  For declared-but-not-
//       mounted entries: report, do not recurse.
//    4. Return the worst exit code (compiler-style aggregation).
static ok64 BEHead(cli *c, b8 seq) {
    sane(c);

    //  Local body for the outer project.  Aggregate but never
    //  short-circuit on its exit — subs are still worth probing
    //  (read-only verb).
    ok64 worst = BEHeadLocal(c, seq);

    //  Need a wt root to enumerate / chdir from.
    if (!u8bHasData(c->repo)) return worst;
    a_dup(u8c, wt_root, $path(c->repo));

    //  Detect transport mode: parent invoked with `ssh://` etc.  In
    //  that case the recursion swaps the URL per-sub (each sub
    //  fetches its OWN remote) — see head/07.  Local mode
    //  (`?ref`, bare `?`, cached `//host`) just forwards verbatim.
    b8 transport = (c->nuris > 0 && !u8csEmpty(c->uris[0].scheme));
    u8cs forwarded_query = {};
    if (transport) u8csMv(forwarded_query, c->uris[0].query);

    //  Build the child argv prefix.  Local mode: include the parent's
    //  URIs (child runs against `?<same-ref>` in its own scope).
    //  Transport mode: stop at flags — the cb appends the sub URI per
    //  call.
    a_pad(u8cs, child_args, 4 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS);
    a_cstr(head_lit, "head");
    a_dup(u8c, head_d, head_lit);
    u8csbFeed1(child_args, head_d);
    for (u32 j = 0; j + 1 < c->nflags; j += 2) {
        if ($eq(c->flags[j], be_at_flag)) continue;
        u8csbFeed1(child_args, c->flags[j]);
        if (!u8csEmpty(c->flags[j + 1]))
            u8csbFeed1(child_args, c->flags[j + 1]);
    }
    if (!transport) {
        for (u32 j = 0; j < c->nuris; j++)
            u8csbFeed1(child_args, c->uris[j].data);
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
    if (rc.worst != OK) worst = rc.worst;
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
static ok64 be_ensure_project_repo(uricp u) {
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
    //  (2) URL basename via SNIFFSubBasename.  Try u->data first
    //      (full URI string, e.g. `ssh://host/foo/bar.git?master`),
    //      fall through to u->path (file: clones, bare paths).
    if (u8csEmpty(proj) && u) {
        u8cs candidate = {NULL, NULL};
        if (!u8csEmpty(u->data))      u8csMv(candidate, u->data);
        else if (!u8csEmpty(u->path)) u8csMv(candidate, u->path);
        if (!u8csEmpty(candidate))
            (void)SNIFFSubBasename(candidate, proj);
    }
    //  (3) `basename($PWD)` — last resort.
    if (u8csEmpty(proj)) PATHu8sBase(proj, cwd_s);

    if (u8csEmpty(proj)) {
        fprintf(stderr, "be: cannot derive project name; "
                "pass `be put ?/<project>` or run from a named "
                "directory\n");
        return BEFAIL;
    }

    //  <cwd>/.be/<project>/ — the project shard.  Refuse if it
    //  already exists on disk: an explicit `?/<proj>` URI must not
    //  silently clobber a half-initialised shard (anchor missing
    //  from wtlog so we reached here, but the shard dir is present).
    //  The anchored-gate above handles the fully-initialised case
    //  (anchor present, project name matches) as a no-op.
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
    call(FILEMakeDirP, $path(shard_dir));
    a_dup(u8c, shard_s, u8bDataC(shard_dir));

    //  Seed empty refs + wtlog inside the shard.
    {
        a_path(p);
        call(PATHu8bFeed, p, shard_s);
        call(PATHu8bPush, p, DOG_REFS_S);
        int fd = -1;
        call(FILECreate, &fd, $path(p));
        call(FILEClose, &fd);
    }
    {
        a_path(p);
        call(PATHu8bFeed, p, shard_s);
        call(PATHu8bPush, p, DOG_WTLOG_S);
        int fd = -1;
        call(FILECreate, &fd, $path(p));
        call(FILEClose, &fd);
    }

    //  Compose row-0 URI: `file:<cwd>/.be/<project>/` (trailing slash
    //  matches sniff_write_repo_row's output shape).
    a_path(uri_path);
    call(PATHu8bFeed, uri_path, shard_s);
    call(u8bFeed1, uri_path, '/');
    call(PATHu8bTerm, uri_path);

    uri urow = {};
    a_cstr(scheme_s, "file");
    u8csMv(urow.scheme, scheme_s);
    {
        a_dup(u8c, p, u8bData(uri_path));
        u8csMv(urow.path, p);
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

    //  Write <cwd>/.be/wtlog with the row-0 anchor.  Primary-wt
    //  layout: wtlog sits inside the store dir alongside the
    //  project shard(s); row-0 pins this wt to one project.
    a_path(wtlog_path);
    call(PATHu8bFeed, wtlog_path, cwd_s);
    call(PATHu8bPush, wtlog_path, DOG_BE_S);
    call(PATHu8bPush, wtlog_path, DOG_WTLOG_S);
    int fd = -1;
    call(FILECreate, &fd, $path(wtlog_path));
    a_dup(u8c, body, u8bData(row));
    call(FILEFeedAll, fd, body);
    FILEClose(&fd);

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

//  `be put` is the ref-writer (VERBS.md §"PUT").  Per the URI's
//  `//remote` slot it also doubles as the FF-push verb — the wire
//  side maps to keeper's old `post` (push) entry point.  Local
//  shapes (label move, file staging, sha reset) stay in sniff put.
//
//  PUT also doubles as the repo-init verb: when nothing is reachable
//  from cwd it lays down the canonical `.be/refs` + `.be/wtlog`
//  markers so the dispatched sniff-put has a HOME to walk into.
static ok64 BEPut(cli *c, b8 seq) {
    sane(c);
    b8 has_remote = NO;
    for (u32 i = 0; i < c->nuris; i++) {
        if (!u8csEmpty(c->uris[i].authority)) { has_remote = YES; break; }
    }
    if (!has_remote) {
        uri *u0 = (c->nuris > 0) ? &c->uris[0] : NULL;
        call(be_ensure_project_repo, u0);
    }
    if (has_remote) {
        //  FF-push: `be put //origin` (cached) and `be put ssh://host`
        //  (transport) both open the wire — VERBS.md §"Schemes —
        //  cached vs transport" carves out PUT-to-remote as the one
        //  cached-form write-through.
        static dog_step const push_steps[] = {
            {u8slit("keeper"), u8slit("post"), NO},
        };
        return BEDispatch(c, push_steps, 1, seq);
    }
    static dog_step const local_steps[] = {
        {u8slit("sniff"),  u8slit("put"), NO},
    };
    return BEDispatch(c, local_steps, 1, seq);
}

//  `be delete` is the mirror of `be put`: stage tree without a file.
//  `be delete <uri>` per VERBS.md §DELETE.  Local URIs (paths or
//  `?branch`) are sniff's job — DELStage / DELBranch.  Remote URIs
//  (`//host` cached or transport-scheme) open the wire / drop the
//  alias; both arms live in `keeper delete`.  Mixing local + remote
//  URIs in one invocation is rejected by the all-or-nothing branch
//  taken on the first authority-bearing URI — the verbs are too
//  different to interleave coherently.
static ok64 BEDelete(cli *c, b8 seq) {
    sane(c);
    b8 has_remote = NO;
    for (u32 i = 0; i < c->nuris; i++) {
        if (!u8csEmpty(c->uris[i].authority)) { has_remote = YES; break; }
    }
    //  Local-only DELETE on a fresh dir is an edge case but the test
    //  surface expects auto-bootstrap parity with PUT/POST.
    if (!has_remote) {
        uri *u0 = (c->nuris > 0) ? &c->uris[0] : NULL;
        call(be_ensure_project_repo, u0);
    }
    if (has_remote) {
        static dog_step const remote_steps[] = {
            {u8slit("keeper"), u8slit("delete"), NO},
        };
        return BEDispatch(c, remote_steps, 1, seq);
    }
    static dog_step const local_steps[] = {
        {u8slit("sniff"),  u8slit("delete"), NO},
    };
    return BEDispatch(c, local_steps, 1, seq);
}

//  `be diff <uri>` — delegate to graf.  For local URIs (no authority)
//  graf reads objects straight from keeper's store; for a remote URI
//  we `keeper get` first to materialize the reachable closure, same
//  as `be patch`.
static ok64 BEDiff(cli *c, b8 seq) {
    sane(c);
    static dog_step const steps[] = {
        {u8slit("keeper"), u8slit("get"),  NO},
        {u8slit("graf"),   u8slit("diff"), NO},
    };
    u32 nsteps = sizeof(steps) / sizeof(steps[0]);
    uri *u = (c->nuris > 0) ? &c->uris[0] : NULL;
    u32 start = (u != NULL && !$empty(u->authority)) ? 0 : 1;
    return BEDispatch(c, steps + start, nsteps - start, seq);
}

//  Ref-expecting verbs (post, patch) may read path/fragment as the query
//  when it fits QURY's
//  ref grammar — `be post feat` (path) and `be post '#feat'` (fragment)
//  both yield query=feat.  Promotion only happens when query is empty
//  (caller hasn't explicitly set a ref).  Returns YES if anything moved.
static b8 be_promote_to_ref(uri *u) {
    //  Legacy: a URILexer-produced path that matches QURY ref grammar
    //  (e.g. `be get feat/sub` → path="feat/sub") gets routed to the
    //  query slot.  Bareword promotion is now handled centrally by
    //  DOGPromoteBareword in becli() per VERBS.md §"Bareword
    //  defaults"; the fragment→query branch was removed so POST's
    //  fragment-default (`be post fix` ⇒ msg="fix") survives.
    if (!$empty(u->query)) return NO;
    qref qr = {};
    if (!$empty(u->path)) {
        u8cs s = {u->path[0], u->path[1]};
        if (QURYu8sDrain(s, &qr) == OK &&
            (qr.type == QURY_REF || qr.type == QURY_SHA)) {
            u8csMv(u->query, s);
            u->path[0] = u->path[1] = NULL;
            return YES;
        }
    }
    return NO;
}

//  `be patch <uri>` — 3-way merge `uri`'s target ref/sha into the
//  worktree.  Steps:
//    1. (transport-scheme remote only) keeper get URI to fetch
//       refs+pack, refreshing `.be/refs` and seeding keeper with
//       the target commit's reachable closure.
//    2. (any remote) graf get URI to walk the (possibly newly
//       fetched) commits into graf's DAG index — sniff's PATCH
//       calls GRAFLca / GRAFResolveTip, which fail if the target's
//       ancestor set isn't indexed.  No-op when the URI's commits
//       are already in graf's index.  Mirrors the equivalent step
//       in BEPost.  Without this, a remote `be patch ssh://host?<sha>`
//       reports GRAFFAIL even though the pack data sits in keeper.
//    3. sniff patch — performs the 3-way merge into the wt.
//  See VERBS.md §PATCH.
static ok64 BEPatch(cli *c, b8 seq) {
    sane(c);
    //  PATCH is ref-expecting (absorbs another branch's stack): promote
    //  bare `be patch feat` to query=feat just like POST.
    for (u32 i = 0; i < c->nuris; i++) be_promote_to_ref(&c->uris[i]);
    //  Auto-bootstrap parity with PUT/POST — local PATCH on a fresh
    //  dir needs the same `.be/` markers downstream.
    b8 has_remote = NO, has_transport = NO;
    for (u32 i = 0; i < c->nuris; i++) {
        if (!u8csEmpty(c->uris[i].authority)) has_remote = YES;
        if (!u8csEmpty(c->uris[i].scheme))    has_transport = YES;
    }
    if (!has_remote) {
        uri *u0 = (c->nuris > 0) ? &c->uris[0] : NULL;
        call(be_ensure_project_repo, u0);
    }

    dog_step steps[3];
    u32 nsteps = 0;
    if (has_transport && has_remote) {
        steps[nsteps++] = (dog_step){u8slit("keeper"), u8slit("get"), NO};
    }
    if (has_remote) {
        steps[nsteps++] = (dog_step){u8slit("graf"),   u8slit("get"), NO};
    }
    steps[nsteps++] = (dog_step){u8slit("sniff"),  u8slit("patch"), NO};
    call(BEDispatch, c, steps, nsteps, seq);
    //  Patch may move the wt's HEAD via 3-way merge; refresh spot+graf.
    (void)be_reindex(c);
    done;
}

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

//  Per-project body — the original BEPost implementation.  The public
//  BEPost wrapper below does post-order recursion across mounted
//  submodules (SUBS.plan.md §POST) and calls this for each level.
static ok64 BEPostLocal(cli *c, b8 seq) {
    sane(c);
    //  Auto-bootstrap: `be post 'msg'` on a fresh dir is the
    //  canonical "init + first commit" path (see workflow.sh §1).
    //  Mirrors BEPut's call to be_ensure_repo; only meaningful when
    //  there's no remote authority (push targets always have a repo).
    b8 has_remote_pre = NO;
    for (u32 i = 0; i < c->nuris; i++) {
        if (!u8csEmpty(c->uris[i].authority)) {
            has_remote_pre = YES; break;
        }
    }
    if (!has_remote_pre) {
        uri *u0 = (c->nuris > 0) ? &c->uris[0] : NULL;
        call(be_ensure_project_repo, u0);
    }
    for (u32 i = 0; i < c->nuris; i++) {
        uri *u = &c->uris[i];
        //  POST is ref-expecting: bare `be post feat` should target ref
        //  feat, not be rejected as path-form.  Promote first.
        be_promote_to_ref(u);
        //  Pure-fragment URIs (commit-message via `#msg` or whitespace
        //  arg) — skip the path-form check, they're message-only.
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
    b8 has_remote    = NO;
    b8 has_transport = NO;
    for (u32 i = 0; i < c->nuris; i++) {
        if (!u8csEmpty(c->uris[i].authority)) has_remote = YES;
        if (!u8csEmpty(c->uris[i].scheme))    has_transport = YES;
    }
    //  Commit-message presence: any URI with a non-empty fragment (the
    //  new convention) or a legacy `-m` flag.
    b8 has_msg = NO;
    for (u32 i = 0; i < c->nuris; i++) {
        if (!u8csEmpty(c->uris[i].fragment)) { has_msg = YES; break; }
    }
    if (!has_msg) {
        a_cstr(mf, "-m");
        for (u32 fi = 0; fi + 1 < c->nflags; fi += 2) {
            if ($eq(c->flags[fi], mf)) { has_msg = YES; break; }
        }
    }
    //  Per VERBS.md §POST:
    //    `be post '#msg'`           — commit on cur.
    //    `be post ?br`              — commit on cur, then FF-advance ?br
    //                                 to cur.tip (POSTPromote inside sniff).
    //    `be post //origin '#msg'`  — commit on cur, then FF-push cur's
    //    `be post ssh://origin '#msg'` new tip to origin's counterpart.
    //  POST never rewrites cur (rebase is `be patch ?br#` + `be post`).
    //  Pure pushes (no commit) belong to PUT (`be put //origin`).  POST
    //  with `//remote` but no commit content refuses via POSTNONE.
    dog_step steps[2];
    u32 nsteps = 0;
    //  Step 1: sniff post — commit-on-cur + optional ?branch promote.
    //  Skip only the bare-status case (no msg, no URIs): there's
    //  nothing to commit and sniff would just dry-run.
    b8 ran_sniff = NO;
    if (has_msg || c->nuris > 0 || !has_remote) {
        steps[nsteps++] = (dog_step){u8slit("sniff"),  u8slit("post"), NO};
        ran_sniff = YES;
    }
    //  Step 2: keeper post — FF-push cur's new tip to remote.  Picks
    //  up the post-commit `--at <root>?<branch>#<sha>` injected by `be`
    //  between dispatch steps so the wire side knows what tip to send.
    //  No transport scheme on `//remote`: keeper_post resolves the
    //  authority via REFSResolve (alias table) and opens the wire.
    if (has_remote) {
        steps[nsteps++] = (dog_step){u8slit("keeper"), u8slit("post"), NO};
    }
    call(BEDispatch, c, steps, nsteps, seq);

    //  Local reindex: a successful sniff post moved the wt's HEAD;
    //  refresh spot/graf so subsequent log/diff/search see the new
    //  tip without a manual `be get`.  Skip the bare-dry-run case
    //  (no commit, no message, no URI) — sniff just printed the
    //  would-be change-set and `.be/wtlog` baseline is unchanged.
    b8 dry_run = !has_msg && c->nuris == 0;
    if (ran_sniff && !dry_run) (void)be_reindex(c);
    done;
}

//  Spawn `be put <subpath>` in the parent wt to stage a `put
//  <subpath>#<40-hex>` row reflecting the sub's current tip
//  (SNIFFSubReadTip).  Used by BEPost after a sub's recursive POST
//  returns OK: BEPostLocal then reads that row and emits the gitlink
//  ADD in the parent commit.  A clean sub (tip == baseline) just gets
//  re-stamped — no decision row, no parent bump.
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
//  POSTNONE detection rides on the exit code.  abc/PRO.h::MAIN
//  returns the full ok64 from main(), but the kernel truncates to
//  the low byte via WEXITSTATUS.  POSTNONE = 0x65871d5d85ce, low
//  byte 0xCE = 206.  POSTNOMSG, KEEPFAIL, SNIFFFAIL etc. have
//  distinct residues — if a future code ever collides on 0xCE the
//  RON60 author needs to pick a different encoding.  See plan
//  §POST: "a sub with no dirty paths just no-ops; parent doesn't
//  bump that gitlink".
//
//  Returns OK on (child exit 0) OR (child exit POSTNONE's
//  low-byte residue, with *postnone_out = YES).  BEDOGEXIT /
//  BEDOGSIG otherwise.
#define BEPOST_POSTNONE_BYTE  ((int)(POSTNONE & 0xFFu))

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
    if (rc == BEPOST_POSTNONE_BYTE) { *postnone_out = YES; done; }
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
    for (u32 i = 0; i + 1 < c->nflags; i += 2) {
        if (!$eq(c->flags[i], sm_flag)) continue;
        u8cs val = {c->flags[i + 1][0], c->flags[i + 1][1]};
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
    a_pad(u8, msg_uri, 1024);
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
    for (u32 j = 0; j + 1 < rc->c->nflags; j += 2) {
        if ($eq(rc->c->flags[j], be_at_flag)) continue;
        if ($eq(rc->c->flags[j], mf_lit))     continue;
        if ($eq(rc->c->flags[j], sm_lit))     continue;
        if ($eq(rc->c->flags[j], q_lit))      continue;
        if ($eq(rc->c->flags[j], qlong_lit))  continue;
        u8csbFeed1(child_args, rc->c->flags[j]);
        if (!u8csEmpty(rc->c->flags[j + 1]))
            u8csbFeed1(child_args, rc->c->flags[j + 1]);
    }
    for (u32 j = 0; j < rc->c->nuris; j++) {
        uri *u = &rc->c->uris[j];
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

    //  Child runs its own BEPost: depth recursion is its responsibility.
    //  bepost_spawn_sub returns OK with `postnone=YES` when the sub had
    //  nothing to commit; we skip the bump in that case so the parent
    //  doesn't move that gitlink (plan §POST).
    b8 postnone = NO;
    ok64 r = bepost_spawn_sub(rc->wt_root, subpath, child_argv, &postnone);
    if (r != OK) { rc->worst = r; return OK; }
    if (rc->dry_only) return OK;       //  status-only walk; no bump.
    if (postnone) return OK;

    //  Sub returned OK with a real commit.  Fork `be put <subpath>` in
    //  the parent so the parent's wtlog gets a `put <subpath>#<40-hex>`
    //  row at the new tip; BEPostLocal picks that up and emits the
    //  gitlink ADD in the parent commit.
    ok64 br = bepost_bump_sub(subpath);
    if (br != OK) rc->worst = br;
    return OK;
}


//  `be post <uri>` — post-order recursion across mounted submodules
//  (SUBS.plan.md §POST).  Each sub commits its own changes first; the
//  parent then reads each sub's new tip via `be put <subpath>` to
//  stage a `put <subpath>#<40-hex>` row, and commits last so the new
//  tree records the bumped gitlinks.
//
//  Cases that skip the wrapper (recursion adds nothing):
//    * transport scheme present (`be post ssh://...`) — explicit URL
//      targets a single project per SUBS.plan.md §"URI/argv rules".
//    * bare status (no msg, no URIs)                  — dry-run printer.
//    * no wt root resolved                            — fresh-dir bootstrap.
//
//  TODO (next): `--dry-run` aggregation pass (post/13), `--sub-msg`
//  per-sub message override (post/14), `.gitmodules` synth when the
//  live mount set differs from baseline (post/15), post-order push
//  for `//origin` (post/16).
static ok64 BEPost(cli *c, b8 seq) {
    sane(c);

    //  Same transport / msg detection as BEPostLocal so we can decide
    //  whether to recurse before any commit work runs.  No flag/URI
    //  mutation here — BEPostLocal repeats the parsing.
    b8 has_transport = NO;
    for (u32 i = 0; i < c->nuris; i++) {
        if (!u8csEmpty(c->uris[i].scheme))    has_transport = YES;
    }
    b8 has_msg = NO;
    for (u32 i = 0; i < c->nuris; i++) {
        if (!u8csEmpty(c->uris[i].fragment)) { has_msg = YES; break; }
    }
    if (!has_msg) {
        a_cstr(mf, "-m");
        for (u32 fi = 0; fi + 1 < c->nflags; fi += 2) {
            if ($eq(c->flags[fi], mf)) { has_msg = YES; break; }
        }
    }

    //  Plan §"dry-run pass": explicit `--dry-run` walks the forest
    //  reporting per-level dirty paths / msg-resolution / .gitmodules
    //  drift, never committing or bumping.  Takes precedence over
    //  the bare-status short-circuit so the recursion fires even when
    //  the parent invocation has no msg / no URIs.
    b8 dry_only = CLIHas(c, "--dry-run") ? YES : NO;

    //  Bare status (no msg + no URIs): dry-run printer; let the
    //  per-project body do auto-resolve (possibly committing via
    //  in-scope patch rows, possibly refusing POSTNOMSG).  No
    //  forest recursion — bare-mode is intentionally a single-
    //  project status / auto-resolve probe.
    //  Transport scheme: explicit URL — targets one project per
    //  SUBS.plan.md §"URI/argv rules".
    b8 bare_status = !has_msg && c->nuris == 0;
    if (!dry_only && (bare_status || has_transport))
        return BEPostLocal(c, seq);

    //  Need a wt root to enumerate / chdir from.
    if (!u8bHasData(c->repo)) return BEPostLocal(c, seq);
    a_dup(u8c, wt_root, $path(c->repo));

    //  Compute parent's effective commit msg (from `#frag` or `-m`).
    //  The cb uses this to derive each sub's msg, either via the
    //  decoration `<parent_msg> [<subpath>]` or a `--sub-msg
    //  <subpath>=<msg>` override.
    u8cs parent_msg = {};
    for (u32 i = 0; i < c->nuris; i++) {
        if (!u8csEmpty(c->uris[i].fragment)) {
            u8csMv(parent_msg, c->uris[i].fragment);
            break;
        }
    }
    if (u8csEmpty(parent_msg)) {
        a_cstr(mf_lit, "-m");
        for (u32 fi = 0; fi + 1 < c->nflags; fi += 2) {
            if ($eq(c->flags[fi], mf_lit)) {
                u8csMv(parent_msg, c->flags[fi + 1]);
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
        //  Parent's status: spawn a bare `sniff post` (no msg, no
        //  URIs) so we emit the same dirty-paths / "0 changes"
        //  report the per-project body would print.  Skip
        //  BEPostLocal entirely — it would commit when a msg is
        //  present (no `--dry-run` plumbing in sniff yet).
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
        return rc.worst;
    }

    //  Sub failure bail-out: any sub that exited non-OK (other than
    //  POSTNONE, which the cb already swallowed as a no-op) halts
    //  the cascade.  Committing the parent on top of a half-applied
    //  forest lands a tree that misses the failed sub's gitlink
    //  bump (or worse, references the unchanged old sha while the
    //  user thinks the cascade ran).  Refusing here keeps the user
    //  in a clean retry state — fix / retry the sub, then re-run
    //  `be post`.
    if (rc.worst != OK) {
        fprintf(stderr,
                "be: post: aborting parent commit — sub recursion "
                "failed (worst=%s).  Resolve the sub and retry.\n",
                ok64str(rc.worst));
        return rc.worst;
    }

    //  Now the parent commit.  After bumps were staged by
    //  bepost_bump_sub above, BEPostLocal sees them as put-rows and
    //  the gitlink decisions fold into this commit.
    ok64 r = BEPostLocal(c, seq);
    if (r != OK) return r;
    return rc.worst;
}

// --- Bare `be`: --update all dogs, then --status each ---

//  Bare `be` — overview of the working tree.  Forwards to bare
//  `sniff`, which lists Changed: and Untracked: against the baseline
//  tree (untracked-but-gitignored filtered).  spot / graf / keeper
//  dogs aren't surfaced here — they're index/storage layers without
//  user-relevant state to print.  Adding their summaries back is a
//  one-liner per dog if it ever matters.
static ok64 BEDefault(void) {
    sane(1);
    a_cstr(sniff_s, "sniff");
    a_pad(u8cs, args, 1);
    u8csbFeed1(args, sniff_s);
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

    if (CLIHas(c, "-h") || CLIHas(c, "--help")) {
        BEUsage();
        done;
    }

    //  Per-verb bareword default (VERBS.md §"Bareword defaults"):
    //  promote a bareword sitting in u->path into the verb's natural
    //  slot.  POST → fragment (commit msg); GET / HEAD / PATCH →
    //  query (branch); PUT / DELETE / verbless → path (no-op).  When
    //  a promotion fires we also rewrite u->data with a leading `?`
    //  or `#` so be_build_argv forwards a URI shape that sub-dogs
    //  re-parse the same way (no second round of bareword promotion
    //  at the sub-dog layer).  Bareword bytes get packed into one
    //  scratch buffer that lives for becli's full frame (covers the
    //  later BEDispatch → be_build_argv hand-off).
    a_pad(u8, bareword_scratch, CLI_MAX_URIS * 65);
    {
        u8 def = 'p';
        if (!$empty(c->verb)) {
            a_cstr(_v_post,  "post");
            a_cstr(_v_get,   "get");
            a_cstr(_v_head,  "head");
            a_cstr(_v_patch, "patch");
            a_cstr(_v_diff,  "diff");
            if      ($eq(c->verb, _v_post))  def = 'f';
            else if ($eq(c->verb, _v_get))   def = 'q';
            else if ($eq(c->verb, _v_head))  def = 'q';
            else if ($eq(c->verb, _v_patch)) def = 'q';
            else if ($eq(c->verb, _v_diff))  def = 'q';
        }
        if (def != 'p') {
            for (u32 i = 0; i < c->nuris; i++) {
                uri *u = &c->uris[i];
                u8cs orig_path = {u->path[0], u->path[1]};
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
            c->nuris > 0)
            (void)be_sub_shard_setup(c, &c->uris[0]);
    }

    //  Read the wt's tip URI (`<root>?<branch>#<sha>`) once, here at
    //  the top of the call chain, and stash it for `BEDispatch` to
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
    if ($empty(c->verb) && c->nuris == 0 && c->nflags == 0) {
        call(BEDefault);
        done;
    }

    // Classify verb
    a_cstr(v_head,   "head");
    a_cstr(v_get,    "get");    a_cstr(v_put,    "put");
    a_cstr(v_post,   "post");   a_cstr(v_delete, "delete");
    a_cstr(v_diff,   "diff");   a_cstr(v_patch,  "patch");
    a_cstr(v_merge,  "merge");  a_cstr(v_sync,   "sync");
    a_cstr(v_status, "status");

    u8cs verb = {};
    $mv(verb, c->verb);

    // Get first URI if available
    uri *u = (c->nuris > 0) ? &c->uris[0] : NULL;

    b8 seq = CLIHas(c, "--seq");

    //  Projector URIs are pure views (VERBS.md Invariant 7).  Route
    //  them through BEProjector regardless of verb — `be get diff:f?r`
    //  must land in graf's diff machinery, not in BEGet's keeper+sniff
    //  checkout pipeline.  GET is the canonical projector verb per
    //  VERBS.md, but the table only specifies the read-only intent;
    //  any verb on a projector URI is treated as GET-equivalent here.
    if (u != NULL && DOGIsProjector(u->scheme)) {
        call(BEProjector, c, u);
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
            a_pad(u8cs, args, 2);
            u8csbFeed1(args, bro);
            u8csbFeed1(args, u->data);
            a_dup(u8cs, argv, u8csbData(args));
            call(BERun, bro, argv, NO);
        } else {
            call(BEDefault);
        }
    } else if ($eq(verb, v_head)) {
        call(BEHead, c, seq);
    } else if ($eq(verb, v_get)) {
        call(BEGet, c, seq);
    } else if ($eq(verb, v_post)) {
        call(BEPost, c, seq);
    } else if ($eq(verb, v_put)) {
        call(BEPut, c, seq);
    } else if ($eq(verb, v_delete)) {
        call(BEDelete, c, seq);
    } else if ($eq(verb, v_status)) {
        call(BEDefault);
    } else if ($eq(verb, v_diff)) {
        call(BEDiff, c, seq);
    } else if ($eq(verb, v_patch)) {
        call(BEPatch, c, seq);
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
    try(becli_inner, &c);
    ok64 ret = __;
    //  `-q` / `--quiet` swallows POSTNONE (a no-op signal, not a
    //  real error) so the outer `be post` recursion's
    //  bepost_spawn_sub passes `-q` to every sub-shard and gets
    //  clean output for shards with no changes.
    if (ret == POSTNONE &&
        (CLIHas(&c, "-q") || CLIHas(&c, "--quiet")))
        ret = OK;
    PATHu8bFree(c.repo);
    return ret;
}

MAIN(becli);
