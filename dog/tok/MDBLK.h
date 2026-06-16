#ifndef TOK_MDBLK_H
#define TOK_MDBLK_H

//  Shared slice-cursor primitives for the Markdown family block lexers
//  (MDT / MKDT).  Each operates on a consumable `u8cs` line cursor —
//  callers advance the same cursor across calls.  No raw pointer
//  arithmetic, no hand-rolled `p++` walks: every step goes through the
//  typed slice API (PTR-002).
//
//  These capture the byte-shape decisions both grammars share.  The two
//  grammars themselves differ (CommonMark vs StrictMark indent / fence /
//  heading rules), so the lexer policy stays in each `.c`; only the
//  cursor-walking idiom is shared here.

#include "TOK.h"

//  Consume up to `max` leading spaces from `line`; return how many were
//  eaten.  Mirrors the "skip ≤N indent spaces" preamble.
fun int MDBLKu8csSkipSpaces(u8cs line, int max) {
    int n = 0;
    while (n < max && !u8csEmpty(line) && *u8csHead(line) == ' ') {
        u8csUsed1(line);
        n++;
    }
    return n;
}

//  Consume leading 4-space "indent blocks" (StrictMark div markup);
//  return the count of full blocks eaten.  Stops at the first group of
//  fewer than four spaces.
fun int MDBLKu8csSkipIndents(u8cs line) {
    int depth = 0;
    while (u8csLen(line) >= 4 && line[0][0] == ' ' && line[0][1] == ' ' &&
           line[0][2] == ' ' && line[0][3] == ' ') {
        u8csUsed(line, 4);
        depth++;
    }
    return depth;
}

//  Consume a run of `ch` from the head of `line`; return run length.
fun int MDBLKu8csRun(u8cs line, u8 ch) {
    int n = 0;
    while (!u8csEmpty(line) && *u8csHead(line) == ch) {
        u8csUsed1(line);
        n++;
    }
    return n;
}

//  YES iff every remaining byte of `line` is ASCII whitespace
//  (space / tab / newline / carriage return).
fun b8 MDBLKu8csAllBlank(u8csc line) {
    $for(u8c, p, line) {
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') return NO;
    }
    return YES;
}

//  Pull one source line off the head of `in`, INCLUDING its trailing
//  '\n' (unlike abc's `u8csDrainLine`, which strips it).  The block
//  lexers emit fence / heading lines verbatim with their newline, so
//  the line bytes must carry it.  `in` advances past the line.  Returns
//  NONE when `in` is empty.
fun ok64 MDBLKu8csDrainLine(u8cs in, u8csp line_out) {
    if (u8csEmpty(in)) return NONE;
    u8c const *start = in[0];
    if (u8csFind(in, '\n') == OK) u8csUsed1(in);  // include the newline
    line_out[0] = start;
    line_out[1] = in[0];
    return OK;
}

#endif
