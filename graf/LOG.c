//  LOG: render commit history one-per-line for `be log:[path]?<ref>#<N>`.
//
//  Two shapes:
//    log:?<ref>#<N>           — branch history; walks parent chain
//                               from tip via the COMMIT_PARENT DAG.
//    log:./path/file.c?<ref>#<N>
//                             — file history; topo-walks the tip's
//                               ancestor closure newest-first, fetches
//                               the blob at `path` for each commit, and
//                               emits a row only when the blob bytes
//                               differ from the next-walked commit.
//
//  Output: one line per commit, "<sha7> <7-date> <message> (<author>)".
//
//  Streaming model: every commit is emitted as its own small hunk,
//  not one fat hunk at the end.  GRAFHunkEmit's `write()` to the bro
//  pipe blocks once the kernel buffer fills, so the producer is paced
//  by pager consumption — `be log:` can walk an arbitrarily long
//  history without buffering it.  When bro exits, write returns
//  EPIPE; GRAFHunkEmit zeroes graf_out_fd, and we detect that to
//  break the walk.
//
#include "GRAF.h"
#include "DAG.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "abc/B.h"
#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/URI.h"
#include "dog/DOG.h"
#include "dog/DPATH.h"
#include "dog/HUNK.h"
#include "dog/tok/TOK.h"
#include "dog/ULOG.h"
#include "dog/WHIFF.h"
#include "dog/git/GIT.h"
#include "keeper/KEEP.h"
#include "keeper/REFS.h"
#include "keeper/RESOLVE.h"
#include "keeper/WALK.h"

//  No artificial cap on default `be log:` — backpressure (pipe write
//  blocks once the pager kernel-buffer fills) paces the producer.
//  `#N` is a hard ceiling when the caller wants one.
#define LOG_DEFAULT_COUNT 0xffffffffu
//  Hard cap on the first-parent walk.  A cyclic DAG (reachable via a
//  60-bit hashlet collision) would otherwise spin up to LOG_DEFAULT_COUNT
//  (~4e9) iterations.  1<<20 ancestors is far beyond any real linear
//  history, so the loop always terminates.
#define LOG_MAX_WALK (1u << 20)
#define LOG_OBJ_BUF       (1UL << 20)
#define LOG_TEXT_BUF      (4UL << 20)   // one big hunk; ~40k commits @ 100B
#define LOG_TOKS_CAP      (1u << 16)    // tok32 entries (~9 per commit)
#define LOG_FILE_VERS_CAP (1u << 16)
#define LOG_ANC_SIZE      (1u << 18)

// --- Accumulator context ----------------------------------------------
//
//  Append-only render buffers shared across the whole walk.  Each
//  commit's row appends to `text` (and tok32 spans to `toks` in TLV
//  mode); GRAFHunkEmit is called once at the end with the merged
//  hunk, so HUNKu8sFeedText emits a single trailing blank line for
//  the whole log rather than one per commit.

typedef struct {
    Bu8   text;
    Bu32  toks;
    b8    tlv;
    i64   now;
} log_ctx;

// --- Helpers -----------------------------------------------------------

//  Resolve URI's `#hex`, `?ref`, or absent-query (use the wt's current
//  tip from sniff/at.log) to a 20-byte commit SHA-1.  Public so other
//  graf verbs (blame, etc.) can reuse the same resolution policy.
//
//  Token-level classification (hex prefix / full hex / branch path)
//  is delegated to `GRAFResolveRef`; this function adds the URI-layer
//  policy on top (which slot to consult, when to fall back to the
//  wt's current tip, remote-authority handling).
ok64 GRAFResolveTip(uricp u, sha1 *out) {
    sane(u && out);
    keeper *k = &KEEP;
    a_path(keepdir);
    call(HOMEBranchDir, k->h, keepdir, NULL);

    //  Fragment slot: any non-empty fragment resolves via the unified
    //  front-door resolver (40-hex / hashlet prefix / branch path).
    //  No cur_branch — fragments don't carry relative branch refs.
    if (!u8csEmpty(u->fragment)) {
        u8cs frag = {u->fragment[0], u->fragment[1]};
        u8cs no_cur = {};
        ok64 fr = KEEPResolveRef(out, frag, no_cur);
        if (fr == OK) return OK;
        //  Fall through to query/cur-fallback handling on miss — a
        //  fragment that doesn't resolve as hex / ref (e.g. `#msg-
        //  search`, future msg-substring) isn't a "tip" question and
        //  is handled elsewhere.
    }

    //  Bare `log:` with no query — fall back to the wt's current tip
    //  parked in `k->h->cur_sha` by HOMEOpen (sourced from `--at`
    //  forwarded by `be`).  Mirrors `git log` defaulting to HEAD with
    //  no args.  Empty `cur_sha` (no `--at` forwarded — direct CLI
    //  invocation) falls through to REFS so a fresh clone still
    //  resolves trunk.  EXCEPTION: when the URI carries an
    //  authority (e.g. `graf get //origin`), the user is asking for
    //  a *remote-tracking* ref — fall through to REFSResolve so the
    //  authority-substring match finds the cached row instead of
    //  silently rewriting "remote tip" as "local cur".
    if (u->query[0] == NULL && u8csEmpty(u->authority)) {
        if (DOGIsFullSha(u8bDataC(k->h->cur_sha))) {
            a_dup(u8c, hx, u8bData(k->h->cur_sha));
            return sha1FromHex(out, hx);
        }
    }

    a_pad(u8, arena_buf, 1024);
    uri resolved = {};
    a_dup(u8c, in_uri, u->data);
    ok64 ro = REFSResolve(&resolved, arena_buf, $path(keepdir), in_uri);

    //  Wire-prefix peel: `?refs/heads/X` / `?refs/X` / `?heads/X`
    //  may all map to a peer-stored canonical query like `?X` (the
    //  wire canonicaliser drops `refs/heads/` on incoming refs).
    //  Splice on u->data to keep authority intact.
    if ((ro != OK || u8csLen(resolved.query) < 40) &&
        u->query[0] != NULL && !u8csEmpty(u->query)) {
        char const *strips[] = {"refs/heads/", "refs/", "heads/", NULL};
        for (u32 si = 0; strips[si] != NULL &&
                         (ro != OK || u8csLen(resolved.query) < 40); si++) {
            a_dup(u8c, q, u->query);
            a_cstr(strip_s, strips[si]);
            if (u8csLen(q) <= u8csLen(strip_s)) continue;
            if (!u8csHasPrefix(q, strip_s)) continue;
            u8csUsed(q, u8csLen(strip_s));
            a_pad(u8, retry_buf, 512);
            u8cs head = {u->data[0], u->query[0]};
            u8bFeed(retry_buf, head);
            u8bFeed(retry_buf, q);
            a_dup(u8c, retry_uri, u8bDataC(retry_buf));
            zero(resolved);
            ro = REFSResolve(&resolved, arena_buf, $path(keepdir), retry_uri);
        }
    }

    if (ro != OK) return ro;
    if (u8csLen(resolved.query) < 40) fail(GRAFFAIL);
    return sha1FromHex(out, resolved.query);
}

//  `#N` / `#LN` → cap; missing or non-numeric fragment → unlimited.
static u32 graflog_count_from_frag(uricp u) {
    if (u8csEmpty(u->fragment)) return LOG_DEFAULT_COUNT;
    u8csc fr = {u->fragment[0], u->fragment[1]};
    u32 n = HUNKu8sFragSplit(fr, NULL);
    return n == 0 ? LOG_DEFAULT_COUNT : n;
}

//  Parse "Name <email> ts tz" into (name, ts).  Best-effort.
static void graflog_parse_author(u8cs value, u8cs name_out, i64 *ts_out) {
    name_out[0] = name_out[1] = NULL;
    *ts_out = 0;
    if ($empty(value)) return;

    u8cp lt = value[0];
    while (lt < value[1] && *lt != '<') lt++;
    u8cp ne = lt;
    while (ne > value[0] && *(ne - 1) == ' ') ne--;
    name_out[0] = value[0];
    name_out[1] = ne;

    u8cp gt = lt;
    while (gt < value[1] && *gt != '>') gt++;
    if (gt < value[1]) gt++;
    while (gt < value[1] && *gt == ' ') gt++;
    i64 ts = 0;
    while (gt < value[1] && *gt >= '0' && *gt <= '9') {
        ts = ts * 10 + (*gt - '0');
        gt++;
    }
    *ts_out = ts;
}

static void graflog_pack(u32b toks, u8b out, u8 tag) {
    if (!$ok(toks)) return;
    (void)u32bFeed1(toks, tok32Pack(tag, (u32)u8bDataLen(out)));
}

//  Emit "<sha7> <7-date> <summary> (<author>)\n" with matching tok32
//  spans (toks may be the zero slice for plain-ASCII paths).  Tags
//  borrow dog/TOK.h: 'L' literal-shaped columns (sha + date), 'S'
//  message word, 'P' parens, 'D' de-emphasised author, 'W' whitespace.
static ok64 graflog_render_commit(u8b out, u32b toks,
                                  sha1cp csha,
                                  u8cs commit_body, i64 now) {
    sane(csha);

    a_pad(u8, hashlet, SHA1_HASHLEN_LEN);
    (void)SHA1u8sFeedHashlet(hashlet_idle, csha);
    (void)u8bFeed(out, u8bDataC(hashlet));
    graflog_pack(toks, out, 'L');
    GRAFPackUriDiffSha(toks, out, csha);
    (void)u8bFeed1(out, ' ');
    graflog_pack(toks, out, 'W');

    u8cs author_val = {};
    u8cs message = {};
    a_dup(u8c, body, commit_body);
    u8cs field = {}, value = {};
    while (GITu8sDrainCommit(body, field, value) == OK) {
        if (u8csEmpty(field)) { $mv(message, value); break; }
        a_cstr(fa, "author");
        if ($eq(field, fa) && u8csEmpty(author_val)) $mv(author_val, value);
    }

    u8cs author_name = {};
    i64 ts = 0;
    graflog_parse_author(author_val, author_name, &ts);

    a_pad(u8, date, 8);
    (void)DOGutf8sFeedDate(date_idle, ts, now);
    (void)u8bFeed(out, u8bDataC(date));
    graflog_pack(toks, out, 'L');
    (void)u8bFeed1(out, ' ');
    graflog_pack(toks, out, 'W');

    u8cp ms = message[0];
    while (ms < message[1] && (*ms == '\n' || *ms == '\r')) ms++;
    u8cp me = ms;
    while (me < message[1] && *me != '\n' && *me != '\r') me++;
    if (me > ms) {
        u8cs summary = {ms, me};
        (void)u8bFeed(out, summary);
    }
    graflog_pack(toks, out, 'S');

    a_cstr(open_paren, " (");
    (void)u8bFeed(out, open_paren);
    graflog_pack(toks, out, 'P');
    if (!$empty(author_name)) (void)u8bFeed(out, author_name);
    graflog_pack(toks, out, 'D');
    (void)u8bFeed1(out, ')');
    graflog_pack(toks, out, 'P');

    (void)u8bFeed1(out, '\n');
    graflog_pack(toks, out, 'W');
    done;
}

//  Append one commit's row to the shared accumulator.  No emission
//  here — GRAFHunkEmit fires once at the end of GRAFLog with the
//  whole batch as a single hunk so HUNKu8sFeedText doesn't insert
//  a separator blank line between every commit.
static ok64 graflog_emit_one(log_ctx *lx, sha1cp csha, u8cs body) {
    sane(lx);
    return graflog_render_commit(lx->text,
                                 lx->tlv ? lx->toks : NULL,
                                 csha, body, lx->now);
}

static void graflog_strip_dotslash(u8cs path) {
    if ($len(path) >= 2 && path[0][0] == '.' && path[0][1] == '/') {
        path[0] += 2;
    }
}

//  Branch-history: walk parent chain from tip via COMMIT_PARENT.
static ok64 graflog_branch(log_ctx *lx, keeper *k, sha1cp tip,
                           u32 count) {
    sane(k && tip);
    a_carve(u8, cbuf, LOG_OBJ_BUF);

    u64 cur_h40 = WHIFFHashlet60(tip);
    //  Cap the walk so a cyclic DAG (hashlet collision) can't spin
    //  ~4e9 times; stop cleanly at the cap.
    u32 walk_cap = count < LOG_MAX_WALK ? count : LOG_MAX_WALK;
    for (u32 i = 0; i < walk_cap && cur_h40 != 0; i++) {
        if (graf_out_fd < 0) break;

        u8bReset(cbuf);
        u8 ot = 0;
        if (KEEPGet(cur_h40, DAG_H60_HEXLEN,
                    cbuf, &ot) != OK || ot != DOG_OBJ_COMMIT) break;
        a_dup(u8c, body, u8bData(cbuf));
        sha1 csha = {};
        KEEPObjSha(&csha, DOG_OBJ_COMMIT, body);

        if (graflog_emit_one(lx, &csha, body) != OK) break;

        //  First-parent walk: try the DAG index first (fast bulk
        //  lookup, returns 60-bit hashlet of parent).  On miss —
        //  common when the commit was fetched but never explicitly
        //  graf-indexed (e.g. via `be head //origin` which doesn't
        //  trigger `graf get`) — fall back to parsing the commit
        //  body we already fetched.  Keeps the log self-sufficient
        //  on freshly-fetched history.
        wh64 par_buf[2] = {};
        wh64s parents = {par_buf, par_buf + 2};
        wh64 *pbase = parents[0];
        wh128css runs = {NULL, NULL};
        GRAFRuns(runs);
        DAGParents(runs, parents, DAGPack(DAG_T_COMMIT, cur_h40));
        if (parents[0] != pbase) {
            cur_h40 = DAGHashlet(*pbase);
            continue;
        }
        //  DAG miss → parse commit body for first `parent <40hex>`.
        b8 found = NO;
        {
            a_dup(u8c, scan, body);
            u8cs field = {}, value = {};
            while (GITu8sDrainCommit(scan, field, value) == OK) {
                if ($empty(field)) break;
                a_cstr(par_kw, "parent");
                if (u8csEq(field, par_kw) && u8csLen(value) >= 40) {
                    sha1 par_sha = {};
                    if (sha1FromHex(&par_sha, value) == OK) {
                        cur_h40 = WHIFFHashlet60(&par_sha);
                        found = YES;
                    }
                    break;
                }
            }
        }
        if (!found) break;            // truly a root commit
    }
    done;
}

//  Walk commit `h40`'s tree to the entry at `path`; on success write its
//  20-byte SHA-1 to `*out` and set `*present = YES`.  On any miss
//  (commit absent, no tree header, path component missing) set
//  `*present = NO` and return OK — caller compares the (present, sha)
//  pair across commit/parents.  Uses caller-owned `cbuf` and `tbuf`
//  as reusable scratch so the hot loop allocates nothing.
static ok64 graflog_path_sha(sha1 *out, b8 *present, keeper *k, u64 h40,
                             u8cs path, Bu8 cbuf, Bu8 tbuf) {
    sane(out && present && k);
    *present = NO;

    u8bReset(cbuf);
    u8 ct = 0;
    if (KEEPGet(h40, DAG_H60_HEXLEN, cbuf, &ct) != OK ||
        ct != DOG_OBJ_COMMIT) done;

    sha1 cur = {};
    if (GITu8sCommitTree(u8bDataC(cbuf), cur.data) != OK) done;

    //  Walk down each tree level reusing `tbuf`.
    u8cs rest = {path[0], path[1]};
    while (!$empty(rest)) {
        u8cp slash = rest[0];
        while (slash < rest[1] && *slash != '/') slash++;
        u8cs name = {rest[0], slash};

        u8bReset(tbuf);
        u8 otype = 0;
        if (KEEPGetExact(&cur, tbuf, &otype) != OK) done;
        if (otype != DOG_OBJ_TREE) done;

        a_dup(u8c, body, u8bDataC(tbuf));
        u8cs field = {}, esha = {};
        b8 found = NO;
        while (GITu8sDrainTree(body, field, esha, NULL) == OK) {
            u8cs entry_name = {};
            if (GITu8sFileSplit(field, NULL, entry_name) != OK) continue;
            if (!u8csEq(entry_name, name)) continue;
            (void)sha1Drain(esha, &cur);
            found = YES;
            break;
        }
        if (!found) done;
        rest[0] = (slash < rest[1]) ? slash + 1 : slash;
    }

    *out = cur;
    *present = YES;
    done;
}

//  File-history: walk the tip's ancestor closure and keep commits whose
//  leaf SHA at `path` differs from EVERY parent's leaf SHA at `path`,
//  mirroring `git log <path>`'s default simplification
//  (TREESAME-to-any-parent drops the commit).  Both presence-flips
//  count: parent-absent/child-present is an add, parent-present/
//  child-absent is a delete, SHA-difference is a modify.  Root commits
//  (no parents) are kept iff the file exists.  Bounded by `count`.
//
//  Per-commit leaf SHAs are cached in a parallel array indexed by
//  topo position; parents (always at lower topo idx) are looked up
//  via a flat back-scan so a parent's tree is walked only once across
//  the whole history — the dominant cost for long linear chains is one
//  tree-fetch per commit, not per (commit, parent) edge.
static ok64 graflog_file(log_ctx *lx, keeper *k, sha1cp tip,
                         u8cs path, u32 count) {
    sane(k && tip && $ok(path));

    u64 tip_h40 = WHIFFHashlet60(tip);

    a_carve(wh128, ancestors, LOG_ANC_SIZE);
    zerob(ancestors);  // hash set — must be zero-init
    wh128css runs = {NULL, NULL};
    GRAFRuns(runs);
    DAGAncestors(ancestors, runs, tip_h40);

    //  Topo-sort parents-before-children, then walk in reverse for
    //  newest-first emission.
    size_t anc_cap = (size_t)(wh128bTerm(ancestors) - wh128bHead(ancestors));
    a_carve(u8, ord_buf, anc_cap * sizeof(u64));
    u64 *ordered = (u64 *)u8bDataHead(ord_buf);
    u32 nord = DAGTopoSort(ordered, (u32)anc_cap, ancestors, runs);

    a_carve(u8, cbuf,     LOG_OBJ_BUF);
    a_carve(u8, tbuf,     LOG_OBJ_BUF);
    a_carve(u8, sha_buf,  anc_cap * 21);
    a_carve(u8, keep_buf, anc_cap * sizeof(u64));
    //  Per-commit leaf cache: 21 bytes = 1 present-flag + 20 SHA bytes,
    //  indexed by topo position.
    u8 *leaf = (u8 *)u8bDataHead(sha_buf);
    u64 *keep = (u64 *)u8bDataHead(keep_buf);
    u32 nkeep = 0;

    for (u32 i = 0; i < nord; i++) {
        u64 h40 = ordered[i];
        u8 *slot = leaf + (size_t)i * 21;
        slot[0] = 0;
        if (h40 == 0) continue;

        sha1 csha = {};
        b8   c_present = NO;
        (void)graflog_path_sha(&csha, &c_present, k, h40, path, cbuf, tbuf);
        slot[0] = c_present ? 1 : 0;
        if (c_present) memcpy(slot + 1, csha.data, 20);

        wh64  par_buf[16] = {};
        wh64s parents = {par_buf, par_buf + 16};
        wh64 *pbase = parents[0];
        DAGParents(runs, parents, DAGPack(DAG_T_COMMIT, h40));
        u32 npar = (u32)(parents[0] - pbase);

        b8 keep_it;
        if (npar == 0) {
            keep_it = c_present;
        } else {
            //  Differs from every parent → real change.  Bail to NO
            //  the moment a parent is TREESAME for `path`.  Parents
            //  always sit at a lower topo idx; back-scan ordered[].
            keep_it = YES;
            for (u32 pi = 0; pi < npar; pi++) {
                u64 ph40 = DAGHashlet(pbase[pi]);
                b8 found_idx = NO;
                u8 *p_slot = NULL;
                for (u32 j = i; j > 0; j--) {
                    if (ordered[j - 1] == ph40) {
                        p_slot = leaf + (size_t)(j - 1) * 21;
                        found_idx = YES;
                        break;
                    }
                }
                b8 p_present = (found_idx && p_slot[0]);
                b8 same = (c_present == p_present) &&
                          (!c_present ||
                           memcmp(slot + 1, p_slot + 1, 20) == 0);
                if (same) { keep_it = NO; break; }
            }
        }

        if (keep_it && nkeep < anc_cap) keep[nkeep++] = h40;
    }

    //  Emit newest-first.
    u32 emitted = 0;
    for (u32 ki = nkeep; ki > 0 && emitted < count; ki--) {
        if (graf_out_fd < 0) break;
        u64 h40 = keep[ki - 1];
        u8bReset(cbuf);
        u8 ot = 0;
        if (KEEPGet(h40, DAG_H60_HEXLEN,
                    cbuf, &ot) != OK || ot != DOG_OBJ_COMMIT) continue;
        a_dup(u8c, body, u8bData(cbuf));
        sha1 csha = {};
        KEEPObjSha(&csha, DOG_OBJ_COMMIT, body);
        if (graflog_emit_one(lx, &csha, body) != OK) break;
        emitted++;
    }

    done;
}

// --- HEAD: commit-message substring search ----------------------------
//
//  `graf head '#parallel'` walks cur's first-parent chain via the DAG
//  COMMIT_PARENT index, fetches each commit body via keeper, and emits
//  the first commit whose message body contains the fragment as a
//  substring.  Used by `be head '#parallel'` (https://replicated.wiki/html/wiki/HEAD.html §HEAD).
//
//  Resolution policy: cur tip comes from `--at <root>?<branch>#<sha>`
//  forwarded by `be` and parked in `k->h->cur_sha` by HOMEOpen.  No
//  `--at` (direct invocation) is an error — we have nothing to walk.
//
//  Walk is bounded by GRAFHEAD_MAX_WALK so a pathological commit graph
//  can't hang the dispatcher.  On no match we leave stdout empty and
//  return GRAFNONE so the caller can render a "not found" hint.

#define GRAFHEAD_MAX_WALK 65536

static ok64 graf_head_msg_search(keeper *k, uricp u) {
    sane(k && u);
    if (u8bDataLen(k->h->cur_sha) != 40) {
        fprintf(stderr, "graf: head: --at sha not set\n");
        return GRAFFAIL;
    }

    sha1 cur_tip = {};
    {
        a_dup(u8c, hx, u8bData(k->h->cur_sha));
        if (sha1FromHex(&cur_tip, hx) != OK) return GRAFFAIL;
    }

    u8csc needle = {u->fragment[0], u->fragment[1]};

    call(GRAFArenaInit);

    log_ctx lx = {};
    lx.tlv = (HUNKMode == HUNKOutTLV);
    lx.now = (i64)time(NULL);
    __ = u8bAcquire(ABC_BASS, lx.text, LOG_TEXT_BUF);
    if (__ != OK) return __;
    if (lx.tlv) {
        __ = u32bAcquire(ABC_BASS, lx.toks, LOG_TOKS_CAP);
        if (__ != OK) return __;
    }

    a_carve(u8, cbuf, LOG_OBJ_BUF);

    ok64 go = GRAFOpen(k->h, NO);
    b8 own_open = (go == OK);
    if (go != OK && go != GRAFOPEN && go != GRAFOPENRO)
        return go;

    u64 cur_h40 = WHIFFHashlet60(&cur_tip);
    b8  found = NO;

    for (u32 i = 0; i < GRAFHEAD_MAX_WALK && cur_h40 != 0 && !found; i++) {
        u8bReset(cbuf);
        u8 ot = 0;
        if (KEEPGet(cur_h40, DAG_H60_HEXLEN, cbuf, &ot) != OK ||
            ot != DOG_OBJ_COMMIT) break;
        a_dup(u8c, body, u8bData(cbuf));
        sha1 csha = {};
        KEEPObjSha(&csha, DOG_OBJ_COMMIT, body);

        //  Skip headers; capture the message body (value after the
        //  empty-field sentinel that ends the header block).
        u8cs message = {};
        {
            a_dup(u8c, scan, u8bDataC(cbuf));
            u8cs field = {}, value = {};
            while (GITu8sDrainCommit(scan, field, value) == OK) {
                if (u8csEmpty(field)) { $mv(message, value); break; }
            }
        }

        if (!u8csEmpty(message) &&
            u8csFindS(message, needle) == OK) {
            (void)graflog_render_commit(lx.text,
                                        lx.tlv ? lx.toks : NULL,
                                        &csha, body, lx.now);
            found = YES;
            break;
        }

        //  First-parent walk — branches are linear (https://replicated.wiki/html/wiki/Verbs.html Inv. 2).
        wh64 par_buf[2] = {};
        wh64s parents = {par_buf, par_buf + 2};
        wh64 *pbase = parents[0];
        wh128css runs = {NULL, NULL};
        GRAFRuns(runs);
        DAGParents(runs, parents, DAGPack(DAG_T_COMMIT, cur_h40));
        if (parents[0] == pbase) break;
        cur_h40 = DAGHashlet(*pbase);
    }

    ok64 ret = OK;
    if (found) {
        a_pad(u8, title, 256);
        a_cstr(prefix, "head:#");
        (void)u8bFeed(title, prefix);
        (void)u8bFeed(title, u->fragment);
        hunk hk = {};
        u8csMv(hk.uri,  u8bDataC(title));
        u8csMv(hk.text, u8bDataC(lx.text));
        if (lx.tlv) u32csMv(hk.toks, u32bDataC(lx.toks));
        (void)GRAFHunkEmit(&hk, NULL);
    } else {
        fprintf(stderr,
                "graf: head: no commit message matches '%.*s'\n",
                (int)$len(u->fragment), (char const *)u->fragment[0]);
        ret = GRAFNONE;
    }

    if (own_open) GRAFClose();
    return ret;
}

// --- HEAD: ahead/behind cur vs target ---------------------------------
//
//  `graf head` (no URI)  → implicit target.  cur ≠ trunk: target =
//                          trunk; cur = trunk: target = cached remote
//                          counterpart in `.be/refs` (no network).
//  `graf head ?br`       → explicit target = branch `br`.
//
//  Output (one hunk, single trailing newline; `+` = ahead, `-` = behind):
//      + <sha7>  HH:MM  <msg> (<author>)         commits in cur, not target
//      - <sha7>  HH:MM  <msg> (<author>)         commits in target, not cur
//      + <path>                                  files only on cur side
//      - <path>                                  files only on target side
//      head: <N> ahead, <M> behind, <K> changed
//
//  Tree diff comes from keeper (`KEEPTreeDiff`); commits come from
//  graf's DAG ancestor sets (`DAGAncestors` + `DAGTopoSort`).

#define GRAFHEAD_ANC_SIZE (1u << 18)

//  Fetch a commit's tree SHA from keeper.  Mirrors graf/BLOB.c's
//  GRAFBlobAtCommit's first half — we only want the tree row.
static ok64 graf_head_commit_tree(keeper *k, u64 commit_h60, sha1 *out) {
    sane(k && out);
    a_carve(u8, cbuf, 1UL << 20);
    u8 ct = 0;
    ok64 o = KEEPGet(commit_h60, DAG_H60_HEXLEN, cbuf, &ct);
    if (o != OK || ct != DOG_OBJ_COMMIT) return KEEPNONE;
    return GITu8sCommitTree(u8bDataC(cbuf), out->data) == OK ? OK : KEEPNONE;
}

//  Per-row context for `graf_head_pick_remote_cb`.
typedef struct {
    sha1 sha;
    b8   found;
    u8cs want_query;   //  empty = wildcard (any peer row);
                       //  set = peer row's `?<q>` must equal this slice
} rt_ctx;

//  REFSEach callback: pick the first authority-bearing (peer-tracking)
//  ref row whose `?<query>` matches `want_query` (or any row when
//  `want_query` is empty).  Stops the walk via REFSSTOP after the
//  first match.
static ok64 graf_head_pick_remote_cb(refcp r, void *ctx) {
    rt_ctx *rt = (rt_ctx *)ctx;
    if (rt->found) return REFSSTOP;
    //  Key has `//` iff two consecutive '/' bytes appear before any '?'.
    //  At the same time, locate the LAST '?' so we can extract the
    //  row's query slot when filtering by `want_query`.
    u8cs key = {};
    u8csMv(key, r->key);
    u8cp p = key[0];
    b8 has_auth = NO;
    u8cp last_q = NULL;
    while (p < key[1]) {
        if (p + 1 < key[1] && p[0] == '/' && p[1] == '/') has_auth = YES;
        if (*p == '?') last_q = p;
        p++;
    }
    if (!has_auth) return OK;
    //  Filter by query when the caller named one (trunk-alias
    //  resolution: `?master` matches only peer rows whose URL ends
    //  in `?master`, not arbitrary `?feat`).
    if (!u8csEmpty(rt->want_query)) {
        u8cs key_query = {NULL, NULL};
        if (last_q != NULL && last_q + 1 <= key[1]) {
            key_query[0] = last_q + 1;
            key_query[1] = key[1];
        }
        if (!u8csEq(key_query, rt->want_query)) return OK;
    }
    //  val = `?<40-hex>` (REFS layout); strip the leading `?`.
    u8cs val = {};
    u8csMv(val, r->val);
    if (u8csLen(val) > 0 && val[0][0] == '?') val[0]++;
    if (u8csLen(val) != 40) return OK;
    if (sha1FromHex(&rt->sha, val) != OK) return OK;
    rt->found = YES;
    return REFSSTOP;
}

//  Decode a 40-hex sha slice into the 20-byte `out`.
static ok64 graf_head_decode_sha(sha1 *out, u8cs hex) {
    sane(out);
    if (u8csLen(hex) != 40) return GRAFFAIL;
    return sha1FromHex(out, hex);
}

//  Resolve target: explicit `?branch` via REFSResolve; else implicit —
//  cur ≠ trunk → trunk (`?` lookup); cur = trunk → first cached
//  remote-tracking row in REFS (the only kind whose key carries `//`).
//  Writes the 20-byte sha into `*out`; returns GRAFNONE on no match.
static ok64 graf_head_resolve_target(keeper *k, uricp u, sha1 *out) {
    sane(k && u && out);
    a_path(keepdir);
    call(HOMEBranchDir, k->h, keepdir, NULL);

    a_pad(u8, arena, 4096);
    uri resolved = {};

    a_dup(u8c, cur_branch, u8bData(k->h->cur_branch));
    b8 on_trunk = u8csEmpty(cur_branch);

    if (!u8csEmpty(u->query)) {
        //  Relative `?..` short-circuit: parent of cur.  For a direct
        //  child of trunk (cur_branch has no `/`), parent is trunk
        //  (REFS key `?`); for deeper branches we strip the last
        //  segment.  Other relative forms (`?./X`, `?../X`) fall
        //  through to REFSResolve as-is for now.
        b8 is_dotdot = (u8csLen(u->query) == 2 &&
                        u->query[0][0] == '.' && u->query[0][1] == '.');
        if (is_dotdot) {
            a_path(parent_q);
            (void)u8bFeed1(parent_q, '?');
            if (!on_trunk) {
                u8cs cb = {};
                u8csMv(cb, cur_branch);
                //  cur_branch is canonical (trailing '/' on non-trunk
                //  branches, per DPATHBranchNormFeed); strip it so the
                //  slash-walk finds the *separating* '/', not the
                //  trailing one.
                if (!u8csEmpty(cb) && *u8csLast(cb) == '/')
                    u8csShed1(cb);
                u8cp slash = cb[1];
                while (slash > cb[0] && *(slash - 1) != '/') slash--;
                if (slash > cb[0]) {
                    u8cs head = {cb[0], slash - 1};
                    (void)u8bFeed(parent_q, head);
                }
            }
            a_dup(u8c, qkey, u8bDataC(parent_q));
            ok64 ro = REFSResolve(&resolved, arena, $path(keepdir), qkey);
            ok64 rv = (ro == OK)
                    ? graf_head_decode_sha(out, resolved.query)
                    : GRAFNONE;
            return rv;
        }
        //  Trunk-alias detection.  In beagle, git's `master`/`main`/
        //  `trunk` are not standalone branches — they map to the
        //  project's trunk (DPATHBranchNormFeed collapses each to the
        //  empty branch).  `be head ?master` on a clone-from-git
        //  parent should therefore compare cur to the most recently
        //  fetched peer `?master` tip, NOT to a local `?master` REFS
        //  row that just records cur's own post on trunk (which would
        //  always report 0/0 ahead/behind).
        //
        //  Pull the branch slice out of the query: canonic form
        //  `/<project>/<branch>/<sha>` splits via DOGCanonQueryParse;
        //  bare `?master` is the query itself.  Normalising the
        //  branch slice and getting an empty result is the canonical
        //  trunk-alias signal.
        u8cs query_branch = {};
        u8csMv(query_branch, u->query);
        {
            u8cs c_proj = {}, c_branch = {}, c_pin = {};
            if (DOGCanonQueryParse(u->query, c_proj, c_branch, c_pin)) {
                u8csMv(query_branch, c_branch);          // legacy pin-in-query
            } else if (!u8csEmpty(query_branch) &&
                       *query_branch[0] == '/') {
                //  URI-001 Stage 3 scope-only form `/<proj>/<branch>`:
                //  strip the project to recover the branch slice.
                DOGQueryStripProject(query_branch);
            }
        }
        a_pad(u8, norm_buf, 256);
        (void)DPATHBranchNormFeed(norm_buf, query_branch);
        if (u8bDataLen(norm_buf) == 0) {
            //  Prefer the peer-form row whose URL carries the same
            //  alias (`ssh://…?master` for input `?master`).  The
            //  alias label is opaque to be — the peer wrote it, we
            //  echo it.  No alias canonicalisation here.
            rt_ctx rt = {.found = NO};
            u8csMv(rt.want_query, query_branch);
            (void)REFSEach($path(keepdir), graf_head_pick_remote_cb, &rt);
            if (rt.found) { sha1Mv(out, &rt.sha); return OK; }
            //  No peer row — fall back to trunk REFS (`?`) lookup.
            //  Catches `be head ?master` on a self-hosted be repo
            //  with no upstream: still resolves to the local trunk
            //  tip (yields 0/0 — accurate).
            a_cstr(qmark, "?");
            a_dup(u8c, qkey, qmark);
            ok64 ro = REFSResolve(&resolved, arena, $path(keepdir), qkey);
            return (ro == OK)
                    ? graf_head_decode_sha(out, resolved.query)
                    : GRAFNONE;
        }
        //  Real (non-alias) branch: resolve its tip through the
        //  canonical funnel (branch-first, leaf+trunk, strip retries).
        //  The legacy pin-in-query form let REFSResolve's canonic
        //  short-circuit lift the pin; the scope-only form (URI-001
        //  Stage 3) carries no pin, so resolve the branch slice itself.
        sha1 bsha = {};
        ok64 ro = KEEPResolveRef(&bsha, query_branch, cur_branch);
        if (ro == OK) { sha1Mv(out, &bsha); return OK; }
        return GRAFNONE;
    }
    //  Implicit (no query): same trunk-or-remote dispatch below.

    if (!on_trunk) {
        a_cstr(qmark, "?");
        a_dup(u8c, qkey, qmark);
        ok64 ro = REFSResolve(&resolved, arena, $path(keepdir), qkey);
        ok64 rv = (ro == OK)
                ? graf_head_decode_sha(out, resolved.query)
                : GRAFNONE;
        return rv;
    }

    //  cur = trunk: pick the first cached remote-tracking ref via
    //  REFSEach.  Refs whose key carries `//` are peer rows; the
    //  callback stops on the first match.  MVP — refine to a "primary
    //  remote" notion later.
    rt_ctx rt = {.found = NO};
    (void)REFSEach($path(keepdir), graf_head_pick_remote_cb, &rt);
    if (rt.found) {
        sha1Mv(out, &rt.sha);
        return OK;
    }
    //  HEAD-002: a self-hosted trunk with NO cached remote-tracking row
    //  has no upstream counterpart, but bare `be head` must still
    //  report "up to date" (0/0), not GRAFNONE "cannot resolve target"
    //  — the trunk tip IS resolvable from the local `?` REFS row.  Fall
    //  back to it (mirrors the trunk-alias `?master`-with-no-peer path
    //  above): target == cur → the ahead/behind walk renders 0/0.  Per
    //  https://replicated.wiki/html/wiki/HEAD.html §HEAD: bare `be head`
    //  on trunk is cur vs its cached remote counterpart; absent one, cur
    //  vs itself is the accurate (and only) honest answer.
    {
        a_cstr(qmark, "?");
        a_dup(u8c, qkey, qmark);
        ok64 ro = REFSResolve(&resolved, arena, $path(keepdir), qkey);
        return (ro == OK)
                ? graf_head_decode_sha(out, resolved.query)
                : GRAFNONE;
    }
}

//  Render one log row prefixed by `+ ` or `- `.  Reuses
//  graflog_render_commit; we just feed the prefix first.
static ok64 graf_head_render_prefixed(log_ctx *lx, u8 prefix,
                                      sha1cp csha, u8cs body) {
    (void)u8bFeed1(lx->text, prefix);
    (void)u8bFeed1(lx->text, ' ');
    if (lx->tlv) graflog_pack(lx->toks, lx->text, 'P');
    return graflog_render_commit(lx->text,
                                 lx->tlv ? lx->toks : NULL,
                                 csha, body, lx->now);
}

//  Topo-sort `set` (parents before children), then iterate newest-first
//  emitting each commit not in `exclude` as a prefixed log row.
static u32 graf_head_emit_diverged(log_ctx *lx, keeper *k,
                                   Bwh128 set, Bwh128 exclude,
                                   wh128css runs, u8 prefix,
                                   Bu8 cbuf) {
    size_t cap = (size_t)(wh128bTerm(set) - wh128bHead(set));
    if (cap == 0) return 0;
    //  Non-sane'd helper called from inside the caller's call() frame:
    //  acquired bytes rewind when the caller returns from call().
    Bu8 ord_buf = {};
    if (u8bAcquire(ABC_BASS, ord_buf, cap * sizeof(u64)) != OK) return 0;
    u64 *ordered = (u64 *)u8bDataHead(ord_buf);
    u32 nord = DAGTopoSort(ordered, (u32)cap, set, runs);

    u32 emitted = 0;
    for (u32 ki = nord; ki > 0; ki--) {
        u64 h = ordered[ki - 1];
        if (h == 0) continue;
        if (DAGAncestorsHas(exclude, h)) continue;
        u8bReset(cbuf);
        u8 ot = 0;
        if (KEEPGet(h, DAG_H60_HEXLEN, cbuf, &ot) != OK ||
            ot != DOG_OBJ_COMMIT) continue;
        a_dup(u8c, body, u8bData(cbuf));
        sha1 csha = {};
        KEEPObjSha(&csha, DOG_OBJ_COMMIT, body);
        if (graf_head_render_prefixed(lx, prefix, &csha, body) != OK) break;
        emitted++;
    }
    return emitted;
}

//  Walk a diff-ULOG (KEEPTreeDiff output) and emit one `<prefix> <path>\n`
//  line for every row whose verb stem matches `verb_stem`.  Used twice
//  by graf_head_ahead_behind: first for `add` (prefix '+'), then for
//  `del` (prefix '-').  Returns count emitted.
static u32 graf_head_emit_path_side(log_ctx *lx, u8cs diff,
                                    ron60 verb_stem, u8 prefix) {
    u32 n = 0;
    a_dup(u8c, scan, diff);
    while (!u8csEmpty(scan)) {
        ulogrec rec = {};
        ok64 dr = ULOGu8sDrain(scan, &rec);
        if (dr == NODATA) break;
        if (dr != OK) continue;
        if (ok64stem(rec.verb) != verb_stem) continue;
        (void)u8bFeed1(lx->text, prefix);
        (void)u8bFeed1(lx->text, ' ');
        if (lx->tlv) graflog_pack(lx->toks, lx->text, 'P');
        u8cs path = {rec.uri.path[0], rec.uri.path[1]};
        (void)u8bFeed(lx->text, path);
        if (lx->tlv) graflog_pack(lx->toks, lx->text, 'F');
        (void)u8bFeed1(lx->text, '\n');
        if (lx->tlv) graflog_pack(lx->toks, lx->text, 'W');
        n++;
    }
    return n;
}

static ok64 grafhead_render(log_ctx *lx, keeper *k, uricp u, Bu8 cbuf,
                            u64 cur_h, u64 target_h);

static ok64 graf_head_ahead_behind(keeper *k, uricp u) {
    sane(k && u);

    //  1. Resolve cur tip from --at.
    if (u8bDataLen(k->h->cur_sha) != 40) {
        fprintf(stderr, "graf: head: --at sha not set\n");
        return GRAFFAIL;
    }
    sha1 cur_tip = {};
    {
        a_dup(u8c, hx, u8bData(k->h->cur_sha));
        if (sha1FromHex(&cur_tip, hx) != OK) return GRAFFAIL;
    }
    u64 cur_h = WHIFFHashlet60(&cur_tip);

    //  2. Resolve target tip.
    sha1 target_tip = {};
    ok64 tr = graf_head_resolve_target(k, u, &target_tip);
    if (tr != OK) {
        fprintf(stderr, "graf: head: cannot resolve target\n");
        return tr;
    }
    u64 target_h = WHIFFHashlet60(&target_tip);

    //  3. Open graf, allocate render context.
    call(GRAFArenaInit);
    log_ctx lx = {};
    lx.tlv = (HUNKMode == HUNKOutTLV);
    lx.now = (i64)time(NULL);
    __ = u8bAcquire(ABC_BASS, lx.text, LOG_TEXT_BUF);
    if (__ != OK) return __;
    if (lx.tlv) {
        __ = u32bAcquire(ABC_BASS, lx.toks, LOG_TOKS_CAP);
        if (__ != OK) return __;
    }
    a_carve(u8, cbuf, LOG_OBJ_BUF);
    //  Branch-aware open: cur first (from --at's `h->cur_branch`), then
    //  switch to the URI's `?branch` so both cur and target chains are
    //  visible in PAST+DATA for the ancestor walks.  Same-branch hint
    //  (target == cur or empty target) collapses to a single open.
    u8cs open_branch_h = {};
    if (u8bHasData(k->h->cur_branch))
        u8csMv(open_branch_h, u8bDataC(k->h->cur_branch));
    static u8c const _h0 = 0;
    if (open_branch_h[0] == NULL) {
        open_branch_h[0] = &_h0; open_branch_h[1] = &_h0;
    }
    //  MEM-040: every post-open error path below leaks the ~18 MB owned
    //  graf open.  Funnel the post-open body through grafhead_render()
    //  so the `if (own_open) GRAFClose()` epilogue runs on EVERY exit
    //  (the body's try/nedo and a_carve early-returns can't bypass it).
    ok64 go = GRAFOpenBranch(k->h, open_branch_h, NO);
    b8 own_open = (go == OK);
    if (go != OK && go != GRAFOPEN && go != GRAFOPENRO)
        return go;
    ok64 rr = grafhead_render(&lx, k, u, cbuf, cur_h, target_h);
    if (own_open) GRAFClose();
    return rr;
}

//  Post-open render body for graf_head_ahead_behind (MEM-040).  Split
//  out so its fallible try / a_carve sites return through the owned-open
//  GRAFClose epilogue instead of leaking the open.
static ok64 grafhead_render(log_ctx *lx, keeper *k, uricp u, Bu8 cbuf,
                            u64 cur_h, u64 target_h) {
    sane(lx && k && u && cbuf);
    //  Switch graf to the target branch from u->query so target's
    //  commits land in DATA while cur is preserved in PAST.  No-op
    //  when target is empty or already covered.  Resolves the
    //  relative anchor `..` (parent) against cur first; deeper
    //  forms (`./X`, `../X`) currently fall through unmodified
    //  (TODO: share with PATCH's `absolutise_query`).
    if (!u8csEmpty(u->query)) {
        a_pad(u8, tabuf, 256);
        u8cs t = {u->query[0], u->query[1]};
        b8 dotdot = (u8csLen(t) == 2 &&
                     t[0][0] == '.' && t[0][1] == '.');
        if (dotdot) {
            //  Trunk's parent of cur: dirname.  For a top-level
            //  branch (no `/`), parent is trunk (empty).
            a_dup(u8c, cb, u8bDataC(k->h->cur_branch));
            u8cs cbv = {};
            u8csMv(cbv, cb);
            if (!u8csEmpty(cbv) && *u8csLast(cbv) == '/')
                u8csShed1(cbv);
            u8cp slash = cbv[1];
            while (slash > cbv[0] && *(slash - 1) != '/') slash--;
            if (slash > cbv[0]) {
                u8cs dn = {cbv[0], slash - 1};
                (void)u8bFeed(tabuf, dn);
            }
            t[0] = u8bDataHead(tabuf);
            t[1] = u8bIdleHead(tabuf);
        }
        //  Trunk aliases (`master`/`main`/`trunk`/heads-prefixed) and
        //  tag pins have no shard dir to switch into — both shapes
        //  surface as GRAFNOPATH.  Treat them as "stay on cur": the
        //  ancestor walks below run over cur's runs, which carry the
        //  full reachable closure for these light-weight forms.
        try(GRAFSwitchBranch, k->h, t);
        on(GRAFNOPATH) __ = OK;
        nedo return __;
    }
    wh128css runs = {NULL, NULL};
    GRAFRuns(runs);

    //  4. Build ancestor sets for both tips.  Hash sets — zero-init.
    a_carve(wh128, anc_cur,    GRAFHEAD_ANC_SIZE);
    a_carve(wh128, anc_target, GRAFHEAD_ANC_SIZE);
    zerob(anc_cur); zerob(anc_target);
    DAGAncestors(anc_cur,    runs, cur_h);
    DAGAncestors(anc_target, runs, target_h);

    //  5. Emit `+` commits (in cur, not in target) then `-` commits.
    u32 nahead = graf_head_emit_diverged(lx, k, anc_cur, anc_target,
                                          runs, '+', cbuf);
    u32 nbehind = graf_head_emit_diverged(lx, k, anc_target, anc_cur,
                                          runs, '-', cbuf);

    //  6. Tree diff via keeper.
    sha1 cur_tree = {}, target_tree = {};
    ok64 ct = graf_head_commit_tree(k, cur_h,    &cur_tree);
    ok64 tt = graf_head_commit_tree(k, target_h, &target_tree);
    u32 nchanged = 0;
    if (ct == OK && tt == OK) {
        a_carve(u8, diff_buf, 1UL << 20);
        //  KEEPTreeDiff(target → cur): `add` rows = paths on cur
        //  side only (`+`), `del` = paths on target side only (`-`),
        //  `mod` = both, sha differs (rendered as `+` then `-`).
        ok64 dr = KEEPTreeDiff(target_tree.data, cur_tree.data,
                               diff_buf);
        if (dr == OK) {
            a_cstr(s_add, "add"); a_dup(u8c, dvadd, s_add);
            a_cstr(s_del, "del"); a_dup(u8c, dvdel, s_del);
            a_cstr(s_mod, "mod"); a_dup(u8c, dvmod, s_mod);
            ron60 v_add = 0, v_del = 0, v_mod = 0;
            (void)RONutf8sDrain(&v_add, dvadd);
            (void)RONutf8sDrain(&v_del, dvdel);
            (void)RONutf8sDrain(&v_mod, dvmod);

            u8cs diff = {u8bDataHead(diff_buf), u8bIdleHead(diff_buf)};
            nchanged += graf_head_emit_path_side(lx, diff, v_add, '+');
            nchanged += graf_head_emit_path_side(lx, diff, v_mod, '+');
            nchanged += graf_head_emit_path_side(lx, diff, v_del, '-');
            nchanged += graf_head_emit_path_side(lx, diff, v_mod, '-');
        }
    }

    //  7. Summary line.  Leads with the current branch (empty == trunk)
    //  so a quick `be` tells the user where they are, then the
    //  ahead/behind/changed counters against the diff target.
    {
        a_dup(u8c, cur_br, u8bDataC(k->h->cur_branch));
        a_cstr(trunk_s, "trunk");
        u8cs br_label = {};
        u8csMv(br_label, u8csEmpty(cur_br) ? trunk_s : cur_br);
        (void)u8bPrintf(lx->text,
                        "head: ?" U8SFMT ": %u ahead, %u behind, %u changed\n",
                        u8sFmt(br_label),
                        (unsigned)nahead, (unsigned)nbehind, (unsigned)nchanged);
        if (lx->tlv) graflog_pack(lx->toks, lx->text, 'L');
    }

    //  8. Emit one hunk.
    a_pad(u8, title, 256);
    a_cstr(prefix, "head:");
    (void)u8bFeed(title, prefix);
    if (u->query[0] != NULL) {
        //  `?` separator emitted whenever the query slot is present,
        //  even if its body is empty (bare `be head ?` ⇒ trunk).
        (void)u8bFeed1(title, '?');
        if (!u8csEmpty(u->query)) (void)u8bFeed(title, u->query);
    }
    hunk hk = {};
    u8csMv(hk.uri,  u8bDataC(title));
    u8csMv(hk.text, u8bDataC(lx->text));
    if (lx->tlv) u32csMv(hk.toks, u32bDataC(lx->toks));
    (void)GRAFHunkEmit(&hk, NULL);

    done;
}

ok64 GRAFHead(uricp u) {
    sane(u);
    keeper *k = &KEEP;
    //  Fragment-only URI → message search; everything else (no URI,
    //  bare `?br`, etc.) → ahead/behind diff vs target.
    b8 frag_only = !u8csEmpty(u->fragment) &&
                   u8csEmpty(u->path) && u8csEmpty(u->query) &&
                   u8csEmpty(u->scheme) && u8csEmpty(u->authority);
    if (frag_only) return graf_head_msg_search(k, u);
    return graf_head_ahead_behind(k, u);
}

// --- Entry -------------------------------------------------------------

ok64 GRAFLog(uricp u) {
    sane(u);
    keeper *k = &KEEP;

    //  GRAFHunkEmit serialises into the legacy `graf_arena` global
    //  (separate from GRAF.arena).  Without init the arena is NULL
    //  and emission silently no-ops, so bro pages zero hunks.
    call(GRAFArenaInit);

    sha1 tip = {};
    call(GRAFResolveTip, u, &tip);

    u32 count = graflog_count_from_frag(u);

    log_ctx lx = {};
    lx.tlv = (HUNKMode == HUNKOutTLV);
    lx.now = (i64)time(NULL);

    __ = u8bAcquire(ABC_BASS, lx.text, LOG_TEXT_BUF);
    if (__ != OK) return __;
    if (lx.tlv) {
        __ = u32bAcquire(ABC_BASS, lx.toks, LOG_TOKS_CAP);
        if (__ != OK) return __;
    }

    a_pad(u8, title, 256);
    a_cstr(prefix, "log:");
    (void)u8bFeed(title, prefix);
    if (!u8csEmpty(u->path)) (void)u8bFeed(title, u->path);
    if (!u8csEmpty(u->query)) {
        (void)u8bFeed1(title, '?');
        (void)u8bFeed(title, u->query);
    }

    u8cs path = {};
    u8csMv(path, u->path);
    graflog_strip_dotslash(path);

    //  Open graf on the wt's current branch (`k->h->cur_branch`,
    //  populated from `--at`).  GRAFOpenBranch walks trunk → cur,
    //  registering every ancestor shard's `.graf.idx` along the
    //  path so the DAG walk below sees the full reachability —
    //  trunk's deep history (in `.be/*.graf.idx`) plus cur's
    //  session-added commits (in `.be/<cur>/*.graf.idx`).
    //
    //  When graf is already open on a different branch (it's a
    //  singleton; an earlier sniff PATCH may have opened it on
    //  some other leaf), GRAFOpenBranch short-circuits to GRAFOPEN
    //  and the cached state stays put.  Force the switch via
    //  SNIFFMaybeSwitchGraf — that's the canonical helper that
    //  flips DATA→PAST and scans the new leaf's idx files.
    u8cs cur_branch = {};
    if (u8bDataLen(k->h->cur_branch) > 0) {
        u8csMv(cur_branch, u8bDataC(k->h->cur_branch));
    } else {
        static u8c const _zero = 0;
        cur_branch[0] = (u8cp)&_zero;
        cur_branch[1] = (u8cp)&_zero;
    }
    ok64 go = GRAFOpenBranch(k->h, cur_branch, NO);
    b8 own_open = (go == OK);
    if (go != OK && go != GRAFOPEN && go != GRAFOPENRO)
        return go;
    //  Whether or not we opened, ensure the active leaf is cur.
    //  Restore-after pattern — warn loudly but don't unwind.
    {
        ok64 gs = GRAFSwitchBranch(k->h, cur_branch);
        if (gs != OK)
            fprintf(stderr,
                    "graf: log: cur-branch restore failed: %s\n",
                    ok64str(gs));
    }

    ok64 wo = $empty(path)
        ? graflog_branch(&lx, k, &tip, count)
        : graflog_file(&lx, k, &tip, path, count);

    if (wo == OK) {
        //  Single hunk for the whole walk — graf_emit (TLV via bro
        //  pipe, or HUNKu8sFeedText for terminal/pipe) picks bytes.
        hunk hk = {};
        hk.uri[0]  = u8bDataHead(title);
        hk.uri[1]  = u8bIdleHead(title);
        hk.text[0] = u8bDataHead(lx.text);
        hk.text[1] = u8bIdleHead(lx.text);
        if (lx.tlv) {
            hk.toks[0] = (u32 const *)u32bDataHead(lx.toks);
            hk.toks[1] = (u32 const *)u32bIdleHead(lx.toks);
        }
        (void)GRAFHunkEmit(&hk, NULL);
    }

    if (own_open) GRAFClose();
    return wo;
}
