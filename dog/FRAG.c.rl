#include "FRAG.h"
#include <string.h>

%%{
    machine frag;

    action line_start { nval = 0; }
    action line_digit { nval = nval * 10 + (*p - '0'); }
    action line_end   { if (f->line == 0) f->line = nval; else f->line_end = nval; }
    action range_end  { f->line_end = nval; }
    action ext_start  { ext_mark = p; }
    action ext_end    {
        if (f->nexts < FRAG_MAX_EXTS) {
            f->exts[f->nexts][0] = ext_mark;
            f->exts[f->nexts][1] = p;
            f->nexts++;
        }
    }

    number = ( digit >line_start digit* ) $line_digit ;

    line_or_range = number %line_end ( '-' number %range_end )? ;

    ext = '.' ( [A-Za-z0-9]+ >ext_start %ext_end ) ;

    ext_spec = ext+ ;

    # Line-only fragment, optionally with ext filters.
    frag_line = line_or_range ext_spec? ;

    # Ext-only fragment (e.g. `.c.h`).
    frag_ext  = ext_spec ;

    main := ( frag_line | frag_ext ) ;
}%%

%% write data nofinal noerror;

ok64 FRAGu8sDrain(u8cs input, fragp f) {
    if (f == NULL) return FRAGBAD;
    memset(f, 0, sizeof(frag));
    if (input[0] == NULL || input[0] >= input[1]) return OK;

    u8cp p = input[0];
    u8cp pe = input[1];
    u8cp eof = pe;
    u8cp ext_mark = NULL;
    u32 nval = 0;
    int cs = 0;

    %% write init;
    %% write exec;

    if (cs < %%{ write first_final; }%%) {
        return FRAGFAIL;
    }

    if (f->type == FRAG_NONE && f->line > 0)
        f->type = FRAG_LINE;

    return OK;
}

// Is `c` legal in a URI fragment?
// Printable ASCII (0x20-0x7E) except '#' (fragment delimiter) and '%' (pct prefix).
static const u8 FRAG_CHAR[256] = {
    // 0x00-0x1F: control chars — illegal
    [0x20] = 1,  // space (unwise, but legal per URI.lex)
    [0x21] = 1,  // !
    [0x22] = 1,  // "
    // 0x23 '#' — fragment delimiter, must escape
    [0x24] = 1,  // $
    // 0x25 '%' — pct-encoded prefix, must escape
    [0x26] = 1,  // &
    [0x27] = 1,  // '
    [0x28] = 1,  // (
    [0x29] = 1,  // )
    [0x2A] = 1,  // *
    [0x2B] = 1,  // +
    [0x2C] = 1,  // ,
    [0x2D] = 1,  // -
    [0x2E] = 1,  // .
    [0x2F] = 1,  // /
    ['0'] = 1, ['1'] = 1, ['2'] = 1, ['3'] = 1, ['4'] = 1,
    ['5'] = 1, ['6'] = 1, ['7'] = 1, ['8'] = 1, ['9'] = 1,
    [0x3A] = 1,  // :
    [0x3B] = 1,  // ;
    [0x3C] = 1,  // <
    [0x3D] = 1,  // =
    [0x3E] = 1,  // >
    [0x3F] = 1,  // ?
    [0x40] = 1,  // @
    ['A'] = 1, ['B'] = 1, ['C'] = 1, ['D'] = 1, ['E'] = 1, ['F'] = 1,
    ['G'] = 1, ['H'] = 1, ['I'] = 1, ['J'] = 1, ['K'] = 1, ['L'] = 1,
    ['M'] = 1, ['N'] = 1, ['O'] = 1, ['P'] = 1, ['Q'] = 1, ['R'] = 1,
    ['S'] = 1, ['T'] = 1, ['U'] = 1, ['V'] = 1, ['W'] = 1, ['X'] = 1,
    ['Y'] = 1, ['Z'] = 1,
    [0x5B] = 1,  // [
    [0x5C] = 1,  // backslash
    [0x5D] = 1,  // ]
    [0x5E] = 1,  // ^
    [0x5F] = 1,  // _
    [0x60] = 1,  // `
    ['a'] = 1, ['b'] = 1, ['c'] = 1, ['d'] = 1, ['e'] = 1, ['f'] = 1,
    ['g'] = 1, ['h'] = 1, ['i'] = 1, ['j'] = 1, ['k'] = 1, ['l'] = 1,
    ['m'] = 1, ['n'] = 1, ['o'] = 1, ['p'] = 1, ['q'] = 1, ['r'] = 1,
    ['s'] = 1, ['t'] = 1, ['u'] = 1, ['v'] = 1, ['w'] = 1, ['x'] = 1,
    ['y'] = 1, ['z'] = 1,
    [0x7B] = 1,  // {
    [0x7C] = 1,  // |
    [0x7D] = 1,  // }
    [0x7E] = 1,  // ~
    // 0x7F DEL — control, illegal
    // 0x80-0xFF — non-ASCII, illegal
};

con u8c FRAG_HEX[16] = "0123456789ABCDEF";

ok64 FRAGu8sEsc(u8s into, u8cs raw) {
    if (into[0] == NULL || into[0] >= into[1]) return FRAGFAIL;
    if (raw[0] == NULL || raw[0] >= raw[1]) return OK;
    u8cp p = raw[0];
    u8cp end = raw[1];
    while (p < end) {
        u8 c = *p++;
        if (FRAG_CHAR[c]) {
            if (into[0] >= into[1]) return FRAGFAIL;
            *into[0]++ = c;
        } else {
            if (into[0] + 3 > into[1]) return FRAGFAIL;
            *into[0]++ = '%';
            *into[0]++ = FRAG_HEX[(c >> 4) & 0xF];
            *into[0]++ = FRAG_HEX[c & 0xF];
        }
    }
    return OK;
}

static int frag_hexval(u8 c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

ok64 FRAGu8sUnesc(u8s into, u8cs esc) {
    if (into[0] == NULL || into[0] >= into[1]) return FRAGFAIL;
    if (esc[0] == NULL || esc[0] >= esc[1]) return OK;
    u8cp p = esc[0];
    u8cp end = esc[1];
    while (p < end) {
        if (into[0] >= into[1]) return FRAGFAIL;
        u8 c = *p++;
        if (c == '%' && p + 2 <= end) {
            int hi = frag_hexval(p[0]);
            int lo = frag_hexval(p[1]);
            if (hi >= 0 && lo >= 0) {
                *into[0]++ = (u8)((hi << 4) | lo);
                p += 2;
                continue;
            }
        }
        *into[0]++ = c;
    }
    return OK;
}
