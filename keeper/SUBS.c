//  KEEPSubsAt — see SUBS.h.

#include "SUBS.h"

#include "abc/BUF.h"
#include "abc/HEX.h"
#include "abc/PRO.h"
#include "abc/RON.h"
#include "abc/S.h"
#include "abc/URI.h"

#include "dog/DOG.h"          // DOG_OBJ_TREE / DOG_OBJ_BLOB
#include "dog/git/GIT.h"      // GITu8sDrainTree / GITu8sFileSplit

#include "KEEP.h"

// --- Tree-step helper.  Look up `name` in `tree_sha`'s entries; on
//     hit, fill `*out_sha` from the entry and `*out_mode` from the
//     parsed octal mode.  Returns KEEPNONE if no entry, KEEPFAIL on
//     malformed tree, OK on success.

//  `tbuf` is a caller-provided scratch buffer reused across every
//  KEEPGetExact (KEEPGet internally resets, so no manual reset).
static ok64 keep_subs_tree_step(sha1cp tree_sha, u8cs name,
                                sha1 *out_sha, u32 *out_mode, u8bp tbuf) {
    sane(tree_sha && out_sha && tbuf);

    u8 otype = 0;
    call(KEEPGetExact, tree_sha, tbuf, &otype);
    if (otype != DOG_OBJ_TREE) fail(KEEPFAIL);

    a_dup(u8c, body, u8bDataC(tbuf));
    u8cs field = {}, esha = {};
    u32 mode = 0;
    ok64 result = KEEPNONE;
    while (GITu8sDrainTree(body, field, esha, &mode) == OK) {
        u8cs entry_name = {};
        if (GITu8sFileSplit(field, NULL, entry_name) != OK) continue;
        if (!u8csEq(entry_name, name)) continue;
        ok64 dr = sha1Drain(esha, out_sha);
        if (dr != OK) fail(KEEPFAIL);
        if (out_mode) *out_mode = mode;
        result = OK;
        break;
    }
    return result;
}

//  Walk `path` (slash-separated segments) starting at `root_tree`
//  through nested tree steps; on success fills the final entry's sha
//  + mode.  Returns KEEPNONE if any segment misses.  `tbuf` is shared
//  scratch passed down to each `keep_subs_tree_step`.
static ok64 keep_subs_walk_path(sha1cp root_tree, u8cs path,
                                sha1 *out_sha, u32 *out_mode, u8bp tbuf) {
    sane(root_tree && out_sha && tbuf);
    sha1 cur = *root_tree;
    u8cs rest = {path[0], path[1]};
    while (!u8csEmpty(rest)) {
        a_dup(u8c, scan, rest);
        b8 had_slash = (u8csFind(scan, '/') == OK);  // scan[0] = '/' if found
        u8cs seg = {rest[0], scan[0]};
        rest[0] = scan[0];
        if (had_slash) u8csUsed1(rest);              // step past the '/'
        if (u8csEmpty(seg)) continue;
        sha1 step = {};
        u32  mode = 0;
        call(keep_subs_tree_step, &cur, seg, &step, &mode, tbuf);
        cur = step;
        if (out_mode) *out_mode = mode;
    }
    *out_sha = cur;
    done;
}

// --- ULOG row emission via SUBSu8sParse callback ----------------

typedef struct {
    sha1cp tree_sha;
    u8bp        out;
    u8bp        tbuf;       // shared 1 MiB scratch for tree fetches
    ron60       ts;
    ron60       verb;
    ok64        err;        // sticky write error
} keep_subs_emit_ctx;

//  Emit one ULOG row for a sub that resolves to a 160000 entry.
//  Row: `<ts>\t<verb>\t<url>?<mount-path>#<hex-pin>\n`.
//
//  The URL is decomposed via URILexer into its native scheme / auth
//  / path slots; the mount path lands in the query slot; the pin is
//  the fragment.
static ok64 keep_subs_emit_row(keep_subs_emit_ctx *kc, u8cs path,
                               u8cs url, sha1cp pin) {
    sane(kc && pin);
    //  ts + verb in RON64.
    call(RONutf8sFeed, u8bIdle(kc->out), kc->ts);
    call(u8bFeed1,     kc->out, '\t');
    call(RONutf8sFeed, u8bIdle(kc->out), kc->verb);
    call(u8bFeed1,     kc->out, '\t');

    //  Parse URL into scheme/auth/path.  URILexer fills the row uri
    //  slots in-place; any query/fragment the URL itself carries is
    //  ignored — the row's query slot is for the mount path.
    uri row = {};
    u8csMv(row.data, url);
    call(URILexer, &row);
    //  Mount path overrides any URL-side query (none expected in
    //  `.gitmodules` URLs in practice; if present, we drop it on the
    //  floor — the mount path is the row's per-path key).
    u8csMv(row.query, path);

    //  Pin as 40-hex fragment, allocated on the stack of this frame
    //  so the slice in row.fragment stays valid through URIutf8Feed.
    a_sha1hex(hex_view, pin);
    u8csMv(row.fragment, hex_view);

    call(URIutf8Feed, u8bIdle(kc->out), &row);
    call(u8bFeed1, kc->out, '\n');
    done;
}

//  SUBSu8sParse callback: resolve pin, emit row.  Skip sections
//  whose mount path doesn't resolve to a 160000 gitlink — the tree,
//  not `.gitmodules`, is authoritative for the live mount set.
static ok64 keep_subs_step(u8cs path, u8cs url, void *vctx) {
    keep_subs_emit_ctx *kc = (keep_subs_emit_ctx *)vctx;
    if (kc->err != OK) return kc->err;

    sha1 pin = {};
    u32  mode = 0;
    ok64 wo = keep_subs_walk_path(kc->tree_sha, path, &pin, &mode, kc->tbuf);
    if (wo == KEEPNONE) return OK;        //  declared but not in tree
    if (wo != OK)       return wo;
    if (mode != 0160000) return OK;       //  not a gitlink — skip

    ok64 r = keep_subs_emit_row(kc, path, url, &pin);
    if (r != OK) kc->err = r;
    return r;
}

//  Worker: assumes `tbuf` and `bbuf` are pre-allocated by the wrapper.
static ok64 keep_subs_at_inner(sha1cp tree_sha, ron60 ts, ron60 verb,
                               u8bp out, u8bp tbuf, u8bp bbuf) {
    sane(tree_sha && out && tbuf && bbuf);
    u8bReset(out);

    //  Find `.gitmodules` in the root tree.  Absence is signalled as
    //  OK with an empty out — same shape as KEEPTreeULog's empty-tree
    //  case, so merge-walks compose cleanly.
    sha1 gm_sha = {};
    u32  gm_mode = 0;
    a_cstr(gm_name, ".gitmodules");
    a_dup(u8c, gm, gm_name);
    ok64 lo = keep_subs_tree_step(tree_sha, gm, &gm_sha, &gm_mode, tbuf);
    if (lo == KEEPNONE) done;
    if (lo != OK)       return lo;
    if (gm_mode != 0100644 && gm_mode != 0100755) done;

    //  Pull the blob bytes into `bbuf`.
    u8 btype = 0;
    call(KEEPGetExact, &gm_sha, bbuf, &btype);
    if (btype != DOG_OBJ_BLOB) fail(KEEPFAIL);

    //  Drive the parser; per declared (path, url) resolve the pin
    //  and emit one ULOG row.  `tbuf` is shared scratch for nested
    //  tree-step lookups; safe because the parser callback runs to
    //  completion before returning.
    keep_subs_emit_ctx kc = {
        .tree_sha = tree_sha,
        .out      = out,
        .tbuf     = tbuf,
        .ts       = ts,
        .verb     = verb,
        .err      = OK,
    };
    a_dup(u8c, blob, u8bData(bbuf));
    ok64 po = SUBSu8sParse(blob, keep_subs_step, &kc);
    if (kc.err != OK) return kc.err;
    return po;
}

ok64 KEEPSubsAt(sha1cp tree_sha, ron60 ts, ron60 verb, u8bp out) {
    sane(tree_sha && out);
    a_carve(u8, tbuf, 1UL << 20);
    a_carve(u8, bbuf, 1UL << 20);
    call(keep_subs_at_inner, tree_sha, ts, verb, out, tbuf, bbuf);
    done;
}
