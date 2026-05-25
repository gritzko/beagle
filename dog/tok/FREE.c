#include "FREE.h"

#include "abc/PRO.h"

// Inline ragel lexer (FREE.rl.c, generated from FREE.c.rl)
ok64 FREELexer(FREEstate *state);

typedef struct {
    u8 overlay;
    TOKcb cb;
    void *ctx;
} FREE_overlay_ctx;

static ok64 FREE_overlay_cb(u8 nat, u8cs tok, void *ctx) {
    FREE_overlay_ctx *o = (FREE_overlay_ctx *)ctx;
    // 'F' (issue key) is sticky — keeps its native tag through the
    // overlay, so issue keys pop visually even inside D-tagged
    // comments instead of vanishing into comment gray.
    u8 tag = (o->overlay && nat != 'F') ? o->overlay : nat;
    return o->cb ? o->cb(tag, tok, o->ctx) : OK;
}

ok64 FREEu8sFeed(u8 overlay, u8cs slice, TOKcb cb, void *ctx) {
    sane($ok(slice));
    FREE_overlay_ctx oc = {overlay, cb, ctx};
    FREEstate st = {
        .data = {slice[0], slice[1]},
        .cb = cb ? FREE_overlay_cb : NULL,
        .ctx = &oc,
    };
    return FREELexer(&st);
}
