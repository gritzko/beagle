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
        a_u8cs(s, '\n');
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
                        u64cs old_region, u64cs new_region) {
    u32 olen = (u32)$len(old_region);
    u32 nlen = (u32)$len(new_region);
    if (olen == 0 && nlen == 0) return OK;
    if (olen == 0) return DIFFu64AddEntry(edl, DIFF_INS, nlen);
    if (nlen == 0) return DIFFu64AddEntry(edl, DIFF_DEL, olen);
    //  Recursive patience: re-anchor on lines that are unique within
    //  this region but weren't unique in the parent.  Bottoms out at
    //  DIFFu64s when no within-region anchors exist.  `BRAMu64s` is
    //  BASS-neutral (it marks/rewinds its own scratch — see below), so
    //  this plain recursion no longer piles per-level scratch on BASS
    //  across the recursion tree (MEM-018).
    ok64 r = BRAMu64s(edl, work, old_region, new_region);
    if (r == OK) return OK;
    ok64 d = DIFFu64AddEntry(edl, DIFF_DEL, olen); if (d != OK) return d;
    return DIFFu64AddEntry(edl, DIFF_INS, nlen);
}

// --- Core (BASS scratch may be left dangling; the wrapper rewinds) ---
//
//  Forward decl so bram_region (above) can recurse through the
//  BASS-neutral public wrapper, not into the core directly.
static ok64 bram_core(e32g edl, i32s work, u64cs old_hashes, u64cs new_hashes);

// --- Public API ------------------------------------------------------
//
//  MEM-018: every buffer bram_core carves (line/pair/anchor arrays, the
//  inner anchor edl/work) is pure scratch — only the caller-owned `edl`
//  carries results out.  bram_core is invoked by a plain (non-call())
//  recursion from bram_region, so without an explicit rewind its scratch
//  would accumulate `depth × region_size` on BASS until `a_carve`
//  returns NOROOM and the diff silently truncates.  Mark BASS on entry
//  and rewind on exit, making BRAMu64s BASS-neutral at every recursion
//  level — the high-water stays bounded by a single level's footprint.
ok64 BRAMu64s(e32g edl, i32s work, u64cs old_hashes, u64cs new_hashes) {
    sane(edl != NULL && work != NULL);
    u8 *mark = u8aMark(ABC_BASS);
    ok64 r = bram_core(edl, work, old_hashes, new_hashes);
    u8aRewind(ABC_BASS, mark);
    return r;
}

static ok64 bram_core(e32g edl, i32s work, u64cs old_hashes, u64cs new_hashes) {
    sane(edl != NULL && work != NULL);
    u32 na = (u32)$len(old_hashes);
    u32 nb = (u32)$len(new_hashes);

    if (na == 0 && nb == 0) return OK;
    if (na == 0) return DIFFu64AddEntry(edl, DIFF_INS, nb);
    if (nb == 0) return DIFFu64AddEntry(edl, DIFF_DEL, na);

    u64cp old_h = old_hashes[0];
    u64cp new_h = new_hashes[0];

    //  Step 1: split into lines (allocate capacity for the worst case
    //  where every token ends a line).  All scratch on BASS — auto-
    //  rewound at the caller's call() boundary.  Note: a_carve does NOT
    //  zero — use a_carve0 / memset where the original u8bAlloc relied
    //  on Balloc's zero-fill.
    a_carve(u8, la_buf, (na + 1) * sizeof(bram_line));
    a_carve(u8, lb_buf, (nb + 1) * sizeof(bram_line));
    bram_line *lines_a = (bram_line *)la_buf[0];
    bram_line *lines_b = (bram_line *)lb_buf[0];
    u32 la_n = bram_lines(lines_a, old_h, na);
    u32 lb_n = bram_lines(lines_b, new_h, nb);

    //  No lines on at least one side → nothing for patience to anchor.
    //  Fall back to plain Myers on the whole input.
    if (la_n == 0 || lb_n == 0)
        return DIFFu64s(edl, work, old_hashes, new_hashes);

    //  Step 2: count line-hashes on each side, mark unique-on-both
    //  lines as anchors.
    a_carve(u8, pairs_buf, (la_n + lb_n) * sizeof(bram_pair));
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

    a_carve(u8, ina_buf, la_n);
    a_carve(u8, inb_buf, lb_n);
    //  u8bAlloc would have zero-filled; a_carve doesn't, so do it manually:
    //  the anchor flags below rely on default-0.
    memset(ina_buf[0], 0, la_n);
    memset(inb_buf[0], 0, lb_n);
    u8 *is_anchor_a = ina_buf[0];
    u8 *is_anchor_b = inb_buf[0];

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

    //  Step 3: build anchor-hash arrays in source order; LCS them.
    a_carve(u8, ah_a_buf, la_n * sizeof(u64));
    a_carve(u8, ah_b_buf, lb_n * sizeof(u64));
    a_carve(u8, ai_a_buf, la_n * sizeof(u32));
    a_carve(u8, ai_b_buf, lb_n * sizeof(u32));

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

    //  No anchors on either side — fall back.
    if (ana == 0 || anb == 0)
        return DIFFu64s(edl, work, old_hashes, new_hashes);

    u64  aedl_sz  = DIFFEdlMaxEntries((u64)ana, (u64)anb);
    u64  awork_sz = DIFFWorkSize((u64)ana, (u64)anb);
    a_carve(u32, anchor_edl_buf,  aedl_sz);
    a_carve(i32, anchor_work_buf, awork_sz);

    e32g aedl = {anchor_edl_buf[0], anchor_edl_buf[3], anchor_edl_buf[0]};
    i32s awork = {i32bHead(anchor_work_buf), i32bTerm(anchor_work_buf)};
    u64cs aha_s = {ah_a, ah_a + ana};
    u64cs ahb_s = {ah_b, ah_b + anb};
    ok64 ar = DIFFu64s(aedl, awork, aha_s, ahb_s);
    if (ar != OK) {
        //  Anchor LCS hit budget — fall back to plain Myers.
        return DIFFu64s(edl, work, old_hashes, new_hashes);
    }
    ok64 r = OK;

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
                a_rest(u64c, oa_rest, old_hashes, a_tok);
                a_head(u64c, oreg, oa_rest, tok_a_lo - a_tok);
                a_rest(u64c, nb_rest, new_hashes, b_tok);
                a_head(u64c, nreg, nb_rest, tok_b_lo - b_tok);
                ok64 ro = bram_region(edl, work, oreg, nreg);
                if (ro != OK) { r = ro; goto cleanup; }

                //  Anchor itself — full EQ run.  DIFFu64AddEntry
                //  handles the bounds check + coalesce with a
                //  trailing EQ that DIFFu64s may have just emitted.
                u32 elen = tok_a_hi - tok_a_lo;
                ok64 ae = DIFFu64AddEntry(edl, DIFF_EQ, elen);
                if (ae != OK) { r = ae; goto cleanup; }

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
        a_rest(u64c, oreg, old_hashes, a_tok);
        a_rest(u64c, nreg, new_hashes, b_tok);
        ok64 ro = bram_region(edl, work, oreg, nreg);
        if (ro != OK) r = ro;
    }

    return r;

cleanup:
    return r;
}
