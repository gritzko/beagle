#include "SQLT.h"

#include "abc/PRO.h"

static const char *SQLT_KEYWORDS[] = {
    "select", "from", "where", "and", "or", "not", "in",
    "insert", "into", "values", "update", "set", "delete",
    "create", "table", "alter", "drop", "index",
    "join", "inner", "left", "right", "outer", "full", "on",
    "as", "asc", "desc", "order", "by", "group", "having",
    "limit", "offset", "union", "all", "distinct",
    "null", "is", "between", "like", "exists",
    "case", "when", "then", "else", "end",
    "begin", "commit", "rollback",
    "primary", "key", "foreign", "references", "unique",
    "default", "check", "constraint",
    "int", "integer", "varchar", "char", "text",
    "float", "double", "decimal", "date", "timestamp",
    "boolean", "true", "false",
    NULL,
};

static u8 SQLTlower(u8 c) {
    return (c >= 'A' && c <= 'Z') ? (u8)(c + 32) : c;
}

static b8 SQLTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = SQLT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        b8 match = YES;
        for (u64 i = 0; i < len; ++i) {
            if (SQLTlower(tok[0][i]) != (u8)k[i]) { match = NO; break; }
        }
        if (match) return YES;
    }
    return NO;
}

ok64 SQLTonComment(u8cs tok, SQLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 SQLTonString(u8cs tok, SQLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 SQLTonNumber(u8cs tok, SQLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 SQLTonWord(u8cs tok, SQLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = SQLTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 SQLTonPunct(u8cs tok, SQLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 SQLTonSpace(u8cs tok, SQLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
