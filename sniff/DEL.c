//  DEL: actually unlink a file (after a dirty-safety check) and
//  append a `delete <path>` row to the ULOG.  The unlink happens
//  immediately, not at POST time — the ULOG row records that the
//  delete happened, and the next POST drops the path from the new
//  tree.
//
//  Dirty-safety: refuse DELDIRTY if `mtime ∉ stamp-set`
//  (file was user-edited since any owning row).  v1 doesn't run
//  the content-equality fallback the spec calls for on mtime drift
//  — TODO once we expose a baseline-tree path → sha lookup.
//
//  Already-absent paths are an OK no-op: append the delete row,
//  no error.  POST drops the path from the new tree as if it had
//  been there.
//
//  Branch-form delete (`?<branch>`) lives in DELBranch — a
//  separate path that writes a REFS tombstone, unrelated to the
//  worktree.
//
#include "DEL.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "abc/B.h"
#include "abc/BUF.h"
#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/OK.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "dog/git/GIT.h"
#include "keeper/KEEP.h"
#include "keeper/REFS.h"
#include "keeper/WALK.h"

#include "AT.h"
#include "CLASS.h"

// --- Per-URI baseline-membership classifier -------------------------
//
//  When a `be delete <path>` target is already absent on disk we
//  need to know whether the baseline tree had it: tracked → emit
//  the delete row so POST drops the path from the next commit;
//  untracked → silent no-op (no row, nothing to drop).
//
//  Mirrors PUT's per-URI classifier: SNIFFClassify yields one step
//  per distinct path; we set in_baseline on the matching req.  The
//  reqs array is parallel to the user's uris so the main loop reads
//  reqs[i] without a secondary lookup.  Empty raw → file-form slot
//  unused (dir-form / skipped URI).

typedef struct {
    u8cs raw;            // bytes from user URI (file-form only)
    b8   in_baseline;    // CLASS_BOTH or CLASS_BASE_ONLY
} del_req;

typedef struct {
    del_req *reqs;
    u32      n;
} del_ctx;

static ok64 del_classify_step(class_step const *step, void *ctx_) {
    del_ctx *w = (del_ctx *)ctx_;
    if (step->kind != CLASS_BOTH && step->kind != CLASS_BASE_ONLY)
        return OK;
    for (u32 j = 0; j < w->n; j++) {
        if (!$ok(w->reqs[j].raw)) continue;
        if ($len(w->reqs[j].raw) != $len(step->path)) continue;
        if (memcmp(w->reqs[j].raw[0], step->path[0],
                   (size_t)$len(step->path)) != 0) continue;
        w->reqs[j].in_baseline = YES;
    }
    return OK;
}

// --- Dir-form recursive delete --------------------------------------

//  Two-pass walker:
//   * pass 1 (preflight)  — refuse DELDIRTY on the first
//                           descendant whose mtime ∉ stamp-set.
//   * pass 2 (apply)      — unlink each descendant.
//  Both passes use the same callback driven by a `mode` flag.

typedef enum { DEL_DIR_PREFLIGHT, DEL_DIR_APPLY } del_dir_mode;

typedef struct {
    u8cs          reporoot;
    del_dir_mode  mode;
    u32           dirty;     // count, only filled by preflight
    u32           unlinked;  // count, only filled by apply
} del_dir_ctx;

static ok64 del_dir_cb(void *vctx, path8bp path) {
    sane(vctx && path);
    del_dir_ctx *c = (del_dir_ctx *)vctx;

    a_dup(u8c, full, u8bData(path));
    u8cs rel = {};
    if (!SNIFFRelFromFull(rel, c->reporoot, full)) return OK;
    if (SNIFFSkipMeta(rel))                         return OK;

    if (c->mode == DEL_DIR_PREFLIGHT) {
        filestat fs = {};
        ok64 lo = FILELStat(&fs, full);
        if (lo == FILENOENT) return OK;    // vanished
        if (lo != OK) return lo;             // permissions etc — propagate
        ron60 mr = fs.mtime;
        if (!SNIFFAtKnown(mr)) {
            fprintf(stderr,
                    "sniff: delete: %.*s has unstamped changes — "
                    "stage with `be put` or revert before deleting\n",
                    (int)$len(rel), (char *)rel[0]);
            c->dirty++;
            return DELDIRTY;     // short-circuit FILEScan
        }
        return OK;
    }

    //  apply
    if (FILEUnLink(full) == OK) c->unlinked++;
    return OK;
}

static ok64 del_dir(u8cs reporoot, u8cs dir_rel) {
    sane($ok(reporoot) && $ok(dir_rel));

    a_path(dir_full);
    if (SNIFFFullpath(dir_full, reporoot, dir_rel) != OK) {
        fprintf(stderr, "sniff: delete: cannot resolve %.*s\n",
                (int)$len(dir_rel), (char *)dir_rel[0]);
        fail(SNIFFFAIL);
    }

    filestat fs = {};
    if (FILELStat(&fs, $path(dir_full)) != OK) {
        //  Already absent — caller will append the dir row idempotently.
        done;
    }

    del_dir_ctx ctx = {.mode = DEL_DIR_PREFLIGHT};
    u8csMv(ctx.reporoot, reporoot);

    //  Preflight: any descendant with ∉ stamp-set mtime aborts.
    ok64 pf = FILEScan(dir_full,
                       (FILE_SCAN)(FILE_SCAN_FILES | FILE_SCAN_LINKS |
                                   FILE_SCAN_DEEP),
                       del_dir_cb, &ctx);
    if (ctx.dirty > 0) return DELDIRTY;
    if (pf != OK) return pf;

    //  Apply: unlink every descendant.  Empty dirs are not removed —
    //  POST won't emit them either (an empty dir has no tree entry).
    ctx.mode = DEL_DIR_APPLY;
    ok64 ar = FILEScan(dir_full,
                       (FILE_SCAN)(FILE_SCAN_FILES | FILE_SCAN_LINKS |
                                   FILE_SCAN_DEEP),
                       del_dir_cb, &ctx);
    if (ar != OK) return ar;

    fprintf(stderr, "sniff: delete: %.*s — %u file(s) unlinked\n",
            (int)$len(dir_rel), (char *)dir_rel[0], ctx.unlinked);
    done;
}

// --- Bare-form sweep: find tracked paths that are gone from disk ---
//
//  Walks the wt's baseline tree once; for every tracked file whose
//  on-disk counterpart is missing (lstat → ENOENT), appends a
//  `delete <path>` row to the ULOG with strictly-increasing ts.
//  Quiet no-op when there's no baseline or nothing is missing.

typedef struct {
    u8cs   reporoot;
    ron60  ts;          // bumped after each emitted row
    ron60  verb;
    u32    emitted;
    ok64   err;
} del_sweep_ctx;

static ok64 del_sweep_visit(u8cs path, u8 kind, u8cp esha,
                            u8cs blob, void0p vctx) {
    (void)esha; (void)blob;
    if (kind == WALK_KIND_DIR || kind == WALK_KIND_SUB) return OK;
    if ($empty(path)) return OK;
    del_sweep_ctx *c = (del_sweep_ctx *)vctx;

    a_path(fp);
    if (SNIFFFullpath(fp, c->reporoot, path) != OK) return OK;

    filestat fs = {};
    if (FILELStat(&fs, $path(fp)) == OK) return OK;
    //  Anything other than ENOENT (permission denied, ELOOP, …) →
    //  silent skip; the bare form is a sweep, not a hard refusal.

    uri urow = {};
    urow.path[0] = path[0];
    urow.path[1] = path[1];
    ok64 ar = SNIFFAtAppendAt(c->ts, c->verb, &urow);
    if (ar != OK) { c->err = ar; return ar; }
    c->ts++;
    c->emitted++;
    return OK;
}

static ok64 del_sweep_missing(u8cs reporoot, ron60 ts, ron60 verb,
                              u32 *emitted_out) {
    sane(emitted_out);
    *emitted_out = 0;

    ron60 bts = 0, bverb = 0;
    uri u = {};
    if (SNIFFAtBaseline(&bts, &bverb, &u) != OK) done;
    sha1hex hex = {};
    if (SNIFFAtQueryFirstSha(&u, &hex) != OK) done;
    sha1 commit_sha = {};
    if (sha1FromSha1hex(&commit_sha, &hex) != OK) done;
    sha1 tree_sha = {};
    if (KEEPCommitTreeSha(&KEEP, &commit_sha, &tree_sha) != OK) done;

    del_sweep_ctx ctx = {.ts = ts, .verb = verb};
    u8csMv(ctx.reporoot, reporoot);
    (void)WALKTreeLazy(&KEEP, tree_sha.data, del_sweep_visit, &ctx);
    if (ctx.err != OK) return ctx.err;
    *emitted_out = ctx.emitted;
    done;
}

ok64 DELStage(u32 nuris, uri const *uris) {
    sane(SNIFF.h && (nuris == 0 || uris != NULL));

    if (nuris == 0) {
        //  Sweep: any tracked path missing from disk gets a delete row.
        ron60 sweep_ts = 0;
        struct timespec sweep_tv = {};
        SNIFFAtNow(&sweep_ts, &sweep_tv);
        ron60 sweep_verb = SNIFFAtVerbDelete();
        a_dup(u8c, sweep_root, u8bData(SNIFF.h->wt));
        u32 sweep_n = 0;
        ok64 so = del_sweep_missing(sweep_root, sweep_ts, sweep_verb,
                                    &sweep_n);
        if (so != OK) return so;
        if (sweep_n > 0)
            fprintf(stderr,
                    "sniff: delete: swept %u missing file(s)\n", sweep_n);
        done;
    }

    ron60 ts = 0;
    struct timespec tv = {};
    SNIFFAtNow(&ts, &tv);

    ron60 verb = SNIFFAtVerbDelete();
    a_dup(u8c, reporoot, u8bData(SNIFF.h->wt));

    u32 unlinked = 0;
    u32 emitted = 0;
    u32 skipped = 0;

    //  Pre-pass: register file-form URIs for one-shot baseline
    //  classification.  Dir-form URIs (handled inline below) don't
    //  need it — their baseline overlap is computed by POST at commit
    //  time.  Empty / skipped URIs leave reqs[i].raw empty so the
    //  main loop and the classify step both fall through.
    del_req reqs[CLI_MAX_URIS] = {};
    b8 any_file_form = NO;
    for (u32 i = 0; i < nuris && i < CLI_MAX_URIS; i++) {
        u8cs raw = {};
        SNIFFAtPathBytes(&uris[i], raw);
        if (u8csEmpty(raw)) continue;
        if (*u8csLast(raw) == '/') continue;        // dir-form
        reqs[i].raw[0] = raw[0];
        reqs[i].raw[1] = raw[1];
        any_file_form = YES;
    }
    if (any_file_form) {
        del_ctx dctx = {.reqs = reqs,
                        .n    = nuris < CLI_MAX_URIS ? nuris : CLI_MAX_URIS};
        call(SNIFFClassify, del_classify_step, &dctx);
    }

    for (u32 i = 0; i < nuris; i++) {
        u8cs raw = {};
        SNIFFAtPathBytes(&uris[i], raw);
        if (u8csEmpty(raw)) continue;

        //  Trailing-slash dir form: atomic recursive delete.  Refuses
        //  DELDIRTY on the first dirty descendant; otherwise
        //  unlinks every file under the prefix and appends one
        //  `delete <dir>/` row.  POST drops the whole subtree from
        //  the new commit's tree.
        if (*u8csLast(raw) == '/') {
            ok64 dr = del_dir(reporoot, raw);
            if (dr != OK) return dr;
            uri urow = {};
            urow.path[0] = raw[0];
            urow.path[1] = raw[1];
            call(SNIFFAtAppendAt, ts, verb, &urow);
            ts++;
            emitted++;
            continue;
        }

        a_path(fp);
        if (SNIFFFullpath(fp, reporoot, raw) != OK) {
            fprintf(stderr, "sniff: delete: cannot resolve %.*s\n",
                    (int)$len(raw), (char *)raw[0]);
            fail(SNIFFFAIL);
        }

        filestat fs = {};
        b8 exists = (FILELStat(&fs, $path(fp)) == OK);

        if (exists) {
            //  Dirty-safety: mtime must be in the ULOG stamp-set
            //  (file was last written by some sniff-tracked op).
            //  ∉ stamp-set ⇒ user-edited; refuse to clobber.
            ron60 mr = fs.mtime;
            if (!SNIFFAtKnown(mr)) {
                fprintf(stderr,
                        "sniff: delete: %.*s has unstamped changes — "
                        "stage with `be put` or revert before deleting\n",
                        (int)$len(raw), (char *)raw[0]);
                return DELDIRTY;
            }

            if (FILEUnLink($path(fp)) != OK) {
                fprintf(stderr, "sniff: delete: unlink %.*s failed\n",
                        (int)$len(raw), (char *)raw[0]);
                fail(SNIFFFAIL);
            }
            unlinked++;
        } else {
            //  Already absent on disk.  Only emit a row if the path
            //  was actually in the baseline tree — otherwise this is
            //  a no-op (the user named a path that never existed in
            //  the repo, e.g. a typo / a renamed-away file).
            //  Pre-classified by SNIFFClassify above.
            b8 in_baseline = (i < CLI_MAX_URIS) ? reqs[i].in_baseline : NO;
            if (!in_baseline) {
                skipped++;
                continue;
            }
        }

        uri urow = {};
        urow.path[0] = raw[0];
        urow.path[1] = raw[1];
        call(SNIFFAtAppendAt, ts, verb, &urow);
        ts++;
        emitted++;
    }

    if (skipped > 0)
        fprintf(stderr,
                "sniff: deleted %u file(s) (%u row(s), %u skipped)\n",
                unlinked, emitted, skipped);
    else
        fprintf(stderr,
                "sniff: deleted %u file(s) (%u row(s))\n",
                unlinked, emitted);
    done;
}

//  Iteration callback: any active key whose query is a strict path
//  prefix of `target` + '/' counts as a descendant.  Hitting one
//  sets `has_descendant` and short-circuits the walk via REFSSTOP.
typedef struct {
    u8cs target;
    b8   has_descendant;
} del_descendant_ctx;

static ok64 del_descendant_cb(refcp r, void *ctx) {
    sane(r && ctx);
    del_descendant_ctx *d = (del_descendant_ctx *)ctx;

    //  Each key looks like `?<branch>` (or with a host prefix for
    //  remote-observed rows).  We only care about local rows whose
    //  query is `<target>/<sub>` (sub-branch).  Parse the URI to get
    //  its query slice; ignore non-local (host-prefixed) rows.
    uri ku = {};
    u8csMv(ku.data, r->key);
    ok64 lo = URILexer(&ku);
    if (lo != OK) done;
    if (!u8csEmpty(ku.host)) done;          // remote observation
    a_dup(u8c, q, ku.query);
    if (u8csEmpty(q)) done;                 // trunk row, ignore

    //  q must start with `<target>/` and have additional bytes.
    size_t tl = u8csLen(d->target);
    if (u8csLen(q) <= tl + 1) done;
    if (!u8csHasPrefix(q, d->target)) done;
    if (q[0][tl] != '/') done;

    d->has_descendant = YES;
    return REFSSTOP;                       // short-circuit walk
}

//  Collect every descendant branch (key whose query is `<target>/<sub>`)
//  into an arena buffer.  Each name is preceded by a 1-byte length so
//  the recursive walk can iterate without further parsing.  Caller
//  picks deepest-first by sorting on length descending, then byte
//  order (irrelevant for correctness).
typedef struct {
    u8cs target;
    u8b *names;       // NUL-terminated names, packed
    ok64 err;
} del_descendants_collect_ctx;

static ok64 del_collect_descendants_cb(refcp r, void *ctx) {
    sane(r && ctx);
    del_descendants_collect_ctx *d = ctx;
    if (d->err != OK) done;

    uri ku = {};
    u8csMv(ku.data, r->key);
    ok64 lo = URILexer(&ku);
    if (lo != OK) done;
    if (!u8csEmpty(ku.host)) done;
    a_dup(u8c, q, ku.query);
    if (u8csEmpty(q)) done;

    size_t tl = u8csLen(d->target);
    if (u8csLen(q) <= tl + 1) done;
    if (!u8csHasPrefix(q, d->target)) done;
    if (q[0][tl] != '/') done;

    u8bFeed(*d->names, q);
    u8bFeed1(*d->names, '\0');
    done;
}

//  Forty ASCII '0' chars — the tombstone fragment.
#define DEL_ZERO_HEX                                               \
    "0000000000000000000000000000000000000000"

ok64 DELBranch(uri const *u, b8 recursive) {
    sane(SNIFF.h && u);

    keeper *k = &KEEP;
    a_path(keepdir, u8bDataC(k->h->root), KEEP_DIR_S);

    //  Target branch name (URI query bytes).  Trunk has an empty
    //  query — refuse early; can't drop trunk via this path.
    u8cs target = {u->query[0], u->query[1]};
    if (u8csEmpty(target)) {
        fprintf(stderr, "sniff: delete: refusing to drop trunk\n");
        fail(SNIFFFAIL);
    }

    //  Refuse if the wt is currently on the branch being deleted —
    //  the wt would lose its branch pointer.  The manual delete-
    //  and-recreate workflow for non-ff recovery is still the
    //  user's: stash/move whatever in-flight changes you need,
    //  switch wt off the branch (`be get ?..`), drop, recreate.
    {
        ron60 bts = 0, bverb = 0;
        uri bu = {};
        if (SNIFFAtBaseline(&bts, &bverb, &bu) == OK) {
            if (!u8csEmpty(bu.query) && u8csEq(bu.query, target)) {
                fprintf(stderr,
                        "sniff: delete: wt is on `%.*s` — switch to "
                        "another branch first (`be get ?..`)\n",
                        (int)u8csLen(target), (char *)target[0]);
                fail(SNIFFFAIL);
            }
        }
    }

    //  Refuse if any active descendant label exists — unless
    //  `recursive` is set, in which case drop them deepest-first
    //  before falling through to drop the target itself.
    {
        del_descendant_ctx dctx = {.target = {target[0], target[1]},
                                   .has_descendant = NO};
        ok64 eo = REFSEach($path(keepdir), del_descendant_cb, &dctx);
        if (eo != OK) {
            fprintf(stderr,
                    "sniff: delete: REFS scan failed (%s)\n",
                    ok64str(eo));
            fail(SNIFFFAIL);
        }
        if (dctx.has_descendant && !recursive) {
            fprintf(stderr,
                    "sniff: delete: `%.*s` has active descendant "
                    "branches — pass `--force` (or `-r`) to drop "
                    "the subtree\n",
                    (int)u8csLen(target), (char *)target[0]);
            fail(SNIFFFAIL);
        }
        if (dctx.has_descendant && recursive) {
            //  Collect every descendant; drop deepest first.
            //  Iterate to a fixed point so deletes that surface
            //  newly-leafed children (after their leaves go) are
            //  picked up.
            for (;;) {
                Bu8 names = {};
                if (u8bAllocate(names, 1UL << 16) != OK) fail(NOROOM);
                del_descendants_collect_ctx cctx = {
                    .target = {target[0], target[1]},
                    .names  = &names,
                    .err    = OK,
                };
                ok64 co = REFSEach($path(keepdir),
                                   del_collect_descendants_cb, &cctx);
                if (co != OK || cctx.err != OK) {
                    u8bFree(names);
                    fail(SNIFFFAIL);
                }
                if ($empty(u8bData(names))) {
                    u8bFree(names);
                    break;
                }
                //  Pick the longest (deepest) name from the buffer.
                u8cp p = u8bDataHead(names);
                u8cp end = u8bIdleHead(names);
                u8cp best_b = NULL;
                size_t best_len = 0;
                while (p < end) {
                    u8cp q = p;
                    while (q < end && *q != '\0') q++;
                    size_t l = (size_t)(q - p);
                    if (l > best_len) { best_b = p; best_len = l; }
                    p = (q < end) ? q + 1 : end;
                }
                if (best_b == NULL) { u8bFree(names); break; }

                //  Build a synthetic uri for DELBranch(non-recursive).
                uri sub = {};
                sub.query[0]    = best_b;
                sub.query[1]    = best_b + best_len;
                a_pad(u8, dbuf, 260);
                u8bFeed1(dbuf, '?');
                u8cs nm = {best_b, best_b + best_len};
                u8bFeed(dbuf, nm);
                sub.data[0] = u8bDataHead(dbuf);
                sub.data[1] = u8bIdleHead(dbuf);
                ok64 dr = DELBranch(&sub, NO);
                u8bFree(names);
                if (dr != OK) fail(dr);
            }
        }
    }

    //  Build refkey `?<target>` and tombstone value `0000…0`.
    a_pad(u8, keybuf, 128);
    u8bFeed1(keybuf, '?');
    u8bFeed(keybuf, target);
    a_dup(u8c, refkey, u8bData(keybuf));
    a_cstr(zeros, DEL_ZERO_HEX);

    call(REFSAppendVerb, $path(keepdir), REFSVerbDelete(), refkey, zeros);

    //  Drop the per-branch keeper shard if it was materialised by a
    //  prior `be post ?./X` (PUTSetLabel).  KEEPBranchDrop is the
    //  inverse of KEEPCreateBranch; KEEPNONE means the dir was never
    //  created (older REFS-only labels), KEEPTRUNK / KEEPDIRTY are
    //  guarded above.  Failing here would leave the REFS tombstone
    //  in place and the dir on disk — log + continue: the REFS state
    //  is the source of truth.
    {
        ok64 do_ = KEEPBranchDrop(k, target);
        if (do_ != OK && do_ != KEEPNONE && do_ != KEEPTRUNK) {
            fprintf(stderr,
                    "sniff: delete: REFS tombstoned but shard dir "
                    "drop failed (%s)\n",
                    ok64str(do_));
        }
    }

    fprintf(stderr, "sniff: deleted ?%.*s\n",
            (int)u8csLen(target), (char *)target[0]);
    done;
}
