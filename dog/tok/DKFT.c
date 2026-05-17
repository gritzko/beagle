#include "DKFT.h"

#include "abc/PRO.h"

static const char *DKFT_KEYWORDS[] = {
    "FROM",     "RUN",      "CMD",      "EXPOSE",   "ENV",
    "ADD",      "COPY",     "ENTRYPOINT", "VOLUME", "USER",
    "WORKDIR",  "ARG",      "ONBUILD",  "STOPSIGNAL",
    "HEALTHCHECK", "SHELL", "LABEL",    "MAINTAINER",
    NULL,
};

static b8 DKFTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = DKFT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 DKFTonComment(u8cs tok, DKFTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 DKFTonString(u8cs tok, DKFTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 DKFTonNumber(u8cs tok, DKFTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 DKFTonVar(u8cs tok, DKFTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('S', tok, state->ctx);
    done;
}

ok64 DKFTonWord(u8cs tok, DKFTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = DKFTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 DKFTonPunct(u8cs tok, DKFTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 DKFTonSpace(u8cs tok, DKFTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
