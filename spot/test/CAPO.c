#include "spot/CAPOi.h"
#include "spot/SPOT.h"

#include <string.h>

#include "abc/FILE.h"
#include "abc/PRO.h"
#include "abc/SORT.h"
#include "abc/TEST.h"

// --- Test 0: spot64Pack/Type/Id/FnRap roundtrip ---
ok64 CAPO0() {
    sane(1);
    a_cstr(tri, "abc");
    u32 tid = spot64TriId(tri);
    want(tid != 0);
    testeq(tid & ~SPOT64_ID_MASK, (u32)0);

    u64 fn_rap = 0xdeadbeef42ULL;
    spot64 e = spot64Pack(SPOT64_TRI, tid, fn_rap);
    testeq(spot64Type(e),  (u8)SPOT64_TRI);
    testeq(spot64Id(e),    tid);
    testeq(spot64FnRap(e), fn_rap);

    // Same input -> same output
    spot64 e2 = spot64Pack(SPOT64_TRI, tid, fn_rap);
    testeq(e, e2);

    // Different trigram -> different id
    a_cstr(tri2, "xyz");
    want(spot64TriId(tri2) != tid);

    done;
}

// --- Test 1: spot64 entries with same id sort by (type, fn_rap) ---
ok64 CAPO1() {
    sane(1);
    a_cstr(tri, "foo");
    u32 tid = spot64TriId(tri);
    a_cstr(p1, "main.c");
    a_cstr(p2, "other.c");
    u64 r1 = CAPOFnRap40(p1);
    u64 r2 = CAPOFnRap40(p2);

    spot64 e1 = spot64Pack(SPOT64_TRI, tid, r1);
    spot64 e2 = spot64Pack(SPOT64_TRI, tid, r2);
    testeq(spot64Id(e1),   spot64Id(e2));
    testeq(spot64Type(e1), spot64Type(e2));
    want(spot64FnRap(e1) != spot64FnRap(e2));

    done;
}

// --- Test 2: CAPOIndexFile extracts trigrams keyed by basename RAP ---
//  Postings land in the SPOT singleton's hash-set scratch
//  (SPOT.entries); the test walks non-zero slots to inspect them.
ok64 CAPO2() {
    sane(1);
    const char *src = "int foo(int x) { return x + 1; }";
    u8csc source = {(u8cp)src, (u8cp)src + strlen(src)};
    u8cs ext = $u8str(".c");
    a_cstr(name, "test.c");

    call(u64bMap, SPOT.entries, 4096);

    call(CAPOIndexFile, source, ext, name);

    u64 fn_rap = CAPOFnRap40(name);
    a_cstr(tri_foo, "foo");
    a_cstr(tri_int, "int");
    u32 id_foo = spot64TriId(tri_foo);
    u32 id_int = spot64TriId(tri_int);

    size_t nentries = 0;
    b8 found_foo = NO, found_int = NO;
    for (u64 *p = SPOT.entries[0]; p < SPOT.entries[3]; p++) {
        if (*p == 0) continue;
        nentries++;
        testeq(spot64FnRap(*p), fn_rap);
        if (spot64Type(*p) != SPOT64_TRI) continue;
        if (spot64Id(*p) == id_foo) found_foo = YES;
        if (spot64Id(*p) == id_int) found_int = YES;
    }
    want(nentries > 0);
    want(found_foo == YES);
    want(found_int == YES);

    u64bUnMap(SPOT.entries);
    done;
}

// --- Test 3: Sort + HIT dedup ---
ok64 CAPO3() {
    sane(1);
    a_cstr(tri, "foo");
    u32 tid = spot64TriId(tri);
    a_cstr(p1, "a.c");
    a_cstr(p2, "b.c");
    spot64 e1 = spot64Pack(SPOT64_TRI, tid, CAPOFnRap40(p1));
    spot64 e2 = spot64Pack(SPOT64_TRI, tid, CAPOFnRap40(p2));
    spot64 e1dup = e1;

    u64 arr[] = {e2, e1, e1dup};
    u64s data = {arr, arr + 3};
    u64sSort(data);

    want(arr[0] <= arr[1]);
    want(arr[1] <= arr[2]);

    u64cs runs[1] = {{(u64cp)arr, (u64cp)arr + 3}};
    u64css iter = {runs, runs + 1};
    HITu64Start(iter);

    u64 out[3];
    u64p op = out;
    HITu64Merge(iter, &op);

    testeq((size_t)(op - out), (size_t)2);

    done;
}

// --- Test 4: TriChar filter ---
ok64 CAPO4() {
    sane(1);
    want(CAPOTriChar('a') != 0);
    want(CAPOTriChar('Z') != 0);
    want(CAPOTriChar('0') != 0);
    want(CAPOTriChar('_') != 0);

    want(CAPOTriChar(' ') == 0);
    want(CAPOTriChar('(') == 0);
    want(CAPOTriChar('{') == 0);
    want(CAPOTriChar('\n') == 0);

    done;
}

// --- Test 5: HIT seek by (type, id) prefix yields all entries in
//             that bucket; range size is 1<<40 ---
ok64 CAPO5() {
    sane(1);
    a_cstr(t1, "aaa");
    a_cstr(t2, "mmm");
    a_cstr(t3, "zzz");
    a_cstr(p1, "x.c");
    a_cstr(p2, "y.c");

    u64 entries[] = {
        spot64Pack(SPOT64_TRI, spot64TriId(t1), CAPOFnRap40(p1)),
        spot64Pack(SPOT64_TRI, spot64TriId(t1), CAPOFnRap40(p2)),
        spot64Pack(SPOT64_TRI, spot64TriId(t2), CAPOFnRap40(p1)),
        spot64Pack(SPOT64_TRI, spot64TriId(t3), CAPOFnRap40(p2)),
    };
    u64s data = {entries, entries + 4};
    u64sSort(data);

    u64cs runs[1] = {{(u64cp)entries, (u64cp)entries + 4}};
    u64css iter = {runs, runs + 1};
    HITu64Start(iter);

    u64 prefix = spot64Pack(SPOT64_TRI, spot64TriId(t2), 0);
    ok64 o = HITu64Seek(iter, &prefix);
    want(o == OK);
    want(!$empty(iter));
    testeq(spot64Id(*(*iter[0])[0]),   spot64TriId(t2));
    testeq(spot64Type(*(*iter[0])[0]), (u8)SPOT64_TRI);

    done;
}

// --- Test 6: spot64 accessors on TRI/MEN/DEF entries ---
ok64 CAPO6() {
    sane(1);
    a_cstr(name, "myFunc");
    a_cstr(path, "main.c");
    u64 fn_rap = CAPOFnRap40(path);
    u32 sid    = spot64SymId(name);

    spot64 men = spot64Pack(SPOT64_MEN, sid, fn_rap);
    spot64 def = spot64Pack(SPOT64_DEF, sid, fn_rap);
    a_cstr(tri6, "foo");
    spot64 tri_e = spot64Pack(SPOT64_TRI, spot64TriId(tri6), fn_rap);

    testeq(spot64Type(men),   (u8)SPOT64_MEN);
    testeq(spot64Type(def),   (u8)SPOT64_DEF);
    testeq(spot64Type(tri_e), (u8)SPOT64_TRI);

    // Same name -> same id for both mention and definition
    testeq(spot64Id(men), spot64Id(def));

    // fn_rap roundtrip
    testeq(spot64FnRap(men), fn_rap);
    testeq(spot64FnRap(def), fn_rap);

    a_cstr(name2, "otherFunc");
    spot64 men2 = spot64Pack(SPOT64_MEN, spot64SymId(name2), fn_rap);
    want(spot64Id(men2) != spot64Id(men));

    done;
}

// --- Test 7: CAPOIndexFile emits both trigram and symbol entries ---
ok64 CAPO7() {
    sane(1);
    const char *src = "int foo(int x) { return x + 1; }";
    u8csc source = {(u8cp)src, (u8cp)src + strlen(src)};
    u8cs ext = $u8str(".c");
    a_cstr(name, "test.c");

    call(u64bMap, SPOT.entries, 4096);

    call(CAPOIndexFile, source, ext, name);

    u64 fn_rap = CAPOFnRap40(name);
    size_t nentries = 0, ntri = 0, nmen = 0, ndef = 0;
    for (u64 *p = SPOT.entries[0]; p < SPOT.entries[3]; p++) {
        if (*p == 0) continue;
        nentries++;
        testeq(spot64FnRap(*p), fn_rap);
        u8 t = spot64Type(*p);
        if (t == SPOT64_TRI) ntri++;
        else if (t == SPOT64_MEN) nmen++;
        else if (t == SPOT64_DEF) ndef++;
    }
    want(nentries > 0);
    want(ntri > 0);
    want(nmen + ndef > 0);

    u64bUnMap(SPOT.entries);
    done;
}

// --- Test 8: same id, different types sort TRI < MEN < DEF ---
ok64 CAPO8() {
    sane(1);
    // Pick a literal 18-bit id that all three entries share, so the
    // sort ordering across types is decided by the type field alone.
    u32 id = 0x12345 & SPOT64_ID_MASK;
    u64 fn_rap = 0xabcdef0042ULL;

    spot64 tri_e = spot64Pack(SPOT64_TRI, id, fn_rap);
    spot64 men_e = spot64Pack(SPOT64_MEN, id, fn_rap);
    spot64 def_e = spot64Pack(SPOT64_DEF, id, fn_rap);

    want(tri_e < men_e);
    want(men_e < def_e);

    u64 arr[] = {def_e, tri_e, men_e};
    u64s data = {arr, arr + 3};
    u64sSort(data);
    testeq(spot64Type(arr[0]), (u8)SPOT64_TRI);
    testeq(spot64Type(arr[1]), (u8)SPOT64_MEN);
    testeq(spot64Type(arr[2]), (u8)SPOT64_DEF);

    done;
}

// --- Test BN: same basename in two dirs collides into one fn_rap ---
ok64 CAPObasenameCollision() {
    sane(1);
    a_cstr(b1, "README.md");
    a_cstr(b2, "README.md");
    a_cstr(b3, "OTHER.md");
    testeq(CAPOFnRap40(b1), CAPOFnRap40(b2));
    want(CAPOFnRap40(b3) != CAPOFnRap40(b1));
    done;
}

// --- Test A: HunkEmit produces valid TLV that roundtrips via Drain ---

ok64 CAPOtestHunkEmit() {
    sane(1);
    char tmppath[] = "/tmp/spot_hunk_test_XXXXXX";
    int fd = mkstemp(tmppath);
    test(fd >= 0, FAILSANITY);

    spot_emit   = HUNKu8sFeed;
    spot_out_fd = fd;
    call(LESSArenaInit);

    const char *src = "void foo() {\n    int x = 1;\n    int y = 2;\n}\n";
    u8csc source = {(u8cp)src, (u8cp)src + strlen(src)};
    u8cs ext = $u8str(".c");
    Bu32 toks = {};
    call(u32bMap, toks, strlen(src) + 1);
    call(SPOTTokenize, toks, source, ext);
    u32cs htoks = {(u32cp)u32bDataHead(toks), (u32cp)u32bIdleHead(toks)};

    range32 hls[1] = {{18, 28}};
    b8 first = YES;
    call(CAPOBuildHunk, source, htoks, 0, (u32)strlen(src),
         hls, 1, ext, "test.c", YES, &first);

    close(fd);
    spot_out_fd = -1;
    spot_emit   = NULL;

    u8bp mapped = NULL;
    a_pad(u8, pathbuf, 256);
    u8cs ps = {(u8cp)tmppath, (u8cp)tmppath + strlen(tmppath)};
    call(u8bFeed, pathbuf, ps);
    call(PATHu8bTerm, pathbuf);
    call(FILEMapRO, &mapped, $path(pathbuf));

    a_dup(u8c, data, u8bDataC(mapped));
    want($len(data) > 20);

    hunk h = {};
    ok64 o = HUNKu8sDrain(data, &h);
    testeq(o, OK);
    want(!$empty(h.text));
    want(!$empty(h.uri));
    want($len(h.text) == strlen(src));
    want(memcmp(h.text[0], src, strlen(src)) == 0);
    want($len(h.uri) >= 6);
    want(memcmp(h.uri[0], "test.c", 6) == 0);

    FILEUnMap(mapped);
    u32bUnMap(toks);
    LESSArenaCleanup();
    unlink(tmppath);
    done;
}

// --- Test B: CAPOKnownExt recognizes standard extensions ---
ok64 CAPOtestKnownExt() {
    sane(1);
    u8cs c_ext = $u8str(".c");
    u8cs h_ext = $u8str(".h");
    u8cs py_ext = $u8str(".py");
    u8cs go_ext = $u8str(".go");
    u8cs txt_ext = $u8str(".xyz_unknown");

    want(CAPOKnownExt(c_ext) == YES);
    want(CAPOKnownExt(h_ext) == YES);
    want(CAPOKnownExt(py_ext) == YES);
    want(CAPOKnownExt(go_ext) == YES);
    want(CAPOKnownExt(txt_ext) == NO);

    u8cs empty = {};
    want(CAPOKnownExt(empty) == NO);
    done;
}

ok64 CAPOtest() {
    sane(1);
    call(CAPO0);
    call(CAPO1);
    call(CAPO2);
    call(CAPO3);
    call(CAPO4);
    call(CAPO5);
    call(CAPO6);
    call(CAPO7);
    call(CAPO8);
    call(CAPObasenameCollision);
    call(CAPOtestHunkEmit);
    call(CAPOtestKnownExt);
    done;
}

TEST(CAPOtest);
