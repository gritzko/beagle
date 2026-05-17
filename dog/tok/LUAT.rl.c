
/* #line 1 "LUAT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "LUAT.h"

ok64 LUATonComment (u8cs tok, LUATstate* state);
ok64 LUATonString (u8cs tok, LUATstate* state);
ok64 LUATonNumber (u8cs tok, LUATstate* state);
ok64 LUATonWord (u8cs tok, LUATstate* state);
ok64 LUATonPunct (u8cs tok, LUATstate* state);
ok64 LUATonSpace (u8cs tok, LUATstate* state);


/* #line 109 "LUAT.c.rl" */



/* #line 15 "LUAT.rl.c" */
static const char _LUAT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	5, 1, 6, 1, 7, 1, 8, 1, 
	9, 1, 10, 1, 11, 1, 12, 1, 
	13, 1, 14, 1, 15, 1, 16, 1, 
	17, 1, 18, 1, 19, 1, 20, 1, 
	21, 1, 22, 1, 23, 1, 24, 1, 
	25, 1, 26, 1, 27, 1, 28, 2, 
	2, 3, 2, 2, 4
};

static const unsigned char _LUAT_key_offsets[] = {
	0, 0, 2, 18, 19, 25, 32, 38, 
	44, 46, 62, 63, 69, 76, 82, 88, 
	89, 90, 94, 96, 100, 102, 106, 108, 
	114, 118, 120, 121, 122, 144, 147, 149, 
	151, 152, 154, 156, 158, 161, 162, 166, 
	168, 169, 176, 180, 182, 187, 189, 198, 
	206, 208, 209, 211, 212, 214, 221
};

static const unsigned char _LUAT_trans_keys[] = {
	34u, 92u, 10u, 34u, 39u, 92u, 102u, 110u, 
	114u, 117u, 120u, 122u, 48u, 57u, 97u, 98u, 
	116u, 118u, 123u, 48u, 57u, 65u, 70u, 97u, 
	102u, 125u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 39u, 92u, 10u, 34u, 
	39u, 92u, 102u, 110u, 114u, 117u, 120u, 122u, 
	48u, 57u, 97u, 98u, 116u, 118u, 123u, 48u, 
	57u, 65u, 70u, 97u, 102u, 125u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	93u, 93u, 43u, 45u, 48u, 57u, 48u, 57u, 
	43u, 45u, 48u, 57u, 48u, 57u, 43u, 45u, 
	48u, 57u, 48u, 57u, 48u, 57u, 65u, 70u, 
	97u, 102u, 43u, 45u, 48u, 57u, 48u, 57u, 
	93u, 93u, 32u, 34u, 39u, 45u, 46u, 47u, 
	48u, 58u, 60u, 61u, 62u, 91u, 95u, 126u, 
	9u, 13u, 49u, 57u, 65u, 90u, 97u, 122u, 
	32u, 9u, 13u, 45u, 62u, 10u, 91u, 10u, 
	10u, 91u, 10u, 93u, 10u, 93u, 46u, 48u, 
	57u, 46u, 69u, 101u, 48u, 57u, 48u, 57u, 
	47u, 46u, 69u, 88u, 101u, 120u, 48u, 57u, 
	69u, 101u, 48u, 57u, 48u, 57u, 46u, 69u, 
	101u, 48u, 57u, 48u, 57u, 46u, 80u, 112u, 
	48u, 57u, 65u, 70u, 97u, 102u, 80u, 112u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	58u, 60u, 61u, 61u, 61u, 62u, 95u, 48u, 
	57u, 65u, 90u, 97u, 122u, 91u, 0
};

static const char _LUAT_single_lengths[] = {
	0, 2, 10, 1, 0, 1, 0, 0, 
	2, 10, 1, 0, 1, 0, 0, 1, 
	1, 2, 0, 2, 0, 2, 0, 0, 
	2, 0, 1, 1, 14, 1, 2, 2, 
	1, 2, 2, 2, 1, 1, 2, 0, 
	1, 5, 2, 0, 3, 0, 3, 2, 
	0, 1, 0, 1, 0, 1, 1
};

static const char _LUAT_range_lengths[] = {
	0, 0, 3, 0, 3, 3, 3, 3, 
	0, 3, 0, 3, 3, 3, 3, 0, 
	0, 1, 1, 1, 1, 1, 1, 3, 
	1, 1, 0, 0, 4, 1, 0, 0, 
	0, 0, 0, 0, 1, 0, 1, 1, 
	0, 1, 1, 1, 1, 1, 3, 3, 
	1, 0, 1, 0, 1, 3, 0
};

static const short _LUAT_index_offsets[] = {
	0, 0, 3, 17, 19, 23, 28, 32, 
	36, 39, 53, 55, 59, 64, 68, 72, 
	74, 76, 80, 82, 86, 88, 92, 94, 
	98, 102, 104, 106, 108, 127, 130, 133, 
	136, 138, 141, 144, 147, 150, 152, 156, 
	158, 160, 167, 171, 173, 178, 180, 187, 
	193, 195, 197, 199, 201, 203, 208
};

static const char _LUAT_indicies[] = {
	1, 2, 0, 0, 0, 0, 0, 0, 
	0, 0, 4, 5, 0, 0, 0, 0, 
	3, 6, 3, 7, 7, 7, 3, 0, 
	7, 7, 7, 3, 8, 8, 8, 3, 
	0, 0, 0, 3, 10, 11, 9, 9, 
	9, 9, 9, 9, 9, 9, 12, 13, 
	9, 9, 9, 9, 3, 14, 3, 15, 
	15, 15, 3, 9, 15, 15, 15, 3, 
	16, 16, 16, 3, 9, 9, 9, 3, 
	19, 18, 20, 18, 22, 22, 23, 21, 
	23, 21, 25, 25, 26, 24, 26, 24, 
	28, 28, 29, 27, 29, 27, 30, 30, 
	30, 27, 32, 32, 33, 31, 33, 31, 
	36, 35, 37, 35, 39, 0, 9, 40, 
	41, 42, 43, 45, 46, 47, 48, 50, 
	49, 47, 39, 44, 49, 49, 38, 39, 
	39, 51, 53, 54, 52, 55, 57, 56, 
	58, 56, 55, 59, 56, 18, 60, 59, 
	18, 61, 59, 63, 64, 62, 54, 65, 
	67, 67, 64, 66, 23, 66, 54, 52, 
	69, 70, 71, 70, 71, 44, 68, 73, 
	73, 69, 72, 26, 72, 69, 70, 70, 
	44, 68, 29, 74, 76, 77, 77, 30, 
	30, 30, 75, 77, 77, 76, 76, 76, 
	75, 33, 75, 54, 52, 54, 52, 54, 
	52, 54, 52, 49, 49, 49, 49, 78, 
	35, 52, 0
};

static const char _LUAT_trans_targs[] = {
	1, 28, 2, 0, 3, 6, 4, 5, 
	7, 8, 28, 9, 10, 13, 11, 12, 
	14, 28, 15, 16, 28, 28, 18, 39, 
	28, 20, 43, 28, 22, 45, 46, 28, 
	25, 48, 28, 26, 27, 28, 28, 29, 
	30, 36, 40, 41, 44, 49, 50, 51, 
	52, 53, 54, 28, 28, 31, 28, 28, 
	32, 33, 28, 34, 35, 32, 28, 37, 
	38, 28, 28, 17, 28, 42, 21, 23, 
	28, 19, 28, 28, 47, 24, 28
};

static const char _LUAT_trans_actions[] = {
	0, 11, 0, 0, 0, 0, 0, 0, 
	0, 0, 13, 0, 0, 0, 0, 0, 
	0, 41, 0, 0, 7, 47, 0, 0, 
	45, 0, 0, 49, 0, 0, 5, 43, 
	0, 0, 51, 0, 0, 9, 17, 0, 
	0, 0, 0, 5, 5, 0, 0, 0, 
	0, 0, 5, 39, 35, 0, 15, 19, 
	58, 0, 53, 5, 5, 55, 37, 0, 
	5, 33, 25, 0, 29, 5, 0, 0, 
	23, 0, 27, 21, 5, 0, 31
};

static const char _LUAT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 1, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0
};

static const char _LUAT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 3, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0
};

static const short _LUAT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 18, 
	18, 22, 22, 25, 25, 28, 28, 28, 
	32, 32, 35, 35, 0, 52, 53, 56, 
	59, 56, 56, 56, 63, 66, 67, 67, 
	53, 69, 73, 73, 69, 75, 76, 76, 
	76, 53, 53, 53, 53, 79, 53
};

static const int LUAT_start = 28;
static const int LUAT_first_final = 28;
static const int LUAT_error = 0;

static const int LUAT_en_main = 28;


/* #line 112 "LUAT.c.rl" */

ok64 LUATLexer(LUATstate* state) {

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

    
/* #line 206 "LUAT.rl.c" */
	{
	cs = LUAT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 130 "LUAT.c.rl" */
    
/* #line 212 "LUAT.rl.c" */
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
	_acts = _LUAT_actions + _LUAT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 231 "LUAT.rl.c" */
		}
	}

	_keys = _LUAT_trans_keys + _LUAT_key_offsets[cs];
	_trans = _LUAT_index_offsets[cs];

	_klen = _LUAT_single_lengths[cs];
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

	_klen = _LUAT_range_lengths[cs];
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
	_trans = _LUAT_indicies[_trans];
_eof_trans:
	cs = _LUAT_trans_targs[_trans];

	if ( _LUAT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _LUAT_actions + _LUAT_trans_actions[_trans];
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
/* #line 31 "LUAT.c.rl" */
	{act = 1;}
	break;
	case 4:
/* #line 31 "LUAT.c.rl" */
	{act = 2;}
	break;
	case 5:
/* #line 31 "LUAT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 37 "LUAT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 37 "LUAT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 37 "LUAT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 55 "LUAT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 55 "LUAT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 31 "LUAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 43 "LUAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 43 "LUAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 43 "LUAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 43 "LUAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 43 "LUAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 49 "LUAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 55 "LUAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 55 "LUAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 55 "LUAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 61 "LUAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 31 "LUAT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 43 "LUAT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 43 "LUAT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 43 "LUAT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 43 "LUAT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 55 "LUAT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 1:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 2:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 510 "LUAT.rl.c" */
		}
	}

_again:
	_acts = _LUAT_actions + _LUAT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 521 "LUAT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _LUAT_eof_trans[cs] > 0 ) {
		_trans = _LUAT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 131 "LUAT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < LUAT_first_final)
        o = LUATBAD;

    return o;
}
