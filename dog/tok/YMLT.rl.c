
/* #line 1 "YMLT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "YMLT.h"

ok64 YMLTonComment (u8cs tok, YMLTstate* state);
ok64 YMLTonString (u8cs tok, YMLTstate* state);
ok64 YMLTonNumber (u8cs tok, YMLTstate* state);
ok64 YMLTonWord (u8cs tok, YMLTstate* state);
ok64 YMLTonPunct (u8cs tok, YMLTstate* state);
ok64 YMLTonSpace (u8cs tok, YMLTstate* state);


/* #line 105 "YMLT.c.rl" */



/* #line 15 "YMLT.rl.c" */
static const char _YMLT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	5, 1, 6, 1, 7, 1, 8, 1, 
	9, 1, 10, 1, 11, 1, 12, 1, 
	13, 1, 14, 1, 15, 1, 16, 1, 
	17, 1, 18, 1, 19, 1, 20, 1, 
	21, 1, 22, 1, 23, 1, 24, 1, 
	25, 1, 26, 1, 27, 1, 28, 1, 
	29, 2, 2, 3, 2, 2, 4
};

static const unsigned char _YMLT_key_offsets[] = {
	0, 7, 9, 9, 10, 14, 18, 20, 
	22, 23, 24, 25, 29, 31, 35, 37, 
	39, 40, 41, 42, 44, 50, 72, 75, 
	83, 90, 97, 99, 100, 105, 112, 113, 
	116, 120, 122, 127, 131, 133, 135, 141, 
	150, 152, 158
};

static const unsigned char _YMLT_trans_keys[] = {
	95u, 48u, 57u, 65u, 90u, 97u, 122u, 34u, 
	92u, 39u, 73u, 105u, 48u, 57u, 43u, 45u, 
	48u, 57u, 48u, 57u, 78u, 110u, 70u, 102u, 
	110u, 43u, 45u, 48u, 57u, 48u, 57u, 43u, 
	45u, 48u, 57u, 48u, 57u, 65u, 97u, 78u, 
	97u, 110u, 48u, 55u, 48u, 57u, 65u, 70u, 
	97u, 102u, 32u, 33u, 34u, 35u, 38u, 39u, 
	42u, 43u, 45u, 46u, 48u, 62u, 95u, 124u, 
	9u, 13u, 49u, 57u, 65u, 90u, 97u, 122u, 
	32u, 9u, 13u, 33u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 95u, 48u, 57u, 65u, 90u, 97u, 
	122u, 34u, 92u, 10u, 95u, 65u, 90u, 97u, 
	122u, 95u, 48u, 57u, 65u, 90u, 97u, 122u, 
	39u, 46u, 48u, 57u, 69u, 101u, 48u, 57u, 
	48u, 57u, 46u, 69u, 101u, 48u, 57u, 69u, 
	101u, 48u, 57u, 48u, 57u, 48u, 57u, 73u, 
	78u, 105u, 110u, 48u, 57u, 46u, 69u, 79u, 
	88u, 101u, 111u, 120u, 48u, 57u, 48u, 55u, 
	48u, 57u, 65u, 70u, 97u, 102u, 95u, 48u, 
	57u, 65u, 90u, 97u, 122u, 0
};

static const char _YMLT_single_lengths[] = {
	1, 2, 0, 1, 2, 2, 0, 2, 
	1, 1, 1, 2, 0, 2, 0, 2, 
	1, 1, 1, 0, 0, 14, 1, 2, 
	1, 1, 2, 1, 1, 1, 1, 1, 
	2, 0, 3, 2, 0, 0, 4, 7, 
	0, 0, 1
};

static const char _YMLT_range_lengths[] = {
	3, 0, 0, 0, 1, 1, 1, 0, 
	0, 0, 0, 1, 1, 1, 1, 0, 
	0, 0, 0, 1, 3, 4, 1, 3, 
	3, 3, 0, 0, 2, 3, 0, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 3, 3
};

static const unsigned char _YMLT_index_offsets[] = {
	0, 5, 8, 9, 11, 15, 19, 21, 
	24, 26, 28, 30, 34, 36, 40, 42, 
	45, 47, 49, 51, 53, 57, 76, 79, 
	85, 90, 95, 98, 100, 104, 109, 111, 
	114, 118, 120, 125, 129, 131, 133, 139, 
	148, 150, 154
};

static const char _YMLT_indicies[] = {
	1, 1, 1, 1, 0, 3, 4, 2, 
	2, 6, 5, 8, 9, 7, 0, 11, 
	11, 12, 10, 12, 10, 14, 15, 13, 
	16, 13, 16, 13, 15, 13, 18, 18, 
	19, 17, 19, 17, 21, 21, 22, 20, 
	22, 20, 24, 24, 23, 25, 23, 26, 
	23, 25, 23, 27, 20, 28, 28, 28, 
	20, 30, 31, 32, 33, 34, 35, 34, 
	36, 36, 37, 38, 40, 41, 40, 30, 
	39, 41, 41, 29, 30, 30, 42, 44, 
	45, 45, 45, 45, 43, 1, 1, 1, 
	1, 46, 45, 45, 45, 45, 47, 3, 
	4, 2, 48, 33, 49, 49, 49, 43, 
	49, 49, 49, 49, 50, 6, 5, 51, 
	39, 43, 53, 53, 7, 52, 12, 52, 
	55, 56, 56, 39, 54, 58, 58, 55, 
	57, 19, 57, 22, 54, 8, 60, 9, 
	61, 7, 59, 55, 56, 62, 63, 56, 
	62, 63, 39, 54, 27, 64, 28, 28, 
	28, 65, 41, 41, 41, 41, 66, 0
};

static const char _YMLT_trans_targs[] = {
	21, 24, 1, 21, 2, 3, 21, 32, 
	7, 10, 21, 6, 33, 21, 8, 9, 
	21, 21, 12, 36, 21, 14, 37, 21, 
	16, 21, 18, 40, 41, 21, 22, 23, 
	26, 27, 28, 30, 31, 38, 39, 34, 
	21, 42, 21, 21, 0, 25, 21, 21, 
	21, 29, 21, 4, 21, 5, 21, 35, 
	13, 21, 11, 21, 15, 17, 19, 20, 
	21, 21, 21
};

static const char _YMLT_trans_actions[] = {
	51, 0, 0, 7, 0, 0, 9, 5, 
	0, 0, 47, 0, 0, 55, 0, 0, 
	11, 45, 0, 0, 49, 0, 0, 53, 
	0, 13, 0, 0, 0, 17, 0, 5, 
	5, 0, 0, 5, 57, 60, 5, 5, 
	15, 0, 43, 39, 0, 0, 35, 33, 
	19, 0, 31, 0, 27, 0, 29, 5, 
	0, 25, 0, 41, 0, 0, 0, 0, 
	23, 21, 37
};

static const char _YMLT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 1, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const char _YMLT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 3, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const unsigned char _YMLT_eof_trans[] = {
	1, 1, 1, 1, 1, 11, 11, 14, 
	14, 14, 14, 18, 18, 21, 21, 24, 
	24, 24, 24, 21, 21, 0, 43, 44, 
	47, 48, 44, 49, 44, 51, 44, 44, 
	53, 53, 55, 58, 58, 55, 60, 55, 
	65, 66, 67
};

static const int YMLT_start = 21;
static const int YMLT_first_final = 21;
static const int YMLT_error = -1;

static const int YMLT_en_main = 21;


/* #line 108 "YMLT.c.rl" */

ok64 YMLTLexer(YMLTstate* state) {

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

    
/* #line 183 "YMLT.rl.c" */
	{
	cs = YMLT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 126 "YMLT.c.rl" */
    
/* #line 189 "YMLT.rl.c" */
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( p == pe )
		goto _test_eof;
_resume:
	_acts = _YMLT_actions + _YMLT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 206 "YMLT.rl.c" */
		}
	}

	_keys = _YMLT_trans_keys + _YMLT_key_offsets[cs];
	_trans = _YMLT_index_offsets[cs];

	_klen = _YMLT_single_lengths[cs];
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

	_klen = _YMLT_range_lengths[cs];
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
	_trans = _YMLT_indicies[_trans];
_eof_trans:
	cs = _YMLT_trans_targs[_trans];

	if ( _YMLT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _YMLT_actions + _YMLT_trans_actions[_trans];
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
/* #line 50 "YMLT.c.rl" */
	{act = 16;}
	break;
	case 4:
/* #line 50 "YMLT.c.rl" */
	{act = 17;}
	break;
	case 5:
/* #line 32 "YMLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 32 "YMLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 38 "YMLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 38 "YMLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 50 "YMLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 50 "YMLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 26 "YMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 38 "YMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 38 "YMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 38 "YMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 38 "YMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 38 "YMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 50 "YMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 50 "YMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 50 "YMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 44 "YMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 50 "YMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 50 "YMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 56 "YMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 38 "YMLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 38 "YMLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 38 "YMLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 50 "YMLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 50 "YMLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 16:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 17:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 493 "YMLT.rl.c" */
		}
	}

_again:
	_acts = _YMLT_actions + _YMLT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 504 "YMLT.rl.c" */
		}
	}

	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _YMLT_eof_trans[cs] > 0 ) {
		_trans = _YMLT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 127 "YMLT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < YMLT_first_final)
        o = YMLTBAD;

    return o;
}
