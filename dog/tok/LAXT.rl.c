
/* #line 1 "LAXT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "LAXT.h"

ok64 LAXTonComment (u8cs tok, LAXTstate* state);
ok64 LAXTonMath (u8cs tok, LAXTstate* state);
ok64 LAXTonCommand (u8cs tok, LAXTstate* state);
ok64 LAXTonNumber (u8cs tok, LAXTstate* state);
ok64 LAXTonWord (u8cs tok, LAXTstate* state);
ok64 LAXTonPunct (u8cs tok, LAXTstate* state);
ok64 LAXTonSpace (u8cs tok, LAXTstate* state);


/* #line 105 "LAXT.c.rl" */



/* #line 16 "LAXT.rl.c" */
static const char _LAXT_actions[] = {
	0, 1, 0, 1, 1, 1, 5, 1, 
	6, 1, 7, 1, 8, 1, 9, 1, 
	10, 1, 11, 1, 12, 1, 13, 1, 
	14, 1, 15, 1, 16, 2, 2, 3, 
	2, 2, 4
};

static const char _LAXT_key_offsets[] = {
	0, 1, 2, 3, 4, 8, 27, 30, 
	31, 33, 36, 38, 44
};

static const unsigned char _LAXT_trans_keys[] = {
	36u, 36u, 36u, 36u, 65u, 90u, 97u, 122u, 
	32u, 36u, 37u, 38u, 46u, 92u, 123u, 9u, 
	13u, 48u, 57u, 65u, 90u, 91u, 95u, 97u, 
	122u, 125u, 126u, 32u, 9u, 13u, 10u, 48u, 
	57u, 46u, 48u, 57u, 48u, 57u, 48u, 57u, 
	65u, 90u, 97u, 122u, 65u, 90u, 97u, 122u, 
	0
};

static const char _LAXT_single_lengths[] = {
	1, 1, 1, 1, 0, 7, 1, 1, 
	0, 1, 0, 0, 0
};

static const char _LAXT_range_lengths[] = {
	0, 0, 0, 0, 2, 6, 1, 0, 
	1, 1, 1, 3, 2
};

static const char _LAXT_index_offsets[] = {
	0, 2, 4, 6, 8, 11, 25, 28, 
	30, 32, 35, 37, 41
};

static const char _LAXT_trans_targs[] = {
	2, 1, 5, 1, 3, 2, 5, 2, 
	12, 12, 5, 6, 0, 7, 5, 8, 
	4, 5, 6, 9, 11, 5, 11, 5, 
	5, 6, 6, 5, 5, 7, 8, 5, 
	10, 9, 5, 10, 5, 11, 11, 11, 
	5, 12, 12, 5, 5, 5, 5, 5, 
	5, 5, 5, 0
};

static const char _LAXT_trans_actions[] = {
	0, 0, 7, 0, 0, 0, 5, 0, 
	0, 0, 9, 0, 0, 0, 11, 32, 
	0, 11, 0, 0, 0, 11, 0, 11, 
	13, 0, 0, 25, 15, 0, 29, 27, 
	0, 0, 21, 0, 19, 0, 0, 0, 
	23, 0, 0, 17, 25, 15, 27, 21, 
	19, 23, 17, 0
};

static const char _LAXT_to_state_actions[] = {
	0, 0, 0, 0, 0, 1, 0, 0, 
	0, 0, 0, 0, 0
};

static const char _LAXT_from_state_actions[] = {
	0, 0, 0, 0, 0, 3, 0, 0, 
	0, 0, 0, 0, 0
};

static const char _LAXT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 45, 46, 
	47, 48, 49, 50, 51
};

static const int LAXT_start = 5;
static const int LAXT_first_final = 5;
static const int LAXT_error = -1;

static const int LAXT_en_main = 5;


/* #line 108 "LAXT.c.rl" */

ok64 LAXTLexer(LAXTstate* state) {

    a_dup(u8c, data, state->data);
    sane($ok(data));

    int cs = 0;
    int act = 0;
    u8c *p = (u8c*) data[0];
    u8c *pe = (u8c*) data[1];
    u8c *eof = pe;
    u8c *ts = NULL;
    u8c *te = NULL;
    ok64 o = OK;

    u8cs tok = {p, p};

    
/* #line 112 "LAXT.rl.c" */
	{
	cs = LAXT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 126 "LAXT.c.rl" */
    
/* #line 118 "LAXT.rl.c" */
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( p == pe )
		goto _test_eof;
_resume:
	_acts = _LAXT_actions + _LAXT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 135 "LAXT.rl.c" */
		}
	}

	_keys = _LAXT_trans_keys + _LAXT_key_offsets[cs];
	_trans = _LAXT_index_offsets[cs];

	_klen = _LAXT_single_lengths[cs];
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

	_klen = _LAXT_range_lengths[cs];
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
_eof_trans:
	cs = _LAXT_trans_targs[_trans];

	if ( _LAXT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _LAXT_actions + _LAXT_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 2:
/* #line 1 "NONE" */
	{te = p+1;}
	break;
	case 3:
/* #line 43 "LAXT.c.rl" */
	{act = 7;}
	break;
	case 4:
/* #line 55 "LAXT.c.rl" */
	{act = 12;}
	break;
	case 5:
/* #line 31 "LAXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonMath(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 31 "LAXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonMath(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 55 "LAXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 55 "LAXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 55 "LAXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 25 "LAXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 37 "LAXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonCommand(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 43 "LAXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 43 "LAXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 49 "LAXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 61 "LAXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 7:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 317 "LAXT.rl.c" */
		}
	}

_again:
	_acts = _LAXT_actions + _LAXT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 328 "LAXT.rl.c" */
		}
	}

	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _LAXT_eof_trans[cs] > 0 ) {
		_trans = _LAXT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 127 "LAXT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < LAXT_first_final)
        o = LAXTBAD;

    return o;
}
