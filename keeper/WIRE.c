//  WIRE: git upload-pack want/have negotiator + segment list builder.
//
//  See WIRE.h for the contract and WIRE.md Phase 4 for the surrounding
//  plan.

#include "WIRE.h"

#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "dog/git/PKT.h"
#include "dog/git/PACK.h"
#include "dog/git/DELT.h"
#include "dog/git/ZINF.h"
#include "keeper/CLOSE.h"

// --- request reader ---

#define WIRE_READ_BUF (1u << 16)   // 64 KiB pkt buffer

//  Capability tokens recognised on the first want line.
static u8c const WIRE_CAP_OFS_DELTA_S[]     = "ofs-delta";
static u8c const WIRE_CAP_SIDE_BAND_64K_S[] = "side-band-64k";
static u8c const WIRE_CAP_MULTI_ACK_DET_S[] = "multi_ack_detailed";
static u8c const WIRE_CAP_THIN_PACK_S[]     = "thin-pack";
static u8c const WIRE_CAP_NO_PROGRESS_S[]   = "no-progress";

//  Token equality: |s| == plen && bytes equal.
static b8 wire_token_eq(u8csc s, u8c const *tok, size_t tlen) {
    if ((size_t)u8csLen(s) != tlen) return NO;
    return memcmp(s[0], tok, tlen) == 0;
}

//  Recognise one upload-pack capability token → cap bits.  Strips
//  "agent=..." (and any other unknown) silently.
static void wire_cap_token(u8csc tok, void0p ctx) {
    u32 *caps = (u32 *)ctx;
    if (wire_token_eq(tok, WIRE_CAP_OFS_DELTA_S,
                      sizeof(WIRE_CAP_OFS_DELTA_S) - 1)) {
        *caps |= WIRE_CAP_OFS_DELTA;
    } else if (wire_token_eq(tok, WIRE_CAP_SIDE_BAND_64K_S,
                             sizeof(WIRE_CAP_SIDE_BAND_64K_S) - 1)) {
        *caps |= WIRE_CAP_SIDE_BAND_64K;
    } else if (wire_token_eq(tok, WIRE_CAP_MULTI_ACK_DET_S,
                             sizeof(WIRE_CAP_MULTI_ACK_DET_S) - 1)) {
        *caps |= WIRE_CAP_MULTI_ACK_DET;
    } else if (wire_token_eq(tok, WIRE_CAP_THIN_PACK_S,
                             sizeof(WIRE_CAP_THIN_PACK_S) - 1)) {
        *caps |= WIRE_CAP_THIN_PACK;
    } else if (wire_token_eq(tok, WIRE_CAP_NO_PROGRESS_S,
                             sizeof(WIRE_CAP_NO_PROGRESS_S) - 1)) {
        *caps |= WIRE_CAP_NO_PROGRESS;
    }
}

//  Parse a space-separated capability list off the tail of the first
//  want line, OR from a pkt-line that consists of only capability
//  tokens (rarely used, kept for tolerance).  Sets bits in *caps.
//  upload-pack caps are SP/'\n'-separated (no TAB).
static void wire_parse_caps(u32 *caps, u8csc tail) {
    KEEPForEachCapToken(tail, NO, wire_cap_token, caps);
}

//  Drain one pkt-line from buf, refilling via FILEDrain on NODATA.
//  Returns OK / PKTFLUSH / PKTDELIM / WIREFAIL on EOF/read error.
static ok64 wire_read_pkt(int in_fd, u8b buf, u8cs adv, u8csp line) {
    for (;;) {
        ok64 o = PKTu8sDrain(adv, line);
        if (o != NODATA) return o;
        if (!u8bHasRoom(buf)) return WIREFAIL;
        u8s fill;
        u8sFork(u8bIdle(buf), fill);
        ok64 fr = FILEDrain(in_fd, fill);
        if (fr == FILEEND) return WIREFAIL;
        if (fr != OK) return WIREFAIL;
        u8sJoin(u8bIdle(buf), fill);
        adv[1] = u8csTerm(u8bDataC(buf));
    }
}

ok64 WIREReadRequest(int in_fd, wire_reqp req) {
    sane(in_fd >= 0 && req);

    zerop(req);

    a_carve(u8, buf, WIRE_READ_BUF);

    u8cs adv = {u8bDataHead(buf), u8bDataHead(buf)};
    b8  saw_want = NO;
    b8  saw_done = NO;
    ok64 rc = OK;

    while (!saw_done) {
        u8cs line = {};
        ok64 d = wire_read_pkt(in_fd, buf, adv, line);
        if (d == PKTFLUSH) {
            //  Flush before "done" — common end-of-haves marker.
            //  Loop continues; client should send "done" shortly.
            //  If we already saw zero wants, treat flush as terminator.
            if (!saw_want) { rc = OK; break; }
            continue;
        }
        if (d == PKTDELIM) continue;
        if (d != OK) { rc = d; break; }

        //  Trim trailing '\n' if present.
        if (u8csLen(line) > 0 && line[1][-1] == '\n') line[1]--;

        wire_evt ev = {};
        if (WIREClassify(line, WIRE_UPLOAD, &ev) != OK) continue;

        switch (ev.kind) {
        case WIRE_WANT:
            if (req->nwants >= WIRE_MAX_WANTS) { rc = WIREBADREQ; goto out; }
            req->wants[req->nwants++] = ev.sha;
            //  Capabilities ride on the first want line only.
            if (!saw_want && !u8csEmpty(ev.caps))
                wire_parse_caps(&req->caps, (u8csc){ev.caps[0], ev.caps[1]});
            saw_want = YES;
            break;
        case WIRE_HAVE:
            //  Over-cap haves dropped silently — over-ship is the failure mode.
            if (req->nhaves < WIRE_MAX_HAVES)
                req->haves[req->nhaves++] = ev.sha;
            break;
        case WIRE_DONE:
            saw_done = YES;
            goto out;
        default: /* shallow / unknown: ignore */ break;
        }
    }
out:;

    return rc;
}

// --- segment builder ---

//  Compose <root>/.be/NNNNN.keeper into `out` (reset first).
//  Mirrors KEEP.c's static keep_pack_path; replicated here because
//  that helper isn't exposed.  `kdir` is the dir prefix (trunk or a
//  branch subdir) the caller already resolved.
static ok64 wire_pack_path(path8b out, u8csc kdir, u32 file_id) {
    sane(u8bOK(out) && !u8csEmpty(kdir));
    a_pad(u8, fname, KEEP_SEQNO_W + sizeof(KEEP_PACK_EXT));
    call(RONu8sFeedPad, u8bIdle(fname), (ok64)file_id, KEEP_SEQNO_W);
    call(u8bFed, fname, KEEP_SEQNO_W);
    a_cstr(ext, KEEP_PACK_EXT);
    u8bFeed(fname, ext);
    call(PATHu8bDup, out, kdir);
    call(PATHu8bPush, out, u8bDataC(fname));
    done;
}

//  GIT-005: the bookmark-tiling segment helpers (wire_find_pack,
//  wire_bookmark_at, wire_tile_count) are gone.  Full-clone serving
//  now ships each `.keeper` file verbatim using the file's own
//  file-level PACK header count (no per-pack tiling, so no
//  count-vs-bytes mismatch — the GET-019 class is structurally
//  removed); incremental fetch goes through the thin-pack builder
//  below.  `wire_locate_sha` (the sha → (file_id, offset) anchor) is
//  retained — the thin builder needs it.

//  Resolve a sha to the LATEST (largest log_off) object-type entry whose
//  hashlet matches, scanning every LSM run.  The plain "first match wins"
//  scan (KEEPLookup-style) returns whichever copy a run holds first,
//  which for a re-ingested / duplicated object is the EARLIEST copy.
//  The segment builder must instead anchor on the copy nearest the log
//  tail: the want's `end_offset` should be the true tail of the object's
//  newest pack, and a have's watermark should sit as high as possible.
//  Using the earliest copy decouples the shipped byte range from the
//  log tail (GET-007: `end_offset` could even fall below the watermark,
//  re-shipping duplicate packs).  Trust the 60-bit hashlet (collision
//  rate ~2^-60); callers needing stricter guarantees re-verify via
//  KEEPGetExact.
static ok64 wire_locate_sha(keeper *k, sha1cp sha,
                            u32 *out_file_id, u64 *out_off) {
    sane(k && sha && out_file_id && out_off);
    u64 hashlet60 = WHIFFHashlet60(sha);
    u64 key_lo = keepKeyPack(KEEP_OBJ_COMMIT, hashlet60);
    u64 key_hi = keepKeyPack(KEEP_OBJ_TAG, hashlet60);

    b8  found    = NO;
    u32 best_fid = 0;
    u64 best_off = 0;
    u32 nruns = keep_run_count_all(k);
    for (u32 r = 0; r < nruns; r++) {
        wh128cs run = {NULL, NULL};
        keep_run_at_all(run, k, r);
        size_t len = wh128csLen(run);
        if (len == 0) continue;
        wh128 needle = {.key = key_lo, .val = 0};
        size_t lo = (size_t)(wh128sFindGE(run, &needle) - run[0]);
        //  A run may hold several same-hashlet entries (COMMIT..TAG, and
        //  duplicate offsets from re-ingest).  Walk the whole matching
        //  span and keep the largest offset seen across all runs.
        for (size_t i = lo; i < len && wh128csAtP(run, i)->key <= key_hi; i++) {
            wh128 const *e = wh128csAtP(run, i);
            if (e->key < key_lo) continue;
            u32 fid = wh64Id(e->val);
            u64 off = wh64Off(e->val);
            if (!found || off > best_off) {
                best_fid = fid;
                best_off = off;
                found    = YES;
            }
        }
    }
    if (!found) return KEEPNONE;
    *out_file_id = best_fid;
    *out_off     = best_off;
    done;
}

//  One `.keeper` file → one verbatim segment.  Each native log is
//  `[12-byte file-level PACK header (count=objects-in-file)][object
//  records, no SHA-1 trailer]` (KEEP.c KEEPPackClose), so a segment of
//  `[12 .. size)` with `count` read from the file's own header is a
//  self-contained, byte-exact contribution to the stitched clone pack.
//  No bookmark tiling: the file header IS the authoritative count, so
//  the count never diverges from the bytes (the GET-019 truncation
//  class cannot arise).  Opens an fd into `fd_pool[slot]`; caller
//  closes after PSTRWrite.
typedef struct {
    keeper   *k;
    pstr_seg *segs;
    int      *fds;
    u32       cap;
    u32       n;
    ok64      err;
} wire_verbatim_ctx;

static ok64 wire_verbatim_file_cb(void0p arg, path8p path) {
    sane(arg && path);
    wire_verbatim_ctx *c = (wire_verbatim_ctx *)arg;
    //  Only `.keeper` pack logs — skip `.keeper.idx`, `refs`, `wtlog`.
    u8cs base = {};
    PATHu8sBase(base, u8bDataC(path));
    a_cstr(kext, KEEP_PACK_EXT);
    size_t n = (size_t)u8csLen(base);
    if (n != KEEP_SEQNO_W + u8csLen(kext)) return OK;
    a_rest(u8c, ext, base, KEEP_SEQNO_W);
    if (!u8csEq(ext, kext)) return OK;

    if (c->n >= c->cap) { c->err = WIRENOROOM; return WIRENOROOM; }

    int fd = -1;
    call(FILEOpen, &fd, u8bDataC(path), O_RDONLY);

    size_t size = 0;
    ok64 so = FILESize(&size, &fd);
    if (so != OK) { close(fd); c->err = so; return so; }
    //  A freshly-created-but-empty log (header only, no objects) ships
    //  nothing and contributes count 0 — harmless, but skip it so we
    //  never emit a degenerate zero-length segment.
    if (size <= 12) { close(fd); return OK; }

    //  Read the 12-byte file-level header to recover the object count.
    a_pad(u8, hdrbuf, 12);
    ssize_t got = pread(fd, *u8bIdle(hdrbuf), 12, 0);
    if (got != 12) { close(fd); c->err = WIREFAIL; return WIREFAIL; }
    u8bFed(hdrbuf, 12);
    pack_hdr h = {};
    a_dup(u8c, hs, u8bData(hdrbuf));
    ok64 ho = PACKDrainHdr(hs, &h);
    if (ho != OK) { close(fd); c->err = ho; return ho; }

    u32 slot = c->n;
    c->fds[slot]          = fd;
    c->segs[slot].fd      = fd;
    c->segs[slot].offset  = 12;
    c->segs[slot].length  = (u64)size - 12;
    c->segs[slot].count   = h.count;
    c->n++;
    return OK;
}

ok64 WIREBuildSegments(refadvcp adv, wire_reqcp req,
                       pstr_seg *out_segs, int *fd_pool,
                       u32 cap, u32 *out_n) {
    sane(req && out_segs && fd_pool && out_n);
    keeper *k = &KEEP;
    (void)adv;  // tip→dir lookup wired in once multi-branch fan-out lands.

    *out_n = 0;
    if (cap == 0) return WIREFAIL;

    if (req->nwants == 0) {
        //  Empty request — caller still sends a 32-byte empty pack
        //  (PSTRWrite with zero segments).
        return OK;
    }

    //  GIT-005 full-clone path: ship the want's whole reachable object
    //  set verbatim.  With one OFS-only flat pool per project, the whole
    //  log IS that set sans framing.  Verify the want is present (so a
    //  bogus want still yields WIRENOSHA, not a silent whole-store dump),
    //  then enumerate every `.keeper` file in the shard and ship each
    //  verbatim.  Handles a shard with MORE THAN ONE `.keeper` file
    //  (legacy / pre-recompaction logs): one segment per file, stitched
    //  in scan order by PSTRWrite.
    {
        u32 wfid = 0;
        u64 woff = 0;
        ok64 wo = wire_locate_sha(k, &req->wants[0], &wfid, &woff);
        if (wo != OK) return WIRENOSHA;
    }

    a_path(kdir);
    call(HOMEBranchDir, kdir, NULL);

    wire_verbatim_ctx c = {.k = k, .segs = out_segs, .fds = fd_pool,
                           .cap = cap, .n = 0, .err = OK};
    ok64 so = FILEScanFiles(kdir, wire_verbatim_file_cb, &c);
    if (c.err != OK) {
        for (u32 i = 0; i < c.n; i++)
            if (fd_pool[i] >= 0) { close(fd_pool[i]); fd_pool[i] = -1; }
        return c.err;
    }
    if (so != OK) {
        for (u32 i = 0; i < c.n; i++)
            if (fd_pool[i] >= 0) { close(fd_pool[i]); fd_pool[i] = -1; }
        return so;
    }
    *out_n = c.n;
    done;
}

// --- thin-pack builder (incremental fetch) ---

//  An object placed in the OUTPUT pack: its sha, its storage anchor
//  (file_id, offset), and — filled during emit — the byte offset of
//  its record START in the output pack (for OFS_DELTA back-offset
//  recompute).  `present` records whether wire_locate_sha found it
//  (a want/have closure object always should; a guard keeps a miss
//  from poisoning the offset map).
typedef struct {
    sha1 sha;
    u32  file_id;
    u64  store_off;     // record start in the stored .keeper log
    u64  out_off;       // record start in the OUTPUT pack (set at emit)
    i64  ref_base;      // a HAVE object stored-delta'd against THIS one
                        // (its sibling version) → fresh REF_DELTA base
                        // when this ship object is itself stored raw; -1
    b8   shipped;       // YES = in the ship-set; NO = have-closure only
    b8   present;       // YES = wire_locate_sha succeeded
} wire_obj;

//  Sort by (file_id, store_off) so a stored OFS base — which always
//  sits at a LOWER offset in the SAME file — is emitted before its
//  dependent.  This keeps the output pack's bases-before-deltas
//  invariant for the OFS arm without a topological sort.
static int wire_obj_cmp(void const *a, void const *b) {
    wire_obj const *x = a, *y = b;
    if (x->file_id != y->file_id) return x->file_id < y->file_id ? -1 : 1;
    if (x->store_off != y->store_off)
        return x->store_off < y->store_off ? -1 : 1;
    return 0;
}

//  Resolve a stored (file_id, base_off) anchor back to the matching
//  emit record via the (file_id, store_off)-sorted `objs` array.
//  Returns the index, or -1 when no shipped/had object sits exactly at
//  that anchor (a base outside both closures — caller ships raw).
static i64 wire_obj_at(wire_obj const *objs, u32 n, u32 file_id,
                       u64 store_off) {
    u32 lo = 0, hi = n;
    while (lo < hi) {
        u32 mid = (lo + hi) >> 1;
        wire_obj const *m = &objs[mid];
        if (m->file_id < file_id ||
            (m->file_id == file_id && m->store_off < store_off))
            lo = mid + 1;
        else if (m->file_id > file_id ||
                 (m->file_id == file_id && m->store_off > store_off))
            hi = mid;
        else return (i64)mid;
    }
    return -1;
}

//  Collect the reachable closure of every want into `out`/`have_set`,
//  pruning anything the haves already cover.  Tags are followed as
//  commits (wire wants are commit/tag tips); a want that resolves to a
//  non-commit object (e.g. a bare blob/tree want) is added directly.
static ok64 wire_collect_ship(wire_reqcp req, close_set const *have,
                              sha1 *out, u32 *n, u32 cap,
                              close_set *ship_set) {
    sane(req && out && n && ship_set);
    for (u32 i = 0; i < req->nwants; i++) {
        sha1cp w = &req->wants[i];
        if (have && CLOSESetHas(have, w)) continue;
        if (CLOSESetHas(ship_set, w)) continue;
        //  Try the commit closure first; a non-commit want (KEEPNONE /
        //  KEEPFAIL from CLOSEWalkCommit's type check) falls back to a
        //  single-object add so a bare blob/tree want still ships.
        u32 before = *n;
        ok64 wo = CLOSEWalkCommit(w, out, n, cap, have, ship_set);
        if (wo == OK) continue;
        if (wo == CLOSEFULL) return WIRENOROOM;
        //  Not a commit: ship the single object (if present) raw.
        *n = before;
        if (*n >= cap) return WIRENOROOM;
        out[(*n)++] = *w;
        CLOSESetAdd(ship_set, w);
    }
    done;
}

//  Emit ONE shipped object's record into `out` (the output pack),
//  setting `o->out_off` to its record start first.  Runs as its own
//  `call`ed frame so every `a_carve` here rewinds per object — the loop
//  over thousands of objects never accumulates BASS scratch.  `out_off`
//  is the bytes-written length (record start from the pack's data head)
//  — buffer-relocation-safe, no pointer math — for OFS_DELTA back-offset
//  recompute.
//    `*raw_reships` — incremented when a stored delta falls back to raw
//                   (over-ship guard, GET-019).
static ok64 wire_thin_emit(u8bp out, wire_obj *objs,
                           u32 nobjs, wire_obj *o, b8 ofs_ok,
                           u64 *raw_reships) {
    sane(u8bOK(out) && objs && o && raw_reships);
    o->out_off = (u64)u8bDataLen(out);

    //  Full object body + git type (1..4) for raw / size.
    a_carve(u8, body_b, 1ULL << 26);
    u8 gtype = 0;
    call(KEEPGetExact, &o->sha, body_b, &gtype);
    a_dup(u8c, body, u8bDataC(body_b));

    //  Inspect the STORED record ONCE: an OFS_DELTA whose base is also
    //  in our table can be re-framed verbatim (inflate the stored delta
    //  payload, re-emit under OFS or REF framing).
    u8cs packbytes = {NULL, NULL};
    b8  store_delta = NO;
    i64 base_idx = -1;
    u64 dlen = 0;
    a_carve(u8, dinst_b, 1ULL << 25);
    u8cs dinst = {NULL, NULL};

    if (o->present && KEEPPackBytes(o->file_id, packbytes) == OK &&
        o->store_off < (u64)u8csLen(packbytes)) {
        a_rest(u8c, rec, packbytes, o->store_off);
        pack_obj so = {};
        if (PACKDrainObjHdr(rec, &so) == OK &&
            (so.type == PACK_OBJ_OFS_DELTA ||
             so.type == PACK_OBJ_REF_DELTA)) {
            store_delta = YES;
            if (so.type == PACK_OBJ_OFS_DELTA &&
                so.ofs_delta <= o->store_off) {
                u64 base_store = o->store_off - so.ofs_delta;
                base_idx = wire_obj_at(objs, nobjs, o->file_id, base_store);
                if (base_idx >= 0) {
                    u8s dinto = {u8bIdleHead(dinst_b),
                                 u8bIdleHead(dinst_b) + u8bIdleLen(dinst_b)};
                    if (PACKInflate(rec, dinto, so.size) == OK) {
                        dinst[0] = u8bIdleHead(dinst_b);
                        dinst[1] = u8bIdleHead(dinst_b) + so.size;
                        dlen = so.size;
                    } else {
                        base_idx = -1;   //  inflate failed → raw
                    }
                }
            }
        }
    }

    if (base_idx >= 0) {
        wire_obj *base = &objs[base_idx];
        a_dup(u8c, dpay, dinst);
        if (base->shipped && ofs_ok) {
            //  OFS_DELTA, back-offset RECOMPUTED output-relative.
            call(PACKu8sFeedObjHdr, out, PACK_OBJ_OFS_DELTA, dlen);
            call(PACKu8sFeedOfs, out, o->out_off - base->out_off);
            call(ZINFDeflate, u8bIdle(out), dpay);
        } else {
            //  Base have-only (client has it) OR ofs-delta not
            //  negotiated: REF_DELTA naming the base by sha.
            call(PACKu8sFeedObjHdr, out, PACK_OBJ_REF_DELTA, dlen);
            u8cs bsha = {};
            sha1slice(bsha, &base->sha);
            a_dup(u8c, bshac, bsha);
            call(u8bFeed, out, bshac);
            call(ZINFDeflate, u8bIdle(out), dpay);
        }
        done;
    }

    if (o->ref_base >= 0) {
        //  THIN efficiency: stored raw, but a have-closure sibling (its
        //  other version, e.g. the prior blob across a 1-line change)
        //  deltas against it.  The client HAS that sibling, so delta
        //  THIS object FRESH against it and ship a small REF_DELTA — no
        //  raw re-ship of a large object across the frontier.
        wire_obj *rb = &objs[o->ref_base];
        a_carve(u8, sib_b, 1ULL << 26);
        u8 sibtype = 0;
        call(KEEPGetExact, &rb->sha, sib_b, &sibtype);
        a_dup(u8c, sib, u8bDataC(sib_b));
        a_carve(u8, fresh_b, 1ULL << 25);
        ok64 de = DELTEncode(sib, body, fresh_b);
        if (de == OK && (u64)u8bDataLen(fresh_b) < (u64)u8csLen(body)) {
            call(PACKu8sFeedObjHdr, out, PACK_OBJ_REF_DELTA,
                 (u64)u8bDataLen(fresh_b));
            u8cs bsha = {};
            sha1slice(bsha, &rb->sha);
            a_dup(u8c, bshac, bsha);
            call(u8bFeed, out, bshac);
            a_dup(u8c, fpay, u8bDataC(fresh_b));
            call(ZINFDeflate, u8bIdle(out), fpay);
            done;
        }
        //  Fresh delta wasn't smaller — fall through to raw.
    }

    //  Raw record.  A stored delta whose base fell outside both closures
    //  (only at a closure root) is a forced raw re-ship — account for it
    //  (over-ship guard, GET-019: never silently cap, report fallback).
    if (store_delta && o->ref_base < 0) (*raw_reships)++;
    a_dup(u8c, bodyc, body);
    call(PACKu8sFeedObjHdr, out, gtype, (u64)u8csLen(body));
    call(ZINFDeflate, u8bIdle(out), bodyc);
    done;
}

ok64 WIREBuildThinPack(refadvcp adv, wire_reqcp req, u8bp out) {
    sane(req && u8bOK(out));
    keeper *k = &KEEP;
    (void)adv;
    u8bReset(out);

    b8 ofs_ok = (req->caps & WIRE_CAP_OFS_DELTA) != 0;

    //  1.  Have-closure: every object the client already holds, walked
    //      from each have tip.  Missing haves / non-commit haves are
    //      tolerated (haveset-build mode).
    a_carve(sha1, have_items_b, WIRE_THIN_MAX_OBJS);
    close_set have = {.items = sha1bDataHead(have_items_b),
                      .n = 0, .cap = WIRE_THIN_MAX_OBJS};
    for (u32 i = 0; i < req->nhaves; i++)
        (void)CLOSEWalkCommit(&req->haves[i], NULL, &(u32){0}, 0,
                              NULL, &have);

    //  2.  Ship-set: wants' closure minus the have-closure.
    a_carve(sha1, ship_b, WIRE_THIN_MAX_OBJS);
    sha1 *ship = sha1bDataHead(ship_b);
    a_carve(sha1, ship_set_items_b, WIRE_THIN_MAX_OBJS);
    close_set ship_set = {.items = sha1bDataHead(ship_set_items_b),
                          .n = 0, .cap = WIRE_THIN_MAX_OBJS};
    u32 nship = 0;
    call(wire_collect_ship, req, req->nhaves ? &have : NULL,
         ship, &nship, WIRE_THIN_MAX_OBJS, &ship_set);

    //  3.  Build the (file_id, store_off)-sorted object table over the
    //      ship-set PLUS the have-closure: a stored OFS base of a
    //      shipped object may itself be a have-only object, and we must
    //      still resolve that anchor back to a sha to flip it REF_DELTA.
    a_carve(u8, objs_raw, 2ULL * WIRE_THIN_MAX_OBJS * sizeof(wire_obj));
    wire_obj *objs = (wire_obj *)u8bIdleHead(objs_raw);
    u32 nobjs = 0;
    for (u32 i = 0; i < nship; i++) {
        u32 fid = 0; u64 off = 0;
        b8 ok = (wire_locate_sha(k, &ship[i], &fid, &off) == OK);
        objs[nobjs++] = (wire_obj){.sha = ship[i], .file_id = fid,
            .store_off = off, .ref_base = -1, .shipped = YES, .present = ok};
    }
    for (u32 i = 0; i < have.n; i++) {
        u32 fid = 0; u64 off = 0;
        if (wire_locate_sha(k, &have.items[i], &fid, &off) != OK) continue;
        objs[nobjs++] = (wire_obj){.sha = have.items[i], .file_id = fid,
            .store_off = off, .ref_base = -1, .shipped = NO, .present = YES};
    }
    qsort(objs, nobjs, sizeof(wire_obj), wire_obj_cmp);

    //  3b.  Reverse-dependency pass for the THIN efficiency case.  Git /
    //  the OFS-only store may store the WANT-side object RAW and the
    //  HAVE-side sibling as a delta against it (delta direction is a
    //  storage choice, not the fetch direction).  Shipping the raw
    //  want-side object would re-transmit a whole large blob across a
    //  1-line change.  So: for every HAVE object stored OFS_DELTA whose
    //  base is a SHIP object, record that have object as the ship
    //  object's `ref_base` — when the ship object is itself stored raw,
    //  the emit loop deltas it FRESH against that sibling and ships a
    //  small REF_DELTA the client resolves locally.
    for (u32 i = 0; i < nobjs; i++) {
        wire_obj *h = &objs[i];
        if (h->shipped || !h->present) continue;   //  have-only objects
        u8cs pb = {NULL, NULL};
        if (KEEPPackBytes(h->file_id, pb) != OK) continue;
        if (h->store_off >= (u64)u8csLen(pb)) continue;
        a_rest(u8c, rec, pb, h->store_off);
        pack_obj so = {};
        if (PACKDrainObjHdr(rec, &so) != OK) continue;
        if (so.type != PACK_OBJ_OFS_DELTA || so.ofs_delta > h->store_off)
            continue;
        u64 base_store = h->store_off - so.ofs_delta;
        i64 bi = wire_obj_at(objs, nobjs, h->file_id, base_store);
        if (bi < 0) continue;
        if (objs[bi].shipped && objs[bi].ref_base < 0)
            objs[bi].ref_base = (i64)i;
    }

    //  4.  Header: count = ship-set size.
    {
        a_pad(u8, hdr, 12);
        call(PACKu8sFeedHdr, u8bIdle(hdr), nship);
        u8bFed(hdr, 12);
        a_dup(u8c, hb, u8bData(hdr));
        call(u8bFeed, out, hb);
    }
    u64 raw_reships = 0;   //  over-ship guard: deltas dropped to raw

    //  5.  Emit each shipped object in (file_id, store_off) order so a
    //      stored OFS base precedes its dependent.  Each object is
    //      emitted in its own `call` frame so per-object BASS scratch
    //      rewinds — the loop never accumulates.
    for (u32 i = 0; i < nobjs; i++) {
        if (!objs[i].shipped) continue;
        call(wire_thin_emit, out, objs, nobjs, &objs[i],
             ofs_ok, &raw_reships);
    }

    //  6.  20-byte SHA-1 trailer over the whole pack.
    {
        sha1 psha = {};
        a_dup(u8c, pack_data, u8bData(out));
        SHA1Sum(&psha, pack_data);
        u8cs psha_s = {};
        sha1slice(psha_s, &psha);
        a_dup(u8c, psha_c, psha_s);
        call(u8bFeed, out, psha_c);
    }

    if (raw_reships > 0)
        fprintf(stderr,
                "wire: thin-pack shipped %llu delta base(s) raw "
                "(base outside ship+have closure)\n",
                (unsigned long long)raw_reships);
    done;
}

// --- one-shot serve ---

ok64 WIREServeUpload(int in_fd, int out_fd, refadvcp adv) {
    sane(in_fd >= 0 && out_fd >= 0);
    FILEIgnoreSIGPIPE();  //  a client closing early must not kill us

    wire_req req = {};
    call(WIREReadRequest, in_fd, &req);

    //  Send NAK pkt-line ahead of the pack stream (canonical reply
    //  when we don't ACK any haves in this MVP).  The vanilla git
    //  client expects this framing per protocol v0.
    {
        a_pad(u8, frame, 16);
        a_cstr(nak, "NAK\n");
        call(PKTu8sFeed, u8bIdle(frame), nak);
        a_dup(u8c, fdata, u8bData(frame));
        call(FILEFeedAll, out_fd, fdata);
    }

    //  GIT-005: branch on haves.  No haves → full clone: ship the
    //  want's whole reachable set verbatim (one segment per `.keeper`
    //  file, stitched by PSTRWrite).  Haves present → incremental fetch:
    //  a real thin pack (wants' closure minus haves' closure, deltas
    //  re-framed OFS/REF for the output pack).  Both arms emit a
    //  standard packfile a vanilla `git index-pack` accepts.
    if (req.nhaves == 0) {
        pstr_seg segs[WIRE_MAX_WANTS];
        int      fds [WIRE_MAX_WANTS];
        for (u32 i = 0; i < WIRE_MAX_WANTS; i++) fds[i] = -1;
        u32 nseg = 0;

        ok64 bo = WIREBuildSegments(adv, &req, segs, fds,
                                    WIRE_MAX_WANTS, &nseg);
        if (bo != OK) {
            for (u32 i = 0; i < WIRE_MAX_WANTS; i++)
                if (fds[i] >= 0) close(fds[i]);
            return bo;
        }

        pstr_segcs segslice = {segs, segs + nseg};
        ok64 wo = PSTRWrite(out_fd, segslice);

        for (u32 i = 0; i < WIRE_MAX_WANTS; i++)
            if (fds[i] >= 0) close(fds[i]);

        return wo;
    }

    //  Incremental: build the thin pack into a HEAP-mapped buffer (NOT
    //  BASS — the thin builder's per-object scratch lives in BASS, so
    //  the output pack must not squeeze it), then blast it.
    Bu8 packbuf = {};
    call(u8bMap, packbuf, 1ULL << 30);
    try(WIREBuildThinPack, adv, &req, packbuf);
    ok64 tb = __;
    if (tb != OK) { u8bUnMap(packbuf); return tb; }
    a_dup(u8c, pdata, u8bData(packbuf));
    ok64 fo = FILEFeedAll(out_fd, pdata);
    u8bUnMap(packbuf);
    return fo;
}
