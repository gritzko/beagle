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
    u8s sb = {out->data, out->data + 20};
    call(HEXu8sDrainSome, sb, hex);
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
static ok64 resolve_branch_path(keeper *k, sha1 *out, u8cs path) {
    sane(k && out);
    if (k->h == NULL || u8bDataLen(k->h->root) == 0) return RESLVFAIL;

    a_path(keepdir);
    call(HOMEBranchDir, k->h, keepdir, NULL);

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
        u8s sb = {out->data, out->data + 20};
        u8cs hx = {resolved.query[0], resolved.query[0] + 40};
        call(HEXu8sDrainSome, sb, hx);
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
            call(HOMEBranchDir, k->h, lookup_dir, NULL);
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
                    u8s sb = {out->data, out->data + 20};
                    u8cs hx = {resolved.query[0], resolved.query[0] + 40};
                    call(HEXu8sDrainSome, sb, hx);
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

    u32 nruns   = (u32)(0);
    nruns       = (u32)(0);  //  silence-pedantic in -Werror builds
    u32 scanned = 0;

    //  Walk pack runs newest-first (highest seqno first) so the most
    //  recent commit matching the needle wins.  Inside each run, walk
    //  COMMIT-typed entries.
    extern u32 keep_run_count_all(keeper const *k);
    extern void keep_run_at_all(wh128csp out, keeper const *k, u32 i);
    nruns = keep_run_count_all(k);

    for (u32 ri = nruns; ri > 0; ri--) {
        wh128cs run = {NULL, NULL};
        keep_run_at_all(run, k, ri - 1);
        if (run[0] == NULL) continue;
        wh128cp base = run[0];
        size_t  rlen = (size_t)(run[1] - run[0]);

        for (size_t ei = rlen; ei > 0; ei--) {
            wh128 const *e = &base[ei - 1];
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
    if ($empty(token)) return resolve_branch_path(k, out, token);

    //  Strip a leading `?` so the caller can pass either `?feat` or
    //  `feat` interchangeably.  Bare `?` (length 1) means trunk.
    a_dup(u8c, t, token);
    if (t[0][0] == '?') u8csUsed1(t);

    //  Empty after strip → trunk.  REFS key for trunk is bare `?` —
    //  resolve_branch_path handles it via the empty-path arm.
    if ($empty(t)) {
        return resolve_branch_path(k, out, t);
    }

    //  Hex token: classify by length.  4..39 hex → hashlet expand;
    //  exactly 40 → verify-exists.
    if (HEXu8sValid(t)) {
        size_t len = u8csLen(t);
        if (len == 40) return resolve_sha40(k, out, t);
        if (len >= 6) return resolve_hashlet(k, out, t);
        //  Short alpha-only-looking shape (3-char) might also be a
        //  legal branch name — fall through to branch resolution.
    }

    //  Branch path, possibly relative.  Queries are path-shaped:
    //  `./X`, `../X`, `..` resolve against `cur_branch` via PATH
    //  primitives (Pop/Push) — branch semantics: popping past
    //  trunk yields trunk (empty), not "..".  Everything else
    //  passes through verbatim (absolute / project-relative; the
    //  branch resolver disambiguates).
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
    return resolve_branch_path(k, out, $path(abs_path));
}

ok64 KEEPResolveHex(sha1hex *out, u8cs token) {
    sane(out && $ok(token));
    sha1 sh = {};
    u8cs cur_branch = {NULL, NULL};
    call(KEEPResolveRef, &sh, token, cur_branch);
    sha1hexFromSha1(out, &sh);
    done;
}
