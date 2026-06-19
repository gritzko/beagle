#include "sniff/SNIFF.h"
#include "sniff/AT.h"
#include "sniff/PATCH.h"   // PATCH_SCOPE_*
#include "sniff/DEL.h"
#include "sniff/GET.h"
#include "sniff/LS.h"
#include "sniff/POST.h"
#include "sniff/PUT.h"
#include "sniff/SUBS.h"

#include "graf/GRAF.h"

#include "abc/FILE.h"

#include "dog/test/TESTBE.h"
#include "dog/HUNK.h"

#include <string.h>
#include <unistd.h>

//  Bump a file's atime/mtime forward by `bump_sec` seconds.  Lets
//  tests observe a distinct mtime without a real sleep.
static void bump_mtime(char const *abs_path, int bump_sec) {
    a_path(p);
    a_cstr(s, abs_path);
    if (PATHu8bFeed(p, s) != OK) return;
    (void)FILEBumpTimes($path(p), (i64)bump_sec);
}

#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/TEST.h"
#include "keeper/REFS.h"
#include "dog/git/SHA1.h"
#include "keeper/WALK.h"

// --- Helpers ---

static char g_tmpdir[256];

//  Hermetic scratch root via the shared C test setup (dog/test/TESTBE.h).
static ok64 make_tmpdir(void) {
    sane(1);
    call(TESTBEmkdtemp, g_tmpdir, sizeof(g_tmpdir));
    done;
}

static void rm_tmpdir(void) {
    TESTBErmrf(g_tmpdir);
}

// --- Test: AT helpers (verb constants, baseline, last-post, scan) ---

typedef struct { u32 n; ron60 verbs[8]; u8 paths[8][32]; u8 lens[8]; } pd_capture;

static ok64 pd_collect(ulogreccp rec, void *ctx) {
    pd_capture *c = (pd_capture *)ctx;
    if (c->n >= 8) return OK;
    c->verbs[c->n] = rec->verb;
    u8cs path = {rec->uri.path[0], rec->uri.path[1]};
    u32 l = (u32)$len(path);
    if (l > 32) l = 32;
    memcpy(c->paths[c->n], path[0], l);
    c->lens[c->n] = (u8)l;
    c->n++;
    return OK;
}

static ok64 at_append_uri(ron60 ts, ron60 verb, char const *uri_cstr) {
    sane(1);
    a_pad(u8, urib, MAX_URI_LEN);
    a_cstr(src, uri_cstr);
    u8bFeed(urib, src);
    uri urow = {};
    a_dup(u8c, ud, u8bData(urib));
    call(URIutf8Drain, ud, &urow);
    call(SNIFFAtAppendAt, ts, verb, &urow);
    done;
}

ok64 SNIFFAtHelpers() {
    sane(1);
    call(FILEInit);
    call(make_tmpdir);

    a_cstr(root, g_tmpdir);
    home h = {};
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);
    call(SNIFFOpen, YES);

    //  Verb constants — stable across calls, distinct from each other.
    ron60 vr = SNIFFAtVerbRepo();
    ron60 vg = SNIFFAtVerbGet();
    ron60 vp = SNIFFAtVerbPost();
    ron60 vx = SNIFFAtVerbPatch();
    ron60 vu = SNIFFAtVerbPut();
    ron60 vd = SNIFFAtVerbDelete();
    want(vr != 0 && vg != 0 && vp != 0 && vx != 0 && vu != 0 && vd != 0);
    want(vr != vg && vr != vp && vr != vx && vr != vu && vr != vd);
    want(vg != vp && vg != vx && vg != vu && vg != vd);
    want(vp != vx && vp != vu && vp != vd);
    want(vx != vu && vx != vd);
    want(vu != vd);
    want(SNIFFAtVerbGet() == vg);   // cached

    //  SNIFFOpen writes the row-0 anchor (verb `get`; the get-
    //  unification, wiki/Title.mkd); synthetic ULOG rows start after it.
    ron60 t_repo = 0, v_repo = 0;
    {
        uri ru = {};
        call(SNIFFAtRepo, &ru);
        //  Re-fetch ts/verb via ULOGRow(0) — SNIFFAtRepo only yields
        //  the URI.
        ulogrec _r0 = {};
        call(ULOGRow, SNIFF.log_data, SNIFF.log_idx, 0, &_r0);
        t_repo = _r0.ts; v_repo = _r0.verb; ru = _r0.uri;
        want(v_repo == vg);   // anchor verb is now `get`, not `repo`
        //  URI path is the colocated shard anchor `…/.be/<project>`; the
        //  project (Title) defaults to the wt basename (DIS-024;
        //  Store.mkd "Worktrees and the anchor").
        a_dup(u8c, rp, ru.path);
        char const *bn = strrchr(g_tmpdir, '/');
        bn = bn ? bn + 1 : g_tmpdir;
        char tailbuf[300];
        snprintf(tailbuf, sizeof tailbuf, "%s/%s", DOG_BE_NAME, bn);
        a_cstr(tail, tailbuf);
        want($len(rp) >= $len(tail));
        want(memcmp(rp[1] - $len(tail), tail[0], $len(tail)) == 0);
    }
    ron60 base = t_repo + 1000;

    //  Post-repo log invariants: baseline empty, stamp-set empty except
    //  for the repo stamp itself.  The pd floor (DIS-010: most recent
    //  `get` OR `post`) is the row-0 anchor's ts — the anchor verb is
    //  now `get` (line above), and a `get` is a valid pd boundary per
    //  wiki/POST.mkd §"Boundaries", so the floor is `t_repo`, not 0.
    {
        ron60 ts = 0, verb = 0;
        uri u = {};
        want(SNIFFAtBaseline(&ts, &verb, &u) == ULOGNONE);
        want(SNIFFAtLastPostTs() == t_repo);
        want(!SNIFFAtKnown(base + 9999));
    }

    //  Timeline (offsets from the repo row):
    //    +1000 get    ?heads/main#aaaa...
    //    +1100 put    src/a.c
    //    +1200 put    src/b.c
    //    +1300 delete src/c.c
    //    +1400 post   ?heads/main#bbbb...
    //    +1500 put    src/d.c            (after last post)
    //    +1600 patch  ?heads/main#bbbb...,cccc...
    call(at_append_uri, base + 0,   vg, "?heads/main#aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    call(at_append_uri, base + 100, vu, "src/a.c");
    call(at_append_uri, base + 200, vu, "src/b.c");
    call(at_append_uri, base + 300, vd, "src/c.c");
    call(at_append_uri, base + 400, vp, "?heads/main#bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    call(at_append_uri, base + 500, vu, "src/d.c");
    call(at_append_uri, base + 600, vx, "?heads/main#bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb,cccccccccccccccccccccccccccccccccccccccc");

    //  Stamp-set: exact timestamps are known, neighbours aren't.
    want(SNIFFAtKnown(base + 0));
    want(SNIFFAtKnown(base + 400));
    want(SNIFFAtKnown(base + 600));
    want(!SNIFFAtKnown(base - 1));
    want(!SNIFFAtKnown(base + 550));

    //  Baseline: most recent get/post/patch is the patch at +600.
    {
        ron60 ts = 0, verb = 0;
        uri u = {};
        call(SNIFFAtBaseline, &ts, &verb, &u);
        want(ts == base + 600);
        want(verb == vx);
        a_dup(u8c, frag, u.fragment);
        want($len(frag) > 40);
        b8 has_comma = NO;
        for (u8c const *p = frag[0]; p < frag[1]; p++)
            if (*p == ',') { has_comma = YES; break; }
        want(has_comma);
    }

    //  Last post ts = base + 400 (most recent get/post; the patch at
    //  +600 is not a pd boundary).
    want(SNIFFAtLastPostTs() == base + 400);

    //  Scan put/delete since last post: only src/d.c at +500.
    {
        pd_capture cap = {};
        call(SNIFFAtScanPutDelete, base + 400, pd_collect, &cap);
        want(cap.n == 1);
        want(cap.verbs[0] == vu);
        want(cap.lens[0] == 7);
        want(memcmp(cap.paths[0], "src/d.c", 7) == 0);
    }

    //  Scan from the beginning (floor=0): all four put/delete rows, in order.
    {
        pd_capture cap = {};
        call(SNIFFAtScanPutDelete, 0, pd_collect, &cap);
        want(cap.n == 4);
        want(cap.verbs[0] == vu && cap.lens[0] == 7);
        want(memcmp(cap.paths[0], "src/a.c", 7) == 0);
        want(cap.verbs[1] == vu);
        want(memcmp(cap.paths[1], "src/b.c", 7) == 0);
        want(cap.verbs[2] == vd);
        want(memcmp(cap.paths[2], "src/c.c", 7) == 0);
        want(cap.verbs[3] == vu);
        want(memcmp(cap.paths[3], "src/d.c", 7) == 0);
    }

    //  Scan from a floor past the tail: no rows.
    {
        pd_capture cap = {};
        call(SNIFFAtScanPutDelete, base + 99999, pd_collect, &cap);
        want(cap.n == 0);
    }

    call(SNIFFClose);
    KEEPClose();
    HOMEClose();
    rm_tmpdir();
    done;
}

// --- Test: POST boundary scans match the spec (DIS-010) ---
//  Two boundaries anchored at the latest GET scope what the next POST
//  consumes (wiki/POST.mkd §"Boundaries and guards", sniff/INDEX.md):
//    pd boundary    = most recent `get` OR `post` row.
//    patch boundary = most recent `get` OR *commit-all* `post`
//                     (commit-all = no put/delete between that post's
//                      pd boundary and itself).
//
//  Repro A (pd floor): `put fileA; get ?branch; post`.  The intervening
//  `get` resets the pd boundary, so the stale pre-checkout `put fileA`
//  must NOT be in scope for the next POST.  Before the fix
//  SNIFFAtLastPostTs() matched only `post`, leaving the floor below the
//  `get` and leaking `put fileA`.
//
//  Repro B (patch boundary): `patch ?X; put f; post (selective); post`.
//  The first post is selective (a put lies between its pd boundary and
//  itself), so it is NOT commit-all and must not reset the patch
//  boundary.  The second POST must therefore still see `patch ?X`'s
//  provenance.  Before the fix any `post` reset the patch boundary,
//  dropping `?X`.
ok64 SNIFFAtBoundaries() {
    sane(1);
    call(FILEInit);
    call(make_tmpdir);

    a_cstr(root, g_tmpdir);
    home h = {};
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);
    call(SNIFFOpen, YES);

    ron60 vg = SNIFFAtVerbGet();
    ron60 vp = SNIFFAtVerbPost();
    ron60 vx = SNIFFAtVerbPatch();
    ron60 vu = SNIFFAtVerbPut();

    ron60 t0 = 0;
    {
        ulogrec r0 = {};
        call(ULOGRow, SNIFF.log_data, SNIFF.log_idx, 0, &r0);
        t0 = r0.ts;
    }
    ron60 base = t0 + 1000;

    // --- Repro A: pd floor matches the most recent get OR post ---
    //  Timeline:
    //    +0   get   ?heads/main#aaaa…   (initial checkout)
    //    +100 put   src/old.c           (stale, pre-checkout stage)
    //    +200 get   ?heads/feat#bbbb…   (checkout resets pd boundary)
    //  The pd floor must now be the +200 get, so a put/delete scan from
    //  the floor sees NOTHING (no put/delete after +200).
    call(at_append_uri, base + 0,   vg, "?heads/main#aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    call(at_append_uri, base + 100, vu, "src/old.c");
    call(at_append_uri, base + 200, vg, "?heads/feat#bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");

    //  Floor sits at the +200 get, NOT at 0.
    want(SNIFFAtLastPostTs() == base + 200);
    {
        pd_capture cap = {};
        call(SNIFFAtScanPutDelete, SNIFFAtLastPostTs(), pd_collect, &cap);
        want(cap.n == 0);   // stale `put src/old.c` is below the floor
    }

    // --- Repro B: patch boundary resets only at get or commit-all post ---
    //  Continue the timeline (DIS-030 row format — `?<sha>!` = WHOLE):
    //    +300 patch ?cccc…!  (whole-branch scope — carries provenance)
    //    +400 put   src/f.c  (makes the next post selective)
    //    +500 post  ?heads/feat#dddd…  (SELECTIVE: put in its pd scope)
    //  The selective +500 post is NOT commit-all, so it must not reset
    //  the patch boundary.  A patch-entries read after it must still
    //  surface the +300 patch row's provenance.
    call(at_append_uri, base + 300, vx, "?cccccccccccccccccccccccccccccccccccccccc!");
    call(at_append_uri, base + 400, vu, "src/f.c");
    call(at_append_uri, base + 500, vp, "?heads/feat#dddddddddddddddddddddddddddddddddddddddd");

    {
        sniff_pe pent[16] = {};
        u32 n_pent = 0;
        call(SNIFFAtPatchEntries, pent, 16, &n_pent);
        //  The +300 whole-branch row's provenance must survive the
        //  selective post (DIS-030: branch-sourced → not named).
        want(n_pent == 1);
        want(pent[0].shape == PATCH_SCOPE_WHOLE);
        want(!pent[0].named);
    }

    call(SNIFFClose);
    KEEPClose();
    HOMEClose();
    rm_tmpdir();
    done;
}

// --- Test: checkout + commit round-trip ---

ok64 SNIFFCheckoutCommit() {
    sane(1);
    call(FILEInit);
    call(make_tmpdir);

    //  Born-sharded (DIS-024): pre-create the project shard so
    //  HOMEOpenAt's single-shard scan resolves the project up front.
    //  This keeps the manual KEEPOpen (below) and the later SNIFFOpen
    //  bootstrap on the SAME shard — otherwise KEEPOpen would write the
    //  objects flat and the SNIFFOpen mint would read them sharded.
    {
        char sp[300];
        snprintf(sp, sizeof sp, "%s/" DOG_BE_NAME, g_tmpdir);
        mkdir(sp, 0755);
        snprintf(sp, sizeof sp, "%s/" DOG_BE_NAME "/proj", g_tmpdir);
        want(mkdir(sp, 0755) == 0);
    }

    a_cstr(root, g_tmpdir);
    home h = {};
    call(HOMEOpenAt, root, YES);

    // Open keeper, create a blob + tree + commit manually

    call(KEEPOpen, YES);

    keep_pack p = {};
    call(KEEPPackOpen, &p);
    //  Hand-rolled objects for test — fed in non-canonical order
    //  (blob before tree).  Sniff's real POST path repacks canonically.
    p.strict_order = NO;

    // Blob: "hello\n"
    a_cstr(blob_data, "hello\n");
    sha1 blob_sha = {};
    call(KEEPPackFeed, &p, DOG_OBJ_BLOB, blob_data, 0, &blob_sha);

    // Tree: one entry "100644 test.txt\0<sha>"
    a_pad(u8, tree_buf, 256);
    a_cstr(tm, "100644 test.txt");
    u8bFeed(tree_buf, tm);
    u8bFeed1(tree_buf, 0);
    a_rawc(sha_s, blob_sha);
    u8bFeed(tree_buf, sha_s);
    a_dup(u8c, tree_content, u8bData(tree_buf));

    sha1 tree_sha = {};
    call(KEEPPackFeed, &p, DOG_OBJ_TREE, tree_content, 0, &tree_sha);

    // Commit
    a_pad(u8, cbuf, 512);
    a_cstr(c1, "tree ");
    u8bFeed(cbuf, c1);
    a_pad(u8, thex, 40);
    a_rawc(ts, tree_sha);
    HEXu8sFeedSome(thex_idle, ts);
    u8bFeed(cbuf, u8bDataC(thex));
    a_cstr(c2, "\nauthor Test <t@t> 1700000000 +0000\n"
               "committer Test <t@t> 1700000000 +0000\n\ninitial\n");
    u8bFeed(cbuf, c2);
    a_dup(u8c, commit_content, u8bData(cbuf));

    sha1 commit_sha = {};
    call(KEEPPackFeed, &p, DOG_OBJ_COMMIT, commit_content, 0, &commit_sha);
    call(KEEPPackClose, &p);

    // Hex of commit SHA for CLI
    a_pad(u8, commit_hex, 40);
    a_rawc(csha_s, commit_sha);
    HEXu8sFeedSome(commit_hex_idle, csha_s);

    // Now checkout via sniff
    sniff s = {};
    call(SNIFFOpen, YES);
    u8cs hex = {u8bDataHead(commit_hex), u8bIdleHead(commit_hex)};
    u8cs no_src_ = {}; call(GETCheckout, root, hex, no_src_);

    // Verify file exists
    a_path(fp, root);
    a_cstr(fn, "/test.txt");
    call(u8bFeed, fp, fn);
    call(PATHu8bTerm, fp);
    filestat fs = {};
    want(FILEStat(&fs, $path(fp)) == OK);

    //  New-model sniff doesn't expose a per-path hashlet cache across
    //  processes — GETCheckout just stamps the files and appends a
    //  `get` ULOG row.  Step 6 will add a full workflow-driving test;
    //  for now this scenario only verifies that:
    //    * the file materialised on disk (done above), and
    //    * a subsequent POSTCommit can still chain a commit behind the
    //      baseline captured in the ULOG's `get` row.

    // Modify file — no SNIFFRecord call anymore; POST in Step 4 will
    // detect change via mtime ∉ stamp-set.
    {
        int fd = -1;
        call(FILECreate, &fd, $path(fp));
        a_cstr(newdata, "modified\n");
        FILEFeedAll(fd, newdata);
        FILEClose(&fd);
    }
    bump_mtime((char *)u8bDataHead(fp), 2);

    // Commit (HEAD is already set to the initial commit from GETCheckout).
    //  POSTCommit's ff check uses GRAFLca, which needs GRAF open.
    call(GRAFOpen, YES);
    a_cstr(msg, "second commit");
    a_cstr(author, "Test <t@t>");
    sha1 new_sha = {};
    u8cs no_target = {};
    (void)root;
    call(POSTCommit, no_target, msg, author, NULL, &new_sha);

    // Verify new commit exists
    u64 new_hashlet = WHIFFHashlet60(&new_sha);
    want(KEEPHas(new_hashlet, 15) == OK);

    // Verify via KEEPGet
    Bu8 out = {};
    call(u8bAllocate, out, 1UL << 20);
    u8 otype = 0;
    call(KEEPGet, new_hashlet, 10, out, &otype);
    want(otype == DOG_OBJ_COMMIT);

    // Verify commit content mentions parent
    u8cs body = {u8bDataHead(out), u8bIdleHead(out)};
    u8cs scan = {body[0], body[1]};
    want(u8csFind(scan, 'p') == OK);  // "parent ..."

    u8bFree(out);
    call(SNIFFClose);
    GRAFClose();
    call(KEEPClose);
    HOMEClose();
    rm_tmpdir();
    done;
}

// --- Helper: sha1 to hex string ---

static void sha2hex(u8bp buf, sha1cp sha) {
    u8bReset(buf);
    a_rawc(s, *sha);
    u8s idle = {buf[2], buf[3]};
    HEXu8sFeedSome(idle, s);
    ((u8p *)buf)[2] = idle[0];  // advance DATA end
}

// --- Helper: write a file under root ---

static ok64 write_file(u8cs root, char const *rel, char const *content) {
    sane($ok(root));
    a_path(fp, root);
    a_cstr(sep, "/");
    call(u8bFeed, fp, sep);
    a_cstr(r, rel);
    call(u8bFeed, fp, r);
    call(PATHu8bTerm, fp);

    // Ensure parent dir exists
    a_path(dp, root);
    u8bFeed(dp, sep);
    u8bFeed(dp, r);
    PATHu8bTerm(dp);
    // Walk back to last /
    u8cs dpath = {u8bDataHead(dp), u8bIdleHead(dp)};
    u8cs dscan = {dpath[0], dpath[1]};
    u8cp last_slash = NULL;
    a_dup(u8c, dfind, dscan);
    while (u8csFind(dfind, '/') == OK) {
        last_slash = dfind[0];
        ++dfind[0];
    }
    if (last_slash && last_slash > dpath[0]) {
        u8 save = *last_slash;
        *(u8p)last_slash = 0;
        a_cstr(dp2, (char *)dpath[0]);
        a_path(dirp, dp2);
        FILEMakeDirP($path(dirp));
        *(u8p)last_slash = save;
    }

    int fd = -1;
    call(FILECreate, &fd, $path(fp));
    a_cstr(data, content);
    call(FILEFeedAll, fd, data);
    close(fd);
    done;
}

// --- Helper: check file exists and has expected content ---

static ok64 check_file(u8cs root, char const *rel, char const *expected) {
    sane($ok(root));
    a_path(fp, root);
    a_cstr(sep, "/");
    call(u8bFeed, fp, sep);
    a_cstr(r, rel);
    call(u8bFeed, fp, r);
    call(PATHu8bTerm, fp);

    filestat fs = {};
    want(FILEStat(&fs, $path(fp)) == OK);

    Bu8 content = {};
    call(u8bAllocate, content, 1UL << 20);
    int fd = -1;
    call(FILEOpen, &fd, $path(fp), O_RDONLY);
    FILEdrainall(u8bIdle(content), fd);
    close(fd);

    size_t elen = strlen(expected);
    want(u8bDataLen(content) == elen);
    want(memcmp(u8bDataHead(content), expected, elen) == 0);
    u8bFree(content);
    done;
}

// --- Helper: check file does NOT exist ---

static b8 file_gone(u8cs root, char const *rel) {
    a_path(fp, root);
    a_cstr(sep, "/");
    u8bFeed(fp, sep);
    a_cstr(r, rel);
    u8bFeed(fp, r);
    PATHu8bTerm(fp);
    filestat fs = {};
    return (FILEStat(&fs, $path(fp)) != OK);
}

// --- Helper: create initial commit with N files in keeper ---

typedef struct { char const *name; char const *data; } testfile;

static ok64 make_commit(sha1 *commit_out, keeper *k,
                        testfile const *files, u32 nfiles,
                        sha1cp parent) {
    sane(k && commit_out);
    keep_pack p = {};
    call(KEEPPackOpen, &p);
    //  Hand-rolled blob→tree→commit sequence for tests — not canonical.
    p.strict_order = NO;

    // Create blobs + tree entries
    a_pad(u8, tree_buf, 4096);
    for (u32 i = 0; i < nfiles; i++) {
        a_cstr(blob, files[i].data);
        sha1 bsha = {};
        call(KEEPPackFeed, &p, DOG_OBJ_BLOB, blob, 0, &bsha);
        a_cstr(mode, "100644 ");
        u8bFeed(tree_buf, mode);
        a_cstr(name, files[i].name);
        u8bFeed(tree_buf, name);
        u8bFeed1(tree_buf, 0);
        a_rawc(sraw, bsha);
        u8bFeed(tree_buf, sraw);
    }

    sha1 tree_sha = {};
    a_dup(u8c, tc, u8bData(tree_buf));
    call(KEEPPackFeed, &p, DOG_OBJ_TREE, tc, 0, &tree_sha);

    // Commit object
    a_pad(u8, cbuf, 1024);
    a_cstr(cl1, "tree ");
    u8bFeed(cbuf, cl1);
    a_pad(u8, thex, 40);
    a_rawc(ts, tree_sha);
    HEXu8sFeedSome(thex_idle, ts);
    u8bFeed(cbuf, u8bDataC(thex));
    u8bFeed1(cbuf, '\n');

    if (parent) {
        a_cstr(pl, "parent ");
        u8bFeed(cbuf, pl);
        a_pad(u8, phex, 40);
        a_rawc(ps, *parent);
        HEXu8sFeedSome(phex_idle, ps);
        u8bFeed(cbuf, u8bDataC(phex));
        u8bFeed1(cbuf, '\n');
    }

    a_cstr(hdr, "author Test <t@t> 1700000000 +0000\n"
                "committer Test <t@t> 1700000000 +0000\n\ninitial\n");
    u8bFeed(cbuf, hdr);

    a_dup(u8c, cc, u8bData(cbuf));
    call(KEEPPackFeed, &p, DOG_OBJ_COMMIT, cc, 0, commit_out);
    call(KEEPPackClose, &p);
    done;
}

// --- Main ---

// --- SNIFFMergeWalk: 3-way grouping by path-key ---------------------

typedef struct {
    ron60 v_base, v_ours, v_theirs;
    u32   n;                  // step count
    u32   sizes[16];
    char  paths[16][64];
    u32   base[16], ours[16], theirs[16];
} merge_ctx;

static ok64 merge_step(ulogreccs recs, void *vctx) {
    merge_ctx *c = (merge_ctx *)vctx;
    if (c->n >= 16) fail(FAIL);
    ulogreccp head = ulogreccsHead(recs);
    size_t L = u8csLen(head->uri.path);
    if (L >= sizeof(c->paths[0])) L = sizeof(c->paths[0]) - 1;
    memcpy(c->paths[c->n], head->uri.path[0], L);
    c->paths[c->n][L] = 0;
    c->sizes[c->n] = (u32)$len(recs);
    u32 nb = 0, no = 0, nt = 0;
    $for(ulogrec const, rec, recs) {
        ron60 v = rec->verb;
        if      (v == c->v_base)   nb++;
        else if (v == c->v_ours)   no++;
        else if (v == c->v_theirs) nt++;
    }
    c->base[c->n]   = nb;
    c->ours[c->n]   = no;
    c->theirs[c->n] = nt;
    c->n++;
    return OK;
}

static ok64 SNIFFMergeWalkTest(void) {
    sane(1);

    //  Build three sorted ULOG buffers, one per "side":
    //    base : a, b, c
    //    ours : a, b
    //    theirs : b, c
    //  Expected steps: a (base, ours), b (base, ours, theirs), c (base, theirs).
    Bu8 arena = {};
    call(u8bAllocate, arena, 4096);

    a_cstr(s_base,   "base");   a_dup(u8c, dup_b, s_base);
    a_cstr(s_ours,   "ours");   a_dup(u8c, dup_o, s_ours);
    a_cstr(s_theirs, "theirs"); a_dup(u8c, dup_t, s_theirs);
    ron60 v_base = 0, v_ours = 0, v_theirs = 0;
    call(RONutf8sDrain, &v_base,   dup_b);
    call(RONutf8sDrain, &v_ours,   dup_o);
    call(RONutf8sDrain, &v_theirs, dup_t);

    //  Helper: feed one row with given (verb, path bytes) — empty mode/sha.
    #define EMIT(g, vv, txt)                                            \
        do {                                                            \
            a_cstr(_p, txt);                                             \
            uri _u = {};                                                 \
            _u.path[0] = _p[0]; _u.path[1] = _p[1];                      \
            ulogrec _r = {.ts = 0, .verb = (vv), .uri = _u};             \
            call(ULOGu8sFeed, u8gRest(g), &_r);                          \
        } while (0)

    b_lign(u8, g_base, arena);
    EMIT(g_base,   v_base,   "a.txt");
    EMIT(g_base,   v_base,   "b.txt");
    EMIT(g_base,   v_base,   "c.txt");
    b_cq(u8, view_b, arena);

    b_lign(u8, g_ours, arena);
    EMIT(g_ours,   v_ours,   "a.txt");
    EMIT(g_ours,   v_ours,   "b.txt");
    b_cq(u8, view_o, arena);

    b_lign(u8, g_theirs, arena);
    EMIT(g_theirs, v_theirs, "b.txt");
    EMIT(g_theirs, v_theirs, "c.txt");
    b_cq(u8, view_t, arena);
    #undef EMIT

    a_pad(u8cs, ins, 3);
    u8cssFeed1(ins_idle, view_b);
    u8cssFeed1(ins_idle, view_o);
    u8cssFeed1(ins_idle, view_t);
    a_dup(u8cs, cursors, u8csbData(ins));

    merge_ctx mctx = {.v_base = v_base, .v_ours = v_ours, .v_theirs = v_theirs};
    call(SNIFFMergeWalk, cursors, merge_step, &mctx);

    want(mctx.n == 3);
    want(strcmp(mctx.paths[0], "a.txt") == 0);
    want(mctx.sizes[0] == 2);
    want(mctx.base[0] == 1 && mctx.ours[0] == 1 && mctx.theirs[0] == 0);

    want(strcmp(mctx.paths[1], "b.txt") == 0);
    want(mctx.sizes[1] == 3);
    want(mctx.base[1] == 1 && mctx.ours[1] == 1 && mctx.theirs[1] == 1);

    want(strcmp(mctx.paths[2], "c.txt") == 0);
    want(mctx.sizes[2] == 2);
    want(mctx.base[2] == 1 && mctx.ours[2] == 0 && mctx.theirs[2] == 1);

    u8bFree(arena);
    done;
}

// --- MEM-028: cascade skip-without-append must not read recs[-1] --------

//  Test seam exported from sniff/POST.c.  Drives the walker's per-child
//  step against a branch with NO REFS tip, so post_cascade_one takes
//  its REFSNONE skip and leaves n == 0.  Before the fix the walker read
//  recs[cc->n - 1] unconditionally → OOB read of recs[-1] (caught by
//  ASan); after the fix the read is guarded by an `appended` signal.
extern ok64 POSTCascadeOneReproForTest(u8cs branch, u32 *n_after,
                                       b8 *appended_out, sha1 *read_back);

static ok64 SNIFFCascadeSkipNoAppend(void) {
    sane(1);
    call(FILEInit);
    call(make_tmpdir);

    //  Born-sharded scratch store (mirrors SNIFFCheckoutCommit).
    {
        char sp[300];
        snprintf(sp, sizeof sp, "%s/" DOG_BE_NAME, g_tmpdir);
        mkdir(sp, 0755);
        snprintf(sp, sizeof sp, "%s/" DOG_BE_NAME "/proj", g_tmpdir);
        want(mkdir(sp, 0755) == 0);
    }
    a_cstr(root, g_tmpdir);
    home h = {};
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);
    call(SNIFFOpen, YES);

    //  A branch with no REFS row at all resolves REFSNONE, forcing the
    //  skip-without-append path on the very first (n == 0) child.
    a_cstr(noexist, "no/such/branch");
    a_dup(u8c, branch, noexist);

    u32 n_after = 99;
    b8  appended = YES;
    sha1 read_back = {};
    ok64 ro = POSTCascadeOneReproForTest(branch, &n_after, &appended,
                                         &read_back);

    //  The skip path returns OK, appends nothing, and (post-fix) never
    //  indexes recs.  Pre-fix: ASan aborts at the recs[-1] read above.
    want(ro == OK);
    want(n_after == 0);
    want(appended == NO);

    call(SNIFFClose);
    call(KEEPClose);
    HOMEClose();
    rm_tmpdir();
    done;
}

// --- SUBS: URL basename + .gitmodules parse/synth -----------------------

static b8 slice_eq_cstr(u8cs s, char const *c) {
    size_t n = strlen(c);
    if (u8csLen(s) != n) return NO;
    if (n == 0) return YES;
    return memcmp(s[0], (u8 const *)c, n) == 0;
}

static ok64 SUBSBasenameTest(void) {
    sane(1);

    struct {
        char const *url;
        char const *want;        // NULL → expect SUBSPARSE
    } cases[] = {
        {"https://github.com/gritzko/libabc.git",    "libabc"},
        {"git@github.com:foo/proj.git",              "proj"},
        {"ssh://localhost/srv/repos/widgets/",       "widgets"},
        {"ssh://localhost/srv/repos/widgets",        "widgets"},
        {"file:///var/repos/proj.git",               "proj"},
        {"http://host/x/y/z/",                       "z"},
        {"single",                                   "single"},
        {"",                                         NULL},
        {"http://host/.git",                         NULL}, // empty basename
        //  GET-004: a `.be` basename would compose `<store>/.be/.be`
        //  (the store dir leaking in as a sub name) — refuse it.  The
        //  `.git` suffix strip can also distill `.be.git` down to `.be`,
        //  which must likewise be rejected.
        {"ssh://localhost/srv/repos/.be",            NULL},
        {"git@github.com:foo/.be.git",               NULL},
        {".be",                                      NULL},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        a_cstr(src, cases[i].url);
        a_dup(u8c, in, src);
        u8cs got = {};
        ok64 o = SNIFFSubBasename(in, got);
        if (cases[i].want == NULL) {
            want(o != OK);
        } else {
            want(o == OK);
            want(slice_eq_cstr(got, cases[i].want));
        }
    }
    done;
}

typedef struct {
    u32         n;
    char        path[8][64];
    char        url [8][128];
} subs_collect;

static ok64 subs_collect_cb(u8cs path, u8cs url, void *vctx) {
    subs_collect *c = (subs_collect *)vctx;
    if (c->n >= 8) return OK;
    size_t pl = u8csLen(path); if (pl > 63) pl = 63;
    size_t ul = u8csLen(url);  if (ul > 127) ul = 127;
    memcpy(c->path[c->n], path[0], pl); c->path[c->n][pl] = 0;
    memcpy(c->url [c->n], url[0],  ul); c->url [c->n][ul] = 0;
    c->n++;
    return OK;
}

static ok64 SUBSParseTest(void) {
    sane(1);

    char const *blob_str =
        "; sample .gitmodules\n"
        "[submodule \"vendor/sub\"]\n"
        "\tpath = vendor/sub\n"
        "\turl = ssh://localhost/srv/sub.git\n"
        "\n"
        "[submodule \"thirdparty/x\"]\n"
        "  path  =  thirdparty/x  \n"
        "  url  =  https://host/x.git\n"
        "[other]\n"
        "ignored = yes\n"
        "[submodule \"no-url\"]\n"
        "  path = oops\n"
        ;
    a_cstr(src, blob_str);
    a_dup(u8c, blob, src);
    subs_collect c = {};
    call(SNIFFSubsParse, blob, subs_collect_cb, &c);
    want(c.n == 2);
    want(strcmp(c.path[0], "vendor/sub") == 0);
    want(strcmp(c.url [0], "ssh://localhost/srv/sub.git") == 0);
    want(strcmp(c.path[1], "thirdparty/x") == 0);
    want(strcmp(c.url [1], "https://host/x.git") == 0);

    //  ParseFind: hit + miss.  url_buf lives in the test's own frame
    //  so the returned slice outlives the parser's call.
    a_pad(u8, url_buf, 512);
    a_dup(u8c, blob2, src);
    a_cstr(want_path, "thirdparty/x");
    u8cs url = {};
    call(SNIFFSubsParseFind, blob2, want_path, url_buf, url);
    want(slice_eq_cstr(url, "https://host/x.git"));

    a_pad(u8, url_buf2, 512);
    a_dup(u8c, blob3, src);
    a_cstr(miss_path, "nope");
    u8cs url2 = {};
    ok64 m = SNIFFSubsParseFind(blob3, miss_path, url_buf2, url2);
    want(m == SUBSNOSEC);

    //  Malformed: missing closing ']'.
    a_cstr(bad_str, "[submodule \"x\"\nurl = y\n");
    a_dup(u8c, bad, bad_str);
    ok64 b = SNIFFSubsParse(bad, subs_collect_cb, &c);
    want(b != OK);
    done;
}

static ok64 SUBSSynthTest(void) {
    sane(1);

    Bu8 arena = {}, out = {};
    call(u8bAllocate, arena, 1024);
    call(u8bAllocate, out,   1024);

    b_lign(u8, g_paths, arena);
    a_cstr(p1, "vendor/sub");   call(u8gFeed, g_paths, p1); call(u8gFeed1, g_paths, '\n');
    a_cstr(p2, "thirdparty/x"); call(u8gFeed, g_paths, p2); call(u8gFeed1, g_paths, '\n');
    b_cq(u8, paths, arena);

    b_lign(u8, g_urls, arena);
    a_cstr(u1, "ssh://localhost/srv/sub.git");
    call(u8gFeed, g_urls, u1); call(u8gFeed1, g_urls, '\n');
    a_cstr(u2, "https://host/x.git");
    call(u8gFeed, g_urls, u2); call(u8gFeed1, g_urls, '\n');
    b_cq(u8, urls, arena);

    call(SNIFFSubsSynth, out, paths, urls);

    char const *expect =
        "[submodule \"vendor/sub\"]\n"
        "\tpath = vendor/sub\n"
        "\turl = ssh://localhost/srv/sub.git\n"
        "[submodule \"thirdparty/x\"]\n"
        "\tpath = thirdparty/x\n"
        "\turl = https://host/x.git\n";
    size_t exp_len = strlen(expect);
    want(u8bDataLen(out) == exp_len);
    want(memcmp(u8bDataHead(out), expect, exp_len) == 0);

    //  Synth-then-parse round-trip.
    subs_collect c = {};
    a_dup(u8c, view, u8bData(out));
    call(SNIFFSubsParse, view, subs_collect_cb, &c);
    want(c.n == 2);
    want(strcmp(c.path[0], "vendor/sub") == 0);
    want(strcmp(c.url [0], "ssh://localhost/srv/sub.git") == 0);
    want(strcmp(c.path[1], "thirdparty/x") == 0);
    want(strcmp(c.url [1], "https://host/x.git") == 0);

    u8bFree(arena);
    u8bFree(out);
    done;
}

// --- SUBS-016: sub-store log seed must never truncate a live log -------

//  Test seam exported from sniff/SUBS.c.  Drives the refs/wtlog seed
//  path (subs_seed_logs) directly against a store_dir that already
//  holds a NON-EMPTY refs + wtlog — the false-negative-mount scenario
//  the production `!already_mounted` guard normally prevents.  Pre-fix
//  the seed used FILECreate (O_TRUNC) and would zero both logs; post-fix
//  it is create-if-missing and preserves them.
extern ok64 SUBSSeedLogsReproForTest(u8cs store_dir);

static ok64 SUBSSeedPreservesLog(void) {
    sane(1);
    call(FILEInit);
    call(make_tmpdir);

    //  Hermetic scratch sub-store dir holding a LIVE (non-empty) refs +
    //  wtlog, simulating an already-mounted sub whose mount check came
    //  back a false-negative — so the seed runs over real data.
    char cmd[1024];
    snprintf(cmd, sizeof cmd,
             "rm -rf %s/substore && mkdir -p %s/substore && "
             "printf 'REFSDATA-not-empty\\n' > %s/substore/refs && "
             "printf 'WTLOGDATA-not-empty\\n' > %s/substore/wtlog",
             g_tmpdir, g_tmpdir, g_tmpdir, g_tmpdir);
    want(system(cmd) == 0);

    a_pad(u8, sd, 512);
    a_cstr(tmp_lit, g_tmpdir);
    a_cstr(sub_lit, "/substore");
    u8bFeed(sd, tmp_lit);
    u8bFeed(sd, sub_lit);
    a_dup(u8c, store_dir, u8bData(sd));

    //  Drive the seed directly — the production `!already_mounted` gate
    //  is bypassed exactly as a false-negative mount check would do.
    call(SUBSSeedLogsReproForTest, store_dir);

    //  Both logs must survive byte-for-byte (pre-fix: zeroed by O_TRUNC).
    {
        a_pad(u8, root_b, 512);
        u8bFeed(root_b, tmp_lit);
        u8bFeed(root_b, sub_lit);
        a_dup(u8c, root_s, u8bData(root_b));
        call(check_file, root_s, "refs",  "REFSDATA-not-empty\n");
        call(check_file, root_s, "wtlog", "WTLOGDATA-not-empty\n");
    }

    //  Fresh-seed still works: an empty store_dir gets both logs created.
    snprintf(cmd, sizeof cmd,
             "rm -rf %s/fresh && mkdir -p %s/fresh", g_tmpdir, g_tmpdir);
    want(system(cmd) == 0);
    a_pad(u8, fd2, 512);
    a_cstr(fresh_lit, "/fresh");
    u8bFeed(fd2, tmp_lit);
    u8bFeed(fd2, fresh_lit);
    a_dup(u8c, fresh_dir, u8bData(fd2));
    call(SUBSSeedLogsReproForTest, fresh_dir);
    {
        a_dup(u8c, fr_s, u8bData(fd2));
        want(!file_gone(fr_s, "refs"));
        want(!file_gone(fr_s, "wtlog"));
    }

    rm_tmpdir();
    done;
}

static ok64 SUBSIsMountTest(void) {
    sane(1);
    call(FILEInit);
    call(make_tmpdir);

    //  Layout under $g_tmpdir:
    //      <tmp>/wt/                       (parent wt root)
    //      <tmp>/wt/.be/                   (parent keeper dir)
    //      <tmp>/wt/.be/wtlog              (parent wtlog)
    //      <tmp>/wt/vendor/sub/.be         (sub-mount anchor — REG file)
    //      <tmp>/wt/vendor/other/          (regular dir, no .be inside)
    //      <tmp>/wt/dir/.be/               (DIR — not a sub-mount)
    //
    //  IsMount must return YES only for vendor/sub.

    a_pad(u8, wt_path, 512);
    a_cstr(wt_lit, "/wt");
    a_cstr(tmp_lit, g_tmpdir);
    u8bFeed(wt_path, tmp_lit);
    u8bFeed(wt_path, wt_lit);
    a_dup(u8c, wt_root, u8bData(wt_path));

    char cmd[1024];
    snprintf(cmd, sizeof cmd,
             "rm -rf %s/wt && mkdir -p %s/wt/.be %s/wt/vendor/sub "
             "%s/wt/vendor/other %s/wt/dir/.be && "
             "echo X > %s/wt/.be/wtlog && "
             "echo Y > %s/wt/vendor/sub/.be",
             g_tmpdir, g_tmpdir, g_tmpdir, g_tmpdir, g_tmpdir,
             g_tmpdir, g_tmpdir);
    system(cmd);

    a_cstr(p_sub,    "vendor/sub");
    a_cstr(p_other,  "vendor/other");
    a_cstr(p_dir,    "dir");
    a_cstr(p_missing,"nope");
    want( SNIFFSubIsMount(wt_root, p_sub));
    want(!SNIFFSubIsMount(wt_root, p_other));
    want(!SNIFFSubIsMount(wt_root, p_dir));
    want(!SNIFFSubIsMount(wt_root, p_missing));

    rm_tmpdir();
    done;
}

// --- Test: absolute ?/project query strips to trunk/branch ---
//  Repro for the ref-adoption bug: a local `get` row whose query is
//  the absolute `?/<project>[/<branch>]` form must resolve cur to the
//  branch WITHIN the project (trunk = empty), never to a branch named
//  literally `/<project>`.  See wiki/URI.mkd §"Ref shapes".
ok64 SNIFFAtProjectStrip() {
    sane(1);
    call(FILEInit);
    call(make_tmpdir);

    a_cstr(root, g_tmpdir);
    home h = {};
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);
    call(SNIFFOpen, YES);

    ron60 vg = SNIFFAtVerbGet();
    ron60 t0 = 0;
    {
        ulogrec r0 = {};
        call(ULOGRow, SNIFF.log_data, SNIFF.log_idx, 0, &r0);
        t0 = r0.ts;
    }
    ron60 base = t0 + 1000;

    //  The composed --at is absolute `?/<project>/<branch>` (https://replicated.wiki/html/wiki/Verbs.html
    //  "Ref resolution"); the project comes from the colocated anchor
    //  (= wt basename), and a row's own leading `/<proj>` segment is
    //  stripped so it is never read as a branch.
    char const *bn = strrchr(g_tmpdir, '/');
    bn = bn ? bn + 1 : g_tmpdir;

    //  Scenario 1: `?/beagle#<sha>` ⇒ project trunk `?/<project>`, with
    //  the row's `/beagle` stripped — NOT a branch named `/beagle`.
    call(at_append_uri, base + 0, vg,
         "?/beagle#aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    {
        a_pad(u8, tail, MAX_URI_LEN);
        a_dup(u8c, wt_s, root);
        call(SNIFFAtTailOf, wt_s, tail);
        a_dup(u8c, td, u8bData(tail));
        uri tu = {};
        call(URIutf8Drain, td, &tu);
        a_dup(u8c, q, tu.query);
        char wq1[300];
        snprintf(wq1, sizeof wq1, "/%s", bn);
        a_cstr(want1, wq1);
        want(u8csEq(q, want1));        // `/<project>` trunk
        a_dup(u8c, frag, tu.fragment);
        want($len(frag) == 40);
    }

    //  Scenario 2: `?/beagle/feat#<sha>` ⇒ `?/<project>/feat` (row's
    //  `/beagle` stripped, branch tail `feat` kept under the anchor's
    //  project).
    call(at_append_uri, base + 100, vg,
         "?/beagle/feat#bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    {
        a_pad(u8, tail, MAX_URI_LEN);
        a_dup(u8c, wt_s, root);
        call(SNIFFAtTailOf, wt_s, tail);
        a_dup(u8c, td, u8bData(tail));
        uri tu = {};
        call(URIutf8Drain, td, &tu);
        a_dup(u8c, q, tu.query);
        char wq2[300];
        snprintf(wq2, sizeof wq2, "/%s/feat", bn);
        a_cstr(want2, wq2);
        want(u8csEq(q, want2));
    }

    call(SNIFFClose);
    KEEPClose();
    HOMEClose();
    rm_tmpdir();
    done;
}

// --- Test: DOGTitleFromUri 3-step precedence ---
//  Canonical title derivation (wiki/Title.mkd).  The two qlog/CLI URI
//  shapes both resolve here: official `…/title.git` (basename) and
//  beagle `?/title` (query override).  `/.be/<seg>/` is the local-
//  shard anchor case.
ok64 DOGTitleTest() {
    sane(1);
    struct { char const *uri; char const *want; } cases[] = {
        { "ssh://host/path/dogs.git?/beagle",      "beagle" }, // 1 query override
        { "ssh://host/path/dogs.git?/beagle/feat", "beagle" }, // 1 proj before branch
        { "file:/home/u/.be/myproj/",              "myproj" }, // 2 /.be/ anchor
        { "ssh://host/gritzko/libabc.git",         "libabc" }, // 3 basename + .git
        { "https://host/beagle.git/",              "beagle" }, // 3 trailing slash
    };
    for (u32 i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        a_pad(u8, urib, MAX_URI_LEN);
        a_cstr(src, cases[i].uri);
        u8bFeed(urib, src);
        uri u = {};
        a_dup(u8c, ud, u8bData(urib));
        call(URIutf8Drain, ud, &u);

        a_pad(u8, out, 256);
        DOGTitleFromUri(&u, out);
        a_dup(u8c, got, u8bData(out));
        u32 wl = (u32)strlen(cases[i].want);
        want((u32)$len(got) == wl);
        want(memcmp(got[0], cases[i].want, wl) == 0);
    }
    done;
}

// --- Test: HUNK table / LS scratch-buffer acquisition + unwind ------
//  BRO-002 moved the row table's text/toks into the HUNK table
//  (HUNKTableOpen maps the 4 MiB text then heap-allocs toks; on a toks
//  failure it unmaps text — the MEM-026 unwind now lives there, BE-007).
//  `ls:`'s only local scratch is the one-level dedup buffer `dir_seen`.
ok64 SNIFFLsBufsLeak() {
    sane(1);

    //  Case 1 (HUNKTableOpen happy path + Close releases text/toks).  An
    //  empty-uri batch accumulator with no rows fed: Close is a clean
    //  flush (nothing to emit) and frees both buffers.  The accessors
    //  expose the live buffers (non-NULL open) / return NULL once closed.
    {
        u8cs empty = {};
        call(HUNKTableOpen, empty, 0, 0, YES);
        want(HUNKTableText() != NULL && HUNKTableToks() != NULL);
        call(HUNKTableClose);
        want(HUNKTableText() == NULL && HUNKTableToks() == NULL);
    }

    //  Case 2 (`ls:` dir_seen acquire + release).
    {
        Bu8 dir_seen = {};
        call(SNIFFLsBufsAcquire, dir_seen, NO /*ls:*/);
        want(dir_seen[0] != NULL);
        u8bFree(dir_seen);
    }

    //  Case 3 (`lsr:` needs no dir_seen — acquire is a no-op).
    {
        Bu8 dir_seen = {};
        call(SNIFFLsBufsAcquire, dir_seen, YES /*lsr:*/);
        want(dir_seen[0] == NULL);
    }
    done;
}

ok64 maintest() {
    sane(1);
    fprintf(stderr, "SNIFFLsBufsLeak...\n");
    call(SNIFFLsBufsLeak);
    fprintf(stderr, "DOGTitle...\n");
    call(DOGTitleTest);
    fprintf(stderr, "SNIFFAtHelpers...\n");
    call(SNIFFAtHelpers);
    fprintf(stderr, "SNIFFAtBoundaries...\n");
    call(SNIFFAtBoundaries);
    fprintf(stderr, "SNIFFAtProjectStrip...\n");
    call(SNIFFAtProjectStrip);
    fprintf(stderr, "SNIFFCheckoutCommit...\n");
    call(SNIFFCheckoutCommit);
    fprintf(stderr, "SNIFFMergeWalk...\n");
    call(SNIFFMergeWalkTest);
    fprintf(stderr, "SNIFFCascadeSkipNoAppend...\n");
    call(SNIFFCascadeSkipNoAppend);
    fprintf(stderr, "SUBSBasename...\n");
    call(SUBSBasenameTest);
    fprintf(stderr, "SUBSParse...\n");
    call(SUBSParseTest);
    fprintf(stderr, "SUBSSynth...\n");
    call(SUBSSynthTest);
    fprintf(stderr, "SUBSIsMount...\n");
    call(SUBSIsMountTest);
    fprintf(stderr, "SUBSSeedPreservesLog...\n");
    call(SUBSSeedPreservesLog);
    fprintf(stderr, "all passed\n");
    done;
}

TEST(maintest);
