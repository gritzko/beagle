#include "TOK.h"

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"

static ok64 toktok_cb(u8 tag, u8cs tok, void *ctx) {
    (void)ctx;
    fputc(tag, stdout);
    fputc('\t', stdout);
    u8c const *p = tok[0];
    while (p < tok[1]) {
        u8 c = *p;
        switch (c) {
            case '\n': fputc('\\', stdout); fputc('n', stdout); break;
            case '\r': fputc('\\', stdout); fputc('r', stdout); break;
            case '\t': fputc('\\', stdout); fputc('t', stdout); break;
            case '\\': fputc('\\', stdout); fputc('\\', stdout); break;
            default:   fputc(c, stdout); break;
        }
        ++p;
    }
    fputc('\n', stdout);
    return OK;
}

static u8 _srcbuf[1 << 24];

ok64 toktok() {
    sane($arglen >= 2);
    a$rg(file, 1);

    a_pad(u8, path, 4096);
    call(u8sFeed, path_idle, file);
    PATHu8bTerm(path);

    // Find the extension
    u8cs ext = {NULL, NULL};
    u8c const *scan = file[1];
    while (scan > file[0]) {
        --scan;
        if (*scan == '.') {
            ext[0] = (u8c*)(scan + 1);
            ext[1] = (u8c*)file[1];
            break;
        }
        if (*scan == '/') break;
    }
    if (ext[0] == NULL) fail(TOKBAD);

    u8 *idle[2] = {_srcbuf, _srcbuf + sizeof(_srcbuf)};
    int fd;
    call(FILEOpen, &fd, $path(path), O_RDONLY);
    u8 *start = idle[0];
    call(FILEdrainall, idle, fd);
    call(FILEClose, &fd);

    TOKstate state = {
        .data = {start, idle[0]},
        .cb = toktok_cb,
        .ctx = NULL,
    };
    call(TOKLexer, &state, ext);
    done;
}

MAIN(toktok);
