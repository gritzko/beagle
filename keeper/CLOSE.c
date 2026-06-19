//  CLOSE: object-closure walk + sorted sha membership set.
//  See CLOSE.h for the contract.

#include "CLOSE.h"

#include <string.h>

#include "abc/PRO.h"
#include "dog/git/GIT.h"
#include "keeper/WALK.h"

// --- close_set: sorted sha array, binary-search membership ---

b8 CLOSESetHas(close_set const *s, sha1cp q) {
    if (!s || s->n == 0) return NO;
    u32 lo = 0, hi = s->n;
    while (lo < hi) {
        u32 mid = (lo + hi) >> 1;
        if (sha1Z(&s->items[mid], q)) lo = mid + 1;
        else if (sha1Z(q, &s->items[mid])) hi = mid;
        else return YES;
    }
    return NO;
}

void CLOSESetAdd(close_set *s, sha1cp v) {
    if (!s || s->n >= s->cap) return;
    //  Insertion sort: find the position and shift the tail up.
    u32 i = s->n;
    while (i > 0 && sha1Z(v, &s->items[i - 1])) {
        s->items[i] = s->items[i - 1];
        i--;
    }
    if (i > 0 && sha1Eq(&s->items[i - 1], v)) return;  //  dup
    s->items[i] = *v;
    s->n++;
}

// --- closure walk ---

//  KEEP-001: CLOSE's tree closure is now a visitor over the one keeper
//  walker (KEEPWalkTree).  The visitor collects each entry's sha
//  (tree or blob) into `out`, prunes against `have` / `add_to_have`
//  (returning WALKSKIP so the shared walker doesn't descend a closed
//  subtree), skips submodule gitlinks, and stops on overflow.
typedef struct {
    sha1            *out;
    u32             *n;
    u32              cap;
    close_set const *have;
    close_set       *add;
    ok64             err;       //  CLOSEFULL on overflow
} close_visit_ctx;

static ok64 close_visit(u8cs path, u8 kind, u8cp esha, u8cs blob,
                        void0p vctx) {
    (void)path; (void)blob;
    close_visit_ctx *c = (close_visit_ctx *)vctx;
    if (kind == WALK_KIND_SUB) return WALKSKIP;     // gitlink: other shard
    sha1 entry = {};
    u8cs es = {esha, esha + 20};
    (void)sha1Drain(es, &entry);
    //  Already-closed (peer has it, or seen via another merge path) —
    //  don't collect, don't descend.
    if (c->have && CLOSESetHas(c->have, &entry)) return WALKSKIP;
    if (c->add  && CLOSESetHas(c->add,  &entry)) return WALKSKIP;
    if (c->out) {
        if (*c->n >= c->cap) { c->err = CLOSEFULL; return WALKSTOP; }
        c->out[(*c->n)++] = entry;
    }
    if (c->add) CLOSESetAdd(c->add, &entry);
    return OK;
}

ok64 CLOSEWalkTree(sha1cp tree_sha, sha1 *out, u32 *n, u32 cap,
                   close_set const *have, close_set *add_to_have) {
    sane(tree_sha && n);
    close_visit_ctx c = {.out = out, .n = n, .cap = cap,
                         .have = have, .add = add_to_have, .err = OK};
    //  KEEP-001: tolerate a missing / non-tree root object the same way
    //  the former hand-rolled walk did — the root sha is already
    //  collected by the visitor; a body we don't hold just doesn't add
    //  sub-objects (haveset-build mode relies on this).  Overflow is the
    //  only hard error, surfaced via `c.err`.
    //  GIT-009: WALK_INCL_ANCHOR so a `.be` store-anchor blob (a foreign
    //  git history may have committed it) is part of the served closure
    //  — the shipped tree references it, so dropping it dangles the pack.
    ok64 wo = KEEPWalkTree((u8cp)tree_sha, NO, WALK_INCL_ANCHOR,
                           close_visit, &c);
    if (c.err != OK) return c.err;
    if (wo == KEEPNONE || wo == WALKBADFMT) done;
    return wo;
}

ok64 CLOSEWalkCommit(sha1cp commit_sha, sha1 *out, u32 *n, u32 cap,
                     close_set const *have, close_set *add_to_have) {
    sane(commit_sha && n);
    if (have && CLOSESetHas(have, commit_sha)) done;
    if (add_to_have && CLOSESetHas(add_to_have, commit_sha)) done;
    if (out) {
        if (*n >= cap) return CLOSEFULL;
        out[(*n)++] = *commit_sha;
    }
    if (add_to_have) CLOSESetAdd(add_to_have, commit_sha);

    Bu8 cbuf = {};
    call(u8bMap, cbuf, 1UL << 20);
    u8 ctype = 0;
    ok64 go = KEEPGetExact(commit_sha, cbuf, &ctype);
    if (go != OK || ctype != KEEP_OBJ_COMMIT) {
        u8bUnMap(cbuf);
        //  Haveset-build mode (out == NULL): tolerate a missing commit
        //  — we collect what we have, the rest just doesn't prune.
        //  Pack-build mode (out != NULL): the body is required.
        if (out == NULL) done;
        return KEEPNONE;
    }
    u8cs commit_body = {u8bDataHead(cbuf), u8bIdleHead(cbuf)};
    sha1 tree_sha = {};
    if (GITu8sCommitTree(commit_body, tree_sha.data) != OK) {
        u8bUnMap(cbuf);
        return KEEPFAIL;
    }

    //  Walk parents.  Each `parent <40-hex>` header names another commit
    //  that must also be in the pack unless the peer has it.
    {
        u8cs body = {u8bDataHead(cbuf), u8bIdleHead(cbuf)};
        u8cs field = {}, value = {};
        while (GITu8sDrainCommit(body, field, value) == OK) {
            if ($empty(field)) break;
            if ($len(field) == 6 && memcmp(field[0], "parent", 6) == 0) {
                sha1 par = {};
                if (sha1FromHex(&par, value) != OK) continue;
                if (have && CLOSESetHas(have, &par)) continue;
                if (add_to_have && CLOSESetHas(add_to_have, &par)) continue;
                call(CLOSEWalkCommit, &par, out, n, cap, have, add_to_have);
            }
        }
    }
    u8bUnMap(cbuf);

    return CLOSEWalkTree(&tree_sha, out, n, cap, have, add_to_have);
}
