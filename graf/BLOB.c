//  BLOB: shared (commit, path) → blob + (tree, name) → sha helpers.
//  Lifted from graf/BLAME.c so GRAFGet can reuse them.
//
#include "BLOB.h"

#include <string.h>

#include "abc/PRO.h"
#include "abc/BUF.h"
#include "dog/DOG.h"
#include "graf/DAG.h"
#include "dog/git/GIT.h"

ok64 GRAFTreeStep(sha1 *cur, u8cs name) {
    sane(cur);
    Bu8 tbuf = {};
    call(u8bAllocate, tbuf, 1UL << 20);
    u8 otype = 0;
    ok64 o = KEEPGetExact(cur, tbuf, &otype);
    if (o != OK) { u8bFree(tbuf); return o; }
    if (otype != DOG_OBJ_TREE) { u8bFree(tbuf); fail(KEEPFAIL); }

    a_dup(u8c, body, u8bDataC(tbuf));
    u8cs field = {}, esha = {};
    ok64 result = KEEPNONE;
    while (GITu8sDrainTree(body, field, esha, NULL) == OK) {
        u8cs entry_name = {};
        if (GITu8sFileSplit(field, NULL, entry_name) != OK) continue;
        if (!u8csEq(entry_name, name)) continue;
        (void)sha1Drain(esha, cur);
        result = OK;
        break;
    }
    u8bFree(tbuf);
    return result;
}

ok64 GRAFPathDescend(sha1 *cur, u8cs path) {
    sane(cur);
    a_dup(u8c, rest, path);
    while (!u8csEmpty(rest)) {
        u8c const *start = rest[0];
        a_dup(u8c, scan, rest);
        (void)u8csFind(scan, '/');
        u8cs name = {start, scan[0]};
        rest[0] = scan[0];
        if (!u8csEmpty(rest)) u8csUsed1(rest);  // step past '/'
        if (u8csEmpty(name)) continue;
        call(GRAFTreeStep, cur, name);
    }
    done;
}

ok64 GRAFBlobAtCommit(u8bp buf, u64 commit_hashlet60, u8cs filepath) {
    sane(buf);

    Bu8 cbuf = {};
    call(u8bAllocate, cbuf, 1UL << 20);
    u8 ct = 0;
    ok64 o = KEEPGet(commit_hashlet60,
                     DAG_H60_HEXLEN, cbuf, &ct);
    if (o != OK || ct != DOG_OBJ_COMMIT) { u8bFree(cbuf); return KEEPNONE; }

    sha1 cur = {};
    o = GITu8sCommitTree(u8bDataC(cbuf), cur.data);
    u8bFree(cbuf);
    if (o != OK) return KEEPNONE;

    call(GRAFPathDescend, &cur, filepath);

    u8 btype = 0;
    call(KEEPGetExact, &cur, buf, &btype);
    if (btype != DOG_OBJ_BLOB) return KEEPNONE;
    done;
}
