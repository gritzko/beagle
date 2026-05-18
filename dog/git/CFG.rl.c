
/* #line 1 "CFG.c.rl" */
//  CFG — gitconfig-family structural parser.  See CFG.h.
//
//  Pull-mode ragel grammar.  Each `CFGu8sFeed` call advances the input
//  to the next assignment OR section header, then fbreaks.  Decoded
//  bytes accumulate in the caller-owned scratch buffer:
//
//      PAST          DATA          IDLE
//      [ sec  sub ] [ key  value ] [ ... ]
//
//  sec/sub live in PAST and stick across assigns under the same
//  section.  key/value live in DATA and get recycled (Back) on every
//  fresh assign.  On section emit, DATA→PAST (Bate) so the just-decoded
//  sec/sub become sticky.
//
//  Escape decoding (`\n` `\t` `\b` `\"` `\\`) and backslash-newline
//  value continuation are performed inline; the caller sees decoded
//  bytes via state->{sec,sub,key,value} slices into `buf`.

#include "CFG.h"

#include "abc/B.h"
#include "abc/PRO.h"

static u8 CFGdecEsc (u8 c) {
    switch (c) {
        case 'n':  return '\n';
        case 't':  return '\t';
        case 'b':  return '\b';
        case '"':  return '"';
        case '\\': return '\\';
        default:   return c;     // grammar restricts to the set above
    }
}


/* #line 186 "CFG.c.rl" */



/* #line 38 "CFG.rl.c" */
static const char _CFG_actions[] = {
	0, 1, 0, 1, 1, 1, 5, 1, 
	6, 1, 7, 1, 8, 1, 9, 1, 
	10, 1, 14, 1, 15, 1, 16, 1, 
	17, 1, 18, 1, 19, 1, 20, 1, 
	21, 2, 0, 20, 2, 2, 0, 2, 
	3, 4, 2, 6, 0, 2, 7, 0, 
	2, 9, 1, 2, 9, 10, 2, 11, 
	0, 2, 12, 13, 2, 15, 0, 2, 
	16, 0, 2, 18, 0, 2, 18, 19, 
	2, 19, 0, 2, 19, 20, 2, 21, 
	1, 2, 21, 10, 3, 10, 11, 0, 
	3, 18, 19, 0, 3, 19, 0, 20, 
	4, 9, 10, 11, 0, 4, 18, 19, 
	0, 20, 4, 21, 10, 11, 0
};

static const unsigned char _CFG_key_offsets[] = {
	0, 0, 11, 12, 23, 27, 35, 43, 
	54, 66, 71, 74, 77, 81, 85, 91, 
	92, 97, 100, 101, 102, 110, 118, 121, 
	124, 130, 136, 141, 144, 155, 166
};

static const unsigned char _CFG_trans_keys[] = {
	9u, 10u, 13u, 32u, 35u, 59u, 91u, 65u, 
	90u, 97u, 122u, 10u, 9u, 13u, 32u, 45u, 
	61u, 48u, 57u, 65u, 90u, 97u, 122u, 9u, 
	13u, 32u, 61u, 9u, 10u, 13u, 32u, 34u, 
	35u, 59u, 92u, 9u, 10u, 13u, 32u, 34u, 
	35u, 59u, 92u, 9u, 13u, 32u, 45u, 46u, 
	48u, 57u, 65u, 90u, 97u, 122u, 9u, 13u, 
	32u, 93u, 45u, 46u, 48u, 57u, 65u, 90u, 
	97u, 122u, 9u, 13u, 32u, 34u, 93u, 10u, 
	34u, 92u, 10u, 34u, 92u, 9u, 13u, 32u, 
	93u, 9u, 13u, 32u, 93u, 9u, 10u, 13u, 
	32u, 35u, 59u, 10u, 34u, 92u, 98u, 110u, 
	116u, 10u, 34u, 92u, 10u, 10u, 9u, 10u, 
	13u, 32u, 34u, 35u, 59u, 92u, 9u, 10u, 
	13u, 32u, 34u, 35u, 59u, 92u, 10u, 34u, 
	92u, 10u, 34u, 92u, 9u, 10u, 13u, 32u, 
	35u, 59u, 9u, 10u, 13u, 32u, 35u, 59u, 
	34u, 92u, 98u, 110u, 116u, 10u, 34u, 92u, 
	9u, 10u, 13u, 32u, 35u, 59u, 91u, 65u, 
	90u, 97u, 122u, 9u, 10u, 13u, 32u, 35u, 
	59u, 91u, 65u, 90u, 97u, 122u, 9u, 10u, 
	13u, 32u, 35u, 59u, 91u, 65u, 90u, 97u, 
	122u, 0
};

static const char _CFG_single_lengths[] = {
	0, 7, 1, 5, 4, 8, 8, 3, 
	4, 5, 3, 3, 4, 4, 6, 1, 
	5, 3, 1, 1, 8, 8, 3, 3, 
	6, 6, 5, 3, 7, 7, 7
};

static const char _CFG_range_lengths[] = {
	0, 2, 0, 3, 0, 0, 0, 4, 
	4, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 2, 2, 2
};

static const unsigned char _CFG_index_offsets[] = {
	0, 0, 10, 12, 21, 26, 35, 44, 
	52, 61, 67, 71, 75, 80, 85, 92, 
	94, 100, 104, 106, 108, 117, 126, 130, 
	134, 141, 148, 154, 158, 168, 178
};

static const char _CFG_indicies[] = {
	0, 2, 0, 0, 3, 3, 5, 4, 
	4, 1, 2, 3, 6, 6, 6, 7, 
	8, 7, 7, 7, 1, 9, 9, 9, 
	10, 1, 12, 13, 12, 12, 14, 15, 
	15, 16, 11, 18, 19, 18, 18, 1, 
	20, 20, 21, 17, 22, 22, 22, 23, 
	23, 23, 23, 1, 24, 24, 24, 26, 
	25, 25, 25, 25, 1, 27, 27, 27, 
	28, 29, 1, 1, 31, 32, 30, 1, 
	34, 35, 33, 36, 36, 36, 37, 1, 
	38, 38, 38, 29, 1, 29, 39, 29, 
	29, 40, 40, 1, 39, 40, 41, 41, 
	41, 41, 41, 1, 1, 43, 44, 42, 
	13, 15, 45, 1, 47, 48, 47, 47, 
	1, 49, 49, 50, 46, 52, 19, 52, 
	52, 14, 20, 20, 53, 51, 1, 55, 
	56, 54, 1, 58, 59, 57, 60, 61, 
	60, 60, 62, 62, 1, 63, 13, 63, 
	63, 15, 15, 1, 64, 64, 64, 64, 
	64, 1, 1, 66, 67, 65, 68, 2, 
	68, 68, 3, 3, 5, 69, 69, 1, 
	70, 71, 70, 70, 72, 72, 74, 73, 
	73, 1, 75, 76, 75, 75, 77, 77, 
	79, 78, 78, 1, 0
};

static const char _CFG_trans_targs[] = {
	1, 0, 28, 2, 3, 7, 4, 3, 
	5, 4, 5, 6, 21, 29, 22, 18, 
	19, 6, 6, 29, 18, 19, 7, 8, 
	9, 8, 14, 9, 10, 14, 11, 12, 
	16, 11, 12, 16, 13, 14, 13, 30, 
	15, 17, 11, 12, 16, 20, 6, 6, 
	29, 18, 19, 6, 21, 19, 23, 24, 
	26, 23, 24, 26, 25, 29, 18, 25, 
	27, 23, 24, 26, 1, 3, 1, 28, 
	2, 3, 7, 1, 28, 2, 3, 7
};

static const char _CFG_trans_actions[] = {
	0, 0, 0, 0, 54, 3, 57, 0, 
	57, 0, 0, 66, 66, 0, 17, 0, 
	25, 72, 92, 75, 75, 27, 0, 36, 
	39, 0, 39, 0, 5, 0, 1, 0, 
	0, 42, 7, 7, 11, 11, 0, 0, 
	0, 0, 45, 9, 9, 0, 1, 33, 
	29, 29, 0, 88, 101, 69, 1, 0, 
	0, 60, 19, 19, 23, 23, 23, 0, 
	0, 63, 21, 21, 15, 84, 81, 31, 
	31, 106, 78, 51, 13, 13, 96, 48
};

static const char _CFG_eof_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 31, 13
};

static const int CFG_start = 28;
static const int CFG_first_final = 28;
static const int CFG_error = 0;

static const int CFG_en_main = 28;


/* #line 189 "CFG.c.rl" */

ok64 CFGu8sFeed (CFGstate *state) {
    sane(state != NULL && $ok(state->data) && Bok(state->buf));

    int  cs  = 0;
    u8c *p   = (u8c *)state->data[0];
    u8c *pe  = (u8c *)state->data[1];
    u8c *eof = pe;
    u8c const *run_s = NULL;
    ok64 o = OK;

    
/* #line 186 "CFG.rl.c" */
	{
	cs = CFG_start;
	}

/* #line 201 "CFG.c.rl" */
    
/* #line 189 "CFG.rl.c" */
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( p == pe )
		goto _test_eof;
	if ( cs == 0 )
		goto _out;
_resume:
	_keys = _CFG_trans_keys + _CFG_key_offsets[cs];
	_trans = _CFG_index_offsets[cs];

	_klen = _CFG_single_lengths[cs];
	if ( _klen > 0 ) {
		const unsigned char *_lower = _keys;
		const unsigned char *_mid;
		const unsigned char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*p) < *_mid )
				_upper = _mid - 1;
			else if ( (*p) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (unsigned int)(_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _CFG_range_lengths[cs];
	if ( _klen > 0 ) {
		const unsigned char *_lower = _keys;
		const unsigned char *_mid;
		const unsigned char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*p) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*p) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += (unsigned int)((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	_trans = _CFG_indicies[_trans];
	cs = _CFG_trans_targs[_trans];

	if ( _CFG_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _CFG_actions + _CFG_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
/* #line 68 "CFG.c.rl" */
	{ run_s = p; }
	break;
	case 1:
/* #line 71 "CFG.c.rl" */
	{ u8bReset(state->buf); }
	break;
	case 2:
/* #line 72 "CFG.c.rl" */
	{ state->sec[0] = (u8c *)state->buf[2]; }
	break;
	case 3:
/* #line 73 "CFG.c.rl" */
	{
    u8cs r = {run_s, p};
    if (u8bFeed(state->buf, r) != OK) { o = CFGNOBUF; {p++; goto _out; } }
}
	break;
	case 4:
/* #line 77 "CFG.c.rl" */
	{ state->sec[1] = (u8c *)state->buf[2]; }
	break;
	case 5:
/* #line 80 "CFG.c.rl" */
	{ state->sub[0] = (u8c *)state->buf[2]; }
	break;
	case 6:
/* #line 81 "CFG.c.rl" */
	{
    u8cs r = {run_s, p};
    if (u8bFeed(state->buf, r) != OK) { o = CFGNOBUF; {p++; goto _out; } }
}
	break;
	case 7:
/* #line 85 "CFG.c.rl" */
	{
    if (u8bFeed1(state->buf, CFGdecEsc(p[-1])) != OK) {
        o = CFGNOBUF; {p++; goto _out; }
    }
}
	break;
	case 8:
/* #line 90 "CFG.c.rl" */
	{ state->sub[1] = (u8c *)state->buf[2]; }
	break;
	case 9:
/* #line 92 "CFG.c.rl" */
	{
    Bate(state->buf);                                   // DATA → PAST
    state->key[0]   = state->key[1]   = (u8c *)state->buf[2];
    state->value[0] = state->value[1] = (u8c *)state->buf[2];
    {p++; goto _out; }
}
	break;
	case 10:
/* #line 102 "CFG.c.rl" */
	{
    Back(state->buf);
    state->key[0]   = state->key[1]   = (u8c *)state->buf[2];
    state->value[0] = state->value[1] = (u8c *)state->buf[2];
}
	break;
	case 11:
/* #line 108 "CFG.c.rl" */
	{ state->key[0] = (u8c *)state->buf[2]; }
	break;
	case 12:
/* #line 109 "CFG.c.rl" */
	{
    u8cs r = {run_s, p};
    if (u8bFeed(state->buf, r) != OK) { o = CFGNOBUF; {p++; goto _out; } }
}
	break;
	case 13:
/* #line 113 "CFG.c.rl" */
	{ state->key[1] = (u8c *)state->buf[2]; }
	break;
	case 14:
/* #line 116 "CFG.c.rl" */
	{ state->value[0] = (u8c *)state->buf[2]; }
	break;
	case 15:
/* #line 117 "CFG.c.rl" */
	{
    u8cs r = {run_s, p};
    if (u8bFeed(state->buf, r) != OK) { o = CFGNOBUF; {p++; goto _out; } }
}
	break;
	case 16:
/* #line 121 "CFG.c.rl" */
	{
    if (u8bFeed1(state->buf, CFGdecEsc(p[-1])) != OK) {
        o = CFGNOBUF; {p++; goto _out; }
    }
}
	break;
	case 17:
/* #line 126 "CFG.c.rl" */
	{ state->value[1] = (u8c *)state->buf[2]; }
	break;
	case 18:
/* #line 130 "CFG.c.rl" */
	{ state->value[0] = (u8c *)state->buf[2]; }
	break;
	case 19:
/* #line 131 "CFG.c.rl" */
	{
    u8cs r = {run_s, p};
    if (u8bFeed(state->buf, r) != OK) { o = CFGNOBUF; {p++; goto _out; } }
}
	break;
	case 20:
/* #line 135 "CFG.c.rl" */
	{
    u8c **bb = (u8c **)state->buf;
    while (bb[2] > bb[1] &&
           (bb[2][-1] == ' ' || bb[2][-1] == '\t' || bb[2][-1] == '\r'))
        bb[2]--;
    state->value[1] = (u8c *)state->buf[2];
}
	break;
	case 21:
/* #line 143 "CFG.c.rl" */
	{ {p++; goto _out; } }
	break;
/* #line 366 "CFG.rl.c" */
		}
	}

_again:
	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	const char *__acts = _CFG_actions + _CFG_eof_actions[cs];
	unsigned int __nacts = (unsigned int) *__acts++;
	while ( __nacts-- > 0 ) {
		switch ( *__acts++ ) {
	case 9:
/* #line 92 "CFG.c.rl" */
	{
    Bate(state->buf);                                   // DATA → PAST
    state->key[0]   = state->key[1]   = (u8c *)state->buf[2];
    state->value[0] = state->value[1] = (u8c *)state->buf[2];
    {p++; goto _out; }
}
	break;
	case 21:
/* #line 143 "CFG.c.rl" */
	{ {p++; goto _out; } }
	break;
/* #line 392 "CFG.rl.c" */
		}
	}
	}

	_out: {}
	}

/* #line 202 "CFG.c.rl" */

    state->data[0] = p;
    if (o != OK) return o;
    if (p == pe && cs >= CFG_first_final) return NODATA;
    if (cs < CFG_first_final) return CFGBAD;
    return OK;
}
