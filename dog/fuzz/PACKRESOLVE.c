//  PACKRESOLVE fuzz (GIT-004): the OFS-only delta resolver must never
//  crash / OOB-read on adversarial pack bytes — only return a bounded
//  error.  Treat the first 4 input bytes as a big-endian start offset,
//  the rest as the pack body, and resolve.  A well-formed result is
//  fine; a corrupt one must yield an error code, never a fault.
//
//  Seed corpus: feed real OFS-only `.keeper` logs (see fuzz run notes).

#include "git/PACK.h"

#include "abc/B.h"
#include "abc/PRO.h"
#include "abc/S.h"
#include "abc/TEST.h"

#define FZ_SCRATCH (1u << 22)   // 4 MiB base + 4 MiB delta scratch

//  One probe in its own frame so the BASS carves are rewound per call
//  (abc.mkd §BASS).  A resolver error is the EXPECTED outcome on
//  adversarial input, so this worker swallows it and always returns OK;
//  only a true fault (ASan abort / OOB) surfaces as a crash.
static ok64 resolve_probe(u8cs pack, u64 off) {
    sane(u8csOK(pack));
    a_carve(u8, bb, FZ_SCRATCH);
    a_carve(u8, db, FZ_SCRATCH);
    u8s bsc = {u8bHead(bb), u8bTerm(bb)};
    u8s dsc = {u8bHead(db), u8bTerm(db)};
    u8cs body = {};
    u8 type = 0;
    ok64 rc = PACKResolveOfs(pack, off, bsc, dsc, body, &type);
    if (rc == OK) {
        must(type >= 1 && type <= 4, "bad resolved type");
        must($ok(body), "bad result slice");
    }
    done;
}

FUZZ(u8, PACKRESOLVEfuzz) {
    sane(1);
    if ($len(input) < 5) done;
    if ($len(input) > (1u << 20)) done;   // bound work per case

    //  First 4 bytes → start offset; remainder → pack bytes.
    u64 off = ((u64)input[0][0] << 24) | ((u64)input[0][1] << 16) |
              ((u64)input[0][2] << 8) | (u64)input[0][3];
    u8cs pack = {input[0] + 4, input[1]};

    //  `call` snapshots+restores BASS, so each probe's carves die here
    //  instead of accumulating across fuzz iterations.
    call(resolve_probe, pack, off);
    done;
}
