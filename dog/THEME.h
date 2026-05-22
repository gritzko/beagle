#ifndef DOG_THEME_H
#define DOG_THEME_H

#include "abc/ANSI.h"
#include "abc/INT.h"

con ok64 THEMEBAD = 0x75139638b28d;       // unknown theme name

//  Flat color palette indexed by tag letter (`tag - 'A'`).  Each slot
//  is one ansi64 carrying fg-only, bg-only, or default — the renderer
//  ORs two lookups (fg-tag + bg-tag) into the final cell SGR.  Slot
//  letters are deliberately disjoint between fg-roles (D G L H R P N
//  C F T) and bg-roles (I J K O) so a single 32-slot table covers
//  both without collision.  Slot 'A' is reserved for the
//  "no-attribute" sentinel (ANSI_DEFAULT) — used by callers that
//  pass 'A' to mean "skip the bg / fg".
//
//  Tag mapping (matches dog/INDEX.md "Tag mapping" plus bro's
//  diff-side bg conventions):
//
//    Fg:  D comment   G string    L number    H preproc
//         R keyword   P punct     N defname (bold)   C funcall (bold)
//         F filename  T title
//    Bg:  I INS (normal + in-pass eq context)
//         O DEL (normal + rm-pass eq context)   ← renamed from 'D'
//         J INS-split (in-pass IN bytes)        ← renamed from 'i'
//         K DEL-split (rm-pass RM bytes)        ← renamed from 'd'
typedef ansi64 theme[32];

#define THEME_16    "16"
#define THEME_DARK  "dark"
#define THEME_LIGHT "light"

//  Currently-active palette pointer; always non-NULL.  Defaults to
//  the 16-color table until THEMESelect re-points it.
extern theme const *THEMEActive;

//  Slot lookup with range check.  `tag` outside A..Z (incl. 0 / 'A')
//  returns ANSI_DEFAULT so the renderer can OR results without
//  guarding.
fun ansi64 THEMEAt(u8 tag) {
    u8 i = (u8)(tag - 'A');
    return (i < 32) ? (*THEMEActive)[i] : ANSI_DEFAULT;
}

//  Pick a theme by name ("16", "dark", or "light").  NULL/empty falls
//  back to `$BRO_THEME`; missing env → "16".  Returns THEMEBAD on an
//  unknown name (leaves THEMEActive unchanged).  Also setenv()s
//  BRO_THEME so child processes inherit the choice.
ok64 THEMESelect(char const *name);

#endif
