#include "HOME.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "DOG.h"
#include "DPATH.h"
#include "ULOG.h"
#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "TOMLT.h"

// --- HOMEOpen / HOMEClose ---

static ok64 home_anchor_resolve(home *h, u8cs anchor);
static ok64 home_single_shard(path8s p, u8b out);

// Capture stdout of `git config --global --get <key>` into out.
// Returns NODATA if git exits non-zero (key unset) or the subprocess
// cannot be spawned.  Trailing '\n' is trimmed.
static ok64 home_git_config_get(char const *key, u8s out) {
    sane($ok(out));
    a_cstr(gitp, "git");  // bare name: FILESpawn/execvp resolves it via PATH
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
    try(FILEEnsureSoft, rfd, buf, u8bIdleLen(buf));
    FILEClose(&rfd);

    int rc = -1;
    try(FILEReap, pid, &rc);  //  rc carries the subprocess outcome
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

    //  <root>/.be via the single store-dir composer (honors *.be-is-store,
    //  drops a `.be` seg, rw-gated mkdir).  h->rw is YES here (guarded above).
    a_path(bedir);
    u8cs noseg = {};
    if (HOMEMakeBeDir(h, noseg, bedir) != OK) return;

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
    (void)FILEFeedAll(fd, data);  //  best-effort: home_bootstrap_config is void
    FILEClose(&fd);
}

// rw bootstrap: idempotently materialize the canonical empty-state
// layout under <h->root>/.be — the `.be/` dir plus the per-worktree
// `wtlog` (always top-level).  `refs` is NOT created here: it belongs to
// the project shard (`.be/<project>/refs`) and is keeper's job (Store.mkd
// "Repo dir layout"; DIS-024 retires the top-level `.be/refs`).  An
// empty wtlog is the well-defined "no rows yet" state.  Best-effort:
// downstream code still fails loudly if it can't read what it needs.
static ok64 home_ensure_markers(home *h) {
    sane(h != NULL && u8bHasData(h->root));
    a_path(bedir);
    u8cs noseg = {};
    call(HOMEMakeBeDir, h, noseg, bedir);   // <root>/.be (*.be-guarded), mkdir in rw
    a_dup(u8c, bedir_s, u8bDataC(bedir));

    a_path(mp);
    call(PATHu8bFeed, mp, bedir_s);
    call(PATHu8bPush, mp, DOG_WTLOG_S);
    filestat fs = {};
    if (FILEStat(&fs, $path(mp)) != OK) {
        int fd = -1;
        call(FILECreate, &fd, $path(mp));
        FILEClose(&fd);
    }
    done;
}

//  Compose the store dir <h->root>/.be[/<seg>] into `out` — or
//  <h->root>[/<seg>] when h->root already ends in `.be` (a *.be path IS
//  the store).  Pure: no filesystem.  Empty `seg` → the bare `.be` dir.
ok64 HOMEBeDir(home const *h, u8cs seg, path8b out) {
    sane(h && out && u8bHasData(h->root));
    u8bReset(out);
    a_dup(u8c, root_s, u8bDataC(h->root));
    //  Strip any trailing '/' so the *.be suffix test is robust
    //  (`~/.be/` must still count as the store, not become `~/.be/.be`).
    while (!u8csEmpty(root_s) && *$last(root_s) == '/') u8csShed1(root_s);
    call(PATHu8bFeed, out, root_s);
    a_cstr(be_suf, DOG_BE_NAME);
    if (!u8csHasSuffix(root_s, be_suf)) call(PATHu8bPush, out, DOG_BE_S);
    //  A `.be` "project" segment is the store dir leaking in via a bad
    //  project derivation (DOGProjectFromBe on a `/.be/.be/` path) — never
    //  compose `<store>/.be/.be`.  Drop it.
    a_cstr(be_nm, DOG_BE_NAME);
    if (!u8csEmpty(seg) && !u8csEq(seg, be_nm)) call(PATHu8bPush, out, seg);
    done;
}

//  Compose, then create the dir — but only in rw mode (never mkdir a shard
//  read-only).  A read-only home just gets the composed path back.
ok64 HOMEMakeBeDir(home const *h, u8cs seg, path8b out) {
    sane(h && out);
    call(HOMEBeDir, h, seg, out);
    if (h->rw) call(FILEMakeDirP, $path(out));
    done;
}

//  OK iff <h->root>/.be/<project> exists AND its `refs` is non-empty — a
//  real populated store, not a missing/empty/stray shard.  Else HOMENOPROJ.
ok64 HOMEProjectExists(home const *h, u8cs project) {
    sane(h);
    a_path(shard);
    call(HOMEBeDir, h, project, shard);
    filestat fs = {};
    if (FILEStat(&fs, $path(shard)) != OK || fs.kind != FILE_KIND_DIR)
        fail(HOMENOPROJ);
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

    // 1. Path buffers for wt and repo root, 1 KB each; tip buffers
    // for branch path (HOME_BRANCH_MAX) and sha (40-hex); project
    // segment (basename-sized).  cur_rw stays NO until claimed.
    call(u8bAllocate, h->root,       FILE_PATH_MAX_LEN);
    call(u8bAllocate, h->wt,         FILE_PATH_MAX_LEN);
    call(u8bAllocate, h->project,    256);
    call(u8bAllocate, h->cur_branch, HOME_BRANCH_MAX);
    call(u8bAllocate, h->aux_branch, HOME_BRANCH_MAX);
    call(u8bAllocate, h->cur_sha,    64);
    h->cur_rw = NO;

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
        //  the wtlog's row-0 anchor URI (verb `get`, legacy `repo`);
        //  otherwise h->root == at_path).
        //
        //  `at_path` is the **wt root** (`sniff/AT.c::SNIFFAtTailOf`
        //  composes `--at` with the wt root in the path slot, not the
        //  store root — the store root rides in the wtlog row-0 URI).
        //  So `home_anchor_resolve` correctly sets h->wt = at_path and
        //  redirects h->root through row-0 when `.be` is a sentinel
        //  file.  No cwd-override needed: a sub-dog invoked from a
        //  subdir gets the right h->wt from at_path directly.
        call(home_anchor_resolve, h, at_path);
        (void)at_query; (void)at_frag;
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

    //  Project derivation (DIS-024 step 1): a primary wt whose row-0
    //  anchor carried no project (a flat / legacy store) takes its
    //  project from the worktree's own store — the single
    //  `<root>/.be/<project>/` shard.  Reading the name FROM THE STORE
    //  (not the wt dir basename) keeps it stable when a store is copied
    //  or checked out into a differently-named wt dir — a pervasive test
    //  pattern (`cp -r src/.be/. .be/`).  A flat store (no shard) leaves
    //  the project empty → flat layout; a multi-project store (>1 shard)
    //  is ambiguous and also left empty (callers pass an explicit
    //  `?/<project>/` query).  Secondary wts never reach here empty —
    //  home_anchor_resolve fills h->project from the `.be` file anchor.
    if (u8bEmpty(h->project)) {
        a_path(bedir);
        a_dup(u8c, root_s, u8bDataC(h->root));
        call(PATHu8bFeed, bedir, root_s);
        call(PATHu8bPush, bedir, DOG_BE_S);
        a_pad(u8, shardbuf, 256);
        ok64 ss = home_single_shard($path(bedir), shardbuf);
        if (ss == OK) {
            call(u8bFeed, h->project, u8bDataC(shardbuf));
        } else if (ss != NODATA) {
            return ss;                          //  real scan error, propagate
        }
        //  NODATA (flat store / ambiguous multi-project) leaves the
        //  project empty → flat layout, removed in DIS-024 step 4.
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
        //  h->cur_branch / h->project from the row-0 anchor.
        //  When at_query is non-empty it's authoritative; when empty,
        //  preserve the anchor-derived branch.
        //
        //  Query shape (per https://replicated.wiki/html/wiki/URI.html §"Ref resolution"):
        //    `?/<project>/<branch>` — absolute (leading `/`).  Splits
        //                              into h->project + h->cur_branch.
        //                              Override h->project (the
        //                              receiver may have been pre-set
        //                              to the parent's project by the
        //                              .be/wtlog probe in
        //                              home_anchor_resolve).
        //    `?<branch>` (no leading `/`) — project-relative; keeps the
        //                                    pre-filled h->project,
        //                                    sets h->cur_branch.
        if (!u8csEmpty(at_query)) {
            if (*at_query[0] == '/') {
                //  Absolute: split on first `/` after the leading one.
                a_dup(u8c, body, at_query);
                u8csUsed1(body);  // strip leading '/'
                u8c const *p = body[0];
                while (p < body[1] && *p != '/') p++;
                u8cs proj = {body[0], (u8c *)p};
                u8cs br   = {(u8c *)p, body[1]};
                if (!u8csEmpty(br) && *br[0] == '/') u8csUsed1(br);
                u8bReset(h->project);
                if (!u8csEmpty(proj)) u8bFeed(h->project, proj);
                u8bReset(h->cur_branch);
                if (!u8csEmpty(br)) u8bFeed(h->cur_branch, br);
            } else {
                //  Project-relative: branch only.
                u8bReset(h->cur_branch);
                u8bFeed(h->cur_branch, at_query);
            }
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
    if (h->aux_branch[0]    != NULL) u8bFree(h->aux_branch);
    if (h->cur_sha[0]       != NULL) u8bFree(h->cur_sha);
    zerop(h);
    done;
}

// --- Branch slots ---

ok64 HOMEOpenBranch(home *h, u8cs branch, b8 rw) {
    sane(h != NULL && $ok(branch));

    // Normalize into a scratch buffer first so we don't disturb the
    // slot buffers on a HOMEOPEN / HOMEROBR return.
    a_pad(u8, normbuf, HOME_BRANCH_MAX);
    call(DPATHBranchNormFeed, normbuf, branch);
    a_dup(u8c, norm, u8bDataC(normbuf));

    // rw always (re-)targets cur — the writable branch follows the
    // active keeper.  Sequential rw opens of different branches
    // (e.g. test/CLI: open trunk, close, open feature) re-target cur.
    if (rw) {
        a_dup(u8c, cur, u8bDataC(h->cur_branch));
        b8 same = h->cur_held && u8csEq(cur, norm);
        u8bReset(h->cur_branch);
        call(u8bFeed, h->cur_branch, norm);
        h->cur_held = YES;
        h->cur_rw   = YES;
        return same ? HOMEOPEN : OK;
    }

    // ro: first claim wins; idempotent on match.  Reset before feed —
    // home_open_inner may have pre-filled cur_branch from the
    // anchor/at-query before calling us.
    if (!h->cur_held) {
        u8bReset(h->cur_branch);
        call(u8bFeed, h->cur_branch, norm);
        h->cur_held = YES;
        h->cur_rw   = NO;
        done;
    }
    {
        a_dup(u8c, cur, u8bDataC(h->cur_branch));
        if (u8csEq(cur, norm)) return HOMEOPEN;
    }

    // Different branch on ro — try aux: claim if empty, idempotent on
    // match, refuse otherwise.
    if (u8bEmpty(h->aux_branch)) {
        call(u8bFeed, h->aux_branch, norm);
        done;
    }
    {
        a_dup(u8c, aux, u8bDataC(h->aux_branch));
        if (u8csEq(aux, norm)) return HOMEOPEN;
    }
    return HOMEROBR;
}

ok64 HOMESetCurBranch(home *h, u8cs new_branch) {
    sane(h != NULL && $ok(new_branch));
    a_pad(u8, normbuf, HOME_BRANCH_MAX);
    call(DPATHBranchNormFeed, normbuf, new_branch);
    u8bReset(h->cur_branch);
    call(u8bFeed, h->cur_branch, u8bDataC(normbuf));
    h->cur_held = YES;
    done;
}

ok64 HOMEWriteBranch(home const *h, u8cs out) {
    sane(h != NULL);
    if (!h->cur_rw) return HOMENOBR;
    a_dup(u8c, cur, u8bDataC(h->cur_branch));
    out[0] = cur[0];
    out[1] = cur[1];
    done;
}

b8 HOMEBranchVisible(home const *h, u8cs branch) {
    if (h->cur_held) {
        a_dup(u8c, cur, u8bDataC(h->cur_branch));
        if (DPATHBranchAncestor(branch, cur)) return YES;
    }
    if (!u8bEmpty(h->aux_branch)) {
        a_dup(u8c, aux, u8bDataC(h->aux_branch));
        if (DPATHBranchAncestor(branch, aux)) return YES;
    }
    return NO;
}

ok64 HOMEBranchDir(home *h, path8bp abs_dir, path8bp branch) {
    sane(h != NULL && abs_dir != NULL);
    //  <root>/.be/<project> via the single dog/HOME composer (honors the
    //  *.be-is-store rule, DIS-024).  The branch is purely ref context
    //  (which `?<branch>` tip), never a dir component — every local
    //  consumer (keeper packs/idx, graf idx, refs) resolves to
    //  `<root>/.be/<project>/`.  See Store.mkd.
    a_dup(u8c, proj, u8bDataC(h->project));
    call(HOMEBeDir, h, proj, abs_dir);
    (void)branch;
    done;
}

// --- Workspace finders ---

//  Peek the first line of a wtlog (either a secondary-wt's `.be`
//  file OR a primary's `<wt>/.be/wtlog`) and extract the URI's path
//  bytes into `path_out` (a slice that lives inside `arena`'s busy
//  region).  When `query_out` is non-NULL, the row's QUERY slice is
//  also copied into the same arena (empty slice when the URI carries
//  no `?` component) — this is the sha-bearing row-0 anchor's
//  `?/<title>/<branch>` carrier (replicated.wiki todo/DIS-001).
//
//  Returns OK on success, NODATA when the file is empty or row 0 is
//  not parsable.  Callers must defend against non-anchor row-0
//  shapes — typically by passing the result through
//  DOGProjectFromBe, which returns empty for any URI without a
//  `/.be/` segment.
static ok64 home_peek_repo_uri(path8s be_path, u8bp arena, u8csp path_out,
                               u8csp query_out) {
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
    ok64 fo = PATHu8bAren(arena, path_out, ru_path);
    if (fo == OK && query_out) {
        u8cs ru_query = {rec.uri.query[0], rec.uri.query[1]};
        fo = PATHu8bAren(arena, query_out, ru_query);
    }
    FILEUnMap(map);
    return fo;
}

//  Resolve (project, branch) from a row-0 anchor.  Prefers the QUERY
//  `?/<title>/<branch>` when present (the sha-bearing row-0 shape —
//  see replicated.wiki todo/DIS-001); falls back to the legacy
//  path-after-`.be` encoding (`/.be/<proj>/<branch>/`) when the query
//  is empty (old on-disk stores).  `repo_path` is the row-0 URI PATH,
//  `repo_query` its QUERY (both already copied into the caller's
//  arena).  Fills proj_out / br_out (reset inside); br_out carries the
//  branch path WITHIN the project (empty for the project's trunk).
static void home_anchor_proj_branch(u8cs repo_path, u8cs repo_query,
                                    u8bp proj_out, u8bp br_out) {
    u8bReset(proj_out);
    u8bReset(br_out);
    a_dup(u8c, q, repo_query);
    if (!u8csEmpty(q)) {
        //  New shape: title + branch live in the query as the standard
        //  absolute ref `?/<title>/<branch>`.
        u8cs qproj = {};
        DOGQueryProject(q, qproj);
        if (!u8csEmpty(qproj)) u8bFeed(proj_out, qproj);
        a_dup(u8c, qbr, repo_query);
        DOGQueryStripProject(qbr);          //  qbr → <branch> (project-free)
        if (!u8csEmpty(qbr)) u8bFeed(br_out, qbr);
        return;
    }
    //  Legacy shape: title + branch encoded in the path after `/.be/`.
    //  DOGBranchFromBe returns `<project>[/<branch>]`; strip the leading
    //  project segment so br_out carries the branch within the project.
    DOGProjectFromBe(repo_path, proj_out);
    a_path(br_all_buf);
    DOGBranchFromBe(repo_path, br_all_buf);
    a_dup(u8c, br_all, u8bDataC(br_all_buf));
    a_dup(u8c, proj_strip, u8bDataC(proj_out));
    if (!u8csEmpty(proj_strip) && !u8csEmpty(br_all) &&
        u8csLen(br_all) >= u8csLen(proj_strip)) {
        u8cs head = {br_all[0], br_all[0] + u8csLen(proj_strip)};
        if (u8csEq(head, proj_strip)) {
            br_all[0] += u8csLen(proj_strip);
            if (!u8csEmpty(br_all) && *br_all[0] == '/') u8csUsed1(br_all);
        }
    }
    if (!u8csEmpty(br_all)) u8bFeed(br_out, br_all);
}

//  Given a wt anchor path (where `.be` lives), populate h->wt with
//  the anchor itself and h->root with the store root:
//    * `.be` is a DIR (primary)        → h->root = anchor
//    * `.be` is a regular FILE (sec)   → h->root from row-0 anchor URI (get/repo)
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
        u8cs repo_query = {};
        //  Secondary wt: the `.be` file IS the wtlog; row 0 must be a
        //  valid `repo` anchor naming the shared store (it points
        //  elsewhere than this dir, but is validated the same way as a
        //  primary `.be/wtlog` row 0).  Empty / unparsable row 0 ⇒ no
        //  worktree is anchored here — refuse instead of falling through
        //  to the store==wt fallback (see sniff/test/norepo.sh).
        if (home_peek_repo_uri($path(probe), arena, repo_path, repo_query)
            != OK)
            return NOTAWT;
        {
            a_path(stripped);
            DOGRepoFromBe(repo_path, stripped);
            if (u8bDataLen(stripped) > 0) {
                a_dup(u8c, sb, u8bDataC(stripped));
                call(PATHu8bFeed, h->root, sb);
                //  Resolve project + branch: prefer the row-0 QUERY
                //  `?/<title>/<branch>` (sha-bearing anchor), else the
                //  legacy path-after-`.be` encoding.  Pre-filling
                //  `h->project` / `h->cur_branch` lets downstream openers
                //  route reads/writes through the right keeper leaf even
                //  when no `get`/`post` row yet exists in the secondary
                //  wtlog.  (Without the right branch, keeper would land
                //  the fetched pack one level too deep — see the
                //  subdir-clone scenario in be_sub_shard_setup.)
                a_path(proj_buf);
                a_path(br_buf);
                home_anchor_proj_branch(repo_path, repo_query,
                                        proj_buf, br_buf);
                if (u8bDataLen(proj_buf) > 0) {
                    a_dup(u8c, ps, u8bDataC(proj_buf));
                    call(u8bFeed, h->project, ps);
                }
                if (u8bDataLen(br_buf) > 0) {
                    a_dup(u8c, br_all, u8bDataC(br_buf));
                    u8bReset(h->cur_branch);
                    call(u8bFeed, h->cur_branch, br_all);
                }
                done;
            }
        }
        //  Row 0 parsed but did not name a `/.be/` store ⇒ invalid
        //  secondary anchor, not a worktree.
        return NOTAWT;
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
        u8cs repo_query = {};
        //  A `.be/` *dir* is a repo even with an empty wtlog (it may hold
        //  only config / packs); reading it RO is legitimate.  The
        //  "empty wtlog ⇒ no worktree" refusal lives in the worktree dog
        //  (SNIFFOpen), which is the layer that actually enumerates the
        //  tree.  Here we only opportunistically pull the project shard
        //  (and branch) from row 0 when present — preferring the
        //  sha-bearing anchor's QUERY over the legacy path encoding.
        if (home_peek_repo_uri($path(wtlog_probe), arena2, repo_path,
                               repo_query) == OK) {
            //  Project only here (branch context for a primary comes from
            //  later get/post rows) — stay behavior-preserving while
            //  honoring the new QUERY-carried title.
            a_path(proj_buf);
            a_path(br_buf);
            home_anchor_proj_branch(repo_path, repo_query, proj_buf, br_buf);
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
//  YES iff the directory at `p` has AT MOST ONE subdirectory — a fresh
//  worktree shield (empty `.be/`, or seeded with only `config`/`refs`)
//  OR a single-project store (one `.be/<project>/` shard, DIS-024).
//  Both are valid anchors: discovery should stop here.  A populated
//  MULTI-project store (e.g. a bare `~/.be` with many shards) has >1
//  subdir, so it returns NO and the walk keeps ascending — bare
//  `be`/`sniff` near such a store refuses instead of adopting it as a
//  wt.  See home_walk_up.
//  Shared subdir scanner for the two `.be`-store classifiers below.
//  Counts the immediate subdirectories of `p` (capped at 2 — both
//  callers only distinguish 0 / 1 / >1), skipping dotted entries
//  (".", "..", and the store's own ".be" are never project shards).
//  When `first` is non-NULL it is reset and filled with the FIRST
//  subdir's basename — trustworthy only when the returned count is 1.
//  FILEIter already resolves DT_UNKNOWN via fstatat and skips "."/".."
//  so no manual stat fallback is needed.  Returns 0/1/2 on success;
//  a buffer error from `first` propagates.
static ok64 home_be_subdirs(path8s p, u8b first, int *out_count) {
    sane(out_count != NULL);
    *out_count = 0;
    a_path(dir, p);
    fileit it = {};
    if (FILEIterOpen(&it, dir) != OK) done;   //  dir absent ⇒ zero subdirs

    int subdirs = 0;
    scan(FILENext, &it) {
        if (it.type != DT_DIR) continue;
        //  FILEIter terminates dir paths with a trailing '/'; drop it
        //  before taking the basename so it is the bare shard name.
        a_dup(u8c, full, u8bDataC(it.path));
        if (!$empty(full) && full[1][-1] == '/') u8csShed1(full);
        u8cs base = {};
        PATHu8sBase(base, full);
        //  Dotted basenames (".be" etc.) are never shards.
        if ($empty(base) || $at(base, 0) == '.') continue;
        if (++subdirs == 1 && first != NULL) {
            u8bReset(first);
            if (u8bFeed(first, base) != OK) {  //  real buffer error
                FILEIterClose(&it);
                fail(BNOROOM);
            }
        }
        if (subdirs > 1) break;                //  >1 ⇒ multi-project, stop
    }
    seen(END);
    FILEIterClose(&it);
    *out_count = subdirs;
    done;
}

static b8 home_dir_shieldlike(path8s p) {
    int n = 0;
    if (home_be_subdirs(p, NULL, &n) != OK) return NO;
    return n <= 1;
}

//  If the `.be` dir at `p` has EXACTLY ONE subdirectory (a single
//  project shard), feed its name into `out` and return OK.  Returns
//  NODATA for zero subdirs (flat store) or more than one (ambiguous
//  multi-project store).  This is the store-side project source for a
//  primary wt whose row-0 anchor names no project (DIS-024).
static ok64 home_single_shard(path8s p, u8b out) {
    sane(u8bOK(out));
    int n = 0;
    call(home_be_subdirs, p, out, &n);
    if (n != 1) return NODATA;                 //  flat / ambiguous store
    done;
}

static ok64 home_walk_up(home *h) {
    sane(h != NULL);

    a_path(here);
    test(FILEGetCwd(here) == OK, NOHOME);
    //  GET-010: stop the walk at `$HOME` (taken raw from the env) so
    //  discovery never escapes above the user's home — a hermetic test's
    //  scratch `$HOME` can't reach the dev box's real `~/.be`.  The `/`
    //  stop is the PATHu8bPop below.
    u8cs henv = {};
    FILEGetEnv("HOME", henv);

    for (;;) {
        a_path(probe);
        a_dup(u8c, cur, u8bDataC(here));
        call(PATHu8bFeed, probe, cur);
        call(PATHu8bPush, probe, DOG_BE_S);
        filestat fs = {};
        if (FILEStat(&fs, $path(probe)) == OK) {
            //  A worktree is anchored here iff `.be` is a regular FILE
            //  (secondary-wt wtlog) OR a directory that CONTAINS a
            //  `wtlog` (primary wt).  A `.be/` dir with no `wtlog` is a
            //  bare store (no checked-out worktree here) — keep walking
            //  up so bare `be`/`sniff` near a multi-project store (e.g.
            //  `~/.be`) refuse instead of treating the store as a wt.
            b8 is_wt = (fs.kind == FILE_KIND_REG);
            if (fs.kind == FILE_KIND_DIR) {
                a_path(wtl);
                a_dup(u8c, pbe, u8bDataC(probe));
                (void)PATHu8bFeed(wtl, pbe);
                (void)PATHu8bPush(wtl, DOG_WTLOG_S);
                filestat wfs = {};
                if (FILEStat(&wfs, $path(wtl)) == OK &&
                    wfs.kind == FILE_KIND_REG)
                    is_wt = YES;
                //  A `.be` dir that is a worktree shield / fresh-
                //  bootstrap target (empty or `config`/`refs` only) OR a
                //  single-project store (one `.be/<project>/` shard,
                //  DIS-024) is an anchor: stop here so discovery doesn't
                //  escape to an ancestor store (e.g. the dogfooding dev's
                //  `~/.be`).  A populated MULTI-project store has >1 shard
                //  subdir — keep walking up.
                if (!is_wt && home_dir_shieldlike($path(probe))) is_wt = YES;
            }
            if (is_wt) {
                a_dup(u8c, anchor, u8bDataC(here));
                return home_anchor_resolve(h, anchor);
            }
        }

        //  Stop at `$HOME` — don't ascend above it.  AFTER the `.be` probe
        //  so `$HOME/.be` is still considered an anchor.
        if (!u8csEmpty(henv)) {
            a_dup(u8c, here_now, u8bDataC(here));
            if (u8csEq(here_now, henv)) return NOHOME;
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
