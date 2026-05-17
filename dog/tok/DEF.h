//
// DEF.h — Mark symbol definitions in tokenized files
//
// Takes a packed u32 token array (from TOK.h), enriches tags to a
// u8 byte stream, runs NFA patterns per language to find definitions,
// re-tags defined symbols S → N.
//
// Enriched alphabet:
//   s = identifier          f = defining keyword
//   r = other keyword       ( ) { } ; = , : * < > = themselves
//   p = other punctuation   g = string  l = number  h = preproc
//   Whitespace and comments are stripped (index map tracks positions).
//

#ifndef TOK_DEF_H
#define TOK_DEF_H

#include "TOK.h"

#define DEF_TAG 'N'
#define CALL_TAG 'C'

con ok64 DEFFAIL = 0xd38f3ca495;

// Mark definition symbols in a token array.
// toks: mutable packed token array (tags may be rewritten S→N).
// data: original source bytes.
// ext:  file extension (e.g. ".c", ".go") to select language patterns.
ok64 DEFMark(u32 *toks[2], u8csc data, u8csc ext);

#endif
