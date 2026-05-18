#include "CT.h"
#include "GOT.h"
#include "PYT.h"
#include "JST.h"
#include "RST.h"
#include "JAT.h"
#include "CPPT.h"
#include "CST.h"
#include "HTMT.h"
#include "CSST.h"
#include "JSONT.h"
#include "SHT.h"
#include "RBT.h"
#include "HST.h"
#include "MLT.h"
#include "JLT.h"
#include "PHPT.h"
#include "AGDT.h"
#include "VERT.h"
#include "TST.h"
#include "KTT.h"
#include "SCLT.h"
#include "SWFT.h"
#include "DARTT.h"
#include "ZIGT.h"
#include "DT.h"
#include "LUAT.h"
#include "PRLT.h"
#include "RT.h"
#include "ELXT.h"
#include "ERLT.h"
#include "NIMT.h"
#include "NIXT.h"
#include "VIMT.h"
#include "YMLT.h"
#include "TOMLT.h"
#include "SQLT.h"
#include "GQLT.h"
#include "PRTT.h"
#include "HCLT.h"
#include "SCSST.h"
#include "LAXT.h"
#include "CLJT.h"
#include "CMKT.h"
#include "DKFT.h"
#include "FORT.h"
#include "FSHT.h"
#include "GLMT.h"
#include "GLST.h"
#include "MAKT.h"
#include "ODNT.h"
#include "PWST.h"
#include "SOLT.h"
#include "TYST.h"
#include "LLT.h"
#include "MDT.h"
#include "TOK.h"
#include "git/CFG.h"

#include "abc/PRO.h"
#include "abc/TEST.h"

typedef struct {
    u8 tags[256];
    int count;
} TOK01ctx;

static ok64 TOK01cb(u8 tag, u8cs tok, void *ctx) {
    TOK01ctx *c = (TOK01ctx *)ctx;
    if (c->count < 256) {
        c->tags[c->count] = tag;
    }
    ++c->count;
    return OK;
}

typedef struct {
    const char *input;
    const char *tags;
} TOK01Case;

// Helper macro: run table-driven test for a given lexer
#define RUN_CASES(LEXER, TYPE, cases, ncases) do { \
    for (int i = 0; i < ncases; ++i) { \
        TOK01ctx ctx = {}; \
        TYPE##state st = { \
            .data = {(u8c *)cases[i].input, \
                     (u8c *)cases[i].input + strlen(cases[i].input)}, \
            .cb = TOK01cb, \
            .ctx = &ctx, \
        }; \
        ok64 o = LEXER(&st); \
        if (o != OK) { \
            fprintf(stderr, "FAIL case %d: '%s' error %s\n", i, \
                    cases[i].input, ok64str(o)); \
            fail(TESTFAIL); \
        } \
        int elen = strlen(cases[i].tags); \
        if (ctx.count != elen) { \
            fprintf(stderr, \
                    "FAIL case %d: '%s' count %d != expected %d\n", \
                    i, cases[i].input, ctx.count, elen); \
            fprintf(stderr, "  got tags: "); \
            for (int j = 0; j < ctx.count && j < 256; ++j) \
                fputc(ctx.tags[j], stderr); \
            fputc('\n', stderr); \
            fail(TESTFAIL); \
        } \
        for (int j = 0; j < elen; ++j) { \
            if (ctx.tags[j] != (u8)cases[i].tags[j]) { \
                fprintf(stderr, \
                        "FAIL case %d: '%s' tag[%d]='%c' expected '%c'\n", \
                        i, cases[i].input, j, ctx.tags[j], \
                        cases[i].tags[j]); \
                fprintf(stderr, "  got tags: "); \
                for (int k = 0; k < ctx.count && k < 256; ++k) \
                    fputc(ctx.tags[k], stderr); \
                fputc('\n', stderr); \
                fail(TESTFAIL); \
            } \
        } \
    } \
} while(0)

ok64 CTBasicTest() {
    sane(1);

    TOK01Case cases[] = {
        {"int x;", "RWSP"},
        {"if (x)", "RWPSP"},
        {"return 0;", "RWLP"},
        {"// comment\n", "DDDDW"},
        {"/* block */", "DDDDDDD"},
        {"\"hello\"", "G"},
        {"'c'", "G"},
        {"42", "L"},
        {"3.14", "L"},
        {"0xff", "L"},
        {"0b101", "L"},
        {"#include <stdio.h>", "HWPSPSP"},
        {"int main(void) { return 0; }", "RWSPRPWPWRWLPWP"},
        {"a + b", "SWPWS"},
        {"  \t\n", "W"},
        {"{}", "PP"},
        {"x=1;", "SPLP"},
        {"while (i < n) {", "RWPSWPWSPWP"},
        {"struct foo {", "RWSWP"},
        {"#define X 1\n", "HWSWLW"},
        {"1e10", "L"},
        {"1.0f", "L"},
        {"100UL", "L"},
        {"0644", "L"},
        {"0x1p10", "L"},
        {".5f", "L"},
        {"1'000'000", "L"},
        {"0xDEAD'BEEF", "L"},
        {"1ULL", "L"},
        {"L\"wide\"", "G"},
        {"u8\"utf8\"", "G"},
        {"U'x'", "G"},
        {"'\\n'", "G"},
        {"'\\xff'", "G"},
        {"\"esc\\t\\n\"", "G"},
        {"#define FOO \\\nbar", "HWSWPWS"},
        {"// end", "DDDD"},
        {"a->b", "SPS"},
        {"a++", "SP"},
        {"x <<= 1", "SWPWL"},
        {"...", "P"},
        {"a != b", "SWPWS"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(CTLexer, CT, cases, ncases);
    done;
}

ok64 CTKeywordTest() {
    sane(1);

    TOK01Case cases[] = {
        {"auto", "R"},
        {"break", "R"},
        {"const", "R"},
        {"continue", "R"},
        {"default", "R"},
        {"do", "R"},
        {"double", "R"},
        {"else", "R"},
        {"enum", "R"},
        {"extern", "R"},
        {"float", "R"},
        {"for", "R"},
        {"goto", "R"},
        {"if", "R"},
        {"inline", "R"},
        {"int", "R"},
        {"long", "R"},
        {"register", "R"},
        {"return", "R"},
        {"short", "R"},
        {"sizeof", "R"},
        {"static", "R"},
        {"struct", "R"},
        {"switch", "R"},
        {"typedef", "R"},
        {"union", "R"},
        {"unsigned", "R"},
        {"void", "R"},
        {"volatile", "R"},
        {"while", "R"},
        {"bool", "R"},
        {"true", "R"},
        {"false", "R"},
        {"nullptr", "R"},
        {"myvar", "S"},
        {"printf", "S"},
        {"foo_bar", "S"},
        {"_private", "S"},
        {"x1", "S"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(CTLexer, CT, cases, ncases);
    done;
}

ok64 TOKDispatchTest() {
    sane(1);

    const char *input = "int x;";
    TOK01ctx ctx = {};
    TOKstate state = {
        .data = u8scstr(input),
        .cb = TOK01cb,
        .ctx = &ctx,
    };
    u8csc ext = {(u8c *)"c", (u8c *)"c" + 1};
    call(TOKLexer, &state, ext);
    want(ctx.count == 4);
    want(ctx.tags[0] == 'R');  // int
    want(ctx.tags[1] == 'W');  // space
    want(ctx.tags[2] == 'S');  // x
    want(ctx.tags[3] == 'P');  // ;
    done;
}

ok64 GOTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"func main() {", "RWSPPWP"},
        {"var x int", "RWSWS"},
        {"// comment\n", "DDDDW"},
        {"/* block */", "DDDDDDD"},
        {"\"hello\"", "G"},
        {"42", "L"},
        {"0xFF", "L"},
        {"0b101", "L"},
        {"0o77", "L"},
        {"3.14", "L"},
        {"1e10", "L"},
        {":=", "P"},
        {"<-", "P"},
        {"if else", "RWR"},
        {"package main", "RWS"},
        {"x := 5", "SWPWL"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(GOTLexer, GOT, cases, ncases);
    done;
}

ok64 PYTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"def foo():", "RWSPPP"},
        {"# comment\n", "DDDW"},
        {"\"hello\"", "G"},
        {"'hello'", "G"},
        {"42", "L"},
        {"0xFF", "L"},
        {"3.14", "L"},
        {"if else", "RWR"},
        {"import os", "RWS"},
        {"True False None", "RWRWR"},
        {"x = 5", "SWPWL"},
        {"@decorator", "H"},
        {"**=", "P"},
        {"//", "P"},
        {"r'[a-z\\-]+$'", "G"},
        {"r'\\(\\)'", "G"},
        {"x'unterminated\nfoo", "SPSWS"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(PYTLexer, PYT, cases, ncases);
    done;
}

ok64 JSTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"function foo() {", "RWSPPWP"},
        {"// comment\n", "DDDDW"},
        {"/* block */", "DDDDDDD"},
        {"\"hello\"", "G"},
        {"'hello'", "G"},
        {"42", "L"},
        {"0xFF", "L"},
        {"3.14", "L"},
        {"const let var", "RWRWR"},
        {"===", "P"},
        {"!==", "P"},
        {"=>", "P"},
        {"x = 5", "SWPWL"},
        // regex literals
        {"/pattern/g", "G"},
        {"/\\n/g", "G"},
        {"/.*\\//g", "G"},
        {"/foo/gimsuy", "G"},
        {"x.replace(/\"/g, '\"')", "SPSPGPWGP"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(JSTLexer, JST, cases, ncases);
    done;
}

ok64 JSTFileTest() {
    sane(1);
    // js2c.js repro: regex literals that previously caused JSTBAD
    const char *input =
        "#!/usr/bin/node\n"
        "\n"
        "const fs = require(\"fs\");\n"
        "const console = require(\"console\");\n"
        "const process = require(\"process\");\n"
        "\n"
        "var file = process.argv[2];\n"
        "\n"
        "str = fs.readFileSync(file, {encoding:\"utf8\"})\n"
        "\n"
        "str = str.replace(/\"/g, '\\\\\"')\n"
        "        .replace(/\\n/g, '\\\\n\"\\n\"');\n"
        "\n"
        "var varn = file.replace(/.*\\//g, \"\").replaceAll('.', '_');\n"
        "\n"
        "var code = \"const char* \" + varn + \" = \\\"\" + str + \"\\\";\";\n"
        "\n"
        "console.log(code);\n";
    TOK01ctx ctx = {};
    JSTstate st = {
        .data = u8scstr(input),
        .cb = TOK01cb,
        .ctx = &ctx,
    };
    ok64 o = JSTLexer(&st);
    if (o != OK) {
        fprintf(stderr, "FAIL JSTFileTest: js2c.js error %s\n", ok64str(o));
        fail(TESTFAIL);
    }
    want(ctx.count > 0);
    done;
}

ok64 RSTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"fn main() {", "RWSPPWP"},
        {"// comment\n", "DDDDW"},
        {"/* block */", "DDDDDDD"},
        {"\"hello\"", "G"},
        {"42", "L"},
        {"0xFF", "L"},
        {"3.14", "L"},
        {"let mut", "RWR"},
        {"pub fn", "RWR"},
        {"::=", "PP"},
        {"x = 5", "SWPWL"},
        {"#[derive(Debug)]", "HPSPP"},
        {"#![allow(unused)]", "HPSPP"},
        {"#", "P"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(RSTLexer, RST, cases, ncases);
    done;
}

ok64 JATBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"public class Foo {", "RWRWSWP"},
        {"// comment\n", "DDDDW"},
        {"/* block */", "DDDDDDD"},
        {"\"hello\"", "G"},
        {"'c'", "G"},
        {"42", "L"},
        {"3.14", "L"},
        {"int long", "RWR"},
        {"@Override", "H"},
        {"x = 5;", "SWPWLP"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(JATLexer, JAT, cases, ncases);
    done;
}

ok64 JSONTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"42", "L"},
        {"3.14", "L"},
        {"-1", "L"},
        {"\"hello\"", "G"},
        {"true", "R"},
        {"false", "R"},
        {"null", "R"},
        {"{}", "PP"},
        {"[]", "PP"},
        {":", "P"},
        {",", "P"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(JSONTLexer, JSONT, cases, ncases);
    done;
}

ok64 SHTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"# comment\n", "DDDW"},
        {"\"hello\"", "G"},
        {"'hello'", "G"},
        {"42", "L"},
        {"if then fi", "RWRWR"},
        {"$HOME", "S"},
        {"||", "P"},
        {"&&", "P"},
        {"$(foo)", "PSP"},
        {"${VAR}", "PSP"},
        {"$(expr 'a)b')", "PSWGP"},
        {"foo'\nbar", "SPWS"},
        {"x \" y", "SWPWS"},
        {"'foo'\\''bar'", "G"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(SHTLexer, SHT, cases, ncases);
    done;
}

ok64 HSTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"-- comment\n", "DDDDW"},
        {"\"hello\"", "G"},
        {"42", "L"},
        {"3.14", "L"},
        {"module where", "RWR"},
        {"let in", "RWR"},
        {"::", "P"},
        {"->", "P"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(HSTLexer, HST, cases, ncases);
    done;
}

ok64 MLTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"(* comment *)", "DDDDDDD"},
        {"\"hello\"", "G"},
        {"42", "L"},
        {"3.14", "L"},
        {"let in", "RWR"},
        {"match with", "RWR"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(MLTLexer, MLT, cases, ncases);
    done;
}

ok64 JLTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"# comment\n", "DDDW"},
        {"\"hello\"", "G"},
        {"42", "L"},
        {"3.14", "L"},
        {"function end", "RWR"},
        {"if else", "RWR"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(JLTLexer, JLT, cases, ncases);
    done;
}

ok64 TSTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"// comment\n", "DDDDW"},
        {"/* block */", "DDDDDDD"},
        {"\"hello\"", "G"},
        {"'hello'", "G"},
        {"42", "L"},
        {"0xFF", "L"},
        {"const let", "RWR"},
        {"=>", "P"},
        {"x = 5", "SWPWL"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(TSTLexer, TST, cases, ncases);
    done;
}

ok64 KTTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"// comment\n", "DDDDW"},
        {"/* block */", "DDDDDDD"},
        {"\"hello\"", "G"},
        {"42", "L"},
        {"fun val var", "RWRWR"},
        {"class when", "RWR"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(KTTLexer, KTT, cases, ncases);
    done;
}

ok64 SCLTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"// comment\n", "DDDDW"},
        {"/* block */", "DDDDDDD"},
        {"\"hello\"", "G"},
        {"42", "L"},
        {"def val var", "RWRWR"},
        {"class object", "RWR"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(SCLTLexer, SCLT, cases, ncases);
    done;
}

ok64 SWFTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"// comment\n", "DDDDW"},
        {"/* block */", "DDDDDDD"},
        {"\"hello\"", "G"},
        {"42", "L"},
        {"func let var", "RWRWR"},
        {"class struct", "RWR"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(SWFTLexer, SWFT, cases, ncases);
    done;
}

ok64 DARTTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"// comment\n", "DDDDW"},
        {"/* block */", "DDDDDDD"},
        {"\"hello\"", "G"},
        {"'hello'", "G"},
        {"42", "L"},
        {"class var", "RWR"},
        {"if else", "RWR"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(DARTTLexer, DARTT, cases, ncases);
    done;
}

ok64 ZIGTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"// comment\n", "DDDDW"},
        {"\"hello\"", "G"},
        {"42", "L"},
        {"const fn pub", "RWRWR"},
        {"if else", "RWR"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(ZIGTLexer, ZIGT, cases, ncases);
    done;
}

ok64 DTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"// comment\n", "DDDDW"},
        {"/* block */", "DDDDDDD"},
        {"\"hello\"", "G"},
        {"42", "L"},
        {"class void", "RWR"},
        {"if else", "RWR"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(DTLexer, DT, cases, ncases);
    done;
}

ok64 LUATBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"-- comment\n", "DDDDW"},
        {"\"hello\"", "G"},
        {"'hello'", "G"},
        {"42", "L"},
        {"3.14", "L"},
        {"function end", "RWR"},
        {"local if then", "RWRWR"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(LUATLexer, LUAT, cases, ncases);
    done;
}

ok64 PRLTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"# comment\n", "DDDW"},
        {"\"hello\"", "G"},
        {"'hello'", "G"},
        {"42", "L"},
        {"my sub", "RWR"},
        {"if else", "RWR"},
        {"$x =~ /\\@(.*)>$/", "SWPWPPPPPPPPS"},
        {"$", "P"},
        {"@", "P"},
        {"%", "P"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(PRLTLexer, PRLT, cases, ncases);
    done;
}

ok64 RTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"# comment\n", "DDDW"},
        {"\"hello\"", "G"},
        {"'hello'", "G"},
        {"42", "L"},
        {"3.14", "L"},
        {"function if else", "RWRWR"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(RTLexer, RT, cases, ncases);
    done;
}

ok64 ELXTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"# comment\n", "DDDW"},
        {"\"hello\"", "G"},
        {"'hello'", "G"},
        {"42", "L"},
        {"def do end", "RWRWR"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(ELXTLexer, ELXT, cases, ncases);
    done;
}

ok64 ERLTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"% comment\n", "DDDW"},
        {"\"hello\"", "G"},
        {"42", "L"},
        {"3.14", "L"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(ERLTLexer, ERLT, cases, ncases);
    done;
}

ok64 NIMTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"# comment\n", "DDDW"},
        {"\"hello\"", "G"},
        {"42", "L"},
        {"proc var let", "RWRWR"},
        {"if else", "RWR"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(NIMTLexer, NIMT, cases, ncases);
    done;
}

ok64 NIXTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"# comment\n", "DDDW"},
        {"\"hello\"", "G"},
        {"42", "L"},
        {"let in", "RWR"},
        {"if then else", "RWRWR"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(NIXTLexer, NIXT, cases, ncases);
    done;
}

ok64 YMLTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"# comment\n", "DDDW"},
        {"\"hello\"", "G"},
        {"'hello'", "G"},
        {"42", "L"},
        {"true", "R"},
        {"false", "R"},
        {"null", "R"},
        {":", "P"},
        {"-", "P"},
        {"a != b", "SWPPWS"},
        {"*windows*)", "PPP"},
        {"!", "P"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(YMLTLexer, YMLT, cases, ncases);
    done;
}

ok64 TOMLTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"# comment\n", "DDDW"},
        {"\"hello\"", "G"},
        {"'hello'", "G"},
        {"42", "L"},
        {"3.14", "L"},
        {"true", "R"},
        {"false", "R"},
        {"=", "P"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(TOMLTLexer, TOMLT, cases, ncases);
    done;
}

ok64 CFGDrainTest() {
    sane(1);
    static const char blob[] =
        "[submodule \"abc\"]\n"
        "\tpath = lib/abc\n"
        "\turl = https://github.com/gritzko/libabc.git\n"
        "\n"
        "; an ini-style comment line\n"
        "[submodule \"ragel-runtime\"]\n"
        "\tpath = vendor/ragel\n"
        "\turl = git@github.com:colmnet/ragel.git\n"
        "[core]\n"
        "\teditor = vim ; trailing comment\n";

    struct expect {
        const char *section;
        const char *key;
        const char *val;
    } want[] = {
        {"submodule/abc",           "path",   "lib/abc"},
        {"submodule/abc",           "url",    "https://github.com/gritzko/libabc.git"},
        {"submodule/ragel-runtime", "path",   "vendor/ragel"},
        {"submodule/ragel-runtime", "url",    "git@github.com:colmnet/ragel.git"},
        {"core",                    "editor", "vim"},
    };
    int n_want = sizeof(want) / sizeof(want[0]);

    a_pad(u8, section, 256);
    a_pad(u8, cfgbuf, 1024);
    u8bReset(section);

    u8cs ini = {(u8c *)blob, (u8c *)blob + sizeof(blob) - 1};
    int i = 0;
    for (;;) {
        u8cs k = {}, v = {};
        ok64 o = CFGu8sDrain(ini, cfgbuf, section, k, v);
        if (o == NODATA) break;
        if (o != OK) {
            fprintf(stderr, "CFGDrain failed at #%d: %s\n", i, ok64str(o));
            fail(TESTFAIL);
        }
        if (i >= n_want) {
            fprintf(stderr, "CFGDrain produced too many pairs\n");
            fail(TESTFAIL);
        }
        a_dup(u8c, sec, u8bDataC(section));
        size_t slen = strlen(want[i].section);
        size_t klen = strlen(want[i].key);
        size_t vlen = strlen(want[i].val);
        if ($len(sec) != slen ||
            memcmp(sec[0], want[i].section, slen) != 0) {
            fprintf(stderr,
                    "CFGDrain #%d section: got '%.*s' want '%s'\n",
                    i, (int)$len(sec), (char *)sec[0], want[i].section);
            fail(TESTFAIL);
        }
        if ($len(k) != klen ||
            memcmp(k[0], want[i].key, klen) != 0) {
            fprintf(stderr,
                    "CFGDrain #%d key: got '%.*s' want '%s'\n",
                    i, (int)$len(k), (char *)k[0], want[i].key);
            fail(TESTFAIL);
        }
        if ($len(v) != vlen ||
            memcmp(v[0], want[i].val, vlen) != 0) {
            fprintf(stderr,
                    "CFGDrain #%d val: got '%.*s' want '%s'\n",
                    i, (int)$len(v), (char *)v[0], want[i].val);
            fail(TESTFAIL);
        }
        ++i;
    }
    if (i != n_want) {
        fprintf(stderr, "CFGDrain produced %d pairs, want %d\n", i, n_want);
        fail(TESTFAIL);
    }
    done;
}

ok64 SQLTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"-- comment\n", "DDDDW"},
        {"'hello'", "G"},
        {"42", "L"},
        {"SELECT FROM WHERE", "RWRWR"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(SQLTLexer, SQLT, cases, ncases);
    done;
}

ok64 GQLTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"# comment\n", "DDDW"},
        {"\"hello\"", "G"},
        {"42", "L"},
        {"{}", "PP"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(GQLTLexer, GQLT, cases, ncases);
    done;
}

ok64 PRTTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"// comment\n", "DDDDW"},
        {"/* block */", "DDDDDDD"},
        {"\"hello\"", "G"},
        {"42", "L"},
        {"message enum", "RWR"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(PRTTLexer, PRTT, cases, ncases);
    done;
}

ok64 HCLTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"# comment\n", "DDDW"},
        {"// comment\n", "DDDDW"},
        {"/* block */", "DDDDDDD"},
        {"\"hello\"", "G"},
        {"42", "L"},
        {"{}", "PP"},
        {"=", "P"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(HCLTLexer, HCLT, cases, ncases);
    done;
}

ok64 SCSSTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"// comment\n", "DDDDW"},
        {"/* block */", "DDDDDDD"},
        {"\"hello\"", "G"},
        {"'hello'", "G"},
        {"42", "L"},
        {"{}", "PP"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(SCSSTLexer, SCSST, cases, ncases);
    done;
}

ok64 CSSTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"/* block */", "DDDDDDD"},
        {"\"hello\"", "G"},
        {"42", "L"},
        {"#fff", "L"},
        {"#abcdef", "L"},
        {"div#id", "SPS"},
        {".cls", "PS"},
        {"@media", "H"},
        {"{}", "PP"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(CSSTLexer, CSST, cases, ncases);
    done;
}

ok64 LAXTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"% comment\n", "DDDW"},
        {"{}", "PP"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(LAXTLexer, LAXT, cases, ncases);
    done;
}

ok64 CLJTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"; comment\n", "DDDW"},
        {"\"hello\"", "G"},
        {"42", "L"},
        {"3.14", "L"},
        {"()", "PP"},
        {"[]", "PP"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(CLJTLexer, CLJT, cases, ncases);
    done;
}

ok64 FORTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"! comment\n", "DDDW"},
        {"\"hello\"", "G"},
        {"'hello'", "G"},
        {"42", "L"},
        {"3.14", "L"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(FORTLexer, FORT, cases, ncases);
    done;
}

ok64 GLSTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"// comment\n", "DDDDW"},
        {"/* block */", "DDDDDDD"},
        {"42", "L"},
        {"3.14", "L"},
        {"{}", "PP"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(GLSTLexer, GLST, cases, ncases);
    done;
}

ok64 SOLTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        {"// comment\n", "DDDDW"},
        {"/* block */", "DDDDDDD"},
        {"\"hello\"", "G"},
        {"42", "L"},
        {"contract function", "RWR"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(SOLTLexer, SOLT, cases, ncases);
    done;
}

ok64 LLTBasicTest() {
    sane(1);

    TOK01Case cases[] = {
        // single token types
        {"define", "R"},
        {"declare", "R"},
        {"call", "R"},
        {"ret", "R"},
        {"alloca", "R"},
        {"getelementptr", "R"},
        {"icmp", "R"},
        {"phi", "R"},
        {"nsw", "R"},
        {"i32", "R"},
        {"ptr", "R"},
        {"void", "R"},
        {"float", "R"},
        {"foo", "S"},
        {"42", "L"},
        {"0xFF", "L"},
        {"0xK40148000", "L"},
        {"3.14", "L"},
        {"\"hello\"", "G"},
        {"c\"hi\\00\"", "G"},
        {"@", "P"},
        {"%", "P"},
        {"!", "P"},
        {"#", "P"},
        {"  ", "W"},
        // comment: TOKSplitText splits sub-tokens
        {"; x", "DDD"},
        // sigil + name/number
        {"@f", "PS"},
        {"%n", "PS"},
        {"!dbg", "PS"},
        {"#0", "PL"},
        // dotted identifiers (LLVM intrinsics)
        {"llvm.memcpy.p0.p0.i64", "S"},
        // label
        {"entry:", "SP"},
        // define: define void @f()
        {"define void @f()", "RWRWPSPP"},
        // call: call void @f()
        {"call void @f()", "RWRWPSPP"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(LLTLexer, LLT, cases, ncases);
    done;
}

ok64 MDTBasicTest() {
    sane(1);
    TOK01Case cases[] = {
        // headings: prefix R, content S→N, trailing \n stays S
        {"# Hello\n", "RRNS"},       // # + space (R,R), Hello (N), \n (S)
        {"## Sub heading\n", "RRRNWNS"}, // ## + space (R,R,R), Sub (N), sp (N), heading (N), \n (S)
        // inline code
        {"`code`", "H"},
        // bold with stars
        {"**bold**", "G"},
        // italic with star
        {"*italic*", "G"},
        // bold with underscores
        {"__bold__", "G"},
        // italic with underscore
        {"_italic_", "G"},
        // strikethrough
        {"~~deleted~~", "G"},
        // bold with inner content
        {"**bold text**", "G"},
        // italic with inner content
        {"*italic text*", "G"},
        // unmatched emphasis falls back to punct
        {"**", "P"},
        {"__", "P"},
        // numbers
        {"42", "L"},
        {"3.14", "L"},
        {"0xFF", "L"},
        // plain words
        {"hello", "S"},
        {"hello world", "SWS"},
        // punctuation
        {"[", "P"},
        {"]", "P"},
        {"(", "P"},
        {")", "P"},
        {"!", "P"},
        // chars previously missing from punct class
        {"?", "P"},
        {"$", "P"},
        {"%", "P"},
        // horizontal rule (TOKSplitText splits each char, incl newline)
        {"---\n", "RRRR"},
        {"***\n", "RRRR"},
        // whitespace
        {"  \t", "W"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    RUN_CASES(MDTLexer, MDT, cases, ncases);
    done;
}

ok64 MDTEmphTest() {
    sane(1);
    // emphasis in context
    const char *input = "some **bold** and *italic* text";
    TOK01ctx ctx = {};
    MDTstate st = {
        .data = u8scstr(input),
        .cb = TOK01cb,
        .ctx = &ctx,
    };
    ok64 o = MDTLexer(&st);
    if (o != OK) {
        fprintf(stderr, "FAIL MDTEmphTest: error %s\n", ok64str(o));
        fail(TESTFAIL);
    }
    want(ctx.count > 0);
    // "some" S, " " S, **bold** G, " " S, "and" S, " " S, *italic* G, " " S, "text" S
    want(ctx.count == 9);
    want(ctx.tags[0] == 'S');  // some
    want(ctx.tags[2] == 'G');  // **bold**
    want(ctx.tags[6] == 'G');  // *italic*
    done;
}

ok64 MDTCodeFenceTest() {
    sane(1);
    // A fenced code block
    const char *input = "```c\nint x;\n```";
    TOK01ctx ctx = {};
    MDTstate st = {
        .data = u8scstr(input),
        .cb = TOK01cb,
        .ctx = &ctx,
    };
    ok64 o = MDTLexer(&st);
    if (o != OK) {
        fprintf(stderr, "FAIL MDTCodeFenceTest: error %s\n", ok64str(o));
        fail(TESTFAIL);
    }
    want(ctx.count > 0);
    // Entire block should be 'H' (code)
    want(ctx.tags[0] == 'H');
    done;
}

ok64 MDTFenceIsolationTest() {
    sane(1);
    // Emphasis inside code fence must NOT be tagged G
    const char *input =
        "```\n"
        "**not bold**\n"
        "```\n";
    TOK01ctx ctx = {};
    MDTstate st = {
        .data = u8scstr(input),
        .cb = TOK01cb,
        .ctx = &ctx,
    };
    ok64 o = MDTLexer(&st);
    if (o != OK) {
        fprintf(stderr, "FAIL MDTFenceIsolationTest: error %s\n", ok64str(o));
        fail(TESTFAIL);
    }
    want(ctx.count == 3);  // 3 lines, each emitted as one H token
    for (int i = 0; i < ctx.count; i++)
        want(ctx.tags[i] == 'H');
    done;
}

ok64 MDTFileTest() {
    sane(1);
    const char *input =
        "# Title\n"
        "\n"
        "Some text with `inline code` and **bold**.\n"
        "\n"
        "## Section\n"
        "\n"
        "- item 1\n"
        "- item 2\n"
        "\n"
        "```python\n"
        "def foo():\n"
        "    pass\n"
        "```\n"
        "\n"
        "A number: 42\n";
    TOK01ctx ctx = {};
    MDTstate st = {
        .data = u8scstr(input),
        .cb = TOK01cb,
        .ctx = &ctx,
    };
    ok64 o = MDTLexer(&st);
    if (o != OK) {
        fprintf(stderr, "FAIL MDTFileTest: error %s\n", ok64str(o));
        fail(TESTFAIL);
    }
    want(ctx.count > 0);

    // Verify key properties:
    // - heading prefix is R
    want(ctx.tags[0] == 'R');
    // - scan for H (inline code or code fence)
    b8 found_code = NO;
    b8 found_emph = NO;
    for (int i = 0; i < ctx.count && i < 256; i++) {
        if (ctx.tags[i] == 'H') found_code = YES;
        if (ctx.tags[i] == 'G') found_emph = YES;
    }
    want(found_code);
    want(found_emph);
    done;
}

ok64 TOK01test() {
    sane(1);
    call(CTBasicTest);
    call(CTKeywordTest);
    call(TOKDispatchTest);
    call(GOTBasicTest);
    call(PYTBasicTest);
    call(JSTBasicTest);
    call(JSTFileTest);
    call(RSTBasicTest);
    call(JATBasicTest);
    call(JSONTBasicTest);
    call(SHTBasicTest);
    call(HSTBasicTest);
    call(MLTBasicTest);
    call(JLTBasicTest);
    call(TSTBasicTest);
    call(KTTBasicTest);
    call(SCLTBasicTest);
    call(SWFTBasicTest);
    call(DARTTBasicTest);
    call(ZIGTBasicTest);
    call(DTBasicTest);
    call(LUATBasicTest);
    call(PRLTBasicTest);
    call(RTBasicTest);
    call(ELXTBasicTest);
    call(ERLTBasicTest);
    call(NIMTBasicTest);
    call(NIXTBasicTest);
    call(YMLTBasicTest);
    call(TOMLTBasicTest);
    call(CFGDrainTest);
    call(SQLTBasicTest);
    call(GQLTBasicTest);
    call(PRTTBasicTest);
    call(HCLTBasicTest);
    call(SCSSTBasicTest);
    call(CSSTBasicTest);
    call(LAXTBasicTest);
    call(CLJTBasicTest);
    call(FORTBasicTest);
    call(GLSTBasicTest);
    call(SOLTBasicTest);
    call(LLTBasicTest);
    call(MDTBasicTest);
    call(MDTEmphTest);
    call(MDTCodeFenceTest);
    call(MDTFenceIsolationTest);
    call(MDTFileTest);
    done;
}

TEST(TOK01test);
