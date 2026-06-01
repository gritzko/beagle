
/* #line 1 "MKDT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "MKDT.h"

ok64 MKDTonEmph (u8cs tok, MKDTstate* state);
ok64 MKDTonCode (u8cs tok, MKDTstate* state);
ok64 MKDTonLink (u8cs tok, MKDTstate* state);
ok64 MKDTonNumber (u8cs tok, MKDTstate* state);
ok64 MKDTonWord (u8cs tok, MKDTstate* state);
ok64 MKDTonPunct (u8cs tok, MKDTstate* state);
ok64 MKDTonSpace (u8cs tok, MKDTstate* state);


/* #line 126 "MKDT.c.rl" */



/* #line 16 "MKDT.rl.c" */
static const char _MKDT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19, 1, 20, 1, 21, 1, 22, 1, 
	23, 1, 24, 1, 25, 1, 26, 1, 
	27, 1, 28, 1, 29, 1, 30, 1, 
	31, 2, 2, 3, 2, 2, 4, 2, 
	2, 5, 2, 2, 6
};

static const unsigned char _MKDT_key_offsets[] = {
	0, 2, 4, 5, 11, 12, 14, 20, 
	22, 29, 30, 32, 34, 36, 38, 74, 
	78, 79, 83, 85, 90, 92, 95, 101, 
	108, 110, 111, 121, 129, 131, 132, 136
};

static const unsigned char _MKDT_trans_keys[] = {
	10u, 93u, 10u, 93u, 91u, 48u, 57u, 65u, 
	90u, 97u, 122u, 93u, 10u, 42u, 48u, 57u, 
	65u, 70u, 97u, 102u, 10u, 93u, 93u, 48u, 
	57u, 65u, 90u, 97u, 122u, 93u, 10u, 95u, 
	10u, 96u, 10u, 126u, 10u, 126u, 10u, 32u, 
	33u, 42u, 46u, 48u, 63u, 91u, 95u, 96u, 
	126u, 127u, 0u, 8u, 9u, 13u, 14u, 31u, 
	34u, 35u, 36u, 37u, 38u, 47u, 49u, 57u, 
	58u, 64u, 65u, 90u, 92u, 94u, 97u, 122u, 
	123u, 125u, 9u, 32u, 11u, 13u, 91u, 32u, 
	42u, 9u, 13u, 48u, 57u, 46u, 88u, 120u, 
	48u, 57u, 48u, 57u, 46u, 48u, 57u, 48u, 
	57u, 65u, 70u, 97u, 102u, 95u, 48u, 57u, 
	65u, 90u, 97u, 122u, 10u, 93u, 91u, 32u, 
	95u, 9u, 13u, 48u, 57u, 65u, 90u, 97u, 
	122u, 10u, 95u, 48u, 57u, 65u, 90u, 97u, 
	122u, 10u, 96u, 126u, 32u, 126u, 9u, 13u, 
	128u, 191u, 0
};

static const char _MKDT_single_lengths[] = {
	2, 2, 1, 0, 1, 2, 0, 2, 
	1, 1, 2, 2, 2, 2, 12, 2, 
	1, 2, 0, 3, 0, 1, 0, 1, 
	2, 1, 2, 2, 2, 1, 2, 0
};

static const char _MKDT_range_lengths[] = {
	0, 0, 0, 3, 0, 0, 3, 0, 
	3, 0, 0, 0, 0, 0, 12, 1, 
	0, 1, 1, 1, 1, 1, 3, 3, 
	0, 0, 4, 3, 0, 0, 1, 1
};

static const unsigned char _MKDT_index_offsets[] = {
	0, 3, 6, 8, 12, 14, 17, 21, 
	24, 29, 31, 34, 37, 40, 43, 68, 
	72, 74, 78, 80, 85, 87, 90, 94, 
	99, 102, 104, 111, 117, 120, 122, 126
};

static const char _MKDT_indicies[] = {
	0, 0, 1, 0, 2, 1, 3, 0, 
	4, 4, 4, 0, 5, 0, 0, 7, 
	6, 9, 9, 9, 8, 0, 11, 10, 
	14, 13, 13, 13, 12, 15, 12, 16, 
	18, 17, 0, 20, 19, 21, 23, 22, 
	21, 24, 22, 27, 26, 28, 30, 31, 
	32, 25, 35, 36, 37, 38, 25, 25, 
	26, 25, 29, 25, 29, 33, 29, 34, 
	29, 34, 29, 39, 26, 26, 26, 40, 
	42, 41, 41, 41, 41, 6, 44, 43, 
	46, 47, 47, 33, 45, 46, 48, 46, 
	33, 45, 9, 9, 9, 49, 34, 34, 
	34, 34, 43, 41, 41, 10, 51, 50, 
	52, 34, 52, 53, 53, 53, 17, 52, 
	54, 53, 53, 53, 17, 41, 41, 19, 
	55, 41, 56, 56, 56, 22, 39, 57, 
	0
};

static const char _MKDT_trans_targs[] = {
	14, 1, 2, 3, 4, 14, 5, 14, 
	14, 22, 7, 25, 14, 9, 14, 14, 
	14, 10, 14, 11, 14, 14, 12, 13, 
	14, 14, 15, 14, 16, 14, 17, 18, 
	19, 21, 23, 24, 26, 28, 29, 31, 
	14, 14, 0, 14, 18, 14, 20, 6, 
	14, 14, 14, 8, 14, 27, 23, 30, 
	14, 14
};

static const char _MKDT_trans_actions[] = {
	53, 0, 0, 0, 0, 17, 0, 9, 
	47, 0, 0, 5, 45, 0, 19, 15, 
	51, 0, 11, 0, 7, 49, 0, 0, 
	13, 25, 0, 23, 5, 21, 5, 66, 
	5, 0, 63, 5, 5, 5, 0, 0, 
	41, 39, 0, 55, 60, 33, 0, 0, 
	31, 29, 27, 0, 37, 5, 57, 5, 
	35, 43
};

static const char _MKDT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 1, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0
};

static const char _MKDT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 3, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char _MKDT_eof_trans[] = {
	1, 1, 1, 1, 1, 1, 9, 1, 
	13, 13, 17, 1, 22, 22, 0, 41, 
	42, 42, 44, 46, 49, 46, 50, 44, 
	42, 51, 53, 53, 42, 42, 57, 58
};

static const int MKDT_start = 14;
static const int MKDT_first_final = 14;
static const int MKDT_error = -1;

static const int MKDT_en_main = 14;


/* #line 129 "MKDT.c.rl" */

ok64 MKDTInlineLexer(MKDTstate* state) {

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

    
/* #line 163 "MKDT.rl.c" */
	{
	cs = MKDT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 147 "MKDT.c.rl" */
    
/* #line 169 "MKDT.rl.c" */
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( p == pe )
		goto _test_eof;
_resume:
	_acts = _MKDT_actions + _MKDT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 186 "MKDT.rl.c" */
		}
	}

	_keys = _MKDT_trans_keys + _MKDT_key_offsets[cs];
	_trans = _MKDT_index_offsets[cs];

	_klen = _MKDT_single_lengths[cs];
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

	_klen = _MKDT_range_lengths[cs];
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
	_trans = _MKDT_indicies[_trans];
_eof_trans:
	cs = _MKDT_trans_targs[_trans];

	if ( _MKDT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _MKDT_actions + _MKDT_trans_actions[_trans];
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
/* #line 28 "MKDT.c.rl" */
	{act = 3;}
	break;
	case 4:
/* #line 46 "MKDT.c.rl" */
	{act = 11;}
	break;
	case 5:
/* #line 52 "MKDT.c.rl" */
	{act = 14;}
	break;
	case 6:
/* #line 58 "MKDT.c.rl" */
	{act = 15;}
	break;
	case 7:
/* #line 34 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonCode(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 28 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonEmph(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 28 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonEmph(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 28 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonEmph(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 40 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonLink(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 40 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonLink(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 40 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonLink(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 58 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 64 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 58 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 40 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonLink(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 46 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 46 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 46 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 58 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 52 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 58 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 64 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 52 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 40 "MKDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonLink(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 46 "MKDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 58 "MKDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 52 "MKDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 58 "MKDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 3:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonEmph(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 11:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 14:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 15:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 495 "MKDT.rl.c" */
		}
	}

_again:
	_acts = _MKDT_actions + _MKDT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 506 "MKDT.rl.c" */
		}
	}

	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _MKDT_eof_trans[cs] > 0 ) {
		_trans = _MKDT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 148 "MKDT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < MKDT_first_final)
        o = MKDTBAD;

    return o;
}
