#ifndef KEEPER_PKT_H
#define KEEPER_PKT_H

//  PKT: git pkt-line framing (protocol v1/v2)
//
//  pkt-line format: 4 hex-digit length + payload
//  Special packets: 0000 (flush), 0001 (delim), 0002 (response-end)
//
//  PKTu8sDrain: reads one pkt-line from input, returns payload.
//  PKTu8bFeed:  writes one pkt-line to buffer.

#include "abc/INT.h"

con ok64 PKTFAIL    = 0x1951d3ca495;
con ok64 PKTBADFMT  = 0x1951d2ca34f59d;
con ok64 PKTFLUSH   = 0x65474f55e711;
con ok64 PKTDELIM   = 0x65474d395496;

// Maximum pkt-line payload (65516 = 65520 - 4)
#define PKT_MAX 65516

//  Drain one pkt-line from `from`, payload into `line`.
//  Returns PKTFLUSH on 0000, PKTDELIM on 0001.
//  Advances `from` past the consumed pkt-line.
ok64 PKTu8sDrain(u8cs from, u8csp line);

//  Feed one pkt-line into buffer: 4-hex-length prefix + payload.
ok64 PKTu8sFeed(u8s into, u8csc payload);

//  Feed a flush packet (0000).
ok64 PKTu8sFeedFlush(u8s into);

#endif
