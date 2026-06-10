//  AT — sniff's attribution log, layered over dog/ULOG.
//
#include "AT.h"
#include "SNIFF.h"
#include "PATCH.h"   // PATCH_SCOPE_*

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/RON.h"
#include "dog/DOG.h"
#include "dog/DPATH.h"
#include "dog/WHIFF.h"
#include "keeper/KEEP.h"
#include "keeper/WALK.h"   // WALK_KIND_*

// --- Standalone RO tail peek (no SNIFF singleton, no keeper) -------

ok64 SNIFFAtTailOf(u8cs wt, u8bp out) {
    sane(u8csOK(wt) && out);

    a_path(apath);
    call(SNIFFWtlogPath, apath, wt);

    //  RO open: callable concurrently with sniff's own RW handle.
    //  ULOGOpenRO maps PROT_READ and skips FILEBook's page-align
    //  ftruncate, so it can't trip the silent-EOF-truncation bug
    //  the legacy RW reader caused.
    u8bp    data = NULL;
    wh128bp idx  = NULL;
    ok64 o = ULOGOpenRO(&data, &idx, $path(apath));
    if (o != OK) fail(SNIFFNONE);

    u32 n = ULOGCount(idx);
    if (n == 0) { ULOGClose(data, &idx, NO); fail(SNIFFNONE); }

    //  Row 0 = repo anchor → root path (and, for a sub-shard anchor
    //  whose URI is `file:<root>/.be/<branch>/`, also the active
    //  branch — the secondary wt's `get`/`post` rows often omit the
    //  branch slot, but the anchor URI's `/.be/<branch>/` segment
    //  is the authoritative locator).
    a_pad(u8, root_buf, FILE_PATH_MAX_LEN);
    a_pad(u8, anchor_branch_buf, 256);
    {
        ulogrec r0 = {};
        //  Row 0 is the anchor: verb `get` (current) or `repo` (legacy
        //  stores written before the get-unification) — accept both.
        if (ULOGRow(data, idx, 0, &r0) != OK ||
            (r0.verb != SNIFFAtVerbGet() &&
             r0.verb != SNIFFAtVerbRepo())) {
            ULOGClose(data, &idx, NO); fail(SNIFFNONE);
        }
        DOGRepoFromBe(r0.uri.path, root_buf);
        DOGBranchFromBe(r0.uri.path, anchor_branch_buf);
    }

    //  Walk back for sha — only `get` / `post` rows move cur.
    //  `put path#<sha>` rows carry the staged blob's content sha
    //  (or submodule pointer) in the fragment slot — they record
    //  staged work, never the wt's current commit.  Accepting them
    //  here misreports cur as a blob/foreign-project sha and breaks
    //  every downstream that resolves the wt tip (graf log, head,
    //  …).  Same for `patch` (records absorbed source sha for the
    //  next POST, not cur) and `delete` (path-side).
    ron60 v_get  = SNIFFAtVerbGet();
    ron60 v_post = SNIFFAtVerbPost();
    u8cs ref_body = {}, sha_body = {};
    b8 found = NO;
    b8 sha_row_local = NO;
    u32 sha_row_idx = 0;
    for (u32 i = n; i > 0; ) {
        i--;
        ulogrec rec = {};
        if (ULOGRow(data, idx, i, &rec) != OK) continue;
        if (rec.verb != v_get && rec.verb != v_post) continue;
        uri u = rec.uri;
        //  Row 0 (the wt→store anchor `get`) participates here like any
        //  other get: a bare/tip-less anchor (no #sha) contributes
        //  nothing (sha_body stays empty below); a sha-bearing combined
        //  anchor IS the cur tip.  No positional skip (get-unification).

        //  Canonical at-log shape: `?<branch>#<curhash>` — fragment
        //  carries the sha, query carries the be-branch (empty for
        //  trunk).  Legacy rows kept the sha in a query spec; fall
        //  through and walk the `&`-chain when the fragment is empty.
        u8csMv0(ref_body);
        u8csMv0(sha_body);
        if (DOGIsFullSha(u.fragment)) u8csMv(sha_body, u.fragment);
        b8 row_is_local = u8csEmpty(u.authority);
        a_dup(u8c, q, u.query);
        //  Absolute `?/<project>/<branch>` starts with the project
        //  name (wiki/URI.mkd §"Ref shapes"); strip it so the branch
        //  tail is adopted, never the literal `/project` segment.
        //  No-op on project-relative / legacy `<branch>&<sha>` shapes.
        DOGQueryStripProject(q);
        while (!u8csEmpty(q)) {
            u8cs chunk = {};
            DOGRefDrain(q, chunk);
            if ($empty(chunk)) continue;
            b8 is_sha = DOGIsFullSha(chunk);
            if (!is_sha && u8csEmpty(ref_body) && row_is_local) {
                //  Only adopt the query as the cur branch when the
                //  row is a LOCAL action (no remote authority).
                //  Fetch rows (`get ssh://host?ref#sha`) detach the
                //  wt at the fetched sha but don't move cur — their
                //  query is the REMOTE ref, not the local leaf.
                u8csMv(ref_body, chunk);
            } else if (is_sha && u8csEmpty(sha_body)) {
                u8csMv(sha_body, chunk);
            }
        }
        if (!u8csEmpty(sha_body)) {
            sha_row_local = row_is_local;
            sha_row_idx = i;
            found = YES;
            break;
        }
    }

    //  Sha-row was a fetch — cur stays on whatever LOCAL switch
    //  preceded it.  Walk further back for the latest local row's
    //  query.  No local row found → cur is the project trunk
    //  (ref_body stays empty).
    if (found && !sha_row_local && u8csEmpty(ref_body)) {
        for (u32 i = sha_row_idx; i > 0; ) {
            i--;
            ulogrec rec = {};
            if (ULOGRow(data, idx, i, &rec) != OK) continue;
            if (rec.verb != v_get && rec.verb != v_post) continue;
            uri u = rec.uri;
            if (!u8csEmpty(u.authority)) continue;
            if (u8csLen(u.fragment) != 40) continue;
            //  Strip the absolute project prefix before adopting the
            //  branch (same rule as the first walk above).
            a_dup(u8c, q2, u.query);
            DOGQueryStripProject(q2);
            DOGQueryBranchOnly(q2, ref_body);
            if (!u8csEmpty(ref_body)) break;
        }
    }

    if (!found) { ULOGClose(data, &idx, NO); fail(SNIFFNONE); }

    //  Compose `<wt_root>?/<project>/<branch>#<sha>` into `out`.  The
    //  path slot is the **wt root** (the dir holding `.be`, whether
    //  `.be` is a primary dir or a secondary-wt sentinel file), NOT
    //  the store root: sniff needs the wt root to open `.be`, and any
    //  dog that needs the store root reads row 0 of the wtlog
    //  (`repo file:<store>`).  Keeping a single canonical root in
    //  `--at` fixes the colocated-wt cwd bug where sub-dogs invoked
    //  from a subdir were given the store root and couldn't find
    //  `.be`.  `root_buf` (from `DOGRepoFromBe(r0.uri.path)`) is no
    //  longer used in the URI; `anchor_branch_buf` still feeds the
    //  query slot, since `<project>/<branch>` is encoded inside the
    //  anchor URI's path-after-`.be/`.
    //  https://replicated.wiki/html/wiki/URI.html §"Ref resolution" — absolute form with leading `/` so
    //  the receiver picks the right project shard.
    u8bReset(out);
    u8bFeed(out, wt);
    u8bFeed1(out, '?');
    {
        a_dup(u8c, ab, u8bDataC(anchor_branch_buf));
        if (!u8csEmpty(ab)) {
            //  Split anchor_branch_buf into <project>[/<rest>].
            //  The first segment is the project shard; the rest
            //  (if any) is the anchor-encoded branch (only present
            //  for secondary-wt sub-shard anchors).  Emit absolute
            //  form `?/<project>/<branch>`.  `ref_body` wins over
            //  the anchor's `<rest>` when both are present: the
            //  primary wt's anchor names the project's trunk but
            //  the latest local row is authoritative for the
            //  current branch.
            u8cs proj_s = {ab[0], ab[1]};
            u8cs anc_br = {};
            for (u8cp p = ab[0]; p < ab[1]; p++) {
                if (*p == '/') {
                    proj_s[1] = p;
                    u8cs r = {p + 1, ab[1]};
                    u8csMv(anc_br, r);
                    break;
                }
            }
            u8bFeed1(out, '/');
            u8bFeed(out, proj_s);
            u8cs branch_out = {};
            if (!u8csEmpty(ref_body)) u8csMv(branch_out, ref_body);
            else if (!u8csEmpty(anc_br)) u8csMv(branch_out, anc_br);
            if (!u8csEmpty(branch_out)) {
                u8bFeed1(out, '/');
                u8bFeed(out, branch_out);
            }
        } else if (!u8csEmpty(ref_body)) {
            //  No anchor project — keep legacy relative form.
            u8bFeed(out, ref_body);
        }
    }
    u8bFeed1(out, '#');
    u8bFeed(out, sha_body);

    ULOGClose(data, &idx, NO);
    done;
}

//  Row-0 invariant guard: row 0 is the wt→store anchor (verb `get`,
//  legacy `repo`).  Rows ≥1 may be any verb (`get` checkouts included),
//  so the only rule is that a fresh log opens with the anchor.  Returns
//  OK if the append is allowed.
static ok64 at_check_row0(ron60 verb) {
    sane(SNIFF.h);
    u32 n = ULOGCount(SNIFF.log_idx);
    if (n == 0 && verb != SNIFFAtVerbGet() && verb != SNIFFAtVerbRepo())
        fail(SNIFFFAIL);
    done;
}

//  Populate an anchor URI's QUERY (`/<title>[/<branch>]`) and FRAGMENT
//  (`<hash>`) for the sha-bearing row-0 shape (replicated.wiki
//  todo/DIS-001).  `qbuf` backs the query bytes and must outlive
//  `urow`'s serialization; `hash` is referenced in place (same).  An
//  empty `title` leaves the query unset (bare anchor); an empty `hash`
//  leaves the fragment unset.  `branch` empty = the project's trunk
//  (`?/<title>`).  Mirror reader: `home_anchor_proj_branch` (dog/HOME.c).
ok64 SNIFFAtAnchorRef(uri *urow, u8bp qbuf, u8cs title, u8cs branch,
                      u8cs hash) {
    sane(urow && qbuf);
    if (!u8csEmpty(title)) {
        call(u8bFeed1, qbuf, '/');
        call(u8bFeed,  qbuf, title);
        if (!u8csEmpty(branch)) {
            call(u8bFeed1, qbuf, '/');
            call(u8bFeed,  qbuf, branch);
        }
        a_dup(u8c, qd, u8bData(qbuf));
        urow->query[0] = qd[0];
        urow->query[1] = qd[1];
    }
    if (!u8csEmpty(hash)) {
        urow->fragment[0] = hash[0];
        urow->fragment[1] = hash[1];
    }
    done;
}

ok64 SNIFFWtRepoAnchor(u8cs anchor_path, u8cs repo_path, u8cs title,
                       u8cs branch, u8cs hash) {
    sane($ok(anchor_path) && $ok(repo_path));

    //  Build the row via ULOGu8sFeed — same shape both BE callers
    //  (be_ensure_project_repo, BEGetWorktree) used to assemble by
    //  hand.  scheme `file:`, path = absolute path to the store root's
    //  `.be/` (sha-bearing shape) or the project shard (legacy).
    //  Anchor verb is `get`: the wt→store anchor doubles as the "last
    //  get" baseline (wiki/Title.mkd get-unification).  When (title,
    //  branch, hash) are supplied it carries `?/<title>/<branch>#<hash>`
    //  (the sha-bearing row 0, DIS-001); tip-less callers pass empty and
    //  get the bare `file:<repo_path>` anchor.  Readers accept `get` OR
    //  legacy `repo` (pre-unification on-disk stores).
    a_cstr(file_scheme, "file");
    ulogrec rec = {
        .ts   = RONNow(),
        .verb = SNIFFAtVerbGet(),
    };
    u8csMv(rec.uri.scheme, file_scheme);
    u8csMv(rec.uri.path,   repo_path);

    a_path(qbuf);
    call(SNIFFAtAnchorRef, &rec.uri, qbuf, title, branch, hash);

    a_pad(u8, row, 1024);
    call(ULOGu8sFeed, u8bIdle(row), &rec);

    a_path(out_path);
    a_dup(u8c, ap, anchor_path);
    call(PATHu8bFeed, out_path, ap);
    int fd = FILE_CLOSED;
    call(FILECreate, &fd, $path(out_path));
    a_dup(u8c, body, u8bData(row));
    (void)FILEFeedAll(fd, body);
    FILEClose(&fd);
    done;
}

ok64 SNIFFAtAppend(ron60 verb, uricp u) {
    sane(SNIFF.h && u);
    call(at_check_row0, verb);
    ulogrec rec = {.verb = verb, .uri = *u};
    return ULOGAppend(SNIFF.log_data, SNIFF.log_idx, &rec);
}

ok64 SNIFFAtAppendAt(ron60 ts, ron60 verb, uricp u) {
    sane(SNIFF.h && u);
    call(at_check_row0, verb);
    ulogrec rec = {.ts = ts, .verb = verb, .uri = *u};
    return ULOGAppendAt(SNIFF.log_data, SNIFF.log_idx, &rec);
}

b8 SNIFFAtKnown(ron60 mtime) {
    if (!SNIFF.h) return NO;
    return ULOGHas(SNIFF.log_idx, mtime);
}

void SNIFFAtPathBytes(uri const *u, u8cs out) {
    if (!u8csEmpty(u->path))     { out[0] = u->path[0];     out[1] = u->path[1];     return; }
    if (!u8csEmpty(u->query))    { out[0] = u->query[0];    out[1] = u->query[1];    return; }
    if (!u8csEmpty(u->fragment)) { out[0] = u->fragment[0]; out[1] = u->fragment[1]; return; }
    out[0] = u->data[0]; out[1] = u->data[1];
}

// --- Verb constants (lazy-cached) ---

static ron60 at_v_repo   = 0;
static ron60 at_v_get    = 0;
static ron60 at_v_post   = 0;
static ron60 at_v_patch  = 0;
static ron60 at_v_put    = 0;
static ron60 at_v_delete = 0;
static ron60 at_v_mod    = 0;

ron60 SNIFFAtVerbRepo(void) {
    if (at_v_repo == 0) { a_cstr(s, "repo"); at_v_repo = SNIFFAtVerbOf(s); }
    return at_v_repo;
}

ron60 SNIFFAtVerbGet(void) {
    if (at_v_get == 0) { a_cstr(s, "get"); at_v_get = SNIFFAtVerbOf(s); }
    return at_v_get;
}
ron60 SNIFFAtVerbPost(void) {
    if (at_v_post == 0) { a_cstr(s, "post"); at_v_post = SNIFFAtVerbOf(s); }
    return at_v_post;
}
ron60 SNIFFAtVerbPatch(void) {
    if (at_v_patch == 0) { a_cstr(s, "patch"); at_v_patch = SNIFFAtVerbOf(s); }
    return at_v_patch;
}
ron60 SNIFFAtVerbPut(void) {
    if (at_v_put == 0) { a_cstr(s, "put"); at_v_put = SNIFFAtVerbOf(s); }
    return at_v_put;
}
ron60 SNIFFAtVerbDelete(void) {
    if (at_v_delete == 0) { a_cstr(s, "delete"); at_v_delete = SNIFFAtVerbOf(s); }
    return at_v_delete;
}
ron60 SNIFFAtVerbMod(void) {
    if (at_v_mod == 0) { a_cstr(s, "mod"); at_v_mod = SNIFFAtVerbOf(s); }
    return at_v_mod;
}

// --- Row-0 anchor lookup ---

ok64 SNIFFAtRepo(urip u_out) {
    sane(SNIFF.h && u_out);
    if (ULOGCount(SNIFF.log_idx) == 0) return ULOGNONE;
    ulogrec rec = {};
    call(ULOGRow, SNIFF.log_data, SNIFF.log_idx, 0, &rec);
    //  Anchor verb is `get` (current) or `repo` (legacy stores) —
    //  accept both (the get-unification, wiki/Title.mkd).
    if (rec.verb != SNIFFAtVerbGet() && rec.verb != SNIFFAtVerbRepo())
        fail(SNIFFFAIL);
    *u_out = rec.uri;
    done;
}

// --- Baseline URI lookup ---

ok64 SNIFFAtBaseline(ron60 *ts_out, ron60 *verb_out, urip u_out) {
    sane(SNIFF.h && ts_out && verb_out && u_out);
    ron60 vg = SNIFFAtVerbGet();
    ron60 vp = SNIFFAtVerbPost();
    ron60 vx = SNIFFAtVerbPatch();
    u32 n = ULOGCount(SNIFF.log_idx);
    //  Scan newest→oldest for a get/post/patch row that carries a sha.
    //  The bare store-anchor row (a get/repo row 0 that pins the store
    //  with no #sha) is excluded by the sha check, NOT by position —
    //  so a sha-bearing anchor (a combined `get <store>?<br>#<sha>`
    //  row 0) is itself a valid baseline (the get-unification).
    for (u32 i = n; i > 0; i--) {
        ulogrec rec = {};
        ok64 o = ULOGRow(SNIFF.log_data, SNIFF.log_idx, i - 1, &rec);
        if (o != OK) return o;
        if (rec.verb == vg || rec.verb == vp || rec.verb == vx) {
            //  Skip a bare/tip-less store-anchor row (no branch and no
            //  sha — query AND fragment both empty); a sha-bearing row
            //  (incl. a combined `get <store>?<br>#<sha>` anchor) is a
            //  valid baseline.  Patch rows carry a multi-sha fragment,
            //  so they pass this gate (the get-unification).
            u8cs q = {rec.uri.query[0],    rec.uri.query[1]};
            u8cs f = {rec.uri.fragment[0], rec.uri.fragment[1]};
            if (u8csEmpty(q) && u8csEmpty(f)) continue;
            *ts_out   = rec.ts;
            *verb_out = rec.verb;
            *u_out    = rec.uri;
            done;
        }
    }
    return ULOGNONE;
}

ok64 SNIFFAtCurTip(ron60 *ts_out, ron60 *verb_out, urip u_out) {
    sane(SNIFF.h && ts_out && verb_out && u_out);
    ron60 vg = SNIFFAtVerbGet();
    ron60 vp = SNIFFAtVerbPost();
    u32 n = ULOGCount(SNIFF.log_idx);
    //  Scan newest→oldest for a get/post row carrying a sha; the bare
    //  store-anchor (no #sha) is excluded by the sha check, not by
    //  position (see SNIFFAtBaseline).
    for (u32 i = n; i > 0; i--) {
        ulogrec rec = {};
        ok64 o = ULOGRow(SNIFF.log_data, SNIFF.log_idx, i - 1, &rec);
        if (o != OK) return o;
        if (rec.verb == vg || rec.verb == vp) {
            //  Skip a bare/tip-less store anchor (query AND fragment
            //  both empty); see SNIFFAtBaseline.
            u8cs q = {rec.uri.query[0],    rec.uri.query[1]};
            u8cs f = {rec.uri.fragment[0], rec.uri.fragment[1]};
            if (u8csEmpty(q) && u8csEmpty(f)) continue;
            *ts_out   = rec.ts;
            *verb_out = rec.verb;
            *u_out    = rec.uri;
            done;
        }
    }
    return ULOGNONE;
}

ok64 SNIFFAtBaselineTreeSha(b8 cur_tip_only, sha1 *out, b8 *have_out) {
    sane(out && have_out);
    *have_out = NO;
    ron60 ts = 0, verb = 0;
    uri u = {};
    ok64 br = cur_tip_only
            ? SNIFFAtCurTip  (&ts, &verb, &u)
            : SNIFFAtBaseline(&ts, &verb, &u);
    if (br != OK) return br;   // propagates ULOGNONE + real errors

    //  Row exists but missing/malformed sha or tree-lookup failure
    //  collapses to OK + have_out=NO so callers can treat "no usable
    //  baseline" uniformly.
    sha1hex hex = {};
    if (SNIFFAtQueryFirstSha(&u, &hex) != OK) done;
    sha1 commit_sha = {};
    if (sha1FromSha1hex(&commit_sha, &hex) != OK) done;
    sha1 tree_sha = {};
    if (KEEPCommitTreeSha(&commit_sha, &tree_sha) != OK) done;

    *out = tree_sha;
    *have_out = YES;
    done;
}

ok64 SNIFFAtResolveRelativeURI(uri *u, path8b qbuf, u8b databuf,
                               b8 *was_relative_out) {
    sane(u);
    if (was_relative_out) *was_relative_out = NO;
    if (u->query[0] == NULL || $empty(u->query)) done;

    //  Current branch from sniff baseline.  Empty / missing baseline
    //  = trunk.
    ron60 bts = 0, bverb = 0;
    uri bu = {};
    u8cs current = {};
    if (SNIFFAtBaseline(&bts, &bverb, &bu) == OK)
        u8csMv(current, bu.query);

    b8 was_rel = NO;
    call(DPATHBranchResolveRel, qbuf, current, u->query, &was_rel);
    if (was_relative_out) *was_relative_out = was_rel;
    if (!was_rel) done;

    u8bFeed1(databuf, '?');
    u8bFeed(databuf, $path(qbuf));
    u8csMv(u->query, $path(qbuf));
    u8csMv(u->data,  u8bDataC(databuf));
    done;
}

//  Pick the 40-hex sha out of a patch row's URI into `out`.  URI-001
//  Stage 4b: rows are BARE — the query slot holds the sha alone for
//  squash/merge/rebase-one, the fragment slot for cherry-pick (no
//  `<branch>/` locator prefix to split off).  `out` is left empty when
//  neither slot has 40+ hex.
static void at_patch_row_sha_hex(u8cs out, uricp u) {
    if (u->query[0] != NULL && u8csLen(u->query) >= 40) {
        $mv(out, u->query);
        //  URI-002: a WHOLE-scope row is `?<sha>!` — shed the trailing
        //  `!` modifier (DOG_BANG_QUERY) via the uniform debanger so
        //  only the 40-hex sha is decoded.
        (void)DOGDebangSlice(out);
        return;
    }
    if (u->fragment[0] != NULL && u8csLen(u->fragment) >= 40) {
        $mv(out, u->fragment);
        return;
    }
    out[0] = NULL;
    out[1] = NULL;
}

//  Classify a patch row's URI into a PATCH_SCOPE_* value (DIS-030).
//  Mirrors PATCHShape() in sniff/PATCH.c but reads the stored row:
//
//    `?<sha>`   query, no trailing `!`  → PATCH_SCOPE_NEXT  (one commit)
//    `?<sha>!`  query, trailing `!`     → PATCH_SCOPE_WHOLE (whole branch)
//    `#<sha>`   fragment only           → PATCH_SCOPE_NAMED (one named)
//
//  POST keys provenance on the slot (query = branch-sourced, fragment =
//  named); the WHOLE/NEXT split steers only the message-reuse heuristic.
static u8 at_patch_row_shape(uricp u) {
    b8 has_q = (u->query[0]    != NULL);
    b8 has_f = (u->fragment[0] != NULL);
    if (has_q) {
        //  URI-002: the query-bang (`?<sha>!` = whole-branch) is read
        //  by the uniform debanger, matching PATCHShape() at parse time.
        a_dup(u8c, q, u->query);
        if (DOGDebangSlice(q)) return PATCH_SCOPE_WHOLE;
        return PATCH_SCOPE_NEXT;
    }
    if (has_f && !u8csEmpty(u->fragment)) return PATCH_SCOPE_NAMED;
    return 0;  // BAD
}

//  A `post` row at index `idx` is commit-all iff no `put`/`delete`
//  row lies between its own pd boundary (the most recent `get`/`post`
//  strictly before it) and itself (wiki/POST.mkd §"Boundaries and
//  guards": "A `post` is commit-all iff no `put`/`delete` lies between
//  its pd boundary and itself — one forward scan, no new verb").
//  One backward scan from idx-1: the first `get`/`post` we hit before
//  any `put`/`delete` means commit-all; a `put`/`delete` first means
//  selective.  Verb constants are passed in to avoid re-resolving.
static b8 at_post_is_commit_all(u32 idx, ron60 vg, ron60 vp,
                                ron60 vu, ron60 vd) {
    for (u32 j = idx; j > 0; ) {
        j--;
        ulogrec rec = {};
        if (ULOGRow(SNIFF.log_data, SNIFF.log_idx, j, &rec) != OK) return NO;
        if (rec.verb == vu || rec.verb == vd) return NO;   // selective
        if (rec.verb == vg || rec.verb == vp) return YES;  // pd boundary, none seen
    }
    return YES;  // nothing before it → no put/delete → commit-all
}

//  patch boundary = most recent `get` OR commit-all `post` row
//  (wiki/POST.mkd / wiki/Sniff.mkd §"Boundaries").  `patch` rows after
//  this are in scope for the next POST.  A selective `post` (one with a
//  put/delete in its pd scope) does NOT reset the patch boundary, so
//  earlier `patch` rows keep contributing their foster/parent/picked
//  provenance to the following commit.  Returns the first in-scope row
//  index (0 when no get/commit-all-post anchor exists — whole log).
static u32 at_patch_boundary_start(u32 n, ron60 vg, ron60 vp,
                                   ron60 vu, ron60 vd) {
    for (u32 i = n; i > 0; i--) {
        ulogrec rec = {};
        if (ULOGRow(SNIFF.log_data, SNIFF.log_idx, i - 1, &rec) != OK) return 0;
        if (rec.verb == vg) return i;
        if (rec.verb == vp && at_post_is_commit_all(i - 1, vg, vp, vu, vd))
            return i;
    }
    return 0;
}

ok64 SNIFFAtPatchChain(sha1b out) {
    sane(SNIFF.h && Bok(out));
    ron60 vg = SNIFFAtVerbGet();
    ron60 vp = SNIFFAtVerbPost();
    ron60 vx = SNIFFAtVerbPatch();
    ron60 vu = SNIFFAtVerbPut();
    ron60 vd = SNIFFAtVerbDelete();
    u32 n = ULOGCount(SNIFF.log_idx);
    if (n == 0) return ULOGNONE;

    u32 start = at_patch_boundary_start(n, vg, vp, vu, vd);

    for (u32 i = start; i < n && sha1bHasRoom(out); i++) {
        ulogrec rec = {};
        ok64 o = ULOGRow(SNIFF.log_data, SNIFF.log_idx, i, &rec);
        if (o != OK) return o;
        if (rec.verb != vx) continue;
        u8cs sha_hex = {};
        at_patch_row_sha_hex(sha_hex, &rec.uri);
        if (u8csLen(sha_hex) < 40) continue;
        sha1 s = {};
        a_raw(sb, s);
        a_dup(u8c, hx, sha_hex);
        if (HEXu8sDrainSome(sb, hx) != OK) continue;
        sha1bFeed1(out, s);
    }
    done;
}

ok64 SNIFFAtPatchEntries(sniff_pe *entries, u32 cap, u32 *n_out) {
    sane(SNIFF.h && entries && n_out);
    *n_out = 0;
    ron60 vg = SNIFFAtVerbGet();
    ron60 vp = SNIFFAtVerbPost();
    ron60 vx = SNIFFAtVerbPatch();
    ron60 vu = SNIFFAtVerbPut();
    ron60 vd = SNIFFAtVerbDelete();
    u32 n = ULOGCount(SNIFF.log_idx);
    if (n == 0) return ULOGNONE;

    u32 start = at_patch_boundary_start(n, vg, vp, vu, vd);

    for (u32 i = start; i < n && *n_out < cap; i++) {
        ulogrec rec = {};
        ok64 o = ULOGRow(SNIFF.log_data, SNIFF.log_idx, i, &rec);
        if (o != OK) return o;
        if (rec.verb != vx) continue;
        u8 sh = at_patch_row_shape(&rec.uri);
        if (sh == 0) continue;
        u8cs sha_hex = {};
        at_patch_row_sha_hex(sha_hex, &rec.uri);
        if (u8csLen(sha_hex) < 40) continue;
        sniff_pe *e = &entries[*n_out];
        e->shape = sh;
        e->named = (sh == PATCH_SCOPE_NAMED) ? YES : NO;
        a_raw(sb, e->sha);
        a_dup(u8c, hx, sha_hex);
        if (HEXu8sDrainSome(sb, hx) != OK) continue;
        //  No user merge-msg at PATCH any more (DIS-031); msg reserved.
        e->msg[0] = NULL;
        e->msg[1] = NULL;
        (*n_out)++;
    }
    done;
}

// --- pd-boundary timestamp ---

//  pd boundary = most recent `get` OR `post` row (wiki/POST.mkd
//  §"Boundaries and guards", wiki/Sniff.mkd §"Boundaries in the
//  wtlog").  put/delete rows after this are in scope for the next
//  POST.  A `get` (a hard reset of the world) resets the pd boundary
//  just like a `post` does, so a checkout between a stale `put` and a
//  commit drops that `put` from scope rather than leaking it.
ron60 SNIFFAtLastPostTs(void) {
    if (!SNIFF.h) return 0;
    ron60 vg = SNIFFAtVerbGet();
    ron60 vp = SNIFFAtVerbPost();
    u32 n = ULOGCount(SNIFF.log_idx);
    for (u32 i = n; i > 0; ) {
        i--;
        ulogrec rec = {};
        if (ULOGRow(SNIFF.log_data, SNIFF.log_idx, i, &rec) != OK) return 0;
        if (rec.verb == vg || rec.verb == vp) return rec.ts;
    }
    return 0;
}

// --- Put/delete forward scan since floor ---

// --- ron60 → timespec helper (used to restamp wt files via utimensat) ---

static struct timespec at_ts_of_ron60(ron60 r) {
    struct tm tm = {};
    u32 ms = 0;
    struct timespec ts = {};
    if (RONToTime(r, &tm, &ms) != OK) return ts;
    //  RONNow wrote via localtime; reverse via mktime (local tz).
    //  `tm_isdst = -1` tells mktime to auto-detect DST from the local
    //  calendar; without it mktime assumes tm_isdst=0, which shifts
    //  the computed time_t by one hour during DST, breaking the
    //  ron60 ↔ timespec round-trip at the SNIFFAtKnown check.
    tm.tm_isdst = -1;
    time_t sec = mktime(&tm);
    ts.tv_sec = sec;
    ts.tv_nsec = (long)ms * 1000000L;
    return ts;
}

void SNIFFAtNow(ron60 *ts_out, struct timespec *tv_out) {
    ron60 now = RONNow();
    //  Guard monotonicity against the ULOG tail.
    if (SNIFF.h) {
        ulogrec tail = {};
        if (ULOGTail(SNIFF.log_data, SNIFF.log_idx, &tail) == OK) {
            if (now <= tail.ts) now = tail.ts + 1;
        }
    }
    *ts_out = now;
    *tv_out = at_ts_of_ron60(now);
}

ok64 SNIFFAtRowAtTs(ron60 mtime, ron60 *verb_out, urip u_out) {
    sane(SNIFF.h && verb_out && u_out);
    u32 i = 0;
    ok64 fo = ULOGFind(SNIFF.log_idx, mtime, &i);
    if (fo != OK) return fo;
    ulogrec rec = {};
    call(ULOGRow, SNIFF.log_data, SNIFF.log_idx, i, &rec);
    *verb_out = rec.verb;
    *u_out    = rec.uri;
    done;
}

ok64 SNIFFCheckClock(void) {
    sane(1);
    if (!SNIFF.h) done;                       // no log yet, nothing to compare
    ulogrec tail = {};
    if (ULOGTail(SNIFF.log_data, SNIFF.log_idx, &tail) != OK) done;
    ron60 now = RONNow();
    if (now < tail.ts) {
        //  A burst of N rows in one wall-clock ms self-bumps the tail
        //  N ms ahead (SNIFFAtNow's monotonicity guard).  A single
        //  `be put .` over a several-thousand-file wt (e.g.
        //  rsync-then-stage of an upstream tag) routinely bumps the
        //  tail several seconds ahead — that's not a clock fault, just
        //  the bulk-put serializing rows in millisecond ticks.
        //  CLOCKBAD is for gross errors (NTP step, DST, suspend/resume)
        //  so allow up to 30 s of self-bump headroom before refusing.
        struct timespec tail_tv = at_ts_of_ron60(tail.ts);
        struct timespec now_tv  = at_ts_of_ron60(now);
        i64 skew_ms = ((i64)tail_tv.tv_sec - (i64)now_tv.tv_sec) * 1000
                    + ((i64)tail_tv.tv_nsec - (i64)now_tv.tv_nsec) / 1000000;
        if (skew_ms > 30000) {
            fprintf(stderr,
                    "sniff: clock skew — system clock is before the latest "
                    "wtlog row; refusing every command until clock catches "
                    "up\n");
            return CLOCKBAD;
        }
    }
    done;
}

ok64 SNIFFAtStampPath(path8b path, ron60 ts) {
    sane(path);
    struct timespec tv = at_ts_of_ron60(ts);
    struct timespec times[2] = { tv, tv };
    char const *cp = (char const *)u8bDataHead(path);
    if (utimensat(AT_FDCWD, cp, times, AT_SYMLINK_NOFOLLOW) != 0) fail(SNIFFFAIL);
    done;
}

ok64 SNIFFAtScanPutDelete(ron60 floor, sniff_at_pd_cb cb, void *ctx) {
    sane(SNIFF.h && cb);
    u32 start = 0;
    ok64 s = ULOGSeek(SNIFF.log_idx, floor, &start);
    if (s != OK && s != ULOGNONE) return s;
    u32 n = ULOGCount(SNIFF.log_idx);
    ron60 vput = SNIFFAtVerbPut();
    ron60 vdel = SNIFFAtVerbDelete();
    for (u32 i = start; i < n; i++) {
        ulogrec rec = {};
        ok64 o = ULOGRow(SNIFF.log_data, SNIFF.log_idx, i, &rec);
        if (o != OK) return o;
        if (rec.ts <= floor) continue;
        if (rec.verb != vput && rec.verb != vdel) continue;
        ok64 cr = cb(&rec, ctx);
        if (cr != OK) return cr;
    }
    done;
}

ok64 SNIFFAtQueryFirstSha(uricp u, sha1hex *out) {
    sane(u && out);

    //  Canonical at-log row: `?<branch>#<curhash>` — fragment is the
    //  current sha.  Take it directly when present.
    {
        u8cs frag = {u->fragment[0], u->fragment[1]};
        if (u8csLen(frag) == sizeof(out->data)) {
            sha1hexMv(out, (sha1hex const *)frag[0]);
            done;
        }
    }

    //  Legacy rows kept the sha in the query (`?<branch>&<sha>`) —
    //  walk the `&`-chain and pick the first 40-hex chunk.
    a_dup(u8c, q, u->query);
    while (!$empty(q)) {
        u8cs chunk = {};
        DOGRefDrain(q, chunk);
        if ($len(chunk) == sizeof(out->data) && DOGIsFullSha(chunk)) {
            sha1hexMv(out, (sha1hex const *)chunk[0]);
            done;
        }
    }
    fail(ULOGNONE);
}

// --- SNIFFAtScanDirty -------------------------------------------------

typedef struct {
    u8cs              reporoot;
    sniff_at_dirty_cb cb;
    void             *user_ctx;
    ok64              cb_err;
    //  Newline-separated list of `<path>/` prefixes harvested from
    //  the wt's baseline-tree gitlink (mode-160000) entries.  Files
    //  whose rel-path starts with any of these belong to a submodule
    //  whose internal state sniff doesn't manage; the dirty test is
    //  meaningless for them and they must not appear in the dirty
    //  callback.  Empty when there's no baseline or no gitlinks.
    u8cs              gitlinks;
    //  Sorted ULOG-row stream produced by `KEEPTreeULog` over the
    //  wt's baseline tree.  Each row encodes one tracked path and
    //  its blob sha.  Used for two extra classifications inside the
    //  cb:
    //    * `rel` not in baseline       → untracked; not dirty.
    //    * `rel` in baseline + bytes
    //       hash equal to base blob   → touched-unchanged; not dirty.
    //  Empty when there's no baseline.
    u8cs              base_rows;
} at_dirty_scan_ctx;

//  Scan `base_rows` (KEEPTreeULog-formatted) for a row whose
//  uri.path equals `rel`.  On match, decode the row's 40-hex
//  fragment into `*out` and return YES.  Linear; baselines run
//  from a few tens to a few thousand of entries — fine for the
//  refusal pre-flight.
static b8 at_baseline_blob_sha(u8cs base_rows, u8cs rel, sha1 *out) {
    if (u8csEmpty(base_rows) || u8csEmpty(rel)) return NO;
    a_dup(u8c, scan, base_rows);
    while (!u8csEmpty(scan)) {
        ulogrec rec = {};
        ok64 dr = ULOGu8sDrain(scan, &rec);
        if (dr == NODATA) break;
        if (dr != OK) continue;
        if (!u8csEq(rec.uri.path, rel)) continue;
        if (u8csLen(rec.uri.fragment) != 40) return NO;
        u8s bin = {out->data, out->data + 20};
        a_dup(u8c, hex, rec.uri.fragment);
        if (HEXu8sDrainSome(bin, hex) != OK) return NO;
        return YES;
    }
    return NO;
}

//  Hash the on-disk content of `full` as a git blob.  Returns OK
//  with `*out` filled, or fails (caller treats as "couldn't hash").
//  Hash the on-disk content of `full` (a path8b buffer) as a git
//  blob.  Returns OK with `*out` filled, or fails (caller treats as
//  "couldn't hash").
static ok64 at_hash_wt_blob(sha1 *out, path8bp full,
                            filestat const *fs) {
    sane(out != NULL && full != NULL && fs != NULL);
    a_dup(u8c, full_s, u8bData(full));   // path8s view for FILE APIs
    if (fs->kind == FILE_KIND_LNK) {
        a_pad(u8, tgt, 4096);
        ok64 ro = FILEReadLink(tgt, full_s);
        if (ro != OK) return ro;
        KEEPObjSha(out, DOG_OBJ_BLOB, u8bDataC(tgt));
        done;
    }
    if (fs->kind != FILE_KIND_REG) fail(SNIFFFAIL);
    if (fs->size == 0) {
        u8cs empty = {NULL, NULL};
        KEEPObjSha(out, DOG_OBJ_BLOB, empty);
        done;
    }
    u8bp m = NULL;
    ok64 mo = FILEMapRO(&m, full_s);
    if (mo != OK) return mo;
    u8cs body = {u8bDataHead(m), u8bIdleHead(m)};
    KEEPObjSha(out, DOG_OBJ_BLOB, body);
    FILEUnMap(m);
    done;
}

//  YES iff `rel` starts with any `<path>/` prefix in the gitlinks
//  slice (each entry is `<path>/\n`).
static b8 at_under_gitlink(u8cs gitlinks, u8cs rel) {
    if (u8csEmpty(gitlinks)) return NO;
    a_dup(u8c, scan, gitlinks);
    while (!u8csEmpty(scan)) {
        u8cs entry = {};
        u8csMv(entry, scan);
        a_dup(u8c, find, scan);
        if (u8csFind(find, '\n') != OK) break;
        entry[1] = find[0];
        if (u8csHasPrefix(rel, entry)) return YES;
        u8csUsed1(find);
        u8csMv(scan, find);
    }
    return NO;
}

static ok64 at_dirty_scan_cb(void *varg, path8bp path) {
    sane(varg && path);
    at_dirty_scan_ctx *c = (at_dirty_scan_ctx *)varg;

    a_dup(u8c, full, u8bData(path));
    u8cs rel = {};
    if (!SNIFFRelFromFull(rel, c->reporoot, full)) return OK;
    if (SNIFFSkipMeta(rel))                         return OK;
    if (at_under_gitlink(c->gitlinks, rel))         return OK;

    filestat fs = {};
    ok64 lo = FILELStat(&fs, full);
    if (lo == FILENONE) return OK;    // vanished mid-walk
    if (lo != OK) return lo;             // propagate other errors

    //  Directory hook: any subdir that hosts its own `.git` (file or
    //  directory) is a separate repository — sniff doesn't manage
    //  its contents.  FILESKIP prunes the recursion so none of its
    //  files get checked.  Catches both git-submodule shapes (`.git`
    //  dir with HEAD/, or `.git` file containing `gitdir: ...`).
    //  Also catches beagle's own sub-mount shape (a regular `.be`
    //  *file* at the subdir's root — secondary-wt anchor written by
    //  GET's submodule materialiser, see MODULES.plan.md).
    if (fs.kind == FILE_KIND_DIR) {
        a_path(probe, full, ((u8cs)u8slit(".git")));
        filestat git_fs = {};
        if (FILELStat(&git_fs, $path(probe)) == OK) return FILESKIP;
        a_path(beprobe, full, ((u8cs)u8slit(".be")));
        filestat be_fs = {};
        if (FILELStat(&be_fs, $path(beprobe)) == OK &&
            be_fs.kind == FILE_KIND_REG) return FILESKIP;
        return OK;
    }

    if (SNIFFAtKnown(fs.mtime)) return OK;

    //  Mtime ∉ stamp set.  Two cases that should still be silent:
    //    1. Path is not in the baseline tree (untracked) — sniff
    //       has no opinion about it.
    //    2. Path IS in baseline AND on-disk bytes hash equal to
    //       the baseline blob sha (touched-unchanged / clean drift).
    sha1 base_sha = {};
    b8 in_base = at_baseline_blob_sha(c->base_rows, rel, &base_sha);
    if (!in_base) return OK;

    sha1 wt_sha = {};
    if (at_hash_wt_blob(&wt_sha, path, &fs) == OK &&
        sha1Eq(&wt_sha, &base_sha)) {
        return OK;
    }

    ok64 o = c->cb(rel, c->user_ctx);
    if (o != OK) c->cb_err = o;
    return o;
}

// --- Baseline pre-walk for the dirty scan ----------------------------

//  Best-effort: produce a baseline ULOG via `KEEPTreeULog` (sorted
//  rows, one per leaf path) into `rows_out`, and a newline-separated
//  list of gitlink prefixes (`<path>/\n`) into `gitlinks_out`.
//  Failures (no baseline, no commit, walk error) silently leave both
//  buffers empty — callers see empty inputs and behave as before
//  the fix.
static void at_collect_baseline(u8bp rows_out, u8bp gitlinks_out) {
    u8bReset(rows_out);
    u8bReset(gitlinks_out);
    sha1 tree_sha = {};
    b8 have_tree = NO;
    if (SNIFFAtBaselineTreeSha(NO, &tree_sha, &have_tree) != OK ||
        !have_tree) return;

    //  Verb stem irrelevant here; we read `kind` via `ok64Lit` later.
    a_cstr(stem_name, "base");
    a_dup(u8c, stem_d, stem_name);
    ron60 stem = 0;
    if (RONutf8sDrain(&stem, stem_d) != OK) return;
    if (KEEPTreeULog(tree_sha.data, 0, stem, rows_out) != OK)
        return;

    //  Filter rows for gitlinks (kind == 's').  Append `<path>/\n`
    //  for each into gitlinks_out.
    a_dup(u8c, scan, u8bDataC(rows_out));
    while (!u8csEmpty(scan)) {
        ulogrec rec = {};
        ok64 dr = ULOGu8sDrain(scan, &rec);
        if (dr != OK) break;
        if (ok64Lit(rec.verb, 0) != RON_s) continue;
        if (u8csEmpty(rec.uri.path)) continue;
        (void)u8bFeed (gitlinks_out, rec.uri.path);
        (void)u8bFeed1(gitlinks_out, '/');
        (void)u8bFeed1(gitlinks_out, '\n');
    }
}

ok64 SNIFFAtScanDirty(u8cs reporoot, sniff_at_dirty_cb cb, void *ctx) {
    sane($ok(reporoot) && cb != NULL);

    //  Pre-walk baseline once: produce a ULOG of leaf rows (used to
    //  classify wt entries as in-baseline / out-of-baseline plus to
    //  recover blob shas for the touched-unchanged check) and a
    //  newline-separated list of gitlink prefixes derived from the
    //  same rows.  Both can stay empty (no baseline / first checkout).
    a_carve(u8, base_rows, 1UL << 22);
    a_carve(u8, gitlinks,  1UL << 14);
    at_collect_baseline(base_rows, gitlinks);

    at_dirty_scan_ctx sc = {.cb = cb, .user_ctx = ctx, .cb_err = OK};
    u8csMv(sc.reporoot,  reporoot);
    u8csMv(sc.gitlinks,  u8bDataC(gitlinks));
    u8csMv(sc.base_rows, u8bDataC(base_rows));

    a_path(wp);
    u8bFeed(wp, reporoot);
    call(PATHu8bTerm, wp);
    //  FILE_SCAN_DIRS gets the cb a chance to FILESKIP whole nested
    //  repos via the `.git`-inside heuristic.
    ok64 so = FILEScan(wp,
                       (FILE_SCAN)(FILE_SCAN_FILES | FILE_SCAN_LINKS |
                                   FILE_SCAN_DIRS  | FILE_SCAN_DEEP),
                       at_dirty_scan_cb, &sc);
    if (sc.cb_err != OK) return sc.cb_err;
    return so;
}

// --- SNIFFWtULog: emit wt entries as ULOG rows ----------------------

typedef struct {
    u8cs  reporoot;
    u8bp  out;
    ron60 verb;
    ok64  err;
} at_ulog_ctx;

//  Map a stat-derived kind/mode to the RON64 letter appended to the
//  caller's verb stem (f=regular, x=executable, l=symlink).  No
//  submodule case here — gitlinks live in trees, not the wt scan.
static u8 wt_kind_letter(filestat const *fs) {
    if      (fs->kind == FILE_KIND_LNK) return RON_l;
    else if (fs->mode & 0100)           return RON_x;
    else                                return RON_f;
}

static ok64 at_ulog_cb(void *varg, path8bp path) {
    sane(varg && path);
    at_ulog_ctx *c = (at_ulog_ctx *)varg;

    a_dup(u8c, full, u8bData(path));
    u8cs rel = {};
    if (!SNIFFRelFromFull(rel, c->reporoot, full)) return OK;
    if (SNIFFSkipMeta(rel))                         return OK;

    filestat fs = {};
    ok64 lo = FILELStat(&fs, full);
    if (lo == FILENONE) return OK;    // vanished mid-walk
    if (lo != OK) return lo;             // propagate other errors

    uri u = {};
    u8csMv(u.path, rel);
    //  query empty (mode encoded in verb), fragment empty (no sha yet).

    ulogrec rec = {.ts   = fs.mtime,
                   .verb = ok64sub(c->verb, wt_kind_letter(&fs)),
                   .uri  = u};
    ok64 o = ULOGu8sFeed(u8bIdle(c->out), &rec);
    if (o != OK) { c->err = o; return o; }
    return OK;
}

ok64 SNIFFWtULog(u8cs reporoot, ron60 verb, u8bp out) {
    sane($ok(reporoot) && out);
    u8bReset(out);
    at_ulog_ctx c = {.out = out, .verb = verb, .err = OK};
    u8csMv(c.reporoot, reporoot);

    a_path(wp);
    u8bFeed(wp, reporoot);
    call(PATHu8bTerm, wp);

    //  FILEScanSorted sorts each visited dir's entries in scratch
    //  space; ignored dirs (Corpus/, .git/objects/, build/) can hold
    //  tens of thousands of entries even though no row will be
    //  emitted for them.  Use a 16 MB mmap region — VA only, paged
    //  on demand — to comfortably handle the worst real-world dir.
    a_carve(u8, scratch, 1UL << 24);

    ok64 so = FILEScanSorted(wp,
                             (FILE_SCAN)(FILE_SCAN_FILES | FILE_SCAN_LINKS |
                                         FILE_SCAN_DEEP),
                             scratch, FILEentryZ, at_ulog_cb, &c);
    if (c.err != OK) return c.err;
    return so;
}
