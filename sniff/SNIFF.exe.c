//  SNIFFExec — run a parsed CLI against an open sniff state.
//  Same effect as invoking `sniff ...` as a separate process.
//
#include "SNIFF.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "AT.h"
#include "CLASS.h"
#include "DEL.h"
#include "GET.h"
#include "LS.h"
#include "CAT.h"
#include "PATCH.h"
#include "POST.h"
#include "PUT.h"
#include "WATCH.h"
#include "dog/CLI.h"
#include "dog/DOG.h"
#include "dog/DPATH.h"
#include "dog/HOME.h"
#include "dog/git/IGNO.h"
#include "dog/ULOG.h"
#include "dog/git/GIT.h"
#include "keeper/KEEP.h"
#include "keeper/REFS.h"
#include "keeper/RESOLVE.h"
#include "keeper/WALK.h"
#include "SUBS.h"

#include "abc/B.h"
#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/RON.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/UTF8.h"

// --- Mode: Status ---
//
//  Bare `sniff` — overview of the working tree via the unified
//  4-way ULOG-merge classifier (`SNIFFClassify`).  Output follows
//  the ULOG row shape — `<time>\t<status>\t<path>` — except the
//  time is rendered human-readable via DOGutf8sFeedDate (7-char
//  relative form: `12:34`, `Tue`, `01Jan`, `01Jan25`).
//
//  Eight statuses, 3-char marker + colour on a tty (groups in
//  output order — clean state first, then staged, then unstaged,
//  then untracked):
//
//    ok   default in baseline + on disk + mtime ∈ stamp-set (clean)
//    put  blue    in baseline + put row since last post (staged mod)
//    new  green   not in baseline + put row             (staged add)
//    mov  cyan    put row with fragment (move src→dst)  (staged rename)
//    mod  yellow  in baseline + mtime ∉ stamp-set, no put/del row
//    del  brown   del row since last post               (staged remove)
//    mis  red     in baseline, file gone, no del row    (rm without `be delete`)
//    unk  grey    wt only, no put row                   (untracked)
//
//  Per-row time source: put/new/mov → put_rec->ts; del → del_rec->ts;
//  ok/mod/unk → wt_rec->ts (file's mtime); mis → 0 ("?").
//  `mov` rows render as `<src> -> <dst>` — the put row's fragment
//  carries the resolved dest path (see sniff/AT.md §"Move-form put
//  rows").  The dest path's own CLASS_WT_ONLY step is suppressed by
//  looking up its wt-mtime against the same put row.
//  Submodules are filtered upstream by SNIFFClassify.

#define STATUS_ANSI_PUT "\033[34m"        // dark blue
#define STATUS_ANSI_NEW "\033[32m"        // dark green
#define STATUS_ANSI_MOV "\033[36m"        // cyan
#define STATUS_ANSI_MOD "\033[33m"        // yellow
#define STATUS_ANSI_DEL "\033[38;5;94m"   // 256-color brown
#define STATUS_ANSI_MIS "\033[31m"        // red
#define STATUS_ANSI_UNK "\033[90m"        // grey
#define STATUS_ANSI_OFF "\033[0m"

//  Display verbs encode the status bucket inside one shared ULOG-
//  formatted row stream.  Disjoint from the .be/wtlog log's own verbs;
//  these never persist — the buffer is mmap'd and freed inside one
//  status invocation.  Re-encoded each call (cheap; <10 bytes each).
typedef struct {
    ron60 v_put, v_new, v_mov, v_mod, v_pat, v_del, v_mis, v_unk;
} status_verbs;

static void status_verbs_init(status_verbs *v) {
    a_cstr(s_put, "put"); v->v_put = SNIFFAtVerbOf(s_put);
    a_cstr(s_new, "new"); v->v_new = SNIFFAtVerbOf(s_new);
    a_cstr(s_mov, "mov"); v->v_mov = SNIFFAtVerbOf(s_mov);
    a_cstr(s_mod, "mod"); v->v_mod = SNIFFAtVerbOf(s_mod);
    a_cstr(s_pat, "pat"); v->v_pat = SNIFFAtVerbOf(s_pat);
    a_cstr(s_del, "del"); v->v_del = SNIFFAtVerbOf(s_del);
    a_cstr(s_mis, "mis"); v->v_mis = SNIFFAtVerbOf(s_mis);
    a_cstr(s_unk, "unk"); v->v_unk = SNIFFAtVerbOf(s_unk);
}

typedef struct {
    //  ULOG-formatted rows: `<ts>\t<verb>\t<path>\n`.  Verb encodes
    //  the bucket so the dump pass groups by verb in render order.
    //  The `ok` bucket never lists rows — clean tracked files would
    //  flood the output — so it's a counter only.
    Bu8 rows;
    u32 ok_n, put_n, new_n, mov_n, mod_n, pat_n, del_n, mis_n, unk_n;
    status_verbs v;
    i64 now;          // unix epoch seconds, for relative-date format
    u8cs reporoot;    // for resolving full paths in the wt-eq-base check
} status_buckets;

//  Touched-unchanged content check lives in CLASS.c so `ls:` and the
//  bare-status walker share one body of truth.  See CLASSWtEqBase.

//  Convert ron60 (packed local-time encoding via RONOfTime) to
//  unix-epoch seconds for DOGutf8sFeedDate.  0 → 0 ("?" placeholder).
static i64 status_ron60_to_secs(ron60 ts) {
    if (ts == 0) return 0;
    struct tm t = {};
    if (RONToTime(ts, &t, NULL) != OK) return 0;
    t.tm_isdst = -1;        // let mktime resolve DST
    time_t s = mktime(&t);
    return s == (time_t)-1 ? 0 : (i64)s;
}

//  Push one ULOG row carrying (ts, verb, path) into the shared
//  buffer.  Verb encodes the bucket — drained back out in render
//  order by status_dump_verb.
static void status_push(Bu8 rows, u8cs path, ron60 ts, ron60 verb,
                        u32 *count) {
    uri u = {};
    u.path[0] = path[0];
    u.path[1] = path[1];
    ulogrec rec = {.ts = ts, .verb = verb, .uri = u};
    if (ULOGu8sFeed(u8bIdle(rows), &rec) != OK) return;
    (*count)++;
}

//  Move flavour — carries both src (path) and dst (fragment) so the
//  dump pass can render `<src> -> <dst>` on one line.
static void status_push_mov(Bu8 rows, u8cs src, u8cs dst, ron60 ts,
                            ron60 verb, u32 *count) {
    uri u = {};
    u.path[0]     = src[0]; u.path[1]     = src[1];
    u.fragment[0] = dst[0]; u.fragment[1] = dst[1];
    ulogrec rec = {.ts = ts, .verb = verb, .uri = u};
    if (ULOGu8sFeed(u8bIdle(rows), &rec) != OK) return;
    (*count)++;
}

//  YES iff `mtime` stamps a put row whose URI carries a non-empty
//  fragment — i.e. the file at this path is the destination of an
//  in-flight move recorded in `.be/wtlog`.  Used to suppress the
//  dest's CLASS_WT_ONLY step (the source's step already emitted the
//  one `mov` line for the pair).
static b8 status_wt_is_mov_dst(ron60 mtime) {
    if (mtime == 0 || !SNIFFAtKnown(mtime)) return NO;
    ron60 ow_verb = 0;
    uri ow_u = {};
    if (SNIFFAtRowAtTs(mtime, &ow_verb, &ow_u) != OK) return NO;
    if (ow_verb != SNIFFAtVerbPut()) return NO;
    return !u8csEmpty(ow_u.fragment) ? YES : NO;
}

//  The `patch`-stamp test and the CLASS_BOTH baseline verdict now live
//  in sniff/CLASS.c::CLASSWtState — the one body of truth bare `be`
//  status and `be ls:` share (DIS-023).

static ok64 status_step(class_step const *step, void *ctx) {
    status_buckets *b = (status_buckets *)ctx;
    u8cs path = {step->path[0], step->path[1]};

    //  Staged groups take precedence — del/put rows describe user
    //  intent regardless of subsequent wt fiddling.
    if (step->del_rec != NULL) {
        status_push(b->rows, path,
                    step->del_rec->ts, b->v.v_del, &b->del_n);
        return OK;
    }
    if (step->put_rec != NULL) {
        ron60 ts = step->put_rec->ts;
        //  Move-form put row: source side carries the dest in
        //  .fragment.  Emit one `mov` line and skip the per-side
        //  buckets — the dest's own WT_ONLY step is suppressed
        //  below via the stamp lookup.
        u8cs frag = {step->put_rec->uri.fragment[0],
                     step->put_rec->uri.fragment[1]};
        //  Sub-mount bump (`put <sub>#<40-hex>`): fragment is a sha,
        //  not a destination path.  Bucket as `put` (or `new`) — the
        //  `mov` renderer would print the sha as a path.
        b8 is_bump = DOGIsFullSha(frag);
        if (!u8csEmpty(frag) && !is_bump) {
            status_push_mov(b->rows, path, frag, ts,
                            b->v.v_mov, &b->mov_n);
            return OK;
        }
        if (step->kind == CLASS_BOTH || step->kind == CLASS_BASE_ONLY)
            status_push(b->rows, path, ts, b->v.v_put, &b->put_n);
        else
            status_push(b->rows, path, ts, b->v.v_new, &b->new_n);
        return OK;
    }
    switch (step->kind) {
        case CLASS_WT_ONLY:
            //  Suppress the destination side of a move — its `mov`
            //  line was already emitted on the source path's step.
            if (step->wt_rec != NULL &&
                status_wt_is_mov_dst(step->wt_rec->ts)) break;
            //  Patched-in file: PATCH stamped the file's mtime to its
            //  row's ts (stamp_wrote / SNIFFAtStampPath); the file
            //  isn't in baseline yet, but it's a known tracked op —
            //  surface as `new` rather than `unk` so the user can see
            //  what the in-scope patch added.  True untracked files
            //  (no stamp) stay in `unk`.
            if (step->wt_rec != NULL &&
                SNIFFAtKnown(step->wt_rec->ts)) {
                status_push(b->rows, path, step->wt_rec->ts,
                            b->v.v_new, &b->new_n);
                break;
            }
            status_push(b->rows, path,
                        step->wt_rec ? step->wt_rec->ts : 0,
                        b->v.v_unk, &b->unk_n);
            break;
        case CLASS_BASE_ONLY:
            //  Gitlink with no on-disk wt row and no put/del intent
            //  is a clean sub-mount: pinned at baseline.  Bucket as
            //  `ok` instead of `mis` (which would imply the path
            //  was removed from disk).
            if (step->base_rec &&
                ok64Lit(step->base_rec->verb, 0) == RON_s) {
                b->ok_n++;
                break;
            }
            //  No useful timestamp — file is gone, baseline rows
            //  carry ts=0 by KEEPTreeULog convention.
            status_push(b->rows, path, 0, b->v.v_mis, &b->mis_n);
            break;
        case CLASS_BOTH:
            //  Classify against the baseline.  CLASSWtState is the one
            //  body of truth shared with `be ls:` (sniff/LS.c): a
            //  `patch`-stamped file is `pat` (merged-but-uncommitted),
            //  a file whose bytes hash to the baseline blob is `ok`
            //  (mtime-known or clean-drift), everything else is `mod`.
            //  Content-confirmed, never mtime-only — a restored-stamp
            //  mtime over edited bytes still reads as `mod` (DIS-023).
            switch (CLASSWtState(b->reporoot, step)) {
                case CLASS_WT_PATCHED:
                    status_push(b->rows, path, step->wt_rec->ts,
                                b->v.v_pat, &b->pat_n);
                    break;
                case CLASS_WT_CLEAN:
                    b->ok_n++;
                    break;
                case CLASS_WT_MODIFIED:
                    status_push(b->rows, path,
                                step->wt_rec->ts, b->v.v_mod, &b->mod_n);
                    break;
            }
            break;
    }
    return OK;
}

//  Drain the shared rows buffer, render rows whose verb matches
//  `verb_filter` as `<date>\t<status>\t<path>` (or `<src> -> <dst>`
//  for move rows whose URI carries a fragment).  On tty: time
//  column wears grey, status wears its own colour, path stays
//  default.  Walked once per bucket (≤7 passes) — trivial for
//  status sizes.
//  Append one tok32 to `toks`, end-offset = current text size.  Mirror
//  of `sniff/LS.c::ls_pack` — same idiom across all sniff per-file
//  emitters.
static void status_pack(Bu32 toks, Bu8 text, u8 tag) {
    (void)u32bFeed1(toks, tok32Pack(tag, (u32)u8bDataLen(text)));
}

//  Render one row into `(text, toks)` in the same column layout as
//  `sniff/LS.c::ls_emit_row`:
//
//    <7-date> <3-verb> <path>[ -> <dst>]\n<cat:URI>
//      tag 'L'  verb's tag  tag 'F'         tag 'U' (invisible)
//
//  ULOGVerbTag picks the palette slot per verb (Y for put, W for new,
//  …); the colour comes from THEME via HUNKu8sFeedColor.  The `cat:`
//  nav target is what `sniff/LS.c` uses for file rows — clicking the
//  row in bro opens the file.
//  YES iff a row of `verb` should click-nav to `diff:<path>` instead
//  of `cat:<path>` — i.e. the file diverges from baseline and the
//  user's likely first action is "show me what changed".  Only `mod`
//  qualifies today (mov already carries the dst so cat: lands on the
//  new file; new/unk have no baseline to diff against; del/mis are
//  gone from disk — diff would error).
static b8 status_verb_wants_diff_nav(ulogreccp rec, status_verbs const *v) {
    return rec->verb == v->v_mod;
}

static void status_emit_row_buf(Bu8 text, Bu32 toks,
                                 ulogreccp rec, i64 now,
                                 status_verbs const *v) {
    u8cs path    = {rec->uri.path[0],     rec->uri.path[1]};
    u8cs mov_dst = {rec->uri.fragment[0], rec->uri.fragment[1]};

    //  Date column (7 cols).
    if (rec->ts) {
        a_pad(u8, date, 8);
        i64 secs = status_ron60_to_secs(rec->ts);
        if (secs > 0) (void)DOGutf8sFeedDate(date_idle, secs, now);
        (void)u8bFeed(text, u8bDataC(date));
    } else {
        a_cstr(sp7, "       ");
        (void)u8bFeed(text, sp7);
    }
    status_pack(toks, text, 'L');
    (void)u8bFeed1(text, ' ');
    status_pack(toks, text, 'S');

    //  Verb column (3 cols, left-justified, space-padded).
    {
        a_pad(u8, vbuf, 16);
        (void)RONutf8sFeed(vbuf_idle, rec->verb);
        a_dup(u8c, vs, u8bDataC(vbuf));
        (void)u8bFeed(text, vs);
        size_t need = ($len(vs) < 3) ? 3 - $len(vs) : 0;
        for (size_t i = 0; i < need; i++) (void)u8bFeed1(text, ' ');
    }
    status_pack(toks, text, ULOGVerbTag(rec->verb));
    (void)u8bFeed1(text, ' ');
    status_pack(toks, text, 'S');

    //  Path column.  Moves render as `<src>#<dst>` — the URI-fragment
    //  form `put` rows actually store, so tooling that greps for
    //  `mov a.txt#a2.txt` (test/put/04-file-dir-mv) keeps matching.
    //  Tagged 'S' (neutral) — paths in bare-be status read better
    //  uncoloured against the verb-palette column to their left.
    (void)u8bFeed(text, path);
    if (!u8csEmpty(mov_dst)) {
        (void)u8bFeed1(text, '#');
        (void)u8bFeed(text, mov_dst);
    }
    (void)u8bFeed1(text, '\n');
    status_pack(toks, text, 'S');

    //  Invisible navigation URI.  `mod` rows go to the diff: projector
    //  (bro click → unified diff); everything else opens the file in
    //  cat: view.  Same `sniff/LS.c` convention for non-changed rows.
    if (status_verb_wants_diff_nav(rec, v)) {
        a_cstr(s, "diff:"); (void)u8bFeed(text, s);
    } else {
        a_cstr(s, "cat:");  (void)u8bFeed(text, s);
    }
    if (!u8csEmpty(mov_dst))
        (void)u8bFeed(text, mov_dst);
    else
        (void)u8bFeed(text, path);
    status_pack(toks, text, 'U');
}

//  Drain `rows`, emit every row matching `verb_filter` into the shared
//  `(text, toks)` accumulator.  One ≤7-pass sweep across the bucket
//  list per call from `sniff_status_work`.
static void status_dump_verb(Bu8 rows, ron60 verb_filter,
                              Bu8 text, Bu32 toks, i64 now,
                              status_verbs const *v) {
    a_dup(u8c, scan, u8bData(rows));
    while (!u8csEmpty(scan)) {
        ulogrec rec = {};
        ok64 dr = ULOGu8sDrain(scan, &rec);
        if (dr == NODATA) break;
        if (dr != OK) continue;                  // skip malformed
        if (rec.verb != verb_filter) continue;
        status_emit_row_buf(text, toks, &rec, now, v);
    }
}

//  Render the trailing summary line ("<rel>?<branch>\t<n> ok, <m> put,
//  …\n") into `(text, toks)`.  Each `<n> <verb>` segment carries the
//  verb's palette tag so colour mode picks up the per-bucket hue
//  (Y put, W new, V mov, E mod, X del, M mis, Q unk, D ok).  `ok`
//  uses 'D' (gray) — informational, deliberately low-contrast.
static void status_emit_summary_buf(Bu8 text, Bu32 toks,
                                     status_buckets const *b) {
    //  Resolve cwd-relative path + current branch for the `<rel>?<br>`
    //  prefix.  Empty when cwd == wt root and branch == trunk.
    u8cs cur_br = {};
    {
        ron60 bts = 0, bverb = 0;
        uri bu = {};
        if (SNIFFAtBaseline(&bts, &bverb, &bu) == OK)
            u8csMv(cur_br, bu.query);
    }
    a_path(cwd_path);
    u8cs rel = {};
    if (FILEGetCwd(cwd_path) == OK && !BNULL(HOME.wt) &&
        u8bDataLen(HOME.wt) > 0) {
        a_dup(u8c, cwd_s, u8bDataC(cwd_path));
        a_dup(u8c, wt_s,  u8bDataC(HOME.wt));
        size_t wt_len = $len(wt_s);
        if ($len(cwd_s) > wt_len &&
            u8csHasPrefix(cwd_s, wt_s) &&
            cwd_s[0][wt_len] == '/') {
            rel[0] = cwd_s[0] + wt_len + 1;
            rel[1] = cwd_s[1];
        }
    }
    if (!u8csEmpty(rel)) (void)u8bFeed(text, rel);
    (void)u8bFeed1(text, '?');
    if (!u8csEmpty(cur_br)) (void)u8bFeed(text, cur_br);
    (void)u8bFeed1(text, '\t');
    status_pack(toks, text, 'S');

    //  Per-bucket `<n> <verb>` pair tagged with the verb's palette slot.
    #define STATUS_BUCKET(count, verb_lit, tag) do {                   \
        if (count > 0) {                                               \
            if (u8bDataLen(text) > 0 && *(u8bIdleHead(text) - 1) != '\t')\
                { (void)u8bFeed1(text, ','); (void)u8bFeed1(text, ' '); \
                  status_pack(toks, text, 'S'); }                      \
            a_pad(u8, nbuf, 16);                                       \
            (void)utf8sFeed10(nbuf_idle, (u64)count);                  \
            (void)u8bFeed(text, u8bDataC(nbuf));                       \
            (void)u8bFeed1(text, ' ');                                 \
            a_cstr(vs, verb_lit);                                      \
            (void)u8bFeed(text, vs);                                   \
            status_pack(toks, text, (tag));                            \
        }                                                              \
    } while (0)
    STATUS_BUCKET(b->ok_n,  "ok",  'D');
    STATUS_BUCKET(b->put_n, "put", 'Y');
    STATUS_BUCKET(b->new_n, "new", 'W');
    STATUS_BUCKET(b->mov_n, "mov", 'V');
    STATUS_BUCKET(b->pat_n, "pat", 'C');
    STATUS_BUCKET(b->mod_n, "mod", 'E');
    STATUS_BUCKET(b->del_n, "del", 'X');
    STATUS_BUCKET(b->mis_n, "mis", 'M');
    STATUS_BUCKET(b->unk_n, "unk", 'Q');
    #undef STATUS_BUCKET
    (void)u8bFeed1(text, '\n');
    status_pack(toks, text, 'S');
}

//  Worker: assumes b's rows buffer is already mapped.  Returns the
//  classification result; never frees.
//
//  Output shape: one hunk per invocation (URI = `status:`), text is
//  the concatenation of per-file rows + a trailing summary line.
//  Per-file rows carry a 'U'-tagged `cat:<path>` nav target after the
//  `\n`, mirroring `sniff/LS.c`; bro turns those into click-anchors.
static ok64 sniff_status_work(status_buckets *b) {
    sane(b);
    call(SNIFFClassify, status_step, b);

    a_carve(u8,  text, 1UL << 20);
    a_carve(u32, toks, 1UL << 16);

    //  `ok` rows are noise — every tracked file at baseline content
    //  prints there.  Surface only the count in the trailing summary.
    if (b->put_n > 0) status_dump_verb(b->rows, b->v.v_put, text, toks, b->now, &b->v);
    if (b->new_n > 0) status_dump_verb(b->rows, b->v.v_new, text, toks, b->now, &b->v);
    if (b->mov_n > 0) status_dump_verb(b->rows, b->v.v_mov, text, toks, b->now, &b->v);
    if (b->pat_n > 0) status_dump_verb(b->rows, b->v.v_pat, text, toks, b->now, &b->v);
    if (b->mod_n > 0) status_dump_verb(b->rows, b->v.v_mod, text, toks, b->now, &b->v);
    if (b->del_n > 0) status_dump_verb(b->rows, b->v.v_del, text, toks, b->now, &b->v);
    if (b->mis_n > 0) status_dump_verb(b->rows, b->v.v_mis, text, toks, b->now, &b->v);
    if (b->unk_n > 0) status_dump_verb(b->rows, b->v.v_unk, text, toks, b->now, &b->v);

    //  Summary line finalises the hunk.
    status_emit_summary_buf(text, toks, b);

    a_cstr(status_uri, "status:");
    hunk hk = {};
    u8csMv(hk.uri,  status_uri);
    u8csMv(hk.text, u8bDataC(text));
    {
        tok32cs kv = {};
        kv[0] = (tok32c *)u32bDataHead(toks);
        kv[1] = (tok32c *)u32bDataHead(toks) + u32bDataLen(toks);
        u32csMv(hk.toks, kv);
    }
    a_carve(u8, big, u8bDataLen(text) + (1UL << 16));
    ok64 fo = HUNKu8sFeedOut(u8bIdle(big), &hk);
    if (fo == OK) (void)FILEout(u8bDataC(big));
    done;
}

//  Entry: maps the shared rows buffer, runs the worker, releases
//  regardless.  16 MB — mmap-backed so VA cost is paid lazily.
//  One buffer instead of six (~4× headroom): real-world wts
//  (~/dogs etc.) routinely produce tens of thousands of `unk` rows.
static ok64 sniff_status(u8cs reporoot) {
    sane(1);

    status_buckets b = {.now = (i64)time(NULL)};
    status_verbs_init(&b.v);
    u8csMv(b.reporoot, reporoot);
    //  16 MB row scratch from BASS (lazy); rewinds at return.  Carved
    //  here, not in the worker, so it outlives the worker's own rewind.
    a_carve(u8, rows, 1UL << 24);
    ((u8 **)b.rows)[0] = rows[0];
    ((u8 **)b.rows)[1] = rows[1];
    ((u8 **)b.rows)[2] = rows[2];
    ((u8 **)b.rows)[3] = rows[3];

    try(sniff_status_work, &b);
    done;
}


// --- Usage ---

static void sniff_usage(void) {
    fprintf(stderr,
            "Usage: sniff <command> [options] [URIs...]\n"
            "\n"
            "  sniff get <ref|sha>         checkout commit into the wt\n"
            "                              (alias: checkout)\n"
            "  sniff put <path>...         record `put` rows in the ULOG\n"
            "  sniff delete <path>...      record `delete` rows in the ULOG\n"
            "  sniff post <msg words...>   commit: walk baseline + wt,\n"
            "                              resolve change-set, feed one pack.\n"
            "                              Trailing free-form words become\n"
            "                              the commit message via the URI's\n"
            "                              #fragment.  (alias: commit)\n"
            "  sniff patch ?<ref|sha>      weave-merge the given ref/sha\n"
            "                              into the wt via graf\n"
            "  sniff status                list mtime-dirty files\n"
            "  sniff [--tlv] ls:[<URI>]    view projector (https://replicated.wiki/html/wiki/Projector.html §View\n"
            "                              projectors); verb-less; --tlv\n"
            "                              emits HUNK TLV for `bro`\n"
            "  sniff watch                 start inotify daemon (fork;\n"
            "                              pid at <wt>/.be/sniff.pid)\n"
            "                              emits `mod <path>` rows\n"
            "  sniff stop                  stop the watch daemon\n"
            "  sniff help                  this message\n"
            "\n"
            "  Change-set rules at post time:\n"
            "    explicit put/delete since last post wins;\n"
            "    otherwise mtime ∉ ULOG stamp-set ⇒ include (implicit);\n"
            "    missing files with explicit-delete OR in implicit mode ⇒ drop.\n"
            "\n"
            "  Flags:\n"
            "    -m <msg>       commit message (legacy; prefer trailing words)\n"
            "    --author <who> author string\n");
}

// --- Verb/flag tables exported for the CLI wrapper ---

char const *const SNIFF_VERBS[] = {
    "index", "update", "status", "checkout",
    "commit", "watch", "stop", "help",
    "get", "post", "put", "delete", "patch", "sub-mount", NULL
};

char const SNIFF_VAL_FLAGS[] =
    "-m\0--author\0--at\0--source\0";

// --- Entry: run the parsed CLI against the open state ---

ok64 SNIFFExec(cli *c) {
    sane(c);

    u8cs reporoot = {};
    if (!u8bHasData(c->repo)) fail(SNIFFFAIL);
    u8csMv(reporoot, $path(c->repo));

    a_cstr(v_help, "help");
    a_cstr(v_update, "update");
    a_cstr(v_status, "status");
    a_cstr(v_checkout, "checkout");
    a_cstr(v_commit, "commit");
    a_cstr(v_watch, "watch");
    a_cstr(v_stop, "stop");
    a_cstr(v_get, "get");
    a_cstr(v_post, "post");
    a_cstr(v_put, "put");
    a_cstr(v_delete, "delete");
    a_cstr(v_patch, "patch");

    if ($eq(c->verb, v_help) || CLIHas(c, "-h") || CLIHas(c, "--help")) {
        sniff_usage(); done;
    }

    if ($eq(c->verb, v_stop)) {
        call(SNIFFWatchStop, reporoot); done;
    }

    b8 is_checkout = $eq(c->verb, v_checkout) || $eq(c->verb, v_get);
    b8 is_post = $eq(c->verb, v_post) || $eq(c->verb, v_commit);
    b8 is_put = $eq(c->verb, v_put);
    b8 is_update = $eq(c->verb, v_update);
    b8 is_watch = $eq(c->verb, v_watch);
    //  Bare `sniff` (no verb, no URI, no `--status` flag) defaults
    //  to status — same overview an interactive user expects.  Any
    //  URI or projector still routes through their own arms below.
    b8 is_status = $eq(c->verb, v_status)
                || CLIHas(c, "--status")
                || ($empty(c->verb) && CLIUriLen(c) == 0);

    //  Verb-less projector invocation (https://replicated.wiki/html/wiki/Projector.html §"View projectors"):
    //  `sniff <proj>:<URI>` — no verb.  Scheme selects the projector;
    //  dog/DOG.c owns the scheme→dog table so we dispatch only when
    //  the URI's scheme resolves to this dog ("sniff").  Only `ls:`
    //  today; the branch is widened row-by-row in DOG_PROJECTORS.
    b8 is_projector = NO;
    uri proj_uv = {};
    uri *proj_u = NULL;
    if ($empty(c->verb) && CLIUriLen(c) > 0) {
        (void)CLIUriAt(&proj_uv, c, 0);
        char const *dog = DOGProjectorDog(proj_uv.scheme);
        if (dog != NULL && strcmp(dog, "sniff") == 0) {
            is_projector = YES;
            proj_u = &proj_uv;
        }
    }
    b8 is_delete = $eq(c->verb, v_delete);
    b8 is_patch = $eq(c->verb, v_patch);
    a_cstr(v_submount, "sub-mount");
    b8 is_submount = $eq(c->verb, v_submount);

    ok64 ret = OK;

    if (is_post) {
        u8cs commit_msg = {};
        //  Per https://replicated.wiki/html/wiki/Verbs.html: free-form trailing words are folded into a
        //  URI's #fragment by CLIParse.  Prefer that over the legacy
        //  `-m <msg>` flag, which still works for backwards compat.
        b8 frag_present = NO;   // a `#…` fragment slot was supplied
        for (u32 i = 0; i < CLIUriLen(c); i++) {
            uri uv = {};
            call(CLIUriAt, &uv, c, i);
            if (!u8csEmpty(uv.fragment)) {
                $mv(commit_msg, uv.fragment);
                frag_present = YES;
                break;
            }
        }
        if (!$ok(commit_msg)) CLIFlag(commit_msg, c, "-m");

        //  URI-002 forget modifier (DOG_BANG_FRAG): a trailing `!` on the
        //  post fragment means "forget" (no parent ref → foster).  The
        //  uniform debanger sheds the single `!` (the same tail-shed every
        //  component parser uses); we remember the intent and inject
        //  `--forget` so POSTCommit's header block picks `foster` over
        //  `parent` for a branch-sourced patch row.  A NAMED row stays
        //  `picked` either way.  Forms: `#!` = reuse-msg + forget,
        //  `#msg!` = new-msg + forget, `#msg` = new-msg + parent, bare =
        //  reuse + parent.
        b8 forget = NO;
        if ($ok(commit_msg) && DOGDebangSlice(commit_msg)) forget = YES;
        //  Ban: after shedding the single modifier, the real message may
        //  not ITSELF still end in `!` (`fix it!!` → `fix it!` here would
        //  be ambiguous with the forget modifier) — POSTBANG, refused
        //  before any commit.  DOGDebangSlice only sheds one `!`, so a
        //  remaining trailing `!` is the literal-message case.
        if ($ok(commit_msg) && !u8csEmpty(commit_msg) &&
            *u8csLast(commit_msg) == '!') {
            fprintf(stderr,
                "sniff: post: commit message may not end in `!` — the "
                "trailing `!` is the forget modifier (use `#msg!` to "
                "forget, `#msg` to refer back)\n");
            return POSTBANG;
        }
        if (forget) {
            a_cstr(forget_flag, "--forget");
            a_cstr(empty_val,   "");
            (void)u8csbFeed1(c->flags, forget_flag);
            (void)u8csbFeed1(c->flags, empty_val);
        }
        //  `#!` (or `#msg!` shed to empty) — fragment was present but the
        //  message is now empty.  Drop commit_msg so the reuse path runs
        //  (original message), still carrying the forget intent via the
        //  injected flag.
        if (frag_present && $ok(commit_msg) && u8csEmpty(commit_msg)) {
            commit_msg[0] = NULL;
            commit_msg[1] = NULL;
        }
        u8cs commit_author = {};
        CLIFlag(commit_author, c, "--author");
        //  Default identity: assemble `<name> <<email>>` from the wt's
        //  `<root>/.be/config` (TOML — `[user] name = "..." email =
        //  "..."`).  Falls back to the legacy sniff sentinel only when
        //  config has neither field (test fixtures without a seeded
        //  identity).
        a_pad(u8, author_buf, 512);
        a_pad(u8, name_buf,   256);
        a_pad(u8, email_buf,  256);
        if (!$ok(commit_author)) {
            a_cstr(user_s,  "user");
            a_cstr(name_s,  "name");
            a_cstr(email_s, "email");
            a_path(name_p,  user_s, name_s);
            a_path(email_p, user_s, email_s);
            u8 *n_start = u8bIdleHead(name_buf);
            u8 *e_start = u8bIdleHead(email_buf);
            u8s ndst = {n_start, name_buf[3]};
            u8s edst = {e_start, email_buf[3]};
            (void)HOMEGetConfig(ndst, $path(name_p));
            (void)HOMEGetConfig(edst, $path(email_p));
            //  HOMEGetConfig advances ndst[0] / edst[0] past the
            //  bytes it wrote; the value lives in [start, ndst[0]).
            u8cs name  = {n_start, ndst[0]};
            u8cs email = {e_start, edst[0]};
            if ($empty(name) && $empty(email)) {
                a_cstr(def, "sniff <sniff@dogs>");
                u8bFeed(author_buf, def);
            } else {
                if (!$empty(name)) {
                    u8bFeed(author_buf, name);
                    u8bFeed1(author_buf, ' ');
                }
                u8bFeed1(author_buf, '<');
                if (!$empty(email)) u8bFeed(author_buf, email);
                u8bFeed1(author_buf, '>');
            }
            commit_author[0] = u8bDataHead(author_buf);
            commit_author[1] = u8bIdleHead(author_buf);
        }

        //  Pick the first URI with a non-empty query as a label target
        //  (e.g. `?heads/main`, `?tags/v0.0.1`).  Also accept bare `?`
        //  (empty query but data starts with `?`) as the trunk target
        //  — `be post ?` from a child branch means "FF trunk to cur".
        uri label_uriv = {};
        uri *label_uri = NULL;
        for (u32 i = 0; i < CLIUriLen(c); i++) {
            uri uu = {};
            call(CLIUriAt, &uu, c, i);
            //  DIS-026: a `?branch` riding a FULL transport URL
            //  (`ssh://host/repo?main`) names the REMOTE push target —
            //  the wire refname `keeper post` (BEActKeeperPush) pushes
            //  onto, not a local label.  Skip only full-transport URIs
            //  (a non-empty transport scheme) so the refname reaches
            //  keeper untouched; a bare `//alias?branch` (no scheme) and
            //  local `?branch` promotes still fire — DIS-020's NOBRANCH
            //  for a missing local label is preserved.  See POST.mkd §POST.
            if (!$empty(uu.scheme) && DOGIsTransport(uu.scheme)) continue;
            if (!$empty(uu.query)) { label_uriv = uu; label_uri = &label_uriv; break; }
            //  Bare `?` (trunk): data is exactly "?" and every other
            //  slot is empty.
            if (!$empty(uu.data) && uu.data[0][0] == '?' &&
                u8csLen(uu.data) == 1 &&
                $empty(uu.path) && $empty(uu.fragment) &&
                $empty(uu.authority) && $empty(uu.scheme)) {
                label_uriv = uu;
                label_uri  = &label_uriv;
                break;
            }
        }

        //  https://replicated.wiki/html/wiki/POST.html §POST invariant 3: POST never rewrites cur's
        //  history; rebase is `be patch ?br#` + `be post`, looped.
        //  The `//remote` slot on POST means "FF-advance remote's
        //  counterpart to cur.tip" — i.e. push — handled at the
        //  dispatcher level by `keeper post` after this sniff post
        //  commits.  Sniff post itself is push-agnostic: it just
        //  commits-on-cur (and optionally FF-advances a local
        //  `?branch` via POSTPromote below).
        //
        //  Resolve a relative label (`?./X`, `?../X`, `?..`) before
        //  PUTSetLabel sees it.  Buffers must outlive PUTSetLabel —
        //  hence stack-local pads scoped to this if-block.
        a_pad(u8, label_qbuf,    256);
        a_pad(u8, label_databuf, 260);
        if (label_uri != NULL) {
            if (SNIFFAtResolveRelativeURI(label_uri, label_qbuf, label_databuf,
                                  NULL) != OK) {
                ret = SNIFFFAIL;
            }
        }

        //  Pure-push intent: every URI carries only `//remote` (no
        //  `#frag`, no `?branch`, no path).  Per https://replicated.wiki/html/wiki/POST.html §POST, the
        //  `//remote` slot is orthogonal — standalone it's a label
        //  move on the remote side, not a commit trigger.  Skip both
        //  the commit step AND the dry-run print; BEPost dispatches
        //  `keeper post` after us to do the wire-push.
        b8 pure_push = NO;
        if (!$ok(commit_msg) && label_uri == NULL && CLIUriLen(c) > 0) {
            pure_push = YES;
            for (u32 i = 0; i < CLIUriLen(c); i++) {
                uri uu = {};
                call(CLIUriAt, &uu, c, i);
                if ($empty(uu.authority) ||
                    !$empty(uu.path) ||
                    !$empty(uu.query) ||
                    !$empty(uu.fragment)) {
                    pure_push = NO;
                    break;
                }
            }
        }
        if (pure_push) {
            //  Nothing to do at sniff layer; keeper post handles wire.
        } else if (!$ok(commit_msg) && label_uri == NULL) {
            //  Bare `sniff post` (no -m, no ?label).  When patch rows
            //  are present since the latest get/post, compose default
            //  msg+author from the absorbed commits and commit; else
            //  fall back to dry-run status so the user sees what the
            //  next non-bare post would produce.
            a_pad(u8, def_msg_buf,  1024);
            a_pad(u8, def_auth_buf, 512);
            u8cs def_msg  = {};
            u8cs def_auth = {};
            u32  def_n    = 0;
            ok64 dr = POSTPatchDefaults(def_msg_buf,  &def_msg,
                                        def_auth_buf, &def_auth,
                                        &def_n);
            if (dr == OK && def_n > 0) {
                u8cs no_target = {};
                sha1 sha = {};
                ret = POSTCommit(no_target,
                                 def_msg, def_auth, c, &sha);
                if (ret == OK) {
                    a_pad(u8, hex, 40);
                    a_rawc(rs, sha);
                    HEXu8sFeedSome(hex_idle, rs);
                    fprintf(stderr, "sniff: commit " U8SFMT "\n",
                            u8sFmt(u8bDataC(hex)));
                }
            } else if (def_n > 0) {
                //  Patch rows in scope but msg can't be auto-resolved
                //  (zero or >1 usable msgs).  Refuse per https://replicated.wiki/html/wiki/POST.html §POST
                //  message-resolution; user must supply `#msg`.
                fprintf(stderr,
                    "sniff: post: cannot auto-resolve commit msg "
                    "from %u patch row(s); supply `#msg`\n", def_n);
                ret = POSTNOMSG;
            } else {
                ret = POSTPrintStatus();
            }
        } else {
            //  POSTCommit does its own wt scan + change-set resolve;
            //  no pre-pass needed anymore.
            a_pad(u8, hex, 40);
            if ($ok(commit_msg)) {
                //  Cross-branch POST: when a label_uri is present,
                //  its query is the *commit target*.  POSTCommit
                //  lands the new commit on that branch (instead of
                //  the wt's baseline branch); the wt's other branch
                //  is left untouched in REFS, and `.be/wtlog` resets
                //  to (target, new_tip).  No separate PUTSetLabel
                //  pass — that was the old "label both branches at
                //  the same sha" behaviour, replaced here.
                u8cs target = {};
                if (label_uri != NULL) {
                    target[0] = label_uri->query[0];
                    target[1] = label_uri->query[1];
                }
                sha1 sha = {};
                ret = POSTCommit(target,
                                 commit_msg, commit_author, c, &sha);
                if (ret == OK) {
                    a_rawc(rs, sha);
                    HEXu8sFeedSome(hex_idle, rs);
                }
            } else if (label_uri != NULL) {
                //  No commit_msg + label_uri (`be post ?<br>`).  Per
                //  https://replicated.wiki/html/wiki/POST.html §POST `?branch` row: "FF-advance ?branch
                //  to cur's tip".  This is the local-branch promote
                //  — we move target's REFS row forward to cur.tip,
                //  migrate any commit/tree/blob objects from cur's
                //  shard into target's shard along the way, and leave
                //  cur untouched.  Refuses if cur is not a descendant
                //  of target.tip (POSTNOFF) or if the rebase produces
                //  a conflict.
                //
                //  (Remote-target shapes — `//host`, `ssh://...` —
                //  short-circuited above via POSTRebaseOntoSha.  Those
                //  pull remote work into cur, which is the inverse
                //  direction.)
                //  POST-004: a parent recursion into a detached sub
                //  passes the synthetic target `?/<sub>/.<proj>[/<br>]`
                //  but may blank the per-sub `#msg` (`--sub-msg sub=`)
                //  to force the sub to auto-resolve from its OWN patch
                //  rows (test post/17).  When in-scope patch rows exist
                //  and a usable msg auto-resolves, COMMIT onto the
                //  target (creating the synthetic branch at the pin) —
                //  this is the absorb-into-detached-sub path that
                //  mirrors the bare-post auto-resolve above, not a pure
                //  label move.  Only when there is nothing to commit do
                //  we fall through to the FF label-promote.
                a_pad(u8, sm_buf,   1024);
                a_pad(u8, sa_buf,   512);
                u8cs sm_msg  = {};
                u8cs sa_auth = {};
                u32  sm_n    = 0;
                ok64 sdr = POSTPatchDefaults(sm_buf,  &sm_msg,
                                             sa_buf, &sa_auth,
                                             &sm_n);
                if (sdr == OK && sm_n > 0) {
                    u8cs target = {};
                    target[0] = label_uri->query[0];
                    target[1] = label_uri->query[1];
                    sha1 sha = {};
                    ret = POSTCommit(target, sm_msg, sa_auth, c, &sha);
                    if (ret == OK) {
                        a_pad(u8, hex, 40);
                        a_rawc(rs, sha);
                        HEXu8sFeedSome(hex_idle, rs);
                        fprintf(stderr, "sniff: commit " U8SFMT "\n",
                                u8sFmt(u8bDataC(hex)));
                    }
                } else {
                //  No commit_msg + label_uri (`be post ?<br>`).  Per
                //  https://replicated.wiki/html/wiki/POST.html §POST `?branch` row: "FF-advance ?branch
                //  to cur's tip".  This is the local-branch promote
                //  — we move target's REFS row forward to cur.tip,
                //  migrate any commit/tree/blob objects from cur's
                //  shard into target's shard along the way, and leave
                //  cur untouched.  Refuses if cur is not a descendant
                //  of target.tip (POSTNOFF) or if the rebase produces
                //  a conflict.
                //
                //  (Remote-target shapes — `//host`, `ssh://...` —
                //  short-circuited above via POSTRebaseOntoSha.  Those
                //  pull remote work into cur, which is the inverse
                //  direction.)
                //  URI-001 Stage 4b: the query IS the target branch —
                //  the located `?<branch>/<sha>` form is retired, so
                //  there is no trailing pin to split off here.
                u8cs t_br = {};
                u8csMv(t_br, label_uri->query);
                //  Normalise trunk-targets: `?` (empty) and `?/` (one
                //  slash) both mean trunk.  Reduce to empty so
                //  POSTPromote's trunk-leaf path fires cleanly.
                if (u8csLen(t_br) == 1 && t_br[0][0] == '/') {
                    static u8c const _z = 0;
                    t_br[0] = (u8cp)&_z;
                    t_br[1] = (u8cp)&_z;
                }
                a_dup(u8c, tb, t_br);
                a_cstr(post_tag, "post");
                ret = POSTPromote(tb, NO, post_tag);
                }
            }
        }
    } else if (is_put) {
        //  Split URIs by aspect (https://replicated.wiki/html/wiki/PUT.html §PUT):
        //    * `?branch` (query, no path) → PUTCreateBranch (create
        //      label at cur.tip; refuses with PUTDUP if exists).
        //    * `?branch#<sha>` (query + sha fragment) → PUTSetBranch
        //      (reset the ref to that sha; non-FF rewrite allowed).
        //    * `?#<sha>` (empty query + sha fragment) → trunk reset.
        //      data must start with `?` so a fragment-only `#<sha>`
        //      (which goes through DOGNormalizeArg differently) isn't
        //      mistaken for a PUT-on-trunk.
        //    * `./path` / bare path → PUTStage (stage file/dir).
        //  Mixed invocations process each in arrival order; first
        //  failure aborts.
        uri path_uris[CLI_MAX_URIS] = {};
        u32 npath = 0;
        for (u32 i = 0; i < CLIUriLen(c) && ret == OK; i++) {
            uri u = {};
            call(CLIUriAt, &u, c, i);
            b8 has_q     = !u8csEmpty(u.query);
            b8 has_path  = !u8csEmpty(u.path);
            b8 has_frag  = !u8csEmpty(u.fragment);
            b8 has_auth  = !u8csEmpty(u.authority);
            //  Trunk reset: `?#<sha>` — empty query, hex fragment,
            //  no path, no authority, data starts with `?`.  Accept
            //  6..40 hex (hashlet prefix); resolve to full 40-hex
            //  via GRAFResolveRef so the REFS row carries a canonical
            //  sha (PUTSetBranch insists on full 40-hex).
            b8 trunk_reset = !has_q && !has_path && !has_auth &&
                             has_frag &&
                             !$empty(u.data) && u.data[0][0] == '?' &&
                             u8csLen(u.fragment) >= 6 &&
                             u8csLen(u.fragment) <= 40 &&
                             HEXu8sValid(u.fragment);
            if (trunk_reset) {
                a_dup(u8c, frag, u.fragment);
                //  Canonicalise the user-typed fragment via the
                //  single front-door resolver (handles both full
                //  40-hex shas and 6..39 hashlet prefixes).
                sha1hex full = {};
                ok64 rr = KEEPResolveHex(&full, frag);
                if (rr != OK) {
                    fprintf(stderr,
                        "sniff: put: cannot resolve ?#%.*s\n",
                        (int)$len(frag), (char *)frag[0]);
                    ret = SNIFFFAIL;
                    break;
                }
                a_rawc(full_s, full);
                //  PUTSetBranch's sane() check rejects all-NULL
                //  slices ($ok); pass a zero-length slice with valid
                //  pointers so it represents "trunk" (empty branch
                //  path).  refkey ends up as bare `?`.
                static u8c const _z = 0;
                u8cs empty_branch = {(u8cp)&_z, (u8cp)&_z};
                ret = PUTSetBranch(reporoot, empty_branch, full_s);
                continue;
            }
            //  `?<40-hex>` (no fragment) — user typed a bare full sha
            //  as the query.  No real branch name is 40 hex chars long,
            //  so the only sane interpretation is "set cur's tip to
            //  this sha".  Rewrite to PUTSetBranch on cur's own branch.
            //  Shorter hashlets are NOT caught here — they may collide
            //  with legit branch names (`abc123`); the user can disambiguate
            //  with `?<sha>/` (forbidden, sha not branch) or use `?#<sha>`
            //  explicitly.
            if (has_q && !has_path && !has_auth && !has_frag &&
                DOGIsFullSha(u.query)) {
                a_dup(u8c, qhex, u.query);
                sha1hex full = {};
                ok64 rr = KEEPResolveHex(&full, qhex);
                if (rr != OK) {
                    fprintf(stderr,
                        "sniff: put: cannot resolve ?%.*s\n",
                        (int)$len(qhex), (char *)qhex[0]);
                    ret = SNIFFFAIL;
                    break;
                }
                a_rawc(full_s, full);
                //  Resolve cur's branch via SNIFFAtCurTip (empty branch
                //  = trunk).  PUTSetBranch accepts an empty-but-valid
                //  slice for trunk; we mirror the trunk_reset arm above.
                a_pad(u8, curbuf, 256);
                {
                    ron60 ts = 0, verb = 0;
                    uri cu = {};
                    if (SNIFFAtCurTip(&ts, &verb, &cu) == OK) {
                        u8cs br = {};
                        DOGQueryBranchOnly(cu.query, br);
                        if (!u8csEmpty(br)) u8bFeed(curbuf, br);
                    }
                }
                a_dup(u8c, cur_target, u8bData(curbuf));
                if (u8csEmpty(cur_target)) {
                    static u8c const _z = 0;
                    u8cs empty_branch = {(u8cp)&_z, (u8cp)&_z};
                    ret = PUTSetBranch(reporoot, empty_branch, full_s);
                } else {
                    ret = PUTSetBranch(reporoot, cur_target, full_s);
                }
                continue;
            }
            if (has_q && !has_path && !has_auth) {
                a_pad(u8, abs_qbuf,    256);
                a_pad(u8, abs_databuf, 260);
                if (SNIFFAtResolveRelativeURI(&u, abs_qbuf, abs_databuf,
                                      NULL) != OK) {
                    ret = SNIFFFAIL;
                    break;
                }
                a_dup(u8c, target, u.query);
                //  `?br#<sha>` aspect (https://replicated.wiki/html/wiki/PUT.html §PUT): the fragment
                //  pins the target sha — write the ref to that value,
                //  bypassing the create-only PUTDUP check.  Non-FF
                //  rewrite is allowed.  Short hashlets (6..40 hex)
                //  resolve via GRAFResolveRef to a canonical 40-hex.
                //  Bare `?br` (no fragment) keeps the create-only
                //  PUTCreateBranch path.
                a_dup(u8c, frag, u.fragment);
                if (u8csLen(frag) >= 6 && u8csLen(frag) <= 40 &&
                    HEXu8sValid(frag)) {
                    //  Canonicalise the hashlet/sha through the
                    //  single front-door resolver.
                    sha1hex full = {};
                    ok64 rr = KEEPResolveHex(&full, frag);
                    if (rr != OK) {
                        fprintf(stderr,
                            "sniff: put: cannot resolve ?%.*s#%.*s\n",
                            (int)$len(target), (char *)target[0],
                            (int)$len(frag), (char *)frag[0]);
                        ret = SNIFFFAIL;
                        break;
                    }
                    a_rawc(full_s, full);
                    ret = PUTSetBranch(reporoot, target, full_s);
                } else {
                    ret = PUTCreateBranch(reporoot, target);
                }
                continue;
            }
            //  Path / bare — defer to PUTStage.
            if (npath < CLI_MAX_URIS) path_uris[npath++] = u;
        }
        if (ret == OK && (npath > 0 || CLIUriLen(c) == 0)) {
            //  PUT.c prints its own staged-row count.
            ret = PUTStage(npath, path_uris);
        }
    } else if (is_delete) {
        //  Two URI shapes:
        //    * branch-form (`?branch`) — drop the label via REFS
        //      tombstone; safety-checked by DELBranch.
        //    * path-form (`<file>`)    — stage a file removal.
        //  Bare `sniff delete` (no URIs) is the legacy "sweep
        //  missing tracked files" path; route through DELStage.
        //
        //  Path-forms are batched into ONE DELStage call so the
        //  trailing summary line appears once for the whole
        //  invocation; branch-forms each go through DELBranch
        //  individually (independent ref ops).
        if (CLIUriLen(c) == 0) {
            ret = DELStage(0, NULL);
        } else {
            uri path_uris[CLI_MAX_URIS];
            u32 npath = 0;
            for (u32 i = 0; i < CLIUriLen(c) && ret == OK; i++) {
                uri uv = {};
                call(CLIUriAt, &uv, c, i);
                uri *u = &uv;
                //  Branch-form is signalled by a literal leading `?`
                //  in the original token (u->data).  Bare tokens like
                //  `a.txt` also land in u->query via DOGNormalizeArg
                //  but their data has no `?` sigil — those are path-
                //  form deletes.
                b8 branch_form = !$empty(u->data) && u->data[0][0] == '?';
                if (branch_form) {
                    a_pad(u8, del_qbuf,    256);
                    a_pad(u8, del_databuf, 260);
                    if (SNIFFAtResolveRelativeURI(u, del_qbuf, del_databuf,
                                          NULL) != OK) {
                        ret = SNIFFFAIL; break;
                    }
                    b8 recursive = CLIHas(c, "-r") || CLIHas(c, "--force");
                    ret = DELBranch(u, recursive);
                } else {
                    if (npath < CLI_MAX_URIS) path_uris[npath++] = *u;
                }
            }
            if (ret == OK && npath > 0)
                ret = DELStage(npath, path_uris);
        }
    } else if (is_checkout) {
        if (CLIUriLen(c) < 1) {
            if ($eq(c->verb, v_get)) {
                ret = SNIFFGetSummary(reporoot);
            } else {
                fprintf(stderr,
                    "sniff: checkout requires a URI or hex\n");
                ret = SNIFFFAIL;
            }
        } else {
            uri uv = {};
            call(CLIUriAt, &uv, c, 0);
            uri *u = &uv;
            if ($eq(c->verb, v_get)) {
                ret = SNIFFGetURI(reporoot, u);
            } else {
                u8cs hex = {};
                if (!$empty(u->path))
                    u8csMv(hex, u->path);
                else
                    u8csMv(hex, u->data);
                ret = SNIFFCheckout(reporoot, hex);
            }
        }
    } else if (is_patch) {
        if (CLIUriLen(c) < 1) {
            fprintf(stderr,
                "sniff: patch requires a URI (query = ref or sha)\n");
            ret = SNIFFFAIL;
        } else {
            uri uv = {};
            call(CLIUriAt, &uv, c, 0);
            uri *u = &uv;
            //  One URI = one shell token: the merge message rides in
            //  the same token as the query (`be patch '?feat#merge msg'`),
            //  never a separate `'#msg'` arg.  No cross-token fragment
            //  coalescing — URILexer already lexes the spaced fragment.
            //  Accept `path?query` for single-file merge, bare
            //  `?query` (with optional `#hash` clamp) for whole-wt
            //  merge, or bare `#hash` for single-commit cherry-pick.
            //  Transport-form URIs (authority + scheme present) carry
            //  the remote's path in `u->path`; that's NOT a local file
            //  path and must route through the whole-tree PATCHApply.
            if (!$empty(u->path) && !$empty(u->query) &&
                $empty(u->authority)) {
                a_dup(u8c, path,  u->path);
                a_dup(u8c, query, u->query);
                a_dup(u8c, frag,  u->fragment);
                ret = PATCHApplyFile(reporoot, path, query, frag);
            } else if ((u->query[0] != NULL) ||
                       (u->fragment[0] != NULL)) {
                //  Pass the URI directly so the present-empty
                //  fragment marker (`?br#` rebase-one shape)
                //  survives.  PATCHApply classifies via PATCHShape
                //  and writes the appropriate row variant.
                ret = PATCHApply(reporoot, u);
            } else {
                fprintf(stderr,
                    "sniff: patch URI must have `?<ref|sha>` "
                    "or `#<sha>`\n");
                ret = SNIFFFAIL;
            }
        }
    } else if (is_submount) {
        //  `sniff sub-mount <subpath>#<40-hex-pin>` — invoked by
        //  beagle's BEGet wrapper for declared-but-not-mounted
        //  submodules.  Reads `.gitmodules` from the parent wt,
        //  looks up the URL, mkdirs the mount point, writes the
        //  secondary-wt anchor, fetches the sub's pack into the
        //  shared keeper, and forks `sniff get <pin>` inside the
        //  mount to check out at the pin.  See SUBS.plan.md §GET.
        //
        //  Running here (post-parent-checkout) gives us a clean
        //  keeper state: the parent's get has already released
        //  its write lock, so WIREFetchAll's writes land cleanly
        //  in the trunk shard instead of getting tangled in a
        //  branch-shard mid-transaction (the get/12 bug).
        uri smv = {};
        if (CLIUriLen(c) > 0) (void)CLIUriAt(&smv, c, 0);
        if (CLIUriLen(c) < 1 || $empty(smv.path) ||
            u8csLen(smv.fragment) != 40) {
            fprintf(stderr,
                "sniff: sub-mount requires `<subpath>#<40-hex-pin>`\n");
            ret = SNIFFFAIL;
        } else {
            uri *u = &smv;
            a_path(gm_path);
            u8cs gmrel = {(u8c *)".gitmodules",
                          (u8c *)".gitmodules" + 11};
            ok64 pe = SNIFFFullpath(gm_path, reporoot, gmrel);
            if (pe != OK) {
                fprintf(stderr, "sniff: sub-mount: no .gitmodules\n");
                ret = SNIFFFAIL;
            } else {
                u8bp gm_map = NULL;
                ok64 me = FILEMapRO(&gm_map, $path(gm_path));
                if (me != OK || gm_map == NULL) {
                    fprintf(stderr,
                        "sniff: sub-mount: cannot map .gitmodules\n");
                    ret = SNIFFFAIL;
                } else {
                    u8cs gm_blob = {u8bDataHead(gm_map),
                                    u8bIdleHead(gm_map)};
                    a_dup(u8c, parent_root_s, u8bDataC(HOME.root));
                    u8cs path_s = {};
                    u8csMv(path_s, u->path);
                    //  Strip the `./` URI-form prefix that the caller
                    //  uses to keep the path in the URI's path slot.
                    if (u8csLen(path_s) >= 2 &&
                        path_s[0][0] == '.' && path_s[0][1] == '/')
                        path_s[0] += 2;
                    u8cs hex_s = {};
                    u8csMv(hex_s, u->fragment);
                    a$rg(argv0, 0);
                    //  GET-011: the in-flight `be get` source URI (the
                    //  remote we are actually cloning from) rides in
                    //  `--source`; SubMount builds the PRIMARY sub-fetch
                    //  candidate from it.  Empty when absent (git parent).
                    u8cs src_uri = {};
                    CLIFlag(src_uri, c, "--source");
                    ret = SNIFFSubMount(reporoot, parent_root_s,
                                        path_s, hex_s, gm_blob, argv0,
                                        src_uri);
                    FILEUnMap(gm_map);
                }
            }
        }
    } else if (is_watch) {
        ret = SNIFFWatch(reporoot);
    } else if (is_status) {
        ret = sniff_status(reporoot);
    } else if (is_projector) {
        //  URI scheme picks the projector.  Output mode (TLV / plain /
        //  color) is set once at process start via the universal
        //  `--tlv` / `--color` / `--plain` rule into the module-global
        //  `HUNKMode`; projectors only emit hunks.
        a_cstr(ls_s,     "ls");
        a_cstr(lsr_s,    "lsr");
        a_cstr(cat_s,    "cat");
        a_cstr(status_s, "status");
        if ($eq(proj_u->scheme, ls_s)) {
            ret = SNIFFLs(reporoot, proj_u);
        } else if ($eq(proj_u->scheme, lsr_s)) {
            ret = SNIFFLsr(reporoot, proj_u);
        } else if ($eq(proj_u->scheme, cat_s)) {
            ret = SNIFFCat(reporoot, proj_u);
        } else if ($eq(proj_u->scheme, status_s)) {
            //  `status:` projector → same one-hunk worktree status
            //  bare `sniff` produces.  Body slot ignored.
            ret = sniff_status(reporoot);
        } else {
            //  Table says sniff owns this scheme but we don't have a
            //  handler wired — should not happen once DOG_PROJECTORS
            //  and this switch are kept in sync.  Fail loudly.
            fprintf(stderr, "sniff: projector '%.*s:' not implemented\n",
                    (int)$len(proj_u->scheme), (char *)proj_u->scheme[0]);
            ret = SNIFFFAIL;
        }
    } else if (is_update) {
        //  No per-path mtime cache in the new model; `update` is a no-op.
        //  Left in the verb table so existing scripts don't break.
        ret = OK;
    } else {
        // Default: index (no-op in the new model; retained for script compat).
        ret = OK;
    }

    return ret;
}
