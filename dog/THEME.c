//  Three palette tables — pick via `--16` / `--dark` / `--light` on
//  any dog CLI, or via $BRO_THEME.  Borrows accent values from
//  Solarized (Ethan Schoonover, MIT) for the dark / light pair; the
//  "16" table preserves bro's pre-existing terminal-adaptive look.
//
//  Solarized 256-color approximations used below:
//    base01 = 240   base1  = 245   yellow  = 136   orange = 166
//    red    = 160   magenta= 125   violet  = 61    blue   = 33
//    cyan   = 37    green  = 64
#include "THEME.h"

#include <stdlib.h>
#include <string.h>

//  16-color: pure ANSI for fg (terminal re-maps to its own theme so
//  this adapts to dark+light terminals).  Backgrounds are 256-color
//  pale tints — present-day terminals all support them and a basic
//  ANSI bg (BG_GREEN / BG_RED) would drown the fg text.
static theme const THEME16TBL = {
    .fg_comment   = 90,    // GRAY
    .fg_string    = 32,    // DARK_GREEN
    .fg_number    = 96,    // LIGHT_CYAN
    .fg_preproc   = 35,    // DARK_PINK
    .fg_keyword   = 94,    // LIGHT_BLUE
    .fg_punct     = 90,    // GRAY
    .fg_defname   = 0,     // default + bold
    .fg_funcall   = 0,     // default + bold
    .fg_filename  = -56,   // violet 256
    .fg_title     = -56,   // violet 256
    .bg_ins       = 194,
    .bg_del       = 224,
    .bg_ins_split = 157,
    .bg_del_split = 217,
};

//  Solarized dark: muted body tones over a dark background, accent
//  colors shared with the light variant.  Diff bg goes dark-tinted
//  (22 = dark green, 52 = dark red) so it reads as a wash, not a slap.
static theme const THEMEDARKTBL = {
    .fg_comment   = -240,  // base01
    .fg_string    = -37,   // cyan
    .fg_number    = -125,  // magenta
    .fg_preproc   = -166,  // orange
    .fg_keyword   = -64,   // green
    .fg_punct     = -240,  // base01
    .fg_defname   = -33,   // blue (bold)
    .fg_funcall   = -33,   // blue (bold)
    .fg_filename  = -61,   // violet
    .fg_title     = -61,   // violet
    .bg_ins       = 22,    // dark green
    .bg_del       = 52,    // dark red
    .bg_ins_split = 28,    // mid green
    .bg_del_split = 88,    // mid red
};

//  Solarized light: same accents on a light background, body tones
//  bumped to base1 (245) for legibility on white.  Diff bg matches
//  the 16-color table — those pale tints were already light-mode.
static theme const THEMELIGHTTBL = {
    .fg_comment   = -245,  // base1
    .fg_string    = -37,   // cyan
    .fg_number    = -125,  // magenta
    .fg_preproc   = -166,  // orange
    .fg_keyword   = -64,   // green
    .fg_punct     = -245,  // base1
    .fg_defname   = -33,   // blue (bold)
    .fg_funcall   = -33,   // blue (bold)
    .fg_filename  = -61,   // violet
    .fg_title     = -61,   // violet
    .bg_ins       = 194,
    .bg_del       = 224,
    .bg_ins_split = 157,
    .bg_del_split = 217,
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
