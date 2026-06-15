//  RESOLVEURI — REFSResolveURI / KEEPResolveURI: the DIS-025 Stage 2
//  text-in/text-out canonicalising funnel.  Hermetic, table-driven.
//
//  A tiny store (one project shard "proj", two commit objects, a few
//  REFS rows) is stood up, then raw user ref queries are canonicalised
//  and checked byte-for-byte against the three structural shapes:
//      ?/proj/<sha>            TRUNK
//      ?/proj//<sha>           DETACHED
//      ?/proj/feat/<sha>       BRANCH

#include "keeper/RESOLVE.h"

#include <stdio.h>
#include <string.h>

#include "abc/B.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/S.h"
#include "abc/TEST.h"
#include "dog/HOME.h"
#include "dog/WHIFF.h"
#include "dog/test/TESTBE.h"

#include "keeper/KEEP.h"
#include "keeper/REFS.h"

//  Render a sha1 as a NUL-terminated 40-char ASCII hex string.
static void sha_hex40(char *out41, sha1cp s) {
    a_sha1hex(sl, s);
    memcpy(out41, sl[0], 40);
    out41[40] = 0;
}

//  Append one REFS row (`from_uri` key, 40-hex `to_uri` value) into the
//  project shard's reflog.
static ok64 add_ref(char const *key, char const *sha40) {
    sane(key && sha40);
    a_path(refsdir);
    call(HOMEBranchDir, refsdir, NULL);
    a_cstr(ks, key);
    a_cstr(vs, sha40);
    call(REFSAppend, u8bDataC(refsdir), ks, vs);
    done;
}

//  Run one REFSResolveURI case: input query → expected canonical text.
static ok64 check_ref(char const *in, char const *expect) {
    sane(in && expect);
    a_cstr(in_s, in);
    u8 pad[320];
    u8s out = {pad, pad + sizeof(pad)};
    call(REFSResolveURI, out, in_s);
    u8cs got = {pad, out[0]};
    size_t el = strlen(expect);
    if ((size_t)u8csLen(got) != el || memcmp(got[0], expect, el) != 0) {
        fprintf(stderr, "REFSResolveURI('%s'): got '%.*s', want '%s'\n",
                in, (int)u8csLen(got), (char const *)got[0], expect);
        fail(FAIL);
    }
    done;
}

//  Run one KEEPResolveURI case (full URI in / out).
static ok64 check_uri(char const *in, char const *expect) {
    sane(in && expect);
    a_cstr(in_s, in);
    u8 pad[384];
    u8s out = {pad, pad + sizeof(pad)};
    call(KEEPResolveURI, out, in_s);
    u8cs got = {pad, out[0]};
    size_t el = strlen(expect);
    if ((size_t)u8csLen(got) != el || memcmp(got[0], expect, el) != 0) {
        fprintf(stderr, "KEEPResolveURI('%s'): got '%.*s', want '%s'\n",
                in, (int)u8csLen(got), (char const *)got[0], expect);
        fail(FAIL);
    }
    done;
}

ok64 RESOLVEURItest() {
    sane(1);
    call(FILEInit);

    char dir[256];
    want(TESTBEmkdtemp(dir, sizeof dir) == OK);
    a_cstr(root, dir);

    call(HOMEOpenAt, root, YES);

    //  Project shard "proj"; cur branch "feat/" (drives relative refs).
    a_cstr(projn, "proj");
    u8bReset(HOME.project);
    call(u8bFeed, HOME.project, projn);

    call(KEEPOpen, YES);

    a_cstr(curn, "feat/");
    u8bReset(HOME.cur_branch);
    call(u8bFeed, HOME.cur_branch, curn);
    HOME.cur_held = YES;

    //  Two commit objects: S0 (trunk tip), S1 (feat / feat/sub tip).
    keep_pack p = {};
    call(KEEPPackOpen, &p);
    a_cstr(c0, "tree 4b825dc642cb6eb9a060e54bf8d69288fbee4904\n\ntrunk\n");
    sha1 s0 = {};
    call(KEEPPackFeed, &p, DOG_OBJ_COMMIT, c0, 0, &s0);
    a_cstr(c1, "tree 4b825dc642cb6eb9a060e54bf8d69288fbee4904\n\nfeat\n");
    sha1 s1 = {};
    call(KEEPPackFeed, &p, DOG_OBJ_COMMIT, c1, 0, &s1);
    call(KEEPPackClose, &p);

    char S0[41], S1[41];
    sha_hex40(S0, &s0);
    sha_hex40(S1, &s1);

    //  REFS: trunk → S0, feat → S1, feat/sub → S1.
    call(add_ref, "?",         S0);
    call(add_ref, "?feat",     S1);
    call(add_ref, "?feat/sub", S1);
    //  URI-001 Stage 1: branches whose NAMES are all-hex.  Branch-first
    //  resolution must pick the branch over a same-spelled hashlet.
    //  `dead` (4 hex) and `c0ffee` (6 hex) never collide with a real
    //  object prefix, so the only way they resolve is as branches.
    call(add_ref, "?dead",     S0);
    call(add_ref, "?c0ffee",   S1);

    //  Expected canonical strings.
    char e_trunk[64], e_feat[96], e_featsub[96], e_detach[64];
    char e_dead[96], e_coffee[96];
    //  URI-001 Stage 3 canonical forms: the funnel canonicalises the
    //  SCOPE only (no tip pinned into a fragment).  Detached carries the
    //  full sha IN the query (the ref's identity, no branch to scope by).
    snprintf(e_trunk,   sizeof e_trunk,   "?/proj");
    snprintf(e_feat,    sizeof e_feat,    "?/proj/feat");
    snprintf(e_featsub, sizeof e_featsub, "?/proj/feat/sub");
    snprintf(e_detach,  sizeof e_detach,  "?/proj/%s", S0);
    snprintf(e_dead,    sizeof e_dead,    "?/proj/dead");
    snprintf(e_coffee,  sizeof e_coffee,  "?/proj/c0ffee");

    //  --- REF arm ---
    //  trunk (empty / bare ?)
    call(check_ref, "",   e_trunk);
    call(check_ref, "?",  e_trunk);
    //  named branch (with / without leading ?)
    call(check_ref, "?feat", e_feat);
    call(check_ref, "feat",  e_feat);
    //  nested branch + relative forms (cur = feat/)
    call(check_ref, "?feat/sub", e_featsub);
    call(check_ref, "?./sub",    e_featsub);   // ./sub from feat/
    call(check_ref, "?..",       e_trunk);     // parent of feat = trunk

    //  URI-001 Stage 1: hex-spelled branch names resolve branch-first.
    //  `dead` (4 hex) used to mis-emit as detached `?/proj//<sha>`;
    //  `c0ffee` (6 hex) used to be unreachable (shadowed by a failing
    //  hashlet lookup).  Bareword (no `?`) must promote identically.
    call(check_ref, "?dead",   e_dead);
    call(check_ref, "dead",    e_dead);
    call(check_ref, "?c0ffee", e_coffee);
    call(check_ref, "c0ffee",  e_coffee);

    //  detached: full sha + hashlet, both → ?/proj//<sha>
    {
        char in_full[64];
        snprintf(in_full, sizeof in_full, "?%s", S0);
        call(check_ref, in_full, e_detach);
        char in_hl[16];
        snprintf(in_hl, sizeof in_hl, "?%.8s", S0);
        call(check_ref, in_hl, e_detach);
    }

    //  idempotent: already-canonical input re-emits verbatim
    call(check_ref, e_trunk,   e_trunk);
    call(check_ref, e_feat,    e_feat);
    call(check_ref, e_detach,  e_detach);

    //  --- KEEPResolveURI: path / authority preserved verbatim
    //      (Stage-3 arms), query canonicalised by the REF arm ---
    {
        char in_path[96], ex_path[128];
        snprintf(in_path, sizeof in_path, "./file.c?feat");
        snprintf(ex_path, sizeof ex_path, "./file.c?/proj/feat");
        call(check_uri, in_path, ex_path);

        char in_auth[96], ex_auth[128];
        snprintf(in_auth, sizeof in_auth, "//host?feat");
        snprintf(ex_auth, sizeof ex_auth, "//host?/proj/feat");
        call(check_uri, in_auth, ex_auth);
    }

    (void)KEEPClose();
    HOMEClose();
    TESTBErmrf(dir);
    done;
}

ok64 maintest() {
    sane(1);
    fprintf(stderr, "RESOLVEURItest...\n"); call(RESOLVEURItest);
    fprintf(stderr, "all passed\n");
    done;
}

TEST(maintest)
