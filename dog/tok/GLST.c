#include "GLST.h"

#include "abc/PRO.h"

static const char *GLST_KEYWORDS[] = {
    "attribute","const",    "uniform",  "varying",  "buffer",
    "shared",   "coherent", "volatile", "restrict", "readonly",
    "writeonly","layout",   "centroid", "flat",     "smooth",
    "noperspective","patch","sample",   "break",    "continue",
    "do",       "for",      "while",    "switch",   "case",
    "default",  "if",       "else",     "subroutine",
    "in",       "out",      "inout",
    "float",    "double",   "int",      "void",     "bool",
    "true",     "false",    "invariant","precise",  "discard",
    "return",
    "mat2",     "mat3",     "mat4",
    "vec2",     "vec3",     "vec4",
    "ivec2",    "ivec3",    "ivec4",
    "bvec2",    "bvec3",    "bvec4",
    "uvec2",    "uvec3",    "uvec4",
    "sampler2D","sampler3D","samplerCube",
    "struct",
    NULL,
};

static b8 GLSTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = GLST_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 GLSTonComment(u8cs tok, GLSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 GLSTonNumber(u8cs tok, GLSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 GLSTonPreproc(u8cs tok, GLSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 GLSTonWord(u8cs tok, GLSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = GLSTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 GLSTonPunct(u8cs tok, GLSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 GLSTonSpace(u8cs tok, GLSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
