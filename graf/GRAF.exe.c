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
        "    diff:[path][?from..to][#Ln]  token-level colored diff (URI)\n"
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

// --- Query range split (`?<from>..<to>`) ---
//
//  Shared by the `weave` and `diff:` projectors: split a `..`-bearing
//  query into `from` (before `..`) and `to` (after).  Returns YES only
//  when BOTH halves are non-empty — so a bare relative `?..` (parent
//  branch, URI.mkd:42) or `?./fix` (child) is NOT mis-split into an
//  empty from..to range (DIFF-004).  `wf`/`wt` slice into `u->query`'s
//  backing bytes; the caller must keep `u` alive.
static b8 graf_query_range(uri *u, u8cs wf, u8cs wt) {
    if (u8csEmpty(u->query)) return NO;
    a_dup(u8c, q, u->query);
    a_cstr(dots, "..");
    if (u8csFindS(q, dots) != OK) return NO;
    //  Compute into locals; commit to wf/wt only on a real range so a
    //  bare relative `?..` (empty halves) leaves the caller's slices
    //  untouched and signals NO.  `from` is the prefix the find
    //  consumed; `to` is what's left after eating the `..`.
    a_past(u8c, from, u->query, q);
    u8csUsed(q, 2);
    if (u8csEmpty(from) || u8csEmpty(q)) return NO;
    u8csMv(wf, from);
    u8csMv(wt, q);
    return YES;
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
    if ($empty(c->verb) && CLIUriLen(c) > 0) {
        uri pu = {};
        (void)CLIUriAt(&pu, c, 0);
        char const *dog = DOGProjectorDog(pu.scheme);
        if (dog != NULL && strcmp(dog, "graf") == 0) {
            if      ($eq(pu.scheme, s_diff))  u8csMv(c->verb, s_diff);
            else if ($eq(pu.scheme, s_log))   u8csMv(c->verb, s_log);
            else if ($eq(pu.scheme, s_map))   u8csMv(c->verb, s_map);
            else if ($eq(pu.scheme, s_blame)) u8csMv(c->verb, s_blame);
            else if ($eq(pu.scheme, s_weave)) u8csMv(c->verb, s_weave);
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
    // If CLI parsing didn't supply a repo, fall back to HOME.root.
    if ($empty(reporoot) && !BNULL(HOME.root)) {
        a_dup(u8c, hs, u8bDataC(HOME.root));
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

    if ($eq(c->verb, v_diff) && CLIUriLen(c) < 1) {
        fprintf(stderr, "graf: diff requires a URI (diff:[<path>][?<ref>])\n");
        return FAILSANITY;
    }

    // --- merge: file-based, no keeper needed ---

    if ($eq(c->verb, v_merge)) {
        if (CLIUriLen(c) < 3) {
            fprintf(stderr, "graf: merge requires 3 files: base ours theirs\n");
            return FAILSANITY;
        }
        uri mb = {}, mo = {}, mt = {};
        (void)CLIUriAt(&mb, c, 0);
        (void)CLIUriAt(&mo, c, 1);
        (void)CLIUriAt(&mt, c, 2);
        u8cs bp = {}, op = {}, tp = {};
        graf_uri_path(bp, &mb);
        graf_uri_path(op, &mo);
        graf_uri_path(tp, &mt);
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
        uri gu = {};
        if (CLIUriLen(c) >= 1) (void)CLIUriAt(&gu, c, 0);
        b8 is_get_op = NO;
        if (CLIUriLen(c) >= 1) {
            if (!u8csEmpty(gu.path) && !u8csEmpty(gu.query) &&
                u8csEmpty(gu.authority))
                is_get_op = YES;
        }
        if (is_get_op) {
            try(graf_get_op, &gu);
            ret = __;
        } else {
            //  Tip-walk indexer.  Bare `graf get` (no URI) walks the
            //  worktree's current tip (via `--at` parked in cur_sha).
            ret = GRAFIndexFromTips(&gu);
        }

    } else if ($eq(c->verb, v_blame)) {
        if (CLIUriLen(c) < 1) {
            fprintf(stderr, "graf: blame requires a file URI\n");
            return FAILSANITY;
        }
        uri bu = {};
        (void)CLIUriAt(&bu, c, 0);
        u8cs path = {};
        graf_uri_path(path, &bu);
        //  Resolve URI's #hex/?ref/absent-query to a tip commit
        //  hashlet so blame can scope its history walk.  A failure
        //  here just yields an unscoped (full-index) blame.
        u64 tip_h = 0;
        {
            sha1 tip = {};
            if (GRAFResolveTip(&bu, &tip) == OK)
                tip_h = WHIFFHashlet60(&tip);
        }
        ret = GRAFBlame(path, tip_h, reporoot);

    } else if ($eq(c->verb, v_diff)) {
        //  URI-driven diff (https://replicated.wiki/html/wiki/Projector.html §"View projectors", `diff:`).  The
        //  right-hand side of every diff is *ours* (the changed state).
        //  URI shape table:
        //
        //    diff:                    → wt vs base    (whole tree)
        //    diff:file.c              → wt vs base    (single file)
        //    diff:?branch             → branch vs base (whole tree, ref-to-ref)
        //    diff:file.c?branch       → branch vs base (single file, ref-to-ref)
        //    diff:?from..to           → from vs to    (whole tree, range)
        //    diff:file.c?from..to#Ln  → from vs to    (single file, range; #L = jump)
        //    diff:?from#to            → from vs to    (legacy fallback, no #L)
        //
        //  DIFF-004: `?from..to` (both refs in the query) is the canonical
        //  range form, freeing the fragment for the `#L<n>` line anchor —
        //  the shape diff hunks emit as click targets.  Legacy `?from#to`
        //  (fragment = range `to`) is kept as a fallback when the query
        //  has no `..`.  A relative `?..` / `?./x` ref is NOT a range
        //  (`graf_query_range` guards on non-empty halves).
        //
        //  The base sha comes from `--at`'s fragment (the worktree's
        //  current baseline, forwarded by `be`).  Every form except an
        //  explicit range needs it; missing → `GRAFNOAT`.
        uri uv = {};
        (void)CLIUriAt(&uv, c, 0);
        uri *u = &uv;

        uri at = {};
        CLIAtURI(&at, c);
        u8cs base_hex = {};
        $mv(base_hex, at.fragment);

        u8cs path = {};
        u8csMv(path, u->path);

        //  DIFF-004: the canonical range form is `?<from>..<to>` (both
        //  refs in the query), leaving the fragment free as the `#L<n>`
        //  line anchor — this is the shape diff hunks emit as click
        //  targets.  `navver` is the verbatim `<from>..<to>` query text
        //  spliced into each per-hunk nav URI.  Legacy `?<from>#<to>`
        //  (fragment = range `to`) stays a fallback ONLY when the query
        //  has no `..` (no line anchor possible in that form).
        u8cs wf = {}, wt = {}, navver = {};
        b8 has_dotrange = graf_query_range(u, wf, wt);
        b8 has_range = has_dotrange;
        if (has_dotrange) {
            u8csMv(navver, u->query);
        } else if (!$empty(u->query) && !$empty(u->fragment)) {
            has_range = YES;
            u8csMv(wf, u->query);
            u8csMv(wt, u->fragment);
        }

        if (has_range) {
            //  `?from..to` (canonical) or legacy `?from#to` — no
            //  baseline needed.
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
                try(KEEPSwitchBranch, wf);
                on(KEEPNONE) __ = OK;
                nedo { return __; }
                try(GRAFSwitchBranch, wf);
                on(GRAFNOPATH) __ = OK;
                nedo { return __; }
            }
            if (!DOGIsHashlet(wt) || GRAFRefIsName(wt) == OK) {
                try(KEEPSwitchBranch, wt);
                on(KEEPNONE) __ = OK;
                nedo { return __; }
                try(GRAFSwitchBranch, wt);
                on(GRAFNOPATH) __ = OK;
                nedo { return __; }
            }
            if (!$empty(path)) {
                //  DIFF-003: file-scope → whole-file view.  DIFF-004:
                //  for the canonical `..` form, `navver` re-encodes the
                //  range so the hunk click target points back at this
                //  same file-scope range diff; the legacy `#to` form has
                //  no navver (its fragment was the range `to`, not `#L`).
                ret = GRAFWeaveDiff(path, reporoot, wf, wt, YES, navver);
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
            ok64 ho = sha1FromHex(&base_sha, base_dup);
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
                    //  DIFF-003: file-scope → whole-file view.
                    //  DIFF-004: nav URI carries the `<branch>..<base>`
                    //  range so the hunk click target re-opens this
                    //  same file-scope range diff — but ONLY for a
                    //  concrete ref.  A relative ref (`..` parent branch,
                    //  `./x` child, URI.mkd:42-43) is left with an empty
                    //  navver so the click target stays the bare `#L`
                    //  form instead of a nonsensical `?....<base>` range.
                    u8cs bnav = {};
                    b8 rel = !$empty(branch) && branch[0][0] == '.';
                    if (!rel) {
                        a_lign(u8, bnav_g);
                        (void)u8gFeed(bnav_g, branch);
                        a_cstr(bdots, "..");
                        (void)u8gFeed(bnav_g, bdots);
                        (void)u8gFeed(bnav_g, base_hex);
                        a_cquire(u8, bnav2);
                        u8csMv(bnav, bnav2);
                    }
                    ret = GRAFWeaveDiff(path, reporoot,
                                        branch, base_hex, YES, bnav);
                } else {
                    ret = GRAFDiffTreeRefs(branch, base_hex,
                                           reporoot);
                }
            } else {
                //  No query → wt vs base (weave-based).
                if (!$empty(path)) {
                    //  DIFF-003: file-scope → whole-file view.
                    ret = GRAFDiffWtFile(path, base_h40, reporoot, YES);
                } else {
                    ret = GRAFDiffWtTree(base_h40, base_hex,
                                         reporoot);
                }
            }
        }

    } else if ($eq(c->verb, v_map)) {
        uri mu = {};
        if (CLIUriLen(c) > 0) (void)CLIUriAt(&mu, c, 0);
        ret = GRAFMap(CLIUriLen(c) > 0 ? &mu : NULL);

    } else if ($eq(c->verb, v_log)) {
        if (CLIUriLen(c) < 1) {
            fprintf(stderr, "graf: log requires a URI\n");
            return FAILSANITY;
        }
        uri lu = {};
        (void)CLIUriAt(&lu, c, 0);
        ret = GRAFLog(&lu);

    } else if ($eq(c->verb, v_head)) {
        //  No URI → ahead/behind cur vs implicit target.  With a URI:
        //  fragment-only does message search; `?branch` does explicit
        //  ahead/behind.  GRAFHead dispatches internally.
        uri hu = {};
        if (CLIUriLen(c) >= 1) (void)CLIUriAt(&hu, c, 0);
        ret = GRAFHead(&hu);

    } else if ($eq(c->verb, v_weave)) {
        if (CLIUriLen(c) < 1) {
            fprintf(stderr, "graf: weave requires a file URI\n");
            return FAILSANITY;
        }
        uri uv = {};
        (void)CLIUriAt(&uv, c, 0);
        uri *u = &uv;
        u8cs wf = {}, wt = {};
        //  Shared `..` split (DIFF-004 `graf_query_range`).  When the
        //  query carries no `..` at all, a single ref is the `to` side
        //  (legacy weave shape — behaviour unchanged); a `..` query with
        //  empty halves (bare `?..`) yields empty wf/wt as before.
        if (!graf_query_range(u, wf, wt) && !u8csEmpty(u->query)) {
            a_dup(u8c, wq, u->query);
            a_cstr(wdots, "..");
            if (u8csFindS(wq, wdots) != OK) u8csMv(wt, u->query);
        }
        u8cs path = {};
        graf_uri_path(path, u);
        //  `weave` verb keeps the changed-hunks-only view (DIFF-003's
        //  whole-file scope is the `diff:<file>` projector's, not this).
        //  DIFF-004: empty navver → hunks keep the bare `#L` nav form,
        //  so the `weave` verb's output is unchanged.
        u8cs wnav = {};
        ret = GRAFWeaveDiff(path, reporoot, wf, wt, NO, wnav);

    } else {
        fprintf(stderr, "graf: unknown verb '%.*s'\n",
                (int)$len(c->verb), (char *)c->verb[0]);
        ret = FAILSANITY;
    }

    return ret;
}
