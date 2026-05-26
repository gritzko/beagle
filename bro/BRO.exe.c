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
#include "dog/THEME.h"
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
        "Themes: --16 (default, terminal-adaptive ANSI)\n"
        "        --dark (Solarized dark)   --light (Solarized light)\n"
        "        also via $BRO_THEME=16|dark|light\n"
        "\n"
        "Keys: q quit, space/f page down, b page up, j/k line, g/G top/end,\n"
        "      / or ' search, n/N next/prev, r reload,\n"
        "      : URI prompt (path#line, #grep.ext, #'snippet'.ext),\n"
        "      [ ] { } prev/next hunk, ( ) prev/next change,\n"
        "      Enter/l open file, h/Backspace back, . list dir,\n"
        "      m toggle mouse (wheel scroll, L-click open, R-click grep)\n");
}

// Fetch a versioned/remote URI's blob via keeper and stage it as a
// hunk.  The blob lands in a BASS scratch buffer that is rewound when
// this frame returns (caller invokes via try, once per URI) — the
// kept bytes are copied into b->arena first, so nothing dangles.
static ok64 bro_stage_keeper_uri(bro *b, uri *u, u8cs file_path) {
    sane(b && u);
    a_carve(u8, blobbuf, 1UL << 24);
    call(KEEPGetByURI, u, blobbuf);
    hunk *hk = hunkbIdleHead(b->hunks);
    *hk = (hunk){};
    hk->verb = HUNK_VERB_HUNK;
    call(u8bHost, b->arena, hk->text, u8bDataC(blobbuf));
    call(u8bHost, b->arena, hk->uri, u->data);
    BROTokenize(hk, file_path);
    hunkbFed1(b->hunks);
    done;
}

// --- Entry ---

ok64 BROExec(bro *b, cli *c) {
    sane(b && c);

    //  Universal three-mode rule: `HUNKMode` (set in main via
    //  `CLISetHUNKMode`) tracks --tlv / --color / --plain / ANSIIsTTY().
    //  `be` forwards the resolved flag explicitly when spawning bro,
    //  so we just trust `HUNKMode` here.  `NO_COLOR` (de-facto standard,
    //  see https://no-color.org) still wins outright.
    b->color = (HUNKMode == HUNKOutColor);
    if (getenv("NO_COLOR"))  b->color = NO;

    if (CLIHas(c, "-h") || CLIHas(c, "--help")) {
        bro_usage();
        done;
    }

    //  --16 / --dark / --light pick a dog/THEME palette; later flags
    //  win.  Without a flag, THEMESelect(NULL) consults $BRO_THEME and
    //  defaults to "16".  THEMESelect setenv's so child bros spawned
    //  through BROForkBe inherit the choice.
    char const *theme_name = NULL;
    if (CLIHas(c, "--16"))    theme_name = THEME_16;
    if (CLIHas(c, "--dark"))  theme_name = THEME_DARK;
    if (CLIHas(c, "--light")) theme_name = THEME_LIGHT;
    ok64 to = THEMESelect(theme_name);
    if (to != OK) {
        fprintf(stderr, "bro: unknown theme: %s\n", ok64str(to));
        fail(to);
    }

    if (uribDataLen(c->uris) > 0) {
        call(BROArenaInit);
        b8 keeper_open = NO;
        for (u32 i = 0; i < uribDataLen(c->uris); i++) {
            if (hunkbIdleLen(b->hunks) == 0) break;
            uri *u = uribAtP(c->uris, i);

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
                try(bro_stage_keeper_uri, b, u, file_path);
                nedo fprintf(stderr, "bro: cannot fetch " U8SFMT ": %s\n",
                             u8sFmt(u->data), ok64str(__));
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
            hk->verb = HUNK_VERB_HUNK;
            call(u8bHost, b->arena, hk->uri, u->data);
            hk->text[0] = u8bDataHead(mapped);
            hk->text[1] = u8bIdleHead(mapped);

            BROTokenize(hk, file_path);
            hunkbFed1(b->hunks);
            BRODefer(mapped);
        }
        if (hunkbDataLen(b->hunks) > 0)
            BRORun(hunkbDataC(b->hunks));
        BROArenaCleanup();
        if (keeper_open) KEEPClose();
    } else {
        call(BROPipeRun, STDIN_FILENO);
    }
    done;
}
