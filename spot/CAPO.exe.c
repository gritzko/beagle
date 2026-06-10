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
#include "abc/RON.h"
#include "dog/CLI.h"
#include "dog/DOG.h"
#include "dog/HOME.h"
#include "dog/HUNK.h"
#include "dog/git/SHA1.h"
#include "dog/ULOG.h"
#include "keeper/KEEP.h"
#include "keeper/REFS.h"
#include "keeper/WALK.h"
#include "spot/CAPOi.h"
#include "spot/LESS.h"

//  Tip-walker counters live on the spot singleton (`SPOT.dbg_*`);
//  SPOTClose drains them under `SPOT.trace_order`.

// --- Verb / flag tables ---

char const *const SPOT_CLI_VERBS[] = {
    "get", "status", "help", NULL
};

//  Spot val-flags: -g -s -r -p -C --grep --spot --replace --pcre --context
//  `spot get URI` walks the URI's tip(s) over keeper's read APIs and
//  tokenises every leaf blob whose (blob, path) pair isn't already
//  in the BLOBFN memo (DOG.md §10a).
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

    //  Use the wt root (`h->wt`) for file scans, not the store root
    //  (`h->root`).  They coincide for a primary wt but diverge for a
    //  secondary wt where the user's files live under `h->wt` while
    //  refs/index/packs are stored under `h->root` (redirected via
    //  the secondary's `.be` anchor's row-0 `repo` URI).
    a_dup(u8c, reporoot, u8bDataC(dog->h->wt));

    u8cs v = {};

    if (CLIHas(c, "-h") || CLIHas(c, "--help")) {
        spot_usage(); done;
    }

    a_cstr(v_get, "get");
    a_cstr(v_status_verb, "status");
    a_cstr(v_help_verb, "help");

    if ($eq(c->verb, v_help_verb)) { spot_usage(); done; }
    if ($eq(c->verb, v_status_verb)) {
        u64css live = {};
        CAPORuns(live);
        u64 total = 0;
        $for(u64cs, run, live) total += (u64)$len(*run);
        fprintf(stderr, "spot: %u index files, %llu entries\n",
                (u32)$len(live), (unsigned long long)total);
        done;
    }
    //  `spot get URI` — invoked by `be` in parallel with keeper/graf/
    //  sniff after keeper finishes its own update (DOG.md §10a).
    //  Walks the URI's tip(s) over keeper's read APIs and tokenises
    //  every leaf blob whose (blob, path) pair isn't already in the
    //  BLOBFN memo.  Bare `spot get` (no URI) walks the worktree's
    //  current tip via `--at`'s fragment.
    if ($eq(c->verb, v_get)) {
        uri u0 = {};
        if (CLIUriLen(c) > 0) (void)CLIUriAt(&u0, c, 0);
        call(KEEPOpen, dog->h, NO);
        ok64 igr = SPOTIndexFromTips(&u0);
        KEEPClose();
        return igr;
    }

    b8 do_status = CLIHas(c, "--status");
    //  HUNKMode (set by CLISetHUNKMode in CAPO.cli.c) picks TLV /
    //  Color / Plain.  Honour the legacy `-t` alias for `--tlv` here
    //  because CLISetHUNKMode doesn't know about it.
    if (CLIHas(c, "-t")) HUNKMode = HUNKOutTLV;

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

    //  Projector dispatch (https://replicated.wiki/html/wiki/Projector.html §"View projectors"):
    //    be spot:#body[.ext]   structural search
    //    be grep:#body[.ext]   literal grep
    //    be regex:#body[.ext]  PCRE
    //  Scheme picks the backend; the URI fragment carries the search
    //  body (URI subresource semantics).  Path stays available for
    //  file/dir narrowing (e.g. `grep:src/#body`).  A trailing `.ext`
    //  on the fragment splits off as an extension filter
    //  (`spot:#'body'.c`).  Surrounding `'…'` quotes around the body
    //  are stripped so shell quoting doesn't leak in.
    //  First URI parsed once into a function-scope transient (URI-004):
    //  the projector block below MUTATES it (clears the consumed
    //  fragment / `.ext` path) and the trail loop reuses the SAME
    //  transient for ui==0 so those clears persist.
    uri uri0v = {};
    b8 have_uri0 = (CLIUriLen(c) > 0);
    if (have_uri0) (void)CLIUriAt(&uri0v, c, 0);

    u8cs proj_ext = {};
    b8 proj_search_uri0 = NO;
    if ($empty(c->verb) && have_uri0) {
        uri *pu = &uri0v;
        a_cstr(s_spot,  "spot");
        a_cstr(s_grep,  "grep");
        a_cstr(s_regex, "regex");
        b8 is_search_proj = $eq(pu->scheme, s_spot)
                         || $eq(pu->scheme, s_grep)
                         || $eq(pu->scheme, s_regex);
        if (is_search_proj) proj_search_uri0 = YES;
        if (is_search_proj && !$empty(pu->fragment)) {
            u8cs body = {pu->fragment[0], pu->fragment[1]};

            //  Path-side `.ext` (e.g. `spot:.c#u8sFeed`) — when the
            //  whole path is a known `.ext`, treat it as the
            //  extension filter and clear the path slot so it doesn't
            //  also get used as a file-narrowing constraint below.
            //  PATHu8sExt's "hidden file" rule rejects bare `.c`, so
            //  go through CAPOKnownExt directly.
            if (!$empty(pu->path) && *u8csHead(pu->path) == '.' &&
                CAPOKnownExt(pu->path)) {
                u8cs p = {};
                u8csMv(p, pu->path);
                $mv(proj_ext, p);
                pu->path[0] = pu->path[1] = NULL;
            }
            //  Strip a single pair of surrounding `'…'` from the body
            //  so shell quoting doesn't leak into the needle.
            if ($len(body) >= 2 && *u8csHead(body) == '\'' &&
                *u8csLast(body) == '\'') {
                u8csUsed1(body);
                u8csShed1(body);
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
    uri ref_uriv = {};
    uri const *ref_uri = NULL;   // first URI with a real `?ref` query
    for (u32 ui = 0; ui < CLIUriLen(c) && ntrail < 16; ui++) {
        //  For ui==0 reuse the projector-mutated transient so its
        //  consumed-fragment / `.ext`-path clears persist; otherwise
        //  parse the entry fresh (URI-004).
        uri uiv = {};
        uri *u;
        if (ui == 0) {
            u = &uri0v;
        } else {
            (void)CLIUriAt(&uiv, c, ui);
            u = &uiv;
        }
        //  Projector consumed the fragment.  Path stays for narrowing
        //  (e.g. `spot:/graf?feat#sym` ⇒ search `sym` under `/graf` on
        //  branch `feat`); the loop below picks it up via u->path.
        //  URILexer can classify a leading-dot arg like `.c` as the
        //  "query" component even without a `?`.  A real ref URI has an
        //  explicit `?` in its input text — require that for has_ref.
        b8 has_ref = NO;
        if (!u8csEmpty(u->query) && !u8csEmpty(u->data)) {
            a_dup(u8c, scan, u->data);
            $for(u8c, p, scan) {
                if (*p == '?') { has_ref = YES; break; }
            }
        }
        if (has_ref && ref_uri == NULL) { ref_uriv = *u; ref_uri = &ref_uriv; }
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

    b8 produces_hunks =
        (!$empty(grep_ndl) || !$empty(pcre_ndl) || !$empty(spot_ndl)) &&
        $empty(spot_rep);
    if (produces_hunks) {
        //  Output sink — stdout always; `be` wraps us in a bro pipe
        //  when it wants pagination.  SIGPIPE matters in TLV mode
        //  because a parent pipe (bro, BE→bro, user shell pipe) may
        //  close before we finish.
        spot_out_fd = STDOUT_FILENO;
        dog->out_fd = STDOUT_FILENO;
        if (HUNKMode == HUNKOutTLV) signal(SIGPIPE, SIG_IGN);
    }

    ok64 ret = OK;

    if (do_status) {
        u64css live = {};
        CAPORuns(live);
        u64 total = 0;
        $for(u64cs, run, live) total += (u64)$len(*run);
        fprintf(stderr, "spot: %u index files, %llu entries\n",
                (u32)$len(live), (unsigned long long)total);
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
    } else if (CLIUriLen(c) > 0) {
        //  A search projector with no body (e.g. `spot:`, `spot:#name`)
        //  is the most likely cause — body belongs in the URI path slot,
        //  not the fragment.  Catch that case with a targeted hint.
        //  Parse fresh (not uri0v, whose slots the projector block may
        //  have cleared) so the original path is reported.
        uri u0v = {};
        (void)CLIUriAt(&u0v, c, 0);
        uri *u0 = &u0v;
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

    spot_out_fd = -1;
    return ret;
}

// --- Tip-walk indexer (DOG.md §10a, called from `spot get URI`) ---
//
// Walks the URI's tip(s) over keeper's read APIs (KEEPLsFiles).  For
// each leaf blob with a tokenizable extension, looks up the BLOBFN
// memo (`off=blob_hl40, type=BLOBFN, id=path_h20`); a hit means we
// already indexed this exact (blob, path) pair on a prior walk —
// skip both tokenisation and emission.  Otherwise pull the blob via
// KEEPGetExact, tokenise it via CAPOIndexBlob with the path hash as
// the posting `id`, and write a fresh BLOBFN row so the next walk
// can short-circuit.
//
// Renames are caught by the path-hash key: same blob + new path =
// different path_h20 = no memo hit = re-tokenise under the new path.

#include "abc/RAP.h"

// Append `ext` to s->ext_arena if not already present; return its
// offset (>= 1).  Offset 0 is reserved as a sentinel "missing".
// Linear scan over an arena that holds ~50 distinct exts in practice.
static u32 capo_ext_intern(spot *s, u8cs ext) {
    if ($empty(ext)) return 0;
    a_dup(u8c, scan, u8bDataC(s->ext_arena));
    u8csUsed1(scan);                            // skip the sentinel NUL
    while (!$empty(scan)) {
        u8cs entry = {scan[0], scan[1]};
        if (u8csFind(scan, 0) == OK) entry[1] = scan[0];
        else                          scan[0] = scan[1];   // last unterminated
        if (u8csEq(entry, ext))
            return (u32)(entry[0] - u8bDataHead(s->ext_arena));
        if (!$empty(scan)) u8csUsed1(scan);     // step past the NUL
    }
    if (u8bIdleLen(s->ext_arena) < (size_t)$len(ext) + 1) return 0;
    u32 off = (u32)u8bDataLen(s->ext_arena);
    u8bFeed(s->ext_arena, ext);
    u8bFeed1(s->ext_arena, 0);
    return off;
}

//  YES iff a row `(off=blob_hl40, type=BLOBFN, id=path_h20)` already
//  exists in any open run.  Pure binary search on the natural u64
//  layout — no allocation.
static b8 spot_memo_hit(u64css runs, u64 blob_hl40, u32 path_h20) {
    u64 needle = wh64Pack(SPOT_BLOBFN, path_h20, blob_hl40);
    a_dup(u64cs, scan, runs);
    $for(u64cs, run, scan) {
        u64s view = {(u64p)(*run)[0], (u64p)(*run)[1]};
        if (u64sBinSearch(&needle, view) != NULL) return YES;
    }
    return NO;
}

//  One row in the post-filter TODO list (rows that actually need
//  tokenising — blobs whose (blob_h40, path_h20) pair isn't already
//  in the BLOBFN memo).  Stored in a flat `u8b` slab so the slab
//  itself is mmap-backed and walk-friendly.
typedef struct {
    sha1 sha;        //  blob's git sha-1 (20 bytes)
    u32  ext_off;    //  offset into SPOT.ext_arena
    u32  path_h20;   //  20-bit RAPHash of the full repo-relative path
    u32  path_off;   //  offset into ulog_buf where the path lives
    u32  path_len;
} spot_todo;

//  Build a probe URI that KEEPResolveTree can resolve to a commit
//  tree.  Mirrors the previous resolution policy: caller's URI as-
//  is, falling back to `--at`'s cur_sha, then REFSResolve on the
//  raw URI data, then `?` (trunk).  Result lands in `*out`; the
//  fragment-buffer caller owns must outlive `*out`.  Returns YES
//  when something resolved; NO when there's nothing to walk.
static b8 spot_probe_uri(keeper *k, uricp u, uri *out, u8b frag_buf) {
    *out = *u;

    //  Promote a 40-hex `?<sha>` query or `<sha>` path into the
    //  fragment slot so the resolution path takes the direct-sha
    //  branch — KEEPResolveTree's query-only path only resolves
    //  named refs / aliases.
    u8cs hex_src = {};
    if (u8csEmpty(out->fragment) && u8csLen(out->query) == 40) {
        $mv(hex_src, out->query);
    } else if (u8csEmpty(out->fragment) &&
               u8csEmpty(out->query) &&
               u8csLen(out->path) == 40) {
        $mv(hex_src, out->path);
    }
    if (!u8csEmpty(hex_src) && HEXu8sValid(hex_src)) {
        out->fragment[0] = hex_src[0];
        out->fragment[1] = hex_src[1];
        out->query[0] = out->query[1] = NULL;
        out->path[0]  = out->path[1]  = NULL;
    }

    b8 has_resolvable =
        !u8csEmpty(out->fragment) ||
        (!u8csEmpty(out->query) && u8csLen(out->query) > 0) ||
        (!u8csEmpty(out->path)   && !u8csEmpty(out->query));

    if (!has_resolvable && u8bDataLen(k->h->cur_sha) == 40) {
        a_dup(u8c, cs, u8bData(k->h->cur_sha));
        u8bFeed(frag_buf, cs);
        out->fragment[0] = u8bDataHead(frag_buf);
        out->fragment[1] = u8bIdleHead(frag_buf);
        out->query[0] = out->query[1] = NULL;
        out->data[0]  = out->data[1]  = NULL;
        has_resolvable = YES;
    }

    if (!has_resolvable) {
        a_path(keepdir);
        (void)HOMEBranchDir(k->h, keepdir, NULL);
        a_pad(u8, arena_buf, 1024);
        uri resolved = {};
        static u8c const q_lit[] = "?";
        u8cs probe_uri = {q_lit, q_lit + 1};
        if (!u8csEmpty(u->data)) {
            probe_uri[0] = u->data[0];
            probe_uri[1] = u->data[1];
        }
        a_dup(u8c, in_uri, probe_uri);
        if (REFSResolve(&resolved, arena_buf, $path(keepdir), in_uri) == OK
            && u8csLen(resolved.query) >= 40) {
            u8bFeed(frag_buf, resolved.query);
            out->fragment[0] = u8bDataHead(frag_buf);
            out->fragment[1] = u8bIdleHead(frag_buf);
            out->query[0] = out->query[1] = NULL;
            out->data[0]  = out->data[1]  = NULL;
            has_resolvable = YES;
        }
    }
    return has_resolvable;
}

//  Index one TODO row: KEEPGetExact the blob, look up its ext,
//  tokenise via CAPOIndexBlob, emit one BLOBFN posting.  Best-
//  effort per row — failures (oversized blob, wrong type, etc.)
//  are silently skipped; the next walk will re-evaluate.
static ok64 spot_index_one(keeper *k, spot_todo const *row,
                            u8cs path, Bu8 bbuf) {
    sane(k && row);
    spotp s = &SPOT;
    u8 btype = 0;
    u8bReset(bbuf);
    sha1 sha = row->sha;
    if (KEEPGetExact(&sha, bbuf, &btype) != OK) return OK;
    if (btype != DOG_OBJ_BLOB) return OK;
    u8cs source = {u8bDataHead(bbuf), u8bIdleHead(bbuf)};

    //  Resolve the ext slice from arena.
    u8cp ext_start = u8bDataHead(s->ext_arena) + row->ext_off;
    u8cp ext_idle  = u8bIdleHead(s->ext_arena);
    u8cp ext_end   = ext_start;
    while (ext_end < ext_idle && *ext_end != 0) ext_end++;
    u8cs ext = {ext_start, ext_end};

    (void)path;   // path is currently unused at index time; reserved
                  // for future per-path diagnostics.
    //  Per the function's best-effort contract (above), use try() so
    //  failures show up in trace without short-circuiting the walk.
    try(CAPOIndexBlob, source, ext, row->path_h20);
    SPOT.dbg_tokenised++;

    u64 blob_hl40 = WHIFFHashlet40(&sha);
    try(CAPOEmit, wh64Pack(SPOT_BLOBFN, row->path_h20, blob_hl40));
    return OK;
}

//  Index one slice of `todos[]` serially.  Single bbuf alloc/free,
//  same shape as the children's body.  Used by the no-fork path,
//  the fork-failed fallback, and the leaf-dir-resolution fallback.
static ok64 spot_index_slice_serial(spot_todo const *todos,
                                     size_t lo, size_t hi,
                                     u8bp ulog_buf) {
    sane(todos != NULL);
    if (lo >= hi) done;
    Bu8 bbuf = {};
    call(u8bMap, bbuf, 1UL << 28);
    keeper *k = &KEEP;
    for (size_t i = lo; i < hi; i++) {
        u8cs path = {};
        if (u8csSub(u8bDataC(ulog_buf), path, todos[i].path_off,
                    todos[i].path_off + todos[i].path_len) != OK) continue;
        (void)spot_index_one(k, &todos[i], path, bbuf);
    }
    u8bUnMap(bbuf);
    done;
}

//  Fork-worker body: switch into `<leafdir>/.wNNNN`, reset the
//  inherited pup stack, index `[lo, hi)`, collapse to a single pup,
//  exit.  `_exit` per ABC convention for forked child failure.
static void spot_index_worker_child(u32 w, spot_todo const *todos,
                                     size_t lo, size_t hi,
                                     u8bp ulog_buf) {
    spotp s = &SPOT;
    {
        a_pad(u8, wname, 16);
        a_cstr(prefix, ".w");
        if (u8bFeed(wname, prefix) != OK) _exit(1);
        if (RONu8sFeedPad(u8bIdle(wname), (ok64)w, 4) != OK) _exit(1);
        if (u8bFed(wname, 4) != OK) _exit(1);
        if (PATHu8bPush(s->leaf_branch, u8bDataC(wname)) != OK) _exit(1);
    }
    a_pad(u8, wleafdir, FILE_PATH_MAX_LEN);
    {
        a_dup(u8c, wl, u8bDataC(s->leaf_branch));
        if (spot_branch_dir(wleafdir, s->h, wl) != OK) _exit(1);
    }
    if (FILEMakeDirP($path(wleafdir)) != OK) _exit(1);

    //  Reset inherited pup stack — start fresh in the worker dir.
    Breset(s->puppies);
    CAPORefreshView();

    if (spot_index_slice_serial(todos, lo, hi, ulog_buf) != OK)
        _exit(1);
    if (CAPOFlushRun() != OK)    _exit(1);
    //  Collapse to a single pup so the parent merge stack is
    //  bounded by `nw`, not `nw * cascade`.
    if (CAPOCompactAll() != OK)  _exit(1);
    _exit(0);
}

ok64 SPOTIndexFromTips(uricp u) {
    sane(u);
    keeper *k = &KEEP;
    spotp s = &SPOT;
    if (!s->rw) done;

    //  Phase 0 — pick a URI keeper can resolve to a commit/tree.
    a_pad(u8, frag_buf, 64);
    uri probe = {};
    if (!spot_probe_uri(k, u, &probe, frag_buf)) done;

    sha1 tree_sha = {};
    if (KEEPResolveTree(&probe, &tree_sha) != OK) done;

    //  Phase 1 — get the tip's ULOG: one row per leaf, sorted by
    //  path, fragment carries the leaf's hex sha.  Single ~big
    //  buffer; cheap O(tree-size) keeper walk.
    Bu8 ulog_buf = {};
    if (u8bMap(ulog_buf, 1UL << 28) != OK) done;
    a_cstr(s_tgt, "tgt"); a_dup(u8c, dt, s_tgt);
    ron60 v_tgt = 0; (void)RONutf8sDrain(&v_tgt, dt);
    if (KEEPTreeULog(tree_sha.data, 0, v_tgt, ulog_buf) != OK) {
        u8bUnMap(ulog_buf);
        done;
    }

    //  Phase 2 — filter: walk rows, drop subs/no-ext, drop memo
    //  hits.  Build a flat `spot_todo[]` slab of rows that actually
    //  need indexing.  Memo lookups read `SPOT.runs` live (safe
    //  across in-walk mutations because `CAPOFlushRun` /
    //  `CAPOCompact` re-publish the view before returning).
    Bu8 todo_buf = {};
    if (u8bMap(todo_buf, 1UL << 28) != OK) {
        u8bUnMap(ulog_buf);
        done;
    }

    {
        a_dup(u8c, scan, u8bData(ulog_buf));
        while (!$empty(scan)) {
            ulogrec rec = {};
            if (ULOGu8sDrain(scan, &rec) != OK) break;
            //  Last RON64 letter of the verb encodes the kind:
            //  f=file, x=exec, l=symlink, s=submodule.
            u8 kletter = ok64Lit(rec.verb, 0);
            if (kletter != RON_f && kletter != RON_x &&
                kletter != RON_l) continue;

            u8cs path = {rec.uri.path[0], rec.uri.path[1]};
            if ($empty(path)) continue;

            u8cs ext = {};
            PATHu8sExt(ext, path);
            if ($empty(ext) || !CAPOKnownExt(ext)) {
                SPOT.dbg_blob_no_ext++;
                continue;
            }

            //  Decode the leaf sha from the row's fragment.
            u8cs hex = {rec.uri.fragment[0], rec.uri.fragment[1]};
            if (u8csLen(hex) != 40) continue;
            sha1 sha = {};
            u8s sb = {sha.data, sha.data + 20};
            if (HEXu8sDrainSome(sb, hex) != OK) continue;

            u64 blob_hl40 = WHIFFHashlet40(&sha);
            u32 path_h20  = CAPOFnRap20(path);

            u64css live_runs = {};
            CAPORuns(live_runs);
            if (spot_memo_hit(live_runs, blob_hl40, path_h20)) {
                SPOT.dbg_memo_hit++;
                continue;
            }

            u32 ext_off = capo_ext_intern(s, ext);
            if (ext_off == 0) continue;

            //  Stash the path slice as offset+length into ulog_buf
            //  (its bytes are alive for the rest of this function).
            u32 path_off = (u32)(path[0] - u8bDataHead(ulog_buf));
            u32 path_len = (u32)$len(path);

            spot_todo row = {
                .sha = sha,
                .ext_off = ext_off,
                .path_h20 = path_h20,
                .path_off = path_off,
                .path_len = path_len,
            };
            if (u8bIdleLen(todo_buf) < sizeof(row)) break;
            u8bFeed(todo_buf,
                    (u8cs){(u8cp)&row, (u8cp)&row + sizeof(row)});
        }
    }

    spot_todo *todos =
        (spot_todo *)(void *)u8bDataHead(todo_buf);
    size_t ntodo = u8bDataLen(todo_buf) / sizeof(spot_todo);

    //  Phase 3 — index the TODO rows.
    //
    //  Decide single-thread vs fork-workers based on size.  Fork()
    //  is the right model here: each child inherits keeper's mmaps
    //  plus a COW-private copy of `k->buf1..buf4`, so KEEPGetExact
    //  in the worker doesn't need keeper to be thread-safe.  Each
    //  child does its own filter/index slice, writes ONE pup run
    //  with a parent-assigned seqno, then exits.  Parent re-opens
    //  the leaf-branch dir post-join so `SPOT.puppies` /
    //  `SPOT.runs` pick up every worker's run, then compacts.
    //
    //  Threshold: at least 1000 rows per worker.  Below that, the
    //  fork+merge overhead dwarfs the parallel win.
    if (ntodo == 0) {
        u8bUnMap(todo_buf);
        u8bUnMap(ulog_buf);
        done;
    }

    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpu < 1) ncpu = 1;
    size_t nw_by_size = ntodo / 1000;
    if (nw_by_size > (size_t)ncpu) nw_by_size = (size_t)ncpu;
    if (nw_by_size < 1) nw_by_size = 1;
    char const *thr_env = getenv("SPOT_WORKERS");
    if (thr_env != NULL && *thr_env) {
        long t = atol(thr_env);
        if (t >= 1) nw_by_size = (size_t)t;
    }
    u32 nw = (u32)nw_by_size;

    if (nw == 1) {
        try(spot_index_slice_serial, todos, 0, ntodo, ulog_buf);
        u8bUnMap(todo_buf);
        u8bUnMap(ulog_buf);
        done;
    }

    //  Multi-worker fork path.  Parent flushes any in-flight BOX
    //  state first (children inherit a COW-clean slate that way),
    //  then forks `nw` children and waits.  Children re-walk their
    //  slice into their own BOX, BOXu64Flush at end, write a pup
    //  run with their pre-assigned seqno, exit.
    try(CAPOFlushRun);
    nedo { u8bUnMap(todo_buf); u8bUnMap(ulog_buf); done; }

    a_pad(u8, leafdir, FILE_PATH_MAX_LEN);
    {
        a_dup(u8c, leaf, u8bDataC(s->leaf_branch));
        if (spot_branch_dir(leafdir, s->h, leaf) != OK) {
            //  Can't compute leaf dir → fall back to serial.
            try(spot_index_slice_serial, todos, 0, ntodo, ulog_buf);
            u8bUnMap(todo_buf);
            u8bUnMap(ulog_buf);
            done;
        }
    }

    pid_t pids[64] = {};
    if (nw > 64) nw = 64;
    for (u32 w = 0; w < nw; w++) {
        size_t lo = (ntodo * w)     / nw;
        size_t hi = (ntodo * (w+1)) / nw;
        pid_t pid = fork();
        if (pid < 0) {
            //  Fork failed — fall back to inline serial for this slice
            //  in the parent.  Log a per-slice failure but continue —
            //  partial coverage beats aborting the whole walk.
            try(spot_index_slice_serial, todos, lo, hi, ulog_buf);
            __ = OK;
            continue;
        }
        if (pid == 0) {
            spot_index_worker_child(w, todos, lo, hi, ulog_buf);
            /* no return */
        }
        pids[w] = pid;
    }

    //  Wait for every child.  Their pup files now exist on disk;
    //  we need to fold them into our `SPOT.puppies` view.
    for (u32 w = 0; w < nw; w++) {
        if (pids[w] > 0) {
            int status = 0;
            (void)waitpid(pids[w], &status, 0);  //  zombie reap
        }
    }

    //  Merge every worker subdir's pup into a single new pup at the
    //  leaf level, drop the worker subdirs, then fold the merged run
    //  into the parent's pre-fork ladder via the usual compact.
    try(CAPOMergeWorkers, nw);
    nedo { u8bUnMap(todo_buf); u8bUnMap(ulog_buf); done; }
    try(CAPOCompact);
    nedo { u8bUnMap(todo_buf); u8bUnMap(ulog_buf); done; }

    u8bUnMap(todo_buf);
    u8bUnMap(ulog_buf);
    done;
}
