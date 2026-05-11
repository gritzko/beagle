//  REFADV: build + emit the git-protocol refs advertisement.
//
//  See REFADV.h for the API contract and WIRE.md Phase 3 for the
//  surrounding plan.

#include "REFADV.h"

#include <stdlib.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "keeper/GIT.h"
#include "keeper/PKT.h"
#include "keeper/REFS.h"

//  Capability list — see WIRE.md Phase 3.
//  No `side-band-64k`: without it git reads the raw pack after NAK
//  exactly as WIREServeUpload emits it (PSTRWrite writes unframed).
static u8c const REFADV_CAPS[] =
    "multi_ack_detailed ofs-delta agent=dogs-keeper";

//  Decode a REFS value into a sha1.  Canonical form is bare 40-hex;
//  also accept legacy `?<40-hex>` (41 chars) for read-path tolerance.
static b8 refadv_decode_terminal(sha1 *out, u8csc val) {
    u8cs hex = {val[0], val[1]};
    if (u8csLen(hex) == 41 && hex[0][0] == '?') u8csUsed(hex, 1);
    if (u8csLen(hex) != 40) return NO;
    a_dup(u8c, hex_dup, hex);
    u8s bin = {out->data, out->data + 20};
    if (HEXu8sDrainSome(bin, hex_dup) != OK) return NO;
    if (!u8sEmpty(bin)) return NO;
    if (!u8csEmpty(hex_dup)) return NO;
    return YES;
}

// --- iteration context ---

#define REFADV_MAX_ENTRIES REFS_MAX_REFS
#define REFADV_ARENA_BYTES (REFS_MAX_REFS * 256)

typedef struct {
    refadv *adv;
    b8      peer_pass;   // YES = second pass, only emit peer-prefixed
                         //       branches not yet in adv->ents
} refadv_ctx;

//  Helper: return YES when `name` is already in adv->ents.
static b8 refadv_branch_seen(refadv *adv, u8csc name) {
    for (u32 i = 0; i < adv->count; i++) {
        if (u8csEq(adv->ents[i].dir, (u8c **)name)) return YES;
    }
    return NO;
}

//  Per-ref callback fed by REFSEach.  Two row shapes contribute to
//  the advert:
//      Local-only:    `?<branch>`           — our own tip (pass 1).
//      Peer-observed: `<peer-uri>?<branch>` — relayed from a peer
//                     (pass 2; only emitted for branches not already
//                     covered by a local row).  A relay keeper —
//                     cloned from another keeper, no local commits —
//                     has only peer-observed rows; without
//                     re-advertising them the upload-pack response
//                     carries zero refs and the client fails with
//                     WIRECLNRF.
//
//  Wire mapping at this boundary (the only place the trunk⇔main
//  alias lives):
//      `?`         → refname `refs/heads/main`,  shard dir = ""
//      `?<X>`      → refname `refs/heads/<X>`,   shard dir = "<X>"
//
//  No tag handling — locally there is no tag/branch distinction; each
//  REFS row is just a branch shard tip.  (Re-introduce tag-as-tag
//  semantics with a separate verb / refkind field if/when needed.)
static ok64 refadv_each_cb(refcp r, void *vctx) {
    sane(r && vctx);
    refadv_ctx *ctx = (refadv_ctx *)vctx;
    refadv *adv = ctx->adv;

    a_dup(u8c, key, r->key);
    a_dup(u8c, val, r->val);

    if (u8csEmpty(key)) done;
    b8 is_local = (*key[0] == '?');
    if (ctx->peer_pass) {
        if (is_local) done;             //  pass 2: skip locals
    } else {
        if (!is_local) done;            //  pass 1: skip peers
    }

    //  Find the `?` separator and take everything after as branch.
    u8c *qmark = key[0];
    while (qmark < key[1] && *qmark != '?') qmark++;
    if (qmark == key[1]) done;          //  malformed: no `?`
    u8cs branch = {qmark + 1, key[1]};

    //  Pass 2: skip if a local row already covered this branch.
    if (ctx->peer_pass && refadv_branch_seen(adv, branch)) done;

    sha1 tip = {};
    if (!refadv_decode_terminal(&tip, val)) done;

    //  Branch-vs-tag classification: a `tags/X` prefix in the local
    //  row's query maps to a TAG advertisement (`refs/tags/X`); all
    //  other queries map to BRANCH (`refs/heads/X`).  Wire alias:
    //  empty branch (trunk) advertises as `main`.
    gitref_kind kind = GITREF_BRANCH;
    u8cs name = {};
    if (u8csEmpty(branch)) {
        u8csMv(name, GIT_MAIN_LIT);
    } else if (u8csLen(branch) > u8csLen(GIT_TAGS_PFX) &&
               u8csHasPrefix(branch, GIT_TAGS_PFX)) {
        kind = GITREF_TAG;
        u8cs nm = {branch[0] + u8csLen(GIT_TAGS_PFX), branch[1]};
        u8csMv(name, nm);
    } else {
        u8csMv(name, branch);
    }

    if (adv->count >= REFADV_MAX_ENTRIES) done;
    refadv_entry *ent = &adv->ents[adv->count];
    ent->tip = tip;

    //  Build refname into the arena via GITFeedRef so the wire form
    //  is built in exactly one place.
    u8 *refname_start = u8bIdleHead(adv->arena);
    {
        ok64 fo = GITFeedRef(adv->arena, kind, name);
        if (fo != OK) return fo;
    }
    ent->refname[0] = refname_start;
    ent->refname[1] = u8bIdleHead(adv->arena);

    //  Shard dir: trunk → ""; other → "<branch>".  Stored as an
    //  arena-owned slice so the caller can keep it past REFSEach.
    u8 *dir_start = u8bIdleHead(adv->arena);
    if (!$empty(branch)) {
        if (u8bIdleLen(adv->arena) < (size_t)u8csLen(branch))
            return BNOROOM;
        u8bFeed(adv->arena, branch);
    }
    ent->dir[0] = dir_start;
    ent->dir[1] = u8bIdleHead(adv->arena);

    adv->count++;
    done;
}

// --- Open / Close ---

ok64 REFADVOpen(refadv *out, keeper *k) {
    sane(out && k);

    out->ents  = NULL;
    out->count = 0;
    memset((void *)out->arena, 0, sizeof(out->arena));

    out->ents = calloc(REFADV_MAX_ENTRIES, sizeof(refadv_entry));
    if (!out->ents) fail(REFADVFAIL);

    ok64 ao = u8bAllocate(out->arena, REFADV_ARENA_BYTES);
    if (ao != OK) {
        free(out->ents);
        out->ents = NULL;
        return ao;
    }

    //  Phase 1c: only the trunk shard exists.  Walk its REFS at
    //  <root>/.be/REFS.  Future phases iterate every shard dir.
    a_path(keepdir, u8bDataC(k->h->root), KEEP_DIR_S);

    //  Two-pass: local rows first (authoritative), then peer-observed
    //  rows for branches not yet covered (relay role).
    refadv_ctx ctx = {.adv = out, .peer_pass = NO};
    ok64 eo = REFSEach($path(keepdir), refadv_each_cb, &ctx);
    if (eo != OK) { REFADVClose(out); return eo; }
    ctx.peer_pass = YES;
    eo = REFSEach($path(keepdir), refadv_each_cb, &ctx);
    if (eo != OK) { REFADVClose(out); return eo; }
    done;
}

void REFADVClose(refadv *adv) {
    if (!adv) return;
    if (adv->ents) {
        free(adv->ents);
        adv->ents = NULL;
    }
    if (adv->arena[0] != NULL) {
        u8bFree(adv->arena);
    }
    adv->count = 0;
}

// --- Tip → dirs lookup ---

u32 REFADVTipDirs(refadv const *adv, sha1 const *tip,
                  u8cs *out_dirs, u32 cap) {
    if (!adv || !tip || !out_dirs || cap == 0) return 0;
    u32 n = 0;
    for (u32 i = 0; i < adv->count && n < cap; i++) {
        if (sha1Eq(&adv->ents[i].tip, tip)) {
            out_dirs[n][0] = adv->ents[i].dir[0];
            out_dirs[n][1] = adv->ents[i].dir[1];
            n++;
        }
    }
    return n;
}

// --- Emit ---
//
//  Pkt-line line shape:
//    first  : "<40-hex-sha> <refname>\0<caps>\n"
//    others : "<40-hex-sha> <refname>\n"
//  Then a flush packet "0000".

//  Compose one ref-line payload (no pkt-line wrapper) into `out`.
//  `with_caps` adds a NUL + capability list before the trailing '\n'.
static ok64 refadv_format_line(u8bp out, refadv_entry const *e, b8 with_caps) {
    sane(out && e);
    sha1hex hex = {};
    sha1hexFromSha1(&hex, &e->tip);
    a_rawc(hex_full, hex);

    if (u8bIdleLen(out) < 40 + 1 + (size_t)u8csLen(e->refname) + 1 +
                          (with_caps ? 1 + sizeof(REFADV_CAPS) - 1 : 0))
        return BNOROOM;
    call(u8bFeed, out, hex_full);
    u8bFeed1(out, ' ');
    u8bFeed(out, e->refname);
    if (with_caps) {
        u8bFeed1(out, 0);
        u8csc caps = {REFADV_CAPS,
                      REFADV_CAPS + sizeof(REFADV_CAPS) - 1};
        u8bFeed(out, caps);
    }
    u8bFeed1(out, '\n');
    done;
}

ok64 REFADVEmit(int out_fd, refadv const *adv) {
    sane(out_fd >= 0 && adv);

    //  Worst-case framing: 4 (pkt prefix) + 40 (sha) + 1 (sp) + ~256
    //  (refname) + 1 (NUL) + caps + 1 (\n) per line, plus 4 for the
    //  flush.  Cap arena at PKT_MAX-aligned bound; realistic refnames
    //  are small.
    u64 cap = 64;  // flush + slack
    for (u32 i = 0; i < adv->count; i++) {
        cap += 4 + 40 + 1 + (u64)u8csLen(adv->ents[i].refname) + 1;
        if (i == 0) cap += 1 + sizeof(REFADV_CAPS) - 1;
    }

    Bu8 frame = {};
    call(u8bAllocate, frame, cap);

    a_pad(u8, line, PKT_MAX);

    for (u32 i = 0; i < adv->count; i++) {
        u8bReset(line);
        ok64 fo = refadv_format_line(line, &adv->ents[i], i == 0);
        if (fo != OK) { u8bFree(frame); return fo; }
        a_dup(u8c, payload, u8bData(line));
        ok64 po = PKTu8sFeed(u8bIdle(frame), payload);
        if (po != OK) { u8bFree(frame); return po; }
    }
    ok64 fo = PKTu8sFeedFlush(u8bIdle(frame));
    if (fo != OK) { u8bFree(frame); return fo; }

    a_dup(u8c, fdata, u8bData(frame));
    ok64 wo = FILEFeedAll(out_fd, fdata);
    u8bFree(frame);
    if (wo != OK) return wo;

    done;
}
