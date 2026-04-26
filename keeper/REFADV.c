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
    u8 buf[20] = {};
    u8s bin = {buf, buf + 20};
    if (HEXu8sDrainSome(bin, hex_dup) != OK) return NO;
    if (bin[0] != buf + 20) return NO;
    if (!u8csEmpty(hex_dup)) return NO;
    memcpy(out->data, buf, 20);
    return YES;
}

// --- iteration context ---

#define REFADV_MAX_ENTRIES REFS_MAX_REFS
#define REFADV_ARENA_BYTES (REFS_MAX_REFS * 256)

typedef struct {
    refadv *adv;
} refadv_ctx;

//  Per-ref callback fed by REFSEach.  Local rows have keys of the
//  form `?<branch-path>` (`?` for trunk).  Peer-prefixed rows
//  (`<peer-uri>?<branch>`) are observations of someone else's tip and
//  must NOT be advertised as ours — skip them.
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

    u8cs key = {r->key[0], r->key[1]};
    u8cs val = {r->val[0], r->val[1]};

    //  Local-only filter: a key that doesn't *start* with `?` carries
    //  a peer URI prefix and represents a remote-observed tip.
    if ($empty(key) || *key[0] != '?') done;
    u8cs branch = {key[0] + 1, key[1]};

    sha1 tip = {};
    if (!refadv_decode_terminal(&tip, val)) done;

    //  Wire alias: empty branch (trunk) advertises as `main`.
    a_cstr(main_s, "main");
    u8cs name = {};
    u8csMv(name, $empty(branch) ? main_s : branch);

    if (adv->count >= REFADV_MAX_ENTRIES) done;
    refadv_entry *ent = &adv->ents[adv->count];
    ent->tip = tip;

    //  Build refname into the arena via GITFeedRef so the wire form
    //  (refs/heads/<name>) is built in exactly one place.
    u8 *refname_start = u8bIdleHead(adv->arena);
    {
        ok64 fo = GITFeedRef(adv->arena, GITREF_BRANCH, name);
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
    //  <root>/.dogs/REFS.  Future phases iterate every shard dir.
    a_path(keepdir, u8bDataC(k->h->root), KEEP_DIR_S);

    refadv_ctx ctx = {.adv = out};
    ok64 eo = REFSEach($path(keepdir), refadv_each_cb, &ctx);
    if (eo != OK) {
        REFADVClose(out);
        return eo;
    }
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
        if (sha1eq(&adv->ents[i].tip, tip)) {
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
    u8 hex[40];
    u8s hex_s = {hex, hex + 40};
    u8cs sha_in = {e->tip.data, e->tip.data + 20};
    a_dup(u8c, sha_dup, sha_in);
    call(HEXu8sFeedSome, hex_s, sha_dup);

    u8csc hex_full = {hex, hex + 40};
    if (u8bIdleLen(out) < 40 + 1 + (size_t)u8csLen(e->refname) + 1 +
                          (with_caps ? 1 + sizeof(REFADV_CAPS) - 1 : 0))
        return BNOROOM;
    u8bFeed(out, hex_full);
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
