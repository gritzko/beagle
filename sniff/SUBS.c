//  SUBS — submodule plumbing.  See SUBS.h.

#include "SUBS.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__FreeBSD__)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/RON.h"
#include "abc/URI.h"
#include "dog/DOG.h"
#include "dog/HOME.h"
#include "dog/WHIFF.h"
#include "keeper/KEEP.h"
#include "keeper/REFS.h"
#include "keeper/WIRE.h"

#include "AT.h"
#include "SNIFF.h"

// --- helpers ----------------------------------------------------------

//  Find the last byte equal to `v` in `s`.  On hit, `out[0]=p, out[1]=s.term`.
//  Returns NO on miss.
static b8 subs_rfind(u8cs s, u8c v, u8csp out) {
    u8c const *hit = NULL;
    $for(u8c, p, s) if (*p == v) hit = p;
    if (!hit) return NO;
    out[0] = hit;
    out[1] = s[1];
    return YES;
}

// --- SubBasename ------------------------------------------------------

ok64 SNIFFSubBasename(u8cs url, u8csp out) {
    if (u8csEmpty(url)) return SUBSPARSE;

    a_dup(u8c, work, url);

    //  Strip scheme `<word>://` if present.
    {
        a_dup(u8c, scan, work);
        if (u8csFind(scan, ':') == OK) {     // scan head → `:`
            a_cstr(sep, "://");
            if (u8csHasPrefix(scan, sep)) {
                u8csUsed(scan, 3);           // past `://`
                u8csMv(work, scan);
            }
        }
    }

    //  `git@host:path` (no scheme): cut at ':'.
    {
        a_dup(u8c, scan, work);
        u8c const *colon = NULL;
        $for(u8c, p, scan) {
            if (*p == ':') { colon = p; break; }
            if (*p == '/') break;
        }
        if (colon) work[0] = colon + 1;
    }

    //  Strip URI query (`?…`) and fragment (`#…`) tails — callers
    //  hand us URIs with ref/sha slots attached (`…/parent.git?master`)
    //  but the basename is purely the path's last segment.  Without
    //  this the shard dir name picks up the `?master` and confuses
    //  every downstream URI/path parser.
    {
        u8c const *cut = NULL;
        $for(u8c, p, work) {
            if (*p == '?' || *p == '#') { cut = p; break; }
        }
        if (cut) work[1] = cut;
    }

    //  Strip trailing '/' so the last segment is non-empty when the
    //  URL ended on a slash (e.g. `…/widgets/`).
    while (!u8csEmpty(work) && *u8csLast(work) == '/')
        u8csShed1(work);

    //  Last '/' segment is the basename candidate.
    u8cs base = {};
    u8cs tail = {};
    if (subs_rfind(work, '/', tail)) {
        //  tail starts on the last '/'; basename is the rest after it.
        u8csMv(base, tail);
        u8csUsed1(base);
    } else {
        u8csMv(base, work);
    }

    //  Strip trailing ".git".  Strip even if the result becomes empty
    //  (`.git` URL); the empty check below rejects that case.
    if (u8csLen(base) >= 4) {
        a_cstr(dotgit, ".git");
        a_tail(u8c, suf, base, 4);
        if (u8csEq(suf, dotgit))
            for (int i = 0; i < 4; i++) u8csShed1(base);
    }

    if (u8csEmpty(base)) return SUBSPARSE;
    //  GET-004: a basename of literally `.be` would compose
    //  `<store>/.be/.be` — the store dir leaking in as a sub name.
    //  Refuse it; `.be` is never a valid project/sub basename.
    {
        a_cstr(be_nm, DOG_BE_NAME);
        if (u8csEq(base, be_nm)) return SUBSPARSE;
    }
    u8csMv(out, base);
    return OK;
}

// --- Sniff-side wrappers around dog/git/SUBS.  The parser lives in
//     dog/git/SUBS.{h,c} now so keeper can use it without depending
//     on sniff.  These thin forwarders preserve the existing call
//     surface across sniff and its tests.

ok64 SNIFFSubsParse(u8cs blob, sniff_subs_cb cb, void *ctx) {
    //  sniff_subs_cb and subs_cb (dog/git/SUBS.h) have the same
    //  signature; reinterpret the pointer.  C lets us pass through
    //  a function pointer of compatible type.
    return SUBSu8sParse(blob, (subs_cb)cb, ctx);
}

ok64 SNIFFSubsParseFind(u8cs blob, u8cs path, u8bp url_buf,
                        u8csp url_out) {
    return SUBSu8sFind(blob, path, url_buf, url_out);
}

ok64 SNIFFSubsSynth(u8bp out, u8cs paths, u8cs urls) {
    return SUBSu8bSynth(out, paths, urls);
}

// --- SubMount: recursive `be get` driver ------------------------------

//  Write a one-row ULOG file at `<wt>/<path>/.be`:
//      <ron60-now>\trepo\tfile:<shard_root>/\n
//  where `<shard_root>` is the sub's own keeper store dir (typically
//  `<parent_root>/.be/<basename>/`).  Mirrors `sniff_write_repo_row`
//  (sniff/SNIFF.c) and BEGetWorktree's secondary-wt seed
//  (beagle/BE.cli.c) — same row shape, same `repo` verb, same trailing
//  slash convention so `home_walk_up` reads it and dispatches to the
//  secondary path.  Routed through URIutf8Feed so the serialization
//  matches the primary writer byte-for-byte; the hand-rolled string
//  used to emit the three-slash `file:///…` form, which differs
//  textually from the primary's `file:/…` and tripped exact-match
//  comparisons.
static ok64 subs_write_anchor(u8cs sub_be_path, u8cs shard_root,
                              u8cs title, u8cs branch, u8cs hash) {
    sane($ok(sub_be_path) && $ok(shard_root));

    a_path(pathbuf);
    call(PATHu8bFeed, pathbuf, shard_root);
    //  Directory URIs carry a trailing slash.
    call(u8bFeed1, pathbuf, '/');
    call(PATHu8bTerm, pathbuf);

    uri urow = {};
    a_cstr(scheme, "file");
    urow.scheme[0] = scheme[0];
    urow.scheme[1] = scheme[1];
    {
        a_dup(u8c, pb, u8bData(pathbuf));
        urow.path[0] = pb[0];
        urow.path[1] = pb[1];
    }
    //  Sha-bearing row 0 (DIS-001): `?/<title>/<branch>#<hash>` carries
    //  the sub title + the gitlink pin; tip-less callers pass empty.
    a_path(qbuf);
    call(SNIFFAtAnchorRef, &urow, qbuf, title, branch, hash);

    a_pad(u8, row, 1024);
    ron60 ts = RONNow();
    call(RONutf8sFeed, u8bIdle(row), ts);
    call(u8bFeed1, row, '\t');
    //  Anchor verb `get` (the wt→store anchor / last-get baseline;
    //  wiki/Title.mkd), formerly `repo`.
    call(RONutf8sFeed, u8bIdle(row), SNIFFAtVerbGet());
    call(u8bFeed1, row, '\t');
    call(URIutf8Feed, u8bIdle(row), &urow);
    call(u8bFeed1, row, '\n');

    a_path(p);
    call(PATHu8bFeed, p, sub_be_path);

    int fd = FILE_CLOSED;
    call(FILECreate, &fd, $path(p));

    a_dup(u8c, body, u8bData(row));
    ok64 wo = FILEFeedAll(fd, body);
    FILEClose(&fd);
    return wo;
}

//  fork → chdir → execvp.  FILESpawn doesn't expose a `cwd`; sub
//  recursion needs the child to start inside `<wt>/<path>/` so its
//  `home_walk_up` finds the secondary-wt anchor we just wrote.  Single
//  call site, so inline rather than carving out a FILE.h knob.
static ok64 subs_spawn_be_get(u8cs exe_path, u8cs wt_path, u8cs arg) {
    sane($ok(exe_path) && $ok(wt_path) && $ok(arg));

    //  Compose NUL-terminated buffers for chdir + execvp.
    a_path(exe_buf);
    call(PATHu8bFeed, exe_buf, exe_path);

    a_path(wt_buf);
    call(PATHu8bFeed, wt_buf, wt_path);

    a_pad(u8, arg_buf, 1024);
    call(u8bFeed,  arg_buf, arg);
    call(u8bFeed1, arg_buf, 0);

    pid_t pid = fork();
    if (pid < 0) fail(SUBSPARSE);
    if (pid == 0) {
        if (chdir((char const *)u8bDataHead(wt_buf)) != 0) {
            fprintf(stderr, "sniff: sub chdir failed: %s\n", strerror(errno));
            _exit(127);
        }
        //  GET-026: silence the IMMEDIATE-checkout child's stdout.  This
        //  is an internal checkout — beagle re-runs a recursive `be get`
        //  and relays THAT as the user-facing report; this child's stdout
        //  is inherited (the relay pipe in the recursive case), so its
        //  state banner would corrupt the parent's TLV drain (TLVBADTYPE).
        {
            int dn = open("/dev/null", O_WRONLY);
            if (dn >= 0) { (void)dup2(dn, STDOUT_FILENO); if (dn != STDOUT_FILENO) close(dn); }
        }
        //  argv[0] = full path to the exe so sibling-dog lookup (if
        //  any) finds the right bin dir without relying on $PATH.
        char *argv[] = {
            (char *)u8bDataHead(exe_buf),
            (char *)"get",
            (char *)u8bDataHead(arg_buf),
            NULL,
        };
        execvp((char const *)u8bDataHead(exe_buf), argv);
        fprintf(stderr, "sniff: sub execvp failed: %s\n", strerror(errno));
        _exit(127);
    }

    int st = 0;
    for (;;) {
        pid_t r = waitpid(pid, &st, 0);
        if (r == pid) break;
        if (r < 0 && errno == EINTR) continue;
        fail(SUBSPARSE);
    }
    if (WIFEXITED(st) && WEXITSTATUS(st) == 0) done;
    fprintf(stderr, "sniff: sub checkout exited non-zero (status=%d)\n", st);
    fail(SUBSPARSE);
}

//  Seed one log file (`refs` or `wtlog`) under `store_dir`, by NAME.
//  Create-only, NEVER truncate: an existing non-empty log is left
//  byte-for-byte intact regardless of any caller-side mount guard
//  (SUBS-016 — same zeroing class as ULOG-001).  `FILECreate` opens
//  `O_TRUNC` and would `ftruncate(0)` a live shard's log, so we guard
//  it behind `FILEExists`: an already-seeded sub's log is left untouched,
//  only a genuinely-fresh sub gets the empty file.
static ok64 subs_seed_log(u8cs store_dir, u8cs name) {
    sane($ok(store_dir) && $ok(name));
    a_path(p);
    a_dup(u8c, s, store_dir);
    call(PATHu8bFeed, p, s);
    call(PATHu8bPush, p, name);
    ok64 e = FILEExists($path(p));
    if (e == OK) done;              // live log present → never truncate (SUBS-016)
    if (e != FILENONE) return e;    // real error (perms/IO) → propagate, don't create
    int fd = FILE_CLOSED;
    call(FILECreate, &fd, $path(p));
    FILEClose(&fd);
    done;
}

//  Seed both `refs` and `wtlog` so HOME walk-up classifies `store_dir`
//  as a well-formed store.  Each create is non-truncating (see
//  subs_seed_log), so this is safe to invoke even when the sub is
//  already a live mount (SUBS-016 hardening — the `!already_mounted`
//  fast-path stays an optimization, not the safety boundary).
static ok64 subs_seed_logs(u8cs store_dir) {
    sane($ok(store_dir));
    a_cstr(refs_s,  "refs");
    a_cstr(wtlog_s, "wtlog");
    call(subs_seed_log, store_dir, refs_s);
    call(subs_seed_log, store_dir, wtlog_s);
    done;
}

//  Test seam (SUBS-016): drive the sub-store log seed directly against
//  a caller-provided `store_dir` that may already hold a live, non-empty
//  `refs`/`wtlog`.  This is the false-negative-mount scenario the
//  production `!already_mounted` guard normally prevents: a unit harness
//  forces the seed to run over a populated log and asserts it is
//  preserved.  Exported, not static, so the test links it directly.
ok64 SUBSSeedLogsReproForTest(u8cs store_dir) {
    return subs_seed_logs(store_dir);
}

//  Reverse-grep the currently-open project's REFS for the most-recent
//  transport `get` row and render its source LOCATOR (scheme + auth +
//  path, NO query) into `loc`.  `*beagle` is set when that source is a
//  beagle remote — scheme `be:`/`keeper:`, or an absolute `?/project`
//  query (the multi-project addressing form).  Last matching row wins
//  (REFS rows are chronological), so each fetched sub-shard's own
//  source row makes the locator self-propagate parent→child→…
//  Local-path probes (mirror beagle/BE.cli.c be_path_is_git): a git
//  repo has `<p>/.git/objects` or bare `<p>/objects`+`<p>/refs`; a
//  beagle store has a `<p>/.be` directory.
static b8 subs_path_is_git(u8cs path) {
    if (u8csEmpty(path)) return NO;
    a_cstr(dotgit, ".git");
    a_cstr(objs,   "objects");
    a_cstr(rfs,    "refs");
    a_path(p1, path, dotgit, objs);
    if (FILEisdir($path(p1)) == OK) return YES;
    a_path(p2, path, objs);
    a_path(p3, path, rfs);
    return FILEisdir($path(p2)) == OK && FILEisdir($path(p3)) == OK;
}
static b8 subs_path_is_be_store(u8cs path) {
    if (u8csEmpty(path)) return NO;
    a_cstr(be, ".be");
    a_path(p, path, be);
    return FILEisdir($path(p)) == OK;
}

typedef struct {
    u8bp loc; ron60 vget; b8 beagle; b8 found; b8 src_is_live_git;
} subs_loc_ctx;

static ok64 subs_loc_cb(uri const *u, ron60 ts, ron60 verb, void *vc) {
    subs_loc_ctx *c = (subs_loc_ctx *)vc;
    (void)ts;
    if (verb != c->vget) return OK;
    u8cs sch  = {u->scheme[0],    u->scheme[1]};
    u8cs auth = {u->authority[0], u->authority[1]};
    u8cs pth  = {u->path[0],      u->path[1]};
    u8cs qry  = {u->query[0],     u->query[1]};
    if (u8csEmpty(sch) && u8csEmpty(auth)) return OK;   // local row, skip
    a_pad(u8, tmp, MAX_URI_LEN);
    u8cs none = {NULL, NULL};
    if (URIMake(u8bIdle(tmp), sch, auth, pth, none, none) != OK) return OK;
    a_dup(u8c, made, u8bData(tmp));
    u8bReset(c->loc);
    if (u8bFeed(c->loc, made) != OK) return OK;
    //  Beagle remote iff: scheme be:/keeper:; OR a `file:` path that is
    //  a beagle store (`.be/`) and NOT a git repo (a dogfooded checkout
    //  with both → git wins, stays git); OR an absolute `?/project`
    //  query survived (be:// cached form).  The recorded `get` row
    //  often normalizes `?/proj` → the remote branch, so the `file:`
    //  store probe is the reliable signal there.
    a_cstr(be_s,   "be");
    a_cstr(kp_s,   "keeper");
    a_cstr(file_s, "file");
    b8 file_beagle = u8csEq(sch, file_s) &&
                     !subs_path_is_git(pth) && subs_path_is_be_store(pth);
    c->beagle = u8csEq(sch, be_s) || u8csEq(sch, kp_s) || file_beagle
             || (!u8csEmpty(qry) && *qry[0] == '/');
    //  GET-011: a `file:` source whose path is a LIVE git repo means the
    //  sub is resolvable from that git source (git's own recursion) — do
    //  NOT then fall back to the local parent store (which during a fresh
    //  git→beagle bootstrap has an empty sub shard, a self-fetch that
    //  poisons the negotiation).  Only a non-git, non-reachable recorded
    //  source (deleted git path, `ssh://` upstream) warrants the fallback.
    c->src_is_live_git = u8csEq(sch, file_s) && subs_path_is_git(pth);
    c->found = YES;
    return OK;   // keep scanning — last (newest) match wins
}

//  Render `file://<store_root>` (authority-bearing TRANSPORT form) into
//  `loc` so a `<loc>?/<sub-project>` candidate addresses a sibling shard
//  of `store_root`'s `.be/` (GET-004 `?/proj` store addressing) and
//  routes through keeper upload-pack — NOT the single-slash `file:<abs>`
//  bare-path form, which be/sniff read as a local clone/restore (see
//  be_file_get_route, beagle/BE.cli.c).  `store_root` is rooted (leading
//  '/'), so `file://` + `/abs` = `file:///abs`, the same triple-slash
//  shape a typed `be get file://<store>?/<proj>` produces.
static ok64 subs_locator_from_root(u8bp loc, u8cs store_root) {
    sane(loc && $ok(store_root));
    u8bReset(loc);
    a_cstr(pfx, "file://");
    call(u8bFeed, loc, pfx);
    a_path(pathbuf);
    call(PATHu8bFeed, pathbuf, store_root);
    a_dup(u8c, pth, u8bDataC(pathbuf));
    call(u8bFeed, loc, pth);
    done;
}

//  GET-011: build the PRIMARY sub-fetch candidate from the in-flight
//  `be get` SOURCE URI — the remote we are actually talking to.  Takes
//  the source's scheme + authority + store-PATH and replaces its
//  `?/<project>` project query with `/<sub_proj>`, so
//  `file:///home/gritzko/.be?/dogs` + sub `abc` → `file:///home/gritzko/.be?/abc`.
//  The sub is a sibling project in the SAME multi-project store ("if it
//  has the parent, it has subs"); addressing it by `?/<sub_proj>` routes
//  through keeper upload-pack against that store.  `out` receives the
//  rendered candidate text.
//
//  Fires only when `src_uri` is a BEAGLE MULTI-PROJECT remote — a
//  transport URI (scheme `be`/`keeper`, or `file` in the authority-
//  bearing `file://…` form) that ALSO carries a `?/<project>` shard
//  query.  That `?/<proj>` query is the discriminator: a beagle store
//  clone is always addressed `<store>?/<proj>` (GET-004), whereas a git
//  `file:` source — which be_file_get_route rewrites to the same
//  triple-slash `file:///abs` transport shape — carries NO query and is
//  git's own to recurse.  A bare `file:<path>` (no authority, a local
//  sibling-worktree route) and a query-less transport both return NONE,
//  so the caller keeps the `.gitmodules` URL / git resolution.
//  `sub_proj` is the sub's path-basename (e.g. `abc`), never the
//  `.gitmodules` url-basename.
//
//  HEAD-004: exported (was static) so beagle's HEAD transport-mode sub
//  recursion can build the same parent-source primary candidate the GET
//  path uses, instead of peeking the dead declared `.gitmodules` URL.
ok64 SNIFFSubCandidateFromSource(u8bp out, u8cs src_uri,
                                 u8cs sub_proj) {
    sane(out);
    if (u8csEmpty(src_uri) || u8csEmpty(sub_proj)) return NONE;

    //  Lex a private copy (URILexer consumes its data slice).
    a_dup(u8c, work, src_uri);
    uri u = {};
    u8csMv(u.data, work);
    if (URILexer(&u) != OK) return NONE;

    u8cs sch  = {u.scheme[0],    u.scheme[1]};
    u8cs auth = {u.authority[0], u.authority[1]};
    u8cs pth  = {u.path[0],      u.path[1]};
    u8cs qry  = {u.query[0],     u.query[1]};

    //  Transport classification.  `be:`/`keeper:` are always wire
    //  remotes.  A `file:` source is a transport only in the
    //  authority-bearing form (`file://…`); the bare `file:<path>` form
    //  (authority absent) is the local sibling-worktree route.
    a_cstr(be_s,   "be");
    a_cstr(kp_s,   "keeper");
    a_cstr(file_s, "file");
    b8 transport = u8csEq(sch, be_s) || u8csEq(sch, kp_s) ||
                   (u8csEq(sch, file_s) && u.authority[0] != NULL);
    if (!transport) return NONE;

    //  Multi-project gate: the source must address a project shard
    //  (`?/<proj>`).  No such query → a git source (or a project-less
    //  bare store form); git keeps its own recursion, so return NONE.
    if (u8csEmpty(qry) || *qry[0] != '/') return NONE;

    //  New query = `/<sub_proj>` — the GET-004 `?/proj` shard form.
    a_pad(u8, qbuf, 512);
    call(u8bFeed1, qbuf, '/');
    call(u8bFeed,  qbuf, sub_proj);
    a_dup(u8c, subq, u8bDataC(qbuf));

    u8bReset(out);
    u8cs none = {NULL, NULL};
    a_pad(u8, made, MAX_URI_LEN);
    call(URIMake, u8bIdle(made), sch, auth, pth, subq, none);
    a_dup(u8c, made_s, u8bData(made));
    call(u8bFeed, out, made_s);
    done;
}

//  SUBS-020: YES iff the parent SOURCE URI's PATH ends in `.git` — the
//  git-parent discriminator.  See SUBS.h; exported for the POST mirror
//  (beagle/BE.cli.c) so push routing reuses the same kind test as GET.
b8 SNIFFSubSrcEndsGit(u8cs src_uri) {
    if (u8csEmpty(src_uri)) return NO;
    a_dup(u8c, work, src_uri);
    uri u = {};
    u8csMv(u.data, work);
    if (URILexer(&u) != OK) return NO;
    a_dup(u8c, pth, u.path);
    a_cstr(dotgit, ".git");
    return u8csHasSuffix(pth, dotgit);
}

//  SUBS-020 case 3: parent is a GIT repo whose URI does NOT end in
//  `.git`; compute the sub's URI by resolving the declared `.gitmodules`
//  `url` (which may be relative, e.g. `../sub`) against the parent
//  SOURCE URI.  See SUBS.h; exported for the POST mirror
//  (beagle/BE.cli.c) so push routing reuses the same parent-relative
//  resolution as GET.  Git resolves a relative submodule URL with the
//  SUPERPROJECT URL treated as a DIRECTORY (so `../sub` off `…/subs/par`
//  is `…/subs/sub`, not RFC 3986's file-relative `…/sub`); we append a
//  trailing '/' to the parent path before URIAbsolute (RFC 3986 §5.3
//  directory base).
ok64 SNIFFSubCandidateGitRel(u8bp out, u8cs src_uri, u8cs url) {
    sane(out);
    if (u8csEmpty(src_uri) || u8csEmpty(url)) return NONE;

    //  Lex the parent SOURCE URI.
    a_dup(u8c, base_work, src_uri);
    uri base = {};
    u8csMv(base.data, base_work);
    if (URILexer(&base) != OK) return NONE;
    a_dup(u8c, bsch,  base.scheme);
    a_dup(u8c, bauth, base.authority);
    a_dup(u8c, bpath, base.path);
    if (u8csEmpty(bpath)) return NONE;

    //  Re-render the base with a DIRECTORY path (trailing '/') so the
    //  relative ref resolves git-style (parent URL = directory), then
    //  re-lex that as the resolution base.
    a_pad(u8, dirbase, MAX_URI_LEN);
    u8cs none = {};
    a_pad(u8, dirpath, FILE_PATH_MAX_LEN);
    call(u8bFeed,  dirpath, bpath);
    a_cstr(slash, "/");
    if (!u8csHasSuffix(bpath, slash)) call(u8bFeed1, dirpath, '/');
    a_dup(u8c, dirpath_s, u8bDataC(dirpath));
    call(URIMake, u8bIdle(dirbase), bsch, bauth, dirpath_s, none, none);
    a_dup(u8c, dirbase_s, u8bData(dirbase));
    uri dbase = {};
    u8csMv(dbase.data, dirbase_s);
    if (URILexer(&dbase) != OK) return NONE;

    //  Lex the declared sub URL as the relative reference.  CLEAR its
    //  `data` slot afterward: URIAbsolute treats a lexed-from-text
    //  rootless ref as VERBATIM (round-trip identity) and would skip the
    //  base-directory merge for `../sub`; a data-less rel merges per
    //  RFC 3986 §5.3 (the same path URIRelative-produced refs take).
    a_dup(u8c, rel_work, url);
    uri rel = {};
    u8csMv(rel.data, rel_work);
    if (URILexer(&rel) != OK) return NONE;
    $null(rel.data);

    //  PTR-009: URIAbsolute writes the merged path into caller-owned
    //  `abs_scr`; `abs.path` views its written prefix, so the region must
    //  outlive the URIutf8Feed below (it lives in this frame, freed on
    //  return).  Pass the pad's full idle range as the writable `u8s`.
    uri abs = {};
    a_pad(u8, abs_scr, MAX_URI_LEN);
    if (URIAbsolute(&abs, &dbase, &rel, u8bIdle(abs_scr)) != OK) return NONE;

    u8bReset(out);
    if (URIutf8Feed(u8bIdle(out), &abs) != OK) return NONE;

    //  An already-absolute declared URL resolves to itself — no distinct
    //  candidate, the caller's final `url` fallback covers it.
    a_dup(u8c, made, u8bDataC(out));
    if (u8csEq(made, url)) { u8bReset(out); return NONE; }
    done;
}

//  GET-011: recover the locator of the remote the parent is actually
//  being cloned from / talking to, so each submodule can be fetched
//  from that SAME source (the sibling sub shard), with the declared
//  `.gitmodules` URL only as a last-resort fallback.
//
//  Two sources, in order:
//    1. The parent project's REFS `get` rows (newest beagle row wins) —
//       self-propagates parent→child→… when a chain of beagle remotes
//       recorded their sources.
//    2. FALLBACK — the live parent STORE itself (`store_root`).  A
//       worktree's own beagle store is, by the "if it has the parent,
//       it has subs" invariant, the authoritative LOCAL source for any
//       sibling sub shard.  This covers the real `~/.be/<proj>` case
//       where the recorded `get` row names an unreachable upstream
//       (`ssh://…`, or a deleted `file://` git source) rather than the
//       `file:<store>` we are presently addressing: subs_loc_cb then
//       reports `!beagle`, and without this fallback only the declared
//       (offline) URL would be tried.  `store_root` is the parent wt's
//       open store root (`KEEP.h->root`); it is a beagle store iff it
//       holds a `.be/` dir, so the fallback fires only for a genuine
//       local beagle parent (a git-source parent keeps an empty/NO
//       locator → git's own resolution, preserving prior behavior).
//  `via_parent_store` (optional out) is set YES only when the locator
//  came from the parent-store FALLBACK (case 2) rather than a recovered
//  REFS row (case 1) — the signal that this is the local-store GET-011
//  case where the sub may already sit in a sibling shard.
static ok64 subs_recover_locator(u8bp loc, b8 *is_beagle, u8cs store_root,
                                 b8 *via_parent_store) {
    sane(loc && is_beagle);
    *is_beagle = NO;
    if (via_parent_store) *via_parent_store = NO;
    u8bReset(loc);
    a_path(keepdir);
    call(HOMEBranchDir, keepdir, NULL);
    subs_loc_ctx c = {.loc = loc, .vget = REFSVerbGet()};
    (void)REFSEachRecord($path(keepdir), subs_loc_cb, &c);
    if (c.beagle) { *is_beagle = YES; done; }
    //  No recoverable beagle row.  Fall back to the live parent store —
    //  UNLESS the recorded source is a reachable git repo (the sub is
    //  git's to resolve; a self-fetch from the still-empty bootstrap
    //  shard would only poison the negotiation).  The fallback fires
    //  only for a genuine local beagle parent whose recorded upstream is
    //  unreachable (deleted git path / `ssh://`), i.e. the real
    //  `~/.be/<proj>` GET-011 case.
    if (!c.src_is_live_git && !u8csEmpty(store_root) &&
        subs_path_is_be_store(store_root) && !subs_path_is_git(store_root)) {
        ok64 lo = subs_locator_from_root(loc, store_root);
        if (lo == OK) {
            *is_beagle = YES;
            if (via_parent_store) *via_parent_store = YES;
            done;
        }
    }
    if (!c.found) return NONE;
    done;
}

//  YES iff the gitlink pin `hex_sha` (40 hex) is present in the
//  currently-open keeper — used to validate a candidate sub fetch.
static b8 subs_pin_present(u8cs hex_sha) {
    if (u8csLen(hex_sha) != 40) return NO;
    a_dup(u8c, hx, hex_sha);
    return KEEPHas(WHIFFHexHashlet60(hx), 40) == OK;
}

//  GET-011: YES iff a sibling shard `<parent_home root>/.be/<shard>/`
//  exists AND already holds the gitlink pin.  Used to detect that the
//  sub is locally satisfiable from a sibling project shard of the very
//  store we are operating on — the in-place re-get case where the
//  recorded source is unreachable and a wire round-trip to the same
//  store would be a self-fetch.  The parent KEEP is open (RW) on entry;
//  this closes it, opens the sibling shard READ-ONLY, probes, then
//  closes and reopens the parent's trunk in its original mode so the
//  caller frame sees the same KEEP it started with.  A missing or empty
//  shard simply reports NO.  `shard` must be a non-empty, non-`.be`
//  basename (the caller passes a path/url basename).
static b8 subs_sibling_shard_has_pin(u8cs shard, u8cs hex_sha) {
    if (u8csEmpty(shard) || u8csLen(hex_sha) != 40)
        return NO;
    //  A populated project shard has a non-empty `refs` (HOMENOPROJ
    //  guards the empty/seeded case) — cheap existence gate before the
    //  keeper churn.
    if (HOMEProjectExists(shard) != OK) return NO;

    b8 parent_rw = (KEEP.lock_fd >= 0);

    //  Preserve the parent project (on the `&HOME` singleton), then
    //  close + reopen on `shard`.  KEEPOpenBranch is a no-op when KEEP is
    //  already open (returns KEEPOPEN, keeps the current shard), so the
    //  close is mandatory to actually switch shards.
    a_pad(u8, saved_proj, 256);
    if (!BNULL(HOME.project) && u8bDataLen(HOME.project) > 0) {
        a_dup(u8c, pp, u8bDataC(HOME.project));
        u8bFeed(saved_proj, pp);
    }

    KEEPClose();
    u8bReset(HOME.project);
    {
        a_dup(u8c, sh, shard);
        u8bFeed(HOME.project, sh);
    }

    static u8c const _zb = 0;
    u8cs trunk = {(u8cp)&_zb, (u8cp)&_zb};
    b8 has = NO;
    if (KEEPOpenBranch(trunk, NO) == OK) {
        has = subs_pin_present(hex_sha);
        KEEPClose();
    }

    //  Restore parent project + trunk open in its original mode.
    u8bReset(HOME.project);
    if (u8bDataLen(saved_proj) > 0) {
        a_dup(u8c, sp, u8bDataC(saved_proj));
        u8bFeed(HOME.project, sp);
    }
    u8cs trunk2 = {(u8cp)&_zb, (u8cp)&_zb};
    (void)KEEPOpenBranch(trunk2, parent_rw);
    return has;
}

ok64 SNIFFSubMount(u8cs reporoot, u8cs parent_root,
                   u8cs path, u8cs hex_sha,
                   u8cs gitmodules, u8cs argv0, u8cs src_uri) {
    sane($ok(reporoot) && $ok(parent_root) && $ok(path) &&
         $ok(hex_sha) && DOGIsFullSha(hex_sha));

    //  Idempotency probe: if the anchor `<mount>/.be` is already a
    //  regular file, this is a re-fetch (sync-existing-mount-to-new-pin)
    //  rather than a first-time mount.  In that case we:
    //    * skip the seed FILECreate for refs/wtlog (O_TRUNC would wipe
    //      a working shard's reflog),
    //    * skip rewriting the anchor (already correct),
    //    * still run the WIREFetchAll + child checkout,
    //    * do NOT rollback the anchor / store_dir on fetch failure
    //      (keep the existing working state).
    b8 already_mounted = SNIFFSubIsMount(reporoot, path);

    //  Fast path: sub already mounted AND at the requested pin.
    //  Skip WIREFetchAll + child checkout — both are no-ops on the
    //  happy path and the fetch round-trips github/ssh for nothing.
    //  Matches git-submodule semantics: don't refetch when the wt is
    //  already at the pin recorded in the parent's tree.
    if (already_mounted) {
        a_pad(u8, hexpad, 40);
        ok64 tr = SNIFFSubReadTip(reporoot, path, u8bIdle(hexpad));
        if (tr == OK) {
            u8bFed(hexpad, 40);
            a_dup(u8c, cur_tip, u8bDataC(hexpad));
            if (u8csEq(cur_tip, hex_sha)) {
                done;
            }
        }
    }

    //  1. URL lookup.  The URL bytes get copied into a frame-local
    //  buffer so the slice remains valid past SubsParseFind's return.
    a_pad(u8, url_buf, 2048);
    u8cs url = {};
    call(SNIFFSubsParseFind, gitmodules, path, url_buf, url);

    //  Recover the parent's fetch-source locator while KEEP is still on
    //  the parent project (the recovery scans the parent's REFS).  Held
    //  in frame buffers + flags so the fetch block below reuses them.
    a_pad(u8, loc_buf, MAX_URI_LEN);
    b8 src_beagle = NO;
    b8 src_via_parent_store = NO;
    (void)subs_recover_locator(loc_buf, &src_beagle, parent_root,
                               &src_via_parent_store);

    //  2. Basename.  The sub-shard is normally named by the declared
    //  `.gitmodules` URL's basename.  GET-011: when the locator came
    //  from the PARENT-STORE fallback (the local-store case — the
    //  recorded source is unreachable and the live store is the only
    //  candidate), and a SIBLING shard named by the sub's PATH-basename
    //  already holds the gitlink pin (e.g. `~/.be/abc` while the URL
    //  declares `libabc`), prefer that path-basename so the sub mounts
    //  directly from the present sibling shard — no wire round-trip
    //  (which, against the same store, would be a self-fetch the
    //  recorded-but-unreachable source can't satisfy).  Every other case
    //  (recovered beagle remote, fresh cross-store clones, git parents)
    //  keeps the URL basename, preserving prior behavior.
    u8cs basename = {};
    call(SNIFFSubBasename, url, basename);

    //  Resolve the sub's PATH basename once (reused for the
    //  `<loc>?/<path-basename>` candidate below).  Backed by a frame
    //  buffer so the slice stays valid for the whole function.
    a_pad(u8, pathbase_buf, 512);
    u8cs pathbase = {};
    {
        u8cs pb = {};
        (void)SNIFFSubBasename(path, pb);
        if (!u8csEmpty(pb)) {
            call(u8bFeed, pathbase_buf, pb);
            u8csMv(pathbase,
                   ((u8cs){u8bDataHead(pathbase_buf), u8bIdleHead(pathbase_buf)}));
        }
    }

    //  GET-011 PRIMARY candidate — built from the in-flight `be get`
    //  SOURCE URI (the remote we are actually talking to), addressing
    //  the sub by its PATH-basename project in the SAME multi-project
    //  store: `file:///home/gritzko/.be?/dogs` + sub `abc` →
    //  `file:///home/gritzko/.be?/abc`.  This takes priority over the
    //  declared `.gitmodules` URL AND over any historical-REFS recovery
    //  below.  `src_inflight_ok` records whether it was built (the
    //  source is a beagle remote); the candidate text lives in
    //  `srccand_buf` for the fetch block.
    a_pad(u8, srccand_buf, MAX_URI_LEN);
    b8 src_inflight_ok = NO;
    if (!u8csEmpty(pathbase)) {
        ok64 sc = SNIFFSubCandidateFromSource(srccand_buf, src_uri, pathbase);
        if (sc == OK && u8bDataLen(srccand_buf) > 0) src_inflight_ok = YES;
    }

    //  GET-011: when the in-flight source is a beagle remote, the sub is
    //  a sibling project of the very store we are talking to.  If a
    //  SIBLING shard named by the sub's PATH-basename already holds the
    //  gitlink pin, mount from it directly — no wire round-trip (which,
    //  against the same `file://` store, would be a self-fetch).  Also
    //  covers the historical parent-store fallback (`src_via_parent_store`).
    if ((src_inflight_ok || src_via_parent_store) &&
        !u8csEmpty(pathbase) && !u8csEq(pathbase, basename) &&
        subs_sibling_shard_has_pin(pathbase, hex_sha)) {
        u8csMv(basename, pathbase);
    }

    //  3. Sub-store dir: `<parent_root>/.be/<basename>/`.  Top-level
    //  subdir of the parent's `.be/`, identical shape to the parent's
    //  trunk dir (NNNNN.keeper, refs, wtlog).  Seed refs+wtlog so
    //  HOME walk-up classifies the dir as a well-formed store.  Skip
    //  the seed creates on re-fetch — `FILECreate` is O_TRUNC and
    //  would zero a working shard's reflog.
    //  GET-004: compose `<parent_root>/.be/<basename>` through the single
    //  store-dir composer (parent_root == KEEP.h->root here; honors
    //  *.be-is-store and drops a `.be` basename).  The mkdir stays
    //  explicit — this dir IS the sub's store and must always exist.
    a_path(store_dir);
    call(HOMEBeDir, basename, store_dir);
    call(FILEMakeDirP, $path(store_dir));
    //  Seed refs+wtlog.  Hardened (SUBS-016): the seed is non-truncating
    //  (subs_seed_log uses O_CREAT|O_WRONLY, never O_TRUNC), so a live
    //  log can never be wiped here even if `already_mounted` is a
    //  false-negative.  The `!already_mounted` gate stays only as a
    //  fast-path skip (avoid two no-op opens on the re-fetch path), not
    //  as the data-loss safety boundary it formerly was.
    if (!already_mounted) {
        a_dup(u8c, s, u8bDataC(store_dir));
        call(subs_seed_logs, s);
    }

    //  4. Sub mount + secondary-wt anchor.  The anchor's row-0 URI
    //  points at the sub-shard (store_dir), NOT the parent's `.be/`,
    //  so the child `be get` opens the sub-shard cleanly.  Skip the
    //  anchor write on re-fetch — content is identical, no need to
    //  bump the row's mtime.
    a_path(mount);
    call(PATHu8bFeed, mount, reporoot);
    call(PATHu8bAdd,  mount, path);          //  multi-segment safe
    call(FILEMakeDirP, $path(mount));

    a_path(anchor);
    a_dup(u8c, mount_s, u8bDataC(mount));
    call(PATHu8bFeed, anchor, mount_s);
    a_cstr(be_s, ".be");
    call(PATHu8bPush, anchor, be_s);
    a_dup(u8c, anchor_s, u8bDataC(anchor));
    //  Anchor path is the parent store's `.be/` (store root); the sub
    //  title moves into the query `?/<basename>` and the gitlink pin
    //  into the fragment `#<hex_sha>` (sha-bearing row 0, DIS-001).
    //  Branch is empty — the gitlink pins a detached commit.
    //  GET-004: the anchor's URI base is the parent store dir
    //  `<parent_root>/.be/`.  Compose through the single composer
    //  (parent_root == KEEP.h->root; *.be-is-store handled), then add the
    //  directory-URI trailing slash.
    a_path(store_root);
    u8cs noseg = {};
    call(HOMEBeDir, noseg, store_root);
    call(u8bFeed1, store_root, '/');
    a_dup(u8c, shard_s,  u8bDataC(store_root));
    u8cs empty_br = {};
    if (!already_mounted) {
        call(subs_write_anchor, anchor_s, shard_s, basename, empty_br,
             hex_sha);
    }

    //  5. Pre-fetch the sub's pack into the SUB's keeper shard.
    //  WIREFetchAll writes through the keeper singleton; we
    //  temporarily swap KEEP from the parent's trunk to the sub-shard
    //  (opened as branch=<basename> under the parent's home so writes
    //  land in `<parent>/.be/<basename>/` only).  After the fetch we
    //  restore the parent's trunk so the rest of SubMount + the
    //  caller's open frame see the same KEEP they started with.
    //
    //  On failure, roll the anchor and (empty) shard dir back so a
    //  later retry sees a clean slate — a stranded `<mount>/.be` file
    //  would otherwise make SNIFFSubIsMount lie and bare `be` think
    //  the sub is mounted with no content.
    {
        b8 parent_rw = (KEEP.lock_fd >= 0);

        //  GET-011 PRIMARY: the in-flight `be get` SOURCE candidate
        //  (`srccand_buf`, built above from the remote on the command
        //  line) is the FIRST source tried — the sub is fetched from the
        //  exact store we are talking to.  Below it, the historical-REFS
        //  recovery candidates (`loc_buf` / `src_beagle`) act as a
        //  lower-priority fallback; the declared `.gitmodules` URL is
        //  always last (preserving git-submodule recursion for non-beagle
        //  parents).  We hold the gitlink pin, so every non-final beagle
        //  candidate is validated by whether that commit actually lands.
        //
        //  The historical wire candidates fire only when the locator came
        //  from a real REFS row (`src_beagle && !src_via_parent_store`) —
        //  the parent-STORE fallback points at the very store we operate
        //  on, where a `?/proj` wire fetch is a self-fetch; that case is
        //  satisfied locally by the path-basename sibling-shard swap
        //  above (pin present → skip fetch).  `pathbase` (the sub's PATH
        //  basename) feeds the historical path-basename candidate.
        b8 use_wire_candidates = src_beagle && !src_via_parent_store &&
                                 u8bDataLen(loc_buf) > 0;
        a_pad(u8, cand0_buf, MAX_URI_LEN);     // <loc>?/<url-basename>
        a_pad(u8, cand1_buf, MAX_URI_LEN);     // <loc>?/<path-basename>
        if (use_wire_candidates) {
            a_dup(u8c, loc_s, u8bDataC(loc_buf));
            a_cstr(qsl, "?/");
            (void)u8bFeed(cand0_buf, loc_s);
            (void)u8bFeed(cand0_buf, qsl);
            (void)u8bFeed(cand0_buf, basename);
            (void)u8bFeed(cand1_buf, loc_s);
            (void)u8bFeed(cand1_buf, qsl);
            (void)u8bFeed(cand1_buf, pathbase);
        }

        //  SUBS-020 case 3: a GIT parent (no beagle source recovered —
        //  in-flight, historical-REFS, OR local-store fallback) whose
        //  SOURCE URI does NOT end in `.git`.  Resolve the declared `url`
        //  relative to the parent URI and try that BEFORE the raw official
        //  URL.  A `.git` parent (case 2) skips this — its declared URL is
        //  taken as canonical, no path computation.
        //  A non-`.git` parent SOURCE URI gets the parent-relative
        //  candidates tried before the raw official URL (after any beagle
        //  wire candidates above, which take priority and are pin-
        //  validated).  Keyed purely on the URI FORM — symmetric with
        //  POST's bepushgit_recurse_cb, which keys on the dest URI only.
        b8 src_git_form = !SNIFFSubSrcEndsGit(src_uri);
        b8 use_git_rel = src_git_form;
        a_pad(u8, gitrel_buf, MAX_URI_LEN);    // URIAbsolute(src_uri, url)
        //  SUBS-024 case 4: an ABSOLUTE declared url resolves-to-self
        //  (case 3 → NONE), so fall back to `<src>/<subpath>` — the sub's
        //  wt path resolved against the parent SOURCE URI.  Bypasses an
        //  unreachable declared upstream (the fetch mirror of POST case 4).
        a_pad(u8, gitnest_buf, MAX_URI_LEN);   // URIAbsolute(src_uri, path)
        b8 use_git_nest = NO;
        if (use_git_rel) {
            ok64 gr = SNIFFSubCandidateGitRel(gitrel_buf, src_uri, url);
            if (gr != OK || u8bDataLen(gitrel_buf) == 0) {
                use_git_rel = NO;
                ok64 gn = SNIFFSubCandidateGitRel(gitnest_buf, src_uri, path);
                if (gn == OK && u8bDataLen(gitnest_buf) > 0) use_git_nest = YES;
            }
        }

        KEEPClose();

        //  Sub IS its own project at the parent's `.be/<basename>/`
        //  (per the layout `.be/<project>/<branch>` where
        //  `.be/<project>/` is the project trunk).  Opening the sub
        //  with `KEEPOpenBranch(basename, ...)` while
        //  parent_home->project is set to the parent's project
        //  composes the leafdir as `.be/<parent-project>/<basename>/`
        //  (basename treated as a branch under parent) — wrong shard.
        //  Temporarily override parent_home->project to basename and
        //  open with empty branch (= sub's trunk) so the leafdir
        //  comes out as `.be/<basename>/`.  Restored below.
        a_dup(u8c, basename_const, basename);
        a_pad(u8, saved_proj_buf, 256);
        if (!BNULL(HOME.project) &&
            u8bDataLen(HOME.project) > 0) {
            a_dup(u8c, pp, u8bDataC(HOME.project));
            u8bFeed(saved_proj_buf, pp);
        }
        u8bReset(HOME.project);
        u8bFeed(HOME.project, basename_const);

        //  Empty-but-valid slice so KEEPOpenBranch's $ok(branch)
        //  sanity check passes (matches KEEPOpen's idiom).
        static u8c const _zero_byte = 0;
        u8cs sub_trunk = {(u8cp)&_zero_byte, (u8cp)&_zero_byte};
        ok64 ko = KEEPOpenBranch(sub_trunk, YES);
        ok64 fo = NONE;
        if (ko == OK && subs_pin_present(hex_sha)) {
            //  Pin already in the sub-shard (fetched into the local
            //  store out of band, e.g. a sibling clone): skip the wire
            //  fetch entirely and go straight to checkout.  Lets an
            //  offline / local-only store satisfy the gitlink without
            //  round-tripping the (possibly unreachable) declared
            //  remote.
            fo = OK;
            KEEPClose();
        } else if (ko == OK) {
            //  Candidate sources, in order; the declared `.gitmodules`
            //  URL is always last.  A non-final beagle candidate counts
            //  as success only when the pin lands (else fall through);
            //  the final candidate succeeds on a clean fetch (git's
            //  semantics — preserves prior behavior for git subs).
            //  Order: [in-flight SOURCE] [historical-REFS wire…]
            //         [git-relative (SUBS-020 case 3)] [URL].
            u8cs cands[5];
            int nc = 0;
            //  GET-011 PRIMARY — the in-flight `be get` source addressing
            //  the sub's path-basename shard in the same store.
            if (src_inflight_ok) {
                u8csMv(cands[nc], u8bDataC(srccand_buf));
                nc++;
            }
            if (use_wire_candidates) {
                u8csMv(cands[nc], u8bDataC(cand0_buf));
                nc++;
                if (!u8csEq(pathbase, basename)) {
                    u8csMv(cands[nc], u8bDataC(cand1_buf));
                    nc++;
                }
            }
            //  SUBS-020 case 3 — the parent-relative resolution of the
            //  declared URL, tried before the raw official URL fallback.
            if (use_git_rel) {
                u8csMv(cands[nc], u8bDataC(gitrel_buf));
                nc++;
            }
            //  SUBS-024 case 4 — `<src>/<subpath>` (absolute-url fallback),
            //  tried before the raw official URL.
            if (use_git_nest) {
                u8csMv(cands[nc], u8bDataC(gitnest_buf));
                nc++;
            }
            u8csMv(cands[nc], url);
            nc++;
            for (int ci = 0; ci < nc; ci++) {
                a_dup(u8c, cu, cands[ci]);
                //  Fetch every advertised ref first — git-compatible and
                //  the common case (the gitlink pin is reachable from an
                //  advertised tip, so its closure carries it).  Must run
                //  on a clean shard: a git peer refuses an unadvertised
                //  `want <sha>` ("not our ref"), and a failed attempt
                //  before fetch-all corrupts the negotiation.
                ok64 f = WIREFetchAll(cu);
                //  Want-by-hash TOP-UP: if fetch-all didn't land the pin
                //  (a keeper peer whose shard advertises no ref whose
                //  closure covers it — the zero-/wrong-refs case), ask
                //  for the pin object directly.  Keeper peers serve any
                //  present object; git peers refuse and this is a no-op.
                a_dup(u8c, pin_ref, hex_sha);
                if (f == OK && !subs_pin_present(hex_sha))
                    (void)WIREFetch(cu, pin_ref);
                b8 last = (ci == nc - 1);
                if (f == OK && (last || subs_pin_present(hex_sha))) {
                    fo = OK;
                    break;
                }
                fo = (f != OK) ? f : NONE;
            }
            KEEPClose();
        }

        //  Restore parent's project + trunk open regardless of fetch
        //  outcome so cleanup paths (FILEUnLink, FILERmDir) and the
        //  caller frame see a sane KEEP.
        u8bReset(HOME.project);
        if (u8bDataLen(saved_proj_buf) > 0) {
            a_dup(u8c, sp, u8bDataC(saved_proj_buf));
            u8bFeed(HOME.project, sp);
        }
        u8cs trunk = {(u8cp)&_zero_byte, (u8cp)&_zero_byte};
        (void)KEEPOpenBranch(trunk, parent_rw);

        if (ko != OK || fo != OK) {
            fprintf(stderr,
                    "sniff: submodule fetch failed for %.*s\n",
                    (int)$len(url), (char *)url[0]);
            //  Rollback only on first-time mount.  On a re-fetch we
            //  keep the existing working mount (the user can still
            //  use the sub at its prior pin).
            if (!already_mounted) {
                (void)FILEUnLink($path(anchor));
                (void)FILERmDir($path(store_dir), false);
            }
            return ko != OK ? ko : fo;
        }
    }

    //  6. Spawn `sniff get <hex>` with cwd = sub-mount.  The child
    //  opens the parent's keeper RO (via the row-0 anchor we wrote)
    //  and checks out the gitlink-pinned commit into the mount.
    //
    //  This handles the IMMEDIATE checkout only — no further sub-of-sub
    //  recursion.  Beagle's BEGet wrapper runs a separate recursive
    //  `be get ?<pin>` against this mount after we return, picking up
    //  any deeper submodules the freshly-checked-out tree declares.
    //  Splitting it that way means a deep-leaf failure can't cascade
    //  up to roll back this sub's anchor.
    //
    //  /proc/self/exe IS `sniff` here (sniff binary is the running
    //  process), so use it directly.  On FreeBSD-native builds procfs
    //  is typically absent — fall back to sysctl(KERN_PROC_PATHNAME),
    //  then to HOMEResolveSibling (argv0 + PATH).  The argv0-based
    //  fallback can pick a stale sibling sniff from PATH when the
    //  parent invoked us via execvp(<full_path>, ["sniff", ...]) —
    //  child sees argv[0]="sniff" (bare) and PATH wins.
    a_path(sniff_exe);
    {
        char self[FILE_PATH_MAX_LEN];
        b8 have_self = NO;
        ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1);
        if (n > 0) {
            self[n] = 0;
            have_self = YES;
        }
#if defined(__FreeBSD__)
        if (!have_self) {
            int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
            size_t cb = sizeof self;
            if (sysctl(mib, 4, self, &cb, NULL, 0) == 0 && cb > 0) {
                //  sysctl returns NUL-terminated; trim trailing NULs.
                while (cb > 0 && self[cb - 1] == 0) cb--;
                self[cb] = 0;
                have_self = YES;
            }
        }
#endif
        if (have_self) {
            a_cstr(self_s, self);
            call(PATHu8bFeed, sniff_exe, self_s);
        } else {
            a_cstr(sniff_name, "sniff");
            HOMEResolveSibling(sniff_exe, sniff_name, argv0);
        }
    }

    a_pad(u8, arg_buf, 64);
    call(u8bFeed, arg_buf, hex_sha);
    a_dup(u8c, arg_s, u8bData(arg_buf));
    a_dup(u8c, exe_s, u8bDataC(sniff_exe));

    //  Release the parent's keeper write-lock for the duration of the
    //  child checkout — the child opens its own keeper handle on the
    //  same `.be/` (shared via the row-0 anchor) and blocks on
    //  LOCK_EX otherwise.  Restore on the way out.
    b8 had_lock = (KEEP.lock_fd >= 0);
    if (had_lock) (void)FILEUnlock(&KEEP.lock_fd);
    ok64 sr = subs_spawn_be_get(exe_s, mount_s, arg_s);
    if (had_lock) (void)FILELock(&KEEP.lock_fd, YES);
    if (sr != OK && !already_mounted) {
        (void)FILEUnLink($path(anchor));
        (void)FILERmDir($path(store_dir), false);
    }
    return sr;
}

// --- IsMount / ReadTip: read-only probes -------------------------------

b8 SNIFFSubIsMount(u8cs wt_root, u8cs subpath) {
    if (!$ok(wt_root) || !$ok(subpath) || u8csEmpty(subpath)) return NO;
    a_path(p);
    if (PATHu8bFeed(p, wt_root) != OK) return NO;
    if (PATHu8bAdd (p, subpath) != OK) return NO;
    a_cstr(be_s, ".be");
    if (PATHu8bPush(p, be_s) != OK) return NO;
    filestat fs = {};
    if (FILELStat(&fs, $path(p)) != OK) return NO;
    return fs.kind == FILE_KIND_REG;
}

ok64 SNIFFSubReadTip(u8cs wt_root, u8cs subpath, u8s out) {
    sane($ok(wt_root) && $ok(subpath) && !u8csEmpty(subpath));
    if ($len(out) < 40) return SUBSPARSE;

    //  Compose `<wt_root>/<subpath>` — `SNIFFAtTailOf` opens
    //  `<that>/.be` if it's a file (secondary-wt anchor) or
    //  `<that>/.be/wtlog` if it's a dir.  Either way it returns a
    //  composed `<root>?<branch>#<sha>` view; we want the fragment.
    a_path(sub_root);
    call(PATHu8bFeed, sub_root, wt_root);
    call(PATHu8bAdd,  sub_root, subpath);

    a_pad(u8, tail_buf, FILE_PATH_MAX_LEN + 128);
    a_dup(u8c, sub_root_s, u8bDataC(sub_root));
    ok64 to = SNIFFAtTailOf(sub_root_s, tail_buf);
    if (to == SNIFFNONE) return SUBSNOSEC;
    if (to != OK) return to;

    uri u = {};
    a_dup(u8c, tail_s, u8bData(tail_buf));
    u8csMv(u.data, tail_s);
    call(URILexer, &u);
    if (u8csLen(u.fragment) != 40) return SUBSNOSEC;
    u8sCopy(out, u.fragment);
    return OK;
}
