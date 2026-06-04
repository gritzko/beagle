//  Three palette tables — pick via `--16` / `--dark` / `--light` on
//  any dog CLI, or via $BRO_THEME.  Borrows accent values from
//  Solarized (Ethan Schoonover, MIT) for the dark / light pair; the
//  "16" table preserves bro's pre-existing terminal-adaptive look.
//
//  Each table is a 32-slot ansi64 array indexed by `tag - 'A'`; only
//  the letters listed in THEME.h are populated, the rest are
//  ANSI_DEFAULT (zero-init).
//
//  Solarized 256-color approximations used below:
//    base01 = 240   base1  = 245   yellow  = 136   orange = 166
//    red    = 160   magenta= 125   violet  = 61    blue   = 33
//    cyan   = 37    green  = 64
#include "THEME.h"

#include <stdlib.h>
#include <string.h>

#define FG16(n)  ANSI64_FG_BASIC(n)
#define FG256(n) ANSI64_FG_256(n)
#define BG256(n) ANSI64_BG_256(n)
#define BOLD     ANSI64_FLAG(ANSI_BOLD)
#define IDX(L)   [(L) - 'A']

//  16-color: pure ANSI 16 for fg (terminal re-maps so it adapts to
//  light+dark terminals).  Bg uses 256-color pale tints — basic ANSI
//  bg (BG_GREEN / BG_RED) would drown the fg text.
static theme const THEME16TBL = {
    IDX('D') = FG16(90),    // comment   — GRAY
    IDX('G') = FG16(32),    // string    — DARK_GREEN
    IDX('L') = FG16(96),    // number    — LIGHT_CYAN
    IDX('H') = FG16(35),    // preproc   — DARK_PINK
    IDX('R') = FG16(94),    // keyword   — LIGHT_BLUE
    IDX('P') = FG16(90),    // punct     — GRAY
    IDX('N') = BOLD,        // defname   — default + bold
    IDX('C') = BOLD,        // funcall   — default + bold
    IDX('F') = FG256(56),   // filename  — violet 256
    IDX('T') = FG256(56),   // title     — violet 256
    IDX('I') = BG256(194),
    IDX('O') = BG256(224),
    IDX('J') = BG256(157),
    IDX('K') = BG256(217),
    //  Status verbs — pre-THEME palette mapping (sniff/SNIFF.exe.c
    //  STATUS_ANSI_*) preserved bit-for-bit so existing scripts that
    //  grep coloured output keep matching.
    IDX('U') = FG16(34),    // put       — blue
    IDX('W') = FG16(32),    // new       — green
    IDX('V') = FG16(36),    // mov       — cyan
    IDX('E') = FG16(33),    // mod       — yellow
    IDX('X') = FG256(94),   // del       — 256-brown
    //  BE-001: red is reserved for CONFLICT statuses only.  Slot 'M'
    //  carries the conflict family (mis/conflict/conf/modl) in bright
    //  red; slot 'S' is the tok / status DEFAULT and MUST stay neutral
    //  (ANSI_DEFAULT) — it tags ordinary code identifiers, whitespace
    //  and the neutral status columns, none of which are conflicts.
    IDX('M') = FG16(91),    // mis/conf/modl — bright red (DIS-018, BE-001)
    //  'S' intentionally ANSI_DEFAULT (zero-init) — see note above.
    IDX('Q') = FG16(90),    // unk       — grey
    IDX('Y') = FG16(34),    // upd       — blue (== put)
    IDX('Z') = FG16(35),    // mrg       — magenta
    IDX('B') = FG16(33),    // eq/hunk   — yellow (no pale option in 16)
};

//  Solarized dark: muted base01 (240) body, accents in cyan/magenta/
//  orange/green/blue/violet, diff bg dark-tinted (22 / 52) so it
//  reads as a wash, not a slap.
static theme const THEMEDARKTBL = {
    IDX('D') = FG256(240),                // base01
    IDX('G') = FG256(37),                 // cyan
    IDX('L') = FG256(125),                // magenta
    IDX('H') = FG256(166),                // orange
    IDX('R') = FG256(64),                 // green
    IDX('P') = FG256(240),
    IDX('N') = FG256(33) | BOLD,          // blue + bold
    IDX('C') = FG256(33) | BOLD,
    IDX('F') = FG256(61),                 // violet
    IDX('T') = FG256(61),
    IDX('I') = BG256(22),                 // dark green
    IDX('O') = BG256(52),                 // dark red
    IDX('J') = BG256(28),                 // mid green
    IDX('K') = BG256(88),                 // mid red
    //  Status verbs — Solarized palette analogues (blue 33, green 64,
    //  cyan 37, yellow 136, orange 166, red 160, magenta 125, base01
    //  240 for grey, violet 61).  Pairings preserve the 16-color
    //  semantic groupings — put/upd both blue, new green, mov cyan,
    //  mis red, mrg magenta, etc.
    IDX('U') = FG256(33),                 // put       — blue
    IDX('W') = FG256(64),                 // new       — green
    IDX('V') = FG256(37),                 // mov       — cyan
    IDX('E') = FG256(136),                // mod       — yellow
    IDX('X') = FG256(166),                // del       — orange (Solarized)
    //  BE-001: conflict family (mis/conf/modl) → slot 'M' bright red;
    //  slot 'S' is the DEFAULT tag and stays ANSI_DEFAULT (neutral).
    IDX('M') = FG256(196),                // mis/conf/modl — bright red
    //  'S' intentionally ANSI_DEFAULT (zero-init).
    IDX('Q') = FG256(240),                // unk       — base01 grey
    IDX('Y') = FG256(33),                 // upd       — blue
    IDX('Z') = FG256(125),                // mrg       — magenta
    IDX('B') = FG256(180),                // eq/hunk   — muted yellow
};

//  Solarized light: same accents over a light background; body bumped
//  to base1 (245) for legibility on white.  Diff bg matches the
//  16-color table — those pale tints are already light-mode.
static theme const THEMELIGHTTBL = {
    IDX('D') = FG256(245),                // base1
    IDX('G') = FG256(37),
    IDX('L') = FG256(125),
    IDX('H') = FG256(166),
    IDX('R') = FG256(64),
    IDX('P') = FG256(245),
    IDX('N') = FG256(33) | BOLD,
    IDX('C') = FG256(33) | BOLD,
    IDX('F') = FG256(61),
    IDX('T') = FG256(61),
    IDX('I') = BG256(194),
    IDX('O') = BG256(224),
    IDX('J') = BG256(157),
    IDX('K') = BG256(217),
    //  Status verbs — Solarized accents over light bg.  Same hues as
    //  the dark palette (Solarized is symmetric); only the body/punct
    //  base shifts to base1 (245) for legibility.
    IDX('U') = FG256(33),
    IDX('W') = FG256(64),
    IDX('V') = FG256(37),
    IDX('E') = FG256(136),
    IDX('X') = FG256(166),
    //  BE-001: conflict family (mis/conf/modl) → slot 'M' bright red;
    //  slot 'S' is the DEFAULT tag and stays ANSI_DEFAULT (neutral).
    IDX('M') = FG256(196),                // mis/conf/modl — bright red
    //  'S' intentionally ANSI_DEFAULT (zero-init).
    IDX('Q') = FG256(245),                // base1 (lighter than base01)
    IDX('Y') = FG256(33),
    IDX('Z') = FG256(125),
    IDX('B') = FG256(186),                // eq/hunk   — pale yellow
};

theme const *THEMEActive = &THEME16TBL;

ok64 THEMESelect(char const *name) {
    if (name == NULL || name[0] == 0) name = getenv("BRO_THEME");
    if (name == NULL || name[0] == 0) name = THEME_16;
    if (strcmp(name, THEME_16) == 0) {
        THEMEActive = &THEME16TBL;
    } else if (strcmp(name, THEME_DARK) == 0) {
        THEMEActive = &THEMEDARKTBL;
    } else if (strcmp(name, THEME_LIGHT) == 0) {
        THEMEActive = &THEMELIGHTTBL;
    } else {
        return THEMEBAD;
    }
    setenv("BRO_THEME", name, 1);
    return OK;
}
