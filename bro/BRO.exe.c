//  BROExec — run a parsed CLI against an open bro state.
//  Same effect as invoking `bro ...` as a separate process.
//
#include "BRO.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/URI.h"
#include "dog/CLI.h"
#include "keeper/KEEP.h"

// --- Usage ---

static void bro_usage(void) {
    fprintf(stderr,
        "Usage: bro [URI...]\n"
        "\n"
        "  bro                   read TLV hunks from stdin (pager mode)\n"
        "  bro file.c [...]      syntax-highlighted cat\n"
        "  bro file.c#42         open file at line 42\n"
        "  bro dir/              list directory\n"
        "\n"
        "Keys: q quit, space/f page down, b page up, j/k line, g/G top/end,\n"
        "      / or ' search, n/N next/prev,\n"
        "      : URI prompt (path#line, #grep.ext, #'snippet'.ext),\n"
        "      [ ] { } prev/next hunk, ( ) prev/next change,\n"
        "      Enter/l open file, h back, . list dir,\n"
        "      m toggle mouse (wheel scroll, click to open)\n");
}

// --- Entry ---

ok64 BROExec(bro *b, cli *c) {
    sane(b && c);

    b->color = c->tty_out;
    if (getenv("BRO_COLOR")) b->color = YES;
    if (getenv("NO_COLOR"))  b->color = NO;

    if (CLIHas(c, "-h") || CLIHas(c, "--help")) {
        bro_usage();
        done;
    }

    if (c->nuris > 0) {
        call(BROArenaInit);
        b8 keeper_open = NO;
        for (u32 i = 0; i < c->nuris; i++) {
            if (hunkbIdleLen(b->hunks) == 0) break;
            uri *u = &c->uris[i];

            u8cs file_path = {};
            if (!$empty(u->path)) {
                $mv(file_path, u->path);
            } else if (!$empty(u->data)) {
                $mv(file_path, u->data);
            }
            if ($empty(file_path)) continue;

            //  Versioned / remote URI — goes to keeper, not the FS.
            u8 pat = URIPattern(u);
            if (pat & (URI_HOST | URI_QUERY)) {
                if (!keeper_open) {
                    ok64 ko = KEEPOpen(b->h, NO);
                    if (ko != OK && ko != KEEPOPEN) {
                        fprintf(stderr, "bro: cannot open keeper: %s\n",
                                ok64str(ko));
                        continue;
                    }
                    keeper_open = YES;
                }
                Bu8 blobbuf = {};
                if (u8bAllocate(blobbuf, 1UL << 24) != OK) continue;
                ok64 go = KEEPGetByURI(u, blobbuf);
                if (go != OK) {
                    fprintf(stderr, "bro: cannot fetch " U8SFMT ": %s\n",
                            u8sFmt(u->data), ok64str(go));
                    u8bFree(blobbuf);
                    continue;
                }
                size_t blen = u8bDataLen(blobbuf);
                u8p body = BROArenaWrite(u8bDataHead(blobbuf), blen);
                u8bFree(blobbuf);
                if (!body) continue;

                hunk *hk = hunkbIdleHead(b->hunks);
                *hk = (hunk){};
                u8p up = BROArenaWrite(u->data[0], (size_t)$len(u->data));
                if (up) { hk->uri[0] = up; hk->uri[1] = up + $len(u->data); }
                hk->text[0] = body;
                hk->text[1] = body + blen;
                BROTokenize(hk, file_path);
                BROHunkAdd();
                continue;
            }

            a_path(fpbuf);
            __ = PATHu8bFeed(fpbuf, file_path);
            if (__ != OK) continue;
            __ = PATHu8bTerm(fpbuf);
            if (__ != OK) continue;

            filestat fs = {};
            if (FILEStat(&fs, $path(fpbuf)) == OK &&
                fs.kind == FILE_KIND_DIR) {
                BROListDir(file_path);
                continue;
            }

            u8bp mapped = NULL;
            ok64 o = FILEMapRO(&mapped, $path(fpbuf));
            if (o != OK) {
                fprintf(stderr, "bro: cannot open " U8SFMT ": %s\n",
                        u8sFmt(file_path), ok64str(o));
                continue;
            }

            hunk *hk = hunkbIdleHead(b->hunks);
            *hk = (hunk){};
            u8p up = BROArenaWrite(u->data[0], (size_t)$len(u->data));
            if (up) { hk->uri[0] = up; hk->uri[1] = up + $len(u->data); }
            hk->text[0] = u8bDataHead(mapped);
            hk->text[1] = u8bIdleHead(mapped);

            BROTokenize(hk, file_path);
            BROHunkAdd();
            BRODefer(mapped);
        }
        if (hunkbDataLen(b->hunks) > 0)
            BRORun(hunkbDataHead(b->hunks),
                   (u32)hunkbDataLen(b->hunks));
        BROArenaCleanup();
        if (keeper_open) KEEPClose();
    } else {
        call(BROPipeRun, STDIN_FILENO);
    }
    done;
}
