//  WALK: tree walker on KEEP.
//
#include "WALK.h"

#include <stdio.h>
#include <string.h>

#include "abc/HEX.h"
#include "abc/PRO.h"
#include "abc/RON.h"
#include "dog/DOG.h"
#include "dog/DPATH.h"
#include "dog/ROWS.h"
#include "dog/ULOG.h"
#include "dog/git/GIT.h"

u8 WALKu8sModeKind(u8cs mode) {
    if ($empty(mode)) return 0;
    u8 c0 = $at(mode, 0);
    if (c0 == '4') return WALK_KIND_DIR;
    if (c0 != '1' || $len(mode) < 2) return 0;
    u8 c1 = $at(mode, 1);
    if (c1 == '6') return WALK_KIND_SUB;
    if (c1 == '2') return WALK_KIND_LNK;
    if (c1 == '0') {
        // 100644 vs 100755
        return ($len(mode) >= 6 && $at(mode, 3) == '7')
             ? WALK_KIND_EXE : WALK_KIND_REG;
    }
    return 0;
}

//  Depth-first dive through one tree.  `pathbuf` carries the current
//  path (no leading/trailing '/'), shared across recursion levels.
//  Each level owns its own `tbuf` and per-entry `bbuf` (blob) so
//  nested KEEPGetExact calls don't clobber parent bytes.
//  `bbuf` is a caller-provided 1 MiB scratch reused across every
//  blob fetch under this dive AND every recursion level — NULL when
//  !eager (no blobs are fetched).  `tbuf` is per-level (parent's
//  tree data must outlive child recursion's tree fetch).
static ok64 walk_tree_dive(keeper *k, sha1cp tree_sha,
                            u8bp pathbuf, b8 eager, b8 incl_anchor,
                            u8bp bbuf, walk_tree_fn visit, void0p ctx,
                            u32 depth) {
    sane(k && tree_sha && visit);

    //  KEEP-001: hard depth cap so a (mal)formed deeply-nested tree
    //  cannot overflow the C stack.  try()-per-subtree bounds live
    //  scratch to the root-to-leaf chain, so the cap is what bounds a
    //  pathological linear tree (the depth, not the width, is held).
    if (depth >= WALK_MAX_DEPTH) return WALKBADFMT;

    //  KEEP-001: right-size the per-tree carve.  A modest default fits
    //  virtually every real tree in one inflate; the live carve is what
    //  every root-to-leaf chain level holds, so keeping it small lets a
    //  legitimately deep tree reach WALK_MAX_DEPTH inside the BASS
    //  arena.  An oversized tree (>64 KiB) is the only case that
    //  re-inflates, into a 1 MiB carve — not a per-node double-inflate.
    u8 otype = 0;
    u8cs tree_s = {};
    a_carve(u8, tbuf, 1UL << 16);
    ok64 go = KEEPGetExact(tree_sha, tbuf, &otype);
    if (go == BNOROOM) {
        a_carve(u8, big, 1UL << 20);
        call(KEEPGetExact, tree_sha, big, &otype);
        u8csMv(tree_s, u8bDataC(big));
    } else if (go != OK) {
        return go;
    } else {
        u8csMv(tree_s, u8bDataC(tbuf));
    }
    if (otype != DOG_OBJ_TREE) return WALKBADFMT;

    u8cs file = {}, esha = {};
    ok64 result = OK;

    //  POST-015: a `.be` entry (the store anchor name) is never a
    //  tracked path — skip it SILENTLY below rather than flood one
    //  `walk: bad path '.be'` line per scanned tree.
    a_cstr(be_name_lit, DOG_BE_NAME);
    a_dup(u8c, be_name, be_name_lit);

    u8 const *tsp = (u8 const *)tree_sha;
    while (GITu8sDrainTree(tree_s, file, esha, NULL) == OK) {
        u8cs mode_s = {}, name_s = {};
        if (GITu8sFileSplit(file, mode_s, name_s) != OK) continue;
        if ($empty(mode_s) || $empty(name_s)) continue;
        u8 kind = WALKu8sModeKind(mode_s);
        if (kind == 0) continue;
        if (DPATHVerify(name_s) != OK) {
            //  GIT-009: an object-closure walk (incl_anchor) keeps the
            //  `.be` store-anchor entry so its blob lands in the pack —
            //  the shipped tree references it, so dropping it dangles.
            //  Working-tree walks still skip `.be` (untracked anchor);
            //  a genuinely invalid name always warns and skips.
            b8 is_anchor = u8csEq(name_s, be_name);
            if (!(is_anchor && incl_anchor)) {
                if (!is_anchor)
                    fprintf(stderr, "walk: bad path '%.*s', skip\n",
                            (int)$len(name_s), (char *)name_s[0]);
                continue;
            }
        }

        // Push "/name" (or just "name" at root) onto pathbuf.
        size_t pre_len = u8bDataLen(pathbuf);
        if (pre_len > 0) {
            if (u8bFeed1(pathbuf, '/') != OK) { result = WALKNOROOM; break; }
        }
        if (u8bFeed(pathbuf, name_s) != OK) { result = WALKNOROOM; break; }
        u8cs path = {u8bDataHead(pathbuf), pathbuf[2]};

        // Eager blob resolve for file-like kinds — into shared bbuf.
        u8cs blob = {};
        b8 is_file = (kind == WALK_KIND_REG || kind == WALK_KIND_EXE ||
                      kind == WALK_KIND_LNK);
        if (eager && is_file && bbuf) {
            sha1 entry_sha = {};
            sha1Mv(&entry_sha, (sha1cp)esha[0]);
            u8 btype = 0;
            if (KEEPGetExact(&entry_sha, bbuf, &btype) == OK &&
                btype == DOG_OBJ_BLOB) {
                blob[0] = u8bDataHead(bbuf);
                blob[1] = u8bIdleHead(bbuf);
            }
        }

        ok64 vo = visit(path, kind, esha[0], blob, ctx);

        if (vo == OK && kind == WALK_KIND_DIR) {
            sha1 sub = {};
            sha1Mv(&sub, (sha1cp)esha[0]);
            //  Recurse through `try` so this subtree's BASS scratch
            //  (each level's 1 MiB `tbuf`, plus everything the visitor
            //  carves under it) is rewound when the child returns.  A
            //  plain C recursion never frees those carves until the
            //  whole walk unwinds, so a wide tree (~1 dir per MiB)
            //  exhausts the 1 GiB arena and the next `a_carve(tbuf)`
            //  returns BNOROOM mid-walk — a clean tree-order truncation
            //  (GET-020).  `sub`/`pathbuf`/`bbuf` live below the
            //  snapshot (the parent's `tbuf` and its tree-entry slices
            //  too), so they survive the rewind.
            try(walk_tree_dive, k, &sub, pathbuf, eager, incl_anchor, bbuf,
                visit, ctx, depth + 1);
            vo = __;
        }

        // Rewind pathbuf to pre-entry length.
        size_t cur_len = u8bDataLen(pathbuf);
        if (cur_len > pre_len) u8bShed(pathbuf, cur_len - pre_len);

        if (vo == WALKSKIP) continue;
        if (vo == WALKSTOP) { result = WALKSTOP; break; }
        if (vo != OK) { result = vo; break; }
    }

    return result;
}

//  KEEP-001: the one keeper tree walker.  WALK / WIRECLI-push / CLOSE
//  all route their tree descent here, passing per-site behaviour as the
//  visitor (dedup short-circuits via WALKSKIP).  See WALK.h.
ok64 KEEPWalkTree(u8cp tree_sha, b8 eager, b8 incl_anchor,
                  walk_tree_fn visit, void0p ctx) {
    sane(tree_sha && visit);
    keeper *k = &KEEP;
    a_pad(u8, pathbuf, 2048);
    sha1 root = {};
    sha1Mv(&root, (sha1cp)tree_sha);

    u8cs empty_path = {}, empty_blob = {};
    ok64 vo = visit(empty_path, WALK_KIND_DIR, tree_sha, empty_blob, ctx);
    if (vo == WALKSTOP) return OK;
    if (vo == WALKSKIP) return OK;
    if (vo != OK) return vo;

    //  POST-021: carve the shared blob buffer ONLY when eager — !eager
    //  (push/closure) never fetches a blob, so the 1 MiB carve was pure
    //  waste that callers recursing the parent chain in plain C never
    //  rewound, draining BASS to BNOROOM on a long history.
    if (!eager) {
        ok64 o = walk_tree_dive(k, &root, pathbuf, NO, incl_anchor, NULL,
                                visit, ctx, 0);
        if (o == WALKSTOP) return OK;
        return o;
    }
    a_carve(u8, bbuf, 1UL << 20);
    ok64 o = walk_tree_dive(k, &root, pathbuf, eager, incl_anchor, bbuf,
                            visit, ctx, 0);
    if (o == WALKSTOP) return OK;
    return o;
}

ok64 WALKTree(u8cp tree_sha, walk_tree_fn visit, void0p ctx) {
    return KEEPWalkTree(tree_sha, YES, WALK_SKIP_ANCHOR, visit, ctx);
}

ok64 WALKTreeLazy(u8cp tree_sha, walk_tree_fn visit, void0p ctx) {
    return KEEPWalkTree(tree_sha, NO, WALK_SKIP_ANCHOR, visit, ctx);
}

//  ls-files: descend an optional /subpath relative to a URI-resolved
//  tree, then walk.  See WALK.h.

//  Wrapper visitor: prepends a fixed prefix (+ '/') to every emitted
//  path so the outward-facing paths remain absolute when the walk
//  itself started at a subtree.
typedef struct {
    walk_tree_fn inner;
    void0p       inner_ctx;
    u8cs         prefix;   // e.g. "drivers/net"  (no trailing '/')
} lsf_prefix_ctx;

static ok64 lsf_prefix_visit(u8cs path, u8 kind, u8cp esha,
                              u8cs blob, void0p ctx) {
    lsf_prefix_ctx *pc = (lsf_prefix_ctx *)ctx;
    if ($empty(pc->prefix)) {
        return pc->inner(path, kind, esha, blob, pc->inner_ctx);
    }
    //  Concatenate "<prefix>" + (empty ? "" : "/" + path).
    a_pad(u8, pbuf, 4096);
    u8bFeed(pbuf, pc->prefix);
    if (!$empty(path)) {
        u8bFeed1(pbuf, '/');
        u8bFeed(pbuf, path);
    }
    a_dup(u8c, full, u8bData(pbuf));
    return pc->inner(full, kind, esha, blob, pc->inner_ctx);
}

//  Shared '/'-separated tree-path descent (CODE-005).  See WALK.h.
ok64 KEEPTreeDescend(sha1cp root_tree, u8cs subpath, u8bp pathbuf,
                     sha1 *out_sha, u8 *out_kind, u8bp tbuf,
                     ok64 notfound) {
    sane(root_tree && out_sha && out_kind && tbuf);

    sha1 cur_sha = *root_tree;
    u8 cur_kind = WALK_KIND_DIR;

    u8cs scan = {};
    u8csMv(scan, subpath);

    //  Iterate '/'-separated segments.
    while (!$empty(scan)) {
        //  Skip leading '/'.
        while (!$empty(scan) && *scan[0] == '/') scan[0]++;
        if ($empty(scan)) break;

        //  Slice one segment.
        u8cs seg = {scan[0], scan[0]};
        while (seg[1] < scan[1] && *seg[1] != '/') seg[1]++;
        if (seg[0] == seg[1]) break;
        scan[0] = seg[1];  // cursor past segment

        if (cur_kind != WALK_KIND_DIR) return notfound;

        //  Fetch current tree, scan entries for `seg`.
        u8 otype = 0;
        ok64 o = KEEPGetExact(&cur_sha, tbuf, &otype);
        if (o != OK || otype != DOG_OBJ_TREE) return o ? o : notfound;

        u8cs tree_s = {u8bDataHead(tbuf), u8bIdleHead(tbuf)};
        b8 found = NO;
        u8 next_kind = 0;
        sha1 next_sha = {};
        u8cs file = {}, esha = {};
        while (GITu8sDrainTree(tree_s, file, esha, NULL) == OK) {
            u8cs mode_s = {}, name_s = {};
            if (GITu8sFileSplit(file, mode_s, name_s) != OK) continue;
            if (u8csLen(name_s) != u8csLen(seg)) continue;
            if (memcmp(name_s[0], seg[0], u8csLen(name_s)) != 0) continue;
            next_kind = WALKu8sModeKind(mode_s);
            (void)sha1Drain(esha, &next_sha);
            found = YES;
            break;
        }
        if (!found || next_kind == 0) return notfound;

        //  Append to prefix pathbuf (caller may pass NULL to skip).
        if (pathbuf) {
            if (u8bDataLen(pathbuf) > 0) u8bFeed1(pathbuf, '/');
            u8bFeed(pathbuf, seg);
        }

        cur_sha = next_sha;
        cur_kind = next_kind;
    }

    *out_sha = cur_sha;
    *out_kind = cur_kind;
    done;
}

//  Descend a '/'-separated subpath from `root_tree`.  On success,
//  *out_sha/*out_kind describe the last resolved entry; *out_prefix
//  gets a slice into `pathbuf` holding the descended prefix (stable
//  until pathbuf is reused).
static ok64 lsf_descend(keeper *k, sha1cp root_tree, u8cs subpath,
                         u8bp pathbuf, sha1 *out_sha, u8 *out_kind) {
    sane(k && root_tree && out_sha && out_kind);
    (void)k;
    a_carve(u8, tbuf, 1UL << 20);
    call(KEEPTreeDescend, root_tree, subpath, pathbuf, out_sha, out_kind,
         tbuf, KEEPNONE);
    done;
}

ok64 KEEPLsFiles(uricp target, walk_tree_fn visit, void0p ctx) {
    sane(target && visit);
    keeper *k = &KEEP;

    //  1. Resolve URI to root tree SHA (commit→tree or tree→tree).
    sha1 root_tree = {};
    call(KEEPResolveTree, target, &root_tree);

    //  2. Descend /subpath (URI path, strip leading '/').
    a_pad(u8, prefix_buf, 4096);
    sha1 target_sha = root_tree;
    u8   target_kind = WALK_KIND_DIR;

    u8cs sub = {};
    //  When the URI has an authority, the `path` is the REMOTE-side
    //  repo path (e.g. `/tmp/sv-keep/src`), not a subtree inside the
    //  resolved tree — descending into it would always miss.  The
    //  in-repo subpath, when needed, belongs after a `.git/` split
    //  (see dog/DOG.md); we don't parse that here yet, so authority-
    //  bearing URIs always walk the full tree.
    if (u8csEmpty(target->authority)) {
        u8csMv(sub, target->path);
        //  "." means repo root, same as empty path.
        if (u8csLen(sub) == 1 && *sub[0] == '.') { sub[0] = sub[1]; }
    }
    call(lsf_descend, k, &root_tree, sub,
         prefix_buf, &target_sha, &target_kind);

    //  3. Dispatch: blob → one event; tree → full walk with prefix.
    a_dup(u8c, prefix_s, u8bData(prefix_buf));

    if (target_kind != WALK_KIND_DIR) {
        //  Leaf: emit a single visitor call with the accumulated path.
        u8cs blob = {};
        return visit(prefix_s, target_kind, target_sha.data, blob, ctx);
    }

    //  Tree: walk via WALKTreeLazy, wrapping the visitor to prepend
    //  `prefix_s` + '/' so paths remain absolute from the repo root.
    lsf_prefix_ctx pc = { .inner = visit, .inner_ctx = ctx, .prefix = {}};
    u8csMv(pc.prefix, prefix_s);
    return KEEPWalkTree(target_sha.data, NO, WALK_SKIP_ANCHOR,
                        lsf_prefix_visit, &pc);
}

//  URI → single blob.  Shares the resolve + descend machinery with
//  KEEPLsFiles; differs in that it requires a file leaf and writes its
//  body into the caller's buffer.
ok64 KEEPGetByURI(uricp target, u8bp out) {
    sane(target && out);
    keeper *k = &KEEP;

    //  Host-bearing URI: remote materialization.  Not wired yet — no
    //  policy for deciding what to pull on demand.  Fail loudly until
    //  that's resolved.
    if (!$empty(target->host)) fail(KEEPFAIL);

    //  No path AND no ?ref/#sha: nothing to resolve against — there is
    //  no tree to descend and no object to name.  Caller is expected to
    //  fall back to the filesystem.  A PATH-bearing URI with no ref
    //  (`blob:file.c` / `blob:file.c?`) is NOT this case: it resolves
    //  against cur's tip — KEEPResolveTree below maps the empty
    //  query+fragment to `h->cur_sha` (SUBS-015), matching `sha1:`.
    if ($empty(target->path) &&
        $empty(target->query) && $empty(target->fragment)) fail(KEEPFAIL);

    //  //?hash — raw blob by hash, no tree descent.  URI has empty
    //  authority and empty path; query is a hex SHA prefix.
    if ($empty(target->path) && !$empty(target->query)) {
        u8 btype = 0;
        u64 hashlet = WHIFFHexHashlet60(target->query);
        u8bReset(out);
        call(KEEPGet, hashlet, u8csLen(target->query), out, &btype);
        if (btype != DOG_OBJ_BLOB) fail(KEEPFAIL);
        done;
    }

    sha1 root_tree = {};
    call(KEEPResolveTree, target, &root_tree);

    a_pad(u8, prefix_buf, 4096);
    sha1 leaf_sha  = root_tree;
    u8   leaf_kind = WALK_KIND_DIR;

    u8cs sub = {};
    u8csMv(sub, target->path);
    if (u8csLen(sub) == 1 && *sub[0] == '.') { sub[0] = sub[1]; }
    call(lsf_descend, k, &root_tree, sub,
         prefix_buf, &leaf_sha, &leaf_kind);

    if (leaf_kind == WALK_KIND_DIR) fail(KEEPFAIL);

    u8 btype = 0;
    call(KEEPGetExact, &leaf_sha, out, &btype);
    if (btype != DOG_OBJ_BLOB) fail(KEEPNONE);
    done;
}

// --- KEEPTreeULog: emit leaves as ULOG rows -------------------------

typedef struct {
    u8bp  out;
    ron60 ts;
    ron60 verb;
    ok64  err;
} treeulog_ctx;

//  Map WALK_KIND_* to a single RON64 letter that appends to the
//  caller's verb stem: f=regular, x=executable, l=symlink,
//  s=submodule.  Returns 0 for kinds with no leaf row (DIR/unknown).
static u8 treeulog_kind_letter(u8 kind) {
    switch (kind) {
        case WALK_KIND_REG: return RON_f;
        case WALK_KIND_EXE: return RON_x;
        case WALK_KIND_LNK: return RON_l;
        case WALK_KIND_SUB: return RON_s;
        default:            return 0;
    }
}

static ok64 treeulog_visit(u8cs path, u8 kind, u8cp esha, u8cs blob,
                           void0p vctx) {
    (void)blob;
    treeulog_ctx *c = (treeulog_ctx *)vctx;

    //  TODO(delta-trees): emit a `dir`-kind row for WALK_KIND_DIR with
    //  the subtree SHA in the fragment so sniff/POST.c:post_build_tree
    //  can pick up the parent commit's same-path tree SHA per prefix
    //  and pass it as KEEPPackFeed's base_hashlet60 (currently 0 at
    //  POST.c:2310 — see TODO there).  Today we only emit leaf rows,
    //  which is why the on-write delta path stays unused for trees.
    if (kind == WALK_KIND_DIR) return OK;       // root + subtrees skipped
    u8 kletter = treeulog_kind_letter(kind);
    if (kletter == 0) return OK;                // unknown kind, skip

    //  Hex-encode the 20-byte leaf sha into a stack buffer used as the
    //  fragment slice.
    sha1 leaf = {};
    u8cs leaf_bin = {esha, esha + 20};
    sha1FromBin(&leaf, leaf_bin);
    a_sha1hex(hex_view, &leaf);

    uri u = {};
    u8csMv(u.path, path);
    u8csMv(u.fragment, hex_view);

    ulogrec rec = {.ts   = c->ts,
                   .verb = ok64sub(c->verb, kletter),
                   .uri  = u};
    ok64 o = ULOGu8sFeed(u8bIdle(c->out), &rec);
    if (o != OK) { c->err = o; return WALKSTOP; }

    //  Submodule entries are leaves but not recursable.
    if (kind == WALK_KIND_SUB) return WALKSKIP;
    return OK;
}

ok64 KEEPTreeULog(u8cp tree_sha,
                  ron60 ts, ron60 verb, u8bp out) {
    sane(tree_sha && out);
    u8bReset(out);
    treeulog_ctx c = {.out = out, .ts = ts, .verb = verb, .err = OK};
    call(WALKTreeLazy, tree_sha, treeulog_visit, &c);
    return c.err;
}

// --- KEEPTreeDiff: tree-vs-tree as a diff ULOG ----------------------
//
//  Build two side-tagged ULOGs via KEEPTreeULog, merge them through
//  ULOGMergeWalk grouped by path, and emit add/del/mod rows into the
//  caller's `out` buffer.  ULOG row layout matches POST's decision-
//  log shape so downstream consumers stay uniform.

typedef struct {
    u8bp  out;
    ron60 v_add;
    ron60 v_del;
    ron60 v_mod;
    ron60 v_a;          // side-tag for the `sha_a` cursor
    ron60 v_b;          // side-tag for the `sha_b` cursor
    ok64  err;
} treediff_ctx;

//  Append a diff row to `out`.  `verb_stem` is one of v_add / v_del /
//  v_mod; we preserve the kind letter from the source row so callers
//  can recover (mode, kind) downstream.
static ok64 treediff_emit(treediff_ctx *c, ron60 verb_stem,
                          ulogreccp src,
                          u8cs old_hex, u8cs new_hex) {
    sane(c && src);
    //  Kind letter rides in the bottom RON digit of the source verb;
    //  re-attach it to the diff verb stem.
    u8 kletter = (u8)ok64Lit(src->verb, 0);
    ron60 verb = (kletter != 0) ? ok64sub(verb_stem, kletter)
                                 : verb_stem;

    uri u = {};
    u8csMv(u.path, src->uri.path);
    if (!u8csEmpty(old_hex)) u8csMv(u.query,    old_hex);
    if (!u8csEmpty(new_hex)) u8csMv(u.fragment, new_hex);

    ulogrec rec = {.ts = 0, .verb = verb, .uri = u};
    return ULOGu8sFeed(u8bIdle(c->out), &rec);
}

static ok64 treediff_step(ulogreccp recs, u32 n, void *ctx) {
    treediff_ctx *c = (treediff_ctx *)ctx;
    sane(c && recs && n > 0);

    //  Identify A / B rows in the tie group by side-tag stem.
    ulogreccp a = NULL;
    ulogreccp b = NULL;
    for (u32 i = 0; i < n; i++) {
        ron60 stem = ok64stem(recs[i].verb);
        if      (stem == c->v_a && a == NULL) a = &recs[i];
        else if (stem == c->v_b && b == NULL) b = &recs[i];
    }

    u8cs empty = {};
    ok64 fo = OK;
    if (a == NULL && b != NULL) {
        u8cs nh = {b->uri.fragment[0], b->uri.fragment[1]};
        fo = treediff_emit(c, c->v_add, b, empty, nh);
    } else if (a != NULL && b == NULL) {
        u8cs oh = {a->uri.fragment[0], a->uri.fragment[1]};
        fo = treediff_emit(c, c->v_del, a, empty, oh);
    } else if (a != NULL && b != NULL) {
        u8cs oh = {a->uri.fragment[0], a->uri.fragment[1]};
        u8cs nh = {b->uri.fragment[0], b->uri.fragment[1]};
        //  Equal-and-same: same kind letter on both verbs AND same
        //  fragment (leaf sha) → no row.  Anything else is `mod`.
        b8 kind_eq = (ok64Lit(a->verb, 0) == ok64Lit(b->verb, 0));
        b8 sha_eq  = ($len(oh) == $len(nh)) &&
                     (u8csEmpty(oh) ||
                      memcmp(oh[0], nh[0], (size_t)$len(oh)) == 0);
        if (kind_eq && sha_eq) return OK;
        //  Use B's kind letter on the `mod` row (the new state wins).
        fo = treediff_emit(c, c->v_mod, b, oh, nh);
    }
    if (fo != OK) c->err = fo;
    return fo;
}

ok64 KEEPTreeDiff(u8cp sha_a, u8cp sha_b, u8bp out) {
    sane(out);
    u8bReset(out);

    //  RON-encode side tags + output verbs once.
    a_cstr(s_a,   "a");   a_dup(u8c, dva,   s_a);
    a_cstr(s_b,   "b");   a_dup(u8c, dvb,   s_b);
    a_cstr(s_add, "add"); a_dup(u8c, dvadd, s_add);
    a_cstr(s_del, "del"); a_dup(u8c, dvdel, s_del);
    a_cstr(s_mod, "mod"); a_dup(u8c, dvmod, s_mod);
    ron60 v_a = 0, v_b = 0, v_add = 0, v_del = 0, v_mod = 0;
    call(RONutf8sDrain, &v_a,   dva);
    call(RONutf8sDrain, &v_b,   dvb);
    call(RONutf8sDrain, &v_add, dvadd);
    call(RONutf8sDrain, &v_del, dvdel);
    call(RONutf8sDrain, &v_mod, dvmod);

    a_carve(u8, ula, 1UL << 20);
    a_carve(u8, ulb, 1UL << 20);

    if (sha_a != NULL) call(KEEPTreeULog, sha_a, 0, v_a, ula);
    if (sha_b != NULL) call(KEEPTreeULog, sha_b, 0, v_b, ulb);

    //  Build the cursor array — two slices over the row buffers.
    u8cs cur[2] = {};
    u8csMv(cur[0], u8bDataC(ula));
    u8csMv(cur[1], u8bDataC(ulb));
    u8css cursors = {cur, cur + 2};

    treediff_ctx ctx = {
        .out = out, .v_add = v_add, .v_del = v_del, .v_mod = v_mod,
        .v_a = v_a, .v_b = v_b, .err = OK,
    };
    ok64 mo = ULOGMergeWalk(cursors, treediff_step, &ctx);
    return (mo != OK) ? mo : ctx.err;
}

// --- Range banner renderers (commit list + per-file diff) ----------
//
//  See WALK.h: render the "what moved" banner shared by GET / POST /
//  PATCH.  Best-effort throughout — a bad object skips, never aborts.

#define KEEP_BANNER_SUBJ_MAX 120

//  `post` status verb, RON-encoded once (mirrors SNIFFAtVerbPost
//  without pulling the sniff layer down into keeper).
static ron60 keep_v_post(void) {
    static ron60 v = 0;
    if (v == 0) { a_cstr(s, "post"); a_dup(u8c, d, s); (void)RONutf8sDrain(&v, d); }
    return v;
}

ok64 KEEPEmitCommitLine(sha1cp commit_sha, ron60 fallback_ts) {
    sane(commit_sha);
    a_carve(u8, cbuf, 1UL << 20);
    u8 ot = 0;
    if (KEEPGetExact(commit_sha, cbuf, &ot) != OK || ot != KEEP_OBJ_COMMIT)
        done;
    git_commit gc = {};
    a_dup(u8c, body, u8bData(cbuf));
    GITu8sParseCommit(body, &gc);

    //  hashlet8 = first SHA1_HASHLEN_LEN hex of the commit sha.
    a_pad(u8, qbuf, SHA1_HASHLEN_LEN);
    u8 *q0 = qbuf_idle[0];
    if (SHA1u8sFeedHashlet(qbuf_idle, commit_sha) != OK) done;
    u8cs hashlet = {q0, q0 + SHA1_HASHLEN_LEN};

    u8cs subj = {gc.subject[0], gc.subject[1]};
    if ($len(subj) > KEEP_BANNER_SUBJ_MAX)
        subj[1] = subj[0] + KEEP_BANNER_SUBJ_MAX;   // fixed-length cap

    ulogrec rep = {.ts   = gc.author_ts ? gc.author_ts : fallback_ts,
                   .verb = keep_v_post()};
    u8csMv(rep.uri.query, hashlet);
    rep.uri.fragment[0] = subj[0];
    rep.uri.fragment[1] = subj[1];
    //  Range row carries the sha in `.query`, subject in `.fragment`;
    //  ROWS_NAV_COMMIT renders the subject as the clickable path with a
    //  hidden `commit:?<sha>` nav (COMMIT-001).
    (void)ROWSPrintRow(&rep, ROWS_NAV_COMMIT);
    done;
}

ok64 KEEPEmitTreeDiffFiles(sha1cp base_commit, sha1cp tgt_commit,
                           ron60 fallback_ts) {
    sane(tgt_commit);
    sha1 tree_a = {}, tree_b = {};
    b8   have_a = NO;

    if (base_commit) {
        a_carve(u8, ba, 1UL << 20);
        u8 t = 0;
        if (KEEPGetExact(base_commit, ba, &t) == OK && t == KEEP_OBJ_COMMIT) {
            a_dup(u8c, body, u8bData(ba));
            if (GITu8sCommitTree(body, tree_a.data) == OK) have_a = YES;
        }
    }
    {
        a_carve(u8, bb, 1UL << 20);
        u8 t = 0;
        if (KEEPGetExact(tgt_commit, bb, &t) != OK || t != KEEP_OBJ_COMMIT)
            done;
        a_dup(u8c, body, u8bData(bb));
        if (GITu8sCommitTree(body, tree_b.data) != OK) done;
    }

    a_carve(u8, diff, 1UL << 22);
    call(KEEPTreeDiff, have_a ? tree_a.data : NULL, tree_b.data, diff);

    //  Re-emit each diff row as a clean `<verb>\t<path>` status line:
    //  drop KEEPTreeDiff's sha query/fragment, strip the kind letter
    //  via ok64stem so the bare add/del/mod verb hits the palette.
    a_dup(u8c, scan, u8bDataC(diff));
    for (;;) {
        ulogrec rec = {};
        ok64 dr = ULOGu8sDrain(scan, &rec);
        if (dr == NODATA) break;
        if (dr != OK) continue;
        ulogrec rep = {.ts = fallback_ts, .verb = ok64stem(rec.verb)};
        u8csMv(rep.uri.path, rec.uri.path);
        (void)ROWSPrintRow(&rep, ROWS_NAV_CAT);
    }
    done;
}

//  Commit-range banner via a direct keeper walk (no graf — the DAG's
//  in-process ingest batch isn't flushed mid-command, so its runs
//  can't be trusted here; see graf/INDEX.c GRAFIndexFromTips).  This
//  is what makes PATCH's banner ancestor-skip-correct.

#define KEEP_RANGE_CAP (1u << 14)   // commit-set / range bound (best-effort)

static b8 keep_sha_in(sha1 const *set, u32 n, sha1cp q) {
    for (u32 i = 0; i < n; i++)
        if (sha1Eq(&set[i], q)) return YES;
    return NO;
}

//  40-hex commit-header value → sha1 is dog/WHIFF.h `sha1FromHex`.

//  Iterative reachable-commit closure of `root` over `parent` (and,
//  when `with_foster`, `foster`) headers into `set` (deduped, capped).
//  One reused 1 MB commit buffer; worklist on BASS so deep history
//  doesn't recurse.  Best-effort — cap / read error just truncates.
static ok64 keep_commit_reach(sha1cp root, b8 with_foster,
                              sha1 *set, u32 *n, u32 cap) {
    sane(root && set && n);
    a_carve(sha1, wl_b, cap);
    sha1 *wl = sha1bDataHead(wl_b);
    u32 wn = 0;
    wl[wn++] = *root;
    a_carve(u8, cbuf, 1UL << 20);
    while (wn > 0) {
        sha1 cur = wl[--wn];
        if (keep_sha_in(set, *n, &cur)) continue;
        if (*n >= cap) break;
        u8bReset(cbuf);
        u8 ct = 0;
        if (KEEPGetExact(&cur, cbuf, &ct) != OK || ct != KEEP_OBJ_COMMIT)
            continue;
        set[(*n)++] = cur;
        u8cs body = {u8bDataHead(cbuf), u8bIdleHead(cbuf)};
        u8cs field = {}, value = {};
        while (GITu8sDrainCommit(body, field, value) == OK) {
            if ($empty(field)) break;
            b8 par = u8csEq(field, GIT_FIELD_PARENT);
            b8 fos = with_foster && u8csEq(field, GIT_FIELD_FOSTER);
            if (!par && !fos) continue;
            sha1 p = {};
            if (sha1FromHex(&p, value) == OK && wn < cap) wl[wn++] = p;
        }
    }
    done;
}

//  Walk `tip`'s parent closure, pruning at any commit already in
//  `base` (= the absorbed-already set), collecting the rest into `out`
//  pre-order (≈ newest-first).  Foster on `tip` is intentionally not
//  followed — `tip`'s mainline is the absorbed stack.
static ok64 keep_commit_collect_since(sha1cp tip, sha1 const *base, u32 nbase,
                                      sha1 *out, u32 *nout, u32 cap) {
    sane(tip && out && nout);
    a_carve(sha1, wl_b, cap);
    sha1 *wl = sha1bDataHead(wl_b);
    u32 wn = 0;
    wl[wn++] = *tip;
    a_carve(u8, cbuf, 1UL << 20);
    while (wn > 0) {
        sha1 cur = wl[--wn];
        if (keep_sha_in(base, nbase, &cur)) continue;   // already absorbed
        if (keep_sha_in(out, *nout, &cur)) continue;     // dedup
        if (*nout >= cap) break;
        u8bReset(cbuf);
        u8 ct = 0;
        if (KEEPGetExact(&cur, cbuf, &ct) != OK || ct != KEEP_OBJ_COMMIT)
            continue;
        out[(*nout)++] = cur;
        u8cs body = {u8bDataHead(cbuf), u8bIdleHead(cbuf)};
        u8cs field = {}, value = {};
        while (GITu8sDrainCommit(body, field, value) == OK) {
            if ($empty(field)) break;
            if (u8csEq(field, GIT_FIELD_PARENT)) {
                sha1 p = {};
                if (sha1FromHex(&p, value) == OK && wn < cap) wl[wn++] = p;
            }
        }
    }
    done;
}

ok64 KEEPEmitCommitsSince(sha1cp base, sha1cp tip, ron60 fallback_ts) {
    sane(tip);
    a_carve(sha1, base_b, KEEP_RANGE_CAP);
    sha1 *baseset = sha1bDataHead(base_b);
    u32   nbase = 0;
    //  Base reach follows parent AND foster — a wt that absorbed a
    //  sub-branch via `foster` already holds those commits, so they
    //  must be pruned from the new range (ancestor-skip).
    if (base) call(keep_commit_reach, base, YES, baseset, &nbase,
                   KEEP_RANGE_CAP);

    a_carve(sha1, out_b, KEEP_RANGE_CAP);
    sha1 *out = sha1bDataHead(out_b);
    u32   nout = 0;
    call(keep_commit_collect_since, tip, baseset, nbase, out, &nout,
         KEEP_RANGE_CAP);

    for (u32 i = 0; i < nout; i++)
        call(KEEPEmitCommitLine, &out[i], fallback_ts);
    done;
}
