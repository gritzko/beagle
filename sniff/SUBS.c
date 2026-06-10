//  SUBS — submodule plumbing.  See SUBS.h.

#include "SUBS.h"

#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

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

//  YES iff a == b (byte-exact).
static b8 subs_eq(u8cs a, u8cs b) {
    if (u8csLen(a) != u8csLen(b)) return NO;
    if (u8csLen(a) == 0) return YES;
    return memcmp(a[0], b[0], u8csLen(a)) == 0;
}

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
        u8c const *colon = NULL;
        $for(u8c, p, scan) if (*p == ':') { colon = p; break; }
        if (colon && colon + 2 < scan[1] &&
            colon[1] == '/' && colon[2] == '/') {
            work[0] = colon + 3;
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
    while (!u8csEmpty(work) && *(work[1] - 1) == '/')
        u8csShed1(work);

    //  Last '/' segment is the basename candidate.
    u8cs base = {};
    u8cs tail = {};
    if (subs_rfind(work, '/', tail)) {
        base[0] = tail[0] + 1;
        base[1] = work[1];
    } else {
        u8csMv(base, work);
    }

    //  Strip trailing ".git".  Strip even if the result becomes empty
    //  (`.git` URL); the empty check below rejects that case.
    if (u8csLen(base) >= 4) {
        u8c const *suf = base[1] - 4;
        if (suf[0] == '.' && suf[1] == 'g' && suf[2] == 'i' && suf[3] == 't')
            for (int i = 0; i < 4; i++) u8csShed1(base);
    }

    if (u8csEmpty(base)) return SUBSPARSE;
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

typedef struct { u8bp loc; ron60 vget; b8 beagle; b8 found; } subs_loc_ctx;

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
    c->found = YES;
    return OK;   // keep scanning — last (newest) match wins
}

static ok64 subs_recover_locator(u8bp loc, b8 *is_beagle) {
    sane(loc && is_beagle);
    *is_beagle = NO;
    u8bReset(loc);
    a_path(keepdir);
    call(HOMEBranchDir, KEEP.h, keepdir, NULL);
    subs_loc_ctx c = {.loc = loc, .vget = REFSVerbGet()};
    (void)REFSEachRecord($path(keepdir), subs_loc_cb, &c);
    *is_beagle = c.beagle;
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

ok64 SNIFFSubMount(u8cs reporoot, u8cs parent_root,
                   u8cs path, u8cs hex_sha,
                   u8cs gitmodules, u8cs argv0) {
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

    fprintf(stderr,
            "SUBS.dbg: SubMount path=" U8SFMT " hex=" U8SFMT
            " reporoot=" U8SFMT " parent_root=" U8SFMT
            " already_mounted=%s\n",
            u8sFmt(path), u8sFmt(hex_sha),
            u8sFmt(reporoot), u8sFmt(parent_root),
            already_mounted ? "YES" : "NO");

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
            if (subs_eq(cur_tip, hex_sha)) {
                fprintf(stderr,
                        "SUBS.dbg: SubMount path=" U8SFMT
                        " already at pin — skip fetch\n",
                        u8sFmt(path));
                done;
            }
        }
    }

    //  1. URL lookup.  The URL bytes get copied into a frame-local
    //  buffer so the slice remains valid past SubsParseFind's return.
    a_pad(u8, url_buf, 2048);
    u8cs url = {};
    call(SNIFFSubsParseFind, gitmodules, path, url_buf, url);

    //  2. Basename.
    u8cs basename = {};
    call(SNIFFSubBasename, url, basename);

    fprintf(stderr,
            "SUBS.dbg: SubMount url=" U8SFMT " basename=" U8SFMT "\n",
            u8sFmt(url), u8sFmt(basename));

    //  3. Sub-store dir: `<parent_root>/.be/<basename>/`.  Top-level
    //  subdir of the parent's `.be/`, identical shape to the parent's
    //  trunk dir (NNNNN.keeper, refs, wtlog).  Seed refs+wtlog so
    //  HOME walk-up classifies the dir as a well-formed store.  Skip
    //  the seed creates on re-fetch — `FILECreate` is O_TRUNC and
    //  would zero a working shard's reflog.
    a_path(store_dir);
    call(PATHu8bFeed, store_dir, parent_root);
    a_cstr(be_dir, ".be");
    call(PATHu8bPush, store_dir, be_dir);
    call(PATHu8bPush, store_dir, basename);
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
    a_path(store_root);
    a_dup(u8c, parent_s, parent_root);
    call(PATHu8bFeed, store_root, parent_s);
    call(PATHu8bPush, store_root, be_s);
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
        home *parent_home = KEEP.h;
        b8 parent_rw = (KEEP.lock_fd >= 0);

        //  Resolve the sub's fetch source while KEEP is still on the
        //  parent project.  For a beagle-remote parent the sub is a
        //  sibling project in the SAME store ("if it has the parent,
        //  it has subs"): try the parent's source locator addressing
        //  the sub by url-basename, then by path-basename, before the
        //  `.gitmodules` URL as specified.  A git-source parent has an
        //  empty/NO locator, so only the declared URL is tried (git's
        //  own resolution).  We hold the gitlink pin, so each beagle
        //  candidate is validated by whether that commit actually lands.
        a_pad(u8, loc_buf, MAX_URI_LEN);
        b8 src_beagle = NO;
        (void)subs_recover_locator(loc_buf, &src_beagle);
        u8cs pathbase = {};
        (void)SNIFFSubBasename(path, pathbase);
        a_pad(u8, cand0_buf, MAX_URI_LEN);     // <loc>?/<url-basename>
        a_pad(u8, cand1_buf, MAX_URI_LEN);     // <loc>?/<path-basename>
        if (src_beagle && u8bDataLen(loc_buf) > 0) {
            a_dup(u8c, loc_s, u8bDataC(loc_buf));
            a_cstr(qsl, "?/");
            (void)u8bFeed(cand0_buf, loc_s);
            (void)u8bFeed(cand0_buf, qsl);
            (void)u8bFeed(cand0_buf, basename);
            (void)u8bFeed(cand1_buf, loc_s);
            (void)u8bFeed(cand1_buf, qsl);
            (void)u8bFeed(cand1_buf, pathbase);
        }

        KEEPClose();

        //  Sub IS its own project at the parent's `.be/<basename>/`
        //  (per the layout `.be/<project>/<branch>` where
        //  `.be/<project>/` is the project trunk).  Opening the sub
        //  with `KEEPOpenBranch(parent_home, basename, ...)` while
        //  parent_home->project is set to the parent's project
        //  composes the leafdir as `.be/<parent-project>/<basename>/`
        //  (basename treated as a branch under parent) — wrong shard.
        //  Temporarily override parent_home->project to basename and
        //  open with empty branch (= sub's trunk) so the leafdir
        //  comes out as `.be/<basename>/`.  Restored below.
        a_dup(u8c, basename_const, basename);
        a_pad(u8, saved_proj_buf, 256);
        if (!BNULL(parent_home->project) &&
            u8bDataLen(parent_home->project) > 0) {
            a_dup(u8c, pp, u8bDataC(parent_home->project));
            u8bFeed(saved_proj_buf, pp);
        }
        u8bReset(parent_home->project);
        u8bFeed(parent_home->project, basename_const);

        //  Empty-but-valid slice so KEEPOpenBranch's $ok(branch)
        //  sanity check passes (matches KEEPOpen's idiom).
        static u8c const _zero_byte = 0;
        u8cs sub_trunk = {(u8cp)&_zero_byte, (u8cp)&_zero_byte};
        ok64 ko = KEEPOpenBranch(parent_home, sub_trunk, YES);
        ok64 fo = NONE;
        if (ko == OK && subs_pin_present(hex_sha)) {
            //  Pin already in the sub-shard (fetched into the local
            //  store out of band, e.g. a sibling clone): skip the wire
            //  fetch entirely and go straight to checkout.  Lets an
            //  offline / local-only store satisfy the gitlink without
            //  round-tripping the (possibly unreachable) declared
            //  remote.
            fprintf(stderr,
                    "SUBS.dbg: pin present in sub-shard — skip fetch\n");
            fo = OK;
            KEEPClose();
        } else if (ko == OK) {
            //  Candidate sources, in order; the declared `.gitmodules`
            //  URL is always last.  A non-final beagle candidate counts
            //  as success only when the pin lands (else fall through);
            //  the final candidate succeeds on a clean fetch (git's
            //  semantics — preserves prior behavior for git subs).
            u8cs cands[3];
            int nc = 0;
            if (src_beagle && u8bDataLen(loc_buf) > 0) {
                cands[nc][0] = u8bDataHead(cand0_buf);
                cands[nc][1] = u8bIdleHead(cand0_buf);
                nc++;
                if (!u8csEq(pathbase, basename)) {
                    cands[nc][0] = u8bDataHead(cand1_buf);
                    cands[nc][1] = u8bIdleHead(cand1_buf);
                    nc++;
                }
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
                fprintf(stderr,
                        "SUBS.dbg: sub fetch try=" U8SFMT " => %s\n",
                        u8sFmt(cu), ok64str(f));
                b8 last = (ci == nc - 1);
                if (f == OK && (last || subs_pin_present(hex_sha))) {
                    fo = OK;
                    break;
                }
                fo = (f != OK) ? f : NONE;
            }
            KEEPClose();
        } else {
            fprintf(stderr,
                    "SUBS.dbg: KEEPOpenBranch basename=" U8SFMT
                    " failed: %s\n",
                    u8sFmt(basename_const), ok64str(ko));
        }

        //  Restore parent's project + trunk open regardless of fetch
        //  outcome so cleanup paths (FILEUnLink, FILERmDir) and the
        //  caller frame see a sane KEEP.
        u8bReset(parent_home->project);
        if (u8bDataLen(saved_proj_buf) > 0) {
            a_dup(u8c, sp, u8bDataC(saved_proj_buf));
            u8bFeed(parent_home->project, sp);
        }
        u8cs trunk = {(u8cp)&_zero_byte, (u8cp)&_zero_byte};
        (void)KEEPOpenBranch(parent_home, trunk, parent_rw);

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
    //  process), so use it directly.
    a_path(sniff_exe);
    {
        char self[FILE_PATH_MAX_LEN];
        ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1);
        if (n > 0) {
            self[n] = 0;
            a_cstr(self_s, self);
            call(PATHu8bFeed, sniff_exe, self_s);
        } else {
            a_cstr(sniff_name, "sniff");
            HOMEResolveSibling(NULL, sniff_exe, sniff_name, argv0);
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
    fprintf(stderr,
            "SUBS.dbg: spawning sniff get hex=" U8SFMT
            " mount=" U8SFMT " exe=" U8SFMT "\n",
            u8sFmt(arg_s), u8sFmt(mount_s), u8sFmt(exe_s));
    ok64 sr = subs_spawn_be_get(exe_s, mount_s, arg_s);
    fprintf(stderr,
            "SUBS.dbg: spawned sniff get path=" U8SFMT " result=%s\n",
            u8sFmt(path), ok64str(sr));
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
