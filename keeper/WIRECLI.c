//  WIRECLI: client side of the git wire protocol (WIRE.md Phase 7).
//
//  WIREFetch — spawn ssh/local upload-pack, drain refs advertisement,
//              send wants/haves, ingest the returned packfile into the
//              local keeper, append a fresh REFS tip entry.
//  WIREPush  — spawn ssh/local receive-pack, drain its advertisement,
//              build a packfile from our reachable closure for the
//              chosen branch, send a single ref update, drain the
//              unpack/per-ref status reply.
//
//  Transport dispatch lives here (URI parsing → ssh argv | local
//  argv).  Everything else is shared with the server-side WIRE.c
//  through library primitives (PKT, REFADV, KEEPIngestFile,
//  KEEPGetExact, ZINFDeflate).

#include "WIRE.h"

#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/URI.h"
#include "dog/DOG.h"
#include "dog/git/SHA1.h"
#include "dog/git/GIT.h"
#include "dog/HOME.h"
#include "keeper/KEEP.h"
#include "keeper/WALK.h"
#include "keeper/REFS.h"
#include "dog/git/PKT.h"
#include "keeper/REFADV.h"
#include "keeper/REFS.h"
#include "dog/git/SHA1.h"
#include "dog/git/ZINF.h"

// --- pkt-line drain with refill ----------------------------------------

#define WCLI_BUF (1u << 16)

//  Drain one pkt-line, refilling from in_fd via FILEDrain on NODATA.
//  Returns OK / PKTFLUSH / PKTDELIM / WIRECLFL.
//
//  When IDLE runs out we compact: bytes already consumed via `adv` head
//  are reclaimed into IDLE so further reads have room.  Without this
//  the fixed-size WCLI_BUF (64 KiB) overruns on large advertisements —
//  vanilla git's `~/src/git` advertises ~1000 refs (≈100 KiB), enough
//  to fail mid-parse; the parent then closes pipes and the upstream
//  ssh git-upload-pack dies with SIGPIPE.
static ok64 wcli_read_pkt(int in_fd, u8b buf, u8cs adv, u8csp line) {
    for (;;) {
        ok64 o = PKTu8sDrain(adv, line);
        if (o != NODATA) return o;
        if (!u8bHasRoom(buf)) {
            size_t consumed = (size_t)(adv[0] - u8bDataC(buf)[0]);
            if (consumed == 0) return WIRECLFL;
            u8bUsed(buf, consumed);
            u8bShift(buf, 0);
            adv[0] = u8bDataC(buf)[0];
            adv[1] = u8csTerm(u8bDataC(buf));
            if (!u8bHasRoom(buf)) return WIRECLFL;
        }
        u8s fill;
        u8sFork(u8bIdle(buf), fill);
        ok64 fr = FILEDrain(in_fd, fill);
        if (fr == FILEEND) return WIRECLFL;
        if (fr != OK) return WIRECLFL;
        u8sJoin(u8bIdle(buf), fill);
        adv[1] = u8csTerm(u8bDataC(buf));
    }
}

// --- transport spawn (ssh / local) -------------------------------------
//
//  Parse `remote_uri` and decide what to exec:
//    file:///P or keeper://local/P    → exec `keeper <verb> P` locally.
//    keeper://host/P or be://host/P   → exec `ssh host keeper <verb> P`
//                                       (keeper-protocol over ssh).
//    //host/P or //host/P.git         → exec `ssh host git-<verb> P` so
//                                       a vanilla git server still works.
//                                       Bare-ssh defaults to git protocol
//                                       since most peers will be plain git
//                                       (mill-tags.sh against ~/src/git);
//                                       use keeper:// to force keeper.
//
//  `verb` is "upload-pack" (fetch) or "receive-pack" (push).  Sets
//  *out_pid + parent's stdin_w / stdout_r ends on success.

static u8c const WCLI_KEEPER_BIN_S[] = "keeper";
static u8c const WCLI_SSH_BIN_S[]    = "/usr/bin/ssh";
static u8c const WCLI_GIT_DOT_S[]    = ".git";

//  Locate the keeper binary to exec for local transport.  Honors the
//  KEEPER_BIN env var so tests can point at the just-built binary
//  without it being on $PATH.  Writes the chosen path slice into
//  `out_path` (alias of either env or the default literal).
static void wcli_keeper_bin(u8cs out_path) {
    char const *env = getenv("KEEPER_BIN");
    if (env && *env) {
        out_path[0] = (u8cp)env;
        out_path[1] = (u8cp)env + strlen(env);
        return;
    }
    out_path[0] = WCLI_KEEPER_BIN_S;
    out_path[1] = WCLI_KEEPER_BIN_S + sizeof(WCLI_KEEPER_BIN_S) - 1;
}

//  Path "P.git" (suffix) → choose vanilla git binary instead of keeper.
static b8 wcli_path_is_git(u8csc path) {
    if ((size_t)u8csLen(path) < sizeof(WCLI_GIT_DOT_S) - 1) return NO;
    u8c const *tail = path[1] - (sizeof(WCLI_GIT_DOT_S) - 1);
    return memcmp(tail, WCLI_GIT_DOT_S, sizeof(WCLI_GIT_DOT_S) - 1) == 0;
}

//  On-disk layout sniff for the local-exec branch: returns YES when
//  `path` looks like a git repo even without the `.git` suffix.
//      bare:   <path>/objects/ + <path>/refs/
//      worktree: <path>/.git/objects/
//  Falls through to the keeper code path on anything else (incl. non-
//  existent paths — the keeper binary will produce its own diagnostic).
static b8 wcli_path_is_git_layout(u8csc path) {
    if (u8csEmpty(path)) return NO;
    a_cstr(objects_s, "objects");
    a_cstr(refs_s,    "refs");
    a_cstr(dotgit_s,  ".git");
    a_path(objp, path, objects_s);
    a_path(refp, path, refs_s);
    if (FILEisdir($path(objp)) == OK && FILEisdir($path(refp)) == OK)
        return YES;
    a_path(wtobjp, path, dotgit_s, objects_s);
    if (FILEisdir($path(wtobjp)) == OK) return YES;
    return NO;
}

//  See WIRE.h.  Path, then optional absolute `?/<project>` selector.
ok64 WIREServePath(u8b out, u8csc path, u8csc query) {
    sane(u8bOK(out));
    call(u8bFeed, out, path);
    if (!u8csEmpty(query) && *query[0] == '/') {
        call(u8bFeed1, out, '?');
        call(u8bFeed, out, query);
    }
    done;
}

static ok64 wcli_spawn(u8csc remote_uri, char const *verb,
                       int *wfd, int *rfd, pid_t *pid) {
    sane(verb && wfd && rfd && pid);

    uri u = {};
    a_dup(u8c, ru, remote_uri);
    if (URIutf8Drain(ru, &u) != OK) return WIRECLFL;

    //  Path is what the peer's upload-pack sees as argv[1].  URI parser
    //  delivers it with a leading '/' for absolute forms (file:///foo,
    //  //host/foo) which is exactly what the peer expects.
    //
    //  GET-003: the empty-path rejection is scheme-SPECIFIC and lives
    //  AFTER classification below.  An empty path is legal for the
    //  keeper protocol (`be://host?/proj`): it denotes the peer's
    //  default `$HOME/.be` store, with the `?/<project>` query selecting
    //  the shard (WIREServePath emits `?/proj`; the server's
    //  keeper_served_at + home_open_inner walk-up resolve it).  Git
    //  transports (ssh/https/git) still require a path — `git-upload-pack
    //  <path>` has nowhere to go otherwise — so they keep rejecting it.
    a_dup(u8c, path, u.path);

    a_cstr(file_s,    "file");
    a_cstr(keeper_s,  "keeper");
    a_cstr(be_s,      "be");
    b8 is_file   = u8csEq(u.scheme, file_s);
    b8 is_keeper = u8csEq(u.scheme, keeper_s);
    b8 is_be     = u8csEq(u.scheme, be_s);
    b8 has_host  = !u8csEmpty(u.host);

    //  A keeper project selector: an absolute `?/<project>` query.  When
    //  present it stands in for the repo path (the shard within the
    //  peer's default store), so the keeper branches accept an empty
    //  path.  Mirrors WIREServePath's `*query[0] == '/'` gate.
    b8 has_proj_query = !u8csEmpty(u.query) && *u.query[0] == '/';

    //  Build a verb slice to pass into argv.
    a_cstr(verb_s, verb);

    //  Local exec branch: file://, keeper://local, or no host at all.
    a_cstr(local_s, "local");
    if (is_file || (is_keeper && (!has_host || u8csEq(u.host, local_s))) ||
        (!has_host && u8csEmpty(u.scheme))) {
        //  Detect a local git repo (suffix `.git` or on-disk layout).
        //  When found, exec `git-<verb> <path>` so vanilla bare/working
        //  git repos served via file:// keep working — symmetric to the
        //  ssh branch's git-<verb> dispatch below.
        b8 local_is_git = wcli_path_is_git(path) ||
                          wcli_path_is_git_layout(path);
        if (local_is_git) {
            a_pad(u8, gitverb, 32);
            a_cstr(git_dash, "git-");
            u8bFeed(gitverb, git_dash);
            u8bFeed(gitverb, verb_s);
            //  NUL-terminate so the slice satisfies path8sc's
            //  C-string contract for execvp.
            PATHu8bTerm(gitverb);
            u8cs argv_arr[2] = {
                {u8bDataHead(gitverb), u8bIdleHead(gitverb)},
                {path[0], path[1]},
            };
            u8css argv = {argv_arr, argv_arr + 2};
            u8csc gbin = {u8bDataHead(gitverb), u8bIdleHead(gitverb)};
            return FILESpawn(gbin, argv, wfd, rfd, pid);
        }
        //  Keeper peer: convey an explicit project selector (`?/proj`)
        //  so the server serves THAT shard instead of the store's
        //  row-0 default project.  Git peers (above) get the bare path
        //  — git repos are single-project.
        //
        //  GET-003: an empty path is legal only when a `?/<project>`
        //  query supplies the shard selector (`keeper://local?/proj` →
        //  the default store's `proj` shard).  Empty path AND no
        //  selector has nothing to serve.
        if (u8csEmpty(path) && !has_proj_query) return WIRECLFL;
        a_pad(u8, kpath, FILE_PATH_MAX_LEN + 64);
        call(WIREServePath, kpath, path, u.query);
        u8cs kbin = {};
        wcli_keeper_bin(kbin);
        u8cs argv_arr[3] = {
            {(u8cp)"keeper", (u8cp)"keeper" + 6},
            {verb_s[0], verb_s[1]},
            {u8bDataHead(kpath), u8bIdleHead(kpath)},
        };
        u8css argv = {argv_arr, argv_arr + 3};
        u8csc kbin_cs = {kbin[0], kbin[1]};
        return FILESpawn(kbin_cs, argv, wfd, rfd, pid);
    }

    //  ssh remote.  Default to vanilla `git-<verb>` so plain git peers
    //  (the common case — mill-tags.sh against ~/src/git, GitHub, etc.)
    //  work transparently.  Use `keeper://host/path` (or `be://`) to
    //  force the keeper protocol when both ends speak it.  `.git` suffix
    //  is honored as an extra git marker.
    a_cstr(ssh_path_s, "/usr/bin/ssh");
    a_dup(u8c, host, u.host);
    if (u8csEmpty(host)) return WIRECLFL;

    //  HOME-relative convention: //host/path delivers `path` with the
    //  URI parser's leading '/' attached.  ssh peers expect a path
    //  relative to the remote login's HOME, so strip it.  Absolute
    //  remote paths need to come through file:/// or be encoded
    //  differently — matching what keeper_get_remote did pre-Phase8.
    if (!u8csEmpty(path) && *path[0] == '/') path[0]++;

    b8 use_keeper_ssh = is_keeper || is_be;
    b8 force_git      = wcli_path_is_git(path);

    //  GET-003: empty-path rejection is scheme-specific.  A keeper peer
    //  (`be://host?/proj`) may send an empty path when the `?/<project>`
    //  query selects the shard within the peer's default `$HOME/.be`
    //  store; WIREServePath then emits a bare `?/proj` argv and the
    //  remote keeper walk-up resolves the store.  Git transports
    //  (ssh/https/git) genuinely need a repo path for
    //  `git-upload-pack <path>`, so they keep rejecting an empty path.
    if (u8csEmpty(path)) {
        if (!(use_keeper_ssh && !force_git && has_proj_query))
            return WIRECLFL;
    }

    if (use_keeper_ssh && !force_git) {
        //  ssh <host> [PATH=...] keeper <verb> <path>[?/proj]
        //
        //  Carry the `?/<project>` selector on the served path so a
        //  remote keeper peer routes to that shard, not its row-0
        //  default — symmetric with the local-exec branch above.
        a_pad(u8, spath, FILE_PATH_MAX_LEN + 64);
        call(WIREServePath, spath, path, u.query);
        a_dup(u8c, spath_s, u8bData(spath));
        //
        //  $DOG_REMOTE_PATH is prepended to the remote shell's PATH so
        //  test harnesses can point at an out-of-tree `keeper` binary
        //  without touching the remote's login rc.  When set, we have
        //  to invoke the remote command via `sh -c` so the assignment
        //  takes effect in the same process that exec()s keeper.
        char const *rpath = getenv("DOG_REMOTE_PATH");
        if (rpath && *rpath) {
            a_pad(u8, rcmd, 1024);
            a_cstr(pre1, "PATH='");
            u8bFeed(rcmd, pre1);
            a_cstr(rp_s, rpath);
            u8bFeed(rcmd, rp_s);
            a_cstr(pre2, "':\"$PATH\" exec keeper ");
            u8bFeed(rcmd, pre2);
            u8bFeed(rcmd, verb_s);
            u8bFeed1(rcmd, ' ');
            u8bFeed(rcmd, spath_s);
            u8cs argv_arr[3] = {
                {(u8cp)"ssh", (u8cp)"ssh" + 3},
                {host[0], host[1]},
                {u8bDataHead(rcmd), u8bIdleHead(rcmd)},
            };
            u8css argv = {argv_arr, argv_arr + 3};
            return FILESpawn(ssh_path_s, argv, wfd, rfd, pid);
        }
        u8cs argv_arr[5] = {
            {(u8cp)"ssh", (u8cp)"ssh" + 3},
            {host[0], host[1]},
            {(u8cp)"keeper", (u8cp)"keeper" + 6},
            {verb_s[0], verb_s[1]},
            {spath_s[0], spath_s[1]},
        };
        u8css argv = {argv_arr, argv_arr + 5};
        return FILESpawn(ssh_path_s, argv, wfd, rfd, pid);
    }
    //  ssh <host> git-<verb> <path>
    a_pad(u8, gitverb, 32);
    a_cstr(git_dash, "git-");
    u8bFeed(gitverb, git_dash);
    u8bFeed(gitverb, verb_s);
    u8cs argv_arr[4] = {
        {(u8cp)"ssh", (u8cp)"ssh" + 3},
        {host[0], host[1]},
        {u8bDataHead(gitverb), u8bIdleHead(gitverb)},
        {path[0], path[1]},
    };
    u8css argv = {argv_arr, argv_arr + 4};
    return FILESpawn(ssh_path_s, argv, wfd, rfd, pid);
}

// --- be ↔ git wire ref translation -------------------------------------
//
//  Be-side branches are opaque local paths (`""` = trunk, `"feature"`,
//  `"feat/fix"`, …).  Git wire-side refnames are `refs/heads/<X>`.  The
//  one wire alias: trunk (`""`) ⇔ git's default `refs/heads/main`.
//  Tag / remote / OTHER ref kinds are not exposed locally yet — they
//  flow through unchanged when we record a peer-observed row but have
//  no first-class be-branch counterpart.

//  be branch → wire refname into `out`.  Pre-reset.
//      ""        → "refs/heads/main"
//      "X"       → "refs/heads/X"
static ok64 wcli_be_to_wire(u8b out, u8csc be_branch) {
    sane(u8bOK(out));
    u8cs name = {};
    u8csMv(name, u8csEmpty(be_branch) ? GIT_MAIN_LIT : be_branch);
    return GITFeedRef(out, GITREF_BRANCH, name);
}

//  Wire refname → be branch.  Sets *kind_out to the parsed kind so
//  callers can decide what to do with non-branch refs (tags, HEAD, …).
//  For BRANCH: returns the bare name; trunk alias `main` collapses to
//  empty (be-side trunk).  Unsupported kinds set name to `bare`.
static ok64 wcli_wire_to_be(u8csc wire_refname, gitref_kind *kind_out,
                            u8csp name_out) {
    sane(kind_out && name_out);
    name_out[0] = name_out[1] = NULL;
    *kind_out = GITREF_NONE;
    u8cs bare = {};
    ok64 po = GITParseRef(wire_refname, kind_out, bare);
    if (po != OK) return po;
    if (*kind_out != GITREF_BRANCH) {
        u8csMv(name_out, bare);
        done;
    }
    //  Trunk alias: wire-side `main` is the be-side empty branch.
    if (u8csEq(bare, GIT_MAIN_LIT)) {
        name_out[0] = bare[1];
        name_out[1] = bare[1];
        done;
    }
    u8csMv(name_out, bare);
    done;
}

//  Match a peer-advertised wire refname against a caller-supplied
//  want_ref.  Both go through `GITParseRef` so the comparison is
//  between (kind, bare) tuples — `heads/master` vs `refs/heads/master`
//  match, `tags/v1.0` vs `refs/tags/v1.0` match, but a branch advert
//  never matches a tag want.  Bare names like `master` route to BRANCH;
//  bare `vN…` routes to TAG (matches `GITParseRef`'s heuristic).
//  Empty `want_branch` is handled by the caller (HEAD discovery), so
//  this function only deals with non-empty wants.
//
//  DIS-028: the advertised side parses with `GITParseRef`, NOT the
//  trunk-collapsing `wcli_wire_to_be` — the latter folds
//  `refs/heads/main` to be-side empty, so a literal `?main` want
//  (`want_bare = "main"`) never matched its own advertised ref and
//  the fetch failed `WIRECLFL`.  The trunk⇔`main` alias belongs only
//  to the empty-want path (caller / fallback loop), where the user
//  asked for trunk, not the literal `main`.  Matching the raw bare
//  name here resolves explicit `?main` like any other branch while
//  leaving bare-trunk discovery untouched.
static b8 wcli_refname_match(u8csc adv_name, u8csc want_branch) {
    gitref_kind adv_k = GITREF_NONE;
    u8cs adv_bare = {};
    if (GITParseRef(adv_name, &adv_k, adv_bare) != OK) return NO;
    if (adv_k != GITREF_BRANCH && adv_k != GITREF_TAG) return NO;

    gitref_kind want_k = GITREF_NONE;
    u8cs want_bare = {};
    if (GITParseRef(want_branch, &want_k, want_bare) != OK) return NO;
    if (want_k != adv_k) return NO;

    if (u8csLen(adv_bare) != u8csLen(want_bare)) return NO;
    if ($empty(want_bare)) return $empty(adv_bare);
    return memcmp(adv_bare[0], want_bare[0],
                  (size_t)u8csLen(want_bare)) == 0;
}

// --- WIREFetch ---------------------------------------------------------

//  Drain a peer's refs advertisement, looking for the entry that
//  matches `want_branch` (be-side; empty = trunk).  Sets *out_sha on
//  success and copies the matched be-side branch into `name_out`
//  (empty bytes for trunk).  If `want_branch` is empty, the wire entry
//  that follows the symref HEAD (matching by sha) wins — git's `clone`
//  default-branch discovery; falls back to the first non-HEAD entry.
static ok64 wcli_match_advert(int rfd, u8b buf, u8csc want_branch,
                              sha1 *out_sha, u8b name_out) {
    sane(rfd >= 0 && out_sha && u8bOK(name_out));
    u8cs adv = {u8bDataHead(buf), u8bDataHead(buf)};
    b8   picked = NO;
    sha1 head_sha = {};
    b8   have_head = NO;
    sha1 first_sha = {};
    u8cs first_name = {NULL, NULL};
    b8   first_seen = NO;

    //  Helper: translate the wire refname to its be-side form and
    //  capture it.  Trunk wire-name `main` collapses to empty bytes.
    //  Tags keep their `tags/` prefix so REFS keys (`?tags/v1.0`) don't
    //  collide with branch keys (`?v1.0` would shadow a branch named
    //  `v1.0`).  REMOTE / OTHER / HEAD leave name_out empty.
    #define WCLI_RECORD_NAME(name) do {                                      \
        u8bReset(name_out);                                                  \
        gitref_kind _k = GITREF_NONE;                                        \
        u8cs _be = {};                                                       \
        if (wcli_wire_to_be((u8csc){(name)[0], (name)[1]},                   \
                            &_k, _be) == OK) {                               \
            if (_k == GITREF_TAG) u8bFeed(name_out, GIT_TAGS_PFX);           \
            if (!u8csEmpty(_be)) u8bFeed(name_out, _be);                     \
        }                                                                    \
    } while (0)

    for (;;) {
        u8cs line = {};
        ok64 d = wcli_read_pkt(rfd, buf, adv, line);
        if (d == PKTFLUSH) break;
        if (d == PKTDELIM) continue;
        if (d != OK) return WIRECLFL;

        //  Trim trailing '\n'.
        if (u8csLen(line) > 0 && line[1][-1] == '\n') line[1]--;

        wire_evt ev = {};
        if (WIREClassify(line, WIRE_ADVERT, &ev) != OK) continue;
        if (ev.kind != WIRE_REF) continue;
        sha1 sha = ev.sha;
        u8cs name = {ev.name[0], ev.name[1]};
        //  Strip "^{}" peeled-tag suffix (we want the tag's own sha here).
        if (u8csLen(name) >= 3 &&
            name[1][-1] == '}' && name[1][-2] == '{' && name[1][-3] == '^')
            continue;

        //  Track HEAD separately — git advertises "HEAD" + capability
        //  list as the very first entry, and the matching branch ref
        //  follows with the same sha.
        if (u8csEq(name, GIT_HEAD_LIT)) {
            head_sha = sha;
            have_head = YES;
            continue;
        }

        //  Skip peer's own remote-tracking refs (`refs/remotes/*`) —
        //  those are git-ism leakage, not real branches of the repo.
        //  Only `refs/heads/*` and `refs/tags/*` are meaningful.
        if (u8csHasPrefix(name, GIT_REFS_REMOTES_PFX)) continue;
        if (!u8csHasPrefix(name, GIT_REFS_HEADS_PFX) &&
            !u8csHasPrefix(name, GIT_REFS_TAGS_PFX))
            continue;

        if (!first_seen) {
            first_sha = sha;
            u8csMv(first_name, name);
            first_seen = YES;
        }
        //  Caller wants trunk (empty) and didn't bind to a specific
        //  branch: take the entry whose sha matches the symref HEAD.
        //  Otherwise compare be-side branch names via the translator.
        if ($empty(want_branch)) {
            if (have_head && sha1Eq(&sha, &head_sha)) {
                *out_sha = sha;
                WCLI_RECORD_NAME(name);
                picked = YES;
            }
        } else if (wcli_refname_match(name, want_branch)) {
            *out_sha = sha;
            WCLI_RECORD_NAME(name);
            picked = YES;
        }
    }
    if (!picked) {
        //  Empty want_branch fallback chain (mirrors `git clone`'s
        //  default-branch discovery):
        //    1. HEAD symref already handled in the loop above.
        //    2. refs/heads/main — be's canonical trunk wire alias.
        //    3. first advertised ref (legacy fallback).
        if (u8csEmpty(want_branch) && first_seen) {
            //  Second pass: prefer the entry whose be-side name maps
            //  to empty (= trunk via wcli_wire_to_be — picks up
            //  refs/heads/main).
            u8cs scan = {u8bDataHead(buf), u8bDataHead(buf)};
            scan[1] = u8bIdleHead(buf);
            for (;;) {
                u8cs line = {};
                ok64 d = PKTu8sDrain(scan, line);
                if (d == PKTFLUSH || d == PKTDELIM) break;
                if (d != OK) break;
                if (u8csLen(line) > 0 && line[1][-1] == '\n') line[1]--;
                wire_evt ev = {};
                if (WIREClassify(line, WIRE_ADVERT, &ev) != OK) continue;
                if (ev.kind != WIRE_REF) continue;
                u8cs name = {ev.name[0], ev.name[1]};
                gitref_kind gk = GITREF_NONE;
                u8cs be_name = {};
                if (wcli_wire_to_be(name, &gk, be_name) != OK) continue;
                if (gk != GITREF_BRANCH) continue;
                if (!u8csEmpty(be_name)) continue;
                //  Found refs/heads/main (be-side empty).
                *out_sha = ev.sha;
                WCLI_RECORD_NAME(name);
                done;
            }
            //  Fall through: legacy first-ref behaviour.
            *out_sha = first_sha;
            a_dup(u8c, fn, first_name);
            WCLI_RECORD_NAME(fn);
            done;
        }
        return WIRECLNRF;
    }
    #undef WCLI_RECORD_NAME
    done;
}

//  Decode a REFS row's val (`?<40-hex>` or bare 40-hex) into a sha1.
//  Mirror of REFADV's refadv_decode_terminal — kept private here so the
//  haves walk doesn't pull REFADV's branch-dedup logic.
static b8 wcli_haves_decode_val(sha1 *out, u8csc val) {
    a_dup(u8c, hex, val);
    if (u8csLen(hex) == 41 && hex[0][0] == '?') u8csUsed(hex, 1);
    if (u8csLen(hex) != 40) return NO;
    a_dup(u8c, hex_dup, hex);
    u8s bin = {out->data, out->data + 20};
    if (HEXu8sDrainSome(bin, hex_dup) != OK) return NO;
    if (bin[0] != out->data + 20) return NO;
    if (!u8csEmpty(hex_dup)) return NO;
    return YES;
}

typedef struct {
    sha1 *out;
    u32   cap;
    u32   n;
} wcli_haves_ctx;

static ok64 wcli_haves_cb(refcp r, void *vctx) {
    sane(r && vctx);
    wcli_haves_ctx *c = (wcli_haves_ctx *)vctx;
    if (c->n >= c->cap) return REFSSTOP;
    sha1 sh = {};
    if (!wcli_haves_decode_val(&sh, r->val)) done;
    if (sha1empty(&sh)) done;
    for (u32 i = 0; i < c->n; i++) {
        if (sha1Eq(&c->out[i], &sh)) done;        // dedup
    }
    c->out[c->n++] = sh;
    done;
}

//  Harvest have-shas from every latest REFS row — local (`?<branch>`)
//  AND peer-observed (`<peer-uri>?<branch>`).  REFADV's per-branch
//  dedup is wrong for haves: the cached peer tip is exactly the
//  overlap we want to advertise to the same peer, but it gets
//  shadowed by the local cur row in REFADV.  Caps at WIRE_MAX_HAVES.
static u32 wcli_collect_haves(keeper *k, sha1 *out, u32 cap) {
    if (!k) return 0;
    a_path(keepdir);
    (void)HOMEBranchDir(k->h, keepdir, NULL);
    wcli_haves_ctx c = {.out = out, .cap = cap, .n = 0};
    (void)REFSEach($path(keepdir), wcli_haves_cb, &c);
    return c.n;
}

//  DIS-012 title-clash gate.  Collect only this shard's LOCAL project
//  tips — rows whose key begins with `?` (`?`, `?heads/main`, …), NOT
//  peer-observed `<host>?<branch>` rows.  A local tip is the project's
//  own referenced history; an incoming clone must share an ancestor
//  with at least one of them, else the two histories are disjoint.
static ok64 wcli_local_tips_cb(refcp r, void *vctx) {
    sane(r && vctx);
    wcli_haves_ctx *c = (wcli_haves_ctx *)vctx;
    if (c->n >= c->cap) return REFSSTOP;
    //  Local key ⇒ first byte is '?' (no scheme/authority prefix).
    if (u8csEmpty(r->key) || r->key[0][0] != '?') done;
    sha1 sh = {};
    if (!wcli_haves_decode_val(&sh, r->val)) done;
    if (sha1empty(&sh)) done;
    for (u32 i = 0; i < c->n; i++)
        if (sha1Eq(&c->out[i], &sh)) done;            // dedup
    c->out[c->n++] = sh;
    done;
}

static u32 wcli_collect_local_tips(keeper *k, sha1 *out, u32 cap) {
    if (!k) return 0;
    a_path(keepdir);
    (void)HOMEBranchDir(k->h, keepdir, NULL);
    wcli_haves_ctx c = {.out = out, .cap = cap, .n = 0};
    (void)REFSEach($path(keepdir), wcli_local_tips_cb, &c);
    return c.n;
}

//  Refuse a disjoint-history clone into an existing shard ([Title]
//  §"Same title, different history is an error").  Returns TITLECLSH
//  when the shard already holds at least one local project tip and the
//  freshly-ingested `incoming` tip shares NO common ancestor with ANY
//  of them; OK otherwise (fresh shard, or shared history converges).
//  Both `incoming` and the existing tips are present post-ingest, so
//  KEEPSharesAncestor can walk both closures.
static ok64 wcli_title_clash_check(keeper *k, sha1cp incoming) {
    sane(k && incoming);
    sha1 tips[WIRE_MAX_HAVES] = {};
    u32  ntips = wcli_collect_local_tips(k, tips, WIRE_MAX_HAVES);
    if (ntips == 0) return OK;                  //  fresh shard: no clash
    for (u32 i = 0; i < ntips; i++) {
        if (KEEPSharesAncestor(&tips[i], incoming)) return OK;
    }
    return TITLECLSH;
}

//  Send the upload-pack request: want <sha> caps + flush + haves +
//  flush + done.  No multi_ack — server replies with one NAK + pack.
static ok64 wcli_send_request(int wfd, sha1cp want_sha,
                              sha1cp haves, u32 nhaves) {
    sane(wfd >= 0 && want_sha);

    a_carve(u8, frame, (1u << 16));

    //  want line.
    {
        a_pad(u8, line, 256);
        a_cstr(want_pfx, "want ");
        u8bFeed(line, want_pfx);
        SHA1u8sFeedHex(u8bIdle(line), want_sha);
        //  Request side-band-64k so the server multiplexes its
        //  "Counting/Compressing/Receiving objects…" progress text
        //  onto band-2; KEEPIngestStream forwards band-2 to our
        //  stderr live and feeds band-1 directly into the keeper
        //  log.  We DROP `no-progress` for the same reason — keeping
        //  it would ask the server not to emit those messages.
        a_cstr(caps_s, " side-band-64k ofs-delta\n");
        u8bFeed(line, caps_s);
        a_dup(u8c, payload, u8bData(line));
        call(PKTu8sFeed, u8bIdle(frame), payload);
    }
    call(PKTu8sFeedFlush, u8bIdle(frame));

    //  have lines.
    for (u32 i = 0; i < nhaves; i++) {
        a_pad(u8, line, 64);
        a_cstr(have_pfx, "have ");
        u8bFeed(line, have_pfx);
        SHA1u8sFeedHex(u8bIdle(line), &haves[i]);
        u8bFeed1(line, '\n');
        a_dup(u8c, payload, u8bData(line));
        call(PKTu8sFeed, u8bIdle(frame), payload);
    }

    //  done.
    {
        a_cstr(done_s, "done\n");
        call(PKTu8sFeed, u8bIdle(frame), done_s);
    }

    a_dup(u8c, fdata, u8bData(frame));
    return FILEFeedAll(wfd, fdata);
}

//  Append `<peer-uri>?<be-branch> → <40-hex>` to local REFS.
//  `be_branch` is be-side (empty for trunk).  Peer's scheme / authority
//  / path land in the row so later lookups (`be get //peer`) can filter
//  by host.  No canonicalisation aliasing — the query is stored as the
//  literal be-branch path (DOGCanonURIFeed only folds shape, not name).
static ok64 wcli_record_ref(keeper *k, u8csc remote_uri, u8csc be_branch,
                             sha1cp new_sha) {
    sane(k);
    //  Wire-side refs land in the leaf branch dir when one is active
    //  (sub-shard fetch isolation: a submodule's `master` ref must
    //  not contaminate the parent's REFS reflog).  Trunk-only opens
    //  keep using `<root>/.be/refs` as before.  Readers compensate
    //  with a leaf→trunk fallback (REFSResolve at leaf first, then
    //  retry at trunk if not found).
    a_path(keepdir);
    call(HOMEBranchDir, k->h, keepdir, k->h->cur_branch);

    uri pu = {};
    pu.data[0] = remote_uri[0];
    pu.data[1] = remote_uri[1];
    (void)URILexer(&pu);

    //  Always present (even if empty) so DOGCanonURIFeed emits the `?`
    //  separator — `<peer-uri>?` is the trunk-on-peer key.
    if ($empty(be_branch)) {
        pu.query[0] = remote_uri[1];
        pu.query[1] = remote_uri[1];
    } else {
        u8csMv(pu.query, be_branch);
    }
    pu.fragment[0] = NULL;
    pu.fragment[1] = NULL;

    a_pad(u8, kbuf, 512);
    call(DOGCanonURIFeed, kbuf, &pu);
    a_dup(u8c, key, u8bData(kbuf));

    a_sha1hex(val, new_sha);

    //  REFSAppend itself dedups on (key, val) so a no-op `be get`
    //  repeat doesn't grow `.be/refs` (per keeper/LOG.md).
    return REFSAppend($path(keepdir), key, val);
}

// --- WIREFetchAll: bulk fetch every advertised heads/tags ref ----------
//
//  Single upload-pack session.  Drains the peer's advertisement, then
//  emits a multi-want request (one `want <sha>` line per advertised
//  branch/tag, capability list on the first only).  The peer streams
//  back one packfile carrying the union of all wants' reachable
//  closures; KEEPIngestStream lands every object in our log.  Each
//  ref is recorded locally via wcli_record_ref under the peer URI key
//  so subsequent `be head //origin?<ref>` reads can hit the cache.
//
//  Bound: WIRECLI_FETCHALL_MAX advertised refs per session.  Past that
//  the peer's tail entries are silently dropped — large mirrors should
//  loop a known-prefix probe instead.

#define WIRECLI_FETCHALL_MAX 64
#define WIRECLI_REFNAME_CAP  256

typedef struct {
    sha1 sha;
    u8cs name;   //  slice into the per-call names_arena
} wcli_advert_ref;

//  Worker: assumes wcli_spawn has populated *wfd / *rfd.  Closes
//  them at the protocol-required points (setting the int to -1 so
//  the wrapper knows not to re-close).  All scratch buffers acquired
//  from BASS — auto-rewound at the wrapper's `try()` boundary.
static ok64 wire_fetch_all_inner(u8csc remote_uri, int *wfd, int *rfd) {
    sane(wfd && rfd && *wfd >= 0 && *rfd >= 0);
    keeper *k = &KEEP;

    a_carve(u8, advbuf, WCLI_BUF);
    a_carve(u8, names_arena, WIRECLI_FETCHALL_MAX * WIRECLI_REFNAME_CAP);

    wcli_advert_ref refs[WIRECLI_FETCHALL_MAX];
    u32 nrefs = 0;

    //  1. Drain advertisement; collect heads/tags only.  Skip HEAD
    //     pseudo-ref, peeled-tag `^{}` lines, and `refs/remotes/*`.
    {
        u8cs adv = {u8bDataHead(advbuf), u8bDataHead(advbuf)};
        for (;;) {
            u8cs line = {};
            ok64 d = wcli_read_pkt(*rfd, advbuf, adv, line);
            if (d == PKTFLUSH) break;
            if (d == PKTDELIM) continue;
            if (d != OK) fail(d);

            if (u8csLen(line) > 0 && line[1][-1] == '\n') line[1]--;

            wire_evt ev = {};
            if (WIREClassify(line, WIRE_ADVERT, &ev) != OK) continue;
            if (ev.kind != WIRE_REF) continue;
            sha1 sha = ev.sha;
            u8cs name = {ev.name[0], ev.name[1]};

            if (u8csEq(name, GIT_HEAD_LIT)) continue;
            if (u8csLen(name) >= 3 && name[1][-1] == '}' &&
                name[1][-2] == '{' && name[1][-3] == '^') continue;
            if (u8csHasPrefix(name, GIT_REFS_REMOTES_PFX)) continue;
            if (!u8csHasPrefix(name, GIT_REFS_HEADS_PFX) &&
                !u8csHasPrefix(name, GIT_REFS_TAGS_PFX))
                continue;

            if (nrefs >= WIRECLI_FETCHALL_MAX) {
                fprintf(stderr,
                    "be: WIREFetchAll: peer advertises >%u refs;"
                    " trailing refs skipped\n",
                    (u32)WIRECLI_FETCHALL_MAX);
                break;
            }
            refs[nrefs].sha = sha;
            //  Snapshot the name into the arena's PAST (one-shot rental,
            //  NUL-terminated — ref names are path-shaped).
            call(PATHu8bAren, names_arena, refs[nrefs].name, name);
            nrefs++;
        }
    }

    if (nrefs == 0) {
        //  Peer advertised no heads/tags — clean disconnect, no pack.
        done;
    }

    //  2. Harvest haves locally so the peer can prune the pack.
    sha1 haves[WIRE_MAX_HAVES] = {};
    u32  nhaves = wcli_collect_haves(k, haves, WIRE_MAX_HAVES);

    //  3. Emit multi-want request.  Caps go on the first want; remaining
    //     wants carry only the sha + newline.
    a_carve(u8, frame, 1u << 16);
    for (u32 i = 0; i < nrefs; i++) {
        a_pad(u8, line, 256);
        a_cstr(want_pfx, "want ");
        u8bFeed(line, want_pfx);
        SHA1u8sFeedHex(u8bIdle(line), &refs[i].sha);
        if (i == 0) {
            a_cstr(caps_s, " side-band-64k ofs-delta\n");
            u8bFeed(line, caps_s);
        } else {
            u8bFeed1(line, '\n');
        }
        a_dup(u8c, payload, u8bData(line));
        call(PKTu8sFeed, u8bIdle(frame), payload);
    }
    call(PKTu8sFeedFlush, u8bIdle(frame));
    for (u32 i = 0; i < nhaves; i++) {
        a_pad(u8, line, 64);
        a_cstr(have_pfx, "have ");
        u8bFeed(line, have_pfx);
        SHA1u8sFeedHex(u8bIdle(line), &haves[i]);
        u8bFeed1(line, '\n');
        a_dup(u8c, payload, u8bData(line));
        call(PKTu8sFeed, u8bIdle(frame), payload);
    }
    {
        a_cstr(done_s, "done\n");
        call(PKTu8sFeed, u8bIdle(frame), done_s);
    }
    {
        a_dup(u8c, fdata, u8bData(frame));
        call(FILEFeedAll, *wfd, fdata);
    }
    close(*wfd); *wfd = -1;

    //  4. Stream-ingest the response packfile.
    call(KEEPIngestStream, *rfd);
    close(*rfd); *rfd = -1;

    //  5. Record each ref locally.  Skip refs whose wire_to_be doesn't
    //     classify (e.g. malformed names) — the pack landed regardless,
    //     but no ref row means no cached lookup for that name.
    u32 recorded = 0;
    for (u32 i = 0; i < nrefs; i++) {
        u8csc wire_name = {refs[i].name[0], refs[i].name[1]};
        gitref_kind kk = GITREF_NONE;
        u8cs be_bare = {};
        if (wcli_wire_to_be(wire_name, &kk, be_bare) != OK) continue;
        if (kk != GITREF_BRANCH && kk != GITREF_TAG) continue;

        a_pad(u8, name_buf, WIRECLI_REFNAME_CAP + 8);
        if (kk == GITREF_TAG) u8bFeed(name_buf, GIT_TAGS_PFX);
        if (!u8csEmpty(be_bare)) u8bFeed(name_buf, be_bare);
        a_dup(u8c, be_name, u8bDataC(name_buf));

        ok64 rr = wcli_record_ref(k, remote_uri, be_name, &refs[i].sha);
        if (rr != OK) {
            fprintf(stderr,
                "be: wcli_record_ref " U8SFMT " failed: %s\n",
                u8sFmt(refs[i].name), ok64str(rr));
            continue;
        }
        recorded++;
    }

    fprintf(stderr, "keeper: fetched %u ref(s)\n", recorded);
    done;
}

ok64 WIREFetchAll(u8csc remote_uri) {
    sane($ok(remote_uri));
    FILEIgnoreSIGPIPE();  //  peer dying mid-transfer must not kill us
    if (u8csEmpty(remote_uri)) return WIRECLFL;

    int wfd = -1, rfd = -1;
    pid_t pid = 0;
    ok64 so = wcli_spawn(remote_uri, "upload-pack", &wfd, &rfd, &pid);
    if (so != OK) return WIRECLFL;

    try(wire_fetch_all_inner, remote_uri, &wfd, &rfd);
    ok64 rv = __;

    if (wfd >= 0) close(wfd);
    if (rfd >= 0) close(rfd);
    if (pid > 0) {
        int rc = 0;
        FILEReap(pid, &rc);
    }
    return rv == OK ? OK : WIRECLFL;
}

//  Worker for WIREFetch: drain advertisement, send request, ingest
//  pack, record ref.  Buffers come from BASS.  Closes wfd/rfd at the
//  protocol-required points.
static ok64 wire_fetch_inner(u8csc remote_uri, u8cs effective_ref,
                              keeper *k, int *wfd, int *rfd) {
    sane(k && wfd && rfd && *wfd >= 0 && *rfd >= 0);

    a_carve(u8, advbuf, WCLI_BUF);

    //  Want-by-hash: a bare 40-hex `want_ref` (e.g. a submodule gitlink
    //  pin) names the exact object to pull, not a branch.  The server's
    //  wire_locate_sha serves any present object, so this works even
    //  when the source shard advertises no ref whose closure covers the
    //  pin (the zero-/wrong-refs case WIREFetchAll cannot satisfy).
    sha1 pin_sha = {};
    b8   want_pin = wcli_haves_decode_val(&pin_sha, effective_ref);

    //  1.  Drain refs advertisement; pick the want sha + capture the
    //      matched ref name (used for the local REFS write below).
    //      For a by-hash want we still drain the advertisement (wire
    //      order) but ignore its picks: WIRECLNRF (no branch matched a
    //      40-hex name) is expected, only a genuine wire failure aborts.
    sha1 want_sha = {};
    a_pad(u8, matched_ref_buf, 256);
    try(wcli_match_advert, *rfd, advbuf, effective_ref, &want_sha,
                           matched_ref_buf);
    ok64 mo = __;
    if (mo != OK && !(want_pin && mo == WIRECLNRF)) fail(mo);

    u8cs matched_ref = {};
    if (want_pin) {
        //  Override with the pin; leave matched_ref empty so
        //  wcli_record_ref writes the peer-trunk key (`<uri>?` → pin) —
        //  the by-hash clone counts as the shard's trunk ref.
        want_sha = pin_sha;
    } else {
        u8csMv(matched_ref, u8bDataC(matched_ref_buf));
        if (u8csEmpty(matched_ref)) u8csMv(matched_ref, effective_ref);
    }

    //  2.  Harvest haves from local REFS (every tracked tip — local
    //      cur AND cached peer-observed rows).
    sha1 haves[WIRE_MAX_HAVES] = {};
    u32  nhaves = wcli_collect_haves(k, haves, WIRE_MAX_HAVES);

    //  3.  Send want + haves + done.
    call(wcli_send_request, *wfd, &want_sha, haves, nhaves);
    close(*wfd); *wfd = -1;

    //  4.  Stream-ingest the upload-pack response straight into the
    //  keeper tail log.  KEEPIngestStream parses pkt-line headers
    //  inline, dispatches side-band frames in real time (band-2
    //  progress to stderr, band-1 bytes to log via u8bFeed), and
    //  drops the trailing 20-byte SHA-1 + the embedded git PACK
    //  header.  No intermediate response/pack buffer.
    {
        ok64 io = KEEPIngestStream(*rfd);
        if (io != OK) {
            fprintf(stderr, "be: KEEPIngestStream failed: %s\n",
                    ok64str(io));
            fail(io);
        }
    }
    close(*rfd); *rfd = -1;

    //  5.  DIS-012 title-clash gate ([Title] §"Same title, different
    //  history is an error").  A by-hash pin fetch is exempt: it is a
    //  submodule gitlink pull whose history legitimately differs from
    //  the parent shard, and a fresh sub-shard has no local tips
    //  anyway.  For a branch fetch, refuse before recording the ref if
    //  the incoming tip shares no ancestor with any existing local
    //  tip — the orphaned objects stay unreferenced (never co-mingled
    //  into the project's reachable history) and the trunk is untouched.
    if (!want_pin) {
        ok64 co = wcli_title_clash_check(k, &want_sha);
        if (co != OK) {
            fprintf(stderr,
                    "be: title clash — the incoming history shares no "
                    "common ancestor with this project's shard.  "
                    "Override the title to give it a distinct shard: "
                    "`be get <uri>?/<title>`.\n");
            fail(co);
        }
    }

    //  6.  Record the ref locally under the actually-matched name,
    //  attributed to the peer URI.
    {
        ok64 rr = wcli_record_ref(k, remote_uri, matched_ref, &want_sha);
        if (rr != OK) {
            fprintf(stderr,
                    "be: wcli_record_ref failed: %s\n", ok64str(rr));
            fail(rr);
        }
    }

    done;
}

ok64 WIREFetch(u8csc remote_uri, u8csc want_ref) {
    sane($ok(remote_uri));
    FILEIgnoreSIGPIPE();  //  peer dying mid-transfer must not kill us
    keeper *k = &KEEP;
    if (u8csEmpty(remote_uri)) return WIRECLFL;

    //  Empty want_ref → let wcli_match_advert pick the peer's HEAD or
    //  first advertised ref (mirrors `git clone`'s default-branch
    //  discovery).  Callers that want an explicit fallback should pass
    //  e.g. "heads/main" themselves.
    u8cs effective_ref = {want_ref[0], want_ref[1]};

    int wfd = -1, rfd = -1;
    pid_t pid = 0;
    ok64 so = wcli_spawn(remote_uri, "upload-pack", &wfd, &rfd, &pid);
    if (so != OK) return WIRECLFL;

    try(wire_fetch_inner, remote_uri, effective_ref, k, &wfd, &rfd);
    ok64 rv = __;

    if (wfd >= 0) close(wfd);
    if (rfd >= 0) close(rfd);
    if (pid > 0) {
        int rc = 0;
        FILEReap(pid, &rc);
    }
    //  Preserve TITLECLSH so callers (and tests) see the clash refusal
    //  distinctly; every other failure collapses to WIRECLFL.
    if (rv == OK) return OK;
    if (rv == TITLECLSH) return TITLECLSH;
    return WIRECLFL;
}

// --- WIREPush ----------------------------------------------------------
//
//  MVP: build a packfile carrying the full reachable closure of our
//  local tip (commit + tree + blobs).  No DAG diff against the peer's
//  advertised tip yet — over-ship is the failure mode (the server
//  ingests the whole pack anyway and refs are FF-checked separately).

#define WPUSH_MAX_OBJS 65536

//  Recursively collect tree + blob SHAs reachable from `tree_sha` into
//  `out` (capacity `cap`).  Mirrors keep_walk_tree (KEEP.c) but lives
//  here so WIRECLI doesn't depend on KEEP.c's static helpers.
// --- have-set (sorted sha array; binary-search membership) ---

typedef struct {
    sha1 *items;
    u32   n;
    u32   cap;
} sha_set;

static b8 sha_set_has(sha_set const *s, sha1cp q) {
    if (!s || s->n == 0) return NO;
    u32 lo = 0, hi = s->n;
    while (lo < hi) {
        u32 mid = (lo + hi) >> 1;
        if (sha1Z(&s->items[mid], q)) lo = mid + 1;
        else if (sha1Z(q, &s->items[mid])) hi = mid;
        else return YES;
    }
    return NO;
}

static void sha_set_add(sha_set *s, sha1cp v) {
    if (!s || s->n >= s->cap) return;
    //  Insertion sort: find the position and shift.
    u32 i = s->n;
    while (i > 0 && sha1Z(v, &s->items[i - 1])) {
        s->items[i] = s->items[i - 1];
        i--;
    }
    if (i > 0 && sha1Eq(&s->items[i - 1], v)) return;  //  dup
    s->items[i] = *v;
    s->n++;
}

//  Walk a tree's closure into `out` (skip-list-aware: a sha already
//  present in `have` is not added and its sub-objects are not
//  enumerated).  When `add_to_have` is non-NULL, also record visited
//  shas into that set — used by the "collect have-set from peer_tip"
//  pass before the local walk.
static ok64 wpush_walk_tree(keeper *k, sha1cp tree_sha,
                            sha1 *out, u32 *n, u32 cap,
                            sha_set const *have, sha_set *add_to_have) {
    sane(k && tree_sha && n);
    if (have && sha_set_has(have, tree_sha)) done;
    //  Dedup against `add_to_have` too — without this check, a merge
    //  history (a commit reachable through two parent paths) re-walks
    //  the same trees / blobs through every alternative path,
    //  exploding O(N) into O(2^depth).
    if (add_to_have && sha_set_has(add_to_have, tree_sha)) done;
    if (out) {
        if (*n >= cap) return WIRECLFL;
        out[(*n)++] = *tree_sha;
        sha1hex _h = {}; sha1hexFromSha1(&_h, tree_sha);
        trace("wpush_dump: tree %.40s\n", _h.data);
    }
    if (add_to_have) sha_set_add(add_to_have, tree_sha);

    Bu8 tbuf = {};
    ok64 mo = u8bMap(tbuf, 1UL << 20);
    if (mo != OK) return mo;
    u8 ttype = 0;
    if (KEEPGetExact(tree_sha, tbuf, &ttype) != OK ||
        ttype != KEEP_OBJ_TREE) {
        u8bUnMap(tbuf);
        done;
    }
    u8cs walk = {u8bDataHead(tbuf), u8bIdleHead(tbuf)};
    u8cs file = {}, sha = {};
    while (GITu8sDrainTree(walk, file, sha, NULL) == OK) {
        if ($len(sha) != 20) continue;
        b8 is_tree = NO;
        b8 is_submodule = NO;
        if ($len(file) >= 5 && file[0][0] == '4' && file[0][1] == '0')
            is_tree = YES;
        if ($len(file) >= 6 && file[0][0] == '1' && file[0][1] == '6' &&
            file[0][2] == '0')
            is_submodule = YES;
        if (is_submodule) continue;
        sha1 entry_sha = {};
        (void)sha1Drain(sha, &entry_sha);
        if (have && sha_set_has(have, &entry_sha)) continue;
        if (add_to_have && sha_set_has(add_to_have, &entry_sha)) continue;
        if (is_tree) {
            wpush_walk_tree(k, &entry_sha, out, n, cap, have, add_to_have);
        } else {
            if (out) {
                if (*n >= cap) break;
                out[(*n)++] = entry_sha;
                sha1hex _h = {}; sha1hexFromSha1(&_h, &entry_sha);
                trace("wpush_dump: blob %.40s\n", _h.data);
            }
            if (add_to_have) sha_set_add(add_to_have, &entry_sha);
        }
    }
    u8bUnMap(tbuf);
    done;
}

//  Collect commit + tree + blob SHAs reachable from `commit_sha`,
//  walking the parent chain too (so multi-commit FF pushes carry the
//  intermediate commits).  When `have` is non-NULL, any sha (commit,
//  tree, blob, or parent commit) already in the set is treated as
//  closed: it is not added to `out` and its sub-graph is not
//  enumerated — the assumption is the peer already has it.
//
//  When `add_to_have` is non-NULL (and `out` is NULL), the function
//  populates the haveset instead — used for the peer-tip closure pass
//  before the local walk.
static ok64 wpush_walk_commit(keeper *k, sha1cp commit_sha,
                              sha1 *out, u32 *n, u32 cap,
                              sha_set const *have, sha_set *add_to_have) {
    sane(k && commit_sha && n);
    if (have && sha_set_has(have, commit_sha)) done;
    //  See wpush_walk_tree: without this check, a merge-history
    //  closure re-walks ancestors through every alternate path.
    if (add_to_have && sha_set_has(add_to_have, commit_sha)) done;
    if (out) {
        if (*n >= cap) return WIRECLFL;
        out[(*n)++] = *commit_sha;
        sha1hex _h = {}; sha1hexFromSha1(&_h, commit_sha);
        trace("wpush_dump: commit %.40s\n", _h.data);
    }
    if (add_to_have) sha_set_add(add_to_have, commit_sha);

    Bu8 cbuf = {};
    ok64 mo = u8bMap(cbuf, 1UL << 20);
    if (mo != OK) return mo;
    u8 ctype = 0;
    ok64 go = KEEPGetExact(commit_sha, cbuf, &ctype);
    if (go != OK || ctype != KEEP_OBJ_COMMIT) {
        u8bUnMap(cbuf);
        //  Haveset-build mode (`add_to_have` set, `out` not):
        //  tolerate missing commits — we collect what we have, the
        //  rest just doesn't prune the local-side closure.
        //  Pack-build mode (out non-NULL): hard fail; the caller
        //  needs the body to feed the pack.
        if (out == NULL) done;
        return WIRECLFL;
    }
    u8cs commit_body = {u8bDataHead(cbuf), u8bIdleHead(cbuf)};
    sha1 tree_sha = {};
    if (GITu8sCommitTree(commit_body, tree_sha.data) != OK) {
        u8bUnMap(cbuf);
        return WIRECLFL;
    }

    //  Walk parents.  Each `parent <40-hex>` header line names another
    //  commit that must also be in the pack unless the peer has it.
    {
        u8cs body = {u8bDataHead(cbuf), u8bIdleHead(cbuf)};
        u8cs field = {}, value = {};
        while (GITu8sDrainCommit(body, field, value) == OK) {
            if ($empty(field)) break;
            if ($len(field) == 6 && memcmp(field[0], "parent", 6) == 0 &&
                $len(value) >= 40) {
                sha1 par = {};
                u8s bin = {par.data, par.data + 20};
                u8cs hx = {value[0], value[0] + 40};
                a_dup(u8c, hx_dup, hx);
                if (HEXu8sDrainSome(bin, hx_dup) != OK) continue;
                if (bin[0] != par.data + 20) continue;
                if (have && sha_set_has(have, &par)) continue;
                if (add_to_have && sha_set_has(add_to_have, &par)) continue;
                wpush_walk_commit(k, &par, out, n, cap, have, add_to_have);
            }
        }
    }
    u8bUnMap(cbuf);

    return wpush_walk_tree(k, &tree_sha, out, n, cap, have, add_to_have);
}

//  Collect commit shas reachable from `tip` but absent from `have`
//  (= the commits this push introduces to the peer).  Pre-order parent
//  walk → newest-first; `seen` dedups merge fan-in.  Commit-only —
//  never descends into trees/blobs.  Best-effort: a missing or
//  non-commit object just stops that subtree.
static ok64 wpush_collect_commits(keeper *k, sha1cp tip,
                                  sha_set const *have, sha_set *seen,
                                  sha1 *out, u32 *n, u32 cap) {
    sane(k && tip && n && out);
    if (have && sha_set_has(have, tip)) done;
    if (seen && sha_set_has(seen, tip)) done;

    Bu8 cbuf = {};
    if (u8bMap(cbuf, 1UL << 20) != OK) done;
    u8 ct = 0;
    if (KEEPGetExact(tip, cbuf, &ct) != OK || ct != KEEP_OBJ_COMMIT) {
        u8bUnMap(cbuf);
        done;
    }
    if (seen) sha_set_add(seen, tip);
    if (*n < cap) out[(*n)++] = *tip;

    u8cs body = {u8bDataHead(cbuf), u8bIdleHead(cbuf)};
    u8cs field = {}, value = {};
    while (GITu8sDrainCommit(body, field, value) == OK) {
        if ($empty(field)) break;
        if ($len(field) == 6 && memcmp(field[0], "parent", 6) == 0 &&
            $len(value) >= 40) {
            sha1 par = {};
            u8s  bin = {par.data, par.data + 20};
            u8cs hx  = {value[0], value[0] + 40};
            a_dup(u8c, hx_dup, hx);
            if (HEXu8sDrainSome(bin, hx_dup) != OK) continue;
            if (bin[0] != par.data + 20) continue;
            wpush_collect_commits(k, &par, have, seen, out, n, cap);
        }
    }
    u8bUnMap(cbuf);
    done;
}

//  Append a pack object header (type + size varint, big-endian-ish) to
//  `buf`.  Mirrors keep_feed_obj_hdr in KEEP.c.
static void wpush_feed_obj_hdr(u8b buf, u8 type, u64 size) {
    u8 first = (u8)((type << 4) | (size & 0x0f));
    size >>= 4;
    if (size > 0) first |= 0x80;
    u8bFeed1(buf, first);
    while (size > 0) {
        u8 c = (u8)(size & 0x7f);
        size >>= 7;
        if (size > 0) c |= 0x80;
        u8bFeed1(buf, c);
    }
}

//  Build a v2 packfile containing the listed objects, in order, into
//  `pack_out` (caller pre-mapped).  Each object is fetched via
//  KEEPGetExact and zlib-deflated inline.  Adds the 12-byte PACK
//  header up front and the 20-byte SHA-1 trailer at the end.
static ok64 wpush_build_pack(keeper *k, sha1cp shas, u32 nshas,
                             u8b pack_out) {
    sane(k && shas && u8bOK(pack_out));

    //  PACK header.
    u8 hdr[12] = {'P','A','C','K', 0,0,0,2, 0,0,0,0};
    hdr[8]  = (u8)((nshas >> 24) & 0xff);
    hdr[9]  = (u8)((nshas >> 16) & 0xff);
    hdr[10] = (u8)((nshas >>  8) & 0xff);
    hdr[11] = (u8) (nshas        & 0xff);
    u8csc hdr_s = {hdr, hdr + 12};
    u8bFeed(pack_out, hdr_s);

    for (u32 i = 0; i < nshas; i++) {
        Bu8 obuf = {};
        ok64 mo = u8bMap(obuf, 1UL << 24);
        if (mo != OK) {
            trace(
                    "wpush: build_pack obj#%u: u8bMap rc=%llx\n",
                    i, (unsigned long long)mo);
            return mo;
        }
        u8 otype = 0;
        ok64 go = KEEPGetExact(&shas[i], obuf, &otype);
        if (go != OK) {
            sha1hex h = {}; sha1hexFromSha1(&h, &shas[i]);
            trace(
                    "wpush: build_pack obj#%u sha=%.40s: "
                    "KEEPGetExact rc=%llx\n",
                    i, h.data, (unsigned long long)go);
            u8bUnMap(obuf);
            return WIRECLFL;
        }
        u64 olen = u8bDataLen(obuf);

        a_pad(u8, ohdr, 16);
        wpush_feed_obj_hdr(ohdr, otype, olen);
        a_dup(u8c, oh, u8bData(ohdr));
        ok64 fho = u8bFeed(pack_out, oh);
        if (fho != OK) {
            trace(
                    "wpush: build_pack obj#%u type=%u: hdr feed rc=%llx "
                    "(pack_out idle=%zu need=%zu)\n",
                    i, (unsigned)otype, (unsigned long long)fho,
                    u8bIdleLen(pack_out), (size_t)u8csLen(oh));
            u8bUnMap(obuf);
            return fho;
        }

        a_dup(u8c, osrc, u8bData(obuf));
        ok64 zo = ZINFDeflate(u8bIdle(pack_out), osrc);
        if (zo != OK) {
            trace(
                    "wpush: build_pack obj#%u type=%u olen=%llu: "
                    "ZINFDeflate rc=%llx (pack_out idle=%zu)\n",
                    i, (unsigned)otype, (unsigned long long)olen,
                    (unsigned long long)zo, u8bIdleLen(pack_out));
            u8bUnMap(obuf);
            return zo;
        }
        u8bUnMap(obuf);
    }

    //  20-byte SHA-1 trailer over the whole pack so far.
    sha1 psha = {};
    a_dup(u8c, pack_data, u8bData(pack_out));
    SHA1Sum(&psha, pack_data);
    u8csc psha_s = {psha.data, psha.data + 20};
    u8bFeed(pack_out, psha_s);
    done;
}

//  Drain peer's refs advertisement.  Two outputs:
//    * if `branch_refname` matches an advertised entry, capture its
//      sha into `*out_sha` and set `*out_have=YES`;
//    * if `peer_tips_out` is non-NULL, push EVERY advertised tip sha
//      into it — used downstream as roots for the "objects the peer
//      already has" walk, so the local pack-build can prune anything
//      reachable from any peer ref (not just the one matching ours).
static ok64 wpush_peer_tip(int rfd, u8b advbuf, u8csc branch_refname,
                           sha1 *out_sha, b8 *out_have,
                           sha1 *peer_tips_out, u32 *peer_tips_n,
                           u32 peer_tips_cap) {
    sane(rfd >= 0 && out_sha && out_have);
    *out_have = NO;
    if (peer_tips_n) *peer_tips_n = 0;
    u8cs adv = {u8bDataHead(advbuf), u8bDataHead(advbuf)};
    for (;;) {
        u8cs line = {};
        ok64 d = wcli_read_pkt(rfd, advbuf, adv, line);
        if (d == PKTFLUSH) break;
        if (d == PKTDELIM) continue;
        if (d != OK) return WIRECLFL;
        if (u8csLen(line) > 0 && line[1][-1] == '\n') line[1]--;
        wire_evt ev = {};
        if (WIREClassify(line, WIRE_ADVERT, &ev) != OK) continue;
        if (ev.kind != WIRE_REF) continue;
        sha1 sha = ev.sha;
        u8cs name = {ev.name[0], ev.name[1]};
        if (u8csLen(name) == u8csLen(branch_refname) &&
            memcmp(name[0], branch_refname[0],
                   (size_t)u8csLen(branch_refname)) == 0) {
            *out_sha  = sha;
            *out_have = YES;
        }
        if (peer_tips_out && peer_tips_n &&
            *peer_tips_n < peer_tips_cap) {
            peer_tips_out[*peer_tips_n] = sha;
            (*peer_tips_n)++;
        }
    }
    done;
}

//  Send "<old> <new> <refname>\0report-status side-band-64k\n" + flush.
static ok64 wpush_send_update(int wfd, sha1cp old_sha,
                              sha1cp new_sha, u8csc refname,
                              b8 have_old) {
    sane(wfd >= 0 && new_sha);
    a_pad(u8, frame, 1024);

    a_pad(u8, line, 512);
    sha1hex oh = {}, nh = {};
    if (have_old) {
        sha1hexFromSha1(&oh, old_sha);
    } else {
        memset(oh.data, '0', 40);
    }
    sha1hexFromSha1(&nh, new_sha);
    a_rawc(oh_s, oh);
    a_rawc(nh_s, nh);
    u8bFeed(line, oh_s);
    u8bFeed1(line, ' ');
    u8bFeed(line, nh_s);
    u8bFeed1(line, ' ');
    u8bFeed(line, refname);
    u8bFeed1(line, 0);
    //  Request side-band-64k so a refusing peer (pre-receive hook,
    //  denyCurrentBranch, dirty `updateInstead` checkout) multiplexes
    //  its `remote: …` diagnostic text onto band-2 — wpush_drain_status
    //  demuxes it to our stderr.  `report-status` still carries the
    //  in-band `unpack`/`ng <ref> <reason>` report (on band-1 when the
    //  peer honours side-band, bare otherwise — keeper's RECVEmitResponse
    //  never multiplexes, so the drain handles both shapes).  Without
    //  this the peer's reason was lost and a push failure collapsed to
    //  an opaque WIRECLFL (DIS-027).
    a_cstr(caps, "report-status side-band-64k");
    u8bFeed(line, caps);
    u8bFeed1(line, '\n');
    a_dup(u8c, payload, u8bData(line));
    call(PKTu8sFeed, u8bIdle(frame), payload);
    call(PKTu8sFeedFlush, u8bIdle(frame));

    a_dup(u8c, fdata, u8bData(frame));
    return FILEFeedAll(wfd, fdata);
}

//  Drain push response, scanning for "unpack ok" + "ok <refname>".
//  Send the pack body to the peer while concurrently draining any
//  status/progress it emits, so a peer that interleaves output with
//  reading the pack (git-receive-pack with sideband, or any large
//  push) can never deadlock us — the classic both-sides-blocked-on-a-
//  full-pipe hang.  `wfd` is switched to non-blocking; early status
//  bytes read during the send accumulate in `early` for the following
//  wpush_drain_status to consume before reading more.  Caller closes
//  `wfd` after this returns OK.
static ok64 wpush_send_pack_interleaved(int wfd, int rfd, u8csc pdata,
                                        u8b early) {
    sane(wfd >= 0 && rfd >= 0);
    int fl = fcntl(wfd, F_GETFL, 0);
    if (fl >= 0) (void)fcntl(wfd, F_SETFL, fl | O_NONBLOCK);

    a_dup(u8c, rest, pdata);   //  unsent tail; head advances as we write
    b8 rfd_live = YES;         //  cleared once the peer closes its report side
    while (!u8csEmpty(rest)) {
        struct pollfd pfd[2];
        pfd[0].fd = wfd;                 pfd[0].events = POLLOUT; pfd[0].revents = 0;
        pfd[1].fd = rfd_live ? rfd : -1; pfd[1].events = POLLIN;  pfd[1].revents = 0;
        int pr = poll(pfd, 2, -1);
        if (pr < 0) { if (errno == EINTR) continue; return FILEFAIL; }

        //  Drain whatever the peer has sent first, so it never blocks
        //  on its own write side while we are trying to write the pack.
        if (rfd_live && (pfd[1].revents & (POLLIN | POLLHUP))) {
            u8s idle;
            u8sFork(u8bIdle(early), idle);
            if ($len(idle) == 0) return WIRECLFL;  //  pre-pack status overflow
            ssize_t rn;
            do { rn = read(rfd, idle[0], (size_t)$len(idle)); }
            while (rn < 0 && errno == EINTR);
            if (rn > 0) { idle[0] += rn; u8sJoin(u8bIdle(early), idle); }
            else if (rn == 0) rfd_live = NO;       //  peer closed report side
        }

        if (pfd[0].revents & POLLOUT) {
            ssize_t wn;
            do { wn = write(wfd, rest[0], (size_t)$len(rest)); }
            while (wn < 0 && errno == EINTR);
            if (wn > 0) rest[0] += wn;
            else if (wn < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { }
            else return FILEFAIL;                  //  EPIPE etc.
        } else if (pfd[0].revents & (POLLERR | POLLHUP)) {
            return FILEFAIL;
        }
    }
    return OK;
}

//  Classify one report-status line (`unpack …`/`ok …`/`ng …`), update
//  the *unpack_ok / *ref_ok flags, and — critically for DIS-027 — print
//  the peer's own refusal reason to stderr so a failed push is
//  diagnosable.  `ng_match` is the `ng <refname>` prefix for our ref.
static void wpush_classify_report(u8csc raw, u8csc ng_match,
                                  b8 *unpack_ok, b8 *ref_ok) {
    //  Strip a trailing newline so our prints stay tidy.
    a_dup(u8c, ln, raw);
    if (!u8csEmpty(ln) && *u8csLast(ln) == '\n') ln[1]--;

    if (u8csEq(ln, GIT_PKT_UNPACK_OK)) {
        *unpack_ok = YES;
    } else if (u8csHasPrefix(ln, GIT_PKT_UNPACK_PFX)) {
        //  "unpack <reason>" — remote refused the pack itself.
        size_t skip = (size_t)u8csLen(GIT_PKT_UNPACK_PFX);
        fprintf(stderr, "remote: unpack %.*s\n",
                (int)(u8csLen(ln) - (ssize_t)skip),
                (char const *)ln[0] + skip);
    } else if (u8csHasPrefix(ln, GIT_PKT_OK_PFX)) {
        //  "ok <ref>" — accept regardless of which ref (the only ref
        //  update we sent), so a peer that reports a normalized refname
        //  still satisfies the ref_ok gate.
        *ref_ok = YES;
    } else if (u8csHasPrefix(ln, ng_match) ||
               u8csHasPrefix(ln, GIT_PKT_NG_PFX)) {
        //  "ng <ref> <reason>" — remote refused the ref update.  Body
        //  after "ng " is "<ref> <reason>"; trim "<ref> " for the
        //  user-facing message and surface it (DIS-027).
        size_t skip = (size_t)u8csLen(GIT_PKT_NG_PFX);
        //  skip the refname token up to the first space.
        while ((ssize_t)skip < u8csLen(ln) && ln[0][skip] != ' ') skip++;
        if ((ssize_t)skip < u8csLen(ln) && ln[0][skip] == ' ') skip++;
        fprintf(stderr, "remote: ref update rejected: %.*s\n",
                (int)(u8csLen(ln) - (ssize_t)skip),
                (char const *)ln[0] + skip);
        *ref_ok = NO;
    }
}

//  `prefill` carries any status bytes already read off `rfd` during the
//  interleaved pack send; they are consumed before reading more.  Pass
//  an empty slice when no early read happened.
//
//  Side-band demux (DIS-027): the push update advertises side-band-64k,
//  so a git-receive-pack peer multiplexes the response — band-1 carries
//  the report-status pkt-lines, band-2 the human-readable `remote: …`
//  diagnostics (hook stderr, denyCurrentBranch, dirty checkout), band-3
//  a fatal message.  We forward band-2/3 to our stderr live and re-parse
//  band-1 as the report stream.  A peer that ignores the cap (keeper's
//  own RECVEmitResponse never multiplexes) sends bare report pkt-lines —
//  their first byte is a printable letter, never a 0x01..0x03 band tag,
//  so the same loop handles both shapes.
static ok64 wpush_drain_status(int rfd, u8csc refname, u8csc prefill) {
    sane(rfd >= 0);
    a_carve(u8, buf, WCLI_BUF);
    if (!u8csEmpty(prefill)) call(u8bFeed, buf, prefill);
    u8cs adv = {u8bDataHead(buf), u8bIdleHead(buf)};
    b8 unpack_ok = NO;
    b8 ref_ok    = NO;

    a_pad(u8, ng_line, 512);
    u8bFeed(ng_line, GIT_PKT_NG_PFX);
    u8bFeed(ng_line, refname);
    a_dup(u8c, ng_match, u8bDataC(ng_line));

    //  band-1 (report pkt-lines) re-assembly buffer: a report line may
    //  arrive split across several side-band frames.
    a_carve(u8, b1, WCLI_BUF);

    for (;;) {
        u8cs line = {};
        ok64 d = wcli_read_pkt(rfd, buf, adv, line);
        if (d == PKTFLUSH) break;
        if (d == PKTDELIM) continue;
        if (d != OK) {
            trace("wpush: drain read returned %llx\n",
                    (unsigned long long)d);
            break;
        }

        //  Side-band frame?  First byte 0x01..0x03 is the band tag; a
        //  bare report line starts with a printable letter ('u'/'o'/'n').
        if (!u8csEmpty(line) && line[0][0] <= 0x03) {
            u8 band = line[0][0];
            a_rest(u8c, data, line, 1);     //  drop the band tag byte
            if (band == 0x02 || band == 0x03) {
                //  Progress / fatal text → stderr live (mirrors the
                //  fetch-path band-2 forwarder, KEEPIngestStream).
                while (!u8csEmpty(data)) {
                    ssize_t w = write(STDERR_FILENO, data[0],
                                      (size_t)u8csLen(data));
                    if (w > 0) { data[0] += w; continue; }
                    if (w < 0 && errno == EINTR) continue;
                    break;
                }
                continue;
            }
            //  band-1: accumulate, then drain embedded report pkt-lines.
            if (!u8csEmpty(data)) {
                if (u8bHasRoom(b1)) u8bFeed(b1, data);
            }
            u8cs b1adv = {u8bDataHead(b1), u8bIdleHead(b1)};
            for (;;) {
                u8cs rl = {};
                ok64 rd = PKTu8sDrain(b1adv, rl);
                if (rd == NODATA) break;
                if (rd == PKTFLUSH || rd == PKTDELIM) continue;
                if (rd != OK) break;
                wpush_classify_report(rl, ng_match, &unpack_ok, &ref_ok);
            }
            //  Reclaim the consumed prefix so the buffer can keep filling.
            {
                size_t consumed = (size_t)(b1adv[0] - u8bDataC(b1)[0]);
                if (consumed) { u8bUsed(b1, consumed); u8bShift(b1, 0); }
            }
            continue;
        }

        //  Bare report pkt-line (non-sideband peer).
        wpush_classify_report(line, ng_match, &unpack_ok, &ref_ok);
    }
    return (unpack_ok && ref_ok) ? OK : WIRECLFL;
}

// --- push-history haveset extension ---
//
//  Real git's receive-pack only advertises current refs.  When peer's
//  current tips don't cover every object peer's keeper still holds
//  (force-push happened upstream, branch was reset, etc.), the
//  advertised-tip-only haveset misses objects peer already has from
//  prior pushes — wpush_build_pack then re-ships them.
//
//  Local `<store>/.be/refs` records every successful push as a
//  `<peer-uri>?<branch> → <new-sha>` row, so the historical "we
//  shipped this to peer" list is right there.  We walk that history,
//  filter by the resolved transport URI's scheme/host/path, and add
//  each row's sha's closure to the haveset.  Caveats:
//    * `git gc --prune=now` on peer invalidates the cache → we'd
//      send objects peer already collected (correct, just wasted).
//    * sha rows whose objects aren't in OUR local keeper anymore:
//      walk_commit aborts the subtree (haveset-mode tolerates).
typedef struct {
    keeper  *k;
    sha_set *haveset;
    u8cs     r_scheme, r_host, r_path;
    u32      n_walked;
} wpush_hist_ctx;

static ok64 wpush_collect_hist_cb(uri const *u, ron60 ts, ron60 verb,
                                  void *vctx) {
    sane(u && vctx);
    (void)ts;
    (void)verb;
    wpush_hist_ctx *c = (wpush_hist_ctx *)vctx;
    //  Exact scheme/host/path match.  The resolved transport URI is
    //  written into REFS by keeper_post (KEEP.exe.c step 5), so
    //  rows for THIS peer are byte-identical on those three slots.
    if (!u8csEq(u->scheme, c->r_scheme)) done;
    if (!u8csEq(u->host,   c->r_host))   done;
    if (!u8csEq(u->path,   c->r_path))   done;

    //  Fragment is `?<40-hex>` (post-WIRECLI write).  Strip the `?`
    //  and decode; tombstones (empty / zero-sha) drop out.
    u8cs frag = {u->fragment[0], u->fragment[1]};
    if (u8csEmpty(frag)) done;
    if (frag[0][0] == '?') u8csUsed(frag, 1);
    if (u8csLen(frag) != 40) done;
    sha1 sh = {};
    u8s bin = {sh.data, sh.data + 20};
    a_dup(u8c, hx, frag);
    if (HEXu8sDrainSome(bin, hx) != OK) done;
    if (bin[0] != sh.data + 20) done;
    if (sha1empty(&sh)) done;
    if (sha_set_has(c->haveset, &sh)) done;
    //  Walk the closure into haveset.  haveset-build mode (out=NULL)
    //  tolerates keeper misses so historical shas we no longer have
    //  locally don't abort the rest of the walk.
    (void)wpush_walk_commit(c->k, &sh, NULL, &(u32){0}, 0,
                            NULL, c->haveset);
    c->n_walked++;
    done;
}

//  Worker for WIREPush.  All scratch buffers (advbuf, packbuf,
//  peer_tips, haveset, shas, seen) acquired from BASS — auto-rewound
//  at the wrapper's `try()` boundary.  Closes wfd/rfd at the
//  protocol-required points.
#define WPUSH_PEER_TIPS_MAX 4096

//  See WIRE.h — number of objects in the last push's pack.  Reset at
//  the top of every push so a short-circuit / pre-pack reject leaves 0.
u32 WIREPushLastObjCount = 0;

static ok64 wire_push_inner(u8csc remote_uri, u8cs refname,
                             keeper *k, sha1cp local_tip_in,
                             int *wfd, int *rfd, b8 force) {
    sane(k && local_tip_in && wfd && rfd && *wfd >= 0 && *rfd >= 0);
    sha1 local_tip = *local_tip_in;
    WIREPushLastObjCount = 0;

    a_carve(u8, advbuf, WCLI_BUF);

    //  Drain peer advert; capture old tip if peer already has the ref.
    //  Also collect EVERY advertised tip — used below as roots for the
    //  "objects peer already has" walk, so we prune the local closure
    //  against the peer's full ref set, not just our specific branch.
    sha1 peer_tip = {};
    b8   have_peer = NO;
    a_carve(sha1, peer_tips_b, WPUSH_PEER_TIPS_MAX);
    sha1 *peer_tips = sha1bDataHead(peer_tips_b);
    u32   peer_tips_n = 0;
    ok64 pt = wpush_peer_tip(*rfd, advbuf, refname, &peer_tip, &have_peer,
                             peer_tips, &peer_tips_n,
                             WPUSH_PEER_TIPS_MAX);
    if (pt != OK) {
        trace("wpush: peer_tip drain failed\n");
        fail(pt);
    }
    //  `have_ref` = peer advertises OUR target ref (drives the FF
    //  gate / old-sha on the update line).  `peer_tips` = total refs
    //  peer advertises; ALL of them seed the have-set below, so an
    //  incremental push prunes against the peer's whole ref set even
    //  when our own ref is brand new (have_ref=0, peer_tips>0 — no
    //  longer a self-contradictory pair).
    trace(
            "wpush: peer advert drained, have_ref=%d peer_tips=%u\n",
            (int)have_peer, peer_tips_n);

    //  Short-circuit: peer already at our tip — nothing to push.
    if (have_peer && sha1Eq(&peer_tip, &local_tip)) {
        //  Still need to send a flush so the peer closes cleanly.
        a_pad(u8, flush_b, 8);
        PKTu8sFeedFlush(u8bIdle(flush_b));
        a_dup(u8c, fdata, u8bData(flush_b));
        FILEFeedAll(*wfd, fdata);
        done;
    }

    //  Live FF check: peer's advertised tip must be an ancestor of
    //  local_tip.  The cache-side check in keeper_post is skipped on
    //  cache miss, so a stale/empty cache would otherwise let a non-FF
    //  push through.  Receive-pack on the wire side won't refuse it
    //  either (git's `denyNonFastForwards=false` default).  This is
    //  the authoritative FF gate for `be post //remote`.  `force=YES`
    //  (PUT-to-remote per https://replicated.wiki/html/wiki/PUT.html §PUT Design invariant 9) skips it.
    if (!force && have_peer && !KEEPIsAncestor(&local_tip, &peer_tip)) {
        sha1hex lh = {}, ph = {};
        sha1hexFromSha1(&lh, &local_tip);
        sha1hexFromSha1(&ph, &peer_tip);
        trace(
                "wpush: non-fast-forward — local tip %.40s is not a "
                "descendant of peer tip %.40s.  Use `be patch` to "
                "merge, or `be put` to force-push.\n",
                lh.data, ph.data);
        return WIRECLNFF;
    }

    //  Build the have-set (objects the peer already has).  Walks
    //  EVERY advertised peer ref's commit + tree closure locally so
    //  the local pack-build can prune anything reachable from any
    //  peer ref — not just the matching branch.  Critical when our
    //  refname is a fresh branch (have_peer=NO) but the peer still
    //  has shared history via main / other branches.  Walks that hit
    //  KEEPNONE (object not in our local keeper) are tolerated —
    //  walk_commit aborts that subtree, the rest of the haveset
    //  remains valid.
    sha_set haveset = {};
    sha_set *have = NULL;
    a_carve(sha1, haveset_b, WPUSH_MAX_OBJS);
    haveset.items = sha1bDataHead(haveset_b);
    haveset.cap   = WPUSH_MAX_OBJS;
    if (peer_tips_n > 0) {
        for (u32 i = 0; i < peer_tips_n; i++) {
            (void)wpush_walk_commit(k, &peer_tips[i], NULL,
                                    &(u32){0}, 0, NULL, &haveset);
        }
        have = &haveset;
        trace(
                "wpush: have-set has %u objects (from %u peer refs)\n",
                haveset.n, peer_tips_n);
    }

    //  Extend the haveset with every sha we previously pushed to this
    //  peer (see comment on wpush_collect_hist_cb).  Critical when
    //  peer's current advert no longer covers objects from earlier
    //  pushes (force-push upstream, branch reset, divergent tip).
    {
        uri ru = {};
        a_dup(u8c, ruv, remote_uri);
        if (URIutf8Drain(ruv, &ru) == OK) {
            a_path(keepdir);
            if (HOMEBranchDir(k->h, keepdir, NULL) == OK) {
                wpush_hist_ctx hc = {.k = k, .haveset = &haveset};
                hc.r_scheme[0] = ru.scheme[0]; hc.r_scheme[1] = ru.scheme[1];
                hc.r_host[0]   = ru.host[0];   hc.r_host[1]   = ru.host[1];
                hc.r_path[0]   = ru.path[0];   hc.r_path[1]   = ru.path[1];
                (void)REFSEachRecord($path(keepdir),
                                     wpush_collect_hist_cb, &hc);
                if (hc.n_walked > 0) {
                    trace(
                            "wpush: have-set now %u objects "
                            "(+%u history shas)\n",
                            haveset.n, hc.n_walked);
                }
            }
        }
        have = &haveset;
    }

    //  Walk the local commit's reachable closure, skipping anything
    //  the peer already advertised (via `have`) and following the
    //  parent chain so multi-commit FF pushes carry intermediate
    //  commits.
    a_carve(sha1, shas_b, WPUSH_MAX_OBJS);
    sha1 *shas = sha1bDataHead(shas_b);
    //  Dedup-set for the local-side walk.  Without it, every tree
    //  shared by N parents (every history fan-in) gets walked N
    //  times, blowing past WPUSH_MAX_OBJS on any non-trivial repo.
    //  `have` (from peer's matching ref) prunes shared-with-peer
    //  ancestors; this fresh set prunes within our own closure.
    sha_set seen = {};
    a_carve(sha1, seen_b, WPUSH_MAX_OBJS);
    seen.items = sha1bDataHead(seen_b);
    seen.cap   = WPUSH_MAX_OBJS;
    u32 nshas = 0;
    ok64 wro = wpush_walk_commit(k, &local_tip, shas, &nshas,
                                 WPUSH_MAX_OBJS, have, &seen);
    trace("wpush: walk_commit rc=%llx nshas=%u\n",
            (unsigned long long)wro, nshas);
    if (wro != OK || nshas == 0) fail(wro != OK ? wro : WIRECLFL);
    trace("wpush: walked %u objects\n", nshas);
    WIREPushLastObjCount = nshas;  //  pack size after have-set pruning

    //  Build the pack.
    a_carve(u8, packbuf, 1ULL << 26);
    ok64 bp = wpush_build_pack(k, shas, nshas, packbuf);
    if (bp != OK) {
        trace("wpush: build_pack failed\n");
        fail(bp);
    }
    trace("wpush: pack built (%llu bytes)\n",
            (unsigned long long)u8bDataLen(packbuf));

    //  Send the ref-update line + flush.
    ok64 su = wpush_send_update(*wfd, &peer_tip, &local_tip, refname,
                                have_peer);
    if (su != OK) {
        trace("wpush: send_update failed\n");
        fail(su);
    }
    //  Send the pack bytes, interleaving with a status drain so a peer
    //  that emits progress/status while still reading the pack can't
    //  deadlock us (both sides blocked on full pipe buffers).
    a_carve(u8, status_pre, WCLI_BUF);
    {
        a_dup(u8c, pdata, u8bData(packbuf));
        ok64 wo = wpush_send_pack_interleaved(*wfd, *rfd, pdata, status_pre);
        if (wo != OK) {
            trace("wpush: pack send failed\n");
            fail(wo);
        }
    }
    close(*wfd); *wfd = -1;

    //  Drain status (seeded with any bytes read during the send).
    ok64 rv = wpush_drain_status(*rfd, refname, u8bDataC(status_pre));
    if (rv != OK) trace("wpush: drain_status returned non-OK\n");
    close(*rfd); *rfd = -1;

    //  Pushed-difference banner (POST only).  List the commits this
    //  push introduced to the peer and the files they touch, in ULOG
    //  status format on stdout — mirrors GET's checkout banner
    //  (https://replicated.wiki/html/wiki/POST.html §POST).  `force` pushes (PUT) advance refs without a
    //  diff promise, so they stay silent — and where the remote tip is
    //  unknowable that is the only safe path (use `be put`).  Empty
    //  peer (`!have_peer`) ⇒ base is the empty tree, so every commit /
    //  file prints as new.  Best-effort: a render hiccup never fails an
    //  already-successful push.
    if (rv == OK && !force) {
        a_carve(sha1, pc_b, WPUSH_PEER_TIPS_MAX);
        sha1 *pcommits = sha1bDataHead(pc_b);
        u32   npc = 0;
        sha_set cseen = {};
        a_carve(sha1, cseen_b, WPUSH_MAX_OBJS);
        cseen.items = sha1bDataHead(cseen_b);
        cseen.cap   = WPUSH_MAX_OBJS;
        (void)wpush_collect_commits(k, &local_tip, have, &cseen,
                                    pcommits, &npc, WPUSH_PEER_TIPS_MAX);
        for (u32 i = 0; i < npc; i++)
            try(KEEPEmitCommitLine, &pcommits[i], (ron60)0);
        try(KEEPEmitTreeDiffFiles,
            have_peer ? &peer_tip : NULL, &local_tip, (ron60)0);
    }
    return rv;
}

ok64 WIREPush(u8csc remote_uri, u8csc local_branch,
              sha1cp local_tip_in, b8 force) {
    sane($ok(remote_uri));
    FILEIgnoreSIGPIPE();  //  peer dying mid-transfer must not kill us
    WIREPushLastObjCount = 0;  //  no-pack-on-error: reset before any return
    keeper *k = &KEEP;
    //  `local_branch` is be-side; empty (NULL or zero-length) selects
    //  the trunk shard, which goes on the wire as `refs/heads/main`.
    if (u8csEmpty(remote_uri)) return WIRECLFL;
    if (!local_tip_in || sha1empty(local_tip_in)) return WIRECLNRF;

    //  Build the wire refname (refs/heads/X, trunk → main) once.
    a_pad(u8, refname_buf, 256);
    call(wcli_be_to_wire, refname_buf, local_branch);
    u8cs refname = {u8bDataHead(refname_buf), u8bIdleHead(refname_buf)};

    //  Spawn receive-pack on the peer.
    trace("wpush: spawning receive-pack, remote=%.*s\n",
            (int)u8csLen(remote_uri), (char const *)remote_uri[0]);
    int wfd = -1, rfd = -1;
    pid_t pid = 0;
    ok64 so = wcli_spawn(remote_uri, "receive-pack", &wfd, &rfd, &pid);
    if (so != OK) {
        trace("wpush: spawn failed (so=%llx)\n",
                (unsigned long long)so);
        return WIRECLFL;
    }
    trace("wpush: spawned ok, pid=%d\n", (int)pid);

    try(wire_push_inner, remote_uri, refname, k, local_tip_in,
                          &wfd, &rfd, force);
    ok64 rv = __;

    if (wfd >= 0) close(wfd);
    if (rfd >= 0) close(rfd);
    if (pid > 0) {
        int rc = 0;
        FILEReap(pid, &rc);
    }
    return rv;
}

// --- WIREPushDelete -----------------------------------------------------
//
//  Delete-only push: send `<peer_old> 000…0 refs/heads/<X>` with no
//  packfile body.  receive-pack accepts a delete command without a
//  pack — see git's pack-protocol.txt §"updates" ("If the only
//  commands are deletes, the client MAY skip the pack data").

//  Worker for WIREPushDelete: drain peer advert, send delete update
//  (or just flush if peer doesn't have the ref), drain status.
static ok64 wire_push_delete_inner(u8cs refname, int *wfd, int *rfd) {
    sane(wfd && rfd && *wfd >= 0 && *rfd >= 0);

    a_carve(u8, advbuf, WCLI_BUF);

    sha1 peer_tip = {};
    b8   have_peer = NO;
    ok64 pt = wpush_peer_tip(*rfd, advbuf, refname, &peer_tip, &have_peer,
                             NULL, NULL, 0);
    if (pt != OK) {
        trace("wpush: delete peer_tip drain failed\n");
        fail(pt);
    }

    if (!have_peer) {
        //  Peer did not advertise the ref — nothing to delete.  Send a
        //  flush so the peer closes cleanly, surface as WIRECLNRF.
        a_pad(u8, flush_b, 8);
        PKTu8sFeedFlush(u8bIdle(flush_b));
        a_dup(u8c, fdata, u8bData(flush_b));
        FILEFeedAll(*wfd, fdata);
        return WIRECLNRF;
    }

    sha1 zero = {};
    ok64 su = wpush_send_update(*wfd, &peer_tip, &zero, refname, YES);
    if (su != OK) {
        trace("wpush: delete send_update failed\n");
        fail(su);
    }
    //  No pack body: receive-pack treats a delete-only command list as
    //  not requiring a packfile.  Close writer to signal end-of-input.
    close(*wfd); *wfd = -1;

    ok64 rv = wpush_drain_status(*rfd, refname, (u8csc){0});
    if (rv != OK)
        trace("wpush: delete drain_status returned non-OK\n");
    close(*rfd); *rfd = -1;
    return rv;
}

ok64 WIREPushDelete(u8csc remote_uri, u8csc local_branch) {
    sane($ok(remote_uri));
    FILEIgnoreSIGPIPE();  //  peer dying mid-transfer must not kill us
    if (u8csEmpty(remote_uri)) return WIRECLFL;

    a_pad(u8, refname_buf, 256);
    call(wcli_be_to_wire, refname_buf, local_branch);
    u8cs refname = {u8bDataHead(refname_buf), u8bIdleHead(refname_buf)};

    trace("wpush: spawning receive-pack (delete), remote=%.*s\n",
            (int)u8csLen(remote_uri), (char const *)remote_uri[0]);
    int wfd = -1, rfd = -1;
    pid_t pid = 0;
    ok64 so = wcli_spawn(remote_uri, "receive-pack", &wfd, &rfd, &pid);
    if (so != OK) {
        trace("wpush: delete spawn failed (so=%llx)\n",
                (unsigned long long)so);
        return WIRECLFL;
    }

    try(wire_push_delete_inner, refname, &wfd, &rfd);
    ok64 rv = __;

    if (wfd >= 0) close(wfd);
    if (rfd >= 0) close(rfd);
    if (pid > 0) {
        int rc = 0;
        FILEReap(pid, &rc);
    }
    return rv;
}
