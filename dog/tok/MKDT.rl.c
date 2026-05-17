
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


/* #line 117 "MKDT.c.rl" */



/* #line 16 "MKDT.rl.c" */
static const char _MKDT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19, 1, 20, 1, 21, 1, 22, 1, 
	23, 1, 24, 1, 25, 1, 26, 1, 
	27, 2, 2, 3, 2, 2, 4, 2, 
	2, 5, 2, 2, 6
};

static const unsigned char _MKDT_key_offsets[] = {
	0, 0, 2, 4, 5, 11, 12, 14, 
	20, 22, 23, 29, 30, 32, 34, 36, 
	38, 74, 78, 79, 83, 85, 90, 92, 
	95, 101, 108, 110, 120, 128, 130, 131, 
	135
};

static const unsigned char _MKDT_trans_keys[] = {
	10u, 93u, 10u, 93u, 91u, 48u, 57u, 65u, 
	90u, 97u, 122u, 93u, 10u, 42u, 48u, 57u, 
	65u, 70u, 97u, 102u, 10u, 93u, 91u, 48u, 
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
	65u, 90u, 97u, 122u, 10u, 93u, 32u, 95u, 
	9u, 13u, 48u, 57u, 65u, 90u, 97u, 122u, 
	10u, 95u, 48u, 57u, 65u, 90u, 97u, 122u, 
	10u, 96u, 126u, 32u, 126u, 9u, 13u, 128u, 
	191u, 0
};

static const char _MKDT_single_lengths[] = {
	0, 2, 2, 1, 0, 1, 2, 0, 
	2, 1, 0, 1, 2, 2, 2, 2, 
	12, 2, 1, 2, 0, 3, 0, 1, 
	0, 1, 2, 2, 2, 2, 1, 2, 
	0
};

static const char _MKDT_range_lengths[] = {
	0, 0, 0, 0, 3, 0, 0, 3, 
	0, 0, 3, 0, 0, 0, 0, 0, 
	12, 1, 0, 1, 1, 1, 1, 1, 
	3, 3, 0, 4, 3, 0, 0, 1, 
	1
};

static const unsigned char _MKDT_index_offsets[] = {
	0, 0, 3, 6, 8, 12, 14, 17, 
	21, 24, 26, 30, 32, 35, 38, 41, 
	44, 69, 73, 75, 79, 81, 86, 88, 
	91, 95, 100, 103, 110, 116, 119, 121, 
	125
};

static const char _MKDT_indicies[] = {
	0, 0, 1, 0, 2, 1, 3, 0, 
	4, 4, 4, 0, 5, 0, 0, 7, 
	6, 9, 9, 9, 8, 0, 11, 10, 
	12, 0, 13, 13, 13, 0, 14, 0, 
	15, 17, 16, 0, 19, 18, 20, 22, 
	21, 20, 23, 21, 26, 24, 27, 29, 
	30, 31, 25, 34, 35, 36, 37, 25, 
	25, 24, 25, 28, 25, 28, 32, 28, 
	33, 28, 33, 28, 38, 24, 24, 24, 
	39, 41, 40, 40, 40, 40, 6, 43, 
	42, 45, 46, 46, 32, 44, 45, 47, 
	45, 32, 44, 9, 9, 9, 48, 33, 
	33, 33, 33, 42, 40, 40, 10, 49, 
	33, 49, 50, 50, 50, 16, 49, 51, 
	50, 50, 50, 16, 40, 40, 18, 52, 
	40, 53, 53, 53, 21, 38, 54, 0
};

static const char _MKDT_trans_targs[] = {
	16, 2, 3, 4, 5, 16, 6, 16, 
	16, 24, 8, 9, 10, 11, 16, 16, 
	12, 16, 13, 16, 16, 14, 15, 16, 
	17, 0, 16, 18, 16, 19, 20, 21, 
	23, 25, 26, 27, 29, 30, 32, 16, 
	16, 1, 16, 20, 16, 22, 7, 16, 
	16, 16, 28, 25, 31, 16, 16
};

static const char _MKDT_trans_actions[] = {
	45, 0, 0, 0, 0, 17, 0, 9, 
	39, 0, 0, 0, 0, 0, 15, 43, 
	0, 11, 0, 7, 41, 0, 0, 13, 
	0, 0, 21, 5, 19, 5, 58, 5, 
	0, 55, 5, 5, 5, 0, 0, 35, 
	33, 0, 47, 52, 27, 0, 0, 25, 
	23, 31, 5, 49, 5, 29, 37
};

static const char _MKDT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	1, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const char _MKDT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	3, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const unsigned char _MKDT_eof_trans[] = {
	0, 1, 1, 1, 1, 1, 1, 9, 
	1, 1, 1, 1, 16, 1, 21, 21, 
	0, 40, 41, 41, 43, 45, 48, 45, 
	49, 43, 41, 50, 50, 41, 41, 54, 
	55
};

static const int MKDT_start = 16;
static const int MKDT_first_final = 16;
static const int MKDT_error = 0;

static const int MKDT_en_main = 16;


/* #line 120 "MKDT.c.rl" */

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

    
/* #line 166 "MKDT.rl.c" */
	{
	cs = MKDT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 138 "MKDT.c.rl" */
    
/* #line 172 "MKDT.rl.c" */
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
	_acts = _MKDT_actions + _MKDT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 191 "MKDT.rl.c" */
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
	{act = 9;}
	break;
	case 5:
/* #line 52 "MKDT.c.rl" */
	{act = 12;}
	break;
	case 6:
/* #line 58 "MKDT.c.rl" */
	{act = 13;}
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
/* #line 58 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 64 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 46 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 46 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 46 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 58 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 52 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 58 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 64 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonSpace(tok, state);
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
/* #line 46 "MKDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 58 "MKDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 52 "MKDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 58 "MKDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
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
	case 9:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 13:
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
/* #line 468 "MKDT.rl.c" */
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
/* #line 479 "MKDT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
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

/* #line 139 "MKDT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < MKDT_first_final)
        o = MKDTBAD;

    return o;
}
