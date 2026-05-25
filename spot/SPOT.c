#include "SPOT.h"

#include <string.h>

#include "abc/PRO.h"

// --- Tokenization callback context ---

typedef struct {
    u32bp toks;
    u32   off;  // running byte offset (start of current token)
} SPOTTokCtx;

static ok64 spot_tok_cb(u8 tag, u8cs tok, void *ctx) {
    sane(ctx != NULL);
    SPOTTokCtx *c = (SPOTTokCtx *)ctx;
    u32 end = c->off + (u32)$len(tok);
    u32 packed = tok32Pack(tag,end);
    call(u32bFeed1, c->toks, packed);
    c->off = end;
    return OK;
}

// --- SPOTTokenize ---

ok64 SPOTTokenize(u32bp toks, u8csc source, u8csc ext) {
    sane(toks != NULL && $ok(source));
    if ($empty(source)) done;

    SPOTTokCtx ctx = {.toks = toks, .off = 0};

    // Strip leading dot from ext for tok/
    u8cs ext_nodot = {};
    if (!$empty(ext) && ext[0][0] == '.') {
        ext_nodot[0] = ext[0] + 1;
        ext_nodot[1] = ext[1];
    } else {
        $mv(ext_nodot, ext);
    }

    // Try tok/ lexer
    TOKstate ts = {
        .data = {source[0], source[1]},
        .cb = spot_tok_cb,
        .ctx = &ctx,
    };
    call(TOKLexer, &ts, ext_nodot);
    done;
}

// --- Helper functions ---

static b8 spot_is_placeholder(u8cs val) {
    if ($len(val) != 1) return (NO);
    u8 c = val[0][0];
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int spot_bind_index(u8 c) {
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c >= 'A' && c <= 'Z') return 26 + (c - 'A');
    return (-1);
}

static b8 spot_is_lower(u8 c) { return c >= 'a' && c <= 'z'; }

static b8 spot_is_whitespace(u8cs val) {
    $for(u8c, p, val) {
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') return (NO);
    }
    return (YES);
}

static int spot_count_spaces(u8cs val) {
    int n = 0;
    $for(u8c, p, val) {
        if (*p == ' ')
            n++;
        else
            break;
    }
    return (n);
}

static int spot_bracket_dir(u8cs val) {
    if ($len(val) != 1) return (0);
    u8 c = val[0][0];
    if (c == '{' || c == '(' || c == '[') return (1);
    if (c == '}' || c == ')' || c == ']') return (-1);
    return (0);
}

// --- Advance to next meaningful token position ---
// Skips comments (D) and whitespace (W) tokens.
// Returns the position of the next meaningful token, or len if exhausted.
static int spot_skip_ws(u32cs toks, u8cp base, int pos, int len) {
    (void)base;
    while (pos < len) {
        u8 tag = tok32Tag(toks[0][pos]);
        if (tag == 'D' || tag == 'W') { pos++; continue; }
        break;
    }
    return pos;
}

// --- Flatten needle tokens ---

static ok64 spot_flatten_needle(SPOTntok *flat, int *nflat,
                                u32cs toks, u8cp base) {
    sane(flat != NULL);
    int len = (int)$len(toks);
    *nflat = 0;
    b8 pending_skip = NO;

    for (int i = 0; i < len; i++) {
        u8 tag = tok32Tag(toks[0][i]);
        u8cs val = {}; tok32Val(val,toks,base,i);

        if (tag == 'D') continue;  // skip comments
        if (tag == 'W') {
            int sp = spot_count_spaces(val);
            if (sp >= 2) pending_skip = YES;
            continue;
        }
        if ($len(val) == 0) continue;

        if (*nflat >= SPOT_MAX_NTOKS) fail(SPOTBAD);
        flat[*nflat].type = tag;
        flat[*nflat].val[0] = val[0];
        flat[*nflat].val[1] = val[1];
        flat[*nflat].skip = (*nflat > 0) ? pending_skip : NO;
        (*nflat)++;
        pending_skip = NO;
    }
    done;
}

// --- Backtracking state ---

typedef struct {
    u64  bound;
    range32 subs[SPOT_MAX_SUBS];
    int  nsubs;
    match32 bind_matches[SPOT_MAX_BINDS];
    match32p ranges[4];
    u8cp source_base;
    u8cp ndl_base;
} SPOTbinds;

typedef struct {
    int pos;
    SPOTbinds binds;
} SPOTsave;

// Record a matched token range into the ranges buffer (if present)
static void spot_record_range(SPOTbinds *b, u32 ndl_lo, u32 ndl_hi,
                             u32 hay_lo, u32 hay_hi) {
    if (b->ranges[0] == NULL) return;
    match32 entry = {.hay = {hay_lo, hay_hi}, .ndl = {ndl_lo, ndl_hi}};
    match32bFeed1(b->ranges, entry);
}

// --- Find next literal anchor in needle ---
// Scans ntoks[from+1..] for first literal (non-placeholder) token
// before any skip boundary. Returns index, or -1 if none found.
static int spot_find_anchor(SPOTntok *ntoks, int nntoks, int from) {
    for (int k = from + 1; k < nntoks; k++) {
        if (ntoks[k].skip) return -1;
        if (!spot_is_placeholder(ntoks[k].val)) return k;
    }
    return -1;
}

// --- Flat matching engine ---

static ok64 spot_match_flat(SPOTbinds *b, SPOTntok *ntoks, int nntoks,
                           int from, u32cs htoks, u8cp hbase,
                           int *hpos, int brace);

static ok64 spot_match_flat(SPOTbinds *b, SPOTntok *ntoks, int nntoks,
                           int from, u32cs htoks, u8cp hbase,
                           int *hpos, int brace) {
    sane(b != NULL);
    int hlen = (int)$len(htoks);
    if (from >= nntoks) done;

    SPOTntok *cur = &ntoks[from];

    if (!cur->skip) {
        // EXACT: get next meaningful token, must match
        int pos = spot_skip_ws(htoks, hbase, *hpos, hlen);
        if (pos >= hlen) fail(SPOTNONE);

        u8cs hv = {}; tok32Val(hv,htoks,hbase,pos);
        u32 leaf_srclo = (pos > 0) ? tok32Offset(htoks[0][pos - 1]) : 0;
        u32 leaf_srchi = tok32Offset(htoks[0][pos]);
        *hpos = pos + 1;

        // Record first token position for segment tracking
        if (from == 0) {
            if (b->nsubs >= SPOT_MAX_SUBS) fail(SPOTNONE);
            b->subs[b->nsubs++] = (range32){leaf_srclo, leaf_srchi};
        }
        if (b->nsubs > 0) b->subs[b->nsubs - 1].hi = leaf_srchi;

        // Needle token range
        u32 ndl_lo = (u32)(cur->val[0] - b->ndl_base);
        u32 ndl_hi = (u32)(cur->val[1] - b->ndl_base);

        // Check placeholder
        if (spot_is_placeholder(cur->val)) {
            u8 c = cur->val[0][0];
            int idx = spot_bind_index(c);
            if (idx < 0) fail(SPOTNONE);
            u64 bit = 1ULL << idx;

            if (spot_is_lower(c)) {
                // Lowercase: bind to a single non-punct leaf token.
                // Punctuation tokens (P) — operators, brackets, ';' ','
                // — cannot satisfy a lowercase binding.  Use uppercase
                // for multi-token / bracket groups.
                if (tok32Tag(htoks[0][pos]) == 'P') fail(SPOTNONE);
                u32 cap_srchi = leaf_srchi;
                if (b->nsubs > 0) b->subs[b->nsubs - 1].hi = cap_srchi;
                if (b->bound & bit) {
                    u8cs bnd = {b->source_base + b->bind_matches[idx].hay.lo,
                                b->source_base + b->bind_matches[idx].hay.hi};
                    u8cs cap = {b->source_base + leaf_srclo,
                                b->source_base + cap_srchi};
                    if (!u8csEq(bnd, cap)) fail(SPOTNONE);
                } else {
                    b->bound |= bit;
                    b->bind_matches[idx] = (match32){
                        .hay = {leaf_srclo, cap_srchi},
                        .ndl = {ndl_lo, ndl_hi}};
                }
                spot_record_range(b, ndl_lo, ndl_hi, leaf_srclo, cap_srchi);
            } else {
                // Uppercase: consume tokens until remaining needle matches
                if (b->bound & bit) {
                    u32 bound_len = b->bind_matches[idx].hay.hi -
                                    b->bind_matches[idx].hay.lo;
                    u32 cap_end = leaf_srclo + bound_len;
                    u32 consumed = leaf_srchi;
                    while (consumed < cap_end) {
                        int p2 = spot_skip_ws(htoks, hbase, *hpos, hlen);
                        if (p2 >= hlen) fail(SPOTNONE);
                        *hpos = p2 + 1;
                        consumed = tok32Offset(htoks[0][p2]);
                    }
                    if (consumed != cap_end) fail(SPOTNONE);
                    u8cs bnd = {b->source_base + b->bind_matches[idx].hay.lo,
                                b->source_base + b->bind_matches[idx].hay.hi};
                    u8cs cap = {b->source_base + leaf_srclo,
                                b->source_base + cap_end};
                    if (!u8csEq(bnd, cap)) fail(SPOTNONE);
                    if (b->nsubs > 0) b->subs[b->nsubs - 1].hi = consumed;
                } else {
                    // Unbound uppercase: anchor-guided extension.
                    // Refuse to start the binding on a closing bracket
                    // — that would leave the capture unbalanced before
                    // we even begin extending.
                    if (spot_bracket_dir(hv) < 0) fail(SPOTNONE);
                    int anchor_idx = spot_find_anchor(ntoks, nntoks, from);
                    int gap = (anchor_idx >= 0) ? anchor_idx - from - 1 : -1;

                    u32 cap_srchi = leaf_srchi;
                    b->bound |= bit;
                    b->bind_matches[idx] = (match32){
                        .hay = {leaf_srclo, cap_srchi},
                        .ndl = {ndl_lo, ndl_hi}};
                    int cap_brace = spot_bracket_dir(hv);

                    if (gap < 0) {
                        // No anchor: shortest-first single try
                        if (b->nsubs > 0) b->subs[b->nsubs - 1].hi = cap_srchi;
                        SPOTsave sv = {.pos = *hpos, .binds = *b};
                        ok64 r = spot_match_flat(b, ntoks, nntoks, from + 1,
                                               htoks, hbase, hpos, brace);
                        if (r == OK) done;
                        *hpos = sv.pos; *b = sv.binds;
                        fail(SPOTNONE);
                    }

                    // Anchor-guided: lookahead window of gap+1 positions
                    int ahead[SPOT_MAX_NTOKS];
                    int nahead = 0;
                    int la_scan = *hpos;

                    // Pre-fill lookahead
                    for (int g = 0; g <= gap && g < SPOT_MAX_NTOKS; g++) {
                        int p = spot_skip_ws(htoks, hbase, la_scan, hlen);
                        if (p >= hlen) break;
                        ahead[nahead++] = p;
                        la_scan = p + 1;
                    }

                    b8 at_boundary = NO;
                    for (;;) {
                        // Check anchor at ahead[gap]
                        if (nahead > gap) {
                            u8cs av = {}; tok32Val(av,htoks,hbase,ahead[gap]);
                            if (cap_brace == 0 && u8csEq(av, ntoks[anchor_idx].val)) {
                                if (b->nsubs > 0) b->subs[b->nsubs - 1].hi = cap_srchi;
                                b->bind_matches[idx].hay.hi = cap_srchi;
                                SPOTsave sv = {.pos = *hpos, .binds = *b};
                                *hpos = ahead[0];
                                ok64 r = spot_match_flat(b, ntoks, nntoks, from + 1,
                                                       htoks, hbase, hpos, brace);
                                if (r == OK) done;
                                *hpos = sv.pos; *b = sv.binds;
                            }
                        }

                        if (at_boundary) fail(SPOTNONE);

                        // Extend: consume ahead[0] into uppercase capture
                        if (nahead == 0) fail(SPOTNONE);
                        u8cs cv = {}; tok32Val(cv,htoks,hbase,ahead[0]);
                        cap_brace += spot_bracket_dir(cv);
                        if (cap_brace < 0) fail(SPOTNONE);
                        if (brace + cap_brace == 0) {
                            if (spot_bracket_dir(cv) < 0)
                                at_boundary = YES;
                            if ($len(cv) == 1 && cv[0][0] == ';')
                                at_boundary = YES;
                        }
                        cap_srchi = tok32Offset(htoks[0][ahead[0]]);
                        *hpos = ahead[0] + 1;

                        // Shift window
                        memmove(ahead, ahead + 1, (nahead - 1) * sizeof(int));
                        nahead--;

                        // Refill one position
                        int p = spot_skip_ws(htoks, hbase, la_scan, hlen);
                        if (p < hlen) {
                            ahead[nahead++] = p;
                            la_scan = p + 1;
                        }
                    }
                }
            }
        } else {
            // Literal: match by value
            if (!u8csEq(cur->val, hv)) fail(SPOTNONE);
            brace += spot_bracket_dir(cur->val);
            spot_record_range(b, ndl_lo, ndl_hi, leaf_srclo, leaf_srchi);
        }

        // Recurse for remaining
        call(spot_match_flat, b, ntoks, nntoks, from + 1,
             htoks, hbase, hpos, brace);
        done;
    }

    // SKIP: scan forward trying each token position with backtracking.
    {
        u32 skip_ndl_lo = (u32)(cur->val[0] - b->ndl_base);
        u32 skip_ndl_hi = (u32)(cur->val[1] - b->ndl_base);
        SPOTbinds saved_binds = *b;
        int scan_brace = 0;
        b8 is_close_ph = NO;
        int ndl_bdir = 0;
        if (!spot_is_placeholder(cur->val)) {
            ndl_bdir = spot_bracket_dir(cur->val);
            is_close_ph = ndl_bdir < 0;
        }
        for (;;) {
            int pos = spot_skip_ws(htoks, hbase, *hpos, hlen);
            if (pos >= hlen) fail(SPOTNONE);

            u8cs hv = {}; tok32Val(hv,htoks,hbase,pos);
            u32 leaf_srclo = (pos > 0) ? tok32Offset(htoks[0][pos - 1]) : 0;
            u32 leaf_srchi = tok32Offset(htoks[0][pos]);
            *hpos = pos + 1;

            int bd = spot_bracket_dir(hv);
            scan_brace += bd;
            if (is_close_ph) {
                if (scan_brace < -brace) fail(SPOTNONE);
            } else {
                if (scan_brace < 0) fail(SPOTNONE);
            }

            // Save cursor state after consuming this token
            int post_pos = *hpos;

            // Record new segment
            if (b->nsubs >= SPOT_MAX_SUBS) fail(SPOTNONE);
            b->subs[b->nsubs++] = (range32){leaf_srclo, leaf_srchi};

            b8 is_ph = spot_is_placeholder(cur->val);
            u8 ph_c = is_ph ? cur->val[0][0] : 0;
            int ph_idx = is_ph ? spot_bind_index(ph_c) : -1;
            b8 is_upper = is_ph && !spot_is_lower(ph_c);

            if (is_upper && ph_idx >= 0) {
                u64 bit = 1ULL << ph_idx;
                if (b->bound & bit) {
                    *b = saved_binds;
                    continue;
                }
                // Don't start an unbound uppercase capture on a
                // closing bracket — it would be unbalanced.
                if (spot_bracket_dir(hv) < 0) {
                    *b = saved_binds;
                    continue;
                }
                b->bound |= bit;
                b->bind_matches[ph_idx] = (match32){
                    .hay = {leaf_srclo, leaf_srchi},
                    .ndl = {(u32)(cur->val[0] - b->ndl_base),
                            (u32)(cur->val[1] - b->ndl_base)}};

                int anchor_idx = spot_find_anchor(ntoks, nntoks, from);
                int gap = (anchor_idx >= 0) ? anchor_idx - from - 1 : -1;
                u32 cap_srchi = leaf_srchi;
                int cap_brace2 = spot_bracket_dir(hv);

                if (gap < 0) {
                    // No anchor: single try
                    if (b->nsubs > 0) b->subs[b->nsubs - 1].hi = cap_srchi;
                    SPOTsave sv = {.pos = *hpos, .binds = *b};
                    ok64 r = spot_match_flat(b, ntoks, nntoks, from + 1,
                                            htoks, hbase, hpos,
                                            brace + scan_brace);
                    if (r == OK) done;
                    *hpos = sv.pos; *b = sv.binds;
                } else {
                    // Anchor-guided lookahead
                    int ahead2[SPOT_MAX_NTOKS];
                    int nahead2 = 0;
                    int la_scan2 = *hpos;
                    for (int g = 0; g <= gap && g < SPOT_MAX_NTOKS; g++) {
                        int p = spot_skip_ws(htoks, hbase, la_scan2, hlen);
                        if (p >= hlen) break;
                        ahead2[nahead2++] = p;
                        la_scan2 = p + 1;
                    }
                    b8 matched = NO;
                    b8 at_boundary2 = NO;
                    for (;;) {
                        if (nahead2 > gap) {
                            u8cs av = {}; tok32Val(av,htoks,hbase,ahead2[gap]);
                            if (cap_brace2 == 0 && u8csEq(av, ntoks[anchor_idx].val)) {
                                if (b->nsubs > 0) b->subs[b->nsubs - 1].hi = cap_srchi;
                                b->bind_matches[ph_idx].hay.hi = cap_srchi;
                                SPOTsave sv = {.pos = *hpos, .binds = *b};
                                *hpos = ahead2[0];
                                ok64 r = spot_match_flat(b, ntoks, nntoks, from + 1,
                                                        htoks, hbase, hpos,
                                                        brace + scan_brace);
                                if (r == OK) { matched = YES; break; }
                                *hpos = sv.pos; *b = sv.binds;
                            }
                        }
                        if (at_boundary2) break;
                        if (nahead2 == 0) break;
                        u8cs cv = {}; tok32Val(cv,htoks,hbase,ahead2[0]);
                        cap_brace2 += spot_bracket_dir(cv);
                        if (cap_brace2 < 0) break;
                        if (brace + scan_brace + cap_brace2 == 0) {
                            if (spot_bracket_dir(cv) < 0)
                                at_boundary2 = YES;
                            if ($len(cv) == 1 && cv[0][0] == ';')
                                at_boundary2 = YES;
                        }
                        cap_srchi = tok32Offset(htoks[0][ahead2[0]]);
                        *hpos = ahead2[0] + 1;
                        memmove(ahead2, ahead2 + 1, (nahead2 - 1) * sizeof(int));
                        nahead2--;
                        int p = spot_skip_ws(htoks, hbase, la_scan2, hlen);
                        if (p < hlen) { ahead2[nahead2++] = p; la_scan2 = p + 1; }
                    }
                    if (matched) done;
                }

                // Reset and continue scan
                *hpos = post_pos;
                *b = saved_binds;
                continue;
            }

            b8 tok_match = NO;
            if (is_ph && ph_idx >= 0 && spot_is_lower(ph_c)) {
                u64 bit = 1ULL << ph_idx;
                // Lowercase: bind to a single non-punct leaf token.
                if (tok32Tag(htoks[0][pos]) == 'P') {
                    *b = saved_binds;
                    continue;
                }
                u32 cap_srchi = leaf_srchi;
                if (b->nsubs > 0) b->subs[b->nsubs - 1].hi = cap_srchi;
                if (b->bound & bit) {
                    u8cs bnd2 = {b->source_base + b->bind_matches[ph_idx].hay.lo,
                                 b->source_base + b->bind_matches[ph_idx].hay.hi};
                    u8cs cap = {b->source_base + leaf_srclo,
                                b->source_base + cap_srchi};
                    tok_match = u8csEq(bnd2, cap);
                } else {
                    b->bound |= bit;
                    b->bind_matches[ph_idx] = (match32){
                        .hay = {leaf_srclo, cap_srchi},
                        .ndl = {(u32)(cur->val[0] - b->ndl_base),
                                (u32)(cur->val[1] - b->ndl_base)}};
                    tok_match = YES;
                }
            } else if (!is_ph) {
                tok_match = u8csEq(cur->val, hv);
                if (tok_match && is_close_ph && scan_brace != -brace)
                    tok_match = NO;
            }

            if (tok_match) {
                spot_record_range(b, skip_ndl_lo, skip_ndl_hi, leaf_srclo, leaf_srchi);
                SPOTsave mr = {.pos = *hpos, .binds = *b};
                ok64 r = spot_match_flat(b, ntoks, nntoks, from + 1,
                                        htoks, hbase, hpos,
                                        brace + scan_brace);
                if (r == OK) done;
                *hpos = post_pos;
                *b = mr.binds;
            }

            *b = saved_binds;
        }
    }
}

// --- SPOTInit ---

ok64 SPOTInit(SPOTstate *st, u32bp ndl_toks,
              u8csc needle_src, u8csc ext,
              u32cs hay_toks, u8csc source) {
    sane(st != NULL && ndl_toks != NULL);
    *st = (SPOTstate){};

    // Tokenize needle
    call(SPOTTokenize, ndl_toks, needle_src, ext);

    st->ntoks[0] = (u32cp)u32bDataHead(ndl_toks);
    st->ntoks[1] = (u32cp)u32bIdleHead(ndl_toks);
    st->htoks[0] = hay_toks[0];
    st->htoks[1] = hay_toks[1];
    st->nsrc[0] = needle_src[0];
    st->nsrc[1] = needle_src[1];
    st->source[0] = source[0];
    st->source[1] = source[1];

    if ($empty(st->ntoks)) fail(SPOTBAD);
    if ($empty(st->htoks)) fail(SPOTBAD);

    // Flatten needle tokens into flat[]
    call(spot_flatten_needle, st->flat, &st->nflat,
         st->ntoks, (u8cp)needle_src[0]);

    st->hpos = 0;
    st->exhausted = NO;
    done;
}

// --- SPOTNext ---

ok64 SPOTNext(SPOTstate *st) {
    sane(st != NULL);
    if (st->exhausted) return (SPOTEND);
    if (st->nflat == 0) return (SPOTEND);

    int hlen = (int)$len(st->htoks);

    for (;;) {
        // Try matching at current position
        int mpos = st->hpos;
        SPOTbinds b = {};
        b.source_base = st->source[0];
        b.ndl_base = (u8cp)st->nsrc[0];
        if (st->ranges[0] != NULL) {
            match32bReset(st->ranges);
            b.ranges[0] = st->ranges[0]; b.ranges[1] = st->ranges[1];
            b.ranges[2] = st->ranges[2]; b.ranges[3] = st->ranges[3];
        }

        ok64 m = spot_match_flat(&b, st->flat, st->nflat, 0,
                                st->htoks, st->source[0],
                                &mpos, 0);
        if (m == OK) {
            if (st->ranges[0] != NULL) st->ranges[2] = b.ranges[2];

            st->bound = b.bound;
            st->src_rng = (b.nsubs > 0)
                ? (range32){b.subs[0].lo, b.subs[b.nsubs - 1].hi}
                : (range32){};
            for (int i = 0; i < SPOT_MAX_BINDS; i++) {
                if (b.bound & (1ULL << i))
                    st->bind_matches[i] = b.bind_matches[i];
                else
                    st->bind_matches[i] = (match32){};
            }
            st->nsubs = (u8)b.nsubs;
            for (int i = 0; i < b.nsubs; i++)
                st->subs[i] = b.subs[i];

            // Advance main haystack by one meaningful token
            int adv = spot_skip_ws(st->htoks, st->source[0], st->hpos, hlen);
            if (adv < hlen)
                st->hpos = adv + 1;
            else
                st->exhausted = YES;

            done;
        }

        // No match — advance by one meaningful token
        int pos = spot_skip_ws(st->htoks, st->source[0], st->hpos, hlen);
        if (pos >= hlen) {
            st->exhausted = YES;
            return (SPOTEND);
        }
        st->hpos = pos + 1;
    }
}

// --- spot_inst_template ---

// Identifier char: [A-Za-z0-9_].
static b8 spot_id_char(u8 c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

// Walk template tmpl, expanding standalone single-letter placeholders
// (a-z, A-Z that aren't adjacent to other identifier chars) into the
// corresponding bound capture; pass everything else through verbatim.
static ok64 spot_inst_template(u8s out, u8csc tmpl,
                              u64 bound,
                              match32 *bind_matches,
                              u8csc source) {
    sane(out[0] != NULL);
    a_dup(u8c, p, tmpl);
    u8 prev = 0;  // 0 acts as "not an ident char" sentinel
    while (!u8csEmpty(p)) {
        u8 c = *u8csHead(p);
        b8 is_alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        if (is_alpha) {
            u8 next = ($len(p) > 1) ? *u8csAtP(p, 1) : 0;
            if (!spot_id_char(prev) && !spot_id_char(next)) {
                int idx = spot_bind_index(c);
                u64 bit = 1ULL << idx;
                if (idx >= 0 && (bound & bit)) {
                    if (bind_matches[idx].hay.lo !=
                        bind_matches[idx].hay.hi) {
                        u8cs bval = {
                            source[0] + bind_matches[idx].hay.lo,
                            source[0] + bind_matches[idx].hay.hi};
                        call(u8sFeed, out, bval);
                    }
                    prev = c;
                    u8csUsed1(p);
                    continue;
                }
            }
        }
        call(u8sFeed1, out, c);
        prev = c;
        u8csUsed1(p);
    }
    done;
}

// --- SPOTReplace ---

#define SPOT_MAX_MATCHES 4096

typedef struct {
    range32 src_rng;
    u64 bound;
    match32 bind_matches[SPOT_MAX_BINDS];
} SPOTrep;

ok64 SPOTReplace(u8s out, u8csc source, u32cs hay_toks,
                 u8csc needle_src, u8csc replace_src, u8csc ext,
                 int *nmatches) {
    sane(out[0] != NULL && source[0] != NULL);

    aBpad(u32, ntoks, 4096);
    SPOTstate st = {};

    call(SPOTInit, &st, ntoks, needle_src, ext, hay_toks, source);

    // Collect all matches (heap buffer, SPOTrep is large)
    Bu8 mbuf = {};
    call(u8bAlloc, mbuf, SPOT_MAX_MATCHES * sizeof(SPOTrep));
    SPOTrep *matches = (SPOTrep *)mbuf[0];
    int nmatch = 0;

    while (SPOTNext(&st) == OK && nmatch < SPOT_MAX_MATCHES) {
        matches[nmatch].src_rng = st.src_rng;
        matches[nmatch].bound = st.bound;
        for (int i = 0; i < SPOT_MAX_BINDS; i++)
            matches[nmatch].bind_matches[i] = st.bind_matches[i];
        nmatch++;
    }

    if (nmatch == 0) {
        u8bFree(mbuf);
        if (nmatches) *nmatches = 0;
        return (SPOTEND);
    }

    // Sort matches by source range ascending
    for (int i = 1; i < nmatch; i++) {
        SPOTrep tmp = matches[i];
        int j = i - 1;
        while (j >= 0 && matches[j].src_rng.lo > tmp.src_rng.lo) {
            matches[j + 1] = matches[j];
            j--;
        }
        matches[j + 1] = tmp;
    }

    // Apply replacements
    u64 pos = 0;
    for (int i = 0; i < nmatch; i++) {
        if (matches[i].src_rng.lo < (u32)pos) continue;

        if (matches[i].src_rng.lo > (u32)pos) {
            u8cs gap = {source[0] + pos, source[0] + matches[i].src_rng.lo};
            ok64 fo = u8sFeed(out, gap);
            if (fo != OK) { u8bFree(mbuf); fail(fo); }
        }

        ok64 io = spot_inst_template(out, replace_src,
             matches[i].bound, matches[i].bind_matches, source);
        if (io != OK) { u8bFree(mbuf); fail(io); }

        pos = matches[i].src_rng.hi;
    }

    if (pos < (u64)$len(source)) {
        u8cs tail = {source[0] + pos, source[1]};
        ok64 fo = u8sFeed(out, tail);
        if (fo != OK) { u8bFree(mbuf); fail(fo); }
    }

    if (nmatches) *nmatches = nmatch;
    u8bFree(mbuf);
    done;
}
