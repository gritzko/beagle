//  CAT — `cat:<path>[?<ref>]` projector.  See CAT.h.
//
//  Builds the same 2-layer weave as `diff:` (graf/DIFFREF.c) — `from`
//  = baseline blob, `to` = wt bytes — then emits **every** alive
//  token via `WEAVEEmitFull` (no context-windowing).  One hunk per
//  file; bro pages it.

#include "CAT.h"

#include <string.h>

#include "abc/B.h"
#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/MMAP.h"
#include "abc/PATH.h"
#include "abc/PRO.h"

#include "dog/HUNK.h"

#include "abc/FILE.h"

#include "graf/GRAF.h"
#include "graf/WEAVE.h"

#include "keeper/KEEP.h"
#include "keeper/WALK.h"

#include "AT.h"
#include "SNIFF.h"

#define WEAVE_BASE_SRC 1u

static b8 cat_in_from(u32 c, void *ctx) { (void)ctx; return c != WEAVE_WT_SRC; }
static b8 cat_in_to  (u32 c, void *ctx) { (void)ctx; (void)c; return YES; }

//  Hunk sink — `GRAFHunkEmit` would silently drop bytes here because
//  `graf_emit`/`graf_out_fd` are wired only inside graf's own CLI.
//  Route directly through the shared `HUNKu8sFeedOut`, which reads the
//  global `HUNKMode` (set once by main from `--tlv` / `--color` /
//  `--plain`) and picks the right encoder.
static ok64 cat_hunk_emit(hunkc *hk, void *ctx) {
    (void)ctx;
    sane(hk != NULL);
    //  Per-hunk scratch sized for one rendered hunk; bro's pager
    //  ingests these incrementally, so we never have to buffer the
    //  whole file at once.  WEAVEEmitFull caps each hunk at 1 MiB.
    Bu8 line = {};
    call(u8bAllocate, line, (1UL << 20) + (1UL << 16));
    ok64 fo = HUNKu8sFeedOut(u8bIdle(line), hk);
    if (fo == OK) (void)FILEout(u8bDataC(line));
    u8bFree(line);
    done;
}

// =====================================================================
//  Baseline blob resolution
//
//  Two paths:
//    ?ref present  → KEEPResolveTree → descend to <path> → KEEPGetExact
//    ?ref empty    → SNIFFAtCurTip → commit's tree → descend → blob
//
//  Returns OK with bytes in *out, or KEEPNONE if no baseline exists
//  (untracked / fresh repo).  Caller distinguishes by emptiness.
// =====================================================================

static ok64 cat_resolve_baseline(uricp u, Bu8 out) {
    sane(u);
    sha1 root_tree = {};
    if (!u8csEmpty(u->query)) {
        call(KEEPResolveTree, u, &root_tree);
    } else {
        ron60 ts = 0, verb = 0;
        uri base = {};
        ok64 br = SNIFFAtCurTip(&ts, &verb, &base);
        if (br == ULOGNONE) return KEEPNONE;     // fresh repo
        if (br != OK) return br;
        sha1hex hex = {};
        if (SNIFFAtQueryFirstSha(&base, &hex) != OK) return KEEPNONE;
        sha1 commit = {};
        if (sha1FromSha1hex(&commit, &hex) != OK) return KEEPNONE;
        call(KEEPCommitTreeSha, &commit, &root_tree);
    }

    //  Descend `u->path` inside the tree.  KEEPGetByPath does the
    //  walk-and-fetch in one call; reuse it.
    uri probe = {};
    probe.path[0]     = u->path[0];
    probe.path[1]     = u->path[1];
    sha1hex hex = {};
    sha1hexFromSha1(&hex, &root_tree);
    sha1hexSlice(probe.fragment, &hex);
    return KEEPGetByURI((uricp)&probe, out);
}

// =====================================================================
//  Entry point
// =====================================================================

ok64 SNIFFCat(u8cs reporoot, uri const *u) {
    sane(u);
    if ($empty(u->path)) {
        //  `cat:` with no path is meaningless — surface as a hard error
        //  rather than emitting an empty hunk silently.
        fail(SNIFFFAIL);
    }

    //  Load baseline (may be empty for untracked / fresh-repo).
    Bu8 base = {};
    call(u8bAllocate, base, 64UL << 20);
    ok64 br = cat_resolve_baseline(u, base);
    u8cs from_data = {};
    if (br == OK) { a_dup(u8c, fd, u8bData(base)); u8csMv(from_data, fd); }
    else if (br != KEEPNONE) { u8bFree(base); return br; }

    //  Load wt bytes (may be empty for a tracked-but-rm'd file).
    a_path(wt_path);
    call(SNIFFFullpath, wt_path, reporoot, u->path);
    u8bp wt_mapped = NULL;
    u8cs to_data = {};
    ok64 wto = FILEMapRO(&wt_mapped, $path(wt_path));
    if (wto == OK && wt_mapped) {
        a_dup(u8c, td, u8bData(wt_mapped));
        u8csMv(to_data, td);
    }

    u8cs ext = {};
    PATHu8sExt(ext, u->path);

    //  Build the same 2-layer weave `diff:` uses, then call the
    //  no-windowing emitter.  Reuses graf's arena conventions —
    //  GRAFArenaInit/Cleanup is the same pair GRAFDiff2Layer wraps.
    call(GRAFArenaInit);

    weave wA = {}, wB = {}, wnu = {};
    if (WEAVEInit(&wA)  != OK ||
        WEAVEInit(&wB)  != OK ||
        WEAVEInit(&wnu) != OK) {
        WEAVEFree(&wA); WEAVEFree(&wB); WEAVEFree(&wnu);
        GRAFArenaCleanup();
        if (wt_mapped) FILEUnMap(wt_mapped);
        u8bFree(base);
        fail(NOROOM);
    }

    ok64 ret = WEAVEFromBlob(&wA, from_data, ext, WEAVE_BASE_SRC);
    if (ret == OK) ret = WEAVEFromBlob(&wnu, to_data, ext, WEAVE_WT_SRC);
    if (ret == OK) ret = WEAVEDiff(&wB, &wA, &wnu, WEAVE_WT_SRC);
    if (ret == OK) {
        ret = WEAVEEmitFull(&wB, u->path,
                            cat_in_from, NULL,
                            cat_in_to,   NULL,
                            cat_hunk_emit, NULL);
    }

    WEAVEFree(&wA); WEAVEFree(&wB); WEAVEFree(&wnu);
    GRAFArenaCleanup();
    if (wt_mapped) FILEUnMap(wt_mapped);
    u8bFree(base);
    return ret;
}
