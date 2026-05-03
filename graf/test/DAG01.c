//
//  DAG01 — graf DAG primitives over an in-memory wh128 run.
//
//  Bypasses GRAFOpen / file I/O: builds a sorted wh128cs[] run by
//  hand, wraps it in `wh128css runs`, and exercises the lookup-side
//  primitives directly.  Ingest-side correctness (the GRAFDagUpdate
//  TREE/COMMIT cases) is covered by the CLI integration tests
//  (toy.sh, get.sh) since it requires the full graf state.
//

#include "DAG.h"

#include <string.h>

#include "abc/PRO.h"
#include "abc/TEST.h"

//  Local instantiations: wh128 sort/dedup macros are declared in the
//  templated headers and instantiated per .c file that needs them.
#define X(M, name) M##wh128##name
#include "abc/QSORTx.h"
#undef X

// --- Test fixtures -------------------------------------------------

#define DAG01_MAX_RECS 64

typedef struct {
    wh128 recs[DAG01_MAX_RECS];
    u32   n;
} dag01_idx;

static void dag01_emit(dag01_idx *idx,
                       u8 ktype, u64 khash,
                       u8 vtype, u64 vhash) {
    if (idx->n >= DAG01_MAX_RECS) return;
    idx->recs[idx->n++] = DAGEntry(ktype, khash, vtype, vhash);
}

//  Sort the index in place, then build a `wh128css runs` view over it.
//  The view aliases `idx->recs` and `view`, both of which must outlive
//  any `runs` use.
static void dag01_view(dag01_idx *idx, wh128cs view[1], wh128css *runs) {
    wh128s d = {idx->recs, idx->recs + idx->n};
    wh128sSort(d);
    view[0][0] = idx->recs;
    view[0][1] = idx->recs + idx->n;
    (*runs)[0] = view;
    (*runs)[1] = view + 1;
}

// --- Tests ---------------------------------------------------------

//  Test 1: single (COMMIT, TREE) edge — DAGCommitTree resolves it.
ok64 DAG01test1() {
    sane(1);
    dag01_idx idx = {};
    dag01_emit(&idx, DAG_T_COMMIT, 0xC1C1, DAG_T_TREE, 0x7777);
    wh128cs view[1]; wh128css runs = {NULL, NULL};
    dag01_view(&idx, view, &runs);

    u64 t = DAGCommitTree(runs, 0xC1C1);
    want(t == 0x7777);
    done;
}

//  Test 2: single (COMMIT, COMMIT) parent edge — DAGParents finds it
//  and DAGCommitTree returns 0 (no tree edge).
ok64 DAG01test2() {
    sane(1);
    dag01_idx idx = {};
    dag01_emit(&idx, DAG_T_COMMIT, 0xC1C1, DAG_T_COMMIT, 0xFEED);
    wh128cs view[1]; wh128css runs = {NULL, NULL};
    dag01_view(&idx, view, &runs);

    wh64 par_buf[4] = {};
    wh64s parents = {par_buf, par_buf + 4};
    wh64 *pbase = parents[0];
    want(DAGParents(runs, parents, DAGPack(DAG_T_COMMIT, 0xC1C1)) == OK);
    want(parents[0] - pbase == 1);
    want(DAGType(pbase[0]) == DAG_T_COMMIT);
    want(DAGHashlet(pbase[0]) == 0xFEED);

    want(DAGCommitTree(runs, 0xC1C1) == 0);
    done;
}

//  Test 3: same-key COMMIT_PARENT and COMMIT_TREE coexist; each
//  side filters by val.type.  Verifies the val-type filtering added
//  in this layout swap.
ok64 DAG01test3() {
    sane(1);
    dag01_idx idx = {};
    dag01_emit(&idx, DAG_T_COMMIT, 0xC1C1, DAG_T_TREE,   0xAAA);
    dag01_emit(&idx, DAG_T_COMMIT, 0xC1C1, DAG_T_COMMIT, 0x111);
    dag01_emit(&idx, DAG_T_COMMIT, 0xC1C1, DAG_T_COMMIT, 0x222);
    wh128cs view[1]; wh128css runs = {NULL, NULL};
    dag01_view(&idx, view, &runs);

    want(DAGCommitTree(runs, 0xC1C1) == 0xAAA);

    wh64 par_buf[4] = {};
    wh64s parents = {par_buf, par_buf + 4};
    wh64 *pbase = parents[0];
    want(DAGParents(runs, parents, DAGPack(DAG_T_COMMIT, 0xC1C1)) == OK);
    want(parents[0] - pbase == 2);
    want(DAGType(pbase[0]) == DAG_T_COMMIT);
    want(DAGType(pbase[1]) == DAG_T_COMMIT);
    u64 h0 = DAGHashlet(pbase[0]), h1 = DAGHashlet(pbase[1]);
    want((h0 == 0x111 && h1 == 0x222) || (h0 == 0x222 && h1 == 0x111));
    done;
}

ok64 maintest() {
    sane(1);
    call(DAG01test1);
    call(DAG01test2);
    call(DAG01test3);
    done;
}

TEST(maintest)
