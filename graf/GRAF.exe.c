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

// --- Bro pager setup ---
//
//  All producers emit `hunk` records via GRAFHunkEmit, which calls
//  `HUNKu8sFeedOut` dispatched off `HUNKMode`.  `HUNKMode` is already
//  resolved by `CLISetHUNKMode` in GRAF.cli.c before GRAFExec runs
//  (--tlv → TLV; --color → Color; --plain → Plain; default →
//  ANSIIsTTY() ? Color : Plain).  This function only picks the byte
//  sink: when the user explicitly asked for TLV on a TTY, spawn bro
//  to consume the wire; otherwise write directly to stdout.
static pid_t graf_start_pager(b8 tty_out) {
    if (HUNKMode == HUNKOutTLV && tty_out) {
        a_path(bropath);
        a$rg(a0, 0);
        a_cstr(bro_name, "bro");
        HOMEResolveSibling(NULL, bropath, bro_name, a0);
        u8cs args[] = {u8slit("bro")};
        u8css argv = {args, args + 1};
        pid_t pid = 0;
        int wfd = -1;
        if (FILESpawn($path(bropath), argv, &wfd, NULL, &pid) == OK) {
            graf_out_fd = wfd;
            signal(SIGPIPE, SIG_IGN);
            return pid;
        }
        //  bro missing — fall through to stdout TLV; SIGPIPE still
        //  matters because the user is likely piping somewhere.
    }
    graf_out_fd = STDOUT_FILENO;
    if (HUNKMode == HUNKOutTLV) signal(SIGPIPE, SIG_IGN);
    return -1;
}

static void graf_stop_pager(pid_t pid) {
    if (graf_out_fd >= 0 && graf_out_fd != STDOUT_FILENO) {
        close(graf_out_fd);
        graf_out_fd = -1;
    }
    if (pid > 0) {
        int rc = 0;
        FILEReap(pid, &rc);
        if (rc == 127)
            fprintf(stderr, "graf: bro pager not found\n");
    }
}

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

    Bu8 cbuf = {};
    call(u8bMap, cbuf, 1UL << 20);
    u8 ot = 0;
    ok64 go = KEEPGet(commit_h60, DAG_H60_HEXLEN, cbuf, &ot);
    if (go != OK || ot != DOG_OBJ_COMMIT) {
        u8bUnMap(cbuf);
        return (go != OK) ? go : KEEPNONE;
    }
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
    u8bUnMap(cbuf);
    return ret;
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

    //  Verb-less projector invocation (VERBS.md §"View projectors"):
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
    if ($empty(c->verb) && c->nuris > 0) {
        uri *pu = &c->uris[0];
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
        graf_start_pager(c->tty_out);
        a_cstr(status_uri, "graf:status");
        hunk hk = {};
        u8csMv(hk.uri,  status_uri);
        u8csMv(hk.text, u8bDataC(body));
        (void)GRAFHunkEmit(&hk, NULL);
        graf_stop_pager(-1);
        done;
    }

    // --- diff: requires a projector URI; resolved against keeper below ---

    if ($eq(c->verb, v_diff) && c->nuris < 1) {
        fprintf(stderr, "graf: diff requires a URI (diff:[<path>][?<ref>])\n");
        return FAILSANITY;
    }

    // --- merge: file-based, no keeper needed ---

    if ($eq(c->verb, v_merge)) {
        if (c->nuris < 3) {
            fprintf(stderr, "graf: merge requires 3 files: base ours theirs\n");
            return FAILSANITY;
        }
        u8cs bp = {}, op = {}, tp = {};
        graf_uri_path(bp, &c->uris[0]);
        graf_uri_path(op, &c->uris[1]);
        graf_uri_path(tp, &c->uris[2]);
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


    //  Branch-aware open: keeper sees `h->cur_branch`'s PAST+DATA
    //  chain so cross-branch reads (KEEPGet for `?other` commits in
    //  the head/log/map projectors) resolve.  Falls back to trunk
    //  when `--at` didn't carry a branch.
    {
        static u8c const _zero = 0;
        u8cs br = {&_zero, &_zero};
        if (u8bHasData(g->h->cur_branch))
            u8csMv(br, u8bDataC(g->h->cur_branch));
        call(KEEPOpenBranch, g->h, br, YES);
    }
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
        if (c->nuris >= 1) {
            uri *gu = &c->uris[0];
            if (!u8csEmpty(gu->path) && !u8csEmpty(gu->query) &&
                u8csEmpty(gu->authority))
                is_get_op = YES;
        }
        if (is_get_op) {
            uri *u = &c->uris[0];
            Bu8 out = {};
            ret = u8bMap(out, 16UL << 20);
            if (ret == OK) {
                a_dup(u8c, uri_in, u->data);
                ret = GRAFGet(out, uri_in);
                if (ret == OK) {
                    a_dup(u8c, obytes, u8bData(out));
                    ret = FILEFeedAll(STDOUT_FILENO, obytes);
                }
                u8bUnMap(out);
            }
        } else {
            //  Tip-walk indexer.  Bare `graf get` (no URI) walks the
            //  worktree's current tip (via `--at` parked in cur_sha).
            uri empty = {};
            uri *u = (c->nuris >= 1) ? &c->uris[0] : &empty;
            ret = GRAFIndexFromTips(u);
        }

    } else if ($eq(c->verb, v_blame)) {
        if (c->nuris < 1) {
            fprintf(stderr, "graf: blame requires a file URI\n");
            KEEPClose();
            return FAILSANITY;
        }
        pid_t pager = graf_start_pager(c->tty_out);
        u8cs path = {};
        graf_uri_path(path, &c->uris[0]);
        //  Resolve URI's #hex/?ref/absent-query to a tip commit
        //  hashlet so blame can scope its history walk.  A failure
        //  here just yields an unscoped (full-index) blame.
        u64 tip_h = 0;
        {
            sha1 tip = {};
            if (GRAFResolveTip(&c->uris[0], &tip) == OK)
                tip_h = WHIFFHashlet60(&tip);
        }
        ret = GRAFBlame(path, tip_h, reporoot);
        graf_stop_pager(pager);

    } else if ($eq(c->verb, v_diff)) {
        //  URI-driven diff (VERBS.md §"View projectors", `diff:`).  The
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
        pid_t pager = graf_start_pager(c->tty_out);
        uri *u = &c->uris[0];

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
            if (!DOGIsHashlet(wf)) {
                (void)KEEPSwitchBranch(KEEP.h, wf);
                (void)GRAFSwitchBranch(KEEP.h, wf);
            }
            if (!DOGIsHashlet(wt)) {
                (void)KEEPSwitchBranch(KEEP.h, wt);
                (void)GRAFSwitchBranch(KEEP.h, wt);
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
                graf_stop_pager(pager);
                KEEPClose();
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
                b8 handled = NO;
                if ($empty(path) && DOGIsHashlet(branch)) {
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
        graf_stop_pager(pager);

    } else if ($eq(c->verb, v_map)) {
        pid_t pager = graf_start_pager(c->tty_out);
        ret = GRAFMap(c->nuris > 0 ? &c->uris[0] : NULL);
        graf_stop_pager(pager);

    } else if ($eq(c->verb, v_log)) {
        if (c->nuris < 1) {
            fprintf(stderr, "graf: log requires a URI\n");
            KEEPClose();
            return FAILSANITY;
        }
        pid_t pager = graf_start_pager(c->tty_out);
        ret = GRAFLog(&c->uris[0]);
        graf_stop_pager(pager);

    } else if ($eq(c->verb, v_head)) {
        //  No URI → ahead/behind cur vs implicit target.  With a URI:
        //  fragment-only does message search; `?branch` does explicit
        //  ahead/behind.  GRAFHead dispatches internally.
        uri empty = {};
        uri *hu = (c->nuris >= 1) ? &c->uris[0] : &empty;
        pid_t pager = graf_start_pager(c->tty_out);
        ret = GRAFHead(hu);
        graf_stop_pager(pager);

    } else if ($eq(c->verb, v_weave)) {
        if (c->nuris < 1) {
            fprintf(stderr, "graf: weave requires a file URI\n");
            KEEPClose();
            return FAILSANITY;
        }
        pid_t pager = graf_start_pager(c->tty_out);
        uri *u = &c->uris[0];
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
        graf_stop_pager(pager);

    } else {
        fprintf(stderr, "graf: unknown verb '%.*s'\n",
                (int)$len(c->verb), (char *)c->verb[0]);
        ret = FAILSANITY;
    }

    KEEPClose();
    return ret;
}
