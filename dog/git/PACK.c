//  PACK: git packfile parser
//
#include "PACK.h"

#include "DELT.h"
#include "GIT.h"
#include "ZINF.h"
#include "abc/PRO.h"

static a_cstr(PACK_MAGIC, "PACK");

ok64 PACKu8sFeedHdr(u8s into, u32 count) {
    sane(u8sOK(into));
    call(u8sFeed, into, PACK_MAGIC);
    u32 ver_be = flip32(2);
    u32 cnt_be = flip32(count);
    call(u8sFeed32, into, &ver_be);
    call(u8sFeed32, into, &cnt_be);
    done;
}

ok64 PACKDrainHdr(u8cs from, pack_hdr *hdr) {
    sane(u8csOK(from) && hdr);
    if ($size(from) < 12) return NODATA;

    if (memcmp(from[0], PACK_MAGIC[0], 4) != 0)
        return PACKBADFMT;
    u8csUsed(from, 4);

    // version + count: big-endian u32
    u32 v = 0, c = 0;
    u8sDrain32(from, &v);
    u8sDrain32(from, &c);
    hdr->version = flip32(v);
    hdr->count = flip32(c);

    done;
}

// Decode git varint: type in bits 6..4 of first byte, size in
// bit 3..0 of first byte then 7-bit continuation.
static ok64 PACKDrainVarint(u8cs from, u8 *type, u64 *size) {
    sane(type && size);
    u8 c = 0;
    call(u8sDrain8, from, &c);

    *type = (c >> 4) & 0x7;
    *size = c & 0x0f;
    u32 shift = 4;

    while (c & 0x80) {
        test(shift < 64, PACKBADFMT);
        call(u8sDrain8, from, &c);
        *size |= (u64)(c & 0x7f) << shift;
        shift += 7;
    }

    done;
}

// Decode OFS_DELTA negative offset varint
static ok64 PACKDrainOfs(u8cs from, u64 *ofs) {
    sane(ofs);
    u8 c = 0;
    call(u8sDrain8, from, &c);
    *ofs = c & 0x7f;

    while (c & 0x80) {
        test(*ofs < (UINT64_MAX >> 7), PACKBADFMT);
        call(u8sDrain8, from, &c);
        *ofs = ((*ofs + 1) << 7) | (c & 0x7f);
    }

    done;
}

ok64 PACKDrainObjHdr(u8cs from, pack_obj *obj) {
    sane(u8csOK(from) && obj);
    zerop(obj);

    call(PACKDrainVarint, from, &obj->type, &obj->size);

    if (obj->type == PACK_OBJ_OFS_DELTA) {
        call(PACKDrainOfs, from, &obj->ofs_delta);
    } else if (obj->type == PACK_OBJ_REF_DELTA) {
        if ($size(from) < GIT_SHA1_LEN) return PACKBADFMT;
        a_head(u8c, ref, from, GIT_SHA1_LEN);
        u8csUsed(from, GIT_SHA1_LEN);
        u8csMv(obj->ref_delta, ref);
    }

    done;
}

ok64 PACKu8sFeedOfs(u8bp buf, u64 val) {
    sane(u8bOK(buf));
    //  Inverse of PACKDrainOfs: the decoder reads MSB-first 7-bit
    //  groups as `ofs = ((ofs+1) << 7) | (c & 0x7f)`, so the encoder
    //  builds the groups low-to-high (decrementing each higher group
    //  to match the decoder's +1), then emits them high-to-low with
    //  a continuation MSB on every byte but the last.
    a_pad(u8, tmp, 16);
    call(u8bFeed1, tmp, (u8)(val & 0x7f));
    while ((val >>= 7) != 0) {
        val--;
        call(u8bFeed1, tmp, (u8)(0x80 | (val & 0x7f)));
    }
    a_dup(u8c, groups, u8bData(tmp));
    $rof(u8c, g, groups) call(u8bFeed1, buf, *g);
    done;
}

ok64 PACKu8sFeedObjHdr(u8bp buf, u8 type, u64 size) {
    sane(u8bOK(buf));
    u8 first = (u8)((type << 4) | (size & 0x0f));
    size >>= 4;
    if (size > 0) first |= 0x80;
    call(u8bFeed1, buf, first);
    while (size > 0) {
        u8 c = (u8)(size & 0x7f);
        size >>= 7;
        if (size > 0) c |= 0x80;
        call(u8bFeed1, buf, c);
    }
    done;
}

ok64 PACKu8sFeedObj(u8bp log, u8 type, u8csc content,
                    u8csc base, u64 cur_off, u64 base_off,
                    u8bp delta, b8 *out_delta) {
    sane(u8bOK(log) && type >= 1 && type <= 4);
    if (out_delta) *out_delta = NO;

    //  Try OFS_DELTA only when a base is supplied and sits earlier in
    //  the log.  REF_DELTA is never emitted into a stored log.
    if (!$empty(base) && base_off < cur_off && u8bOK(delta)) {
        u8bReset(delta);
        ok64 deo = DELTEncode(base, content, delta);
        if (deo == OK && (u64)u8bDataLen(delta) < (u64)u8csLen(content)) {
            u64 delta_len = u8bDataLen(delta);
            //  Header carries the delta-instruction length; the OFS
            //  varint is the negative distance back to the base record.
            a_pad(u8, ohdr, 16);
            call(PACKu8sFeedObjHdr, ohdr, PACK_OBJ_OFS_DELTA, delta_len);
            a_dup(u8c, ohb, u8bData(ohdr));
            call(u8bFeed, log, ohb);

            a_pad(u8, ofs, 16);
            call(PACKu8sFeedOfs, ofs, cur_off - base_off);
            a_dup(u8c, ofsb, u8bData(ofs));
            call(u8bFeed, log, ofsb);

            a_dup(u8c, zsrc, u8bDataC(delta));
            call(ZINFDeflate, u8bIdle(log), zsrc);
            if (out_delta) *out_delta = YES;
            done;
        }
    }

    //  Raw object record: header(type,size) + deflated content.
    a_pad(u8, ohdr, 16);
    call(PACKu8sFeedObjHdr, ohdr, type, u8csLen(content));
    a_dup(u8c, oh, u8bData(ohdr));
    call(u8bFeed, log, oh);

    a_dup(u8c, czsrc, content);
    call(ZINFDeflate, u8bIdle(log), czsrc);
    done;
}

ok64 PACKInflate(u8cs from, u8s into, u64 size) {
    sane(u8csOK(from) && u8sOK(into));
    if ((u64)u8sLen(into) < size) return NOROOM;

    a_dup(u8, trimmed, into);
    u8sShed(trimmed, u8sLen(trimmed) - size);
    ok64 o = ZINFInflate(trimmed, from);
    if (o != OK) return PACKBADOBJ;
    u8sJoin(into, trimmed);

    done;
}
