//  GET: checkout a commit tree from keeper.
//
//  Responsibilities:
//    * Materialise every file in the commit's tree, creating parent
//      dirs as needed.
//    * Dirty-protect: if a file on disk has an mtime not in sniff's
//      stamp-set, leave it alone.
//    * Stamp every file we write with a shared ron60 timestamp via
//      utimensat, so a later stat() recovers that same stamp.
//    * Append one `get` ULOG row with the same timestamp.
//    * Prune: unlink any wt file that sniff wrote before but isn't
//      in the new target tree (stamp-set check protects user files).
//
#include "GET.h"

#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "dog/HOME.h"
#include "keeper/GIT.h"
#include "keeper/REFS.h"
#include "keeper/WALK.h"

#include "AT.h"

typedef struct {
    keeper        *k;
    u8cs           reporoot;
    ron60          ts;          // stamp to apply via utimensat
    struct timespec tv;         // same stamp in timespec form
    Bu8            target;      // bitmap: target[idx] = 1 iff path idx is
                                // in the new tree (set during walk, read
                                // during prune).
    Bu8            gitlink;     // bitmap: gitlink[idx] = 1 iff path idx is
                                // a submodule entry (mode 160000) in the
                                // new tree.  Prune does not descend into
                                // any path whose ancestor is a gitlink —
                                // sniff doesn't manage submodule contents,
                                // so wiping files there would corrupt a
                                // legitimately checked-out submodule.
    u32            target_cap;
    ok64           error;
    //  Newline-separated, lex-sorted path list (subset of the target
    //  tree) where the target sha matches the baseline sha.  WALKTreeLazy
    //  visits the target in the same order, so we advance this cursor
    //  in lockstep: a path matching the head means "no-op overlay —
    //  preserve whatever bytes are on disk" (including dirty user edits).
    u8cs           noop_cursor;
} get_ctx;

static void get_mark_target(get_ctx *g, u32 idx) {
    if (idx >= g->target_cap) return;
    u8 *base = u8bDataHead(g->target);
    base[idx] = 1;
}

static b8 get_is_target(get_ctx const *g, u32 idx) {
    if (idx >= g->target_cap) return NO;
    u8 const *base = u8bDataHead(g->target);
    return base[idx] != 0;
}

static void get_mark_gitlink(get_ctx *g, u32 idx) {
    if (idx >= g->target_cap) return;
    u8 *base = u8bDataHead(g->gitlink);
    base[idx] = 1;
}

static b8 get_is_gitlink(get_ctx const *g, u32 idx) {
    if (idx >= g->target_cap) return NO;
    u8 const *base = u8bDataHead(g->gitlink);
    return base[idx] != 0;
}

//  YES iff `rel` lives strictly beneath any path the new tree marks
//  as a gitlink (submodule).  Walks rel left-to-right, interning each
//  `<prefix-up-to-slash>` and checking the gitlink bitmap.  Used by
//  prune to leave the submodule's own contents alone.
static b8 get_under_gitlink(get_ctx const *g, u8cs rel) {
    u8c const *start = rel[0];
    u8c const *end   = rel[1];
    u8c const *cur   = start;
    while (cur < end) {
        if (*cur == '/') {
            u8cs prefix = {start, cur};
            u32 idx = SNIFFIntern(prefix);
            if (get_is_gitlink(g, idx)) return YES;
        }
        cur++;
    }
    return NO;
}

//  Write one blob to disk, create dirs as needed, chmod if exec,
//  symlink if link; then stamp it with ctx->tv.  Caller is
//  responsible for the dirty-overlap pre-flight (see get_overlap_check
//  and GETCheckout); reaching here means no dirty collision was
//  detected.
static ok64 get_write_one(get_ctx *g, u8cs path, u8 kind, u8cp esha) {
    sane(g);
    keeper *k = g->k;

    a_path(fp);
    call(SNIFFFullpath, fp, g->reporoot, path);

    Bu8 bbuf = {};
    call(u8bAllocate, bbuf, 1UL << 24);
    u8 bt = 0;
    sha1 entry_sha = {};
    memcpy(entry_sha.data, esha, 20);
    ok64 o = KEEPGetExact(k, &entry_sha, bbuf, &bt);
    if (o != OK) { u8bFree(bbuf); return o; }

    if (kind == WALK_KIND_LNK) {
        unlink((char *)u8bDataHead(fp));
        u8bFeed1(bbuf, 0);
        if (symlink((char *)u8bDataHead(bbuf), (char *)u8bDataHead(fp)) != 0) {
            u8bFree(bbuf);
            fail(SNIFFFAIL);
        }
    } else {
        int fd = -1;
        o = FILECreate(&fd, $path(fp));
        if (o != OK) { u8bFree(bbuf); return o; }
        u8cs data = {u8bDataHead(bbuf), u8bIdleHead(bbuf)};
        o = FILEFeedAll(fd, data);
        FILEClose(&fd);
        if (o != OK) { u8bFree(bbuf); return o; }
        if (kind == WALK_KIND_EXE)
            chmod((char *)u8bDataHead(fp), 0755);
    }
    u8bFree(bbuf);

    call(SNIFFAtStampPath, fp, g->ts);
    done;
}

static ok64 get_visit(u8cs path, u8 kind, u8cp esha, u8cs blob,
                      void0p vctx) {
    (void)blob;  // lazy mode
    get_ctx *g = (get_ctx *)vctx;

    if (kind == WALK_KIND_SUB) {
        //  Submodule (gitlink, mode 160000).  Mark the path so prune
        //  treats it as part of the target tree (don't unlink the
        //  submodule directory itself), and record the gitlink prefix
        //  so prune doesn't descend into the submodule's own files.
        u32 idx = SNIFFIntern(path);
        get_mark_target(g, idx);
        get_mark_gitlink(g, idx);
        return WALKSKIP;
    }

    if (kind == WALK_KIND_DIR) {
        if ($empty(path)) return OK;    // root; walker recurses
        a_path(dp);
        SNIFFFullpath(dp, g->reporoot, path);
        FILEMakeDirP($path(dp));
        return OK;
    }

    //  File-like entry (REG / EXE / LNK).  Mark the path as a target
    //  before writing so prune won't touch it even if write fails.
    {
        u32 idx = SNIFFIntern(path);
        get_mark_target(g, idx);
    }

    //  No-op overlay: target's content at this path equals baseline's;
    //  pre-flight cleared the merged path list in lockstep with this
    //  walk's order.  Skip the write so dirty user edits aren't
    //  clobbered by a rewrite of identical bytes.  Don't stamp either —
    //  the file's mtime stays whatever the user left it as.
    //
    //  Exception: if the file was wiped from disk (lstat fails), there
    //  is nothing to preserve — fall through and recreate it.
    if (!u8csEmpty(g->noop_cursor)) {
        u8cs head = {};
        a_dup(u8c, peek, g->noop_cursor);
        if (u8csDrainLine(peek, head) == OK
            && $len(head) == $len(path)
            && memcmp(head[0], path[0], (size_t)$len(head)) == 0) {
            (void)u8csDrainLine(g->noop_cursor, head);
            a_path(probe);
            if (SNIFFFullpath(probe, g->reporoot, path) == OK) {
                struct stat sb = {};
                if (lstat((char *)u8bDataHead(probe), &sb) == 0)
                    return OK;     // present on disk — preserve it
            }
            //  File missing → fall through to get_write_one.
        }
    }

    ok64 o = get_write_one(g, path, kind, esha);
    if (o != OK) g->error = o;
    return o;
}

// --- Pre-flight overlap check via baseline ↔ target N-way merge.
//
//  Materialise both trees as sorted (path, meta) pairs (meta = 21-byte
//  records {kind, sha[20]}), then drain via KEEPu8ssDrain.  For each
//  emitted path:
//    * both sides + identical {kind,sha} → no-op overlay: append to
//      `noop_out` so the WRITE pass can skip the rewrite (preserving
//      any dirty user content), no dirty check needed
//    * either side is SUB                → gitlink change, no wt-file
//                                           write → skip
//    * else (real change, add, or delete) → lstat; conflict if mtime
//                                            ∉ stamp-set.
//  Returns SNIFFOVRL when conflicts > 0 (printing up to 5 paths and a
//  summary line).  `noop_out` is reset on entry; on success it carries
//  newline-separated lex-sorted paths (subset of the target tree).
//
//  `base_tree` may be NULL — first-checkout / no-baseline case.  Then
//  every drained path has mask 0b10 (target only) and is treated as
//  incoming, matching the pre-merge per-target-path overlap check;
//  `noop_out` ends up empty.
static ok64 get_overlap_check(keeper *k, u8cs reporoot,
                              u8cp base_tree, u8cp tgt_tree,
                              u8bp noop_out) {
    sane(k && tgt_tree && noop_out);
    u8bReset(noop_out);
    Bu8 bp = {}, bm = {}, tp = {}, tm = {};
    call(u8bAllocate, bp, 1UL << 20);
    call(u8bAllocate, bm, 1UL << 20);
    call(u8bAllocate, tp, 1UL << 20);
    call(u8bAllocate, tm, 1UL << 20);

    ok64 r = OK;
    if (base_tree) r = KEEPTreeListLeaves(k, base_tree, bp, bm);
    if (r == OK)   r = KEEPTreeListLeaves(k, tgt_tree, tp, tm);
    if (r != OK) {
        u8bFree(bp); u8bFree(bm); u8bFree(tp); u8bFree(tm);
        return r;
    }

    a_dup(u8c, view_b, u8bData(bp));
    a_dup(u8c, view_t, u8bData(tp));
    a_pad(u8cs, ins, 2);
    u8cssFeed1(ins_idle, view_b);
    u8cssFeed1(ins_idle, view_t);
    a_dup(u8cs, view, u8csbData(ins));

    u8 const *bm_base = u8bDataHead(bm);
    u8 const *tm_base = u8bDataHead(tm);
    size_t b_idx = 0, t_idx = 0;
    u32 conflicts = 0;

    for (;;) {
        u8cs path = {};
        u64 mask = 0;
        ok64 dr = KEEPu8ssDrain(view, path, &mask);
        if (dr != OK) break;

        b8 in_base = (mask & 1) != 0;
        b8 in_tgt  = (mask & 2) != 0;
        u8cs br = {};
        u8cs tr = {};
        if (in_base) {
            br[0] = bm_base + b_idx * 21;
            br[1] = bm_base + (b_idx + 1) * 21;
        }
        if (in_tgt) {
            tr[0] = tm_base + t_idx * 21;
            tr[1] = tm_base + (t_idx + 1) * 21;
        }
        u8 b_kind = in_base ? br[0][0] : 0;
        u8 t_kind = in_tgt  ? tr[0][0] : 0;

        b8 changed = !in_base || !in_tgt
                  || u8cscmp(&br, &tr) != 0;
        b8 is_sub  = (b_kind == WALK_KIND_SUB) || (t_kind == WALK_KIND_SUB);

        if (!changed && in_tgt && !is_sub) {
            //  No-op overlay: record the path so the WRITE pass skips it.
            u8bFeed(noop_out, path);
            u8bFeed1(noop_out, '\n');
        } else if (changed && !is_sub) {
            a_path(fp);
            ok64 fo = SNIFFFullpath(fp, reporoot, path);
            if (fo == OK) {
                struct stat sb = {};
                if (lstat((char *)u8bDataHead(fp), &sb) == 0) {
                    struct timespec mts = {.tv_sec = sb.st_mtim.tv_sec,
                                           .tv_nsec = sb.st_mtim.tv_nsec};
                    ron60 mr = SNIFFAtOfTimespec(mts);
                    if (!SNIFFAtKnown(mr)) {
                        if (conflicts < 5)
                            fprintf(stderr,
                                    "sniff: dirty overlap %.*s\n",
                                    (int)$len(path),
                                    (char *)path[0]);
                        conflicts++;
                    }
                }
            }
        }

        if (in_base) b_idx++;
        if (in_tgt)  t_idx++;
    }

    u8bFree(bp); u8bFree(bm); u8bFree(tp); u8bFree(tm);

    if (conflicts > 0) {
        fprintf(stderr,
                "sniff: GET refused — %u dirty file(s) overlap with "
                "incoming changes; commit, stash, or reset before "
                "checkout\n", conflicts);
        return SNIFFOVRL;
    }
    done;
}

// --- Prune: unlink any wt file that sniff wrote before but isn't in
//     the new target tree.  The stamp-set check protects user-created
//     untracked files (they never carry a sniff stamp).

typedef struct { get_ctx *g; u32 pruned; } prune_ctx;

static ok64 get_prune_cb(void *varg, path8bp path) {
    sane(varg);
    prune_ctx *p = (prune_ctx *)varg;
    get_ctx *g = p->g;
    a_dup(u8c, full, u8bData(path));

    u8cs rel = {};
    if (!SNIFFRelFromFull(&rel, g->reporoot, full)) return OK;
    if (SNIFFSkipMeta(rel))                         return OK;

    //  Don't descend into a submodule the new tree carries as a
    //  gitlink.  The submodule's own files (mtimes ≠ sniff stamps in
    //  the normal case, but stale leaks from a corrupted commit can
    //  still carry sniff stamps) are not ours to manage.
    if (get_under_gitlink(g, rel)) return OK;

    u32 idx = SNIFFIntern(rel);
    if (get_is_target(g, idx)) return OK;

    struct stat sb = {};
    if (lstat((char const *)full[0], &sb) != 0) return OK;
    struct timespec ts = {.tv_sec = sb.st_mtim.tv_sec,
                          .tv_nsec = sb.st_mtim.tv_nsec};
    ron60 r = SNIFFAtOfTimespec(ts);
    if (!SNIFFAtKnown(r)) return OK;   // untracked user file — leave alone.

    //  unlink() wants a NUL-terminated C string; `path` is NUL-termed
    //  by FILEScanRecurse → PATHu8bTerm at each level, so the byte at
    //  full[1] is already '\0' (see abc/PATH.h).
    if (unlink((char const *)full[0]) == 0) p->pruned++;
    return OK;
}

static ok64 get_prune(get_ctx *g) {
    sane(g);
    a_path(root_path);
    u8bFeed(root_path, g->reporoot);
    call(PATHu8bTerm, root_path);
    prune_ctx pctx = {.g = g, .pruned = 0};
    call(FILEScan, root_path,
         (FILE_SCAN)(FILE_SCAN_FILES | FILE_SCAN_LINKS | FILE_SCAN_DEEP),
         get_prune_cb, &pctx);
    if (pctx.pruned > 0)
        fprintf(stderr, "sniff: pruned %u file(s)\n", pctx.pruned);
    done;
}

// --- Pre-flight: cross-branch wt-dirty scan ---
//
//  Walks every non-meta file in the wt; if any has an mtime that
//  isn't in sniff's stamp-set, the wt is considered dirty and a
//  cross-branch GET must refuse.  Same membership rule as
//  `sniff status` (shared via SNIFFAtScanDirty).

static ok64 get_wt_dirty_cb(u8cs rel, void *ctx) {
    sane(ctx);
    u32 *count = (u32 *)ctx;
    if (*count < 5)
        fprintf(stderr, "sniff: dirty %.*s\n",
                (int)$len(rel), (char *)rel[0]);
    (*count)++;
    return OK;
}

static b8 get_wt_dirty(u8cs reporoot) {
    u32 count = 0;
    (void)SNIFFAtScanDirty(reporoot, get_wt_dirty_cb, &count);
    return count > 0;
}

// --- Public API ---

ok64 GETCheckout(u8cs reporoot, u8cs hex, u8cs source) {
    sane($ok(hex));
    keeper *k = &KEEP;

    fprintf(stderr, "GETDBG GETCheckout hex=[%.*s] hexlen=%lld\n",
            (int)$len(hex), (char const *)hex[0], (long long)$len(hex));
    size_t hexlen = $len(hex);
    if (hexlen > 15) hexlen = 15;
    u64 hashlet = WHIFFHexHashlet60(hex);

    Bu8 buf = {};
    call(u8bAllocate, buf, 1UL << 24);
    u8 otype = 0;
    ok64 o = KEEPGet(k, hashlet, hexlen, buf, &otype);
    if (o != OK) {
        u8bFree(buf);
        fprintf(stderr, "sniff: object not found\n");
        fail(SNIFFFAIL);
    }

    //  Dereference annotated tag.
    if (otype == DOG_OBJ_TAG) {
        u8cs body = {u8bDataHead(buf), u8bIdleHead(buf)};
        u8cs field = {}, value = {};
        sha1 tag_sha = {};
        a_raw(tag_bin, tag_sha);
        b8 found = NO;
        while (GITu8sDrainCommit(body, field, value) == OK) {
            if ($empty(field)) break;
            if ($len(field) == 6 && memcmp(field[0], "object", 6) == 0 &&
                $len(value) >= 40) {
                u8cs hex40 = {value[0], $atp(value, 40)};
                HEXu8sDrainSome(tag_bin, hex40);
                found = YES;
                break;
            }
        }
        if (!found) {
            u8bFree(buf);
            fprintf(stderr, "sniff: bad tag (no object)\n");
            fail(SNIFFFAIL);
        }
        u8bReset(buf);
        o = KEEPGetExact(k, &tag_sha, buf, &otype);
        if (o != OK || otype != DOG_OBJ_COMMIT) {
            u8bFree(buf);
            fprintf(stderr, "sniff: tag target not a commit\n");
            fail(SNIFFFAIL);
        }
    }

    if (otype != DOG_OBJ_COMMIT) {
        u8bFree(buf);
        fprintf(stderr, "sniff: not a commit\n");
        fail(SNIFFFAIL);
    }

    sha1 tree_sha = {};
    u8cs commit = {u8bDataHead(buf), u8bIdleHead(buf)};
    fprintf(stderr, "GETDBG commit body (first 120 bytes): %.*s\n",
            (int)($len(commit) < 120 ? $len(commit) : 120),
            (char const *)commit[0]);
    o = GITu8sCommitTree(commit, tree_sha.data);
    fprintf(stderr, "GETDBG tree_sha=%02x%02x%02x%02x\n",
            tree_sha.data[0], tree_sha.data[1],
            tree_sha.data[2], tree_sha.data[3]);
    u8bFree(buf);
    if (o != OK) {
        fprintf(stderr, "sniff: bad commit (no tree)\n");
        fail(SNIFFFAIL);
    }

    //  --- Pre-flight gate: cross-branch wt-dirty refuse ---------
    //  Compare baseline branch (latest get/post/patch row) to the
    //  target branch.  When they differ, any unattributed mtime in
    //  the wt blocks the switch — the caller must commit, stash,
    //  or reset before `be get` will move them off the current
    //  branch.  Same-branch GETs (refresh, restore wiped wt, or
    //  switch tips on the same branch) skip this gate; the per-
    //  file overlap pre-flight further down is sufficient there.
    {
        ron60 bts = 0, bverb = 0;
        uri bu = {};
        if (SNIFFAtBaseline(&bts, &bverb, &bu) == OK) {
            u8cs t_branch = {};
            if ($ok(source) && !u8csEmpty(source) &&
                *source[0] == '?' && $len(source) != 41) {
                t_branch[0] = $atp(source, 1);
                t_branch[1] = source[1];
            }
            u8cs b_branch = {bu.query[0], bu.query[1]};
            ssize_t bl = $len(b_branch), tl = $len(t_branch);
            b8 same_branch =
                (bl == tl) &&
                (bl == 0 ||
                 memcmp(b_branch[0], t_branch[0], (size_t)bl) == 0);

            if (!same_branch && get_wt_dirty(reporoot)) {
                fprintf(stderr,
                        "sniff: cross-branch GET refused "
                        "— wt is dirty\n");
                fail(SNIFFDRTY);
            }
        }
    }
    //  --- end pre-flight gate ------------------------------------

    //  Resolve the baseline tree from the latest get/post/patch row.
    //  Used by the overlap pre-flight to distinguish "incoming change"
    //  from "no-op overlay".  Absent baseline (fresh wt) → NULL pointer
    //  passes through, every target path is treated as incoming.
    sha1 base_tree = {};
    b8 has_base_tree = NO;
    {
        ron60 bts = 0, bverb = 0;
        uri bu = {};
        if (SNIFFAtBaseline(&bts, &bverb, &bu) == OK) {
            u8 hex40[40];
            if (SNIFFAtQueryFirstSha(&bu, hex40) == OK) {
                u8cs hex_s = {hex40, hex40 + 40};
                u64 bhashlet = WHIFFHexHashlet60(hex_s);
                Bu8 cbuf = {};
                if (u8bAllocate(cbuf, 1UL << 24) == OK) {
                    u8 ctype = 0;
                    if (KEEPGet(k, bhashlet, 40, cbuf, &ctype) == OK &&
                        ctype == DOG_OBJ_COMMIT) {
                        u8cs cbody = {u8bDataHead(cbuf),
                                      u8bIdleHead(cbuf)};
                        if (GITu8sCommitTree(cbody, base_tree.data) == OK)
                            has_base_tree = YES;
                    }
                    u8bFree(cbuf);
                }
            }
        }
    }

    Bu8 noop = {};
    call(u8bAllocate, noop, 1UL << 20);
    o = get_overlap_check(k, reporoot,
                          has_base_tree ? base_tree.data : NULL,
                          tree_sha.data, noop);
    if (o != OK) { u8bFree(noop); return o; }

    get_ctx ctx = {.k = k, .error = OK};
    u8csMv(ctx.reporoot, reporoot);
    SNIFFAtNow(&ctx.ts, &ctx.tv);
    ctx.noop_cursor[0] = u8bDataHead(noop);
    ctx.noop_cursor[1] = u8bIdleHead(noop);

    //  Size the target/gitlink bitmaps: SNIFFCount() grows during the
    //  walk as keeper interns new tree paths, so pad generously.
    ctx.target_cap = SNIFFCount() + (1u << 20);
    call(u8bAllocate, ctx.target, ctx.target_cap);
    memset(u8bDataHead(ctx.target), 0, ctx.target_cap);
    call(u8bAllocate, ctx.gitlink, ctx.target_cap);
    memset(u8bDataHead(ctx.gitlink), 0, ctx.target_cap);

    o = WALKTreeLazy(k, tree_sha.data, get_visit, &ctx);
    if (o == OK && ctx.error != OK) o = ctx.error;
    if (o != OK) {
        u8bFree(noop); u8bFree(ctx.target); u8bFree(ctx.gitlink);
        return o;
    }

    //  Prune: any path on disk that was sniff-stamped but isn't in the
    //  new target tree.
    (void)get_prune(&ctx);
    u8bFree(ctx.target); u8bFree(ctx.gitlink); u8bFree(noop);

    //  Compose the `get` row URI via abc/URI.  Canonical at-log form:
    //  `?<branch>#<curhash>` — query carries the be-branch path
    //  (empty for trunk), fragment carries the tip sha.  Mirrors the
    //  REFS row format so readers walk the same shape everywhere.
    uri urow = {};
    a_pad(u8, qbuf, 128);
    if ($ok(source) && !u8csEmpty(source) && *source[0] == '?' &&
        $len(source) != 41) {
        //  Named refs come in with a leading '?', e.g. `?feat`.
        //  URI query slices exclude the sentinel per RFC 3986, so
        //  drop the leading byte before copying the slice into qbuf.
        a_dup(u8c, q, source);
        u8csUsed1(q);
        u8bFeed(qbuf, q);
    }
    {
        a_dup(u8c, q, u8bData(qbuf));
        urow.query[0] = q[0];
        urow.query[1] = q[1];
    }
    {
        a_dup(u8c, h, hex);
        urow.fragment[0] = h[0];
        urow.fragment[1] = h[1];
    }

    ron60 verb = SNIFFAtVerbGet();
    call(SNIFFAtAppendAt, ctx.ts, verb, &urow);

    //  Advance the keeper-side local-branch tip with verb `post`.
    //  Key: `?heads/<branch>` when `source` carries a branch; bare `?`
    //  (trunk) only when nothing in `source` names one.  Using the
    //  literal branch is critical — REFADV walks `<store>/refs` for
    //  `?heads/<X>` keys, and a bare-`?` row leaves the per-branch
    //  local tip unadvertised, which silently short-circuits future
    //  `WIREPush` calls.  Failure here is non-fatal: the worktree is
    //  already updated, subsequent `be get` re-resolves via the
    //  peer-prefixed row.
    {
        a_path(keepdir, u8bDataC(KEEP.h->root), KEEP_DIR_S);
        a_pad(u8, key_buf, 128);
        u8bFeed1(key_buf, '?');
        if ($ok(source) && !u8csEmpty(source) && *source[0] == '?' &&
            $len(source) != 41) {
            a_dup(u8c, ref_q, source);
            u8csUsed1(ref_q);  //  drop leading '?'
            //  Strip a leading `refs/` if present so the key is the
            //  same `?heads/<X>` form REFSAppendVerb / REFADV expect.
            a_cstr(refs_pfx, "refs/");
            if ($len(ref_q) > 5 &&
                memcmp(ref_q[0], refs_pfx[0], 5) == 0)
                u8csUsed(ref_q, 5);
            u8bFeed(key_buf, ref_q);
        }
        a_dup(u8c, key_s, u8bData(key_buf));
        (void)REFSAppendVerb($path(keepdir), REFSVerbPost(), key_s, hex);
    }

    fprintf(stderr, "sniff: checkout done\n");
    done;
}
