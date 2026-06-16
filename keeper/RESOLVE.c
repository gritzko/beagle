//  RESOLVE: one-step canonicalisation of user-typed refs into commit
//  shas.  See keeper/RESOLVE.h for the contract.
//
//  Lives in keeper/ because every token shape we recognise needs the
//  pack registry (KEEPGet, KEEPGetExact) or the REFS log (REFSResolve)
//  — both keeper-owned.  graf/sniff callers go through this funnel
//  exactly once per command and then operate on canonical forms.

#include "RESOLVE.h"

#include <string.h>

#include "abc/B.h"
#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/URI.h"
#include "dog/DOG.h"
#include "dog/DPATH.h"
#include "dog/HOME.h"

#include "dog/git/GIT.h"
#include "KEEP.h"
#include "REFS.h"

#define RESOLVE_OBJ_BUF       (1UL << 16)

//  Decode a 40-hex sha into `out` and verify the object exists in
//  keeper.  Returns OK on success, RESLVNONE if not found.
static ok64 resolve_sha40(keeper *k, sha1 *out, u8cs hex) {
    sane(k && out);
    (void)k;
    call(sha1FromHex, out, hex);
    a_carve(u8, cbuf, RESOLVE_OBJ_BUF);
    u8 ct = 0;
    ok64 ko = KEEPGetExact(out, cbuf, &ct);
    if (ko != OK) return RESLVNONE;
    return OK;
}

//  Expand a 4..39-hex hashlet via the keeper pack index, then rebuild
//  the full 20-byte sha from the resolved object body.  Returns OK on
//  success, RESLVNONE on miss.
static ok64 resolve_hashlet(keeper *k, sha1 *out, u8cs hex) {
    sane(k && out);
    (void)k;
    u64 h60 = WHIFFHexHashlet60(hex);
    size_t hexlen = u8csLen(hex);
    if (hexlen > 15) hexlen = 15;
    a_carve(u8, cbuf, RESOLVE_OBJ_BUF);
    u8 ct = 0;
    ok64 ko = KEEPGet(h60, hexlen, cbuf, &ct);
    if (ko == OK) {
        a_dup(u8c, body, u8bDataC(cbuf));
        KEEPObjSha(out, ct, body);
    }
    return ko == OK ? OK : RESLVNONE;
}

//  REFSResolve `?<path>` and decode the resolved 40-hex into `out`.
//  Retries the usual prefix strips (`refs/heads/`, `refs/`, `heads/`)
//  so the caller can pass any of the legal absolute spellings.
static ok64 resolve_branch_path(sha1 *out, u8cs path) {
    sane(out);
    if (BNULL(HOME.root) || u8bDataLen(HOME.root) == 0) return RESLVFAIL;

    a_path(keepdir);
    call(HOMEBranchDir, keepdir, NULL);

    //  Empty path is the canonical trunk lookup.  REFSResolve treats
    //  a URI with `?` and no body as "match trunk only" (presence test,
    //  not emptiness — see keeper/REFS.c line 440 comment).  We pass
    //  the literal `?` string to get exactly that shape; the strip
    //  retries don't apply (no prefix to peel).
    if ($empty(path)) {
        a_cstr(trunk_uri, "?");
        a_pad(u8, arena, 512);
        uri resolved = {};
        ok64 ro = REFSResolve(&resolved, arena,
                              $path(keepdir), trunk_uri);
        if (ro != OK || u8csLen(resolved.query) < 40) return RESLVNONE;
        call(sha1FromHex, out, resolved.query);
        done;
    }

    //  Each branch's own keeper shard carries its own REFS reflog —
    //  `keeper get //host/path?<branch>` writes the peer-form row into
    //  `.be/[<project>/]<branch>/refs`, NOT the trunk REFS, so the
    //  parent's reflog stays clean for sub-shard isolation (see
    //  wcli_record_ref).  We look in two places, longest match wins:
    //   1. Trunk REFS — local POSTs, sniff get rows, anything written
    //      while cur_branch was trunk.
    //   2. `.be/[<project>/]<path>/refs` — the leaf shard the branch
    //      itself owns; peer-form rows from `keeper get //host?<path>`
    //      land here.
    //  Per-location: try each refname-strip variant.  Per-attempt:
    //   pass 0 — local-only (no authority key).
    //   pass 1 — `//.` authority trips REFSResolve's auth_is_dot so
    //            peer-form rows (`//<host>/<repo>?<br>#<sha>`) match.
    //  See TRIANGLE.todo.md gap #6 for the spec rationale.
    char const *strips[] = {"", "refs/heads/", "refs/", "heads/", NULL};
    a_cstr(dot_auth, "//.");
    for (u32 loc = 0; loc < 2; loc++) {
        a_path(lookup_dir);
        if (loc == 0) {
            call(PATHu8bFeed, lookup_dir, $path(keepdir));
        } else {
            call(HOMEBranchDir, lookup_dir, NULL);
            call(PATHu8bAdd, lookup_dir, path);
        }
        for (u32 pass = 0; pass < 2; pass++) {
            for (u32 si = 0; strips[si] != NULL; si++) {
                a_dup(u8c, p, path);
                a_cstr(strip, strips[si]);
                if (!u8csEmpty(strip)) {
                    if (u8csLen(p) <= u8csLen(strip)) continue;
                    if (!u8csHasPrefix(p, strip)) continue;
                    u8csUsed(p, u8csLen(strip));
                }
                a_pad(u8, arena, 512);
                uri resolved = {};
                ok64 ro;
                if (pass == 0) {
                    a_uri(qkey, 0, 0, 0, p, 0);
                    ro = REFSResolve(&resolved, arena, $path(lookup_dir),
                                     qkey);
                } else {
                    a_uri(qkey, 0, dot_auth, 0, p, 0);
                    ro = REFSResolve(&resolved, arena, $path(lookup_dir),
                                     qkey);
                }
                if (ro == OK && u8csLen(resolved.query) >= 40) {
                    call(sha1FromHex, out, resolved.query);
                    return OK;
                }
            }
        }
    }
    return RESLVNONE;
}

//  POSTPONED — commit-message substring search.  Walks the pack
//  index for COMMIT-typed entries, fetches each commit body via
//  `KEEPGet`, and returns the first whose message contains `needle`
//  as a substring.  Bounded by RESOLVE_MSG_MAX_WALK so a pathological
//  pack with millions of commits can't lock up `be`.
//
//  Lifted from graf/LOG.c::graf_head_msg_search but reshaped to:
//   1. live in keeper/, so resolution doesn't depend on graf state;
//   2. scan keeper's currently-open pack logs directly (no DAG walk)
//      so it works on a fresh fetch before any graf indexing has run;
//   3. return the matched sha to the caller rather than render to bro.
//
//  Not wired into `KEEPResolveRef` yet — kept around so the search
//  semantic is in the right module when we turn it on.  Activation
//  is a one-line dispatch in KEEPResolveRef's last arm.
#define RESOLVE_MSG_MAX_WALK 4096

static ok64 keep_msg_search(keeper *k, u8cs needle, sha1 *out)
    __attribute__((unused));

static ok64 keep_msg_search(keeper *k, u8cs needle, sha1 *out) {
    sane(k && out);
    if (u8csEmpty(needle)) return RESLVNONE;

    Bu8 cbuf = {};
    call(u8bAllocate, cbuf, RESOLVE_OBJ_BUF);

    u32 scanned = 0;

    //  Walk pack runs newest-first (highest seqno first) so the most
    //  recent commit matching the needle wins.  Inside each run, walk
    //  COMMIT-typed entries.
    u32 nruns = keep_run_count_all(k);

    for (u32 ri = nruns; ri > 0; ri--) {
        wh128cs run = {NULL, NULL};
        keep_run_at_all(run, k, ri - 1);
        if (run[0] == NULL) continue;
        size_t rlen = wh128csLen(run);

        for (size_t ei = rlen; ei > 0; ei--) {
            wh128 const *e = wh128csAtP(run, ei - 1);
            u8 type = wh64Type(e->key);
            if (type != KEEP_OBJ_COMMIT) continue;
            if (scanned++ >= RESOLVE_MSG_MAX_WALK) {
                u8bFree(cbuf);
                return RESLVNONE;
            }
            u64 h60 = e->key >> 4;
            u8bReset(cbuf);
            u8 ct = 0;
            ok64 ko = KEEPGet(h60, 15, cbuf, &ct);
            if (ko != OK || ct != KEEP_OBJ_COMMIT) continue;

            //  Skip headers; grab the message body (everything after
            //  the empty field that terminates the header block).
            u8cs message = {NULL, NULL};
            {
                a_dup(u8c, scan, u8bDataC(cbuf));
                u8cs field = {}, value = {};
                while (GITu8sDrainCommit(scan, field, value) == OK) {
                    if (u8csEmpty(field)) { $mv(message, value); break; }
                }
            }
            if (u8csEmpty(message)) continue;
            if (u8csFindS(message, needle) != OK) continue;

            a_dup(u8c, body, u8bDataC(cbuf));
            KEEPObjSha(out, ct, body);
            u8bFree(cbuf);
            return OK;
        }
    }
    u8bFree(cbuf);
    return RESLVNONE;
}

ok64 KEEPResolveRef(sha1 *out, u8cs token, u8cs cur_branch) {
    sane(out && $ok(token));
    keeper *k = &KEEP;

    //  Empty token is a legitimate "trunk" lookup — used by PATCH /
    //  POST when `?..` from a top-level branch absolutises to "".
    //  Down to resolve_branch_path which handles the empty-path arm.
    if ($empty(token)) return resolve_branch_path(out, token);

    //  Strip a leading `?` so the caller can pass either `?feat` or
    //  `feat` interchangeably.  Bare `?` (length 1) means trunk.
    a_dup(u8c, t, token);
    if (t[0][0] == '?') u8csUsed1(t);

    //  Empty after strip → trunk.  REFS key for trunk is bare `?` —
    //  resolve_branch_path handles it via the empty-path arm.
    if ($empty(t)) {
        return resolve_branch_path(out, t);
    }

    //  Branch path, possibly relative.  Queries are path-shaped:
    //  `./X`, `../X`, `..` resolve against `cur_branch` via PATH
    //  primitives (Pop/Push) — branch semantics: popping past
    //  trunk yields trunk (empty), not "..".  Everything else
    //  passes through verbatim (absolute / project-relative / a bare
    //  hex token; the branch resolver disambiguates).
    a_path(abs_path);
    path8s ref_in = {t[0], t[1]};
    if (!$empty(ref_in) && ref_in[0][0] == '.') {
        if (!$empty(cur_branch)) call(PATHu8bFeed, abs_path, cur_branch);
        path8s rel = {ref_in[0], ref_in[1]};
        if ($len(rel) >= 2 && rel[0][0] == '.' && rel[0][1] == '/') {
            u8csUsed(rel, 2);
        } else if ($len(rel) >= 3 && rel[0][0] == '.' &&
                   rel[0][1] == '.' && rel[0][2] == '/') {
            call(PATHu8bPop, abs_path);
            u8csUsed(rel, 3);
        } else if ($len(rel) == 2 && rel[0][0] == '.' && rel[0][1] == '.') {
            call(PATHu8bPop, abs_path);
            u8csUsed(rel, 2);
        }
        if (!$empty(rel)) call(PATHu8bPush, abs_path, rel);
    } else if (!$empty(ref_in)) {
        call(PATHu8bFeed, abs_path, ref_in);
    }

    //  URI-001 §"The one rule": disambiguation is BRANCH-FIRST and
    //  hash-length-agnostic.
    //   1. Does the ref name a branch (REFS lookup)?  → branch / trunk.
    ok64 bo = resolve_branch_path(out, $path(abs_path));
    if (bo == OK) return OK;

    //   2. Else, is the ref all-hex AND does that hash exist (pack
    //      index)?  → hashlet / detached.  The index knows sha1 (40)
    //      vs sha256 (64); a `.`-prefixed (relative) token never gets
    //      here — it has no hex spelling.
    if (HEXu8sValid(t)) {
        size_t len = u8csLen(t);
        if (len == 40) return resolve_sha40(k, out, t);
        if (len >= 6)  return resolve_hashlet(k, out, t);
    }

    //   3. Else → not found: surface the branch lookup's outcome
    //      (RESLVNONE on a clean miss, RESLVFAIL on no-home).
    return bo;
}

ok64 KEEPResolveHex(sha1hex *out, u8cs token) {
    sane(out && $ok(token));
    sha1 sh = {};
    u8cs cur_branch = {NULL, NULL};
    call(KEEPResolveRef, &sh, token, cur_branch);
    sha1hexFromSha1(out, &sh);
    done;
}

//  --- DIS-025 Stage 2: text-in / text-out canonicalising funnel ---

//  Branch-only absolute path (project stripped) for a non-hex ref.
//  Empty result == trunk (relative `..` popped past root, or an
//  absolute `/<proj>` with no branch).  Relative refs (`./x`, `../x`,
//  `..`) resolve against `cur_leaf`; absolute refs get their project
//  stripped; bare project-relative refs pass through (trailing '/'
//  trimmed).  Mirrors beagle/BE.cli.c::be_abs_branch (keeper may not
//  depend on beagle, so the logic is duplicated here).
static ok64 resolve_abs_branch(u8b out, u8cs cur_leaf, u8cs query) {
    sane($ok(out));
    u8bReset(out);
    if (u8csEmpty(query)) done;
    a_dup(u8c, q, query);
    if (*q[0] == '.') {                          //  relative
        b8 rel = NO;
        call(DPATHBranchResolveRel, out, cur_leaf, query, &rel);
        done;
    }
    if (*q[0] == '/') {                          //  absolute → strip project
        a_dup(u8c, br, q);
        DOGQueryStripProject(br);
        if (!u8csEmpty(br)) call(u8bFeed, out, br);
        done;
    }
    a_dup(u8c, body, q);                         //  bare project-relative
    if (!u8csEmpty(body) && *u8csLast(body) == '/') u8csShed1(body);
    call(u8bFeed, out, body);
    done;
}

ok64 REFSResolveURI(u8s abs_ref, u8cs rel_ref) {
    sane($ok(abs_ref) && $ok(rel_ref));

    //  Idempotent: an already-canonical input is re-emitted verbatim
    //  (with a single leading `?`).  This makes the funnel a no-op on
    //  text it has already produced — the common case when a caller
    //  re-resolves a URI a prior `be` invocation canonicalised.
    if (REFSQueryKind(rel_ref) != REFKIND_NONE) {
        a_dup(u8c, body, rel_ref);
        if (!u8csEmpty(body) && *body[0] == '?') u8csUsed1(body);
        call(URIMake, abs_ref, NULL, NULL, NULL, body, NULL);
        done;
    }

    //  Strip a leading `?` from the raw input.
    a_dup(u8c, in, rel_ref);
    if (!u8csEmpty(in) && *in[0] == '?') u8csUsed1(in);

    //  cur's branch leaf (project-stripped) from home.  HOME stores the
    //  canonical trailing-'/' form (`feat/`), but the relative resolver
    //  (DPATHBranchResolveRel → PATHu8bPop) expects the slash-less shape
    //  the wtlog-derived callers feed it — a trailing slash makes `..`
    //  pop the empty tail segment instead of the branch.  Trim it.
    u8cs cur_leaf = {};
    u8csMv(cur_leaf, u8bDataC(HOME.cur_branch));
    while (!u8csEmpty(cur_leaf) && *u8csLast(cur_leaf) == '/')
        u8csShed1(cur_leaf);

    //  Project segment: an absolute input carries its own; otherwise
    //  take cur's project from home.  No project anywhere → can't
    //  compose a canonical form.
    u8cs project = {};
    if (!u8csEmpty(in) && *in[0] == '/') DOGQueryProject(in, project);
    if (u8csEmpty(project)) u8csMv(project, u8bDataC(HOME.project));
    if (u8csEmpty(project)) fail(RESLVFAIL);

    //  URI-001 §"The one rule": classify by RESOLUTION, branch-first —
    //  never by hex syntax.  Compute the branch-only path, try it as a
    //  branch (REFS lookup); a hit is BRANCH (or TRUNK when the path is
    //  empty).  Only on a branch miss AND an all-hex ref naming an
    //  existing object is the target DETACHED.  This is what lets a
    //  branch whose NAME is all-hex (`?c0ffee`) win over a same-spelled
    //  hashlet.  Empty-but-non-NULL trunk token: KEEPResolveRef's
    //  `sane($ok(token))` rejects a {NULL,NULL} slice, so spell trunk as
    //  a zero-length slice over a real address.
    keeper *k = &KEEP;
    a_path(branchb);
    refkind kind;
    sha1 pin = {};
    if (u8csEmpty(in)) {
        a_cstr(trunk, "");
        call(KEEPResolveRef, &pin, trunk, cur_leaf);
        kind = REFKIND_TRUNK;
    } else {
        call(resolve_abs_branch, branchb, cur_leaf, in);
        ok64 bo;
        if (u8bHasData(branchb)) {
            bo = resolve_branch_path(&pin, u8bDataC(branchb));
            kind = REFKIND_BRANCH;
        } else {
            a_cstr(trunk, "");
            bo = KEEPResolveRef(&pin, trunk, cur_leaf);
            kind = REFKIND_TRUNK;
        }
        if (bo != OK) {
            //  Branch miss → detached only if the raw ref is an all-hex
            //  object that exists; otherwise surface the branch outcome.
            if (!HEXu8sValid(in)) return bo;
            call(KEEPResolveRef, &pin, in, cur_leaf);
            kind = REFKIND_DETACHED;
        }
    }
    a_sha1hex(pinslice, &pin);

    //  Compose the canonical resolved SCOPE (URI-001 §"Canonical form").
    //  This funnel canonicalises the QUERY only — it NEVER pins the tip
    //  into a fragment: in argv URIs the fragment is verb payload
    //  (PATCH squash/merge/rebase mode, GET `#~N`/`#sha`), so a tip pin
    //  there would collide.  The sha pin lives in `--at` (wtlog) and
    //  REFS storage.  DETACHED carries the sha IN the query — it is the
    //  ref's identity, there is no branch to scope by:
    //    TRUNK     query=/<project>           → ?/proj
    //    BRANCH    query=/<project>/<branch>  → ?/proj/branch
    //    DETACHED  query=/<project>/<full-sha>→ ?/proj/<sha>
    a_pad(u8, qbody, 320);
    call(u8bFeed1, qbody, '/');
    call(u8bFeed, qbody, project);
    if (kind == REFKIND_BRANCH) {
        call(u8bFeed1, qbody, '/');
        call(u8bFeed, qbody, u8bDataC(branchb));
    } else if (kind == REFKIND_DETACHED) {
        call(u8bFeed1, qbody, '/');
        call(u8bFeed, qbody, pinslice);
    }
    call(URIMake, abs_ref, NULL, NULL, NULL, u8bDataC(qbody), NULL);
    done;
}

ok64 KEEPResolveURI(u8s abs_uri, u8cs rel_uri) {
    sane($ok(abs_uri) && $ok(rel_uri));

    uri u = {};
    u.data[0] = rel_uri[0];
    u.data[1] = rel_uri[1];
    call(URILexer, &u);

    //  No query slot → nothing for the REF arm to canonicalise; emit
    //  the URI unchanged (path / authority arms are Stage-3 TODOs).
    if (u.query[0] == NULL) {
        u8cs sc = {u.scheme[0], u.scheme[1]};
        u8cs au = {u.authority[0], u.authority[1]};
        u8cs pa = {u.path[0], u.path[1]};
        u8cs fr = {u.fragment[0], u.fragment[1]};
        call(URIMake, abs_uri, sc, au, pa, NULL, fr);
        done;
    }

    //  REF arm: canonicalise the QUERY (scope) into a scratch, then strip
    //  its leading `?` (URIMake re-adds it).  The funnel canonicalises the
    //  scope only — it never pins a tip into a fragment — so the input
    //  fragment (verb payload: PATCH mode, GET `#~N`, …) is preserved
    //  VERBATIM on the recompose.
    u8 _qpad[320];
    u8s qw = {_qpad, _qpad + sizeof(_qpad)};
    u8cs qin = {u.query[0], u.query[1]};
    call(REFSResolveURI, qw, qin);
    u8cs qbody = {_qpad, qw[0]};
    if (!u8csEmpty(qbody) && *qbody[0] == '?') u8csUsed1(qbody);

    //  Recompose, preserving authority / path / fragment verbatim (path
    //  arm = cwd→root, auth arm = //alias→URL are Stage-3 TODOs).
    u8cs sc = {u.scheme[0], u.scheme[1]};
    u8cs au = {u.authority[0], u.authority[1]};
    u8cs pa = {u.path[0], u.path[1]};
    u8cs fr = {u.fragment[0], u.fragment[1]};
    call(URIMake, abs_uri, sc, au, pa, qbody, fr);
    done;
}
