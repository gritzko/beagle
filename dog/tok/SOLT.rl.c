
/* #line 1 "SOLT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "SOLT.h"

ok64 SOLTonComment (u8cs tok, SOLTstate* state);
ok64 SOLTonString (u8cs tok, SOLTstate* state);
ok64 SOLTonNumber (u8cs tok, SOLTstate* state);
ok64 SOLTonWord (u8cs tok, SOLTstate* state);
ok64 SOLTonPunct (u8cs tok, SOLTstate* state);
ok64 SOLTonSpace (u8cs tok, SOLTstate* state);


/* #line 105 "SOLT.c.rl" */



/* #line 15 "SOLT.rl.c" */
static const char _SOLT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	8, 1, 9, 1, 10, 1, 11, 1, 
	12, 1, 13, 1, 14, 1, 15, 1, 
	16, 1, 17, 1, 18, 1, 19, 1, 
	20, 1, 21, 1, 22, 1, 23, 1, 
	24, 1, 25, 1, 26, 1, 27, 2, 
	2, 3, 2, 2, 4, 2, 2, 5, 
	2, 2, 6, 2, 2, 7
};

static const unsigned char _SOLT_key_offsets[] = {
	0, 0, 2, 16, 22, 28, 34, 40, 
	42, 56, 62, 68, 74, 80, 84, 86, 
	88, 89, 91, 95, 97, 99, 103, 105, 
	107, 113, 140, 143, 144, 152, 154, 156, 
	158, 160, 162, 167, 170, 173, 175, 176, 
	177, 185, 189, 194, 197, 203, 206, 213, 
	215, 217, 219
};

static const unsigned char _SOLT_trans_keys[] = {
	34u, 92u, 34u, 39u, 48u, 92u, 110u, 114u, 
	117u, 120u, 97u, 98u, 101u, 102u, 116u, 118u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	39u, 92u, 34u, 39u, 48u, 92u, 110u, 114u, 
	117u, 120u, 97u, 98u, 101u, 102u, 116u, 118u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	43u, 45u, 48u, 57u, 48u, 57u, 48u, 57u, 
	42u, 42u, 47u, 43u, 45u, 48u, 57u, 48u, 
	57u, 48u, 57u, 43u, 45u, 48u, 57u, 48u, 
	57u, 48u, 57u, 48u, 57u, 65u, 70u, 97u, 
	102u, 32u, 33u, 34u, 36u, 37u, 38u, 39u, 
	42u, 43u, 45u, 46u, 47u, 48u, 60u, 61u, 
	62u, 94u, 95u, 124u, 9u, 13u, 49u, 57u, 
	65u, 90u, 97u, 122u, 32u, 9u, 13u, 61u, 
	36u, 95u, 48u, 57u, 65u, 90u, 97u, 122u, 
	38u, 61u, 42u, 61u, 43u, 61u, 45u, 61u, 
	48u, 57u, 69u, 95u, 101u, 48u, 57u, 95u, 
	48u, 57u, 42u, 47u, 61u, 10u, 47u, 10u, 
	10u, 46u, 69u, 88u, 95u, 101u, 120u, 48u, 
	57u, 69u, 101u, 48u, 57u, 69u, 95u, 101u, 
	48u, 57u, 95u, 48u, 57u, 46u, 69u, 95u, 
	101u, 48u, 57u, 95u, 48u, 57u, 95u, 48u, 
	57u, 65u, 70u, 97u, 102u, 60u, 61u, 61u, 
	62u, 61u, 62u, 61u, 124u, 0
};

static const char _SOLT_single_lengths[] = {
	0, 2, 8, 0, 0, 0, 0, 2, 
	8, 0, 0, 0, 0, 2, 0, 0, 
	1, 2, 2, 0, 0, 2, 0, 0, 
	0, 19, 1, 1, 2, 2, 2, 2, 
	2, 0, 3, 1, 3, 2, 1, 1, 
	6, 2, 3, 1, 4, 1, 1, 2, 
	0, 2, 2
};

static const char _SOLT_range_lengths[] = {
	0, 0, 3, 3, 3, 3, 3, 0, 
	3, 3, 3, 3, 3, 1, 1, 1, 
	0, 0, 1, 1, 1, 1, 1, 1, 
	3, 4, 1, 0, 3, 0, 0, 0, 
	0, 1, 1, 1, 0, 0, 0, 0, 
	1, 1, 1, 1, 1, 1, 3, 0, 
	1, 0, 0
};

static const short _SOLT_index_offsets[] = {
	0, 0, 3, 15, 19, 23, 27, 31, 
	34, 46, 50, 54, 58, 62, 66, 68, 
	70, 72, 75, 79, 81, 83, 87, 89, 
	91, 95, 119, 122, 124, 130, 133, 136, 
	139, 142, 144, 149, 152, 156, 159, 161, 
	163, 171, 175, 180, 183, 189, 192, 197, 
	200, 202, 205
};

static const char _SOLT_indicies[] = {
	1, 2, 0, 0, 0, 0, 0, 0, 
	0, 4, 5, 0, 0, 0, 3, 6, 
	6, 6, 3, 5, 5, 5, 3, 7, 
	7, 7, 3, 0, 0, 0, 3, 9, 
	10, 8, 8, 8, 8, 8, 8, 8, 
	11, 12, 8, 8, 8, 3, 13, 13, 
	13, 3, 12, 12, 12, 3, 14, 14, 
	14, 3, 8, 8, 8, 3, 16, 16, 
	17, 15, 17, 15, 18, 15, 21, 20, 
	21, 22, 20, 24, 24, 25, 23, 25, 
	23, 26, 23, 28, 28, 29, 27, 29, 
	30, 31, 27, 32, 32, 32, 30, 34, 
	35, 0, 36, 35, 37, 8, 38, 39, 
	40, 41, 42, 43, 44, 45, 46, 35, 
	36, 47, 34, 31, 36, 36, 33, 34, 
	34, 48, 49, 30, 36, 36, 36, 36, 
	36, 50, 49, 49, 51, 49, 49, 51, 
	49, 49, 51, 49, 49, 51, 18, 52, 
	54, 55, 54, 18, 53, 16, 17, 53, 
	20, 56, 49, 51, 57, 59, 58, 57, 
	58, 57, 59, 61, 62, 63, 64, 62, 
	63, 31, 60, 66, 66, 26, 65, 66, 
	67, 66, 26, 65, 24, 25, 65, 61, 
	62, 64, 62, 31, 60, 28, 29, 68, 
	63, 32, 32, 32, 69, 70, 49, 51, 
	49, 51, 49, 70, 51, 49, 49, 51, 
	0
};

static const char _SOLT_trans_targs[] = {
	1, 25, 2, 0, 3, 5, 4, 6, 
	7, 25, 8, 9, 11, 10, 12, 25, 
	14, 35, 34, 25, 16, 17, 25, 25, 
	19, 43, 42, 25, 22, 45, 25, 44, 
	46, 25, 26, 27, 28, 29, 30, 31, 
	32, 33, 36, 40, 47, 48, 49, 50, 
	25, 25, 25, 25, 25, 25, 13, 15, 
	37, 25, 38, 39, 25, 41, 21, 24, 
	23, 25, 18, 20, 25, 25, 27
};

static const char _SOLT_trans_actions[] = {
	0, 9, 0, 0, 0, 0, 0, 0, 
	0, 11, 0, 0, 0, 0, 0, 39, 
	0, 5, 5, 43, 0, 0, 7, 37, 
	0, 5, 5, 41, 0, 50, 45, 53, 
	47, 15, 0, 59, 0, 0, 0, 0, 
	0, 0, 5, 53, 0, 0, 0, 0, 
	35, 13, 29, 31, 33, 23, 0, 0, 
	0, 17, 0, 0, 27, 5, 0, 0, 
	0, 21, 0, 0, 25, 19, 56
};

static const char _SOLT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const char _SOLT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 3, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const short _SOLT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 16, 16, 16, 
	20, 20, 24, 24, 24, 28, 31, 28, 
	31, 0, 49, 31, 51, 52, 52, 52, 
	52, 53, 54, 54, 52, 58, 58, 58, 
	61, 66, 66, 66, 61, 69, 70, 52, 
	52, 52, 52
};

static const int SOLT_start = 25;
static const int SOLT_first_final = 25;
static const int SOLT_error = 0;

static const int SOLT_en_main = 25;


/* #line 108 "SOLT.c.rl" */

ok64 SOLTLexer(SOLTstate* state) {

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

    
/* #line 204 "SOLT.rl.c" */
	{
	cs = SOLT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 126 "SOLT.c.rl" */
    
/* #line 210 "SOLT.rl.c" */
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
	_acts = _SOLT_actions + _SOLT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 229 "SOLT.rl.c" */
		}
	}

	_keys = _SOLT_trans_keys + _SOLT_key_offsets[cs];
	_trans = _SOLT_index_offsets[cs];

	_klen = _SOLT_single_lengths[cs];
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

	_klen = _SOLT_range_lengths[cs];
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
	_trans = _SOLT_indicies[_trans];
_eof_trans:
	cs = _SOLT_trans_targs[_trans];

	if ( _SOLT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _SOLT_actions + _SOLT_trans_actions[_trans];
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
/* #line 41 "SOLT.c.rl" */
	{act = 6;}
	break;
	case 4:
/* #line 41 "SOLT.c.rl" */
	{act = 9;}
	break;
	case 5:
/* #line 41 "SOLT.c.rl" */
	{act = 10;}
	break;
	case 6:
/* #line 53 "SOLT.c.rl" */
	{act = 12;}
	break;
	case 7:
/* #line 53 "SOLT.c.rl" */
	{act = 13;}
	break;
	case 8:
/* #line 29 "SOLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 35 "SOLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 35 "SOLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 53 "SOLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 53 "SOLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 29 "SOLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 41 "SOLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 41 "SOLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 41 "SOLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 41 "SOLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 41 "SOLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 47 "SOLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 53 "SOLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 53 "SOLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 59 "SOLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 41 "SOLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 41 "SOLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 41 "SOLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 53 "SOLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 6:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 9:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 10:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 13:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 509 "SOLT.rl.c" */
		}
	}

_again:
	_acts = _SOLT_actions + _SOLT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 520 "SOLT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _SOLT_eof_trans[cs] > 0 ) {
		_trans = _SOLT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 127 "SOLT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < SOLT_first_final)
        o = SOLTBAD;

    return o;
}
