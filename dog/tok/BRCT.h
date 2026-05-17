//
// BRCT - Bracket detection for tokenized files
//
// Works with packed u32 token arrays (from TOK.h) and source bytes.
// Supports {} () []. Only inspects P-tagged tokens (punctuation),
// so brackets inside strings/comments are ignored.
// Returns token indices; use tok32Val to get byte slices.
//

#ifndef TOK_BRCT_H
#define TOK_BRCT_H

#include "TOK.h"

con ok64 BRCTNONE = 0x2db31d5d85ce;
con ok64 BRCTBAD = 0xb6cc74b28d;

// Find matching bracket for token at index `at`.
// Returns the index of the matching bracket, or -1 if not a bracket
// or unmatched. Works in both directions (open→close, close→open).
i64 BRCTMatch(u32csc toks, u8csc data, u64 at);

// Find the innermost bracketed region containing token `at`.
// Sets *from/*till to the open/close bracket indices.
// Returns BRCTNONE if `at` is not inside any brackets.
ok64 BRCTInner(u64 *from, u64 *till, u32csc toks, u8csc data, u64 at);

// Find the outermost bracketed region containing token `at`.
ok64 BRCTOuter(u64 *from, u64 *till, u32csc toks, u8csc data, u64 at);

// Check if all brackets are balanced in the token range.
// Returns OK if balanced, BRCTBAD if not.
ok64 BRCTCheck(u32csc toks, u8csc data);

// Get bracket nesting depth at token `at` (0 = top level).
u64 BRCTDepth(u32csc toks, u8csc data, u64 at);

#endif
