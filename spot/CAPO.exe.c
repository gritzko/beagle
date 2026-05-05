//  SPOTExec — run a parsed CLI against an open spot state.
//  Same effect as invoking `spot ...` as a separate process.
//
#include "CAPO.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "dog/CLI.h"
#include "dog/DOG.h"
#include "dog/FRAG.h"
#include "dog/HOME.h"
#include "dog/HUNK.h"
#include "dog/SHA1.h"
#include "keeper/GIT.h"
#include "keeper/KEEP.h"
#include "keeper/WALK.h"
#include "spot/CAPOi.h"
#include "spot/LESS.h"

// kv64 hashtable for hashlet32 → (off:32 | len:32)
#define X(M, name) M##kv64##name
#include "abc/HASHx.h"
#undef X

//  Per-session ingest stats — printed at SPOTClose so we can spot
//  pack-ordering / hash-table issues without per-object noise.
u64 SPOT_DBG_BLOB_HIT      = 0;
u64 SPOT_DBG_BLOB_MISS     = 0;
u64 SPOT_DBG_BLOB_NO_EXT   = 0;
u64 SPOT_DBG_TOKENISED     = 0;
//  Object-stream order stats — populated by SPOTUpdate, drained by
//  SPOTClose.  The miss → "orphan" terminology means: the BLOB arrived
//  before the TREE that names it, so the blob_to_fn lookup failed and
//  the blob's trigrams never made it into the index.  A high orphan
//  ratio indicates the pack producer is interleaving trees and blobs
//  (typical of git's delta-chain ordering); the search index will be
//  silently incomplete until the ingest path either two-passes or
//  buffers orphans.
u64 SPOT_DBG_TREES         = 0;  // total TREE objects seen
u64 SPOT_DBG_BLOBS         = 0;  // total BLOB objects seen
u64 SPOT_DBG_COMMITS       = 0;  // total COMMIT objects seen
u64 SPOT_DBG_TAGS          = 0;  // total TAG objects seen
u64 SPOT_DBG_BLOB_PRE_TREE = 0;  // BLOBs arriving before any TREE
u64 SPOT_DBG_RUN_BLOB_MAX  = 0;  // longest BLOB-only run between TREEs
u64 SPOT_DBG_RUN_BLOB_CUR  = 0;  // current BLOB-only run length

// --- Verb / flag tables ---

char const *const SPOT_CLI_VERBS[] = {
    "get", "status", "help", NULL
};

//  Spot val-flags: -g -s -r -p -C --grep --spot --replace --pcre --context
//  Pack-add indexing happens as keeper resolves objects (UNPKIndex's
//  emit hook → SPOTUpdate).  `spot get` is a no-op left in place so
//  that `be` can still invoke it unconditionally after a keeper fetch.
char const SPOT_CLI_VAL_FLAGS[] =
    "-g\0-s\0-r\0-p\0-C\0"
    "--grep\0--spot\0--replace\0--pcre\0--context\0--at\0";

// --- Helpers ---

static void spot_usage(void) {
    fprintf(stderr,
        "Usage: spot [--flags] [URI...]\n"
        "\n"
        "  spot status                        index stack summary\n"
        "  spot -s \"pattern\" .ext             code snippet search\n"
        "  spot -s \"pat\" -r \"repl\" .ext       code snippet search + replace\n"
        "  spot -g \"text\" [.ext]              grep (substring)\n"
        "  spot -p \"regex\" [.ext]             regex grep\n"
        "  spot '#pattern.ext'                URI-style search\n"
        "\n"
        "Patterns: single-letter placeholders (a-z match one token/group,\n"
        "A-Z match multiple tokens). Two spaces = skip gap.\n"
        "\n"
        "Indexing is driven by keeper (pack ingest) — no spot CLI flags.\n"
        "Diff/merge tools live in graf — see `graf --help`.\n"
    );
}

static b8 argIsExt(u8csc a) {
    if ($len(a) < 2 || a[0][0] != '.') return NO;
    return CAPOKnownExt(a);
}

// --- Entry ---

ok64 SPOTExec(cli *c) {
    sane(c);
    spotp dog = &SPOT;

    if (getenv("SPOT_COLOR")) { dog->color = YES; CAPO_COLOR = YES; }

    a_dup(u8c, reporoot, u8bDataC(dog->h->root));

    u8cs v = {};

    if (CLIHas(c, "-h") || CLIHas(c, "--help")) {
        spot_usage(); done;
    }

    a_cstr(v_get, "get");
    a_cstr(v_status_verb, "status");
    a_cstr(v_help_verb, "help");

    if ($eq(c->verb, v_help_verb)) { spot_usage(); done; }
    if ($eq(c->verb, v_status_verb)) {
        a_path(capodir);
        CAPOResolveDir(capodir, reporoot);
        a_dup(u8c, dirslice, u8bDataC(capodir));
        u64cs runs[CAPO_MAX_LEVELS] = {};
        u64css stack = {runs, runs};
        u8bp mmaps[CAPO_MAX_LEVELS] = {};
        u32 nidxfiles = 0;
        CAPOStackOpen(stack, mmaps, &nidxfiles, dirslice);
        u64 total = 0;
        for (u32 i = 0; i < nidxfiles; i++)
            total += (u64)$len(runs[i]);
        CAPOStackClose(mmaps, nidxfiles);
        fprintf(stderr, "spot: %u index files, %llu entries\n",
                nidxfiles, (unsigned long long)total);
        done;
    }
    //  `spot get` — invoked by `be` after `keeper get`/`sniff get`.
    //  Indexing is already done per-object via UNPKIndex's emit hook
    //  (keeper/KEEP.cli.c → SPOTUpdate); any pending scratch flushes
    //  on SPOTClose.  This verb is kept as an explicit no-op so the
    //  orchestrator's invocation pattern stays uniform across dogs.
    if ($eq(c->verb, v_get)) done;

    b8 do_status = CLIHas(c, "--status");
    b8 force_tlv = CLIHas(c, "-t") || CLIHas(c, "--tlv");

    u32 grep_ctx = 3;
    CLIFlag(v, c, "-C");
    if (!$empty(v)) grep_ctx = (u32)atoi((char *)v[0]);
    CLIFlag(v, c, "--context");
    if (!$empty(v)) grep_ctx = (u32)atoi((char *)v[0]);

    u8cs spot_ndl = {}, spot_rep = {}, grep_ndl = {}, pcre_ndl = {};
    CLIFlag(v, c, "-s");
    if (!$empty(v)) { $mv(spot_ndl, v); }
    CLIFlag(v, c, "--spot");
    if (!$empty(v)) { $mv(spot_ndl, v); }
    CLIFlag(v, c, "-r");
    if (!$empty(v)) { $mv(spot_rep, v); }
    CLIFlag(v, c, "--replace");
    if (!$empty(v)) { $mv(spot_rep, v); }
    CLIFlag(v, c, "-g");
    if (!$empty(v)) { $mv(grep_ndl, v); }
    CLIFlag(v, c, "--grep");
    if (!$empty(v)) { $mv(grep_ndl, v); }
    CLIFlag(v, c, "-p");
    if (!$empty(v)) { $mv(pcre_ndl, v); }
    CLIFlag(v, c, "--pcre");
    if (!$empty(v)) { $mv(pcre_ndl, v); }

    //  Projector dispatch (VERBS.md §"View projectors"):
    //    be spot:#body[.ext]   structural search
    //    be grep:#body[.ext]   literal grep
    //    be regex:#body[.ext]  PCRE
    //  Scheme picks the backend; the URI fragment carries the search
    //  body (URI subresource semantics).  Path stays available for
    //  file/dir narrowing (e.g. `grep:src/#body`).  A trailing `.ext`
    //  on the fragment splits off as an extension filter
    //  (`spot:#'body'.c`).  Surrounding `'…'` quotes around the body
    //  are stripped so shell quoting doesn't leak in.
    u8cs proj_ext = {};
    b8 proj_search_uri0 = NO;
    if ($empty(c->verb) && c->nuris > 0) {
        uri *pu = &c->uris[0];
        a_cstr(s_spot,  "spot");
        a_cstr(s_grep,  "grep");
        a_cstr(s_regex, "regex");
        b8 is_search_proj = $eq(pu->scheme, s_spot)
                         || $eq(pu->scheme, s_grep)
                         || $eq(pu->scheme, s_regex);
        if (is_search_proj) proj_search_uri0 = YES;
        if (is_search_proj && !$empty(pu->fragment)) {
            u8cs body = {pu->fragment[0], pu->fragment[1]};
            //  Trailing `.ext` (e.g. `'body'.c`) — split body off ext.
            //  Scan from the end for a `.` followed by ext-legal chars.
            u8cp dot = NULL;
            for (u8cp p = body[1]; p > body[0]; ) {
                p--;
                u8 ch = *p;
                if (ch == '.') { dot = p; break; }
                if (!((ch >= 'a' && ch <= 'z') ||
                      (ch >= 'A' && ch <= 'Z') ||
                      (ch >= '0' && ch <= '9'))) break;
            }
            if (dot != NULL && dot > body[0] && dot < body[1] - 1) {
                proj_ext[0] = dot;
                proj_ext[1] = body[1];
                body[1]     = dot;
            }
            //  Strip a single pair of surrounding `'…'` from the body.
            if ($len(body) >= 2 && body[0][0] == '\'' &&
                body[1][-1] == '\'') {
                body[0]++; body[1]--;
            }
            if ($eq(pu->scheme, s_spot)) {
                $mv(spot_ndl, body);
            } else if ($eq(pu->scheme, s_grep)) {
                $mv(grep_ndl, body);
            } else {
                $mv(pcre_ndl, body);
            }
            //  Fragment consumed; clear so the trailing-args loop
            //  doesn't reprocess it.  Path is left intact — it stays
            //  available as a file/dir narrowing constraint.
            pu->fragment[0] = pu->fragment[1] = NULL;
        }
    }

    u8cs trail[16] = {};
    int ntrail = 0;
    if (!$empty(proj_ext)) { $mv(trail[ntrail], proj_ext); ntrail++; }
    uri const *ref_uri = NULL;   // first URI with a real `?ref` query
    for (u32 ui = 0; ui < c->nuris && ntrail < 16; ui++) {
        uri *u = &c->uris[ui];
        //  Projector consumed the fragment.  Path stays for narrowing
        //  (e.g. `spot:/graf?feat#sym` ⇒ search `sym` under `/graf` on
        //  branch `feat`); the loop below picks it up via u->path.
        //  URILexer can classify a leading-dot arg like `.c` as the
        //  "query" component even without a `?`.  A real ref URI has an
        //  explicit `?` in its input text — require that for has_ref.
        //  URILexer can classify a leading-dot arg like `.c` as the
        //  "query" component even without a `?`.  A real ref URI has an
        //  explicit `?` in its input text — require that for has_ref.
        b8 has_ref = NO;
        if (!u8csEmpty(u->query) && !u8csEmpty(u->data)) {
            for (u8cp p = u->data[0]; p < u->data[1]; p++) {
                if (*p == '?') { has_ref = YES; break; }
            }
        }
        if (has_ref && ref_uri == NULL) ref_uri = u;
        //  Path is a file/dir narrowing constraint.  Skip it only when
        //  the URI carries an authority (`//host/repo?ref`) where the
        //  path is the *remote* repo location, not a local subtree.
        b8 remote = !$empty(u->authority);
        //  Search-projector URIs already had their fragment consumed
        //  above; don't let their full data string ("grep:#body") leak
        //  into trail[] as a bogus file filter.  Path stays valid.
        b8 is_search_uri = (ui == 0 && proj_search_uri0);
        if (!remote) {
            u8cs dat = {};
            if (!$empty(u->path)) {
                $mv(dat, u->path);
            } else if (!has_ref && !is_search_uri && !$empty(u->data)) {
                //  No structured slot set — fall back to raw arg
                //  (e.g. a bare `.c` ext token written without `?`/`#`).
                $mv(dat, u->data);
            }
            if (!$empty(dat) && ntrail < 16) {
                $mv(trail[ntrail], dat);
                ntrail++;
            }
        }
    }

    if (!$empty(spot_rep) && $empty(spot_ndl)) {
        fprintf(stderr, "spot: --replace requires --spot\n");
        return FAILSANITY;
    }

    pid_t bro_pid = -1;
    b8 produces_hunks =
        (!$empty(grep_ndl) || !$empty(pcre_ndl) || !$empty(spot_ndl)) &&
        $empty(spot_rep);
    if (produces_hunks) {
        if (force_tlv) {
            spot_out_fd = STDOUT_FILENO;
            spot_emit   = HUNKu8sFeed;
            signal(SIGPIPE, SIG_IGN);
        } else if (c->tty_out) {
            a_path(bropath);
            a$rg(a0, 0);
            a_cstr(bro_name, "bro");
            HOMEResolveSibling(NULL, bropath, bro_name, a0);
            u8cs bargs[] = {u8slit("bro")};
            u8css bargv = {bargs, bargs + 1};
            int wfd = -1;
            call(FILESpawn, $path(bropath), bargv, &wfd, NULL, &bro_pid);
            dog->out_fd = wfd;
            dog->emit   = HUNKu8sFeed;
            spot_out_fd = dog->out_fd;
            spot_emit   = dog->emit;
            signal(SIGPIPE, SIG_IGN);
        } else {
            dog->out_fd = STDOUT_FILENO;
            dog->emit   = HUNKu8sFeedText;
            spot_out_fd = dog->out_fd;
            spot_emit   = dog->emit;
        }
    }

    ok64 ret = OK;

    if (do_status) {
        a_path(capodir);
        vcall("resolve_dir", CAPOResolveDir, capodir, reporoot);
        a_dup(u8c, dirslice, u8bDataC(capodir));
        u64cs runs[CAPO_MAX_LEVELS] = {};
        u64css stack = {runs, runs};
        u8bp mmaps[CAPO_MAX_LEVELS] = {};
        u32 nidxfiles = 0;
        vcall("stack_open", CAPOStackOpen, stack, mmaps, &nidxfiles, dirslice);
        u64 total = 0;
        for (u32 i = 0; i < nidxfiles; i++)
            total += (u64)$len(runs[i]);
        CAPOStackClose(mmaps, nidxfiles);
        fprintf(stderr, "spot: %u index files, %llu entries\n",
                nidxfiles, (unsigned long long)total);
    } else if (!$empty(grep_ndl)) {
        u8cs ext = {};
        u8cs gfiles[16] = {};
        int gnf = 0;
        for (int i = 0; i < ntrail; i++) {
            if (argIsExt(trail[i])) {
                $mv(ext, trail[i]);
            } else if (gnf < 16) {
                $mv(gfiles[gnf], trail[i]);
                gnf++;
            }
        }
        if ($empty(ext) && gnf > 0) {
            u8cs pe = {};
            PATHu8sExt(pe, gfiles[0]);
            if (!$empty(pe)) {
                ext[0] = pe[0] - 1;
                ext[1] = pe[1];
            }
        }
        a_dup(u8c, ndl, grep_ndl);
        u8css gf = {gfiles, gfiles + gnf};
        ret = CAPOGrep(ndl, ext, reporoot, grep_ctx, gf, ref_uri);
    } else if (!$empty(pcre_ndl)) {
        u8cs ext = {};
        u8cs gfiles[16] = {};
        int gnf = 0;
        for (int i = 0; i < ntrail; i++) {
            if (argIsExt(trail[i])) {
                $mv(ext, trail[i]);
            } else if (gnf < 16) {
                $mv(gfiles[gnf], trail[i]);
                gnf++;
            }
        }
        if ($empty(ext) && gnf > 0) {
            u8cs pe = {};
            PATHu8sExt(pe, gfiles[0]);
            if (!$empty(pe)) {
                ext[0] = pe[0] - 1;
                ext[1] = pe[1];
            }
        }
        a_dup(u8c, ndl, pcre_ndl);
        u8css gf = {gfiles, gfiles + gnf};
        ret = CAPOPcreGrep(ndl, ext, reporoot, grep_ctx, gf, ref_uri);
    } else if (!$empty(spot_ndl)) {
        u8cs ext = {};
        u8cs sfiles[16] = {};
        int snf = 0;
        for (int i = 0; i < ntrail; i++) {
            if (argIsExt(trail[i])) {
                $mv(ext, trail[i]);
            } else if (snf < 16) {
                $mv(sfiles[snf], trail[i]);
                snf++;
            }
        }
        if ($empty(ext) && snf > 0) {
            u8cs pe = {};
            PATHu8sExt(pe, sfiles[0]);
            if (!$empty(pe)) {
                ext[0] = pe[0] - 1;
                ext[1] = pe[1];
            }
        }
        if ($empty(ext)) {
            fprintf(stderr, "spot: --spot requires a .ext argument\n");
            ret = FAILSANITY;
        } else {
            a_dup(u8c, ndl, spot_ndl);
            a_dup(u8c, rep, spot_rep);
            u8css sf = {sfiles, sfiles + snf};
            ret = CAPOSpot(ndl, rep, ext, reporoot, sf, ref_uri);
        }
    } else if (c->nuris > 0) {
        //  A search projector with no body (e.g. `spot:`, `spot:#name`)
        //  is the most likely cause — body belongs in the URI path slot,
        //  not the fragment.  Catch that case with a targeted hint.
        uri *u0 = &c->uris[0];
        a_cstr(s_spot,  "spot");
        a_cstr(s_grep,  "grep");
        a_cstr(s_regex, "regex");
        b8 is_search = $eq(u0->scheme, s_spot)
                    || $eq(u0->scheme, s_grep)
                    || $eq(u0->scheme, s_regex);
        if (is_search) {
            //  Body lives in the fragment slot (`spot:#body`); a search
            //  URI with only a path (`spot:body`) means the user put the
            //  body in the wrong slot — point them at the right shape.
            if (!$empty(u0->path)) {
                u8cs path = {u0->path[0], u0->path[1]};
                if (!$empty(path) && *path[0] == '/') u8csFed(path, 1);
                fprintf(stderr,
                    "spot: search body goes in the URI fragment, not "
                    "the path\n  try: %.*s:#%.*s\n",
                    (int)$len(u0->scheme), (char *)u0->scheme[0],
                    (int)$len(path), (char *)path[0]);
            } else {
                fprintf(stderr,
                    "spot: %.*s: needs a search body\n  try: "
                    "%.*s:#<body>\n",
                    (int)$len(u0->scheme), (char *)u0->scheme[0],
                    (int)$len(u0->scheme), (char *)u0->scheme[0]);
            }
        } else {
            fprintf(stderr, "spot: file display moved to bro\n");
        }
        ret = FAILSANITY;
    } else {
        spot_usage();
    }

    // Cleanup bro pipe (globals)
    if (spot_out_fd >= 0 && spot_out_fd != STDOUT_FILENO) {
        close(spot_out_fd);
        spot_out_fd = -1;
    }
    if (bro_pid > 0) {
        int rc = 0;
        FILEReap(bro_pid, &rc);
        if (rc == 127)
            fprintf(stderr, "spot: bro pager not found\n");
    }

    return ret;
}

// --- Update: index a single git object during pack ingest ---
//
// Driven by keeper's UNPK emit hook.  Pack producers (git, sniff)
// emit trees before blobs, so each TREE stamps its own children
// directly into `blob_to_fn` and BLOB lookups hit without buffering.
// COMMIT objects are ignored.

#define SPOT_FN_VAL_PACK(fn20, ext_off) \
    (((u64)((fn20) & WHIFF_ID_MASK) << 24) | \
     ((u64)(ext_off) & 0xFFFFFF))
#define SPOT_FN_VAL_HASH(v) ((u32)(((v) >> 24) & WHIFF_ID_MASK))
#define SPOT_FN_VAL_EOFF(v) ((u32)((v) & 0xFFFFFF))

// Append `ext` to s->ext_arena if not already present; return its
// offset (>= 1).  Offset 0 is reserved as a sentinel "missing".
// Linear scan over an arena that holds ~50 distinct exts in practice.
static u32 capo_ext_intern(spot *s, u8cs ext) {
    if ($empty(ext)) return 0;
    u8cp base = u8bDataHead(s->ext_arena);
    u8cp idle = u8bIdleHead(s->ext_arena);
    size_t want = (size_t)$len(ext);
    for (u8cp p = base + 1; p < idle; ) {
        u8cp end = p;
        while (end < idle && *end != 0) end++;
        if ((size_t)(end - p) == want &&
            memcmp(p, ext[0], want) == 0)
            return (u32)(p - base);
        p = end + 1;
    }
    if (u8bIdleLen(s->ext_arena) < want + 1) return 0;
    u32 off = (u32)u8bDataLen(s->ext_arena);
    u8bFeed(s->ext_arena, ext);
    u8bFeed1(s->ext_arena, 0);
    return off;
}

ok64 SPOTUpdate(u8 obj_type, sha1 const *sha, u8cs blob) {
    sane(1);
    spotp s = &SPOT;
    if (!s->rw || !sha) done;
    if (BNULL(s->blob_to_fn)) done;

    //  Per-type counters and BLOB-run tracking — always on (cheap;
    //  drained at SPOTClose).  Per-object trace lines stay opt-in
    //  behind SPOT_TRACE_ORDER to avoid log volume on big packs.
    switch (obj_type) {
        case DOG_OBJ_COMMIT: SPOT_DBG_COMMITS++; break;
        case DOG_OBJ_TAG:    SPOT_DBG_TAGS++;    break;
        case DOG_OBJ_TREE:
            SPOT_DBG_TREES++;
            if (SPOT_DBG_RUN_BLOB_CUR > SPOT_DBG_RUN_BLOB_MAX)
                SPOT_DBG_RUN_BLOB_MAX = SPOT_DBG_RUN_BLOB_CUR;
            SPOT_DBG_RUN_BLOB_CUR = 0;
            break;
        case DOG_OBJ_BLOB:
            SPOT_DBG_BLOBS++;
            SPOT_DBG_RUN_BLOB_CUR++;
            if (SPOT_DBG_TREES == 0) SPOT_DBG_BLOB_PRE_TREE++;
            break;
        default: break;
    }

    if (getenv("SPOT_TRACE_ORDER")) {
        static const char H[] = "0123456789abcdef";
        char ord_hex[15];
        for (int i = 0; i < 7; i++) {
            ord_hex[i*2]   = H[(sha->data[i] >> 4) & 0xf];
            ord_hex[i*2+1] = H[ sha->data[i]       & 0xf];
        }
        ord_hex[14] = 0;
        char const *tname = (obj_type == DOG_OBJ_COMMIT) ? "C" :
                            (obj_type == DOG_OBJ_TREE)   ? "T" :
                            (obj_type == DOG_OBJ_BLOB)   ? "B" :
                            (obj_type == DOG_OBJ_TAG)    ? "G" : "?";
        fprintf(stderr, "ORD %s %s\n", tname, ord_hex);
    }

    if (obj_type == DOG_OBJ_TREE) {
        //  For each tree entry whose basename has a known tokenizer
        //  ext, stamp blob_to_fn[child_hl] = (fn_rap40 << 24) | ext_off.
        //  Subtrees (mode 040000) and untokenizable blobs are ignored.
        kv64s tbl = {s->blob_to_fn[0], s->blob_to_fn[3]};
        a_dup(u8c, scan, blob);
        u8cs file = {}, esha = {};
        u32  mode = 0;
        while (GITu8sDrainTree(scan, file, esha, &mode) == OK) {
            if (mode != 0100644 && mode != 0100755 && mode != 0120000)
                continue;
            u8cs fscan = {file[0], file[1]};
            if (u8csFind(fscan, ' ') != OK) continue;
            u8cs name = {fscan[0] + 1, file[1]};
            if ($empty(name) || u8csLen(esha) != 20) continue;

            u8cs ext = {};
            PATHu8sExt(ext, name);
            if ($empty(ext) || !CAPOKnownExt(ext)) continue;
            u32 ext_off = capo_ext_intern(s, ext);
            if (ext_off == 0) continue;

            sha1 csha = {};
            memcpy(csha.data, esha[0], 20);
            u64 child_hl = CAPOObjHashlet(&csha);
            u32 fn_hash20 = CAPOFnRap20(name);

            //  Transient map for in-pack BLOB lookups (single-value;
            //  last basename within a pack wins).  Used by BLOB
            //  ingest to recover the ext for tokenization.
            kv64 e = {.key = child_hl,
                      .val = SPOT_FN_VAL_PACK(fn_hash20, ext_off)};
            (void)HASHkv64Put(tbl, &e);

            //  Persisted blob → fn_hash mapping.  Multiple basenames
            //  for the same blob produce multiple BLOBFN rows; search
            //  range-scans `(off=blob_hl40, type=BLOBFN)` to recover
            //  every bucket the blob has lived in (rename-safe).
            u64 blob_hl40 = WHIFFHashlet40(&csha);
            u64 blobfn = wh64Pack(SPOT_BLOBFN, fn_hash20, blob_hl40);
            (void)CAPOEmit(blobfn);
        }
        done;
    }

    if (obj_type != DOG_OBJ_BLOB) done;

    //  BLOB: look up (fn_rap, ext_off); miss = no tokenizable basename
    //  in any tree we saw → silent skip.
    u64 blob_hl = CAPOObjHashlet(sha);
    kv64s tbl = {s->blob_to_fn[0], s->blob_to_fn[3]};
    kv64 probe = {.key = blob_hl, .val = 0};
    if (HASHkv64Get(&probe, tbl) != OK) {
        SPOT_DBG_BLOB_MISS++;
        done;
    }
    SPOT_DBG_BLOB_HIT++;

    u32 fn_hash20 = SPOT_FN_VAL_HASH(probe.val);
    u32 ext_off   = SPOT_FN_VAL_EOFF(probe.val);
    if (ext_off == 0 || ext_off >= u8bDataLen(s->ext_arena)) {
        SPOT_DBG_BLOB_NO_EXT++;
        done;
    }

    u8cp ext_start = u8bDataHead(s->ext_arena) + ext_off;
    u8cp ext_idle  = u8bIdleHead(s->ext_arena);
    u8cp ext_end   = ext_start;
    while (ext_end < ext_idle && *ext_end != 0) ext_end++;
    u8cs ext = {ext_start, ext_end};

    (void)CAPOIndexBlob(blob, ext, fn_hash20);
    SPOT_DBG_TOKENISED++;

    //  Hash-set drain is owned by CAPOEmit (it flushes on HASHNOROOM
    //  and retries) — no per-blob threshold check needed here.

    done;
}
