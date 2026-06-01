#ifndef TOK_MKDT_H
#define TOK_MKDT_H

#include "TOK.h"

con ok64 MKDTBAD = 0x1650d74b28d;
con ok64 MKDTFAIL = 0x59435d3ca495;

typedef struct {
    u8cs data;
    TOKcb cb;
    void *ctx;
} MKDTstate;

ok64 MKDTLexer(MKDTstate *state);

// Inline tokenizer (generated from MKDT.c.rl); emits G/H/L/S/P/W via cb.
ok64 MKDTInlineLexer(MKDTstate *state);

// --- Block-line classifiers ---
//
// These wrap the line-shape decisions MKDTLexer makes internally and are
// exposed so renderers (e.g. the `mark` dog) can recover block structure
// without re-deriving it.  The StrictMark grammar itself is unchanged.
// Each takes one source line (with or without its trailing '\n').

int MKDTFenceOpen(u8csc line);             // opening fence width 3/4, else 0
b8 MKDTFenceClose(u8csc line, int flen);   // closes a fence of width flen
int MKDTHeadingLevel(u8csc line);          // ATX level 1..4, else 0
b8 MKDTHRule(u8csc line);                  // "----" horizontal ruler
b8 MKDTRefDef(u8csc line);                 // "[x]: …" reference definition
int MKDTIndentDepth(u8csc line);           // count of leading 4-space blocks

typedef enum {
    MKDT_MARK_NONE = 0,
    MKDT_MARK_QUOTE,  // >
    MKDT_MARK_ULIST,  // -
    MKDT_MARK_OLIST,  // N.
    MKDT_MARK_TODO,   // [ ] [x] [X]
} mkdtmark;

// Classify the block marker in the 4-char group after `depth` indents.
// *markend := end of the 4-char marker slot on a hit, else line[0]+depth*4.
mkdtmark MKDTLineMarker(u8csc line, int depth, u8c **markend);

#endif
