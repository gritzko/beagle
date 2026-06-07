//  GRAFExec — run a parsed CLI against an open graf state.
//  Same effect as invoking `graf ...` as a separate process.
//
#include "GRAF.h"
#include "DAG.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "dog/CLI.h"
#include "dog/DOG.h"
#include "dog/HOME.h"
#include "dog/HUNK.h"
#include "dog/WHIFF.h"
#include "dog/git/GIT.h"
#include "keeper/KEEP.h"
#include "keeper/RESOLVE.h"

// --- Verb / flag tables ---

char const *const GRAF_CLI_VERBS[] = {
    "get", "diff", "merge", "blame", "weave", "index",
    "log", "map", "head", "status", "help", NULL
};

char const GRAF_CLI_VAL_FLAGS[] = "-o\0--at\0";

// --- Usage ---

static void graf_usage(void) {
    fprintf(stderr,
        "Usage: graf <verb> [flags] [URI...]\n"
        "\n"
        "  Verbs:\n"
        "    get path?sha1&sha2[&...]     deterministic blob/tree merge\n"
        "    diff:[path][?from][#to]      token-level colored diff (URI)\n"
        "    merge base ours theirs       3-way merge\n"
        "    blame file                   token-level blame\n"
        "    weave file?from..to          weave diff between refs\n"
        "    log [path]?ref[#N]           commit history (one per line)\n"
        "    head '#<msg-substring>'      find cur-reachable commit by msg\n"
        "    index                        index object graph from keeper\n"
        "    status                       show index stats\n"
        "    help                         this message\n"
        "\n"
        "  Flags:\n"
        "    -o <file>                    merge output file\n"
        "    -t | --tlv                   force TLV output\n"
    );
}

// --- Output setup ---
//
//  graf writes hunks to stdout exclusively.  `HUNKMode` (resolved by
//  `CLISetHUNKMode` in GRAF.cli.c — `--tlv` / `--color` / `--plain` /
//  ANSIIsTTY() default) picks TLV / Color / Plain shape; `be` is the
//  only thing that forks bro for pagination.  Direct-invocation
//  pagination (`graf log` on a bare TTY) is the user's responsibility
//  (`graf log | bro` or `graf log --tlv | bro`).

// --- First-parent lookup for diff:?<sha> commit-show ---
//
//  Resolves `query` (a sha or hashlet hex) to a commit and writes the
//  40-hex sha of its first parent into `*out_parent`.  Returns OK on
//  success, GRAFNONE on a root commit (no parent), or the underlying
//  resolve / fetch error.  Body-parse path mirrors the LOG.c pattern
//  so it works even when the DAG isn't indexed.
static ok64 graf_first_parent_hex(u8cs query, sha1hex *out_parent) {
    sane(out_parent);
    sha1hex commit_hex = {};
    call(KEEPResolveHex, &commit_hex, query);
    sha1 commit_sha = {};
    call(sha1FromSha1hex, &commit_sha, &commit_hex);
    u64 commit_h60 = WHIFFHashlet60(&commit_sha);

    a_carve(u8, cbuf, 1UL << 20);
    u8 ot = 0;
    ok64 go = KEEPGet(commit_h60, DAG_H60_HEXLEN, cbuf, &ot);
    if (go != OK || ot != DOG_OBJ_COMMIT)
        return (go != OK) ? go : KEEPNONE;
    a_dup(u8c, scan, u8bDataC(cbuf));
    u8cs field = {}, value = {};
    ok64 ret = GRAFNONE;
    while (GITu8sDrainCommit(scan, field, value) == OK) {
        if (u8csEmpty(field)) break;
        a_cstr(par_kw, "parent");
        if (u8csEq(field, par_kw) && u8csLen(value) >= 40) {
            if (sha1hexFromHex(out_parent, value) == OK) ret = OK;
            break;
        }
    }
    return ret;
}

// --- `graf get <path>?<sha>[&...]` helper: deterministic blob/tree fetch.
//
//  Split out so the 16MB output buffer can ride on BASS via `a_carve`:
//  on a_carve's allocation failure (or any downstream call() failure),
//  this helper returns early with BASS rewound; keeper itself is opened
//  and closed by GRAFOpenBranch / GRAFClose, not here.
static ok64 graf_get_op(uri *u) {
    sane(u);
    a_carve(u8, out, 16UL << 20);
    a_dup(u8c, uri_in, u->data);
    call(GRAFGet, out, uri_in);
    a_dup(u8c, obytes, u8bData(out));
    call(FILEFeedAll, STDOUT_FILENO, obytes);
    done;
}

// --- URI path helper ---

static void graf_uri_path(u8cs out, uri *u) {
    if (!u8csEmpty(u->path))
        u8csMv(out, u->path);
    else
        u8csMv(out, u->data);
}

// --- Entry ---

ok64 GRAFExec(cli *c) {
    sane(c);
    graf *g = &GRAF;

    a_cstr(v_get,    "get");
    a_cstr(v_diff,   "diff");
    a_cstr(v_merge,  "merge");
    a_cstr(v_blame,  "blame");
    a_cstr(v_weave,  "weave");
    a_cstr(v_index,  "index");
    a_cstr(v_log,    "log");
    a_cstr(v_map,    "map");
    a_cstr(v_head,   "head");
    a_cstr(v_status, "status");
    a_cstr(v_help,   "help");

    if ($eq(c->verb, v_help) || CLIHas(c, "-h") || CLIHas(c, "--help")) {
        graf_usage(); done;
    }

    //  Verb-less projector invocation (https://replicated.wiki/html/wiki/Projector.html §"View projectors"):
    //  `graf <proj>:<URI>` — no verb.  The URI's scheme must resolve
    //  through DOG_PROJECTORS to "graf"; today that's only `diff:`.
    //  We synthesize the matching verb so the existing dispatch below
    //  runs unchanged.  BE wires this up by spawning `graf [--tlv] <URI>`
    //  on `be get diff:<URI>` (and verb-less `be diff:<URI>`).
    a_cstr(s_diff,  "diff");
    a_cstr(s_log,   "log");
    a_cstr(s_map,   "map");
    a_cstr(s_blame, "blame");
    a_cstr(s_weave, "weave");
    if ($empty(c->verb) && uribDataLen(c->uris) > 0) {
        uri *pu = uribAtP(c->uris, 0);
        char const *dog = DOGProjectorDog(pu->scheme);
        if (dog != NULL && strcmp(dog, "graf") == 0) {
            if      ($eq(pu->scheme, s_diff))  u8csMv(c->verb, s_diff);
            else if ($eq(pu->scheme, s_log))   u8csMv(c->verb, s_log);
            else if ($eq(pu->scheme, s_map))   u8csMv(c->verb, s_map);
            else if ($eq(pu->scheme, s_blame)) u8csMv(c->verb, s_blame);
            else if ($eq(pu->scheme, s_weave)) u8csMv(c->verb, s_weave);
        }
    }

    if ($empty(c->verb)) {
        graf_usage();
        return FAILSANITY;
    }

    //  HUNKMode is set by CLISetHUNKMode in GRAF.cli.c (--tlv / --color
    //  / --plain / auto-from-TTY).  Honour the legacy `-t` alias for
    //  `--tlv` here since CLISetHUNKMode doesn't know about it.
    if (CLIHas(c, "-t")) HUNKMode = HUNKOutTLV;

    //  Output sink — stdout always; `be` wraps us in a bro pipe when
    //  it wants pagination.  SIGPIPE handling matters in TLV mode
    //  because a parent pipe (bro, BE→bro, user shell pipe) may close
    //  before we finish; let the write fail and graf_emit's EPIPE
    //  guard tear down cleanly.
    graf_out_fd = STDOUT_FILENO;
    if (HUNKMode == HUNKOutTLV) signal(SIGPIPE, SIG_IGN);

    u8cs reporoot = {};
    if (u8bHasData(c->repo)) u8csMv(reporoot, $path(c->repo));
    // If CLI parsing didn't supply a repo, fall back to h->root.
    if ($empty(reporoot) && g->h && g->h->root[0]) {
        a_dup(u8c, hs, u8bDataC(g->h->root));
        u8csMv(reporoot, hs);
    }

    // --- status: uses graf state only ---

    if ($eq(c->verb, v_status)) {
        u64 total_entries = 0;
        for (u32 i = 0; i < g->runs_n; i++)
            total_entries += (u64)(g->runs[i][1] - g->runs[i][0]);
        a_pad(u8, body, 96);
        a_cstr(pfx, "graf: ");
        (void)u8sFeed(body_idle, pfx);
        (void)utf8sFeed10(body_idle, (u64)g->runs_n);
        a_cstr(mid, " index run(s), ");
        (void)u8sFeed(body_idle, mid);
        (void)utf8sFeed10(body_idle, total_entries);
        a_cstr(sfx, " entries\n");
        (void)u8sFeed(body_idle, sfx);

        call(GRAFArenaInit);
        a_cstr(status_uri, "graf:status");
        hunk hk = {};
        u8csMv(hk.uri,  status_uri);
        u8csMv(hk.text, u8bDataC(body));
        (void)GRAFHunkEmit(&hk, NULL);
        done;
    }

    // --- diff: requires a projector URI; resolved against keeper below ---

    if ($eq(c->verb, v_diff) && uribDataLen(c->uris) < 1) {
        fprintf(stderr, "graf: diff requires a URI (diff:[<path>][?<ref>])\n");
        return FAILSANITY;
    }

    // --- merge: file-based, no keeper needed ---

    if ($eq(c->verb, v_merge)) {
        if (uribDataLen(c->uris) < 3) {
            fprintf(stderr, "graf: merge requires 3 files: base ours theirs\n");
            return FAILSANITY;
        }
        u8cs bp = {}, op = {}, tp = {};
        graf_uri_path(bp, uribAtP(c->uris, 0));
        graf_uri_path(op, uribAtP(c->uris, 1));
        graf_uri_path(tp, uribAtP(c->uris, 2));
        u8cs merge_out = {};
        CLIFlag(merge_out, c, "-o");
        return GRAFMerge(bp, op, tp, merge_out);
    }

    // --- index, blame, weave: require keeper ---

    if (!reporoot[0]) {
        fprintf(stderr, "graf: %.*s requires a keeper store under .be/\n",
                (int)$len(c->verb), (char *)c->verb[0]);
        return FAILSANITY;
    }


    //  Keeper is already open RO (GRAFOpenBranch did it, at the top of
    //  the call chain).  GRAFExec is a pure read: it resolves the URI's
    //  `?ref` against the flat pool's REFS and never opens/closes keeper
    //  itself — the open-once store serves every branch.
    ok64 ret = OK;

    if ($eq(c->verb, v_index)) {
        //  Pull every keeper object through graf's DOG.md §8 streaming
        //  ingest: COMMIT → TREE → BLOB → finish.  Idempotent.
        ret = GRAFIndex();

    } else if ($eq(c->verb, v_get)) {
        //  Two shapes share the verb:
        //    * `path?sha[&sha...]` → deterministic blob/tree fetch
        //      or merge (GRAFGet) — single-tip identity is allowed
        //      per graf/GET.md.
        //    * any URI without that path+query pair (or none) →
        //      tip-walk indexer.  Under DOG.md §10a, `be get URI`
        //      spawns `graf get URI` in parallel with keeper/spot/
        //      sniff; graf walks back from the URI's tip(s) until
        //      it hits commits already in its own DAG.
        //
        //  Distinguish on the path slot: GRAFGet's URIs always carry
        //  a blob/tree path, while `be get`'s URIs (`?ref`, `?sha`,
        //  `//host/path?ref`, bare) target the repo as a whole.
        b8 is_get_op = NO;
        if (uribDataLen(c->uris) >= 1) {
            uri *gu = uribAtP(c->uris, 0);
            if (!u8csEmpty(gu->path) && !u8csEmpty(gu->query) &&
                u8csEmpty(gu->authority))
                is_get_op = YES;
        }
        if (is_get_op) {
            uri *u = uribAtP(c->uris, 0);
            try(graf_get_op, u);
            ret = __;
        } else {
            //  Tip-walk indexer.  Bare `graf get` (no URI) walks the
            //  worktree's current tip (via `--at` parked in cur_sha).
            uri empty = {};
            uri *u = (uribDataLen(c->uris) >= 1) ? uribAtP(c->uris, 0) : &empty;
            ret = GRAFIndexFromTips(u);
        }

    } else if ($eq(c->verb, v_blame)) {
        if (uribDataLen(c->uris) < 1) {
            fprintf(stderr, "graf: blame requires a file URI\n");
            return FAILSANITY;
        }
        u8cs path = {};
        graf_uri_path(path, uribAtP(c->uris, 0));
        //  Resolve URI's #hex/?ref/absent-query to a tip commit
        //  hashlet so blame can scope its history walk.  A failure
        //  here just yields an unscoped (full-index) blame.
        u64 tip_h = 0;
        {
            sha1 tip = {};
            if (GRAFResolveTip(uribAtP(c->uris, 0), &tip) == OK)
                tip_h = WHIFFHashlet60(&tip);
        }
        ret = GRAFBlame(path, tip_h, reporoot);

    } else if ($eq(c->verb, v_diff)) {
        //  URI-driven diff (https://replicated.wiki/html/wiki/Projector.html §"View projectors", `diff:`).  The
        //  right-hand side of every diff is *ours* (the changed state).
        //  URI shape table:
        //
        //    diff:                  → wt vs base    (whole tree)
        //    diff:file.c            → wt vs base    (single file)
        //    diff:?branch           → branch vs base (whole tree, ref-to-ref)
        //    diff:file.c?branch     → branch vs base (single file, ref-to-ref)
        //    diff:?from#to          → from vs to    (whole tree, explicit)
        //    diff:file.c?from#to    → from vs to    (single file, explicit)
        //
        //  The base sha comes from `--at`'s fragment (the worktree's
        //  current baseline, forwarded by `be`).  Every form except the
        //  explicit `?from#to` range needs it; missing → `GRAFNOAT`.
        uri *u = uribAtP(c->uris, 0);

        uri at = {};
        CLIAtURI(&at, c);
        u8cs base_hex = {};
        $mv(base_hex, at.fragment);

        u8cs path = {};
        u8csMv(path, u->path);

        u8cs wf = {}, wt = {};
        b8 has_range = !$empty(u->query) && !$empty(u->fragment);

        if (has_range) {
            //  Explicit `?from#to` — no baseline needed.
            u8csMv(wf, u->query);
            u8csMv(wt, u->fragment);
            //  Load both side branches' packs into keeper/graf
            //  so the per-ref tree+blob fetches downstream resolve.
            //  When from/to look like branch names (non-hex), switch
            //  through each so its idx/pack pups land in PAST or DATA.
            //  `wf` / `wt` may name a tag (no shard dir under <store>);
            //  KEEPSwitchBranch / GRAFSwitchBranch return KEEPNONE /
            //  GRAFNOPATH in that case, but the REFS rows under cur's
            //  shard still resolve the tag.  Treat the missing-dir
            //  arms as "stay on cur" and propagate other failures.
            //  URI-001 §"one rule": branch-FIRST.  Switch for any
            //  non-hex ref (as before) and also for a hex-NAMED branch
            //  (`GRAFRefIsName`), which `DOGIsHashlet` alone would
            //  wrongly skip.  A bare sha still skips the switch.
            if (!DOGIsHashlet(wf) || GRAFRefIsName(wf) == OK) {
                try(KEEPSwitchBranch, KEEP.h, wf);
                on(KEEPNONE) __ = OK;
                nedo { return __; }
                try(GRAFSwitchBranch, KEEP.h, wf);
                on(GRAFNOPATH) __ = OK;
                nedo { return __; }
            }
            if (!DOGIsHashlet(wt) || GRAFRefIsName(wt) == OK) {
                try(KEEPSwitchBranch, KEEP.h, wt);
                on(KEEPNONE) __ = OK;
                nedo { return __; }
                try(GRAFSwitchBranch, KEEP.h, wt);
                on(GRAFNOPATH) __ = OK;
                nedo { return __; }
            }
            if (!$empty(path)) {
                ret = GRAFWeaveDiff(path, reporoot, wf, wt);
            } else {
                ret = GRAFDiffTreeRefs(wf, wt, reporoot);
            }
        } else {
            if ($empty(base_hex)) {
                fprintf(stderr,
                    "graf: diff: no --at baseline; need explicit"
                    " 'diff:?<from>#<to>' or a sniff anchor\n");
                return GRAFNOAT;
            }
            sha1 base_sha = {};
            a_dup(u8c, base_dup, base_hex);
            u8s sb = {(u8p)base_sha.data, (u8p)base_sha.data + 20};
            ok64 ho = HEXu8sDrainSome(sb, base_dup);
            u64 base_h40 = (ho == OK) ? WHIFFHashlet60(&base_sha) : 0;

            if (!u8csEmpty(u->query)) {
                //  `?branch` → branch vs base (ref-to-ref).
                u8cs branch = {};
                u8csMv(branch, u->query);
                //  diff:?<hashlet> (no path) → commit-show:
                //  diff this commit against its first parent (the
                //  `git show <sha>` shape).  This is the click-
                //  through target the log/map/blame projections emit
                //  via U-tokens.  Falls through to branch-vs-base
                //  when the parent lookup fails (root commit, unindexed
                //  commit, or `branch` isn't actually a hashlet).
                //  URI-001 §"one rule": branch-FIRST — a ref whose name
                //  is all-hex but exists as a REFS name is a branch, not
                //  a commit-show hashlet; route it to branch-vs-base.
                b8 handled = NO;
                if ($empty(path) && DOGIsHashlet(branch) &&
                    GRAFRefIsName(branch) != OK) {
                    sha1hex parent_hex = {};
                    if (graf_first_parent_hex(branch, &parent_hex) == OK) {
                        u8cs parent_s = {};
                        sha1hexSlice(parent_s, &parent_hex);
                        ret = GRAFDiffTreeRefs(parent_s, branch,
                                               reporoot);
                        handled = YES;
                    }
                }
                if (handled) {
                    /* nothing */
                } else if (!$empty(path)) {
                    ret = GRAFWeaveDiff(path, reporoot,
                                        branch, base_hex);
                } else {
                    ret = GRAFDiffTreeRefs(branch, base_hex,
                                           reporoot);
                }
            } else {
                //  No query → wt vs base (weave-based).
                if (!$empty(path)) {
                    ret = GRAFDiffWtFile(path, base_h40, reporoot);
                } else {
                    ret = GRAFDiffWtTree(base_h40, base_hex,
                                         reporoot);
                }
            }
        }

    } else if ($eq(c->verb, v_map)) {
        ret = GRAFMap(uribDataLen(c->uris) > 0 ? uribAtP(c->uris, 0) : NULL);

    } else if ($eq(c->verb, v_log)) {
        if (uribDataLen(c->uris) < 1) {
            fprintf(stderr, "graf: log requires a URI\n");
            return FAILSANITY;
        }
        ret = GRAFLog(uribAtP(c->uris, 0));

    } else if ($eq(c->verb, v_head)) {
        //  No URI → ahead/behind cur vs implicit target.  With a URI:
        //  fragment-only does message search; `?branch` does explicit
        //  ahead/behind.  GRAFHead dispatches internally.
        uri empty = {};
        uri *hu = (uribDataLen(c->uris) >= 1) ? uribAtP(c->uris, 0) : &empty;
        ret = GRAFHead(hu);

    } else if ($eq(c->verb, v_weave)) {
        if (uribDataLen(c->uris) < 1) {
            fprintf(stderr, "graf: weave requires a file URI\n");
            return FAILSANITY;
        }
        uri *u = uribAtP(c->uris, 0);
        u8cs wf = {}, wt = {};
        if (!u8csEmpty(u->query)) {
            a_dup(u8c, q, u->query);
            a_cstr(dots, "..");
            if (u8csFindS(q, dots) == OK) {
                wf[0] = u->query[0];
                wf[1] = q[0];
                wt[0] = q[0] + 2;
                wt[1] = u->query[1];
            } else {
                u8csMv(wt, u->query);
            }
        }
        u8cs path = {};
        graf_uri_path(path, u);
        ret = GRAFWeaveDiff(path, reporoot, wf, wt);

    } else {
        fprintf(stderr, "graf: unknown verb '%.*s'\n",
                (int)$len(c->verb), (char *)c->verb[0]);
        ret = FAILSANITY;
    }

    return ret;
}
