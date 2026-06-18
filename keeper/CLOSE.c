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

ok64 CLOSEWalkTree(sha1cp tree_sha, sha1 *out, u32 *n, u32 cap,
                   close_set const *have, close_set *add_to_have) {
    sane(tree_sha && n);
    if (have && CLOSESetHas(have, tree_sha)) done;
    //  Dedup against `add_to_have` too — a merge history (a tree
    //  reachable through two paths) would otherwise re-walk the same
    //  subtree through every alternative path, exploding O(N) into
    //  O(2^depth).
    if (add_to_have && CLOSESetHas(add_to_have, tree_sha)) done;
    if (out) {
        if (*n >= cap) return CLOSEFULL;
        out[(*n)++] = *tree_sha;
    }
    if (add_to_have) CLOSESetAdd(add_to_have, tree_sha);

    Bu8 tbuf = {};
    call(u8bMap, tbuf, 1UL << 20);
    u8 ttype = 0;
    if (KEEPGetExact(tree_sha, tbuf, &ttype) != OK ||
        ttype != KEEP_OBJ_TREE) {
        u8bUnMap(tbuf);
        done;
    }
    u8cs walk = {u8bDataHead(tbuf), u8bIdleHead(tbuf)};
    u8cs file = {}, sha = {};
    while (GITu8sDrainTree(walk, file, sha, NULL) == OK) {
        if ($len(sha) != 20) continue;
        u8 kind = WALKu8sModeKind(file);
        if (kind == WALK_KIND_SUB) continue;        // gitlink: other shard
        sha1 entry_sha = {};
        (void)sha1Drain(sha, &entry_sha);
        if (have && CLOSESetHas(have, &entry_sha)) continue;
        if (add_to_have && CLOSESetHas(add_to_have, &entry_sha)) continue;
        if (kind == WALK_KIND_DIR) {
            call(CLOSEWalkTree, &entry_sha, out, n, cap, have, add_to_have);
        } else {
            if (out) {
                if (*n >= cap) { u8bUnMap(tbuf); return CLOSEFULL; }
                out[(*n)++] = entry_sha;
            }
            if (add_to_have) CLOSESetAdd(add_to_have, &entry_sha);
        }
    }
    u8bUnMap(tbuf);
    done;
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
