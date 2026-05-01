//  URI-driven diff: file or whole tree, ref vs wt or ref vs ref.
//  Companion to DIFF.c (file-pair) and BLAME.c (weave).
//
#include "GRAF.h"

#include <stdio.h>
#include <string.h>

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/URI.h"
#include "dog/DOG.h"
#include "dog/HUNK.h"
#include "graf/TDIFF.h"
#include "keeper/KEEP.h"
#include "keeper/WALK.h"

#define DIFFREF_MAX_FILES 512
#define DIFFREF_PATH_MAX  256

typedef struct {
    char path[DIFFREF_PATH_MAX];
    u16  path_len;
    u8   sha[20];
} diffref_entry;

typedef struct {
    diffref_entry *v;
    u32            n;
    u32            cap;
    u32            overflow;
} diffref_set;

static ok64 diffref_set_push(diffref_set *s, u8cs path, u8cp esha) {
    sane(s);
    if (s->n >= s->cap) { s->overflow++; done; }
    size_t plen = (size_t)$len(path);
    if (plen == 0 || plen >= DIFFREF_PATH_MAX) { s->overflow++; done; }
    diffref_entry *e = &s->v[s->n++];
    memcpy(e->path, path[0], plen);
    e->path[plen] = 0;
    e->path_len = (u16)plen;
    memcpy(e->sha, esha, 20);
    done;
}

//  Compose `<lead><ref>` into `ubuf` where `lead` is `#` for a bare
//  40-hex sha, `?` otherwise.  KEEPResolveTree's fragment branch is the
//  fast path for 40-hex; the query branch goes through REFS which has
//  no bare-sha entries — so we have to dispatch here.
static b8 ref_is_hex40(u8cs ref) {
    if ($len(ref) != 40) return NO;
    for (u8cp p = ref[0]; p < ref[1]; p++) {
        u8 c = *p;
        b8 d = (c >= '0' && c <= '9');
        b8 lo = (c >= 'a' && c <= 'f');
        b8 up = (c >= 'A' && c <= 'F');
        if (!d && !lo && !up) return NO;
    }
    return YES;
}

static ok64 diffref_compose_ref_uri(u8bp ubuf, u8cs ref) {
    sane(ubuf);
    u8 lead = ref_is_hex40(ref) ? '#' : '?';
    call(u8bFeed1, ubuf, lead);
    call(u8bFeed,  ubuf, ref);
    done;
}

static diffref_entry *diffref_set_find(diffref_set *s, u8cs path) {
    size_t plen = (size_t)$len(path);
    for (u32 i = 0; i < s->n; i++) {
        if (s->v[i].path_len != plen) continue;
        if (memcmp(s->v[i].path, path[0], plen) == 0) return &s->v[i];
    }
    return NULL;
}

// --- Shared: emit one diff hunk block from two byte ranges --------

static ok64 diffref_emit_pair(u8cs old_data, u8cs new_data, u8cs name) {
    sane($ok(name));

    // No change: skip (both empty is handled by DIFFu8cs too but
    // cheaper to bail early).
    if ($len(old_data) == $len(new_data) &&
        ($len(old_data) == 0 ||
         memcmp(old_data[0], new_data[0], (size_t)$len(old_data)) == 0)) {
        done;
    }

    u8cs ext_nodot = {};
    PATHu8sExt(ext_nodot, name);

    char dispname[DIFFREF_PATH_MAX];
    size_t dlen = (size_t)$len(name);
    if (dlen >= sizeof(dispname)) dlen = sizeof(dispname) - 1;
    memcpy(dispname, name[0], dlen);
    dispname[dlen] = 0;

    call(GRAFArenaInit);
    ok64 o = DIFFu8cs(graf_arena, old_data, new_data, ext_nodot,
                      dispname, GRAFHunkEmit, NULL);
    GRAFArenaCleanup();
    return o;
}

// --- Shared: load blob at ref+path via KEEPGetByURI ---------------

static ok64 diffref_load_blob(Bu8 out, keeper *k, u8cs path, u8cs ref) {
    sane(k);
    a_pad(u8, ubuf, DIFFREF_PATH_MAX + 128);
    if (!$empty(path)) call(u8bFeed, ubuf, path);
    call(diffref_compose_ref_uri, ubuf, ref);
    a_dup(u8c, udata, u8bData(ubuf));
    uri target = {};
    call(URIutf8Drain, udata, &target);
    u8bReset(out);
    call(KEEPGetByURI, k, &target, out);
    done;
}

// --- Shared: mmap wt file (ok to fail with FILEOPEN → empty) ------

static ok64 diffref_load_wt(u8bp *mapped, u8cs out_data,
                             u8cs reporoot, u8cs path) {
    sane(mapped);
    a_path(fp, reporoot, path);
    ok64 o = FILEMapRO(mapped, $path(fp));
    if (o != OK) { out_data[0] = NULL; out_data[1] = NULL; return o; }
    out_data[0] = u8bDataHead(*mapped);
    out_data[1] = u8bIdleHead(*mapped);
    done;
}

// --- wt-vs-base diff: weave-based ---------------------------------
//
//  `wt` is the next version after the base sha.  Build the file's
//  weave with `tip_h = base_h40`, fold the wt bytes as a final layer
//  tagged `WEAVE_WT_SRC`, then walk the weave: tokens introduced by
//  wt are 'I', tokens removed by wt are 'D', survivors are context.

static b8 wt_in_from(u32 c, void *ctx) {
    (void)ctx;
    return c != WEAVE_WT_SRC;
}
static b8 wt_in_to(u32 c, void *ctx) {
    (void)ctx; (void)c;
    return YES;
}

ok64 GRAFDiffWtFile(keeper *k, u8cs filepath, u64 base_h40, u8cs reporoot) {
    sane(k && $ok(filepath) && $ok(reporoot));

    call(GRAFArenaInit);

    weave wA = {}, wB = {}, wnu = {};
    call(WEAVEInit, &wA);
    call(WEAVEInit, &wB);
    call(WEAVEInit, &wnu);

    weave *final = NULL;
    ok64 fwo = GRAFFileWeave(&wA, &wB, &wnu, &final, k, filepath, base_h40,
                              reporoot, WEAVE_WT_SRC, NULL, NULL);
    ok64 ret = OK;
    if (fwo == OK && final) {
        ret = WEAVEEmitDiff(final, filepath,
                            wt_in_from, NULL,
                            wt_in_to,   NULL,
                            GRAFHunkEmit, NULL);
    } else if (fwo != OK) {
        ret = fwo;
    }

    WEAVEFree(&wA);
    WEAVEFree(&wB);
    WEAVEFree(&wnu);
    GRAFArenaCleanup();
    return ret;
}

// --- Whole tree, wt vs base ---------------------------------------
//
//  Walks the base tree, collecting (path, blob_sha) pairs.  Then
//  per-file weave diff against the wt.  Files only present in the wt
//  (untracked additions) are not yet emitted — same TODO as before.

typedef struct {
    diffref_set *set;
} diffref_collect_ctx;

static ok64 diffref_collect_visit(u8cs path, u8 kind, u8cp esha,
                                   u8cs blob, void0p ctx) {
    (void)blob;
    diffref_collect_ctx *c = (diffref_collect_ctx *)ctx;
    if (kind == WALK_KIND_REG || kind == WALK_KIND_EXE ||
        kind == WALK_KIND_LNK) {
        diffref_set_push(c->set, path, esha);
    }
    return OK;
}

ok64 GRAFDiffWtTree(keeper *k, u64 base_h40, u8cs base_hex, u8cs reporoot) {
    sane(k && $ok(base_hex) && $ok(reporoot));

    a_pad(u8, ubuf, 256);
    call(diffref_compose_ref_uri, ubuf, base_hex);
    a_dup(u8c, udata, u8bData(ubuf));
    uri target = {};
    call(URIutf8Drain, udata, &target);

    diffref_entry entries[DIFFREF_MAX_FILES];
    diffref_set set = {.v = entries, .cap = DIFFREF_MAX_FILES};
    diffref_collect_ctx cctx = {.set = &set};
    call(KEEPLsFiles, k, &target, diffref_collect_visit, &cctx);

    if (set.overflow) {
        fprintf(stderr, "graf: diff-tree: %u files skipped (>%u limit)\n",
                set.overflow, (u32)DIFFREF_MAX_FILES);
    }

    for (u32 i = 0; i < set.n; i++) {
        u8cs path = {(u8cp)set.v[i].path,
                     (u8cp)set.v[i].path + set.v[i].path_len};
        GRAFDiffWtFile(k, path, base_h40, reporoot);
    }
    done;
}

// --- Whole tree, ref vs ref ---------------------------------------

ok64 GRAFDiffTreeRefs(keeper *k, u8cs from, u8cs to, u8cs reporoot) {
    sane(k && $ok(from) && $ok(to));
    (void)reporoot;

    // --- 1. Walk `from`, collect ---
    a_pad(u8, fbuf, 256);
    call(diffref_compose_ref_uri, fbuf, from);
    a_dup(u8c, fdata, u8bData(fbuf));
    uri ftarget = {};
    call(URIutf8Drain, fdata, &ftarget);

    diffref_entry from_entries[DIFFREF_MAX_FILES];
    diffref_set from_set = {.v = from_entries, .cap = DIFFREF_MAX_FILES};
    diffref_collect_ctx fctx = {.set = &from_set};
    call(KEEPLsFiles, k, &ftarget, diffref_collect_visit, &fctx);

    // --- 2. Walk `to`, collect ---
    a_pad(u8, tbuf, 256);
    call(diffref_compose_ref_uri, tbuf, to);
    a_dup(u8c, tdata, u8bData(tbuf));
    uri ttarget = {};
    call(URIutf8Drain, tdata, &ttarget);

    diffref_entry to_entries[DIFFREF_MAX_FILES];
    diffref_set to_set = {.v = to_entries, .cap = DIFFREF_MAX_FILES};
    diffref_collect_ctx tctx = {.set = &to_set};
    call(KEEPLsFiles, k, &ttarget, diffref_collect_visit, &tctx);

    if (from_set.overflow || to_set.overflow) {
        fprintf(stderr, "graf: diff-tree: files skipped (>%u limit)\n",
                (u32)DIFFREF_MAX_FILES);
    }

    // --- 3. For each to-entry, diff against matching from-entry ---
    Bu8 old_buf = {}, new_buf = {};
    call(u8bMap, old_buf, 16UL << 20);
    call(u8bMap, new_buf, 16UL << 20);

    for (u32 i = 0; i < to_set.n; i++) {
        u8cs path = {(u8cp)to_set.v[i].path,
                     (u8cp)to_set.v[i].path + to_set.v[i].path_len};
        diffref_entry *f = diffref_set_find(&from_set, path);

        // Same sha on both sides → unchanged, skip cheaply.
        if (f && memcmp(f->sha, to_set.v[i].sha, 20) == 0) continue;

        u8cs old_data = {}, new_data = {};
        if (f) {
            sha1 fs = {};
            memcpy(fs.data, f->sha, 20);
            u8bReset(old_buf);
            u8 ot = 0;
            if (KEEPGetExact(k, &fs, old_buf, &ot) == OK && ot == DOG_OBJ_BLOB) {
                old_data[0] = u8bDataHead(old_buf);
                old_data[1] = u8bIdleHead(old_buf);
            }
        }
        sha1 ts = {};
        memcpy(ts.data, to_set.v[i].sha, 20);
        u8bReset(new_buf);
        u8 nt = 0;
        if (KEEPGetExact(k, &ts, new_buf, &nt) == OK && nt == DOG_OBJ_BLOB) {
            new_data[0] = u8bDataHead(new_buf);
            new_data[1] = u8bIdleHead(new_buf);
        }

        diffref_emit_pair(old_data, new_data, path);
    }

    // --- 4. from-only entries (deletions): diff blob vs empty ---
    for (u32 i = 0; i < from_set.n; i++) {
        u8cs path = {(u8cp)from_set.v[i].path,
                     (u8cp)from_set.v[i].path + from_set.v[i].path_len};
        if (diffref_set_find(&to_set, path) != NULL) continue;

        sha1 fs = {};
        memcpy(fs.data, from_set.v[i].sha, 20);
        u8bReset(old_buf);
        u8 ot = 0;
        if (KEEPGetExact(k, &fs, old_buf, &ot) != OK || ot != DOG_OBJ_BLOB)
            continue;
        u8cs old_data = {u8bDataHead(old_buf), u8bIdleHead(old_buf)};
        u8cs new_data = {};
        diffref_emit_pair(old_data, new_data, path);
    }

    u8bUnMap(old_buf);
    u8bUnMap(new_buf);
    done;
}
