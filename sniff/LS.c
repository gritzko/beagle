#include "LS.h"

#include <stdio.h>
#include <string.h>

#include "abc/B.h"
#include "abc/BUF.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/RON.h"

#include "dog/HUNK.h"
#include "dog/ULOG.h"
#include "keeper/KEEP.h"
#include "keeper/WALK.h"

#include "AT.h"
#include "SNIFF.h"

// --- Line emit helper ---

//  Append one listing line: `<path>` + '/' if is_dir + '\n'.  Swallows
//  u8bFeed errors: the buffer is pre-sized in SNIFFLs and all call
//  sites are additive, so a failed append means "too big" and we drop
//  the line rather than abort the whole listing.
static void ls_emit_line(Bu8 out, u8cs path, b8 is_dir) {
    if ($empty(path)) return;
    (void)u8bFeed(out, path);
    if (is_dir) (void)u8bFeed1(out, '/');
    (void)u8bFeed1(out, '\n');
}

// --- Mode: wt listing via SNIFFWtULog ---
//
//  Walk the wt as a sorted ULOG row buffer (`SNIFFWtULog`) and emit
//  one line per file.  `prefix` (possibly empty) filters to entries
//  whose path starts with it — recursive listing under the subdir,
//  matching `git ls-files <prefix>` shape.

static ok64 ls_wt(Bu8 out, u8cs prefix) {
    sane(1);
    a_dup(u8c, root, u8bDataC(SNIFF.h->wt));

    a_cstr(s_wt, "wt"); a_dup(u8c, dw, s_wt);
    ron60 v_wt = 0;
    call(RONutf8sDrain, &v_wt, dw);

    Bu8 wu = {};
    call(u8bAllocate, wu, 1UL << 20);
    ok64 wo = SNIFFWtULog(root, v_wt, wu);
    if (wo != OK) { u8bFree(wu); return wo; }

    a_dup(u8c, scan, u8bData(wu));
    while (!u8csEmpty(scan)) {
        ulogrec rec = {};
        ok64 dr = ULOGu8sDrain(scan, &rec);
        if (dr == NODATA) break;
        if (dr != OK) continue;
        u8cs path = {rec.uri.path[0], rec.uri.path[1]};
        if ($empty(path)) continue;
        if (!$empty(prefix)) {
            if ((size_t)$len(path) < (size_t)$len(prefix)) continue;
            if (memcmp(path[0], prefix[0],
                       (size_t)$len(prefix)) != 0) continue;
        }
        ls_emit_line(out, path, NO);
    }
    u8bFree(wu);
    done;
}

// --- Mode: tree at a ref (via keeper) ---

typedef struct {
    u8 **out_buf;   // == &Bu8[0], usable as u8b arg to u8bFeed
} ls_tree_ctx;

static ok64 ls_tree_visit(u8cs path, u8 kind, u8cp esha,
                           u8cs blob, void0p ctx) {
    (void)esha; (void)blob;
    ls_tree_ctx *c = (ls_tree_ctx *)ctx;
    //  Skip the root tree's self-visit (empty path).  Dirs get a
    //  trailing '/'; files verbatim — matching wt-mode shape.
    if ($empty(path)) return OK;
    b8 is_dir = (kind == WALK_KIND_DIR);
    ls_emit_line((u8 *const *)c->out_buf, path, is_dir);
    return OK;
}

static ok64 ls_ref(Bu8 out, uri const *u) {
    sane(u);
    keeper *k = &KEEP;
    ls_tree_ctx ctx = {.out_buf = (u8 **)out};

    //  Bare `?` (present-but-empty query) = current branch tip.
    //  KEEPResolveTree's REFS lookup gets confused by the empty query;
    //  resolve it via sniff's ULOG baseline instead.  Construct a
    //  local URI with #<sha> fragment and hand that to KEEPLsFiles.
    if (!$empty(u->query)) {
        return KEEPLsFiles(k, u, ls_tree_visit, &ctx);
    }

    //  query present-but-empty: look up the baseline sha.
    ron60 ts = 0, verb = 0;
    uri bu = {};
    ok64 br = SNIFFAtBaseline(&ts, &verb, &bu);
    if (br != OK) {
        fprintf(stderr, "sniff: ls: no baseline; nothing checked out\n");
        fail(SNIFFFAIL);
    }
    sha1hex hex = {};
    if (SNIFFAtQueryFirstSha(&bu, &hex) != OK) {
        fprintf(stderr, "sniff: ls: baseline has no sha\n");
        fail(SNIFFFAIL);
    }
    uri tip = {};
    tip.path[0]     = u->path[0];
    tip.path[1]     = u->path[1];
    sha1hexSlice(tip.fragment, &hex);
    return KEEPLsFiles(k, &tip, ls_tree_visit, &ctx);
}

// --- URI-shape dispatch ---

//  Range URI (`?v1..v2`) — not yet implemented.  Recognised so we
//  can emit a useful diagnostic instead of silently treating
//  `v1..v2` as an opaque ref.  `..` inside a query is the agreed
//  range marker (VERBS.md §GET, dog/QURY).
static b8 uri_query_is_range(uri const *u) {
    if ($empty(u->query)) return NO;
    for (u8cp p = u->query[0]; p + 1 < u->query[1]; p++)
        if (p[0] == '.' && p[1] == '.') return YES;
    return NO;
}

ok64 SNIFFLs(u8cs reporoot, uri const *u, b8 tlv) {
    sane(u);
    (void)reporoot;

    Bu8 text = {};
    call(u8bAllocate, text, 1UL << 20);

    ok64 ret = OK;
    b8 has_query = (u->query[0] != NULL);
    b8 is_range  = uri_query_is_range(u);

    if (is_range) {
        fprintf(stderr,
                "sniff: ls: range (?v1..v2) diff-stat not yet "
                "implemented\n");
        ret = SNIFFFAIL;
    } else if (has_query) {
        ret = ls_ref(text, u);
    } else {
        a_dup(u8c, prefix, u->path);
        ret = ls_wt(text, prefix);
    }

    if (ret != OK) {
        u8bFree(text);
        return ret;
    }

    if (tlv) {
        //  Title URI for the hunk header bar: friendly label, not a
        //  protocol URI.  Shapes:
        //    "ls:"                         (wt, no filter)
        //    "ls:<prefix>"                 (wt subdir)
        //    "ls:?<ref>"                   (tree at ref)
        //    "ls:<prefix>?<ref>"           (subtree at ref)
        a_pad(u8, title, 512);
        a_cstr(ls_pfx, "ls:");
        (void)u8bFeed(title, ls_pfx);
        if (!$empty(u->path)) (void)u8bFeed(title, u->path);
        if (has_query) {
            (void)u8bFeed1(title, '?');
            (void)u8bFeed(title, u->query);
        }

        hunk hk = {};
        hk.uri[0]  = u8bDataHead(title);
        hk.uri[1]  = u8bIdleHead(title);
        hk.text[0] = u8bDataHead(text);
        hk.text[1] = u8bIdleHead(text);

        //  Emit into a scratch arena, then drop on stdout.  The TLV
        //  wrapper for a single listing hunk is bounded by the text
        //  size plus a few bytes of envelope; allocate 64 KiB of
        //  headroom over the text.
        size_t tlen = u8bDataLen(text);
        Bu8 outbuf = {};
        ok64 ao = u8bAllocate(outbuf, tlen + (1UL << 16));
        if (ao != OK) { u8bFree(text); return ao; }
        ok64 fo = HUNKu8sFeed(u8bIdle(outbuf), &hk);
        if (fo != OK) {
            u8bFree(outbuf);
            u8bFree(text);
            return fo;
        }
        fwrite(u8bDataHead(outbuf), 1, u8bDataLen(outbuf), stdout);
        fflush(stdout);
        u8bFree(outbuf);
    } else {
        fwrite(u8bDataHead(text), 1, u8bDataLen(text), stdout);
        fflush(stdout);
    }

    u8bFree(text);
    done;
}
