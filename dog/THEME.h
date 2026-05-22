#ifndef DOG_THEME_H
#define DOG_THEME_H

#include "abc/INT.h"

con ok64 THEMEBAD = 0x75139638b28d;       // unknown theme name

//  Shared color palette for dog viewers (bro, graf, spot output).
//  Tag letters match dog/INDEX.md "Tag mapping":
//    D comment, G string, L number, H preproc, R keyword, P punct,
//    N defined name, C function call, F filename.
//  Diff-side backgrounds use the convention from bro:
//    I/D = INS/DEL in the normal (inline) pass,
//    i/d = INS/RM bytes in the split (in-pass / rm-pass) rows.
//
//  Encoding for `fg_*` fields:
//    > 0  →  ANSI 16-color SGR code (e.g. 32 = green, 94 = light blue)
//    < 0  →  -(256-color slot N)                (e.g. -56 = violet 256)
//    = 0  →  default fg (terminal-dependent)
//  Encoding for `bg_*` fields:
//    > 0  →  256-color slot N
//    = 0  →  default bg (no highlight)
typedef struct {
    int fg_comment;   // 'D'
    int fg_string;    // 'G'
    int fg_number;    // 'L'
    int fg_preproc;   // 'H'
    int fg_keyword;   // 'R'
    int fg_punct;     // 'P'
    int fg_defname;   // 'N' (caller adds bold)
    int fg_funcall;   // 'C' (caller adds bold)
    int fg_filename;  // 'F'
    int fg_title;     // hunk title bar (also filename URIs)
    int bg_ins;       // 'I' + 'g' (eq context in in-pass)
    int bg_del;       // 'D' + 'p' (eq context in rm-pass)
    int bg_ins_split; // 'i'
    int bg_del_split; // 'd'
} theme;

#define THEME_16    "16"
#define THEME_DARK  "dark"
#define THEME_LIGHT "light"

//  Currently-active palette pointer.  Initialised to the 16-color
//  table at process start; THEMESelect re-points it.  Always non-NULL.
extern theme const *THEMEActive;

//  Pick a theme by name ("16", "dark", or "light").  NULL/empty falls
//  back to `$BRO_THEME`; missing env → "16".  Returns THEMEBAD on an
//  unknown name (leaves THEMEActive unchanged).  Also setenv()s
//  BRO_THEME so child processes (BROForkBe, spot, graf) inherit.
ok64 THEMESelect(char const *name);

#endif
