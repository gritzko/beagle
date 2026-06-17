//  WIRE: git upload-pack want/have negotiator + segment list builder.
//
//  See WIRE.h for the contract and WIRE.md Phase 4 for the surrounding
//  plan.

#include "WIRE.h"

#include <fcntl.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "dog/git/PKT.h"

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

//  Find the PACK bookmark covering log_off in file_id by scanning
//  every LSM run for the trunk shard.  Sets *bm_offset, *bm_count,
//  *bm_byte_len.  Returns KEEPNONE if no covering bookmark exists.
static ok64 wire_find_pack(keeper *k, u32 file_id, u64 log_off,
                           u64 *bm_offset, u32 *bm_count, u32 *bm_byte_len) {
    sane(k && bm_offset && bm_count && bm_byte_len);
    u64  best_off = 0;
    u32  best_count = 0;
    u32  best_len  = 0;
    b8   any = NO;
    u32 nruns = keep_run_count_all(k);
    for (u32 r = 0; r < nruns; r++) {
        wh128cs run = {NULL, NULL};
        keep_run_at_all(run, k, r);
        size_t len = wh128csLen(run);
        for (size_t i = 0; i < len; i++) {
            wh128 const *e = wh128csAtP(run, i);
            if (wh64Type(e->key) != KEEP_TYPE_PACK) continue;
            if (wh64Id(e->key)   != file_id)        continue;
            //  GET-027: skip degenerate zero-length bookmarks so a stray
            //  empty re-index never anchors the want's pack (which would
            //  set end_offset = bo, shipping nothing) and never masks the
            //  real bookmark sharing its offset.
            if (keepPackBmLen(e->val) == 0)         continue;
            u64 bo = wh64Off(e->key);
            if (bo > log_off) continue;
            if (any && bo <= best_off) continue;
            best_off   = bo;
            best_count = keepPackBmCount(e->val);
            best_len   = keepPackBmLen(e->val);
            any = YES;
        }
    }
    if (!any) return KEEPNONE;
    *bm_offset   = best_off;
    *bm_count    = best_count;
    *bm_byte_len = best_len;
    done;
}

//  Locate the PACK bookmark for `file_id` that STARTS exactly at byte
//  offset `at`, returning its (obj_count, byte_len).  Returns:
//    OK         — exactly one bookmark starts at `at`.
//    KEEPNONE   — no bookmark starts at `at` (a gap in the tiling).
//    WIRECRPT   — TWO OR MORE bookmarks start at `at` with differing
//                 extents (a duplicate / overlapping bookmark — corrupt
//                 source shard, GET-019).  Byte-identical duplicates
//                 cannot survive: the LSM sort+dedup collapses identical
//                 rows, so two surviving rows at one offset must differ,
//                 and a differing extent is the overlap that poisons the
//                 count.
static ok64 wire_bookmark_at(keeper *k, u32 file_id, u64 at,
                             u32 *count, u32 *blen) {
    sane(k && count && blen);
    b8  found = NO;
    u32 fc = 0, fl = 0;
    u32 nruns = keep_run_count_all(k);
    for (u32 r = 0; r < nruns; r++) {
        wh128cs run = {NULL, NULL};
        keep_run_at_all(run, k, r);
        size_t len = wh128csLen(run);
        for (size_t i = 0; i < len; i++) {
            wh128 const *e = wh128csAtP(run, i);
            if (wh64Type(e->key) != KEEP_TYPE_PACK) continue;
            if (wh64Id(e->key)   != file_id)        continue;
            if (wh64Off(e->key)  != at)             continue;
            u32 c = keepPackBmCount(e->val);
            u32 l = keepPackBmLen(e->val);
            //  GET-027: a zero-length bookmark covers no bytes and no
            //  objects (a degenerate re-index of an empty append).  It
            //  is NOT an overlapping dup of a real bookmark at the same
            //  offset — skip it so the authoritative non-empty bookmark
            //  wins and the tiling proceeds (else a healthy pack with a
            //  stray empty bookmark refuses the whole clone, WIRECRPT).
            if (l == 0) continue;
            if (found && l != fl) return WIRECRPT;  //  overlapping dup
            found = YES;
            fc = c;
            fl = l;
        }
    }
    if (!found) return KEEPNONE;
    *count = fc;
    *blen  = fl;
    done;
}

//  Sum obj_count over the byte range [from, to) by walking the PACK
//  bookmarks that TILE it — each one starting exactly where the prior
//  ends.  This is the count the segment's pack header will declare, and
//  it must equal the objects physically present in those bytes, or the
//  client's UNPK scans short ("scan incomplete N/M").  A clean store
//  always tiles cleanly; a corrupt one (gap / overlap / overshoot) is
//  REFUSED here (WIRECRPT) so a poisoned pack never ships (GET-019).
static ok64 wire_tile_count(keeper *k, u32 file_id, u64 from, u64 to,
                            u32 *out_count) {
    sane(k && out_count && from <= to);
    u64 cursor = from;
    u64 total  = 0;
    while (cursor < to) {
        u32 c = 0, l = 0;
        call(wire_bookmark_at, k, file_id, cursor, &c, &l);
        if (l == 0) return WIRECRPT;           //  zero-length bookmark loops
        cursor += (u64)l;
        if (cursor > to) return WIRECRPT;       //  overshoot — overlap
        total  += (u64)c;
        if (total > 0xffffffffu) return WIRECRPT;
    }
    //  cursor must land exactly on `to`; a short landing is a gap.
    if (cursor != to) return WIRECRPT;
    *out_count = (u32)total;
    done;
}

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

    //  Phase 1c: dir chain is always [trunk shard 0].  One want →
    //  one segment.  Multi-want is a follow-up.
    sha1cp want = &req->wants[0];

    u32 want_fid = 0;
    u64 want_off = 0;
    ok64 wo = wire_locate_sha(k, want, &want_fid, &want_off);
    if (wo != OK) return WIRENOSHA;

    u64 want_pack_off = 0;
    u32 want_pack_count = 0;
    u32 want_pack_len   = 0;
    call(wire_find_pack, k, want_fid, want_off,
         &want_pack_off, &want_pack_count, &want_pack_len);
    u64 end_offset = want_pack_off + want_pack_len;

    //  Walk haves; per dir take the max start of a covering pack.
    //  Phase 1c: only the trunk shard, so a single watermark.
    u64 watermark = 12;          // start-of-first-pack default
    u32 watermark_fid = want_fid;
    b8  have_anchor = NO;

    for (u32 i = 0; i < req->nhaves; i++) {
        u32 hfid = 0;
        u64 hoff = 0;
        ok64 hr = wire_locate_sha(k, &req->haves[i], &hfid, &hoff);
        if (hr != OK) continue;             // unknown have, skip
        if (hfid != want_fid) continue;     // different log file (multi-shard tba)
        u64 hpoff = 0;
        u32 hpcount = 0;
        u32 hplen   = 0;
        ok64 fp = wire_find_pack(k, hfid, hoff, &hpoff, &hpcount, &hplen);
        if (fp != OK) continue;
        u64 cand = hpoff + hplen;
        if (!have_anchor || cand > watermark) {
            watermark = cand;
            have_anchor = YES;
        }
    }
    if (!have_anchor) {
        //  Default: ship the whole trunk log up to (and including)
        //  the want's pack — start from the very first object.
        watermark = 12;
    }

    //  Open the trunk pack log file.
    a_path(kdir);
    call(HOMEBranchDir, kdir, NULL);
    a_pad(u8, packpath, FILE_PATH_MAX_LEN);
    call(wire_pack_path, packpath, $path(kdir), want_fid);

    int fd = -1;
    call(FILEOpen, &fd, $path(packpath), O_RDONLY);
    fd_pool[0] = fd;

    if (end_offset <= watermark) {
        //  Client is already at or past the want's pack tail — nothing
        //  to ship.  No tiling needed (the range is empty / degenerate),
        //  so the corruption check below is skipped for a no-op segment.
        out_segs[0].fd     = fd;
        out_segs[0].offset = watermark;
        out_segs[0].length = 0;
        out_segs[0].count  = 0;
        *out_n = 1;
        done;
    }

    //  Compute object count for the segment by tiling the PACK
    //  bookmarks over [watermark .. end_offset).  A corrupt source
    //  shard (gap / overlapping bookmark) is REFUSED (WIRECRPT) rather
    //  than over-counted — shipping a header whose count exceeds the
    //  objects in the bytes is exactly the GET-019 truncation
    //  ("unpk: scan incomplete").
    u32 seg_count = 0;
    call(wire_tile_count, k, watermark_fid, watermark, end_offset,
         &seg_count);

    out_segs[0].fd     = fd;
    out_segs[0].offset = watermark;
    out_segs[0].length = end_offset - watermark;
    out_segs[0].count  = seg_count;
    *out_n = 1;
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

    pstr_seg segs[WIRE_MAX_WANTS];
    int      fds [WIRE_MAX_WANTS];
    for (u32 i = 0; i < WIRE_MAX_WANTS; i++) fds[i] = -1;
    u32 nseg = 0;

    ok64 bo = WIREBuildSegments(adv, &req, segs, fds, WIRE_MAX_WANTS, &nseg);
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
