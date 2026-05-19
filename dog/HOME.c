#include "HOME.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "DOG.h"
#include "DPATH.h"
#include "ULOG.h"
#include "abc/FILE.h"
#include "abc/PRO.h"
#include "TOMLT.h"

// --- HOMEOpen / HOMEClose ---

static ok64 home_anchor_resolve(home *h, u8cs anchor);

// Capture stdout of `git config --global --get <key>` into out.
// Returns NODATA if git exits non-zero (key unset) or the subprocess
// cannot be spawned.  Trailing '\n' is trimmed.
static ok64 home_git_config_get(char const *key, u8s out) {
    sane($ok(out));
    a_cstr(gitp, "/usr/bin/git");
    u8cs av[] = {
        u8slit("git"),
        u8slit("config"),
        u8slit("--global"),
        u8slit("--get"),
        u8scstr(key),
    };
    u8css argv = {av, av + 5};

    pid_t pid = 0;
    int rfd = -1;
    if (FILESpawn(gitp, argv, NULL, &rfd, &pid) != OK) return NODATA;

    a_pad(u8, buf, 256);
    FILEEnsureSoft(rfd, buf, u8bIdleLen(buf));
    FILEClose(&rfd);

    int rc = -1;
    FILEReap(pid, &rc);
    if (rc != 0) return NODATA;

    u8cs raw = {u8bDataHead(buf), u8bIdleHead(buf)};
    while (!$empty(raw) && (raw[1][-1] == '\n' || raw[1][-1] == '\r'))
        raw[1]--;
    if ($empty(raw)) return NODATA;
    return u8sFeed(out, raw);
}

// Fresh clones: seed .be/config from git's global config so commits
// and ref authorities get a sensible default identity without manual
// setup.  Called from HOMEOpen when rw=YES and config is absent.
// Best-effort — silent on any failure.
static void home_bootstrap_config(home *h) {
    if (!h->rw) return;

    a_pad(u8, emailbuf, 256);
    a_pad(u8, namebuf,  256);
    u8s email = {emailbuf[0], emailbuf[3]};
    u8s name  = {namebuf[0],  namebuf[3]};
    u8cp email_start = email[0];
    u8cp name_start  = name[0];
    b8 got_email = (home_git_config_get("user.email", email) == OK);
    b8 got_name  = (home_git_config_get("user.name",  name)  == OK);
    if (!got_email && !got_name) return;

    a_path(bedir);
    a_dup(u8c, root_s, u8bDataC(h->root));
    if (PATHu8bFeed(bedir, root_s) != OK) return;
    if (PATHu8bPush(bedir, DOG_BE_S) != OK) return;
    if (FILEMakeDirP($path(bedir)) != OK) return;

    a_path(cfgp);
    a_dup(u8c, be_s, u8bDataC(bedir));
    if (PATHu8bFeed(cfgp, be_s) != OK) return;
    if (PATHu8bPush(cfgp, DOG_CONFIG_S) != OK) return;

    a_pad(u8, body, 1024);
    a_cstr(hdr, "[user]\n");
    u8bFeed(body, hdr);
    if (got_name) {
        a_cstr(key, "name = \"");
        a_cstr(eol, "\"\n");
        u8cs v = {name_start, name[0]};
        u8bFeed(body, key);
        u8bFeed(body, v);
        u8bFeed(body, eol);
    }
    if (got_email) {
        a_cstr(key, "email = \"");
        a_cstr(eol, "\"\n");
        u8cs v = {email_start, email[0]};
        u8bFeed(body, key);
        u8bFeed(body, v);
        u8bFeed(body, eol);
    }

    int fd = -1;
    if (FILECreate(&fd, $path(cfgp)) != OK) return;
    u8cs data = {u8bDataHead(body), u8bIdleHead(body)};
    FILEFeedAll(fd, data);
    FILEClose(&fd);
}

// rw bootstrap: idempotently materialize the canonical empty-state
// layout under <h->root>/.be — directory plus the two append-only
// marker logs (`refs`, `wtlog`).  Stat-then-create avoids truncating
// existing data; an empty file is the well-defined "no rows yet"
// state for both logs, so creating fresh ones is safe.  Best-effort
// from the caller's perspective: downstream code will still fail
// loudly if it can't read what it needs.
static ok64 home_ensure_markers(home *h) {
    sane(h != NULL && u8bHasData(h->root));
    a_dup(u8c, root_s, u8bDataC(h->root));
    a_path(bedir);
    call(PATHu8bFeed, bedir, root_s);
    call(PATHu8bPush, bedir, DOG_BE_S);
    call(FILEMakeDirP, $path(bedir));
    a_dup(u8c, bedir_s, u8bDataC(bedir));

    u8cs const markers[2] = {
        {DOG_REFS_S[0],  DOG_REFS_S[1]},
        {DOG_WTLOG_S[0], DOG_WTLOG_S[1]},
    };
    for (int i = 0; i < 2; i++) {
        a_path(mp);
        call(PATHu8bFeed, mp, bedir_s);
        call(PATHu8bPush, mp, markers[i]);
        filestat fs = {};
        if (FILEStat(&fs, $path(mp)) == OK) continue;
        int fd = -1;
        call(FILECreate, &fd, $path(mp));
        FILEClose(&fd);
    }
    done;
}

// Worker body for HOMEOpen.  Allocates buffers, mmaps the arena,
// resolves wt/root, mmaps config.  Any early-return on failure leaves
// already-allocated resources for the wrapper to release via
// `HOMEClose` (which is null-safe per field).
static ok64 home_open_inner(home *h, uricp at, b8 rw) {
    sane(h != NULL);
    zerop(h);
    h->rw = rw;

    // 0. Branch-sharding scaffolding: interning buffer + open-branch
    // slice stack.  Empty until the first HOMEOpenBranch call.
    call(u8bAllocate, h->branches_data, HOME_BRANCHES_DATA_SIZE);
    h->open_branches_count = 0;
    h->write_frozen = NO;

    // 1. Path buffers for wt and repo root, 1 KB each; tip buffers
    // for branch path (interning size) and sha (40-hex); project
    // segment (basename-sized).
    call(u8bAllocate, h->root,       FILE_PATH_MAX_LEN);
    call(u8bAllocate, h->wt,         FILE_PATH_MAX_LEN);
    call(u8bAllocate, h->project,    256);
    call(u8bAllocate, h->cur_branch, 256);
    call(u8bAllocate, h->cur_sha,    64);

    // 2. Resolve root + wt:
    //    * Explicit URI path  → use as h->root.
    //                            h->wt = cwd if query/fragment is
    //                            present (`--at` forward from `be`),
    //                            else colocated (== h->root).
    //    * No path             → HOMEFindDogs walks up and sets BOTH
    //                            h->wt (anchor location) and h->root
    //                            (store root: same as wt for primary,
    //                            redirected via row-0 for secondary).
    u8cs at_path  = {};
    u8cs at_query = {};
    u8cs at_frag  = {};
    if (at != NULL) {
        u8csMv(at_path,  at->path);
        u8csMv(at_query, at->query);
        u8csMv(at_frag,  at->fragment);
    }
    if (!u8csEmpty(at_path)) {
        //  Resolve the anchor at `at_path` (peek `.be`'s shape; if it's
        //  a regular file, redirect h->root to the primary store via
        //  the wtlog's row-0 `repo` URI; otherwise h->root == at_path).
        call(home_anchor_resolve, h, at_path);
        //  `--at <root>?<branch>#<sha>` forward from `be`: subprocess
        //  cwd is the actual worktree, which may differ from the
        //  anchor `at_path` carried over the boundary.  Override h->wt
        //  with the cwd in that case.
        if (!u8csEmpty(at_query) || !u8csEmpty(at_frag)) {
            a_path(cwdp);
            if (FILEGetCwd(cwdp) == OK) {
                u8bReset(h->wt);
                a_dup(u8c, cwd_s, u8bDataC(cwdp));
                call(PATHu8bFeed, h->wt, cwd_s);
            }
        }
    } else {
        ok64 fr = HOMEFindDogs(h);   // sets both h->wt and h->root
        if (fr == NOHOME && rw) {
            //  Fresh-clone / `be put` in an empty dir: anchor at cwd
            //  instead of failing — the marker-creation step below will
            //  lay down `.be/{refs,wtlog}` so the next HOMEOpen walks
            //  up to a well-formed anchor.
            a_path(cwdp);
            call(FILEGetCwd, cwdp);
            a_dup(u8c, cwd_s, u8bDataC(cwdp));
            u8bReset(h->wt);
            u8bReset(h->root);
            call(PATHu8bFeed, h->wt,   cwd_s);
            call(PATHu8bFeed, h->root, cwd_s);
        } else if (fr != OK) {
            return fr;
        }
    }

    //  rw bootstrap: ensure `<root>/.be/{refs,wtlog}` exist.  Idempotent
    //  and best-effort — failure here doesn't abort the open (a read-only
    //  filesystem still gets a chance to fail later with a more specific
    //  error from whichever sub-system actually needed to write).
    if (rw) (void)home_ensure_markers(h);

    // 3. Scratch arena — 4 GB VA, pages on demand.
    call(u8bMap, h->arena, HOME_ARENA_SIZE);

    // 4. Config mmap (best-effort).  Missing file is not an error.
    a_path(cfg);
    a_dup(u8c, root_s, u8bDataC(h->root));
    call(PATHu8bFeed, cfg, root_s);
    call(PATHu8bPush, cfg, DOG_BE_S);
    ok64 po = PATHu8bPush(cfg, DOG_CONFIG_S);
    if (po == OK) {
        u8bp mapped = NULL;
        if (FILEMapRO(&mapped, $path(cfg)) != OK && rw) {
            // Fresh clone: seed from git's global config, then retry.
            home_bootstrap_config(h);
            FILEMapRO(&mapped, $path(cfg));
        }
        if (mapped != NULL) {
            for (int i = 0; i < 4; i++)
                ((u8 **)h->config)[i] = mapped[i];
        }
    }

    // 5. Tip from URI: query → cur_branch + claim slot 0 via
    //    HOMEOpenBranch; fragment → cur_sha.  Trunk maps to the
    //    canonical empty branch — feed an empty slice when query is
    //    absent so HOMEOpenBranch's normalizer claims slot 0 anyway.
    //    No tip yet (fresh clone, direct sub-dog without --at) →
    //    leave both buffers empty and skip the branch claim.
    if (!u8csEmpty(at_query) || !u8csEmpty(at_frag)) {
        //  Reset before feed: home_anchor_resolve may have pre-filled
        //  h->cur_branch from the row-0 anchor's `/.be/<branch>/`
        //  segment; appending the at_query slot would concatenate the
        //  two (e.g. anchor "origin" + at_query "master" → "originmaster")
        //  and route reads/writes through a nonexistent branch dir.
        //  When at_query is non-empty it's authoritative; when empty,
        //  preserve the anchor-derived branch.
        if (!u8csEmpty(at_query)) {
            u8bReset(h->cur_branch);
            u8bFeed(h->cur_branch, at_query);
        }
        if (!u8csEmpty(at_frag)) u8bFeed(h->cur_sha, at_frag);
        a_dup(u8c, br, u8bDataC(h->cur_branch));
        ok64 bo = HOMEOpenBranch(h, br, rw);
        if (bo != OK && bo != HOMEOPEN) return bo;
    }
    done;
}

// Wrapper: runs the worker via `try` so partial-init state from any
// failure path (e.g. `HOMEFindDogs` returning NOHOME) gets released
// via HOMEClose.  Per ABC.md §"Resource lifecycle" — one entry fn
// holds the cleanup, worker fn does the work.
ok64 HOMEOpen(home *h, uricp at, b8 rw) {
    sane(h != NULL);
    try(home_open_inner, h, at, rw);
    nedo HOMEClose(h);
    done;
}

ok64 HOMEClose(home *h) {
    sane(h != NULL);
    if (h->config[0]        != NULL) FILEUnMap(h->config);
    if (h->arena[0]         != NULL) u8bUnMap(h->arena);
    if (h->root[0]          != NULL) u8bFree(h->root);
    if (h->wt[0]            != NULL) u8bFree(h->wt);
    if (h->project[0]       != NULL) u8bFree(h->project);
    if (h->cur_branch[0]    != NULL) u8bFree(h->cur_branch);
    if (h->cur_sha[0]       != NULL) u8bFree(h->cur_sha);
    if (h->branches_data[0] != NULL) u8bFree(h->branches_data);
    zerop(h);
    done;
}

// --- Branch-sharding (Phase 0) ---

ok64 HOMEOpenBranch(home *h, u8cs branch, b8 rw) {
    sane(h != NULL && $ok(branch));

    // Normalize into a scratch buffer first so we can dedup without
    // polluting the interning store on a hit.
    a_pad(u8, normbuf, 256);
    call(DPATHBranchNormFeed, normbuf, branch);
    a_dup(u8c, norm, u8bDataC(normbuf));

    // Already open?
    for (size_t i = 0; i < h->open_branches_count; i++) {
        u8cs slot = {h->open_branches[i][0], h->open_branches[i][1]};
        if (u8csEq(slot, norm)) {
            if (rw && (i != 0 || !h->write_frozen))
                return HOMEROBR;
            return HOMEOPEN;
        }
    }

    size_t before_n = h->open_branches_count;

    // rw is only grantable on the very first open.
    if (rw && before_n > 0) return HOMEROBR;

    // Capacity checks: slot array + interning buffer.
    if (before_n >= HOME_OPEN_BRANCHES_MAX) return HOMEMAX;
    if (u8csLen(norm) > u8bIdleLen(h->branches_data))
        return HOMEMAX;

    // Intern the canonical bytes and append the resulting slice.
    u8cp at = u8bIdleHead(h->branches_data);
    call(u8bFeed, h->branches_data, norm);
    h->open_branches[before_n][0] = at;
    h->open_branches[before_n][1] = u8bIdleHead(h->branches_data);
    h->open_branches_count = before_n + 1;

    if (before_n == 0 && rw) h->write_frozen = YES;
    done;
}

ok64 HOMEWriteBranch(home const *h, u8cs out) {
    sane(h != NULL);
    if (!h->write_frozen || h->open_branches_count == 0)
        return HOMENOBR;
    out[0] = h->open_branches[0][0];
    out[1] = h->open_branches[0][1];
    done;
}

b8 HOMEBranchVisible(home const *h, u8cs branch) {
    for (size_t i = 0; i < h->open_branches_count; i++) {
        u8cs slot = {h->open_branches[i][0], h->open_branches[i][1]};
        if (DPATHBranchAncestor(branch, slot)) return YES;
    }
    return NO;
}

// --- Workspace finders ---

//  Peek the first line of a wtlog (either a secondary-wt's `.be`
//  file OR a primary's `<wt>/.be/wtlog`) and extract the URI's path
//  bytes into `path_out` (a slice that lives inside `arena`'s busy
//  region).
//
//  Returns OK on success, NODATA when the file is empty or row 0 is
//  not parsable.  Callers must defend against non-anchor row-0
//  shapes — typically by passing the result through
//  DOGProjectFromBe, which returns empty for any URI without a
//  `/.be/` segment.
static ok64 home_peek_repo_uri(path8s be_path, u8bp arena, u8csp path_out) {
    sane($ok(be_path) && arena && path_out);
    u8bp map = NULL;
    if (FILEMapRO(&map, be_path) != OK) return NODATA;
    u8cs scan = {u8bDataHead(map), u8bIdleHead(map)};
    //  Find the first newline so the drain doesn't run past row 0.
    u8cp nl = scan[0];
    while (nl < scan[1] && *nl != '\n') nl++;
    if (nl == scan[1]) { FILEUnMap(map); return NODATA; }
    u8cs row = {scan[0], nl + 1};
    ulogrec rec = {};
    ok64 dr = ULOGu8sDrain(row, &rec);
    if (dr != OK) { FILEUnMap(map); return NODATA; }
    //  Copy the URI path bytes into the caller's arena before unmap —
    //  the slice in `rec` points into the soon-to-be-invalid map.
    u8cs ru_path = {rec.uri.path[0], rec.uri.path[1]};
    if (u8csEmpty(ru_path)) { FILEUnMap(map); return NODATA; }
    u8cp start = u8bIdleHead(arena);
    ok64 fo = u8bFeed(arena, ru_path);
    FILEUnMap(map);
    if (fo != OK) return fo;
    path_out[0] = start;
    path_out[1] = u8bIdleHead(arena);
    return OK;
}

//  Given a wt anchor path (where `.be` lives), populate h->wt with
//  the anchor itself and h->root with the store root:
//    * `.be` is a DIR (primary)        → h->root = anchor
//    * `.be` is a regular FILE (sec)   → h->root from row-0 `repo` URI
//    * absent / unparsable             → h->root = anchor (fallback)
static ok64 home_anchor_resolve(home *h, u8cs anchor) {
    sane(h && $ok(anchor));
    u8bReset(h->wt);
    u8bReset(h->root);
    call(PATHu8bFeed, h->wt, anchor);

    a_path(probe);
    a_dup(u8c, anchor_s, u8bDataC(h->wt));
    call(PATHu8bFeed, probe, anchor_s);
    call(PATHu8bPush, probe, DOG_BE_S);

    filestat fs = {};
    ok64 so = FILEStat(&fs, $path(probe));
    if (so == OK && fs.kind == FILE_KIND_REG) {
        a_path(arena);
        u8cs repo_path = {};
        if (home_peek_repo_uri($path(probe), arena, repo_path) == OK) {
            a_path(stripped);
            DOGRepoFromBe(repo_path, stripped);
            if (u8bDataLen(stripped) > 0) {
                a_dup(u8c, sb, u8bDataC(stripped));
                call(PATHu8bFeed, h->root, sb);
                //  Project-sharded layout: the first segment after
                //  `/.be/` in the anchor URI names the project shard.
                //  Pre-fill `h->project` so downstream openers can
                //  route reads/writes through the right shard.  Empty
                //  result == legacy single-project anchor; leave
                //  `h->project` empty and let consumers fall back to
                //  the implicit single project.
                a_path(proj_buf);
                DOGProjectFromBe(repo_path, proj_buf);
                if (u8bDataLen(proj_buf) > 0) {
                    a_dup(u8c, ps, u8bDataC(proj_buf));
                    call(u8bFeed, h->project, ps);
                }
                //  Anchor URI may carry a branch suffix after `/.be/`
                //  (`file:<root>/.be/<basename>/` for a submodule mount).
                //  Pre-fill `h->cur_branch` so downstream openers route
                //  reads/writes through the right keeper leaf even when
                //  no `get`/`post` row yet exists in the secondary wtlog.
                //  TODO: under the project-sharded layout this slice
                //  starts with the project segment; consumers wanting
                //  just the branch should strip `h->project` from the
                //  front.  Cleanup pending alongside the call-site
                //  migration.
                a_path(br_buf);
                DOGBranchFromBe(repo_path, br_buf);
                if (u8bDataLen(br_buf) > 0) {
                    u8bReset(h->cur_branch);
                    a_dup(u8c, brs, u8bDataC(br_buf));
                    call(u8bFeed, h->cur_branch, brs);
                }
                done;
            }
        }
    }
    //  Primary / colocated / unreadable row 0: store == wt.
    a_dup(u8c, wt_s, u8bDataC(h->wt));
    call(PATHu8bFeed, h->root, wt_s);
    //  Primary wts may also carry a project anchor on row 0 of
    //  <wt>/.be/wtlog.  When present, populate `h->project` so
    //  keeper composes paths through the project shard.  Non-anchor
    //  row-0 verbs (put/post/get/…) have URI paths that don't carry
    //  a `/.be/` segment, so DOGProjectFromBe returns empty for
    //  them — h->project stays empty for legacy single-project wts.
    if (so == OK && fs.kind == FILE_KIND_DIR) {
        a_path(wtlog_probe);
        call(PATHu8bFeed, wtlog_probe, anchor_s);
        call(PATHu8bPush,  wtlog_probe, DOG_BE_S);
        call(PATHu8bPush,  wtlog_probe, DOG_WTLOG_S);
        a_path(arena2);
        u8cs repo_path = {};
        if (home_peek_repo_uri($path(wtlog_probe), arena2, repo_path) == OK) {
            a_path(proj_buf);
            DOGProjectFromBe(repo_path, proj_buf);
            if (u8bDataLen(proj_buf) > 0) {
                a_dup(u8c, ps, u8bDataC(proj_buf));
                call(u8bFeed, h->project, ps);
            }
        }
    }
    done;
}

// Walk up from cwd to the first ancestor containing a `.be` anchor.
// Fills h->wt with the anchor location and h->root via
// `home_anchor_resolve` (primary: wt; secondary: row-0 redirect).
// Returns NOHOME if the walk reaches / without finding an anchor.
static ok64 home_walk_up(home *h) {
    sane(h != NULL);

    a_path(here);
    test(FILEGetCwd(here) == OK, NOHOME);

    for (;;) {
        a_path(probe);
        a_dup(u8c, cur, u8bDataC(here));
        call(PATHu8bFeed, probe, cur);
        call(PATHu8bPush, probe, DOG_BE_S);
        filestat fs = {};
        if (FILEStat(&fs, $path(probe)) == OK &&
            (fs.kind == FILE_KIND_DIR || fs.kind == FILE_KIND_REG)) {
            a_dup(u8c, anchor, u8bDataC(here));
            return home_anchor_resolve(h, anchor);
        }

        size_t before = $len(u8bDataC(here));
        call(PATHu8bPop, here);
        size_t after = $len(u8bDataC(here));
        if (after >= before) return NOHOME;
    }
}

ok64 HOMEFind(home *h) {
    sane(h != NULL);
    return home_walk_up(h);
}

ok64 HOMEFindDogs(home *h) {
    sane(h != NULL);
    return home_walk_up(h);
}

// --- Resolve sibling binary ---

static b8 home_is_exe(path8s p) {
    filestat fs = {};
    if (FILEStat(&fs, p) != OK) return NO;
    return (fs.mode & 0100) ? YES : NO;
}

// If <dir>/<name> is executable, feed its full path into `out`.
static ok64 home_try_sibling(path8b out, u8csc dir, u8csc name) {
    sane(out != NULL && $ok(dir) && $ok(name));
    a_path(tmp);
    a_dup(u8c, dir_s, dir);
    a_dup(u8c, name_s, name);
    call(PATHu8bFeed, tmp, dir_s);
    call(PATHu8bPush, tmp, name_s);
    if (!home_is_exe($path(tmp))) return NONE;
    a_dup(u8c, src, u8bDataC(tmp));
    call(PATHu8bFeed, out, src);
    done;
}

ok64 HOMEResolveSibling(home *h, path8b out, u8csc name, u8csc argv0) {
    sane(out != NULL && $ok(name));
    (void)h;   // unused — sibling lookup is ambient (argv0 + PATH)

    if ($ok(argv0) && !u8csEmpty(argv0)) {
        // "Has a directory" means argv0 literally contains '/'.
        // PATHu8sDir synthesizes "." for bare names; that's the PATH
        // case, not the dirname case.
        a_dup(u8c, a0_scan, argv0);
        b8 has_slash = u8csFind(a0_scan, '/') == OK;
        if (has_slash) {
            a_dup(u8c, a0, argv0);
            u8cs dir = {};
            PATHu8sDir(dir, a0);
            u8csc dir_c = {dir[0], dir[1]};
            if (home_try_sibling(out, dir_c, name) == OK) done;
        } else {
            // Bare argv0 → scan PATH for it, then look beside it.
            char const *env = getenv("PATH");
            if (env != NULL) {
                a_cstr(env_s, env);
                a_dup(u8c, scan, env_s);
                while (!u8csEmpty(scan)) {
                    u8cs entry = {scan[0], scan[1]};
                    a_dup(u8c, probe, scan);
                    if (u8csFind(probe, ':') == OK) {
                        //  u8csFind advances probe[0] *to* the ':'.
                        //  entry is half-open [scan_start, colon);
                        //  advance scan past the ':' for next iter.
                        entry[1] = probe[0];
                        scan[0]  = probe[0] + 1;
                    } else {
                        scan[0]  = scan[1];
                    }
                    if (u8csEmpty(entry)) continue;

                    a_path(exe);
                    u8csc entry_c = {entry[0], entry[1]};
                    a_dup(u8c, a0_s, argv0);
                    if (PATHu8bFeed(exe, entry)   != OK) continue;
                    if (PATHu8bPush(exe, a0_s)    != OK) continue;
                    if (!home_is_exe($path(exe))) continue;
                    if (home_try_sibling(out, entry_c, name) == OK) done;
                    break;
                }
            }
        }
    }

    // Fallback: feed just `name` — caller can still hand it to execvp.
    a_dup(u8c, name_s, name);
    call(PATHu8bFeed, out, name_s);
    done;
}

// --- Config: .be/config (TOML) ---

// The TOML header `[a.b.c]` and dotted keys `a.b = "v"` both express the
// same dotted hierarchy.  We track the full active path in `current`
// (a path8b, segments joined by '/') and match it against `needle`
// supplied by the caller as a path8s.  On a match the cb feeds `out`
// and returns NODATA so TOMLTLexer `fbreak`s immediately.
typedef struct {
    path8s  needle;    // caller-supplied, e.g. $path(a_path(..,"a","b","c"))
    path8bp current;   // active dotted path, borrowed from caller (a_path)
    size_t  hdr_end;   // current's DATA length at end of last header
    u8s     out;
    u8      in_hdr;    // 1 between '[' and ']'
    u8      await_val; // 1 after '=' — next string is the value
} home_cfg_ctx;

static ok64 home_cfg_cb(u8 tag, u8cs tok, void *ctx) {
    sane(ctx != NULL);
    home_cfg_ctx *c = (home_cfg_ctx *)ctx;
    if (tag == 'D') return OK;   // comment

    // Whitespace with a newline closes a kv line; rewind `current`
    // back to the header root so the next line starts fresh.
    if (tag == 'W') {
        b8 nl = NO;
        for (u8cp p = tok[0]; p < tok[1]; p++)
            if (*p == '\n') { nl = YES; break; }
        if (nl && !c->in_hdr) {
            size_t dl = u8bDataLen(c->current);
            if (dl > c->hdr_end) u8bShed(c->current, dl - c->hdr_end);
            c->await_val = 0;
        }
        return OK;
    }

    // Header framing
    if ($len(tok) == 1 && tok[0][0] == '[') {
        c->in_hdr = 1;
        u8bReset(c->current);
        return OK;
    }
    if ($len(tok) == 1 && tok[0][0] == ']') {
        c->in_hdr = 0;
        c->hdr_end = u8bDataLen(c->current);
        return OK;
    }

    // Dotted separator between segments (inside or outside a header).
    // TOMLT tags it 'P' outside headers and 'R' inside (TOKSplitText).
    if ($len(tok) == 1 && tok[0][0] == '.')
        return OK;

    // '='
    if (tag == 'P' && $len(tok) == 1 && tok[0][0] == '=') {
        c->await_val = 1;
        return OK;
    }

    // Identifier: extends the active path (in header or building a key).
    if ((tag == 'S' || tag == 'R') && !c->await_val) {
        PATHu8bPush(c->current, tok);
        return OK;
    }

    // Quoted string value — match → feed, then NODATA to stop the lexer.
    if (c->await_val && tag == 'G' && $len(tok) >= 2) {
        a_dup(u8c, cu, u8bDataC(c->current));
        if ($eq(c->needle, cu)) {
            u8cs val = {tok[0] + 1, tok[1] - 1};
            call(u8sFeed, c->out, val);
            return NODATA;
        }
        return OK;
    }

    return OK;
}

ok64 HOMEHost(home *h, u8s out) {
    sane(h != NULL && $ok(out));
    a_cstr(user_s, "user");
    a_cstr(host_s, "host");
    a_cstr(mail_s, "email");
    a_path(needle, user_s, host_s);
    ok64 o = HOMEGetConfig(h, out, $path(needle));
    if (o == OK) done;
    if (o != NOCONF) return o;
    a_path(email, user_s, mail_s);
    return HOMEGetConfig(h, out, $path(email));
}

ok64 HOMEGetConfig(home *h, u8s value, path8s needle) {
    sane(h != NULL && $ok(value) && $ok(needle));

    if (h->config[0] == NULL) return NOCONF;

    a_path(current);
    home_cfg_ctx ctx = {
        .needle  = {needle[0], needle[1]},
        .current = current,
        .out     = {value[0], value[1]},
    };
    TOMLTstate st = {
        .data = {u8bDataHead(h->config), u8bIdleHead(h->config)},
        .cb   = home_cfg_cb,
        .ctx  = &ctx,
    };
    ok64 lo = TOMLTLexer(&st);
    if (lo != OK && lo != NODATA) return lo;
    if (ctx.out[0] == value[0]) return NOCONF;   // nothing fed
    value[0] = ctx.out[0];
    done;
}
