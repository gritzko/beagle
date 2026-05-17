
/* #line 1 "GLMT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "GLMT.h"

ok64 GLMTonComment (u8cs tok, GLMTstate* state);
ok64 GLMTonString (u8cs tok, GLMTstate* state);
ok64 GLMTonNumber (u8cs tok, GLMTstate* state);
ok64 GLMTonWord (u8cs tok, GLMTstate* state);
ok64 GLMTonPunct (u8cs tok, GLMTstate* state);
ok64 GLMTonSpace (u8cs tok, GLMTstate* state);


/* #line 105 "GLMT.c.rl" */



/* #line 15 "GLMT.rl.c" */
static const char _GLMT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	8, 1, 9, 1, 10, 1, 11, 1, 
	12, 1, 13, 1, 14, 1, 15, 1, 
	16, 1, 17, 1, 18, 1, 19, 1, 
	20, 1, 21, 1, 22, 1, 23, 1, 
	24, 1, 25, 1, 26, 2, 2, 3, 
	2, 2, 4, 2, 2, 5, 2, 2, 
	6, 2, 2, 7
};

static const unsigned char _GLMT_key_offsets[] = {
	0, 0, 2, 16, 22, 28, 34, 40, 
	44, 46, 48, 52, 54, 56, 60, 62, 
	64, 66, 68, 74, 95, 98, 99, 100, 
	101, 104, 109, 112, 113, 114, 126, 130, 
	135, 138, 144, 147, 150, 153, 160, 162, 
	169
};

static const unsigned char _GLMT_trans_keys[] = {
	34u, 92u, 34u, 39u, 48u, 92u, 110u, 114u, 
	117u, 120u, 97u, 98u, 101u, 102u, 116u, 118u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	43u, 45u, 48u, 57u, 48u, 57u, 48u, 57u, 
	43u, 45u, 48u, 57u, 48u, 57u, 48u, 57u, 
	43u, 45u, 48u, 57u, 48u, 57u, 48u, 57u, 
	48u, 49u, 48u, 55u, 48u, 57u, 65u, 70u, 
	97u, 102u, 32u, 33u, 34u, 38u, 45u, 46u, 
	47u, 48u, 60u, 95u, 124u, 9u, 13u, 49u, 
	57u, 61u, 62u, 65u, 90u, 97u, 122u, 32u, 
	9u, 13u, 61u, 38u, 62u, 46u, 48u, 57u, 
	69u, 95u, 101u, 48u, 57u, 95u, 48u, 57u, 
	47u, 10u, 46u, 66u, 69u, 79u, 88u, 95u, 
	98u, 101u, 111u, 120u, 48u, 57u, 69u, 101u, 
	48u, 57u, 69u, 95u, 101u, 48u, 57u, 95u, 
	48u, 57u, 46u, 69u, 95u, 101u, 48u, 57u, 
	95u, 48u, 57u, 95u, 48u, 49u, 95u, 48u, 
	55u, 95u, 48u, 57u, 65u, 70u, 97u, 102u, 
	45u, 61u, 95u, 48u, 57u, 65u, 90u, 97u, 
	122u, 62u, 124u, 0
};

static const char _GLMT_single_lengths[] = {
	0, 2, 8, 0, 0, 0, 0, 2, 
	0, 0, 2, 0, 0, 2, 0, 0, 
	0, 0, 0, 11, 1, 1, 1, 1, 
	1, 3, 1, 1, 1, 10, 2, 3, 
	1, 4, 1, 1, 1, 1, 2, 1, 
	2
};

static const char _GLMT_range_lengths[] = {
	0, 0, 3, 3, 3, 3, 3, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 3, 5, 1, 0, 0, 0, 
	1, 1, 1, 0, 0, 1, 1, 1, 
	1, 1, 1, 1, 1, 3, 0, 3, 
	0
};

static const unsigned char _GLMT_index_offsets[] = {
	0, 0, 3, 15, 19, 23, 27, 31, 
	35, 37, 39, 43, 45, 47, 51, 53, 
	55, 57, 59, 63, 80, 83, 85, 87, 
	89, 92, 97, 100, 102, 104, 116, 120, 
	125, 128, 134, 137, 140, 143, 148, 151, 
	156
};

static const char _GLMT_indicies[] = {
	1, 2, 0, 0, 0, 0, 0, 0, 
	0, 4, 5, 0, 0, 0, 3, 6, 
	6, 6, 3, 5, 5, 5, 3, 7, 
	7, 7, 3, 0, 0, 0, 3, 9, 
	9, 10, 8, 10, 8, 11, 8, 13, 
	13, 14, 12, 14, 12, 15, 12, 17, 
	17, 18, 16, 18, 19, 20, 16, 21, 
	19, 22, 19, 23, 23, 23, 19, 25, 
	26, 0, 27, 28, 29, 30, 31, 32, 
	33, 34, 25, 20, 26, 33, 33, 24, 
	25, 25, 35, 37, 36, 37, 36, 37, 
	36, 37, 11, 38, 40, 41, 40, 11, 
	39, 9, 10, 39, 42, 36, 43, 42, 
	45, 46, 47, 48, 49, 50, 46, 47, 
	48, 49, 20, 44, 52, 52, 15, 51, 
	52, 53, 52, 15, 51, 13, 14, 51, 
	45, 47, 50, 47, 20, 44, 17, 18, 
	54, 46, 21, 55, 48, 22, 56, 49, 
	23, 23, 23, 57, 37, 37, 36, 33, 
	33, 33, 33, 58, 37, 37, 36, 0
};

static const char _GLMT_trans_targs[] = {
	1, 19, 2, 0, 3, 5, 4, 6, 
	19, 8, 26, 25, 19, 11, 32, 31, 
	19, 14, 34, 19, 33, 35, 36, 37, 
	19, 20, 21, 22, 23, 24, 27, 29, 
	38, 39, 40, 19, 19, 19, 19, 19, 
	7, 9, 28, 19, 19, 30, 16, 13, 
	17, 18, 15, 19, 10, 12, 19, 19, 
	19, 19, 19
};

static const char _GLMT_trans_actions[] = {
	0, 7, 0, 0, 0, 0, 0, 0, 
	39, 0, 5, 5, 37, 0, 5, 5, 
	41, 0, 54, 43, 57, 51, 48, 45, 
	11, 0, 0, 0, 0, 0, 0, 57, 
	0, 0, 0, 35, 31, 9, 33, 23, 
	0, 0, 0, 13, 27, 5, 0, 0, 
	0, 0, 0, 21, 0, 0, 25, 19, 
	17, 15, 29
};

static const char _GLMT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const char _GLMT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 3, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const unsigned char _GLMT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 9, 
	9, 9, 13, 13, 13, 17, 20, 17, 
	20, 20, 20, 0, 36, 37, 37, 37, 
	39, 40, 40, 37, 44, 45, 52, 52, 
	52, 45, 55, 56, 57, 58, 37, 59, 
	37
};

static const int GLMT_start = 19;
static const int GLMT_first_final = 19;
static const int GLMT_error = 0;

static const int GLMT_en_main = 19;


/* #line 108 "GLMT.c.rl" */

ok64 GLMTLexer(GLMTstate* state) {

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

    
/* #line 182 "GLMT.rl.c" */
	{
	cs = GLMT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 126 "GLMT.c.rl" */
    
/* #line 188 "GLMT.rl.c" */
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
	_acts = _GLMT_actions + _GLMT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 207 "GLMT.rl.c" */
		}
	}

	_keys = _GLMT_trans_keys + _GLMT_key_offsets[cs];
	_trans = _GLMT_index_offsets[cs];

	_klen = _GLMT_single_lengths[cs];
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

	_klen = _GLMT_range_lengths[cs];
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
	_trans = _GLMT_indicies[_trans];
_eof_trans:
	cs = _GLMT_trans_targs[_trans];

	if ( _GLMT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _GLMT_actions + _GLMT_trans_actions[_trans];
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
/* #line 42 "GLMT.c.rl" */
	{act = 3;}
	break;
	case 4:
/* #line 42 "GLMT.c.rl" */
	{act = 4;}
	break;
	case 5:
/* #line 42 "GLMT.c.rl" */
	{act = 5;}
	break;
	case 6:
/* #line 42 "GLMT.c.rl" */
	{act = 8;}
	break;
	case 7:
/* #line 42 "GLMT.c.rl" */
	{act = 9;}
	break;
	case 8:
/* #line 36 "GLMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 54 "GLMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 54 "GLMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 30 "GLMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 42 "GLMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 42 "GLMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 42 "GLMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 42 "GLMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 42 "GLMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 42 "GLMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 42 "GLMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 48 "GLMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 54 "GLMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 54 "GLMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 60 "GLMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 42 "GLMT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 42 "GLMT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 42 "GLMT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 3:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 4:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 5:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 8:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 9:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 479 "GLMT.rl.c" */
		}
	}

_again:
	_acts = _GLMT_actions + _GLMT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 490 "GLMT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _GLMT_eof_trans[cs] > 0 ) {
		_trans = _GLMT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 127 "GLMT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < GLMT_first_final)
        o = GLMTBAD;

    return o;
}
