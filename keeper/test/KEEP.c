#include "keeper/KEEP.h"
#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PRO.h"
#include "abc/TEST.h"
#include "dog/git/GIT.h"
#include "dog/git/PACK.h"

// --- wh64 hashlet tests ---

ok64 WH64hashlet() {
    sane(1);

    // SHA: 816fb46be665c8b63647...
    u8 sha[20] = {0x81, 0x6f, 0xb4, 0x6b, 0xe6, 0x65, 0xc8, 0xb6,
                  0x36, 0x47, 0xf0, 0x09, 0x68, 0x45, 0xfe, 0xf3,
                  0x63, 0x73, 0x6b, 0x20};
    sha1 s = {};
    memcpy(s.data, sha, 20);
    u64 hashlet = WHIFFHashlet40(&s);

    // Hex output should match SHA prefix
    a_pad(u8, hex, 12);
    WHIFFHexFeed40(hex_idle, hashlet);
    want(memcmp(u8bDataHead(hex), "816fb46be6", 10) == 0);

    // FromHex round-trip
    a_cstr(h2hex, "816fb46be6");
    u64 h2 = WHIFFHexHashlet40(h2hex);
    want(h2 == hashlet);

    // Short prefix: WHIFFHexHashlet40 with 7 chars, mask compare
    a_cstr(h7hex, "816fb46");
    u64 h7 = WHIFFHexHashlet40(h7hex);
    u64 mask7 = ~((1ULL << (40 - 28)) - 1) & WHIFF_HASHLET40_MASK;
    want((hashlet & mask7) == (h7 & mask7));

    a_cstr(h7bad, "816fb47");
    u64 hbad = WHIFFHexHashlet40(h7bad);
    want((hashlet & mask7) != (hbad & mask7));

    done;
}

// --- wh64 pack/unpack tests ---

ok64 WH64pack() {
    sane(1);

    wh64 v = wh64Pack(5, 1000, 0xABCDEF0123ULL);
    want(wh64Type(v) == 5);
    want(wh64Id(v) == 1000);
    want(wh64Off(v) == 0xABCDEF0123ULL);

    wh64 vmax = wh64Pack(0xf, WHIFF_ID_MASK, WHIFF_OFF_MASK);
    want(wh64Type(vmax) == 0xf);
    want(wh64Id(vmax) == WHIFF_ID_MASK);
    want(wh64Off(vmax) == WHIFF_OFF_MASK);

    wh64 v0 = wh64Pack(0, 0, 0);
    want(wh64Type(v0) == 0);
    want(wh64Id(v0) == 0);
    want(wh64Off(v0) == 0);

    done;
}

// --- keeper open/close on empty store ---

ok64 KEEPempty() {
    sane(1);
    call(FILEInit);

    char tmpdir[] = "/tmp/keeper-test-XXXXXX";
    want(mkdtemp(tmpdir) != NULL);

    u8cs root = {(u8cp)tmpdir, (u8cp)tmpdir + strlen(tmpdir)};
    home h = {};
    call(HOMEOpenAt, &h, root, YES);
    
    call(KEEPOpen, &h, YES);
    want(kv64bDataLen(KEEP.packs) == 0);
    want(DOGPupCount(KEEP.puppies) == 0);

    a_cstr(_h, "abcdef");
    u64 hashlet = WHIFFHexHashlet60(_h);
    u64 val = 0;
    want(KEEPLookup(hashlet, 6, &val) == KEEPNONE);
    want(KEEPHas(hashlet, 6) == KEEPNONE);

    call(KEEPClose);
    HOMEClose(&h);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);

    done;
}

ok64 KEEPput() {
    sane(1);
    call(FILEInit);

    char tmpdir[] = "/tmp/keeper-put-XXXXXX";
    want(mkdtemp(tmpdir) != NULL);

    a_cstr(root, tmpdir);
    home h = {};
    call(HOMEOpenAt, &h, root, YES);
    
    call(KEEPOpen, &h, YES);
    want(kv64bDataLen(KEEP.packs) == 0);

    // Store two blobs
    a_cstr(blob1, "hello world\n");
    a_cstr(blob2, "goodbye world\n");
    u8csc objs[2] = {
        {blob1[0], blob1[1]},
        {blob2[0], blob2[1]},
    };
    wh64 wh[2] = {
        wh64Pack(DOG_OBJ_BLOB, 0, 0),
        wh64Pack(DOG_OBJ_BLOB, 0, 0),
    };

    call(KEEPPut, objs, wh, 2);
    want(kv64bDataLen(KEEP.packs) == 1);
    want(DOGPupCount(KEEP.puppies) == 1);

    // Both whiffs should have valid types
    want(wh64Type(wh[0]) == DOG_OBJ_BLOB);
    want(wh64Type(wh[1]) == DOG_OBJ_BLOB);

    // Compute 60-bit hashlets from known blob content
    // "hello world\n" SHA = 3b18e512dba79e4c8300dd08aeb37f8e728b8dad
    u8 sha0[20] = {0x3b,0x18,0xe5,0x12,0xdb,0xa7,0x9e,0x4c,0x83,0x00,
                   0xdd,0x08,0xae,0xb3,0x7f,0x8e,0x72,0x8b,0x8d,0xad};
    u64 h0 = WHIFFHashlet60((sha1cp)sha0);
    // "goodbye world\n" — compute via git hash-object
    a_cstr(_ce, "ce0136");
    u64 h1 = WHIFFHexHashlet60(_ce);  // just need a prefix

    // Should be retrievable by 7-char prefix (git default)
    a_cstr(_3b, "3b18e51");
    want(KEEPHas(WHIFFHexHashlet60(_3b), 7) == OK);

    // Get content back
    Bu8 out = {};
    call(u8bMap, out, 1UL << 20);
    u8 obj_type = 0;
    call(KEEPGet, h0, 15, out, &obj_type);
    want(obj_type == DOG_OBJ_BLOB);
    want(u8bDataLen(out) == u8csLen(blob1));
    want(memcmp(u8bDataHead(out), blob1[0], u8csLen(blob1)) == 0);

    u8bUnMap(out);
    call(KEEPClose);
    HOMEClose(&h);

    a_pad(u8, rmbuf, 256);
    a_cstr(rmcmd, "rm -rf ");
    u8bFeed(rmbuf, rmcmd);
    u8bFeed(rmbuf, root);
    u8bFeed1(rmbuf, 0);
    system((char *)u8bDataHead(rmbuf));

    done;
}

ok64 KEEPpackIncremental() {
    sane(1);
    call(FILEInit);

    char tmpdir[] = "/tmp/keeper-pack-XXXXXX";
    want(mkdtemp(tmpdir) != NULL);

    a_cstr(root, tmpdir);
    home h = {};
    call(HOMEOpenAt, &h, root, YES);
    
    call(KEEPOpen, &h, YES);

    keep_pack p = {};
    call(KEEPPackOpen, &p);

    //  Pre-compute the blob SHA so we can build the tree object
    //  first (intra-pack order: commit→tree→blob→tag).
    //  "hello world\n" — git SHA-1 = 3b18e512dba79e4c8300dd08aeb37f8e728b8dad.
    a_cstr(blob_content, "hello world\n");
    sha1 blob_sha = {};
    KEEPObjSha(&blob_sha, DOG_OBJ_BLOB, blob_content);

    u8 expected_blob_sha[20] = {
        0x3b,0x18,0xe5,0x12,0xdb,0xa7,0x9e,0x4c,0x83,0x00,
        0xdd,0x08,0xae,0xb3,0x7f,0x8e,0x72,0x8b,0x8d,0xad};
    want(memcmp(blob_sha.data, expected_blob_sha, 20) == 0);

    //  Build tree content: "100644 hello.txt\0" + 20-byte blob SHA.
    a_pad(u8, tree_buf, 256);
    a_cstr(tree_mode, "100644 hello.txt");
    u8bFeed(tree_buf, tree_mode);
    u8bFeed1(tree_buf, 0);  // NUL separator
    u8cs sha_slice = {blob_sha.data, blob_sha.data + 20};
    u8bFeed(tree_buf, sha_slice);

    a_dup(u8c, tree_content, u8bData(tree_buf));
    sha1 tree_sha = {};
    //  Feed tree first (type 2), then blob (type 3) — monotone order.
    call(KEEPPackFeed, &p, DOG_OBJ_TREE, tree_content, 0, &tree_sha);
    sha1 blob_sha2 = {};
    call(KEEPPackFeed, &p, DOG_OBJ_BLOB, blob_content, 0, &blob_sha2);
    want(memcmp(blob_sha.data, blob_sha2.data, 20) == 0);

    // Verify tree SHA matches git: 68aba62e560c0ebc3396e8ae9335232cd93a3f60
    u8 expected_tree_sha[20] = {
        0x68,0xab,0xa6,0x2e,0x56,0x0c,0x0e,0xbc,0x33,0x96,
        0xe8,0xae,0x93,0x35,0x23,0x2c,0xd9,0x3a,0x3f,0x60};
    want(memcmp(tree_sha.data, expected_tree_sha, 20) == 0);

    call(KEEPPackClose, &p);
    want(kv64bDataLen(KEEP.packs) == 1);
    want(DOGPupCount(KEEP.puppies) == 1);

    // Retrieve blob by 7-char prefix (git default)
    u64 blob_hashlet = WHIFFHashlet60(&blob_sha);
    Bu8 out = {};
    call(u8bMap, out, 1UL << 20);
    u8 obj_type = 0;
    a_cstr(_bh, "3b18e51");
    call(KEEPGet, WHIFFHexHashlet60(_bh), 7, out, &obj_type);
    want(obj_type == DOG_OBJ_BLOB);
    want(u8bDataLen(out) == u8csLen(blob_content));
    want(memcmp(u8bDataHead(out), blob_content[0], u8csLen(blob_content)) == 0);

    // Retrieve tree by full 15-char hashlet
    u8bReset(out);
    u64 tree_hashlet = WHIFFHashlet60(&tree_sha);
    call(KEEPGet, tree_hashlet, 15, out, &obj_type);
    want(obj_type == DOG_OBJ_TREE);
    want(u8bDataLen(out) == u8csLen(tree_content));
    want(memcmp(u8bDataHead(out), tree_content[0], u8csLen(tree_content)) == 0);

    u8bUnMap(out);
    call(KEEPClose);
    HOMEClose(&h);

    a_pad(u8, rmbuf, 256);
    a_cstr(rmcmd, "rm -rf ");
    u8bFeed(rmbuf, rmcmd);
    u8bFeed(rmbuf, root);
    u8bFeed1(rmbuf, 0);
    system((char *)u8bDataHead(rmbuf));

    done;
}

// --- Phase 1c: KEEPBranchDrop preconditions ---

typedef struct {
    char const *input;
    ok64        expect;
} BranchDropCase;

ok64 KEEPBranchDropTable() {
    sane(1);
    call(FILEInit);

    char tmpdir[] = "/tmp/keeper-drop-XXXXXX";
    want(mkdtemp(tmpdir) != NULL);

    a_cstr(root, tmpdir);
    home h = {};
    call(HOMEOpenAt, &h, root, YES);
    call(KEEPOpen, &h, YES);

    //  Trunk aliases must all refuse with KEEPTRUNK — none may be
    //  dropped because trunk carries the paths registry plus the
    //  root-level `refs` file (which also holds host aliases).
    BranchDropCase const cases[] = {
        {"",              KEEPTRUNK},
        {"main",          KEEPTRUNK},
        {"master",        KEEPTRUNK},
        {"trunk",         KEEPTRUNK},
        {"heads/main",    KEEPTRUNK},
        {"heads/master",  KEEPTRUNK},
        {"heads/trunk",   KEEPTRUNK},
        //  Flat store: non-trunk drop is an idempotent OK (REFS
        //  tombstone; shared object pool untouched).
        {"feature",       OK},
        {"heads/feature", OK},
        {"tags/v0.0.1",   OK},
        {"feature/fix1",  OK},
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        u8cs in = {(u8cp)cases[i].input,
                   (u8cp)cases[i].input + strlen(cases[i].input)};
        ok64 got = KEEPBranchDrop(in);
        if (got != cases[i].expect) {
            fprintf(stderr, "FAIL drop[%zu] '%s': got %s want %s\n",
                    i, cases[i].input,
                    ok64str(got), ok64str(cases[i].expect));
            fail(TESTFAIL);
        }
    }

    call(KEEPClose);
    HOMEClose(&h);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);
    done;
}

// --- Step 2: branch open/create/drop round-trip ---

#include <sys/stat.h>

ok64 KEEPbranchRoundTrip() {
    sane(1);
    call(FILEInit);

    char tmpdir[] = "/tmp/keeper-branch-XXXXXX";
    want(mkdtemp(tmpdir) != NULL);

    a_cstr(root, tmpdir);
    home h = {};
    call(HOMEOpenAt, &h, root, YES);

    //  Phase 1: open trunk first (creates the .be dir + lock), then
    //  use KEEPCreateBranch to materialise nested feat/ → feat/fix.
    call(KEEPOpen, &h, YES);
    call(KEEPClose);

    //  Re-open from a fresh home for create.  Flat store: creating any
    //  branch label just records a REFS row — no per-branch dir and no
    //  parent ordering requirement.
    a_cstr(feat,    "feat");
    a_cstr(featfix, "feat/fix");

    //  Any label is OK in the flat model (no KEEPNONE parent gate).
    want(KEEPCreateBranch(&h, featfix) == OK);

    //  Create "feat".
    want(KEEPCreateBranch(&h, feat) == OK);

    //  Idempotent re-create → OK (no KEEPDUP in the flat model).
    want(KEEPCreateBranch(&h, feat) == OK);

    //  Nested label again → OK.
    want(KEEPCreateBranch(&h, featfix) == OK);

    //  Open feat/fix.  Trunk + feat + feat/fix all exist on disk.
    call(KEEPOpenBranch, &h, featfix, YES);
    //  Canonical leaf_branch carries a trailing '/'.
    a_cstr(featfix_canon, "feat/fix/");
    {
        a_dup(u8c, leaf, u8bDataC(KEEP.h->cur_branch));
        want(u8csLen(leaf) == u8csLen(featfix_canon));
        want(memcmp(leaf[0], featfix_canon[0],
                    u8csLen(featfix_canon)) == 0);
    }
    want(KEEP.lock_fd >= 0);

    //  Flat shard lock lives at <root>/.be/.lock (HOMEBranchDir
    //  ignores the branch arg; no project set in this test).
    {
        a_pad(u8, p, 256);
        u8bFeed(p, root);
        a_cstr(rel, "/" DOG_BE_NAME "/.lock");
        u8bFeed(p, rel);
        u8bFeed1(p, 0);
        struct stat st = {};
        want(stat((char *)u8bDataHead(p), &st) == 0);
    }

    //  Flat-model property: ingest one object on the open leaf, then
    //  verify it survives a branch drop — objects live in one shared
    //  pool; a drop only tombstones the REFS row.
    a_cstr(blob, "flat store object survives branch drop");
    u8csc objs[1] = {
        {blob[0], blob[1]},
    };
    wh64 wh[1] = {
        wh64Pack(DOG_OBJ_BLOB, 0, 0),
    };
    sha1 blob_sha = {};
    KEEPObjSha(&blob_sha, DOG_OBJ_BLOB, objs[0]);
    u64 obj_hashlet = WHIFFHashlet60(&blob_sha);
    call(KEEPPut, objs, wh, 1);
    want(KEEPHas(obj_hashlet, 15) == OK);

    call(KEEPClose);

    //  Reopen on trunk to exercise the drops.
    call(KEEPOpen, &h, YES);

    //  Object still present after reopen (shared pool, not branch-owned).
    want(KEEPHas(obj_hashlet, 15) == OK);

    //  Flat store: dropping any non-trunk branch is an idempotent OK
    //  (REFS tombstone only; no KEEPDIRTY, no dir unlink).
    want(KEEPBranchDrop(feat) == OK);
    call(KEEPBranchDrop, featfix);

    //  Objects linger: still retrievable after the branch drops.
    want(KEEPHas(obj_hashlet, 15) == OK);

    call(KEEPClose);
    HOMEClose(&h);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);
    done;
}

// --- MEM-022: KEEPGetPacked OFS_DELTA offset underflow ---
//
//  Build a 2-object pack (v0 raw blob + v1 OFS_DELTA against v0) with
//  the real writer, then corrupt v1's on-disk ofs_delta varint to a
//  value larger than v1's own pack offset.  KEEPGetPacked does
//  `cur = cur - obj.ofs_delta` with no underflow guard, so `cur`
//  wraps to a wild base pointer and `{pack+cur, pack+packlen}` reads
//  out of bounds (ASan trap in Debug; UB / wild read in Release where
//  sane() is compiled out).  After the fix the resolver must reject
//  the underflow and return KEEPFAIL — no OOB.

//  Locate the OFS_DELTA object in `pack` (after the 12-byte header)
//  and return, in `*ofs_pos` / `*ofs_len`, the byte position+length of
//  its negative-offset varint (the bytes right after the type/size
//  varint).  Returns OK iff a single OFS_DELTA object was found.
static ok64 find_ofs_varint(u8csc pack, u64 *ofs_pos, u64 *ofs_len) {
    sane(ofs_pos && ofs_len);
    //  Plain by-value slice copies (two pointers) — NOT a_dup, which
    //  would carve BASS that the nested call() rewinds out from under us.
    u8cs scan = {pack[0], pack[1]};
    pack_hdr hdr = {};
    call(PACKDrainHdr, scan, &hdr);
    for (u32 i = 0; i < hdr.count; i++) {
        //  Peek the full header (copy) to learn the type + header end.
        u8cs peek = {scan[0], scan[1]};
        pack_obj obj = {};
        call(PACKDrainObjHdr, peek, &obj);
        if (obj.type == PACK_OBJ_OFS_DELTA) {
            //  Re-scan just the type/size varint to find where the
            //  ofs varint starts (first header byte after the
            //  continuation run).
            u8cs vs = {scan[0], scan[1]};
            u8 c = 0;
            call(u8sDrain8, vs, &c);
            while (c & 0x80) call(u8sDrain8, vs, &c);
            u64 ofs_start = (u64)(vs[0] - pack[0]);
            u64 hdr_end   = (u64)(peek[0] - pack[0]);
            *ofs_pos = ofs_start;
            *ofs_len = hdr_end - ofs_start;
            done;
        }
        //  Advance scan past this object's header + zlib body.
        pack_obj o2 = {};
        call(PACKDrainObjHdr, scan, &o2);
        static u8 sink[1 << 16];
        u8s into = {sink, sink + sizeof(sink)};
        call(PACKInflate, scan, into, o2.size);
    }
    return KEEPFAIL;
}

ok64 KEEPofsUnderflow() {
    sane(1);
    call(FILEInit);

    char tmpdir[] = "/tmp/keeper-ofs-XXXXXX";
    want(mkdtemp(tmpdir) != NULL);
    a_cstr(root, tmpdir);

    home h = {};
    call(HOMEOpenAt, &h, root, YES);
    call(KEEPOpen, &h, YES);

    //  Pack 1: v0 raw + v1 OFS_DELTA(v0).  Mirrors DELTA_ROUND pack 1.
    //  v0 is deliberately large and poorly-compressible (pseudo-random
    //  hex) so v1's OFS_DELTA sits >127 bytes into the pack — that makes
    //  the on-disk ofs varint MULTI-byte, so we can patch it to a value
    //  thousands of bytes large.  The resulting `cur - ofs_delta`
    //  underflow then lands `pack + cur` far (≈ -16k) before the mmap,
    //  i.e. a *deterministic* OOB (ASan trap) rather than a near-miss
    //  inside the same page.
    a_pad(u8, v0b, 1024);
    {
        u32 s = 0x9e3779b9;
        for (int i = 0; i < 512; i++) {
            s = s * 1664525u + 1013904223u;
            u8 nib = (u8)((s >> 24) & 0xf);
            u8bFeed1(v0b, (u8)(nib < 10 ? '0' + nib : 'a' + nib - 10));
        }
    }
    a_dup(u8c, v0, u8bData(v0b));
    //  v1 = v0 with a short suffix appended (a near-copy → delta-able).
    a_pad(u8, v1b, 1024);
    u8bFeed(v1b, v0);
    a_cstr(suffix, " THE END.");
    u8bFeed(v1b, suffix);
    a_dup(u8c, v1, u8bData(v1b));

    sha1 sha0 = {}, sha1v = {};
    keep_pack p = {};
    call(KEEPPackOpen, &p);
    p.strict_order = NO;
    call(KEEPPackFeed, &p, DOG_OBJ_BLOB, v0, 0, &sha0);
    call(KEEPPackFeed, &p, DOG_OBJ_BLOB, v1, WHIFFHashlet60(&sha0), &sha1v);
    call(KEEPPackClose, &p);
    call(KEEPClose);

    //  Corrupt v1's ofs_delta on disk: map the log RW, find the ofs
    //  varint, overwrite it with the max value its byte-length can
    //  hold so the delta exceeds v1's pack offset (underflow).
    a_pad(u8, lp, 1100);
    u8bFeed(lp, root);
    a_cstr(rel, "/" DOG_BE_NAME "/0000000001.keeper");
    u8bFeed(lp, rel);
    u8cs lpt = {u8bDataHead(lp), u8bIdleHead(lp)};
    u8bp logmap = NULL;
    call(FILEMapRW, &logmap, lpt);

    //  RW maps land the file bytes in the IDLE region (FILEMapFD leaves
    //  data empty for PROT_WRITE), so read+patch off the idle slice.
    u8cs logbytes = {u8bIdleHead(logmap), u8bTerm(logmap)};
    u8p  logbase  = u8bIdleHead(logmap);
    u64 ofs_pos = 0, ofs_len = 0;
    ok64 fr = find_ofs_varint(logbytes, &ofs_pos, &ofs_len);
    want(fr == OK);
    want(ofs_len >= 2);  // multi-byte → patchable to a far underflow

    //  Overwrite the ofs varint with the maximal value its byte-length
    //  can hold (MSB-first per PACKDrainOfs): [0xff]*(len-1) + [0x7f].
    //  With a 2-byte varint the decoded ofs_delta is ~16511, far past
    //  v1's pack offset, so `cur = cur - ofs_delta` underflows to a huge
    //  value (`pack + cur` ≈ pack-16k).  Same byte-length keeps the
    //  following zlib body position valid; only the decoded value grows.
    {
        u8s patch = {logbase + ofs_pos, logbase + ofs_pos + ofs_len};
        for (u64 i = 0; i + 1 < ofs_len; i++) patch[0][i] = 0xff;
        patch[0][ofs_len - 1] = 0x7f;
    }
    FILEUnMap(logmap);

    //  Reopen (re-scans the existing idx, whose v1 val still points at
    //  the OFS_DELTA object at its correct offset) and resolve v1.
    //  Before the fix: OOB read inside KEEPGetPacked (ASan abort).
    //  After the fix: bounded KEEPFAIL, no OOB.
    home h2 = {};
    call(HOMEOpenAt, &h2, root, YES);
    call(KEEPOpen, &h2, YES);

    Bu8 out = {};
    call(u8bMap, out, 1UL << 20);
    u8 ot = 0;
    u64 v1_hashlet = WHIFFHashlet60(&sha1v);
    ok64 got = KEEPGet(v1_hashlet, 15, out, &ot);
    if (got != KEEPFAIL) {
        fprintf(stderr,
                "KEEPofsUnderflow: expected KEEPFAIL, got %s\n",
                ok64str(got));
        fail(TESTFAIL);
    }

    u8bUnMap(out);
    call(KEEPClose);
    HOMEClose(&h2);
    HOMEClose(&h);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);
    done;
}

// --- MEM-022: deep cross-file REF_DELTA recursion (stack-overflow DoS) ---
//
//  Lay down two raw `.keeper` packs that form a REF_DELTA cycle across
//  two file_ids: pack 1's object X is REF_DELTA(base=Y), pack 2's
//  object Y is REF_DELTA(base=X).  The index maps hashlet(X_sha)→pack1
//  and hashlet(Y_sha)→pack2.  KEEPGet(X) then ping-pongs
//  KEEPGetPacked -> keep_get_rec -> KEEPGetPacked over the two
//  file_ids; without a recursion cap each cross-file hop spends a C
//  stack frame → unbounded recursion → stack-overflow DoS.  With the
//  KEEP_XFILE_RECUR_MAX bound the call returns KEEPFAIL after a finite
//  number of hops.  (UNPK's same-pair cycle guard sits on the ingest
//  path; this hand-built corrupt index bypasses it, exercising the
//  resolver's own bound.)

//  Write `bytes` to `<root>/.be/<name>`.
static ok64 write_be_file(u8cs root, char const *name, u8cs bytes) {
    sane($ok(root) && name);
    a_pad(u8, path, 1100);
    u8bFeed(path, root);
    a_cstr(be, "/" DOG_BE_NAME "/");
    u8bFeed(path, be);
    u8cs ns = {(u8cp)name, (u8cp)name + strlen(name)};
    u8bFeed(path, ns);
    u8cs pt = {u8bDataHead(path), u8bIdleHead(path)};
    int fd = -1;
    call(FILECreate, &fd, pt);
    callsafe(FILEFeedAll(fd, bytes), close(fd));
    close(fd);
    done;
}

//  Append a REF_DELTA object header (type=7, size=1) + 20-byte base
//  sha + a single zlib-ish body byte into `pack`.  The body is never
//  inflated (the recursion aborts at base lookup), so its content is
//  irrelevant — it only has to keep the file non-empty.
static void feed_ref_delta_obj(u8bp pack, u8 const base_sha[20]) {
    u8bFeed1(pack, (u8)((PACK_OBJ_REF_DELTA << 4) | 1));  // type|size, no cont
    u8cs sha = {(u8cp)base_sha, (u8cp)base_sha + 20};
    u8bFeed(pack, sha);
    u8bFeed1(pack, 0x00);  // placeholder body byte
}

ok64 KEEPxfileRecursion() {
    sane(1);
    call(FILEInit);

    char tmpdir[] = "/tmp/keeper-xrec-XXXXXX";
    want(mkdtemp(tmpdir) != NULL);
    a_cstr(root, tmpdir);

    //  Open+close once so the .be/ trunk dir + refs skeleton exist.
    home h = {};
    call(HOMEOpenAt, &h, root, YES);
    call(KEEPOpen, &h, YES);
    call(KEEPClose);

    //  Two distinct base shas X and Y (arbitrary — only used as lookup
    //  keys + ref bytes; no content verification on the KEEPGet path).
    u8 X_sha[20], Y_sha[20];
    for (int i = 0; i < 20; i++) { X_sha[i] = (u8)(0x10 + i); Y_sha[i] = (u8)(0xA0 + i); }
    u64 X_h = WHIFFHashlet60((sha1cp)X_sha);
    u64 Y_h = WHIFFHashlet60((sha1cp)Y_sha);

    //  Pack 1 (file_id 1): header + objX = REF_DELTA(base=Y) at off 12.
    a_pad(u8, pk1, 256);
    {
        u8s into = {u8bIdleHead(pk1), u8bTerm(pk1)};
        call(PACKu8sFeedHdr, into, 1);
        u8bFed(pk1, 12);  // PACK header is always 12 bytes
    }
    feed_ref_delta_obj(pk1, Y_sha);

    //  Pack 2 (file_id 2): header + objY = REF_DELTA(base=X) at off 12.
    a_pad(u8, pk2, 256);
    {
        u8s into = {u8bIdleHead(pk2), u8bTerm(pk2)};
        call(PACKu8sFeedHdr, into, 1);
        u8bFed(pk2, 12);
    }
    feed_ref_delta_obj(pk2, X_sha);

    //  Idx 1: one entry keyed by hashlet(X_sha) → (file_id 1, off 12).
    //  Idx 2: one entry keyed by hashlet(Y_sha) → (file_id 2, off 12).
    wh128 e1 = { .key = keepKeyPack(KEEP_OBJ_BLOB, X_h),
                 .val = wh64Pack(KEEP_VAL_FLAGS, 1, 12) };
    wh128 e2 = { .key = keepKeyPack(KEEP_OBJ_BLOB, Y_h),
                 .val = wh64Pack(KEEP_VAL_FLAGS, 2, 12) };
    u8cs idx1 = {(u8cp)&e1, (u8cp)&e1 + sizeof(e1)};
    u8cs idx2 = {(u8cp)&e2, (u8cp)&e2 + sizeof(e2)};

    a_dup(u8c, pk1b, u8bData(pk1));
    a_dup(u8c, pk2b, u8bData(pk2));
    call(write_be_file, root, "0000000001.keeper",     pk1b);
    call(write_be_file, root, "0000000002.keeper",     pk2b);
    call(write_be_file, root, "0000000001.keeper.idx", idx1);
    call(write_be_file, root, "0000000002.keeper.idx", idx2);

    //  Reopen: the scan registers both packs (file_ids 1, 2) + idx runs.
    home h2 = {};
    call(HOMEOpenAt, &h2, root, YES);
    call(KEEPOpen, &h2, YES);

    Bu8 out = {};
    call(u8bMap, out, 1UL << 20);
    u8 ot = 0;
    //  Without the cap this recurses until the C stack overflows; with
    //  it the resolver returns a bounded failure.
    ok64 got = KEEPGet(X_h, 15, out, &ot);
    if (got == OK) {
        fprintf(stderr,
                "KEEPxfileRecursion: expected bounded failure, got OK\n");
        fail(TESTFAIL);
    }

    u8bUnMap(out);
    call(KEEPClose);
    HOMEClose(&h2);
    HOMEClose(&h);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);
    done;
}

// --- CODE-004/005: shared sha40 decode + obj-hdr + cap tokens --------

//  dog/WHIFF.h sha1FromHex: leading-40-hex → sha1, no ptr-arith, full
//  decode, honest ok64.  Valid 40-hex → OK + correct sha1; <40 →
//  BADRANGE; non-hex inside the first 40 → HEXBAD (from HEXu8sDrainSome).
ok64 KEEPhex40Table() {
    sane(1);

    typedef struct { char const *in; ok64 want; } row;
    //  40 valid hex; 40 valid + trailing junk (decode first 40 only);
    //  short (<40 → BADRANGE); non-hex inside the first 40 (→ HEXBAD);
    //  empty (→ BADRANGE).
    row cases[] = {
        {"3b18e512dba79e4c8300dd08aeb37f8e728b8dad",      OK},
        {"3b18e512dba79e4c8300dd08aeb37f8e728b8dad more", OK},
        {"3b18e512dba79e4c8300dd08aeb37f8e728b8da",       BADRANGE}, // 39
        {"Zb18e512dba79e4c8300dd08aeb37f8e728b8dad",      HEXBAD},   // non-hex hi-nibble
        {"",                                              BADRANGE},
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        u8csc v = {(u8cp)cases[i].in, (u8cp)cases[i].in + strlen(cases[i].in)};
        sha1 s = {};
        ok64 got = sha1FromHex(&s, v);
        if (got != cases[i].want) {
            fprintf(stderr, "hex40[%zu] '%s': got %llx want %llx\n",
                    i, cases[i].in, (unsigned long long)got,
                    (unsigned long long)cases[i].want);
            fail(TESTFAIL);
        }
        if (got == OK) {
            //  First 40 chars must round-trip back to the same bytes.
            sha1hex hx = {};
            sha1hexFromSha1(&hx, &s);
            want(memcmp(hx.data, cases[i].in, 40) == 0);
        }
    }
    done;
}

//  KEEPForEachCapToken: SP / TAB / '\n' tokenizer scaffold.
typedef struct { char joined[256]; u32 n; } captok_ctx;
static void captok_collect(u8csc tok, void0p ctx) {
    captok_ctx *c = (captok_ctx *)ctx;
    if (c->n) strncat(c->joined, "|", sizeof(c->joined) - strlen(c->joined) - 1);
    size_t l = u8csLen(tok);
    char tmp[64] = {};
    if (l >= sizeof(tmp)) l = sizeof(tmp) - 1;
    memcpy(tmp, tok[0], l);
    strncat(c->joined, tmp, sizeof(c->joined) - strlen(c->joined) - 1);
    c->n++;
}

ok64 KEEPcapTokenTable() {
    sane(1);

    typedef struct {
        char const *in;
        b8          tab_sep;
        char const *want;   // tokens joined with '|'
        u32         n;
    } row;
    row cases[] = {
        //  Plain SP list.
        {" ofs-delta side-band-64k", NO, "ofs-delta|side-band-64k", 2},
        //  Leading/trailing/multiple SP collapse to empty-token-free.
        {"  a   b  ",                NO, "a|b", 2},
        //  Newline terminates the line; trailing '\n' eaten.
        {"report-status\n",          NO, "report-status", 1},
        //  TAB NOT a separator (WIRE mode) → one token with embedded TAB.
        {"a\tb",                     NO, "a\tb", 1},
        //  TAB IS a separator (RECV mode) → two tokens.
        {"a\tb",                     YES, "a|b", 2},
        //  Empty input → no tokens.
        {"",                         NO, "", 0},
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        u8csc tail = {(u8cp)cases[i].in, (u8cp)cases[i].in + strlen(cases[i].in)};
        captok_ctx c = {};
        KEEPForEachCapToken(tail, cases[i].tab_sep, captok_collect, &c);
        if (c.n != cases[i].n || strcmp(c.joined, cases[i].want) != 0) {
            fprintf(stderr, "captok[%zu] '%s' tab=%d: got n=%u '%s' want n=%u '%s'\n",
                    i, cases[i].in, cases[i].tab_sep, c.n, c.joined,
                    cases[i].n, cases[i].want);
            fail(TESTFAIL);
        }
    }
    done;
}

// --- CODE-004: ancestor / shares-ancestor over a fixture DAG ---------
//
//  Build commit objects bottom-up so each child can name its real
//  parent sha.  A commit body only needs a `tree` line (value need not
//  resolve — the walks follow parent/foster only) plus parent/foster
//  headers; KEEPGetExact just confirms KEEP_OBJ_COMMIT and the header
//  block is drained by GITu8sDrainCommit.

//  Append "<field> <40-hex(sha)>\n" to a commit body buffer.
static void commit_feed_ref(u8bp body, u8csc field, sha1cp sha) {
    u8bFeed(body, field);
    u8bFeed1(body, ' ');
    sha1hex hx = {};
    sha1hexFromSha1(&hx, sha);
    u8cs hs = {hx.data, hx.data + 40};
    u8bFeed(body, hs);
    u8bFeed1(body, '\n');
}

//  Build a commit with the given parents[] (and foster, if non-NULL),
//  feed it into the pack, return its sha.  `tag` differentiates bodies
//  so distinct commits get distinct shas.
static ok64 build_commit(keep_pack *p, char tag, sha1cp const *parents,
                         u32 nparent, sha1cp foster, sha1 *out) {
    sane(p && out);
    a_pad(u8, body, 1024);
    //  tree line: a fixed all-zero 40-hex (need not resolve).
    a_cstr(zero40, "0000000000000000000000000000000000000000");
    u8bFeed(body, GIT_FIELD_TREE);
    u8bFeed1(body, ' ');
    u8bFeed(body, zero40);
    u8bFeed1(body, '\n');
    for (u32 i = 0; i < nparent; i++)
        commit_feed_ref(body, GIT_FIELD_PARENT, parents[i]);
    if (foster) commit_feed_ref(body, GIT_FIELD_FOSTER, foster);
    a_cstr(au, "author x <x> 0 +0000\ncommitter x <x> 0 +0000\n\n");
    u8bFeed(body, au);
    u8bFeed1(body, tag);
    u8bFeed1(body, '\n');
    a_dup(u8c, bc, u8bData(body));
    call(KEEPPackFeed, p, DOG_OBJ_COMMIT, bc, 0, out);
    done;
}

ok64 KEEPancestorTable() {
    sane(1);
    call(FILEInit);

    char tmpdir[] = "/tmp/keeper-anc-XXXXXX";
    want(mkdtemp(tmpdir) != NULL);
    a_cstr(root, tmpdir);
    home h = {};
    call(HOMEOpenAt, &h, root, YES);
    call(KEEPOpen, &h, YES);

    keep_pack p = {};
    call(KEEPPackOpen, &p);
    p.strict_order = NO;

    //  Fixture DAG (parent edges point to ancestors):
    //
    //      A ── B ── C          (linear)
    //      A ── D               (fork off A)
    //      C, D ── M            (merge: M has parents C and D)
    //      X                    (disjoint root, no shared ancestor)
    //      C ── F  via FOSTER   (F fosters C, no parent edge)
    sha1 A={}, B={}, C={}, D={}, M={}, X={}, F={};
    call(build_commit, &p, 'A', NULL, 0, NULL, &A);
    { sha1cp pp[1] = {&A}; call(build_commit, &p, 'B', pp, 1, NULL, &B); }
    { sha1cp pp[1] = {&B}; call(build_commit, &p, 'C', pp, 1, NULL, &C); }
    { sha1cp pp[1] = {&A}; call(build_commit, &p, 'D', pp, 1, NULL, &D); }
    { sha1cp pp[2] = {&C,&D}; call(build_commit, &p, 'M', pp, 2, NULL, &M); }
    call(build_commit, &p, 'X', NULL, 0, NULL, &X);
    call(build_commit, &p, 'F', NULL, 0, &C, &F);   // foster C only
    call(KEEPPackClose, &p);

    typedef struct { sha1 *from; sha1 *target; b8 want; } anc;
    anc acases[] = {
        {&C, &A, YES},   // A is ancestor of C
        {&C, &B, YES},
        {&A, &C, NO},    // C is not ancestor of A
        {&M, &A, YES},   // merge reaches A via both legs
        {&M, &D, YES},
        {&M, &C, YES},
        {&B, &D, NO},    // sibling fork, not ancestor
        {&C, &X, NO},    // disjoint
        {&F, &C, YES},   // foster edge counts for reachability
        {&F, &A, YES},   // …and transitively through C's parents
        {&F, &X, NO},
    };
    for (size_t i = 0; i < sizeof(acases)/sizeof(acases[0]); i++) {
        b8 got = KEEPIsAncestor(acases[i].from, acases[i].target);
        if (got != acases[i].want) {
            fprintf(stderr, "IsAncestor[%zu]: got %d want %d\n",
                    i, got, acases[i].want);
            fail(TESTFAIL);
        }
    }

    typedef struct { sha1 *a; sha1 *b; b8 want; } shr;
    shr scases[] = {
        {&C, &D, YES},   // common ancestor A
        {&B, &D, YES},   // common ancestor A
        {&M, &C, YES},   // M descends from C
        {&C, &X, NO},    // fully disjoint roots → clash
        {&A, &X, NO},
        {&F, &A, YES},   // F fosters C whose closure includes A
        {&F, &X, NO},
    };
    for (size_t i = 0; i < sizeof(scases)/sizeof(scases[0]); i++) {
        b8 got = KEEPSharesAncestor(scases[i].a, scases[i].b);
        if (got != scases[i].want) {
            fprintf(stderr, "SharesAncestor[%zu]: got %d want %d\n",
                    i, got, scases[i].want);
            fail(TESTFAIL);
        }
    }

    call(KEEPClose);
    HOMEClose(&h);
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);
    done;
}

ok64 maintest() {
    sane(1);
    fprintf(stderr, "KEEPhex40Table...\n");
    call(KEEPhex40Table);
    fprintf(stderr, "KEEPcapTokenTable...\n");
    call(KEEPcapTokenTable);
    fprintf(stderr, "KEEPancestorTable...\n");
    call(KEEPancestorTable);
    fprintf(stderr, "WH64hashlet...\n");
    call(WH64hashlet);
    fprintf(stderr, "WH64pack...\n");
    call(WH64pack);
    fprintf(stderr, "KEEPempty...\n");
    call(KEEPempty);
    fprintf(stderr, "KEEPput...\n");
    call(KEEPput);
    fprintf(stderr, "KEEPpackIncremental...\n");
    call(KEEPpackIncremental);
    fprintf(stderr, "KEEPBranchDropTable...\n");
    call(KEEPBranchDropTable);
    fprintf(stderr, "KEEPbranchRoundTrip...\n");
    call(KEEPbranchRoundTrip);
    fprintf(stderr, "KEEPofsUnderflow...\n");
    call(KEEPofsUnderflow);
    fprintf(stderr, "KEEPxfileRecursion...\n");
    call(KEEPxfileRecursion);
    fprintf(stderr, "all passed\n");
    done;
}

TEST(maintest);
