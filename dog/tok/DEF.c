//
// DEF.c — Mark symbol definitions in tokenized files
//

#include "DEF.h"

#include "abc/NFA.h"
#include "abc/PRO.h"
#include "KEYW.h"

// ============================================================
//  Language keyword tables
// ============================================================
//
//  All defkw lists are `u8cs[]` so length is compile-time (via
//  `u8slit(sizeof(s)-1)`) — no per-token strlen loop on the hot
//  path.  Callers iterate with index + count.

#define DKW_NEL(arr) (sizeof(arr) / sizeof(arr[0]))

static u8cs C_DEF_KW[] = {
    u8slit("struct"), u8slit("union"), u8slit("enum"),
    u8slit("typedef"), u8slit("class"), u8slit("namespace"),
};
static u8cs GO_DEF_KW[] = {
    u8slit("func"), u8slit("type"), u8slit("var"), u8slit("const"),
};
static u8cs PY_DEF_KW[] = {
    u8slit("def"), u8slit("class"),
};
static u8cs RS_DEF_KW[] = {
    u8slit("fn"),   u8slit("struct"), u8slit("enum"),  u8slit("trait"),
    u8slit("type"), u8slit("mod"),    u8slit("const"), u8slit("static"),
};
static u8cs JS_DEF_KW[] = {
    u8slit("function"), u8slit("class"), u8slit("const"),
    u8slit("let"),      u8slit("var"),
};
static u8cs TS_DEF_KW[] = {
    u8slit("function"),  u8slit("class"), u8slit("interface"),
    u8slit("type"),      u8slit("enum"),  u8slit("const"),
    u8slit("let"),       u8slit("var"),
};
static u8cs JA_DEF_KW[] = {
    u8slit("class"), u8slit("interface"), u8slit("enum"), u8slit("record"),
};
static u8cs KT_DEF_KW[] = {
    u8slit("fun"), u8slit("class"), u8slit("interface"), u8slit("enum"),
    u8slit("object"), u8slit("val"), u8slit("var"),
};
static u8cs SW_DEF_KW[] = {
    u8slit("func"), u8slit("class"), u8slit("struct"), u8slit("enum"),
    u8slit("protocol"), u8slit("let"), u8slit("var"),
};
static u8cs DA_DEF_KW[] = {
    u8slit("class"), u8slit("enum"), u8slit("mixin"), u8slit("extension"),
};

// ============================================================
//  Definition patterns per language (table-driven)
// ============================================================
//
// Enriched alphabet:
//   s = identifier       f = defining keyword     t = typedef
//   r = other keyword    k = flow keyword         g = string
//   l = number literal   h = preprocessor
//   ( ) { } ; = , : . * < > = punctuation         p = other punct
//
// In patterns, uppercase S marks the identifier to retag as definition.
// After a successful NFA match, the name is located by S context:
//   - '}' before S in pattern → first 's' after outermost '}'
//   - lowercase letter before S → first 's' after that letter
//   - otherwise → last 's' before the first char/class after S
//
// All patterns end with .* (NFAu8Match requires full-string match).
// fresh=YES means the rule only fires at statement-start positions
// (after ; { } or stream start; cleared by = and flow keywords).

typedef struct {
    const char *regex;
    b8 fresh;
} DEFrule;

// C/C++: function defs, struct/enum/class names, typedefs, #defines
static const DEFrule C_RULES[] = {
    {"[rs*<>]+S[(][^(){};]*[)][{;].*",                         YES},
    {"[rs*<>]+S[(][^(){};]*[(][^)]*[)][^(){};]*[)][{;].*",     YES},
    {"fS[:{;].*",                                               NO},
    {"t[f]*[{].*[}][*,]*S[;].*",                               NO},
    {"t[frs*]+[(][*,rs]*S[)][^;]*[;].*",                       NO},
    {"t[frs*]+S[;].*",                                          NO},
    {"hS.*",                                                    NO},
    {NULL, NO},
};

// Java/C#: C-like + permissive class/record names (no struct-ptr issue)
static const DEFrule JA_RULES[] = {
    {"[rs*<>]+S[(][^(){};]*[)][{;].*",                         YES},
    {"[rs*<>]+S[(][^(){};]*[(][^)]*[)][^(){};]*[)][{;].*",     YES},
    {"fS[^;]*[{;].*",                                          NO},
    {NULL, NO},
};

// Go: func Name + method receivers func (r *T) Name
static const DEFrule GO_RULES[] = {
    {"fS.*",                NO},  // func Name, type Name, var/const name
    {"f[(][^)]*[)]S.*",     NO},  // func (r *Type) Method()
    {NULL, NO},
};

// Dart: C-like return-type functions + keyword-prefix classes
static const DEFrule DA_RULES[] = {
    {"[rs*]+S[(][^(){};]*[)][{;].*",     YES},  // void name() {}
    {"fS[^;]*[{;].*",                    NO},   // class Name ... {}
    {NULL, NO},
};

// LLVM IR: define/declare [attrs/types] @name(
static u8cs LL_DEF_KW[] = {
    u8slit("define"), u8slit("declare"),
};
static const DEFrule LL_RULES[] = {
    {"f[rsp]*S[(].*", NO},
    {NULL, NO},
};

// Keyword-prefix languages: func/def/type/class Name
static const DEFrule KW_RULES[] = {
    {"fS.*", NO},
    {NULL, NO},
};

// ============================================================
//  Enrichment: token array → compact u8 stream + index map
// ============================================================

static b8 DEFIsWs(u8csc val) {
    for (u8c const *p = val[0]; p < val[1]; p++)
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
            return NO;
    return YES;
}

//  Flow-control keywords shared by every C-family language; used by
//  `DEFEnrich` to tag the enriched stream byte as 'k' (blocks the
//  "fresh" statement-start flag so function-def rules don't fire
//  after `if (x) foo()` etc).  One shared KEYW instance, lazy-init.
static u8cs FLOW_KW[] = {
    u8slit("if"),     u8slit("else"),   u8slit("for"),    u8slit("while"),
    u8slit("do"),     u8slit("switch"), u8slit("case"),   u8slit("return"),
    u8slit("goto"),   u8slit("break"),  u8slit("continue"),
};

static keyw FLOW_KEYW;
static b8   FLOW_KEYW_INITED;

static b8 DEFIsFlowKw(u8csc val) {
    if (!FLOW_KEYW_INITED) {
        if (KEYWOpen(&FLOW_KEYW, FLOW_KW, DKW_NEL(FLOW_KW)) != OK) return NO;
        FLOW_KEYW_INITED = YES;
    }
    u8csc v = {val[0], val[1]};
    return KEYWHas(&FLOW_KEYW, v);
}

//  Defkw lists are tiny (2-8 entries) and already carry compile-time
//  lengths via `u8slit`.  Linear scan with length-first short-circuit
//  is cheaper than a 256-byte KEYW probe at this size.
static b8 DEFIsDefKw(u8csc val, u8cs const *kws, u32 n) {
    if (!kws) return NO;
    u64 len = u8csLen(val);
    for (u32 i = 0; i < n; i++) {
        if (u8csLen(kws[i]) != len) continue;
        if (__builtin_memcmp(val[0], kws[i][0], len) == 0) return YES;
    }
    return NO;
}

static b8 DEFIsTypedef(u8csc val) {
    return $len(val) == 7 && __builtin_memcmp(val[0], "typedef", 7) == 0;
}

static u8 DEFPunctByte(u8csc val) {
    if ($len(val) == 1) {
        u8 ch = *val[0];
        switch (ch) {
            case '(':
            case ')':
            case '{':
            case '}':
            case ';':
            case '=':
            case ',':
            case ':':
            case '.':
            case '*':
            case '<':
            case '>':
            case '[':
            case ']':
            case '&':
            case '?':
                return ch;
        }
    }
    return 'p';
}

typedef struct {
    u8 *enr;      // enriched byte stream (allocated)
    u32 *map;     // map[i] = token index for enriched byte i
    u32 len;      // length of enriched stream
    u32 cap;      // capacity
} DEFenr;

static ok64 DEFEnrich(DEFenr *e, u32 const *const *toks, u8csc data,
                       u8cs const *defkw, u32 ndefkw) {
    sane(e != NULL);
    u32 ntoks = (u32)$len(toks);
    u8c *base = data[0];
    for (u32 i = 0; i < ntoks; i++) {
        u32 tok = toks[0][i];
        u8 tag = tok32Tag(tok);
        u8cs val;
        tok32Val(val,toks,base,i);

        if (tag == 'D' || tag == 'W') continue;

        u8 byte;
        switch (tag) {
            case 'R':
                byte = DEFIsTypedef(val)                      ? 't'
                     : DEFIsDefKw(val, defkw, ndefkw)         ? 'f'
                     : DEFIsFlowKw(val)                       ? 'k'
                                                              : 'r';
                break;
            case 'S':
                byte = 's';
                break;
            case 'P':
                byte = DEFPunctByte(val);
                break;
            case 'G':
                byte = 'g';
                break;
            case 'L':
                byte = 'l';
                break;
            case 'H':
                byte = 'h';
                break;
            default:
                byte = '?';
                break;
        }

        if (e->len >= e->cap) return NFANOROOM;
        e->enr[e->len] = byte;
        e->map[e->len] = i;
        e->len++;
    }
    done;
}

// ============================================================
//  Re-tag a token S → N
// ============================================================

static void DEFRetag(u32 **toks, u32 idx, u8 tag) {
    u32 tok = toks[0][idx];
    if (tok32Tag(tok) == 'S')
        toks[0][idx] = tok32Pack(tag, tok32Offset(tok));
}

// ============================================================
//  Pattern matching: find definitions in enriched stream
// ============================================================

// Try NFA pattern anchored at position i. Uses prefix match: stops as
// soon as the pattern reaches MATCH state, no need for trailing .* .
// `ws` is caller-owned scratch (hoisted to blob scope — allocating
// 48 KB on every DEFTryMatch call added up fast).
static u32 DEFTryMatch(nfau8cs nfa, u8c *enr, u32 len, u32 at,
                        u32 *ws[2]) {
    u8cs text = {enr + at, enr + len};
    u16 n = NFAu8States(nfa);
    if (n == 0 || $len(ws) < 3 * (u64)n) return 0;
    return NFAu8MatchPrefix(nfa, text, ws) ? 1 : 0;
}

// Find last 's' at or before position `end` in enriched stream,
// scanning back no further than `start`.
static i64 DEFLastIdent(u8c *enr, u32 start, u32 end) {
    for (i64 j = (i64)end; j >= (i64)start; j--)
        if (enr[j] == 's') return j;
    return -1;
}

// Compile one NFA pattern, returns nstates (0 on failure)
static ok64 DEFCompile(nfau8g prog, u8csc pat, u32 *ws[2]) {
    u8cs mpat = {(u8c *)pat[0], (u8c *)pat[1]};
    return NFAu8Compile(prog, mpat, ws);
}

// --- Auto-derive name-finding rule from S position in pattern ---

static void DEFParseNameRule(const char *regex, u8 *anchor, b8 *after) {
    const char *s = regex;
    while (*s && *s != 'S') s++;
    if (*s != 'S') { *anchor = ';'; *after = NO; return; }

    // '}' before S → first 's' after '}' (typedef struct{} Name;)
    for (const char *p = regex; p < s; p++) {
        if (*p == '}') { *anchor = '}'; *after = YES; return; }
        if (*p == '[') {
            p++;
            while (*p && *p != ']') {
                if (*p == '}') { *anchor = '}'; *after = YES; return; }
                p++;
            }
        }
    }

    // concrete char before S → first 's' after that char
    char pre = (s > regex) ? *(s - 1) : 0;
    if (pre == ']') {
        // extract first char from [...] class
        const char *q = s - 2;
        while (q > regex && *q != '[') q--;
        if (*q == '[') {
            q++;
            if (*q == '^') q++;
            pre = *q;
        }
    }
    if (pre && pre != '.' && pre != '*' && pre != '+' && pre != '?'
        && pre != '[' && pre != '^') {
        *anchor = (u8)pre;
        *after = YES;
        return;
    }

    // char/class after S → last 's' before first such char
    const char *a = s + 1;
    if (*a == '[') {
        a++;
        *anchor = (*a && *a != ']') ? (u8)*a : ';';
    } else if (*a && *a != '.' && *a != '*' && *a != '+' && *a != '?') {
        *anchor = (u8)*a;
    } else {
        *anchor = ';';
    }
    *after = NO;
}

// --- Find the definition name after a successful NFA match ---

static i64 DEFFindName(u8c *enr, u32 start, u32 len, u8 anchor, b8 after) {
    if (after) {
        if (anchor == ')') {
            // depth-aware: first 's' after first ')' at paren depth 0
            i32 depth = 0;
            for (u32 j = start; j < len; j++) {
                if (enr[j] == '(') depth++;
                else if (enr[j] == ')') {
                    if (--depth <= 0) {
                        for (u32 k = j + 1; k < len; k++) {
                            if (enr[k] == 's') return (i64)k;
                            if (enr[k] == ';' || enr[k] == '{') break;
                        }
                        return -1;
                    }
                }
            }
            return -1;
        }
        if (anchor == '}') {
            // first 's' after LAST '}' (depth: nested structs)
            i64 last_anch = -1;
            for (u32 j = start; j < len; j++) {
                if (enr[j] == '}') last_anch = (i64)j;
                else if (last_anch >= 0 && (enr[j] == ';' || enr[j] == '{'))
                    break;
            }
            if (last_anch >= 0) {
                for (u32 j = (u32)(last_anch + 1); j < len; j++) {
                    if (enr[j] == 's') return (i64)j;
                    if (enr[j] == ';' || enr[j] == '{') break;
                }
            }
            return -1;
        }
        // first 's' after FIRST 'anchor'
        for (u32 j = start; j < len; j++) {
            if (enr[j] == anchor) {
                for (u32 k = j + 1; k < len; k++) {
                    if (enr[k] == 's') return (i64)k;
                    if (enr[k] == ';' || enr[k] == '{') break;
                }
                return -1;
            }
        }
        return -1;
    }
    // last 's' before first 'anchor'
    for (u32 j = start; j < len; j++) {
        if (enr[j] == anchor)
            return DEFLastIdent(enr, start, j > 0 ? j - 1 : 0);
    }
    return -1;
}

// --- Unified table-driven matcher ---

#define DEF_MAX_RULES       8
#define DEF_NFA_CAP         256
#define DEF_MAX_RULE_CACHES 8

//  Compiled NFA cache, keyed by the `DEFrule *` pointer (rule-set
//  identity, not per-language — several languages share KW_RULES
//  etc.).  Populated lazily by `DEFGetCache`; reused across every
//  blob that dispatches to the same ruleset.  Before this cache
//  existed, `DEFMarkRules` recompiled all rules for every blob —
//  ~140 k redundant NFA compilations during a full src/git clone.
typedef struct {
    const DEFrule *rules;                 // NULL → slot unused
    u32            nrules;
    b8             valid[DEF_MAX_RULES];
    b8             fresh[DEF_MAX_RULES];
    u8             anchor[DEF_MAX_RULES];
    b8             after[DEF_MAX_RULES];
    //  First-byte bitmap per rule: bit `c` set → rule might fire at a
    //  position where `e->enr[i] == c`.  Derived from the regex head
    //  ("[rs*<>]+..." → {r,s,*,<,>}; "fS[:{;]..." → {f}; etc.).  The
    //  per-position inner loop short-circuits on miss, cutting most
    //  of the ~3500 DEFTryMatch calls/blob that did nothing useful.
    u8             firstset[DEF_MAX_RULES][32];
    nfau8          nbuf[DEF_MAX_RULES][DEF_NFA_CAP];
    nfau8c        *nfa0[DEF_MAX_RULES];
    nfau8c        *nfa1[DEF_MAX_RULES];
} DEFRuleCache;

//  Derive the first-byte set for a rule regex, as a 256-bit bitmap
//  (32 bytes).  Handles the subset our rules use: single literal,
//  `[chars]` class (no ranges), `[^chars]` negation.  Unknown
//  forms fall back to "any byte" (all bits set), preserving
//  correctness at the cost of the fast-fail.
//
//  Special case: 'S' in the regex is the marker for the identifier
//  to retag and is compiled as 's' into the NFA (see the S→s
//  substitution in the compile loop), so the firstset for a rule
//  starting with 'S' must also admit 's'.
fun void DEFFSAdd(u8 out[32], u8 c) {
    if (c == 'S') c = 's';
    out[c >> 3] |= (u8)(1 << (c & 7));
}

static void DEFRuleFirstSet(const char *regex, u8 out[32]) {
    memset(out, 0, 32);
    if (!regex || !*regex) { memset(out, 0xff, 32); return; }

    const char *p = regex;
    if (*p == '[') {
        p++;
        b8 negate = NO;
        if (*p == '^') { negate = YES; p++; }
        u8 tmp[32] = {};
        while (*p && *p != ']') {
            DEFFSAdd(tmp, (u8)*p);
            p++;
        }
        if (negate) {
            for (u32 i = 0; i < 32; i++) out[i] = (u8)~tmp[i];
        } else {
            memcpy(out, tmp, 32);
        }
    } else {
        DEFFSAdd(out, (u8)*p);
    }
}

static DEFRuleCache DEF_RULE_CACHES[DEF_MAX_RULE_CACHES];
static u32          DEF_RULE_CACHE_N;

//  Lookup or populate the cache for `rules`.  Linear scan over the
//  ≤8 registered rule-sets is cheap and only runs once per blob.
static DEFRuleCache *DEFGetCache(const DEFrule *rules) {
    for (u32 i = 0; i < DEF_RULE_CACHE_N; i++)
        if (DEF_RULE_CACHES[i].rules == rules) return &DEF_RULE_CACHES[i];

    if (DEF_RULE_CACHE_N >= DEF_MAX_RULE_CACHES) return NULL;
    DEFRuleCache *c = &DEF_RULE_CACHES[DEF_RULE_CACHE_N];

    u32 nrules = 0;
    while (rules[nrules].regex && nrules < DEF_MAX_RULES) nrules++;
    c->nrules = nrules;

    u32 pb[4096];
    u32 *pws[2] = {pb, pb + 4096};

    for (u32 r = 0; r < nrules; r++) {
        c->fresh[r] = rules[r].fresh;
        c->valid[r] = NO;
        DEFParseNameRule(rules[r].regex, &c->anchor[r], &c->after[r]);
        DEFRuleFirstSet(rules[r].regex, c->firstset[r]);

        // S → s for NFA compilation; strip trailing .* (prefix match).
        u8 pat[256];
        u32 plen = 0;
        for (const char *p = rules[r].regex; *p && plen < sizeof(pat) - 1; p++)
            pat[plen++] = (*p == 'S') ? 's' : (u8)*p;
        if (plen >= 2 && pat[plen - 2] == '.' && pat[plen - 1] == '*')
            plen -= 2;

        nfau8g pr = {c->nbuf[r], c->nbuf[r] + DEF_NFA_CAP, c->nbuf[r]};
        ok64 o = DEFCompile(pr, (u8cs){pat, pat + plen}, pws);
        if (o == OK) {
            c->nfa0[r] = (nfau8c *)pr[2];
            c->nfa1[r] = (nfau8c *)pr[0];
            c->valid[r] = YES;
        }
    }

    //  Publish last so a concurrent reader either sees the fully
    //  populated cache or misses and allocates its own slot.  At
    //  current use (sequential ingest) races are moot; this is a
    //  cheap safety belt for future readers.
    c->rules = rules;
    DEF_RULE_CACHE_N++;
    return c;
}

static ok64 DEFMarkRules(u32 **toks, DEFenr *e, const DEFrule *rules, u8 tag) {
    sane(e != NULL && rules != NULL);

    DEFRuleCache *c = DEFGetCache(rules);
    if (!c) return OK;

    u32 nrules = c->nrules;

    //  Per-blob NFA simulation scratch.  NFAu8MatchPrefix needs
    //  3 × nstates u32 slots; 12 KB covers any rule we have.
    u32 wbuf[3 * 1024];
    u32 *ws[2] = {wbuf, wbuf + sizeof(wbuf) / sizeof(wbuf[0])};

    b8 fresh = YES;
    for (u32 i = 0; i < e->len; i++) {
        u8 ch = e->enr[i];
        if (ch == ';' || ch == '{' || ch == '}') { fresh = YES; continue; }
        if (ch == '=' || ch == 'k') { fresh = NO; continue; }

        //  First-byte fast-fail: skip rules whose bitmap says this
        //  position can't start a match.  For non-letter bytes (most
        //  of the stream) all rules are skipped without any NFA work.
        for (u32 r = 0; r < nrules; r++) {
            if (!c->valid[r] || (c->fresh[r] && !fresh)) continue;
            if (!(c->firstset[r][ch >> 3] & (1u << (ch & 7)))) continue;

            nfau8cs nfa = {c->nfa0[r], c->nfa1[r]};
            if (!DEFTryMatch(nfa, e->enr, e->len, i, ws)) continue;

            i64 name = DEFFindName(e->enr, i, e->len, c->anchor[r], c->after[r]);
            if (name >= 0) DEFRetag(toks, e->map[name], tag);
            break;
        }
    }
    done;
}

// Function call: any identifier followed by (
static const DEFrule CALL_RULES[] = {
    {"S[(].*", NO},
    {NULL, NO},
};

// ============================================================
//  Language dispatch
// ============================================================

typedef struct {
    u8cs           ext;     // extension slice; compile-time length via u8slit
    u8cs const    *defkw;   // defkw array (borrowed), or NULL
    u32            ndefkw;  // entries in defkw
    const DEFrule *rules;
    b8 calls;               // mark s( call sites as CALL_TAG
} DEFlang;

//  Extension dispatch table.  Length is carried in the `u8cs ext` so
//  per-blob lookup does no strlen — just u8csLen subtraction then
//  memcmp.  Terminator row has a zero-length ext.
#define L(e, kw, rules, calls) {u8slit(e), kw, DKW_NEL(kw), rules, calls}
#define L0(e, rules, calls)    {u8slit(e), NULL, 0, rules, calls}

static const DEFlang DEF_LANGS[] = {
    L ("c",      C_DEF_KW,  C_RULES,  YES),
    L ("h",      C_DEF_KW,  C_RULES,  YES),
    L ("cc",     C_DEF_KW,  C_RULES,  YES),
    L ("cpp",    C_DEF_KW,  C_RULES,  YES),
    L ("cxx",    C_DEF_KW,  C_RULES,  YES),
    L ("hpp",    C_DEF_KW,  C_RULES,  YES),
    L ("hh",     C_DEF_KW,  C_RULES,  YES),
    L ("hxx",    C_DEF_KW,  C_RULES,  YES),
    L ("go",     GO_DEF_KW, GO_RULES, YES),
    L ("py",     PY_DEF_KW, KW_RULES, YES),
    L ("rs",     RS_DEF_KW, KW_RULES, YES),
    L ("js",     JS_DEF_KW, KW_RULES, YES),
    L ("jsx",    JS_DEF_KW, KW_RULES, YES),
    L ("ts",     TS_DEF_KW, KW_RULES, YES),
    L ("tsx",    TS_DEF_KW, KW_RULES, YES),
    L ("java",   JA_DEF_KW, JA_RULES, YES),
    L ("cs",     JA_DEF_KW, JA_RULES, YES),
    L ("kt",     KT_DEF_KW, KW_RULES, YES),
    L ("swift",  SW_DEF_KW, KW_RULES, YES),
    L ("dart",   DA_DEF_KW, DA_RULES, YES),
    L ("zig",    RS_DEF_KW, KW_RULES, YES),
    L0("rb",               NULL,     YES),
    L0("lua",              NULL,     YES),
    L0("pl",               NULL,     YES),
    L0("pm",               NULL,     YES),
    L0("php",              NULL,     YES),
    L0("r",                NULL,     YES),
    L0("R",                NULL,     YES),
    L0("jl",               NULL,     YES),
    L0("ex",               NULL,     YES),
    L0("exs",              NULL,     YES),
    L0("nim",              NULL,     YES),
    L0("nims",             NULL,     YES),
    L0("scala",            NULL,     YES),
    L0("sc",               NULL,     YES),
    L0("d",                NULL,     YES),
    L0("clj",              NULL,     YES),
    L0("cljs",             NULL,     YES),
    L0("gleam",            NULL,     YES),
    L0("odin",             NULL,     YES),
    L0("sol",              NULL,     YES),
    L0("graphql",          NULL,     YES),
    L0("gql",              NULL,     YES),
    L ("ll",     LL_DEF_KW, LL_RULES, YES),
    L0("proto",            NULL,     NO),
    L0("sql",              NULL,     NO),
};
#define NDEF_LANGS (sizeof(DEF_LANGS) / sizeof(DEF_LANGS[0]))

#undef L
#undef L0

static const DEFlang *DEFLookup(u8csc ext) {
    u64 elen = u8csLen(ext);
    for (u32 i = 0; i < NDEF_LANGS; i++) {
        const DEFlang *l = &DEF_LANGS[i];
        if (u8csLen(l->ext) != elen) continue;
        if (__builtin_memcmp(ext[0], l->ext[0], elen) == 0) return l;
    }
    return NULL;
}

ok64 DEFMark(u32 *toks[2], u8csc data, u8csc ext) {
    sane($ok(toks) && $ok(data));
    const DEFlang *lang = DEFLookup(ext);
    if (!lang) return OK;

    u32 ntoks = (u32)$len(toks);
    u32 cap = ntoks;

    u8 enr_stack[8192];
    u32 map_stack[8192];
    u8 *enr_buf = (cap <= 8192) ? enr_stack : (u8 *)malloc(cap);
    u32 *map_buf = (cap <= 8192) ? map_stack : (u32 *)malloc(cap * sizeof(u32));
    if (!enr_buf || !map_buf) return NFANOROOM;

    DEFenr e = {.enr = enr_buf, .map = map_buf, .len = 0, .cap = cap};

    u32 const *ctoks[2] = {toks[0], toks[1]};
    ok64 o = DEFEnrich(&e, ctoks, data, lang->defkw, lang->ndefkw);
    if (o != OK) goto cleanup;

    if (lang->rules) {
        o = DEFMarkRules(toks, &e, lang->rules, DEF_TAG);
        if (o != OK) goto cleanup;
    }
    if (lang->calls) {
        o = DEFMarkRules(toks, &e, CALL_RULES, CALL_TAG);
    }

cleanup:
    if (enr_buf != enr_stack) free(enr_buf);
    if (map_buf != map_stack) free(map_buf);
    return o;
}
