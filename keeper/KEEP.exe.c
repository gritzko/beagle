//  KEEPExec — run a parsed CLI against an open keeper state.
//  Same effect as invoking `keeper ...` as a separate process.
//
#include "KEEP.h"
#include "PROJ.h"
#include "REFS.h"
#include "RESOLVE.h"
#include "SUBS.h"
#include "WIRE.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PRO.h"
#include "dog/CLI.h"
#include "dog/DOG.h"
#include "dog/WHIFF.h"
#include "dog/git/GIT.h"

// --- Verb / flag tables ---

char const *const KEEP_CLI_VERBS[] = {
    "get", "put", "post", "delete", "status", "import", "verify",
    "refs", "tips", "ls-files", "subs",
    "upload-pack", "receive-pack",
    "help", NULL
};

char const KEEP_CLI_VAL_FLAGS[] = "--want\0--have\0--at\0";

// --- Usage ---

static void keep_usage(void) {
    fprintf(stderr,
        "Usage: keeper <verb> [flags] [URI...]\n"
        "\n"
        "  Verbs:\n"
        "    get //remote[?ref]         fetch objects from remote\n"
        "    get .#hashprefix           cat object to stdout\n"
        "    get .?refname              resolve ref to SHA\n"
        "    put .?ref .#sha            move local ref pointer\n"
        "    put //remote?ref           force-push cur's tip to remote ref\n"
        "    post //remote              push HEAD's tip to remote\n"
        "    delete //remote?ref        push-delete a remote ref\n"
        "    delete //remote            drop alias (tombstone host rows)\n"
        "    status                     show store stats\n"
        "    import <packfile>          import a git packfile\n"
        "    verify .#sha               verify object + recurse\n"
        "    refs                       list known refs\n"
        "    tips                       list local-branch tips\n"
        "    ls-files [URI]             list files reachable from ref/sha\n"
        "    upload-pack <repo-path>    git-upload-pack drop-in (stdin/stdout)\n"
        "    receive-pack <repo-path>   git-receive-pack drop-in (stdin/stdout)\n"
        "    help                       this message\n"
    );
}

// --- Helpers ---

static ok64 refs_print_cb(refcp r, void *ctx) {
    int *count = (int *)ctx;
    fprintf(stdout, "  %.*s\t→ %.*s\n",
            (int)$len(r->key), (char *)r->key[0],
            (int)$len(r->val), (char *)r->val[0]);
    (*count)++;
    return OK;
}

// --- Verb: status ---

static ok64 keeper_status(keeper *k) {
    sane(k);
    //  Show every loaded pack/idx across PastData — status is a
    //  cross-branch overview, not a per-leaf write summary.
    u32 nruns  = DOGPupCountAll(k->puppies);
    u32 npacks = (u32)kv64bPastDataLen(k->packs);
    fprintf(stdout, "keeper: %u pack file(s), %u index run(s)\n",
            npacks, nruns);
    u64 total_pack = 0;
    {
        kv64s packs_all = {};
        kv64PastDataS(k->packs, packs_all);
        for (kv64 const *p = packs_all[0]; p < packs_all[1]; p++) {
            u8bp slot = FILE_WANT_BUFS[p->val];
            if (slot && slot[0]) total_pack += (u64)u8bDataLen(slot);
        }
    }
    u64 total_idx = 0;
    for (u32 i = 0; i < nruns; i++) {
        u8cs raw = {NULL, NULL};
        DOGPupDataAll(raw, k->puppies, i);
        total_idx += (u64)(raw[1] - raw[0]);
    }
    fprintf(stdout, "  packs: %llu bytes\n", (unsigned long long)total_pack);
    fprintf(stdout, "  index: %llu entries\n",
            (unsigned long long)(total_idx / sizeof(wh128)));
    done;
}

// --- Verb: import ---

static ok64 keeper_import(keeper *k, u8cs path) {
    sane(k && $ok(path));
    call(KEEPImport, path);
    done;
}

// --- Verb: verify ---

static ok64 keeper_verify(keeper *k, u8cs hex) {
    sane(k && $ok(hex));
    return KEEPVerify(hex);
}

// --- Verb: ls-files ---

#include "WALK.h"

//  Visitor for `keeper ls-files`.  Prints one line per leaf entry in
//  `git ls-tree -r` format:  "<mode> <type> <sha40>\t<path>\n".
//  Skips intermediate tree events (we only want leaves).
static ok64 keeper_lsfiles_visit(u8cs path, u8 kind, u8cp esha,
                                  u8cs blob, void0p ctx) {
    (void)blob; (void)ctx;
    char const *mode = NULL;
    char const *type = NULL;
    switch (kind) {
        case WALK_KIND_REG: mode = "100644"; type = "blob";   break;
        case WALK_KIND_EXE: mode = "100755"; type = "blob";   break;
        case WALK_KIND_LNK: mode = "120000"; type = "blob";   break;
        case WALK_KIND_SUB: mode = "160000"; type = "commit"; break;
        case WALK_KIND_DIR:
            //  Skip directory events: git ls-tree -r omits them.
            //  The root visit also arrives with empty path; either way
            //  we only surface leaves.
            return OK;
        default:
            return OK;
    }
    char hex[41];
    for (int i = 0; i < 20; i++)
        snprintf(hex + 2 * i, 3, "%02x", esha[i]);
    fprintf(stdout, "%s %s %s\t%.*s\n",
            mode, type, hex,
            (int)$len(path), (char *)path[0]);
    return OK;
}

static ok64 keeper_lsfiles(keeper *k, uricp target) {
    sane(k && target);
    return KEEPLsFiles(target, keeper_lsfiles_visit, NULL);
}

// --- Verb: refs ---

static ok64 keeper_refs(keeper *k) {
    sane(k);
    a_path(keepdir);
    call(HOMEBranchDir, k->h, keepdir, NULL);
    int rcount = 0;
    ok64 o = REFSEach($path(keepdir), refs_print_cb, &rcount);
    if (o != OK && o != REFSNONE)
        fprintf(stderr, "keeper: refs: %s\n", ok64str(o));
    fprintf(stdout, "keeper: %d ref(s)\n", rcount);
    done;
}

// --- Verb: subs ---
//
//  `keeper subs ?<ref>` — enumerate submodules declared in the tree
//  of the commit `<ref>` resolves to.  Writes one KEEPSubsAt ULOG row
//  per declared sub to stdout (`<ts>\t<verb>\t<url>?<mount-path>#<pin>\n`).
//  Beagle invokes this from its BEGet wrapper to drive the
//  per-project mount/recurse orchestration (SUBS.plan.md §GET).
//
//  `<ref>` may be a branch path or a sha prefix; resolution goes
//  through the same REFSResolve path as `keeper get .?<ref>`.

static ok64 keeper_subs(keeper *k, cli *c) {
    sane(k && c);
    uri uv = {};
    if (CLIUriLen(c) > 0) (void)CLIUriAt(&uv, c, 0);
    if (CLIUriLen(c) < 1 ||
        (u8csEmpty(uv.query) && u8csEmpty(uv.fragment))) {
        fprintf(stderr, "keeper: subs requires `?<ref>` URI\n");
        return KEEPFAIL;
    }
    uri *u = &uv;

    //  DIS-035: the PATCH whole-branch scope `!` rides on the query
    //  (`?/<proj>/<sha>!` — the local form `be`'s BEActResolveRemote
    //  rewrites a fetched source to).  It is a sniff PATCH concern, never
    //  part of a sub-pin resolution; shed it so `<sha>!` classifies as a
    //  direct sha (DOGIsFullSha) instead of falling through to an
    //  unresolvable banged refname (RESLVNONE).
    (void)DOGDebangSlice(u->query);

    //  Resolve ?<ref> to a 40-hex commit sha.  Direct-sha shortcuts:
    //    * `?<branch>#<sha>` — fragment carries the sha; query carries
    //      the branch (already consumed by KEEP.cli.c to open the
    //      right leaf shard).  Use the fragment directly.
    //    * `?<sha>` — query IS the 40-hex sha (legacy direct form).
    //  Either skips REFSResolve.  Falls through to REFSResolve when
    //  the input is a ref name to look up.
    sha1 commit_sha = {};
    if (DOGIsFullSha(u->fragment)) {
        if (sha1FromHex(&commit_sha, u->fragment) != OK) {
            fprintf(stderr, "keeper: subs: bad sha\n");
            fail(KEEPFAIL);
        }
    } else if (DOGIsFullSha(u->query)) {
        if (sha1FromHex(&commit_sha, u->query) != OK) {
            fprintf(stderr, "keeper: subs: bad sha\n");
            fail(KEEPFAIL);
        }
    } else {
        //  Scope-only canonical ref (`/<project>/<branch>`, no pin —
        //  URI-001 Stage 3): strip the project and resolve the branch
        //  tip through the canonical funnel.  KEEPResolveRef is
        //  branch-first and walks the leaf shard + trunk with the
        //  refname-strip retries (sub-shard fetch isolation), so it
        //  subsumes the old manual leaf/trunk REFSResolve.  The legacy
        //  pin-in-query form relied on REFSResolve's canonic
        //  short-circuit to lift the pin; scope-only has none.
        a_dup(u8c, branch, u->query);
        DOGQueryStripProject(branch);
        u8cs cur_b = {};
        u8csMv(cur_b, u8bDataC(k->h->cur_branch));
        ok64 ro = KEEPResolveRef(&commit_sha, branch, cur_b);
        if (ro != OK) {
            fprintf(stderr, "keeper: subs: ref %.*s not resolvable\n",
                    (int)u8csLen(u->query), (char *)u->query[0]);
            return ro;
        }
    }

    sha1 tree_sha = {};
    ok64 to = KEEPCommitTreeSha(&commit_sha, &tree_sha);
    if (to != OK) {
        fprintf(stderr, "keeper: subs: commit not found\n");
        return to;
    }

    //  Drive KEEPSubsAt; pipe the ULOG straight to stdout.  ts=0
    //  (caller has no per-command ts); verb=`sub`.
    Bu8 out = {};
    call(u8bAlloc, out, 64UL << 10);
    a_cstr(sub_word, "sub");
    a_dup(u8c, sub_d, sub_word);
    ron60 verb = 0;
    call(RONutf8sDrain, &verb, sub_d);
    ok64 so = KEEPSubsAt(&tree_sha, 0, verb, out);
    if (so == OK || so == KEEPNONE) {
        a_dup(u8c, body, u8bData(out));
        if (!u8csEmpty(body))
            write(STDOUT_FILENO, body[0], u8csLen(body));
        u8bFree(out);
        done;
    }
    u8bFree(out);
    fprintf(stderr, "keeper: subs: %s\n", ok64str(so));
    return so;
}

// --- Verb: tips ---
//
//  Print every local-branch tip via KEEPEachTip.  Trunk renders as a
//  bare `?` row; child branches as `?<path>`.  One row per branch,
//  tab-separated `<path>\t<sha40>`, terminated by `keeper: N tip(s)`.

static ok64 keeper_tips_print_cb(keep_tipcp t, void *ctx) {
    int *n = (int *)ctx;
    fprintf(stdout, "?%.*s\t%.*s\n",
            (int)$len(t->path), (char *)t->path[0],
            (int)$len(t->sha),  (char *)t->sha[0]);
    (*n)++;
    return OK;
}

static ok64 keeper_tips(keeper *k) {
    sane(k);
    int n = 0;
    ok64 o = KEEPEachTip(keeper_tips_print_cb, &n);
    if (o != OK && o != REFSNONE)
        fprintf(stderr, "keeper: tips: %s\n", ok64str(o));
    fprintf(stdout, "keeper: %d tip(s)\n", n);
    done;
}

// --- Verb: get ---

//  Build a transport URI `[<scheme>:]//<host>/<path>` from `g` into
//  `out`.  When `g`'s authority is a substring of any stored origin in
//  REFS (e.g. `//github` matches `https://github.com/…?…`), that row's
//  scheme/host/path win.  Drops query/fragment — those carry the
//  ref/object selector, not the transport target.  `rarena_out` is a
//  caller-owned buffer backing the resolved slices; caller u8bUnMap's
//  it after finishing with the resolved URI bytes.
//  Scheme-only completion (GET-002 part 2).  `be get ssh:` names a
//  transport but no authority/path — reuse the RECENTMOST get/post row
//  recorded under that same transport scheme (its full
//  authority+path+query).  REFSEachRecord walks every row in
//  chronological order; we keep the last one whose scheme matches and
//  that carries a host (a real wire target), copying its bytes into the
//  caller-owned `arena` (the row's mmap slices die when the callback
//  returns).
typedef struct {
    u8cs scheme;     //  needle — the bare transport scheme to match
    u8bp arena;      //  caller buffer backing the copied-out slices
    u8cs host;       //  most-recent matched host   (into arena)
    u8cs path;       //  most-recent matched path   (into arena)
    u8cs query;      //  most-recent matched query  (into arena)
    b8   found;
} keeper_scheme_ctx;

static ok64 keeper_scheme_match(uri const *u, ron60 ts, ron60 verb,
                                void *vctx) {
    sane(u && vctx);
    (void)ts; (void)verb;
    keeper_scheme_ctx *m = (keeper_scheme_ctx *)vctx;
    u8cs row_scheme = {u->scheme[0], u->scheme[1]};
    if (u8csEmpty(row_scheme) || !u8csEq(row_scheme, m->scheme)) done;
    u8cs row_host  = {u->host[0],  u->host[1]};
    u8cs row_path  = {u->path[0],  u->path[1]};
    u8cs row_query = {u->query[0], u->query[1]};
    //  Need a wire target: a host (`ssh://host/...`) OR a path (a
    //  host-less `file://<abs>` clone source).  Host-only `//alias`
    //  rows with no transport are no use here.
    if (u8csEmpty(row_host) && u8csEmpty(row_path)) done;
    //  Latest-wins: overwrite the previous match (chronological order).
    //  Copy each component's bytes into the caller arena and record the
    //  copied-out slice (the row's mmap dies when this callback returns).
    u8bp arena = m->arena;
    u8cp head = u8bIdleHead(arena);
    try(u8bFeed, arena, row_host);
    m->host[0] = head; m->host[1] = u8bIdleHead(arena);
    head = u8bIdleHead(arena);
    if (!u8csEmpty(row_path)) try(u8bFeed, arena, row_path);
    m->path[0] = head; m->path[1] = u8bIdleHead(arena);
    head = u8bIdleHead(arena);
    if (!u8csEmpty(row_query)) try(u8bFeed, arena, row_query);
    m->query[0] = head; m->query[1] = u8bIdleHead(arena);
    m->found = YES;
    done;
}

static ok64 keeper_remote_uri(keeper *k, uri *g, u8b out, u8b rarena_out) {
    sane(k && g && u8bOK(out) && u8bOK(rarena_out));
    a_path(keepdir);
    call(HOMEBranchDir, k->h, keepdir, NULL);

    u8cs rscheme = {};
    u8cs rhost = {};
    u8cs rpath = {};
    u8cs rquery = {};   //  recovered project selector (scheme-only path)
    u8csMv(rscheme, g->scheme);
    u8csMv(rhost, g->host);
    u8csMv(rpath, g->path);

    if (!u8csEmpty(g->authority)) {
        //  Explicit transport URI (scheme + path both present) wins
        //  verbatim — the user named the wire target unambiguously.
        //  REFSResolve only fills missing components for cached-form
        //  `//host` lookups (where the user relies on alias lookup).
        b8 has_explicit_path  = !u8csEmpty(rpath);
        b8 has_explicit_proto = !u8csEmpty(rscheme);
        b8 explicit_full = has_explicit_path && has_explicit_proto;
        uri resolved = {};
        ok64 rr = OK;
        if (!explicit_full) {
            a_dup(u8c, in_uri, g->data);
            //  Leaf-first lookup: wire-recorded peer URIs land in the
            //  active leaf shard's REFS (sub-shard / sub-clone isolation,
            //  per `wcli_record_ref`).  Trunk holds only sniff-POST rows
            //  in that case.  Try leaf first, fall back to trunk.
            rr = REFSNONE;
            if (u8bDataLen(k->h->cur_branch) > 0) {
                a_path(leafdir);
                a_dup(u8c, trunk_s, u8bDataC(keepdir));
                call(PATHu8bFeed, leafdir, trunk_s);
                a_dup(u8c, leaf_s, u8bDataC(k->h->cur_branch));
                call(PATHu8bAdd, leafdir, leaf_s);
                rr = REFSResolve(&resolved, rarena_out, $path(leafdir),
                                 in_uri);
            }
            //  Retry against the trunk shard when the leaf lookup found
            //  no usable transport target.  A host-less `file://` clone
            //  source resolves a path but no host, so "usable" is host OR
            //  path — gating on host alone would skip a local source
            //  (replicated.wiki todo/GET-002 part 1).
            if (rr != OK ||
                (u8csEmpty(resolved.host) && u8csEmpty(resolved.path))) {
                zero(resolved);
                rr = REFSResolve(&resolved, rarena_out, $path(keepdir),
                                 in_uri);
            }
        }
        //  Fill the missing transport components from the persisted clone
        //  source.  A host-bearing source (`ssh://host/path`) refills
        //  host+path; a host-less `file://` source has no host but a real
        //  path — gating on host alone dropped that path and shipped an
        //  empty repo to the wire (WIRECLFL).  So the completion fires
        //  whenever the resolved source carries a host OR a path.
        if (!explicit_full && rr == OK &&
            (!u8csEmpty(resolved.host) || !u8csEmpty(resolved.path))) {
            if (!u8csEmpty(resolved.scheme)) u8csMv(rscheme, resolved.scheme);
            if (!u8csEmpty(resolved.host))   u8csMv(rhost, resolved.host);
            if (!u8csEmpty(resolved.path))   u8csMv(rpath, resolved.path);
        } else if (!explicit_full &&
                   u8csEmpty(rscheme) && u8csEmpty(rpath)) {
            //  Alias miss on a bare `//host` URI with no in-place
            //  transport — the user named a remote that's not
            //  registered.  Emit a friendly hint instead of letting
            //  wcli_spawn bail later with a cryptic ENOENT/empty-path.
            fprintf(stderr,
                "keeper: unknown remote //%.*s — register first with "
                "`be get scheme://host/path?ref`, or pass a full URL\n",
                (int)$len(rhost), (char const *)rhost[0]);
            return KEEPNONE;
        }
    } else if (!u8csEmpty(g->scheme)) {
        //  SCHEME ONLY (`ssh:` / `file:` — no authority, GET-002 part
        //  2).  Adopt the full transport target (host + path + project
        //  query) of the RECENTMOST get/post row recorded under this
        //  same transport scheme.  The user's own `?ref` (g->query) is
        //  still the want — only an absolute `?/<project>` selector is
        //  rebuilt onto the wire URI below.
        keeper_scheme_ctx sc = {.scheme = {g->scheme[0], g->scheme[1]},
                                .arena = rarena_out};
        ok64 se = REFSEachRecord($path(keepdir), keeper_scheme_match, &sc);
        if (se == OK && sc.found) {
            u8csMv(rhost, sc.host);
            if (!u8csEmpty(sc.path))  u8csMv(rpath, sc.path);
            if (!u8csEmpty(sc.query)) u8csMv(rquery, sc.query);
        } else {
            fprintf(stderr,
                "keeper: bare scheme %.*s: — no prior get/post recorded "
                "for this transport; pass a full URL\n",
                (int)$len(rscheme), (char const *)rscheme[0]);
            return KEEPNONE;
        }
    }

    if (!u8csEmpty(rscheme)) {
        u8bFeed(out, rscheme);
        u8bFeed1(out, ':');
    }
    a_cstr(slashes, "//");
    u8bFeed(out, slashes);
    u8bFeed(out, rhost);
    if (!u8csEmpty(rpath)) {
        //  Make sure exactly one '/' separates host from path.  ssh
        //  HOME-relative stripping is wcli_spawn's job (it knows the
        //  transport).
        if (*rpath[0] != '/') u8bFeed1(out, '/');
        u8bFeed(out, rpath);
    }
    //  Preserve an absolute `?/<project>` selector so the rebuilt URI
    //  still names the requested shard; wcli_spawn forwards it on the
    //  served path (the peer routes via keeper_served_at → HOMEOpen
    //  step 5).  Leading '/' gates it to the project form — a bare
    //  `?ref` is the want, carried separately by WIREFetch.  Precedence:
    //  the user's explicit `?/<project>` wins; else the project query
    //  recovered from the recentmost same-scheme row (GET-002 part 2).
    if (!u8csEmpty(g->query) && *g->query[0] == '/') {
        u8bFeed1(out, '?');
        u8bFeed(out, g->query);
    } else if (!u8csEmpty(rquery) && *rquery[0] == '/') {
        u8bFeed1(out, '?');
        u8bFeed(out, rquery);
    }
    done;
}

//  `keeper get //remote[?ref]` — fetch via WIREFetch.  Empty ?ref means
//  fast-forward the current worktree branch (per https://replicated.wiki/html/wiki/Verbs.html `be get //origin`).
ok64 KEEPGetRemote(uri *g) {
    sane(g);
    keeper *k = &KEEP;

    //  DIS-035: the PATCH whole-branch scope modifier `!` rides on the
    //  QUERY (`?<proj>!` / `?<proj>/<br>!`) — but it is purely a sniff
    //  PATCH concern (carried separately to sniff by `be`'s
    //  BEActResolveRemote), NEVER part of the wire fetch.  Shed it here,
    //  at the single fetch chokepoint, BEFORE any project / ref
    //  derivation: otherwise the trailing `!` leaks into the
    //  `?/<project>` shard selector, the project title derives as
    //  `<proj>!` (a shard that doesn't exist), and both the local
    //  resolution AND the wire-served path die `HOMENOPROJ` → `WIRECLFL`.
    //  WIREFetch already debangs the want-ref, but the project selector
    //  needed the same treatment — so one debang on the whole query
    //  covers both `?<proj>!` and `?<proj>/<br>!`.
    (void)DOGDebangSlice(g->query);

    Bu8 rarena = {};
    call(u8bMap, rarena, (size_t)REFS_MAX_REFS * 320);
    a_pad(u8, ubuf, FILE_PATH_MAX_LEN);
    ok64 ru = keeper_remote_uri(k, g, ubuf, rarena);
    if (ru != OK) {
        u8bUnMap(rarena);
        return ru;
    }
    a_dup(u8c, remote_uri, u8bDataC(ubuf));

    //  Default ref: current worktree branch (`be get //origin` semantics).
    //
    //  Absolute query (`?/<project>/<branch>`) carries a project
    //  prefix that's local-side state (already consumed by
    //  home_open_inner + be_ensure_project_repo).  Strip it so the
    //  wire sees just the branch portion (`DOGQueryStripProject`).
    u8cs want_ref = {};
    u8csMv(want_ref, g->query);
    DOGQueryStripProject(want_ref);

    //  `?*` wildcard: bulk-fetch every advertised heads/tags ref in
    //  one upload-pack session (multi-want).  See https://replicated.wiki/html/wiki/HEAD.html §HEAD —
    //  `be head ssh://origin?*` mirrors `git fetch`.
    if ($len(want_ref) == 1 && want_ref[0][0] == '*') {
        ok64 fa = WIREFetchAll(remote_uri);
        u8bUnMap(rarena);
        return fa;
    }

    //  Local short-circuit for `?<40hex>` queries: plain git peers
    //  reject `want <sha>` without uploadpack.allowReachableSHA1InWant,
    //  so the supported flow is "seed with a named ref first, then
    //  look up by sha".  If the object is already in the local store,
    //  skip the wire round-trip entirely.
    if (DOGIsFullSha(want_ref)) {
        u64 hashlet = WHIFFHexHashlet60(want_ref);
        u64 val = 0;
        if (KEEPLookup(hashlet, 40, &val) == OK) {
            u8bUnMap(rarena);
            return OK;
        }
    }

    a_pad(u8, branch_buf, 256);
    u8cs cur_branch = {};
    //  No explicit ref → let `wcli_match_advert` pick the peer's
    //  HEAD-mapped branch (mirrors `git clone`'s default behavior).
    //  Previously we defaulted to `heads/<cur_branch>` from the
    //  HOMEOpen anchor, but cur_branch is the LOCAL leaf (e.g.
    //  "origin" for a sub-clone), not a remote-ref name; emitting
    //  `want heads/origin` against a peer that only advertises
    //  `heads/master` ends the wire with WIRECLNRF.
    (void)cur_branch;
    (void)branch_buf;

    ok64 fo = WIREFetch(remote_uri, want_ref);
    if (fo != OK) {
        //  Journal the failed fetch so recovery / audit tooling sees a
        //  `get_fail <peer-uri>?<branch>` row alongside the success
        //  rows already emitted by wcli_record_ref.  Best-effort write
        //  — we still return the underlying fetch error.
        a_path(keepdir);
        call(HOMEBranchDir, k->h, keepdir, NULL);
        a_pad(u8, key_buf, 512);
        u8bFeed(key_buf, remote_uri);
        u8bFeed1(key_buf, '?');
        if (!u8csEmpty(want_ref)) u8bFeed(key_buf, want_ref);
        a_dup(u8c, key, u8bData(key_buf));
        a_cstr(empty_to, "");
        //  Best-effort journal — preserve the underlying fetch error; log
        //  any append failure via try().
        try(REFSAppendVerb, $path(keepdir), REFSVerbGetFail(),
            key, empty_to);
    }
    u8bUnMap(rarena);
    return fo;
}

static ok64 keeper_get_object(keeper *k, u8cs prefix) {
    sane(k && $ok(prefix));
    if (u8csLen(prefix) < HASH_MIN_HEX) {
        fprintf(stderr, "keeper: hash too short (min %d)\n",
                HASH_MIN_HEX);
        return KEEPFAIL;
    }
    size_t hexlen = u8csLen(prefix);
    u64 hashlet = WHIFFHexHashlet60(prefix);
    Bu8 out = {};
    call(u8bMap, out, 64UL << 20);
    u8 obj_type = 0;
    ok64 o = KEEPGet(hashlet, hexlen, out, &obj_type);
    if (o == OK) {
        a_dup(u8c, data, u8bData(out));
        write(STDOUT_FILENO, data[0], u8csLen(data));
    } else {
        fprintf(stderr, "keeper: object not found\n");
    }
    u8bUnMap(out);
    return o;
}

static ok64 keeper_get_ref(keeper *k, u8cs query) {
    sane(k && $ok(query));
    a_path(keepdir);
    call(HOMEBranchDir, k->h, keepdir, NULL);

    a_pad(u8, qbuf, 256);
    u8bFeed1(qbuf, '?');
    u8bFeed(qbuf, query);
    a_dup(u8c, qkey, u8bData(qbuf));

    a_pad(u8, arena, 1024);
    uri resolved = {};
    ok64 ro = REFSResolve(&resolved, arena, $path(keepdir), qkey);
    if (ro == OK && !u8csEmpty(resolved.query)) {
        fprintf(stdout, "%.*s\n",
                (int)u8csLen(resolved.query),
                (char *)resolved.query[0]);
        done;
    }
    fprintf(stderr, "keeper: ref not found\n");
    return REFSNONE;
}

//  Blob projector: `keeper get <path>?<ref>` — resolves the path inside
//  the ref's tree via KEEPGetByURI and writes the blob bytes to stdout.
//  No sniff, no checkout, no worktree side effects.
static ok64 keeper_get_blob(keeper *k, uri *g) {
    sane(k && g);
    Bu8 out = {};
    call(u8bAlloc, out, 64UL << 20);
    ok64 go = KEEPGetByURI(g, out);
    if (go == OK) {
        a_dup(u8c, data, u8bData(out));
        write(STDOUT_FILENO, data[0], u8csLen(data));
    } else {
        fprintf(stderr, "keeper: blob not found: %s\n", ok64str(go));
    }
    u8bFree(out);
    return go;
}

static ok64 keeper_get(keeper *k, cli *c) {
    sane(k && c);
    if (CLIUriLen(c) == 0) {
        fprintf(stderr, "keeper: get requires a URI\n");
        return KEEPFAIL;
    }
    uri gv = {};
    call(CLIUriAt, &gv, c, 0);
    uri *g = &gv;

    if (!u8csEmpty(g->authority))
        return KEEPGetRemote(g);
    //  Scheme-only transport URI (`ssh:` / `https:` — authority absent,
    //  GET-002 part 2): route to the remote path so keeper_remote_uri
    //  can complete it from the recentmost same-scheme get/post row.
    //  (`file:` / `be:` / `keeper:` land here via KEEPExec's `plain`
    //  block above.)  A scheme with a `?query` would otherwise be
    //  mis-read as a local ref lookup below.
    if (!u8csEmpty(g->scheme) && DOGIsTransport(g->scheme))
        return KEEPGetRemote(g);
    if (!u8csEmpty(g->fragment))
        return keeper_get_object(k, g->fragment);
    //  path+query (no authority) is a blob projector: resolve `path` in
    //  `?ref`'s tree and cat its bytes.  Disambiguates from a bare ref
    //  resolution (query-only), which only prints the resolved sha.
    if (!u8csEmpty(g->path) && !u8csEmpty(g->query))
        return keeper_get_blob(k, g);
    if (!u8csEmpty(g->query))
        return keeper_get_ref(k, g->query);

    fprintf(stderr, "keeper: get: need //remote, #hash, ?ref, or path?ref\n");
    return KEEPFAIL;
}

// --- Verb: put ---

static ok64 keeper_put(keeper *k, cli *c) {
    sane(k && c);
    if (CLIUriLen(c) == 0) {
        fprintf(stderr, "keeper: put requires a URI\n");
        return KEEPFAIL;
    }
    uri gv = {};
    call(CLIUriAt, &gv, c, 0);
    uri *g = &gv;

    if (!u8csEmpty(g->authority)) {
        fprintf(stderr, "keeper: remote push not yet implemented\n");
        return KEEPFAIL;
    }

    u8cs ref_name = {};
    u8cs sha_frag = {};

    for (u32 i = 0; i < CLIUriLen(c); i++) {
        uri uv = {};
        call(CLIUriAt, &uv, c, i);
        if (!u8csEmpty(uv.query) && !$ok(ref_name))
            u8csMv(ref_name, uv.query);
        if (!u8csEmpty(uv.fragment) && !$ok(sha_frag))
            u8csMv(sha_frag, uv.fragment);
    }

    if (!$ok(ref_name) || !$ok(sha_frag)) {
        fprintf(stderr, "keeper: put requires ?ref and #sha\n");
        return KEEPFAIL;
    }

    a_path(keepdir);
    call(HOMEBranchDir, k->h, keepdir, NULL);

    //  Canonical key: build a query-only URI with the user's ref
    //  name and canonicalise — strips `refs/` and collapses the
    //  trunk aliases so `heads/master` / `master` / `refs/heads/main`
    //  all become bare `?` (trunk).
    uri uk = {};
    u8csMv(uk.query, ref_name);
    a_pad(u8, fbuf, 256);
    call(DOGCanonURIFeed, fbuf, &uk);
    a_dup(u8c, from, u8bDataC(fbuf));

    //  Canonical value: strip a leading `?` if the user supplied one
    //  in the URI fragment; otherwise the sha is already bare.
    a_dup(u8c, sha, sha_frag);
    if (!u8csEmpty(sha) && sha[0][0] == '?') u8csUsed(sha, 1);
    a_dup(u8c, to, sha);

    //  `keeper put` is a local-move verb (user setting a ref).
    ok64 o = REFSAppendVerb($path(keepdir), REFSVerbPost(), from, to);
    if (o != OK) return o;

    fprintf(stdout, "keeper: %.*s → %.*s\n",
            (int)u8csLen(from), (char *)from[0],
            (int)u8csLen(to), (char *)to[0]);
    done;
}

// --- POST fast-forward check ---
//
//  https://replicated.wiki/html/wiki/POST.html §POST / Design invariant 9: POST is FF-only.  Default-
//  config bare git's receive-pack has `denyNonFastForwards=false`
//  and would silently accept a non-FF push, so keeper enforces on
//  the client side.  Two probes share `KEEPIsAncestor` (KEEP.c):
//    - cache-side, below in keeper_post — fast pre-flight against
//      the REFSResolve hit; skipped on cache miss.
//    - live-advert-side, inside WIREPush (WIRECLI.c) — runs after
//      the receive-pack advert, so an empty / stale cache no longer
//      lets a non-FF push slip through.
//  PUT (force-push) bypasses both — it's the unconstrained
//  ref-writer.

// --- Verb: post ---

//  YES iff posting to `remote_uri` would reach a GIT peer's
//  `git-receive-pack` — a git wire that rejects be-only synthetic
//  dot-coordinate refnames as "funny refs" (DIS-019).
//
//  Two cases, mirroring keeper/WIRECLI.c::wcli_spawn's dispatch:
//    1. A git-protocol transport scheme (ssh/https/http/git) —
//       `DOGIsGitTransport`.  (`be`/`keeper` speak the beagle
//       protocol; a be-only dot-branch is legitimate there.)
//    2. A local exec (`file://` or schemeless) whose path is a git
//       repo — `.git` suffix or on-disk `objects/`+`refs/` layout.
static b8 keep_post_is_git_wire(u8csc remote_uri) {
    uri u = {};
    a_dup(u8c, ru, remote_uri);
    if (DOGParseURI(&u, ru) != OK) return NO;
    if (DOGIsGitTransport(u.scheme)) return YES;
    //  Local exec: only file:// / schemeless land on a local path.
    a_cstr(file_s, "file");
    b8 local = u8csEmpty(u.scheme) || u8csEq(u.scheme, file_s);
    if (!local || u8csEmpty(u.path)) return NO;
    a_cstr(dotgit_s, ".git");
    a_dup(u8c, p, u.path);
    if (u8csHasSuffix(p, dotgit_s)) return YES;
    //  On-disk layout sniff (bare repo): <path>/objects + <path>/refs.
    a_cstr(objects_s, "objects");
    a_cstr(refs_s,    "refs");
    a_path(objp, p, objects_s);
    a_path(refp, p, refs_s);
    if (FILEisdir($path(objp)) == OK && FILEisdir($path(refp)) == OK)
        return YES;
    return NO;
}

//  Push the current worktree commit to a remote.  Nothing is staged
//  locally (sniff already committed if anything was).  Flow:
//    1. Determine target branch from URI query (`?main` / `?heads/X`)
//       or fall back to the worktree's current branch.
//    2. Build the transport URI from the URI's authority/scheme (with
//       alias resolution).
//    3. Check that the peer's cached tip is an ancestor of cur's tip
//       (POST is FF-only — https://replicated.wiki/html/wiki/POST.html §POST).  Cache is whatever the
//       last `be head ssh://...` (or transport-scheme POST/PATCH/GET)
//       populated in `<store>/.be/refs`.  Skipped when the peer ref
//       is unknown (fresh ref creation).
//    4. Hand off to WIREPush — it harvests our local tip via REFADV,
//       speaks the git wire protocol to the peer's receive-pack, and
//       on success the caller advances the cached peer-tip ref below.
//  No URI → this verb is a no-op (sniff already wrote the commit).
static ok64 keeper_post(keeper *k, cli *c) {
    sane(k && c);
    uri gv = {};
    uri *g = NULL;
    if (CLIUriLen(c) > 0) { (void)CLIUriAt(&gv, c, 0); g = &gv; }
    //  POST-008: a valid push target is anything `wcli_spawn` /
    //  `keeper_remote_uri` can route, NOT just a host-bearing ssh URI.
    //  The old `u8csEmpty(g->host)` gate rejected a host-less local
    //  keeper store (`file:///abs`, authority-empty) and the
    //  scheme-only completion forms — even though fetch already
    //  accepts them (mirrors `keeper_get`'s acceptance: authority OR a
    //  transport scheme).  Accept when ANY of these is present:
    //    - host          (ssh/be/keeper/https://host/...)
    //    - authority      (//host or //host:port; HOME-relative forms)
    //    - path           (file:///abs, //host/path, scheme path)
    //    - a `?/<project>` selector (be://host?/proj — empty path)
    //    - a transport scheme alone (file:/be:/ssh: — keeper_remote_uri
    //      completes host+path from the recentmost same-scheme REFS row)
    b8 has_proj_query = g && !u8csEmpty(g->query) && *g->query[0] == '/';
    b8 routable = g && (!u8csEmpty(g->host)   ||
                        !u8csEmpty(g->authority) ||
                        !u8csEmpty(g->path)   ||
                        has_proj_query        ||
                        (!u8csEmpty(g->scheme) && DOGIsTransport(g->scheme)));
    if (!routable) {
        fprintf(stderr, "keeper: post needs a remote URI "
                        "(ssh://host/path[?branch], be://host/path, "
                        "or file:///abs/path)\n");
        return KEEPFAIL;
    }
    a_path(keepdir);
    call(HOMEBranchDir, k->h, keepdir, NULL);

    //  1. Worktree's current branch + tip (used both as the WIREPush
    //     local_branch default and to record the new peer-side ref).
    //     Sourced from `--at <root>?<branch>#<sha>` forwarded by `be`
    //     and parked in `h->cur_branch` / `h->cur_sha` by HOMEOpen.
    //     Empty when `--at` was not forwarded (direct `keeper post`
    //     without sniff in the loop).
    if (u8bDataLen(k->h->cur_sha) != 40) {
        fprintf(stderr, "keeper: post: worktree commit not set\n");
        return KEEPFAIL;
    }
    a_dup(u8c, at_branch, u8bDataC(k->h->cur_branch));
    a_dup(u8c, at_sha,    u8bDataC(k->h->cur_sha));

    //  2. Target branch.  Precedence:
    //       a. explicit URI `?query`           — user said which branch.
    //       b. refs-log host-prefix match      — `be post //sniff` after
    //          a prior `be get ssh://sniff/...?feat` recovers `feat`
    //          from `<store>/refs` via REFSResolve.
    //       c. worktree current branch (h->cur_branch) — last-resort default.
    //     Branch is be-side and may be empty (= trunk).  WIREPush's
    //     wcli_be_to_wire applies the trunk⇔refs/heads/main alias.
    a_pad(u8, peer_arena, 1024);
    u8cs peer_refname = {};
    if (u8csEmpty(g->query) && !u8csEmpty(g->authority)) {
        uri resolved = {};
        a_dup(u8c, in_uri, g->data);
        if (REFSResolve(&resolved, peer_arena,
                        $path(keepdir), in_uri) == OK) {
            if (!u8csEmpty(resolved.fragment))
                u8csMv(peer_refname, resolved.fragment);
        }
    }
    a_pad(u8, branch_buf, 256);
    {
        u8cs src = {};
        if (!u8csEmpty(peer_refname)) {
            u8csMv(src, peer_refname);
        } else if (u8csEmpty(g->query)) {
            u8csMv(src, at_branch);
        } else {
            //  An absolute `?/<project>[/<branch>]` selector names a
            //  PROJECT, not a branch — the leading `/proj` segment is a
            //  shard selector consumed peer-side, not a wire ref.  Strip
            //  it (DOGQueryStripProject) so what's left is just the
            //  branch (empty = trunk); pushing the raw `/proj` would
            //  drive the peer's receive-pack to land objects on a bogus
            //  `refs/heads//proj` ref, so trunk never advances (POST-009).
            //  The relative `?<branch>` form has no leading `/` and is
            //  left intact.
            a_dup(u8c, q, g->query);
            DOGQueryStripProject(q);
            u8csMv(src, q);
        }
        if (!u8csEmpty(src)) u8bFeed(branch_buf, src);
    }
    a_dup(u8c, branch, u8bDataC(branch_buf));
    //  Empty branch = trunk; WIREPush handles it (wire alias to main).

    //  WIREPush takes the be-side branch directly — it walks REFADV
    //  and translates to refs/heads/<X> (or refs/heads/main for trunk)
    //  internally via wcli_be_to_wire.
    a_dup(u8c, local_branch, branch);

    //  3. Build the remote transport URI (substring-resolved origin).
    Bu8 rarena = {};
    call(u8bMap, rarena, (size_t)REFS_MAX_REFS * 320);
    a_pad(u8, ubuf, FILE_PATH_MAX_LEN);
    ok64 ru = keeper_remote_uri(k, g, ubuf, rarena);
    if (ru != OK) {
        u8bUnMap(rarena);
        return ru;
    }
    a_dup(u8c, remote_uri, u8bDataC(ubuf));

    //  DIS-019 / POST-013: a be-only synthetic dot-coordinate branch
    //  (`?/<sub>/.<parent>[/<br>]`, branch[0]=='.') is the normal cur of
    //  a mounted beagle submodule after a parent POST.  It has no git
    //  counterpart: a git peer's receive-pack rejects `refs/heads/.<…>`
    //  as a "funny ref" AFTER a full pack build.  The synthetic
    //  coordinate is meaningful ONLY to a be peer (beagle protocol),
    //  where the dot-branch is a real REFS tip — push it there as-is.
    //
    //  For a GIT wire the user nonetheless named a usable git target
    //  (`be post <git-remote>/abc`).  Rather than refuse (the old DIS-019
    //  behavior — friction: the user had to remember `?main`), default
    //  to the remote's OWN default branch (advertised symref HEAD, else
    //  main/master — resolved on the wire inside WIREPush).  An explicit
    //  `?branch` is NOT synthetic (branch[0] != '.') and pushes to that
    //  named ref exactly as before.
    b8 git_wire_default = NO;
    if (!$empty(branch) && branch[0][0] == '.' &&
        keep_post_is_git_wire(remote_uri)) {
        git_wire_default = YES;
        fprintf(stderr,
                "keeper: post: branch ?%.*s is a be-only synthetic "
                "coordinate; pushing to git remote %.*s default branch\n",
                (int)$len(branch), (char const *)branch[0],
                (int)$len(remote_uri), (char const *)remote_uri[0]);
    }

    //  4. Push.  WIREPush handles peer-tip advert + pack build + status.
    //  We pass at_sha (decoded from sniff's at-log) as the authoritative
    //  local_tip — keeper REFS may lag, and a stale REFADV would make
    //  WIREPush's peer==local short-circuit no-op a real push.
    sha1 at_tip = {};
    if (sha1FromHex(&at_tip, at_sha) != OK) {
        fprintf(stderr,
                "keeper: post: bad at_sha (%lld bytes)\n",
                (long long)$len(at_sha));
        u8bUnMap(rarena);
        return KEEPFAIL;
    }

    //  4a. POST is FF-only.  Look up the peer's cached tip and
    //  refuse if it's not an ancestor of at_tip.  PUT bypasses
    //  this (see https://replicated.wiki/html/wiki/PUT.html §PUT — non-FF on either namespace is
    //  allowed for PUT).  Skipped when the peer ref isn't cached
    //  (fresh ref creation, or user didn't `be head` first; the
    //  receive-pack will accept whatever we send).
    //
    //  REFSResolve matches host substring-only (not path), so it
    //  can return a sibling-project row when parent + sub share
    //  a hostname.  Filter additionally by path when the user
    //  supplied an explicit transport URI with a path.
    //  `--force` skips this cache-side FF check entirely (PUT's
    //  force-push path per https://replicated.wiki/html/wiki/PUT.html §PUT Design invariant 9).
    b8 force = CLIHas(c, "--force") ? YES : NO;
    //  Cache-side FF probe keys on the local (synthetic) branch; for a
    //  git-wire default push the wire target is the remote's OWN default
    //  branch, so the synthetic-keyed cache row is irrelevant — skip the
    //  cache probe and let WIREPush's live advert-side FF gate decide.
    if (!force && !git_wire_default) {
        a_pad(u8, ffarena, 1024);
        uri resolved = {};
        a_dup(u8c, in_uri, g->data);
        b8 path_mismatch = NO;
        if (REFSResolve(&resolved, ffarena,
                        $path(keepdir), in_uri) == OK &&
            DOGIsFullSha(resolved.query)) {
            //  Reject the match if the user supplied an explicit
            //  path that disagrees with what REFSResolve returned.
            if (!u8csEmpty(g->path) && !u8csEmpty(resolved.path) &&
                !u8csEq(g->path, resolved.path)) {
                path_mismatch = YES;
            }
            sha1 peer_tip = {};
            if (!path_mismatch &&
                sha1FromHex(&peer_tip, resolved.query) == OK &&
                !sha1Eq(&peer_tip, &at_tip) &&
                !KEEPIsAncestor(&at_tip, &peer_tip)) {
                fprintf(stderr,
                        "keeper: post: non-fast-forward — local tip "
                        "%.*s is not a descendant of peer tip "
                        "%.*s.  Use `be patch //%.*s?` + `be post` "
                        "to merge, or `be put` to force-push.\n",
                        (int)$len(at_sha), (char *)at_sha[0],
                        (int)u8csLen(resolved.query),
                        (char *)resolved.query[0],
                        (int)$len(g->host), (char *)g->host[0]);
                u8bUnMap(rarena);
                return KEEPFAIL;
            }
        }
    }

    ok64 pu = WIREPush(remote_uri, local_branch, &at_tip, force,
                       git_wire_default);
    u8bUnMap(rarena);
    if (pu != OK) return pu;

    //  5. Advance local `<peer-uri>?<branch> → <new-sha>` so
    //     subsequent fetches know the peer's tip.  Use the RESOLVED
    //     transport URI (host+path from alias resolution), not `g`:
    //     `be post //sniff` arrives with empty path, and recording a
    //     pathless `//sniff?<branch>` row would mask the original
    //     `ssh://sniff/src/dogs?<branch>` row in later REFSResolve
    //     lookups, breaking subsequent pushes.  Branch is be-side
    //     (empty for trunk → key ends in bare `?`).
    uri gk = {};
    {
        u8csMv(gk.data, remote_uri);
        (void)URILexer(&gk);
        u8csMv(gk.data, remote_uri);
    }
    //  A git-wire default push landed on the remote's default branch
    //  (its alias is be-side trunk), NOT on the local synthetic
    //  coordinate — record the cache row keyed on trunk (bare `?`) so a
    //  subsequent fetch resolves it, never on the synthetic dot-branch.
    if (u8csEmpty(branch) || git_wire_default) {
        //  Present-but-empty query so DOGCanonURIFeed emits the `?`.
        gk.query[0] = remote_uri[1];
        gk.query[1] = remote_uri[1];
    } else {
        u8csMv(gk.query, branch);
    }
    u8csMv0(gk.fragment);
    a_pad(u8, rkey, 1280);
    call(DOGCanonURIFeed, rkey, &gk);
    a_dup(u8c, remote_key, u8bDataC(rkey));
    //  `at_sha` is a slice (a_dup u8c *[2]), not a Bu8 — copy by
    //  slice, not by buffer, and read its length / head accordingly.
    a_dup(u8c, v, at_sha);
    //  Push is a local move (we updated the peer's tip), so record
    //  with verb `post`, not the back-compat `get` shim.
    call(REFSAppendVerb, $path(keepdir), REFSVerbPost(), remote_key, v);

    fprintf(stdout, "keeper: pushed %s%.*s → %.*s\n",
            (git_wire_default || $empty(branch)) ? "(default)" : "?",
            git_wire_default ? 0 : (int)$len(branch),
            git_wire_default ? (char *)"" : (char *)branch[0],
            (int)$len(at_sha), (char *)at_sha[0]);
    done;
}

// --- Verb: delete ---
//
//  Two arms keyed off the URI shape:
//    `//host?branch`       — push-delete: WIREPushDelete the remote
//                            ref then tombstone the cached row.
//    `//host` (no `?ref`)  — alias drop: walk REFS, tombstone every
//                            row whose authority is the named host.
//                            No network.
//
//  https://replicated.wiki/html/wiki/Verbs.html spec stores aliases in `<store>/ALIAS`; the current
//  keeper folds aliases into REFS rows keyed by host (REFS.h:16),
//  so dropping the alias is the same as tombstoning every row that
//  carries that authority.

#define KEEP_DEL_ZERO_HEX                                            \
    "0000000000000000000000000000000000000000"

typedef struct {
    u8cs  host;
    u8b  *keys;       // packed NUL-terminated row-keys to tombstone
    ok64  err;
} keeper_delete_alias_ctx;

static ok64 keeper_delete_alias_collect(refcp r, void *vctx) {
    sane(r && vctx);
    keeper_delete_alias_ctx *ctx = vctx;
    if (ctx->err != OK) done;

    //  r->key is the URI (minus fragment) re-emitted by URIutf8Feed;
    //  re-parse to extract its host slice.  Skip rows whose host
    //  isn't a byte-exact match against the requested host — alias
    //  drop is precise; substring matching is REFSResolve's job, not
    //  this verb's.
    uri ku = {};
    u8csMv(ku.data, r->key);
    if (URILexer(&ku) != OK) done;
    u8cs row_host = {ku.host[0], ku.host[1]};
    if (u8csEmpty(row_host)) done;
    if (u8csLen(row_host) != u8csLen(ctx->host)) done;
    if (memcmp(row_host[0], ctx->host[0],
               (size_t)u8csLen(ctx->host)) != 0) done;

    u8bFeed(*ctx->keys, r->key);
    u8bFeed1(*ctx->keys, '\0');
    done;
}

static ok64 keeper_delete_alias(keeper *k, u8cs host) {
    sane(k && !u8csEmpty(host));
    a_path(keepdir);
    call(HOMEBranchDir, k->h, keepdir, NULL);

    a_carve(u8, keys, 1UL << 16);

    keeper_delete_alias_ctx ctx = {.host = {host[0], host[1]},
                                   .keys = &keys, .err = OK};
    call(REFSEach, $path(keepdir),
                   keeper_delete_alias_collect, &ctx);
    if (ctx.err != OK) return ctx.err;

    if (!u8bHasData(keys)) {
        fprintf(stderr,
                "keeper: delete: no rows for //%.*s\n",
                (int)u8csLen(host), (char const *)host[0]);
        return KEEPNONE;
    }

    a_cstr(zeros, KEEP_DEL_ZERO_HEX);
    //  Canonical tombstone shape: one row per key under verb
    //  `delete`.  REFSLoad / REFSResolve dedup by URI key only
    //  (ULOGeachLatestKey), so a single `delete` row supersedes any
    //  earlier `get`/`post` row for the same key.
    u32 dropped = 0;
    u8cp p = u8bDataHead(keys);
    u8cp end = u8bIdleHead(keys);
    while (p < end) {
        u8cp q = p;
        while (q < end && *q != '\0') q++;
        u8cs row_key = {p, q};
        if (!u8csEmpty(row_key)) {
            call(REFSAppendVerb, $path(keepdir), REFSVerbDelete(),
                                  row_key, zeros);
            dropped++;
        }
        p = (q < end) ? q + 1 : end;
    }
    fprintf(stdout, "keeper: dropped alias //%.*s (%u row(s))\n",
            (int)u8csLen(host), (char const *)host[0], dropped);
    done;
}

static ok64 keeper_delete(keeper *k, cli *c) {
    sane(k && c);
    if (CLIUriLen(c) == 0) {
        fprintf(stderr, "keeper: delete requires a //host[?ref] URI\n");
        return KEEPFAIL;
    }
    uri gv = {};
    call(CLIUriAt, &gv, c, 0);
    uri *g = &gv;
    if (u8csEmpty(g->host)) {
        fprintf(stderr,
                "keeper: delete needs a remote URI (//host[?ref])\n");
        return KEEPFAIL;
    }

    //  Bare `//host` → alias drop.  No wire, no transport spawn.
    if (u8csEmpty(g->query)) {
        u8cs host = {g->host[0], g->host[1]};
        return keeper_delete_alias(k, host);
    }

    //  `//host?branch` → push-delete via receive-pack.  Resolve the
    //  alias to its full transport URI just like keeper_post does;
    //  delete-only commands are accepted without a packfile body.
    Bu8 rarena = {};
    call(u8bMap, rarena, (size_t)REFS_MAX_REFS * 320);
    a_pad(u8, ubuf, FILE_PATH_MAX_LEN);
    ok64 ru = keeper_remote_uri(k, g, ubuf, rarena);
    if (ru != OK) {
        u8bUnMap(rarena);
        return ru;
    }
    a_dup(u8c, remote_uri, u8bDataC(ubuf));

    a_dup(u8c, branch, g->query);
    ok64 pu = WIREPushDelete(remote_uri, branch);
    u8bUnMap(rarena);
    if (pu != OK) return pu;

    //  Tombstone the local cached `<peer-uri>?<branch>` row so future
    //  cached reads stop returning the now-deleted tip.  Mirrors the
    //  REFS-write at the end of keeper_post (KEEP.exe.c:620+).
    a_path(keepdir);
    call(HOMEBranchDir, k->h, keepdir, NULL);
    {
        a_pad(u8, kbuf, 1280);
        uri gk = {};
        a_dup(u8c, ru2, remote_uri);
        gk.data[0] = ru2[0];
        gk.data[1] = ru2[1];
        (void)URILexer(&gk);
        gk.data[0] = ru2[0];
        gk.data[1] = ru2[1];
        u8csMv(gk.query, branch);
        gk.fragment[0] = NULL;
        gk.fragment[1] = NULL;
        if (DOGCanonURIFeed(kbuf, &gk) == OK) {
            a_dup(u8c, key, u8bData(kbuf));
            a_cstr(zeros, KEEP_DEL_ZERO_HEX);
            //  Canonical tombstone: one row, verb=`delete`.  REFS's
            //  URI-key-only dedup ensures it masks any prior write.
            call(REFSAppendVerb, $path(keepdir), REFSVerbDelete(),
                 key, zeros);
        }
    }

    fprintf(stdout, "keeper: deleted //%.*s?%.*s\n",
            (int)u8csLen(g->host), (char const *)g->host[0],
            (int)u8csLen(branch), (char const *)branch[0]);
    done;
}

// --- Entry ---

ok64 KEEPExec(cli *c) {
    sane(c);
    keeper *k = &KEEP;

    a_cstr(v_help,   "help");
    a_cstr(v_get,    "get");
    a_cstr(v_put,    "put");
    a_cstr(v_post,   "post");
    a_cstr(v_delete, "delete");
    a_cstr(v_status, "status");
    a_cstr(v_import, "import");
    a_cstr(v_verify, "verify");
    a_cstr(v_refs,   "refs");
    a_cstr(v_tips,   "tips");

    if ($eq(c->verb, v_help) || CLIHas(c, "-h") || CLIHas(c, "--help")) {
        keep_usage(); done;
    }

    //  Verb-less projector invocation (https://replicated.wiki/html/wiki/Projector.html §"View projectors"):
    //  `keeper <proj>:<URI>` — no verb.  Scheme selects the projector;
    //  dog/DOG.c owns the scheme→dog table so we dispatch only when
    //  the URI's scheme resolves to this dog ("keeper").  `--tlv`
    //  switches the emitter from raw bytes to a HUNK TLV record so
    //  `bro` (started by BE on a TTY) can render it.
    if ($empty(c->verb) && CLIUriLen(c) > 0) {
        uri pu = {};
        (void)CLIUriAt(&pu, c, 0);
        char const *dog = DOGProjectorDog(pu.scheme);
        if (dog != NULL && strcmp(dog, "keeper") == 0) {
            b8 tlv = CLIHas(c, "--tlv");
            return KEEPProjDispatch(&pu, tlv);
        }
    }

    if ($empty(c->verb)) {
        keep_usage();
        fail(KEEPFAIL);
    }

    if ($eq(c->verb, v_status))  return keeper_status(k);
    if ($eq(c->verb, v_refs))    return keeper_refs(k);
    if ($eq(c->verb, v_tips))    return keeper_tips(k);

    a_cstr(v_subs, "subs");
    if ($eq(c->verb, v_subs))    return keeper_subs(k, c);

    //  `be://` and `file://` dispatch.  Phase 8: route through WIRE
    //  (git wire protocol) so client and server are symmetric across
    //  every transport.  The keeper-protocol case (`be://`, `keeper://`,
    //  `file://`) execs `keeper upload-pack` / `receive-pack` on the
    //  peer end via wcli_spawn.
    if (CLIUriLen(c) >= 1) {
        uri uv = {};
        (void)CLIUriAt(&uv, c, 0);
        uri *u = &uv;
        a_cstr(be_sch,     "be");
        a_cstr(file_sch,   "file");
        a_cstr(keeper_sch, "keeper");
        b8 plain = $eq(u->scheme, be_sch) || $eq(u->scheme, file_sch) ||
                   $eq(u->scheme, keeper_sch);
        if (plain && $eq(c->verb, v_get))  return KEEPGetRemote(u);
        if (plain && $eq(c->verb, v_post)) return keeper_post(k, c);
    }

    if ($eq(c->verb, v_get))     return keeper_get(k, c);
    if ($eq(c->verb, v_put))     return keeper_put(k, c);
    if ($eq(c->verb, v_post))    return keeper_post(k, c);
    if ($eq(c->verb, v_delete))  return keeper_delete(k, c);

    if ($eq(c->verb, v_import)) {
        if (CLIUriLen(c) < 1) {
            fprintf(stderr, "keeper: import requires a packfile path\n");
            return KEEPFAIL;
        }
        uri iv = {};
        call(CLIUriAt, &iv, c, 0);
        return keeper_import(k, iv.path);
    }

    if ($eq(c->verb, v_verify)) {
        uri vv = {};
        if (CLIUriLen(c) > 0) (void)CLIUriAt(&vv, c, 0);
        if (CLIUriLen(c) < 1 || u8csEmpty(vv.fragment)) {
            fprintf(stderr, "keeper: verify requires #sha\n");
            return KEEPFAIL;
        }
        return keeper_verify(k, vv.fragment);
    }

    a_cstr(v_lsfiles, "ls-files");
    if ($eq(c->verb, v_lsfiles)) {
        uri uv = {};
        if (CLIUriLen(c) > 0) {
            (void)CLIUriAt(&uv, c, 0);
        } else {
            //  Default: local HEAD.  Construct a minimal URI with query = "HEAD".
            a_cstr(head_q, "HEAD");
            uv.query[0] = head_q[0];
            uv.query[1] = head_q[1];
        }
        return keeper_lsfiles(k, &uv);
    }

    fprintf(stderr, "keeper: unknown verb '%.*s'\n",
            (int)$len(c->verb), (char *)c->verb[0]);
    return KEEPFAIL;
}
