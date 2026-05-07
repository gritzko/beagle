//
//  BRAM — Cohen patience diff over u64 token-hash arrays.  See BRAM.h
//  for the algorithm; this file implements the driver on top of
//  abc/DIFF's `DIFFu64s` Myers LCS.
//
#include "BRAM.h"

#include <stdlib.h>     // qsort
#include <string.h>     // memset

#include "abc/PRO.h"
#include "abc/RAP.h"

// --- DIFF u64 template instantiation (for the inner LCS) -------------
#define X(M, name) M##u64##name
#include "abc/DIFFx.h"
#undef X

// --- FNV-1a 64 line-hash fold ---------------------------------------

#define BRAM_FNV_INIT  0xcbf29ce484222325ULL
#define BRAM_FNV_PRIME 0x100000001b3ULL

//  Hash of "\n" — the NL boundary every weave_blob_cb-tokenised
//  whitespace `\n` carries (after the split).  Computed once.
static u64 bram_nl_hash_cache = 0;

static u64 bram_nl_hash(void) {
    if (bram_nl_hash_cache == 0) {
        u8 nl = '\n';
        u8cs s = {&nl, &nl + 1};
        bram_nl_hash_cache = RAPHash(s);
    }
    return bram_nl_hash_cache;
}

// --- Line index ------------------------------------------------------

typedef struct {
    u64 hash;       // FNV-fold of the line's token hashes
    u32 tok_lo;     // first token index (inclusive)
    u32 tok_hi;     // first token past this line (exclusive)
} bram_line;

//  Split a u64 hash array into lines.  Caller-allocated `lines` must
//  have capacity >= ntok+1.  Returns the line count.
static u32 bram_lines(bram_line *lines, u64cp hashes, u32 ntok) {
    u64 nl = bram_nl_hash();
    u32 nlines = 0;
    u32 line_start = 0;
    u64 h = BRAM_FNV_INIT;
    for (u32 i = 0; i < ntok; i++) {
        h ^= hashes[i];
        h *= BRAM_FNV_PRIME;
        if (hashes[i] == nl) {
            lines[nlines].hash   = h;
            lines[nlines].tok_lo = line_start;
            lines[nlines].tok_hi = i + 1;
            nlines++;
            line_start = i + 1;
            h = BRAM_FNV_INIT;
        }
    }
    if (line_start < ntok) {
        //  Trailing partial line (no terminating \n).
        lines[nlines].hash   = h;
        lines[nlines].tok_lo = line_start;
        lines[nlines].tok_hi = ntok;
        nlines++;
    }
    return nlines;
}

// --- Anchor extraction ----------------------------------------------

//  Sortable (hash, side, line_idx) tuple.  Sort by hash; ties by side
//  (so OLD-side neighbours cluster, then NEW-side); on the second walk
//  we count per-side occurrences within each hash group.
typedef struct {
    u64 hash;
    u32 idx;        // line index on its side
    u8  side;       // 0 = OLD, 1 = NEW
    u8  pad[3];
} bram_pair;

static int bram_pair_cmp(void const *a, void const *b) {
    bram_pair const *pa = (bram_pair const *)a;
    bram_pair const *pb = (bram_pair const *)b;
    if (pa->hash < pb->hash) return -1;
    if (pa->hash > pb->hash) return  1;
    if (pa->side < pb->side) return -1;
    if (pa->side > pb->side) return  1;
    return 0;
}

// --- Region recursion (token-level) ---------------------------------

//  Recurse into a sub-slice of the original token-hash arrays and
//  append its EDL entries to `edl`.  Tries patience again first
//  (unique-within-region lines may exist even when unique-on-the-
//  whole-file lines don't), bottoming out at plain `DIFFu64s` when
//  no anchors are found.  Falls back to wholesale DEL+INS on
//  budget bail-out.
static ok64 bram_region(e32g edl, i32s work,
                        u64cp old_h, u32 oa, u32 ob,
                        u64cp new_h, u32 na, u32 nb) {
    u32 olen = ob - oa;
    u32 nlen = nb - na;
    if (olen == 0 && nlen == 0) return OK;
    if (olen == 0) {
        if (edl[0] >= edl[1]) return DIFFNOROOM;
        *edl[0]++ = DIFF_ENTRY(DIFF_INS, nlen);
        return OK;
    }
    if (nlen == 0) {
        if (edl[0] >= edl[1]) return DIFFNOROOM;
        *edl[0]++ = DIFF_ENTRY(DIFF_DEL, olen);
        return OK;
    }
    u64cs ra = {old_h + oa, old_h + ob};
    u64cs rb = {new_h + na, new_h + nb};
    //  Recursive patience: re-anchor on lines that are unique within
    //  this region but weren't unique in the parent.  Bottoms out at
    //  DIFFu64s when no within-region anchors exist.
    ok64 r = BRAMu64s(edl, work, ra, rb);
    if (r == OK) return OK;
    if (edl[0] + 2 > edl[1]) return DIFFNOROOM;
    *edl[0]++ = DIFF_ENTRY(DIFF_DEL, olen);
    *edl[0]++ = DIFF_ENTRY(DIFF_INS, nlen);
    return OK;
}

// --- Public API ------------------------------------------------------

ok64 BRAMu64s(e32g edl, i32s work, u64cs old_hashes, u64cs new_hashes) {
    sane(edl != NULL && work != NULL);
    u32 na = (u32)$len(old_hashes);
    u32 nb = (u32)$len(new_hashes);

    if (na == 0 && nb == 0) return OK;
    if (na == 0) {
        if (edl[0] >= edl[1]) return DIFFNOROOM;
        *edl[0]++ = DIFF_ENTRY(DIFF_INS, nb);
        return OK;
    }
    if (nb == 0) {
        if (edl[0] >= edl[1]) return DIFFNOROOM;
        *edl[0]++ = DIFF_ENTRY(DIFF_DEL, na);
        return OK;
    }

    u64cp old_h = old_hashes[0];
    u64cp new_h = new_hashes[0];

    //  Step 1: split into lines (allocate capacity for the worst case
    //  where every token ends a line).
    Bu8 la_buf = {}, lb_buf = {};
    ok64 r = OK;
    if ((r = u8bAlloc(la_buf, (na + 1) * sizeof(bram_line))) != OK) return r;
    if ((r = u8bAlloc(lb_buf, (nb + 1) * sizeof(bram_line))) != OK) {
        u8bFree(la_buf); return r;
    }
    bram_line *lines_a = (bram_line *)la_buf[0];
    bram_line *lines_b = (bram_line *)lb_buf[0];
    u32 la_n = bram_lines(lines_a, old_h, na);
    u32 lb_n = bram_lines(lines_b, new_h, nb);

    //  No lines on at least one side → nothing for patience to anchor.
    //  Fall back to plain Myers on the whole input.
    if (la_n == 0 || lb_n == 0) {
        u8bFree(la_buf); u8bFree(lb_buf);
        return DIFFu64s(edl, work, old_hashes, new_hashes);
    }

    //  Step 2: count line-hashes on each side, mark unique-on-both
    //  lines as anchors.
    Bu8 pairs_buf = {};
    if ((r = u8bAlloc(pairs_buf, (la_n + lb_n) * sizeof(bram_pair))) != OK) {
        u8bFree(la_buf); u8bFree(lb_buf); return r;
    }
    bram_pair *pairs = (bram_pair *)pairs_buf[0];
    u32 npairs = 0;
    for (u32 i = 0; i < la_n; i++) {
        pairs[npairs].hash = lines_a[i].hash;
        pairs[npairs].idx  = i;
        pairs[npairs].side = 0;
        npairs++;
    }
    for (u32 i = 0; i < lb_n; i++) {
        pairs[npairs].hash = lines_b[i].hash;
        pairs[npairs].idx  = i;
        pairs[npairs].side = 1;
        npairs++;
    }
    qsort(pairs, npairs, sizeof(bram_pair), bram_pair_cmp);

    Bu8 ina_buf = {}, inb_buf = {};
    if ((r = u8bAlloc(ina_buf, la_n)) != OK) {
        u8bFree(la_buf); u8bFree(lb_buf); u8bFree(pairs_buf); return r;
    }
    if ((r = u8bAlloc(inb_buf, lb_n)) != OK) {
        u8bFree(la_buf); u8bFree(lb_buf); u8bFree(pairs_buf);
        u8bFree(ina_buf); return r;
    }
    u8 *is_anchor_a = ina_buf[0];
    u8 *is_anchor_b = inb_buf[0];
    memset(is_anchor_a, 0, la_n);
    memset(is_anchor_b, 0, lb_n);

    //  Walk pairs grouped by hash; each hash with count_a == count_b
    //  == 1 contributes one anchor on each side.
    {
        u32 i = 0;
        while (i < npairs) {
            u32 j = i;
            u32 ca = 0, cb = 0;
            u32 ia = 0, ib = 0;
            while (j < npairs && pairs[j].hash == pairs[i].hash) {
                if (pairs[j].side == 0) { ca++; ia = pairs[j].idx; }
                else                     { cb++; ib = pairs[j].idx; }
                j++;
            }
            if (ca == 1 && cb == 1) {
                is_anchor_a[ia] = 1;
                is_anchor_b[ib] = 1;
            }
            i = j;
        }
    }
    u8bFree(pairs_buf);

    //  Step 3: build anchor-hash arrays in source order; LCS them.
    Bu8 ah_a_buf = {}, ah_b_buf = {};
    Bu8 ai_a_buf = {}, ai_b_buf = {};
    if ((r = u8bAlloc(ah_a_buf, la_n * sizeof(u64))) != OK) goto fail0;
    if ((r = u8bAlloc(ah_b_buf, lb_n * sizeof(u64))) != OK) goto fail1;
    if ((r = u8bAlloc(ai_a_buf, la_n * sizeof(u32))) != OK) goto fail2;
    if ((r = u8bAlloc(ai_b_buf, lb_n * sizeof(u32))) != OK) goto fail3;

    u64 *ah_a = (u64 *)ah_a_buf[0]; u32 ana = 0;
    u64 *ah_b = (u64 *)ah_b_buf[0]; u32 anb = 0;
    u32 *ai_a = (u32 *)ai_a_buf[0];     // line index into lines_a
    u32 *ai_b = (u32 *)ai_b_buf[0];     // line index into lines_b
    for (u32 k = 0; k < la_n; k++) if (is_anchor_a[k]) {
        ah_a[ana] = lines_a[k].hash;
        ai_a[ana] = k;
        ana++;
    }
    for (u32 k = 0; k < lb_n; k++) if (is_anchor_b[k]) {
        ah_b[anb] = lines_b[k].hash;
        ai_b[anb] = k;
        anb++;
    }
    u8bFree(ina_buf);
    u8bFree(inb_buf);

    //  No anchors on either side — fall back.
    if (ana == 0 || anb == 0) {
        u8bFree(la_buf); u8bFree(lb_buf);
        u8bFree(ah_a_buf); u8bFree(ah_b_buf);
        u8bFree(ai_a_buf); u8bFree(ai_b_buf);
        return DIFFu64s(edl, work, old_hashes, new_hashes);
    }

    Bu32 anchor_edl_buf = {};
    Bi32 anchor_work_buf = {};
    u64  aedl_sz  = DIFFEdlMaxEntries((u64)ana, (u64)anb);
    u64  awork_sz = DIFFWorkSize((u64)ana, (u64)anb);
    if ((r = u32bAllocate(anchor_edl_buf,  aedl_sz))  != OK) goto fail4;
    if ((r = i32bAllocate(anchor_work_buf, awork_sz)) != OK) goto fail5;

    e32g aedl = {anchor_edl_buf[0], anchor_edl_buf[3], anchor_edl_buf[0]};
    i32s awork = {i32bHead(anchor_work_buf), i32bTerm(anchor_work_buf)};
    u64cs aha_s = {ah_a, ah_a + ana};
    u64cs ahb_s = {ah_b, ah_b + anb};
    ok64 ar = DIFFu64s(aedl, awork, aha_s, ahb_s);
    if (ar != OK) {
        //  Anchor LCS hit budget — fall back to plain Myers.
        u32bFree(anchor_edl_buf); i32bFree(anchor_work_buf);
        u8bFree(la_buf); u8bFree(lb_buf);
        u8bFree(ah_a_buf); u8bFree(ah_b_buf);
        u8bFree(ai_a_buf); u8bFree(ai_b_buf);
        return DIFFu64s(edl, work, old_hashes, new_hashes);
    }

    //  Step 4 + 5: walk anchor EDL, recurse on between-anchor regions
    //  and emit matched-anchor EQ runs.
    u32 a_idx = 0, b_idx = 0;       // anchor cursors (in ah_a/ah_b)
    u32 a_tok = 0, b_tok = 0;       // token cursors (in old_h/new_h)

    e32c *aep = aedl[2];
    e32c *aee = aedl[0];
    while (aep < aee) {
        u32 op  = DIFF_OP(*aep);
        u32 len = DIFF_LEN(*aep);
        if (op == DIFF_EQ) {
            for (u32 m = 0; m < len; m++) {
                u32 line_a = ai_a[a_idx + m];
                u32 line_b = ai_b[b_idx + m];
                u32 tok_a_lo = lines_a[line_a].tok_lo;
                u32 tok_a_hi = lines_a[line_a].tok_hi;
                u32 tok_b_lo = lines_b[line_b].tok_lo;
                u32 tok_b_hi = lines_b[line_b].tok_hi;

                //  Region between previous anchor (or start) and this
                //  one — token-level Myers handles intra-region detail.
                ok64 ro = bram_region(edl, work, old_h, a_tok, tok_a_lo,
                                                  new_h, b_tok, tok_b_lo);
                if (ro != OK) { r = ro; goto cleanup; }

                //  Anchor itself — full EQ run.
                u32 elen = tok_a_hi - tok_a_lo;
                if (edl[0] >= edl[1]) { r = DIFFNOROOM; goto cleanup; }
                //  Coalesce with a trailing EQ on edl[0]-1 (DIFFu64s
                //  may have just emitted one for the region).
                if (edl[0] > edl[2] && DIFF_OP(*(edl[0] - 1)) == DIFF_EQ) {
                    e32 *prev = edl[0] - 1;
                    *prev = DIFF_ENTRY(DIFF_EQ, DIFF_LEN(*prev) + elen);
                } else {
                    *edl[0]++ = DIFF_ENTRY(DIFF_EQ, elen);
                }

                a_tok = tok_a_hi;
                b_tok = tok_b_hi;
            }
            a_idx += len;
            b_idx += len;
        } else if (op == DIFF_DEL) {
            //  Unmatched anchor on OLD — its tokens stay in the next
            //  region (they're between a_tok and the next matched
            //  anchor's token-lo).  Just bump the anchor cursor.
            a_idx += len;
        } else { // DIFF_INS
            b_idx += len;
        }
        aep++;
    }

    //  Trailing region after the last matched anchor (or whole input
    //  when no anchor matched).
    {
        ok64 ro = bram_region(edl, work, old_h, a_tok, na,
                                          new_h, b_tok, nb);
        if (ro != OK) r = ro;
    }

cleanup:
    u32bFree(anchor_edl_buf);
    i32bFree(anchor_work_buf);
fail5: (void)0;
fail4:
    u8bFree(ai_b_buf);
fail3:
    u8bFree(ai_a_buf);
fail2:
    u8bFree(ah_b_buf);
fail1:
    u8bFree(ah_a_buf);
fail0:
    u8bFree(la_buf);
    u8bFree(lb_buf);
    return r;
}
