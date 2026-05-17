
/* #line 1 "MLT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "MLT.h"

ok64 MLTonComment (u8cs tok, MLTstate* state);
ok64 MLTonString (u8cs tok, MLTstate* state);
ok64 MLTonNumber (u8cs tok, MLTstate* state);
ok64 MLTonWord (u8cs tok, MLTstate* state);
ok64 MLTonPunct (u8cs tok, MLTstate* state);
ok64 MLTonSpace (u8cs tok, MLTstate* state);


/* #line 103 "MLT.c.rl" */



/* #line 15 "MLT.rl.c" */
static const char _MLT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	8, 1, 9, 1, 10, 1, 11, 1, 
	12, 1, 13, 1, 14, 1, 15, 1, 
	16, 1, 17, 1, 18, 1, 19, 1, 
	20, 1, 21, 1, 22, 1, 23, 1, 
	24, 1, 25, 2, 2, 3, 2, 2, 
	4, 2, 2, 5, 2, 2, 6, 2, 
	2, 7
};

static const unsigned char _MLT_key_offsets[] = {
	0, 0, 2, 17, 19, 21, 27, 33, 
	35, 36, 51, 54, 56, 58, 64, 70, 
	71, 72, 76, 78, 80, 84, 86, 88, 
	90, 92, 98, 127, 130, 143, 144, 156, 
	160, 165, 168, 174, 177, 180, 183, 190
};

static const unsigned char _MLT_trans_keys[] = {
	34u, 92u, 10u, 34u, 39u, 92u, 110u, 114u, 
	116u, 118u, 120u, 48u, 57u, 97u, 98u, 101u, 
	102u, 48u, 57u, 48u, 57u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 39u, 92u, 39u, 10u, 34u, 39u, 92u, 
	110u, 114u, 116u, 118u, 120u, 48u, 57u, 97u, 
	98u, 101u, 102u, 9u, 32u, 39u, 48u, 57u, 
	48u, 57u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 42u, 41u, 
	43u, 45u, 48u, 57u, 48u, 57u, 48u, 57u, 
	43u, 45u, 48u, 57u, 48u, 57u, 48u, 57u, 
	48u, 49u, 48u, 55u, 48u, 57u, 65u, 70u, 
	97u, 102u, 32u, 34u, 35u, 39u, 40u, 41u, 
	44u, 48u, 59u, 91u, 93u, 94u, 95u, 124u, 
	126u, 9u, 13u, 33u, 47u, 49u, 57u, 58u, 
	64u, 65u, 90u, 97u, 122u, 123u, 125u, 32u, 
	9u, 13u, 33u, 58u, 94u, 124u, 126u, 36u, 
	38u, 42u, 43u, 45u, 47u, 60u, 64u, 42u, 
	46u, 66u, 69u, 79u, 88u, 95u, 98u, 101u, 
	111u, 120u, 48u, 57u, 69u, 101u, 48u, 57u, 
	69u, 95u, 101u, 48u, 57u, 95u, 48u, 57u, 
	46u, 69u, 95u, 101u, 48u, 57u, 95u, 48u, 
	57u, 95u, 48u, 49u, 95u, 48u, 55u, 95u, 
	48u, 57u, 65u, 70u, 97u, 102u, 39u, 95u, 
	48u, 57u, 65u, 90u, 97u, 122u, 0
};

static const char _MLT_single_lengths[] = {
	0, 2, 9, 0, 0, 0, 0, 2, 
	1, 9, 3, 0, 0, 0, 0, 1, 
	1, 2, 0, 0, 2, 0, 0, 0, 
	0, 0, 15, 1, 5, 1, 10, 2, 
	3, 1, 4, 1, 1, 1, 1, 2
};

static const char _MLT_range_lengths[] = {
	0, 0, 3, 1, 1, 3, 3, 0, 
	0, 3, 0, 1, 1, 3, 3, 0, 
	0, 1, 1, 1, 1, 1, 1, 1, 
	1, 3, 7, 1, 4, 0, 1, 1, 
	1, 1, 1, 1, 1, 1, 3, 3
};

static const unsigned char _MLT_index_offsets[] = {
	0, 0, 3, 16, 18, 20, 24, 28, 
	31, 33, 46, 50, 52, 54, 58, 62, 
	64, 66, 70, 72, 74, 78, 80, 82, 
	84, 86, 90, 113, 116, 126, 128, 140, 
	144, 149, 152, 158, 161, 164, 167, 172
};

static const char _MLT_indicies[] = {
	1, 2, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 5, 4, 0, 0, 3, 
	6, 3, 0, 3, 7, 7, 7, 3, 
	0, 0, 0, 3, 3, 9, 8, 10, 
	3, 11, 8, 8, 8, 8, 8, 8, 
	8, 13, 12, 8, 8, 3, 11, 11, 
	10, 3, 14, 3, 8, 3, 15, 15, 
	15, 3, 8, 8, 8, 3, 18, 17, 
	19, 17, 21, 21, 22, 20, 22, 20, 
	23, 20, 25, 25, 26, 24, 26, 27, 
	28, 24, 29, 27, 30, 27, 31, 31, 
	31, 27, 32, 0, 34, 35, 36, 34, 
	34, 37, 34, 34, 34, 33, 38, 33, 
	33, 32, 33, 28, 33, 38, 38, 34, 
	3, 32, 32, 39, 33, 33, 33, 33, 
	33, 33, 33, 33, 33, 40, 17, 41, 
	43, 44, 45, 46, 47, 48, 44, 45, 
	46, 47, 28, 42, 50, 50, 23, 49, 
	50, 51, 50, 23, 49, 21, 22, 49, 
	43, 45, 48, 45, 28, 42, 25, 26, 
	52, 44, 29, 53, 46, 30, 54, 47, 
	31, 31, 31, 55, 38, 38, 38, 38, 
	38, 56, 0
};

static const char _MLT_trans_targs[] = {
	1, 26, 2, 0, 3, 5, 4, 6, 
	8, 9, 26, 10, 11, 13, 12, 14, 
	26, 15, 16, 26, 26, 18, 33, 32, 
	26, 21, 35, 26, 34, 36, 37, 38, 
	27, 28, 26, 7, 29, 30, 39, 26, 
	26, 26, 26, 31, 23, 20, 24, 25, 
	22, 26, 17, 19, 26, 26, 26, 26, 
	26
};

static const char _MLT_trans_actions[] = {
	0, 9, 0, 0, 0, 0, 0, 0, 
	0, 0, 11, 0, 0, 0, 0, 0, 
	39, 0, 0, 7, 35, 0, 5, 5, 
	37, 0, 52, 41, 55, 49, 46, 43, 
	0, 0, 13, 0, 5, 55, 0, 33, 
	29, 31, 25, 5, 0, 0, 0, 0, 
	0, 21, 0, 0, 23, 19, 17, 15, 
	27
};

static const char _MLT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 1, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0
};

static const char _MLT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 3, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char _MLT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 17, 
	17, 21, 21, 21, 25, 28, 25, 28, 
	28, 28, 0, 40, 41, 42, 43, 50, 
	50, 50, 43, 53, 54, 55, 56, 57
};

static const int MLT_start = 26;
static const int MLT_first_final = 26;
static const int MLT_error = 0;

static const int MLT_en_main = 26;


/* #line 106 "MLT.c.rl" */

ok64 MLTLexer(MLTstate* state) {

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

    
/* #line 181 "MLT.rl.c" */
	{
	cs = MLT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 124 "MLT.c.rl" */
    
/* #line 187 "MLT.rl.c" */
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
	_acts = _MLT_actions + _MLT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 206 "MLT.rl.c" */
		}
	}

	_keys = _MLT_trans_keys + _MLT_key_offsets[cs];
	_trans = _MLT_index_offsets[cs];

	_klen = _MLT_single_lengths[cs];
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

	_klen = _MLT_range_lengths[cs];
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
	_trans = _MLT_indicies[_trans];
_eof_trans:
	cs = _MLT_trans_targs[_trans];

	if ( _MLT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _MLT_actions + _MLT_trans_actions[_trans];
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
/* #line 43 "MLT.c.rl" */
	{act = 4;}
	break;
	case 4:
/* #line 43 "MLT.c.rl" */
	{act = 5;}
	break;
	case 5:
/* #line 43 "MLT.c.rl" */
	{act = 6;}
	break;
	case 6:
/* #line 43 "MLT.c.rl" */
	{act = 8;}
	break;
	case 7:
/* #line 43 "MLT.c.rl" */
	{act = 9;}
	break;
	case 8:
/* #line 31 "MLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 37 "MLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 37 "MLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 55 "MLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 43 "MLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 43 "MLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 43 "MLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 43 "MLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 43 "MLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 43 "MLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 49 "MLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 55 "MLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 55 "MLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 61 "MLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 43 "MLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 43 "MLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 55 "MLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 4:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 5:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 6:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 8:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 9:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 470 "MLT.rl.c" */
		}
	}

_again:
	_acts = _MLT_actions + _MLT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 481 "MLT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _MLT_eof_trans[cs] > 0 ) {
		_trans = _MLT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 125 "MLT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < MLT_first_final)
        o = MLTBAD;

    return o;
}
