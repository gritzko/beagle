//  dog/AT: thin compat reader for sniff's `.sniff` log.
//
//  The log is a dog/ULOG — rows of `<ron60-ms>\t<verb>\t<uri>\n`.
//  See sniff/AT.h for URI schema.  `DOGAtTail` extracts the current
//  (branch, sha) for code that doesn't want to link against snifflib
//  (keeper, beagle).  Walks newest-first and accepts the first
//  `get`/`post`/`patch` row whose query carries a 40-hex SHA spec
//  (dog/QURY); the `repo` anchor at row 0 and `put`/`delete` intent
//  rows are naturally skipped by the SHA-length check.
//
#include "AT.h"

#include <string.h>

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/OK.h"
#include "dog/QURY.h"
#include "dog/ULOG.h"

con ok64 DOGATNONE = 0xd61029d5d85ce;

ok64 DOGAtTail(u8bp branch_out, u8bp sha_out, u8cs reporoot) {
    sane(branch_out && sha_out && $ok(reporoot));

    //  Build path: <reporoot>/.sniff
    static u8c *const AT_REL_S[2] = {
        (u8c *)".sniff",
        (u8c *)".sniff" + sizeof(".sniff") - 1,
    };
    a_path(apath, reporoot, AT_REL_S);

    //  The log is already a well-formed ULOG; read-only open +
    //  backward scan.
    u8bp  data = NULL;
    Bkv64 idx  = {};
    ok64 o = ULOGOpen(&data, idx, $path(apath));
    if (o != OK) fail(DOGATNONE);

    u32 n = ULOGCount(idx);
    if (n == 0) { ULOGClose(data, idx, YES); fail(DOGATNONE); }

    ulogrec rec = {};
    b8 found = NO;
    for (u32 i = n; i > 0; ) {
        i--;
        ok64 ro = ULOGRow(data, idx, i, &rec);
        if (ro != OK) continue;
        uri u = rec.uri;

        //  Canonical at-log shape: `?<branch>#<curhash>` — fragment
        //  carries the sha, query carries the be-branch (empty for
        //  trunk).  Legacy rows kept the sha as a query spec; fall
        //  through and walk the `&`-chain when the fragment is empty.
        u8cs ref_body = {}, sha_body = {};
        {
            u8cs frag = {u.fragment[0], u.fragment[1]};
            if (u8csLen(frag) == 40) {
                sha_body[0] = frag[0];
                sha_body[1] = frag[1];
            }
        }
        a_dup(u8c, q, u.query);
        while (!$empty(q)) {
            qref spec = {};
            if (QURYu8sDrain(q, &spec) != OK) break;
            if (spec.type == QURY_NONE) break;
            if (spec.type == QURY_REF && $empty(ref_body)) {
                ref_body[0] = spec.body[0];
                ref_body[1] = spec.body[1];
            } else if (spec.type == QURY_SHA &&
                       $len(spec.body) == 40 &&
                       $empty(sha_body)) {
                sha_body[0] = spec.body[0];
                sha_body[1] = spec.body[1];
            }
        }
        if ($empty(sha_body)) continue;

        u8bReset(branch_out);
        if (!$empty(ref_body)) u8bFeed(branch_out, ref_body);

        u8bReset(sha_out);
        u8bFeed(sha_out, sha_body);

        found = YES;
        break;
    }
    ULOGClose(data, idx, YES);
    if (!found) fail(DOGATNONE);
    done;
}
