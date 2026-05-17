//  KEEPMoveCommits: see keeper/KEEP.h for the contract.
//
//  Cross-shard pack migration with delta-base hints.  Caller has
//  KEEPSwitchBranch'd to the destination leaf; source packs sit in
//  PAST.  For each commit in the supplied slice we open a fresh pack
//  in the active leaf, emit the commit body + its tree (recursive)
//  + every leaf blob, with delta-base hints from the parent commit's
//  same-path predecessor.

#include <string.h>

#include "abc/B.h"
#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "dog/DOG.h"
#include "dog/HOME.h"
#include "dog/WHIFF.h"

#include "dog/git/GIT.h"
#include "KEEP.h"

#define MIG_BUF (16UL << 20)        // 16 MB per object body scratch

//  Forward decl: tree-emit recurses through subdirectories and is
//  called from the commit emit loop.
static ok64 mig_emit_tree(keep_pack *p, sha1cp new_tree, sha1cp old_tree);

//  Extract `tree` + first `parent` sha from a commit body.  `body`
//  is the canonical commit object content (KEEPGetExact's output
//  for DOG_OBJ_COMMIT).  `*has_parent` set NO on root commits.
//  Uses GITu8sDrainCommit — no manual byte parsing.
static ok64 mig_commit_fields(u8cs body, sha1p tree_out,
                              sha1p parent_out, b8 *has_parent) {
    sane(tree_out && parent_out && has_parent);
    *has_parent = NO;
    sha1Zero(tree_out);
    sha1Zero(parent_out);

    a_dup(u8c, scan, body);
    u8cs field = {}, value = {};
    b8 got_tree = NO;
    while (GITu8sDrainCommit(scan, field, value) == OK) {
        if ($empty(field)) break;
        a_cstr(tree_kw, "tree");
        a_cstr(par_kw,  "parent");
        if (u8csEq(field, tree_kw) && u8csLen(value) >= 40) {
            u8cs hex = {value[0], value[0] + 40};
            u8s  bin = {tree_out->data, tree_out->data + 20};
            call(HEXu8sDrainSome, bin, hex);
            got_tree = YES;
            continue;
        }
        if (u8csEq(field, par_kw) && u8csLen(value) >= 40 && !*has_parent) {
            u8cs hex = {value[0], value[0] + 40};
            u8s  bin = {parent_out->data, parent_out->data + 20};
            call(HEXu8sDrainSome, bin, hex);
            *has_parent = YES;
            continue;
        }
    }
    if (!got_tree) fail(KEEPMVFAIL);
    done;
}

//  Look up an entry's sha by name in a parsed-on-the-fly tree body.
//  Returns OK with `*out` populated when found, KEEPNONE on miss.
static ok64 mig_tree_find(u8cs body, u8cs name, sha1p out) {
    sane(out);
    a_dup(u8c, scan, body);
    u8cs field = {}, esha = {};
    while (GITu8sDrainTree(scan, field, esha, NULL) == OK) {
        u8cs ename = {};
        if (GITu8sFileSplit(field, NULL, ename) != OK) continue;
        if (!u8csEq(ename, name)) continue;
        return sha1FromBin(out, esha);
    }
    return KEEPNONE;
}

static ok64 mig_emit_blob(keep_pack *p,
                          sha1cp new_blob, sha1cp old_blob) {
    sane(p && new_blob);
    Bu8 buf = {};
    call(u8bMap, buf, MIG_BUF);
    u8 type = 0;
    ok64 ko = KEEPGetExact(&KEEP, (sha1p)new_blob, buf, &type);
    if (ko != OK) {
        //  Object not in any open pack.  Don't silently skip — the
        //  target shard ends up missing bytes its REFS row claims to
        //  cover, and the next `be get ?<target>` produces a wt full
        //  of "new" entries because tree resolution dead-ends here.
        //  Surface the missing sha so the caller can `be get <sha>`
        //  from upstream (or refuse the FF until history is filled).
        a_pad(u8, hex, 40);
        a_rawc(sb, *(sha1p)new_blob);
        HEXu8sFeedSome(hex_idle, sb);
        fprintf(stderr,
            "sniff: post: migration missing blob %.*s — "
            "run `be get` from upstream first\n",
            (int)u8bDataLen(hex), (char *)u8bDataHead(hex));
        u8bUnMap(buf);
        return KEEPMVFAIL;
    }

    a_dup(u8c, body, u8bData(buf));
    u64 hint = (old_blob && !sha1empty(old_blob))
                ? WHIFFHashlet60(old_blob) : 0;
    sha1 verify = {};
    ok64 fo = KEEPPackFeed(&KEEP, p, type, body, hint, &verify);
    u8bUnMap(buf);
    return fo;
}

static ok64 mig_emit_tree(keep_pack *p,
                          sha1cp new_tree, sha1cp old_tree) {
    sane(p && new_tree);

    Bu8 new_buf = {};
    call(u8bMap, new_buf, MIG_BUF);
    u8 nt = 0;
    ok64 ko = KEEPGetExact(&KEEP, (sha1p)new_tree, new_buf, &nt);
    if (ko != OK) {
        //  Tree missing from any open pack.  Same hazard as mig_emit_
        //  blob: silently skipping leaves the target shard incomplete.
        //  Surface the sha so the caller can hydrate from upstream.
        a_pad(u8, hex, 40);
        a_rawc(sb, *(sha1p)new_tree);
        HEXu8sFeedSome(hex_idle, sb);
        fprintf(stderr,
            "sniff: post: migration missing tree %.*s — "
            "run `be get` from upstream first\n",
            (int)u8bDataLen(hex), (char *)u8bDataHead(hex));
        u8bUnMap(new_buf);
        return KEEPMVFAIL;
    }
    if (nt != DOG_OBJ_TREE) { u8bUnMap(new_buf); fail(KEEPMVFAIL); }

    //  Optional same-path predecessor tree, for per-entry hints.
    Bu8 old_buf = {};
    b8  have_old = NO;
    if (old_tree && !sha1empty(old_tree)) {
        if (u8bMap(old_buf, MIG_BUF) == OK) {
            u8 ot = 0;
            if (KEEPGetExact(&KEEP, (sha1p)old_tree, old_buf, &ot) == OK
                && ot == DOG_OBJ_TREE) {
                have_old = YES;
            }
        }
    }

    //  Emit the tree object itself, hinted by the old tree.
    a_dup(u8c, new_body, u8bData(new_buf));
    u64 tree_hint = (old_tree && !sha1empty(old_tree))
                     ? WHIFFHashlet60(old_tree) : 0;
    sha1 tverify = {};
    ok64 fo = KEEPPackFeed(&KEEP, p, DOG_OBJ_TREE, new_body,
                           tree_hint, &tverify);
    if (fo != OK) {
        if (have_old) u8bUnMap(old_buf);
        u8bUnMap(new_buf); return fo;
    }

    //  Walk entries.  Recurse into subdirs; emit blobs at leaves.
    a_dup(u8c, walk, u8bData(new_buf));
    u8cs field = {}, esha = {};
    while (GITu8sDrainTree(walk, field, esha, NULL) == OK) {
        u8cs mode = {}, name = {};
        if (GITu8sFileSplit(field, mode, name) != OK) continue;
        if ($empty(name)) continue;

        sha1 entry = {};
        if (sha1FromBin(&entry, esha) != OK) continue;

        //  Same-name predecessor lookup (for delta-base hints).
        sha1 old_entry = {};
        b8 has_old_entry = NO;
        if (have_old) {
            a_dup(u8c, old_body_v, u8bData(old_buf));
            if (mig_tree_find(old_body_v, name, &old_entry) == OK) {
                has_old_entry = YES;
            }
        }

        b8 is_dir = ($len(mode) > 0 && *mode[0] == '4');
        ok64 sub = is_dir
            ? mig_emit_tree(p, &entry,
                            has_old_entry ? &old_entry : NULL)
            : mig_emit_blob(p, &entry,
                            has_old_entry ? &old_entry : NULL);
        if (sub != OK) {
            if (have_old) u8bUnMap(old_buf);
            u8bUnMap(new_buf); return sub;
        }
    }

    if (have_old) u8bUnMap(old_buf);
    u8bUnMap(new_buf);
    done;
}

//  Scan every shard dir under `<root>/.be/` and register its packs.
//  Used as a safety net for the common cross-shard-reference issue:
//  POST emits commits that reference trees/blobs living in a
//  different (sibling) shard.  Until POST learns to copy referenced
//  objects into the active shard at write time, migration has to
//  scan widely or lookups will hit KEEPNONE.
//
//  Idempotent — DOGPupOpenAll skips already-registered seqnos.
//  Recursive (FILE_SCAN_DEEP) so nested branches participate.
typedef struct { int n; } mig_scan_ctx;
static ok64 mig_scan_cb(void0p arg, path8p path) {
    sane(arg && path);
    (void)arg;
    u8cs dir = {path[0], path[1]};
    a_cstr(pack_ext, KEEP_PACK_EXT);
    (void)DOGPupOpenAll(KEEP.packs,   dir, pack_ext);
    a_cstr(idx_ext, KEEP_IDX_EXT);
    (void)DOGPupOpenAll(KEEP.puppies, dir, idx_ext);
    return OK;
}
static void mig_scan_all_shards(void) {
    if (KEEP.h == NULL) return;
    a_path(bedir, u8bDataC(KEEP.h->root), KEEP_DIR_S);
    a_pad(u8, scratch, 64 * 1024);
    mig_scan_ctx ctx = {0};
    (void)FILEScanSorted(bedir,
                         FILE_SCAN_DEEP | FILE_SCAN_DIRS,
                         scratch, FILEentryZ,
                         mig_scan_cb, &ctx);
}

//  Idempotent registration of a source shard's packs in PAST so this
//  function can fetch source-only objects via KEEPGetExact.  No-op
//  when the dir's already registered (DOGPupOpenAll skips known
//  seqnos).  Empty branch → trunk (root dir).
static void mig_open_source(path8sc src_branch) {
    if (KEEP.h == NULL) return;
    a_path(dir, u8bDataC(KEEP.h->root), KEEP_DIR_S);
    if (!u8csEmpty(src_branch)) {
        if (PATHu8bAdd(dir, src_branch) != OK) return;
    }
    a_dup(u8c, dir_s, u8bDataC(dir));
    a_cstr(pack_ext, KEEP_PACK_EXT);
    (void)DOGPupOpenAll(KEEP.packs,   dir_s, pack_ext);
    a_cstr(idx_ext, KEEP_IDX_EXT);
    (void)DOGPupOpenAll(KEEP.puppies, dir_s, idx_ext);
}

ok64 KEEPMoveCommits(sha1cs commits, path8sc src_branch) {
    sane($ok(commits));
    if ($empty(commits)) return KEEPMVNOOP;

    //  Ensure the source shard's packs are registered as PAST so
    //  KEEPGetExact can find source-only objects.  Also scan every
    //  other shard — POST may have emitted commits in src that
    //  reference trees/blobs living in sibling shards (a known
    //  cross-shard-reference issue).
    mig_open_source(src_branch);
    mig_scan_all_shards();

    //  One pack for the whole batch.  strict_order=NO — we emit
    //  commit-then-tree-then-blob per commit; that ordering doesn't
    //  match the file-level commit/tree/blob/tag invariant.
    keep_pack p = {};
    call(KEEPPackOpen, &KEEP, &p);
    p.strict_order = NO;

    ok64 ret = OK;
    sha1cp prev_tree = NULL;
    sha1 prev_tree_storage = {};
    sha1cp prev_commit = NULL;
    sha1 prev_commit_storage = {};

    $for(sha1c, c, commits) {
        if (ret != OK) break;

        //  Fetch commit body, extract its tree (+ first parent).
        Bu8 cbuf = {};
        ok64 mo = u8bMap(cbuf, MIG_BUF);
        if (mo != OK) { ret = mo; break; }
        u8 ct = 0;
        ok64 ko = KEEPGetExact(&KEEP, (sha1p)c, cbuf, &ct);
        if (ko != OK) { u8bUnMap(cbuf); ret = ko; break; }
        if (ct != DOG_OBJ_COMMIT) {
            u8bUnMap(cbuf); ret = KEEPMVFAIL; break;
        }
        a_dup(u8c, body, u8bData(cbuf));

        sha1 ctree = {}, cparent = {};
        b8 has_parent = NO;
        ok64 pf = mig_commit_fields(body, &ctree, &cparent, &has_parent);
        if (pf != OK) { u8bUnMap(cbuf); ret = pf; break; }

        //  Tree migration first (delta-base = the previous commit's
        //  tree we just emitted, if any — that one's already in
        //  PAST/DATA and gives the densest delta).
        ok64 te = mig_emit_tree(&p, &ctree, prev_tree);
        if (te != OK) { u8bUnMap(cbuf); ret = te; break; }

        //  Now feed the commit body.  Hint = previous commit (same
        //  reasoning as tree above).
        u64 chint = prev_commit ? WHIFFHashlet60(prev_commit)
                  : has_parent ? WHIFFHashlet60(&cparent) : 0;
        a_dup(u8c, body2, u8bData(cbuf));
        sha1 verify = {};
        ok64 fo = KEEPPackFeed(&KEEP, &p, DOG_OBJ_COMMIT, body2,
                               chint, &verify);
        u8bUnMap(cbuf);
        if (fo != OK) { ret = fo; break; }

        //  Stage hints for next iter.
        prev_tree_storage = ctree;
        prev_tree = &prev_tree_storage;
        prev_commit_storage = *c;
        prev_commit = &prev_commit_storage;
    }

    ok64 cl = KEEPPackClose(&KEEP, &p);
    return ret == OK ? cl : ret;
}
