//  DAG: graf's commit-graph index, streaming ingest.
//
//  Fed via GRAFUpdate one COMMIT object at a time (TREE/BLOB
//  callbacks are accepted but ignored — only commit→parent and
//  commit→tree edges are recorded).  Finish flushes the pending
//  batch and triggers compaction.  No historical keeper lookups.
//
//  Layout:
//      .dogs/graf/0000000001.idx   sorted wh128 runs (LSM)
//
#include "DAG.h"
#include "GRAF.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/KV.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/RAP.h"
#include "abc/RON.h"
#include "dog/DPATH.h"
#include "dog/SHA1.h"
#include "keeper/GIT.h"

// Resolve a 40-bit object hashlet.  Prefer the caller-supplied SHA
// (the UNPK hot path has it) — falls back to computing it from the
// object body for callers that don't (e.g. `graf index`'s manual
// reindex walk at graf/INDEX.c).
static u64 dag_obj_hashlet(u8 obj_type, sha1 const *sha, u8cs body) {
    if (sha) return WHIFFHashlet40(sha);

    char hdr[32];
    char const *tn = "blob";
    switch (obj_type) {
        case DOG_OBJ_COMMIT: tn = "commit"; break;
        case DOG_OBJ_TREE:   tn = "tree";   break;
        case DOG_OBJ_BLOB:   tn = "blob";   break;
        case DOG_OBJ_TAG:    tn = "tag";    break;
    }
    int hlen = snprintf(hdr, sizeof(hdr), "%s %zu",
                        tn, (size_t)u8csLen(body));
    if (hlen < 0 || (size_t)hlen >= sizeof(hdr)) return 0;

    SHA1state st;
    SHA1Open(&st);
    u8cs hs = {(u8cp)hdr, (u8cp)hdr + hlen + 1};  // include trailing NUL
    SHA1Feed(&st, hs);
    SHA1Feed(&st, body);
    sha1 out = {};
    SHA1DCFinal(out.data, &st);
    return WHIFFHashlet40(&out);
}

// --- Template instantiations for wh128 (sort, merge, hash).
// Bx.h already instantiated via dog/WHIFF.h.
#define X(M, name) M##wh128##name
#include "abc/QSORTx.h"
#include "abc/HITx.h"
#include "abc/HASHx.h"
#undef X

// --- Constants ---

#define DAG_DIR         ".dogs/graf"
#define DAG_IDX_EXT     ".idx"
#define DAG_SEQNO_W     10
#define DAG_BATCH       (1 << 18)   // 256K entries per flush

// --- Ingest state (opaque to callers) ---

struct dag_ingest {
    wh128  *batch;          // emit buffer
    size_t  batch_len;
    size_t  batch_cap;

    u64     seqno;
    u8      finished;

    u8cs    dagdir;         // borrowed; points into graf state
    char    dagdir_buf[512];
};

// --- COMMIT bookmark dir existence helpers ---

static b8 dag_is_hex_sha(char const *s, size_t len) {
    if (len < 40) return NO;
    for (int i = 0; i < 40; i++) {
        u8 c = (u8)s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return NO;
    }
    return YES;
}

// --- LSM file I/O ---

static ok64 dag_index_write(u8cs dagdir, wh128cs run, u64 seqno) {
    sane($ok(dagdir));
    if ($empty(run)) done;

    a_cstr(idxext, DAG_IDX_EXT);
    a_cstr(tmpsuf, ".tmp");

    //  Atomic publication: write <seqno>.idx.tmp, rename to
    //  <seqno>.idx.  Lockless readers in dag_stack_open filter by
    //  the exact ".idx" suffix, so the tmp file is invisible to
    //  them during the write window.
    a_pad(u8, path, FILE_PATH_MAX_LEN);
    call(u8bFeed, path, dagdir);
    call(u8bFeed1, path, '/');
    call(RONu8sFeedPad, u8bIdle(path), seqno, DAG_SEQNO_W);
    ((u8 **)path)[2] += DAG_SEQNO_W;
    call(u8bFeed, path, idxext);
    call(PATHu8bTerm, path);

    a_pad(u8, tmppath, FILE_PATH_MAX_LEN);
    call(u8bFeed, tmppath, dagdir);
    call(u8bFeed1, tmppath, '/');
    call(RONu8sFeedPad, u8bIdle(tmppath), seqno, DAG_SEQNO_W);
    ((u8 **)tmppath)[2] += DAG_SEQNO_W;
    call(u8bFeed, tmppath, idxext);
    call(u8bFeed, tmppath, tmpsuf);
    call(PATHu8bTerm, tmppath);

    int fd = -1;
    call(FILECreate, &fd, $path(tmppath));
    size_t bytes = $len(run) * sizeof(wh128);
    u8cs data = {(u8cp)run[0], (u8cp)run[0] + bytes};
    call(FILEFeedAll, fd, data);
    close(fd);
    call(FILERename, $path(tmppath), $path(path));
    done;
}

static ok64 dag_next_seqno(u64 *seqno, u8cs dagdir) {
    sane(seqno && $ok(dagdir));
    *seqno = 1;

    a_path(dpat);
    call(PATHu8bFeed, dpat, dagdir);

    DIR *d = opendir((char *)u8bDataHead(dpat));
    if (!d) done;
    u64 maxseq = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t nlen = strlen(e->d_name);
        if (nlen != DAG_SEQNO_W + 4) continue;
        if (memcmp(e->d_name + DAG_SEQNO_W, DAG_IDX_EXT, 4) != 0) continue;
        u8cs numslice = {(u8cp)e->d_name, (u8cp)e->d_name + DAG_SEQNO_W};
        u64 val = 0;
        ok64 r = RONutf8sDrain(&val, numslice);
        if (r == OK && val > maxseq) maxseq = val;
    }
    closedir(d);
    *seqno = maxseq + 1;
    done;
}

// --- dag_stack (LSM read-side) ---

ok64 dag_stack_open(dag_stack *st, u8cs dagdir) {
    sane(st && $ok(dagdir));
    memset(st, 0, sizeof(*st));

    //  Lockless reader: list + mmap is racy against dag_compact,
    //  which unlinks old runs after writing the new merged one.
    //  If a just-listed run is missing at mmap time, the listing
    //  is stale — drop what we have and relist.  After compaction
    //  the merged run subsumes the unlinked ones, so a fresh scan
    //  is correct.  Bounded retries guard a pathological writer.
    ok64 last = OK;
    for (u32 attempt = 0; attempt < 4; attempt++) {
        dag_stack_close(st);
        memset(st, 0, sizeof(*st));

        a_path(dpat);
        call(PATHu8bFeed, dpat, dagdir);

        DIR *d = opendir((char *)u8bDataHead(dpat));
        if (!d) done;

        char names[MSET_MAX_LEVELS][64];
        char *namep[MSET_MAX_LEVELS];
        u32 count = 0;
        struct dirent *e;
        while ((e = readdir(d)) != NULL && count < MSET_MAX_LEVELS) {
            size_t nlen = strlen(e->d_name);
            if (nlen != DAG_SEQNO_W + 4) continue;
            if (memcmp(e->d_name + DAG_SEQNO_W, DAG_IDX_EXT, 4) != 0) continue;
            memcpy(names[count], e->d_name, nlen + 1);
            namep[count] = names[count];
            count++;
        }
        closedir(d);

        // Sort by name (oldest first)
        for (u32 i = 0; i + 1 < count; i++)
            for (u32 j = i + 1; j < count; j++)
                if (strcmp(namep[i], namep[j]) > 0) {
                    char *t = namep[i]; namep[i] = namep[j]; namep[j] = t;
                }

        b8 retry = NO;
        for (u32 i = 0; i < count; i++) {
            u8cs fn = {(u8cp)namep[i], (u8cp)namep[i] + strlen(namep[i])};
            a_path(fpath, dagdir, fn);

            u8bp mapped = NULL;
            ok64 mo = FILEMapRO(&mapped, $path(fpath));
            if (mo == FILENOENT) {
                retry = YES;
                break;
            }
            if (mo != OK) { last = mo; continue; }
            wh128cp base = (wh128cp)u8bDataHead(mapped);
            size_t nentries = (u8bIdleHead(mapped) - u8bDataHead(mapped)) / sizeof(wh128);
            st->runs[st->n][0] = base;
            st->runs[st->n][1] = base + nentries;
            st->maps[st->n] = mapped;
            st->n++;
        }
        if (!retry) done;
        last = FILENOENT;
    }
    dag_stack_close(st);
    memset(st, 0, sizeof(*st));
    return last;
}

void dag_stack_close(dag_stack *st) {
    if (!st) return;
    for (u32 i = 0; i < st->n; i++)
        if (st->maps[i]) FILEUnMap(st->maps[i]);
    st->n = 0;
}

// --- Graph-navigation primitives ---

u32 DAGParents(dag_stack const *idx, u64 commit_h, u64 *out, u32 cap) {
    if (!idx) return 0;
    u64 lo = DAGPack(DAG_COMMIT_PARENT, 0, commit_h);
    u64 hi = DAGPack(DAG_COMMIT_PARENT, WHIFF_ID_MASK, commit_h);
    u32 total = 0;
    for (u32 r = 0; r < idx->n; r++) {
        wh128cp base = idx->runs[r][0];
        size_t len = (size_t)(idx->runs[r][1] - base);
        size_t lo_i = 0, hi_i = len;
        while (lo_i < hi_i) {
            size_t mid = lo_i + (hi_i - lo_i) / 2;
            if (base[mid].key < lo) lo_i = mid + 1;
            else hi_i = mid;
        }
        while (lo_i < len && base[lo_i].key >= lo && base[lo_i].key <= hi) {
            if (DAGType(base[lo_i].key) == DAG_COMMIT_PARENT) {
                if (total < cap && out) {
                    out[total] = DAGHashlet(base[lo_i].val);
                }
                total++;
            }
            lo_i++;
        }
    }
    return total;
}

ok64 dag_anc_put(Bwh128 set, u64 commit_h) {
    wh128 rec = {.key = wh64Pack(0, 0, commit_h), .val = 0};
    wh128s tab = {wh128bHead(set), wh128bTerm(set)};
    return HASHwh128Put(tab, &rec);
}

b8 DAGAncestorsHas(Bwh128 set, u64 commit_h) {
    wh128 probe = {.key = wh64Pack(0, 0, commit_h), .val = 0};
    wh128s tab = {wh128bHead(set), wh128bTerm(set)};
    return HASHwh128Get(&probe, tab) == OK;
}

ok64 DAGAncestors(Bwh128 set, dag_stack const *idx, u64 tip) {
    sane(idx);
    if (tip == 0) done;

    // BFS queue sized to match the set's capacity (the queue never
    // outgrows the set — every queue entry also lives in the set).
    size_t cap = (size_t)(wh128bTerm(set) - wh128bHead(set));
    if (cap == 0) return DAGFAIL;

    Bwh128 queue = {};
    call(wh128bMap, queue, cap);

    dag_anc_put(set, tip);
    wh128 q0 = { .key = wh64Pack(0, 0, tip), .val = 0 };
    wh128bFeed1(queue, q0);

    size_t head = 0;
    u64 parents[16];
    while (head < wh128bDataLen(queue)) {
        wh128cp cur = wh128bDataHead(queue) + head;
        u64 c = DAGHashlet(cur->key);
        head++;

        u32 np = DAGParents(idx, c, parents, 16);
        if (np > 16) np = 16;
        for (u32 i = 0; i < np; i++) {
            if (DAGAncestorsHas(set, parents[i])) continue;
            if (dag_anc_put(set, parents[i]) != OK) continue;
            wh128 qr = { .key = wh64Pack(0, 0, parents[i]), .val = 0 };
            if (wh128bFeed1(queue, qr) != OK) break;
        }
    }

    wh128bUnMap(queue);
    done;
}

ok64 DAGAncestorsOfMany(Bwh128 set, dag_stack const *idx,
                        u64 const *tips, u32 n) {
    sane(idx);
    for (u32 i = 0; i < n; i++) {
        if (tips[i] == 0) continue;
        call(DAGAncestors, set, idx, tips[i]);
    }
    done;
}

ok64 DAGAllCommits(Bwh128 set, dag_stack const *idx) {
    sane(idx);
    for (u32 r = 0; r < idx->n; r++) {
        wh128cp base = idx->runs[r][0];
        wh128cp end  = idx->runs[r][1];
        for (wh128cp p = base; p < end; p++) {
            if (DAGType(p->key) != DAG_COMMIT_TREE) continue;
            dag_anc_put(set, DAGHashlet(p->key));
        }
    }
    done;
}

// --- Topological sort over a hashlet set ---
//
//  Iterative DFS post-order: descend into parents that are inside
//  `set`; emit a commit when all its in-set parents have been emitted.
//  Result: parents-before-children for arbitrary topology, no gen field
//  required.

#define DAG_TOPO_MAX_PARENTS 16

typedef struct {
    u64 c;
    u32 par_i;       // next parent slot to explore
    u32 npar;
    u64 pars[DAG_TOPO_MAX_PARENTS];
} topo_frame;

u32 DAGTopoSort(u64 *out, u32 cap,
                Bwh128 set, dag_stack const *idx) {
    if (cap == 0 || !idx || !out) return 0;

    size_t set_cap = (size_t)(wh128bTerm(set) - wh128bHead(set));
    if (set_cap == 0) return 0;

    Bwh128 visited = {};
    if (wh128bMap(visited, set_cap) != OK) return 0;

    //  Stack capacity = set capacity is overkill but safe (a DFS stack
    //  is bounded by the longest chain in the subgraph, which never
    //  exceeds the number of nodes).
    Bu8 stk_buf = {};
    if (u8bMap(stk_buf, set_cap * sizeof(topo_frame)) != OK) {
        wh128bUnMap(visited);
        return 0;
    }
    topo_frame *stack = (topo_frame *)u8bDataHead(stk_buf);
    u32 stack_max = (u32)set_cap;

    u32 written = 0;
    wh128cp set_head = wh128bHead(set);
    wh128cp set_term = wh128bTerm(set);

    for (wh128cp p = set_head; p < set_term && written < cap; p++) {
        if (p->key == 0) continue;            // empty hash slot
        u64 root = DAGHashlet(p->key);
        if (DAGAncestorsHas(visited, root)) continue;
        if (1 > stack_max) goto outta_room;

        u32 sp = 0;
        stack[sp].c = root;
        stack[sp].par_i = 0;
        stack[sp].npar = DAGParents(idx, root, stack[sp].pars,
                                    DAG_TOPO_MAX_PARENTS);
        if (stack[sp].npar > DAG_TOPO_MAX_PARENTS)
            stack[sp].npar = DAG_TOPO_MAX_PARENTS;
        sp++;
        dag_anc_put(visited, root);

        while (sp > 0) {
            topo_frame *t = &stack[sp - 1];
            b8 descended = NO;
            while (t->par_i < t->npar) {
                u64 par = t->pars[t->par_i++];
                if (par == 0) continue;
                if (!DAGAncestorsHas(set, par)) continue;
                if (DAGAncestorsHas(visited, par)) continue;
                if (sp >= stack_max) goto outta_room;

                stack[sp].c = par;
                stack[sp].par_i = 0;
                stack[sp].npar = DAGParents(idx, par, stack[sp].pars,
                                            DAG_TOPO_MAX_PARENTS);
                if (stack[sp].npar > DAG_TOPO_MAX_PARENTS)
                    stack[sp].npar = DAG_TOPO_MAX_PARENTS;
                sp++;
                dag_anc_put(visited, par);
                descended = YES;
                break;
            }
            if (!descended) {
                if (written < cap) out[written++] = t->c;
                sp--;
            }
        }
    }

outta_room:
    u8bUnMap(stk_buf);
    wh128bUnMap(visited);
    return written;
}

// --- Compaction (merges multiple runs when newer is large vs older) ---

static ok64 dag_compact(u8cs dagdir) {
    sane($ok(dagdir));

    a_cstr(ext, DAG_IDX_EXT);
    Bkv32 pups = {};
    call(kv32bAllocate, pups, FILE_MAX_OPEN);
    call(DOGPupOpenAll, pups, dagdir, ext);

    u32 nfiles = DOGPupCount(pups);
    if (nfiles < 2) { DOGPupClose(pups); done; }

    //  Build typed view from puppy data slices.
    wh128cs runs[MSET_MAX_LEVELS] = {};
    for (u32 i = 0; i < nfiles && i < MSET_MAX_LEVELS; i++) {
        u8cs raw = {};
        DOGPupData(raw, pups, i);
        runs[i][0] = (wh128cp)raw[0];
        runs[i][1] = (wh128cp)raw[1];
    }
    wh128css stack = {runs, runs + nfiles};

    if (HITwh128IsCompact(stack)) { DOGPupClose(pups); done; }

    size_t total = 0;
    for (u32 i = 0; i < nfiles; i++)
        total += (size_t)(runs[i][1] - runs[i][0]);

    Bwh128 cbuf = {};
    call(wh128bAllocate, cbuf, total);
    wh128 *base = cbuf[0];
    wh128s into = {cbuf[0], cbuf[3]};
    size_t before_len = $len(stack);
    call(HITwh128Compact, stack, into);
    size_t m = before_len - $len(stack) + 1;
    if (m < 2) { wh128bFree(cbuf); DOGPupClose(pups); done; }

    u8cs merged = {(u8cp)base, (u8cp)(into[0])};
    call(DOGPupThinTail, pups, dagdir, ext, (u32)m);
    call(DOGPupCreate, pups, dagdir, ext, merged);

    wh128bFree(cbuf);
    DOGPupClose(pups);
    done;
}

// --- Ingest state management ---

static ok64 dag_ingest_alloc(dag_ingest **out, u8cs dagdir) {
    sane(out && $ok(dagdir));
    *out = NULL;

    dag_ingest *g = calloc(1, sizeof(*g));
    if (!g) return DAGFAIL;

    // dagdir copy (caller's buffer may be transient)
    size_t dlen = $len(dagdir);
    if (dlen >= sizeof(g->dagdir_buf)) { free(g); return DAGFAIL; }
    memcpy(g->dagdir_buf, dagdir[0], dlen);
    g->dagdir_buf[dlen] = 0;
    g->dagdir[0] = (u8p)g->dagdir_buf;
    g->dagdir[1] = (u8p)g->dagdir_buf + dlen;

    g->batch_cap = DAG_BATCH;
    g->batch = calloc(g->batch_cap, sizeof(wh128));
    if (!g->batch) goto fail;

    call(dag_next_seqno, &g->seqno, g->dagdir);

    *out = g;
    done;

fail:
    free(g->batch);
    free(g);
    return DAGFAIL;
}

static void dag_ingest_free(dag_ingest *g) {
    if (!g) return;
    free(g->batch);
    free(g);
}

// --- Emit helpers ---

static void dag_emit(dag_ingest *g,
                     u8 atype, u32 agen, u64 ahash,
                     u8 btype, u32 bgen, u64 bhash) {
    if (g->batch_len >= g->batch_cap) return;  // overflow; handled by flush
    g->batch[g->batch_len++] = DAGEntry(atype, agen, ahash,
                                        btype, bgen, bhash);
}

static ok64 dag_flush_batch(dag_ingest *g) {
    sane(g);
    if (g->batch_len == 0) done;
    wh128s d = {g->batch, g->batch + g->batch_len};
    wh128sSort(d);
    wh128sDedup(d);
    g->batch_len = (size_t)(d[1] - d[0]);
    wh128cs run = {g->batch, g->batch + g->batch_len};
    call(dag_index_write, g->dagdir, run, g->seqno);
    g->seqno++;
    g->batch_len = 0;
    done;
}

static void dag_batch_maybe_flush(dag_ingest *g) {
    if (g->batch_len + 64 >= g->batch_cap) dag_flush_batch(g);
}

// --- Finish: flush pending records, compact runs. ---

static ok64 dag_finish(dag_ingest *g) {
    sane(g);
    if (g->finished) done;
    call(dag_flush_batch, g);
    dag_compact(g->dagdir);
    g->finished = 1;
    done;
}

// ============================================================
// Public entry: GRAFUpdate
// ============================================================

// This is the real meat the GRAFUpdate wrapper calls into.  `state`
// is graf's own state (struct graf from GRAF.h).  We reach into
// state->ing to lazily allocate the ingest context.  Forward-decl
// of struct graf comes from GRAF.h include above.

ok64 GRAFDagUpdate(u8 obj_type, sha1 const *sha, u8cs blob) {
    sane(1);
    graf *state = &GRAF;

    // Lazy allocate ingest state on first call.
    if (!state->ing) {
        a_dup(u8c, root_s, u8bDataC(state->h->root));
        a_path(dp, root_s);
        a_cstr(rel, "/" DAG_DIR);
        call(u8bFeed, dp, rel);
        call(PATHu8bTerm, dp);
        a_dup(u8c, dagdir, u8bDataC(dp));
        call(FILEMakeDirP, $path(dp));
        call(dag_ingest_alloc, &state->ing, dagdir);
    }

    dag_ingest *g = state->ing;

    switch (obj_type) {
    case DOG_OBJ_COMMIT: {
        // Parse commit header for tree_h and parents[].
        a_dup(u8c, scan, blob);
        u8cs field = {}, value = {};
        sha1 tree_sha = {};
        sha1 parents[16] = {};
        u32 npar = 0;
        b8 got_tree = NO;
        while (GITu8sDrainCommit(scan, field, value) == OK) {
            if (u8csEmpty(field)) break;
            a_cstr(ft, "tree");
            a_cstr(fp, "parent");
            if ($eq(field, ft) && u8csLen(value) >= 40) {
                DAGsha1FromHex(&tree_sha, (char const *)value[0]);
                got_tree = YES;
            } else if ($eq(field, fp) && u8csLen(value) >= 40 && npar < 16) {
                DAGsha1FromHex(&parents[npar], (char const *)value[0]);
                npar++;
            }
        }
        if (!got_tree) return DAGFAIL;

        u64 commit_h = dag_obj_hashlet(DOG_OBJ_COMMIT, sha, blob);

        u64 tree_h = WHIFFHashlet40(&tree_sha);

        //  Emit COMMIT_TREE + COMMIT_PARENT[] tuples.  Generation
        //  numbers are no longer indexed; the wh128 `id` slot stays
        //  zero in every record we write here.
        dag_emit(g, DAG_COMMIT_TREE, 0, commit_h,
                    DAG_COMMIT_TREE, 0, tree_h);
        for (u32 i = 0; i < npar; i++) {
            u64 parent_h = WHIFFHashlet40(&parents[i]);
            dag_emit(g, DAG_COMMIT_PARENT, 0, commit_h,
                        DAG_COMMIT_PARENT, 0, parent_h);
        }

        dag_batch_maybe_flush(g);
        done;
    }

    case DOG_OBJ_TREE:
    case DOG_OBJ_BLOB:
    default:
        done;  // tree/blob payloads ignored — only commit edges are indexed.
    }
}

ok64 GRAFDagFinish(void) {
    sane(1);
    graf *state = &GRAF;
    if (!state->ing) done;
    ok64 r = dag_finish(state->ing);
    dag_ingest_free(state->ing);
    state->ing = NULL;
    return r;
}
