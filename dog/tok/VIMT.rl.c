
/* #line 1 "VIMT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "VIMT.h"

ok64 VIMTonComment (u8cs tok, VIMTstate* state);
ok64 VIMTonString (u8cs tok, VIMTstate* state);
ok64 VIMTonNumber (u8cs tok, VIMTstate* state);
ok64 VIMTonWord (u8cs tok, VIMTstate* state);
ok64 VIMTonPunct (u8cs tok, VIMTstate* state);
ok64 VIMTonSpace (u8cs tok, VIMTstate* state);


/* #line 105 "VIMT.c.rl" */



/* #line 15 "VIMT.rl.c" */
static const char _VIMT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19, 1, 20, 1, 21, 1, 22
};

static const char _VIMT_key_offsets[] = {
	0, 0, 1, 7, 8, 12, 14, 18, 
	20, 22, 24, 30, 31, 32, 57, 60, 
	62, 63, 70, 72, 74, 85, 89, 91, 
	96, 98, 100, 102, 108, 110, 112, 121
};

static const unsigned char _VIMT_trans_keys[] = {
	10u, 38u, 95u, 65u, 90u, 97u, 122u, 39u, 
	43u, 45u, 48u, 57u, 48u, 57u, 43u, 45u, 
	48u, 57u, 48u, 57u, 48u, 49u, 48u, 55u, 
	48u, 57u, 65u, 70u, 97u, 102u, 50u, 51u, 
	32u, 33u, 34u, 37u, 38u, 39u, 45u, 46u, 
	47u, 48u, 61u, 95u, 124u, 9u, 13u, 42u, 
	43u, 49u, 57u, 60u, 62u, 65u, 90u, 97u, 
	122u, 32u, 9u, 13u, 61u, 126u, 61u, 95u, 
	48u, 57u, 65u, 90u, 97u, 122u, 61u, 62u, 
	46u, 61u, 46u, 66u, 69u, 79u, 88u, 98u, 
	101u, 111u, 120u, 48u, 57u, 69u, 101u, 48u, 
	57u, 48u, 57u, 46u, 69u, 101u, 48u, 57u, 
	48u, 57u, 48u, 49u, 48u, 55u, 48u, 57u, 
	65u, 70u, 97u, 102u, 61u, 126u, 63u, 120u, 
	35u, 58u, 95u, 48u, 57u, 65u, 90u, 97u, 
	122u, 124u, 0
};

static const char _VIMT_single_lengths[] = {
	0, 1, 2, 1, 2, 0, 2, 0, 
	0, 0, 0, 1, 1, 13, 1, 2, 
	1, 1, 0, 2, 9, 2, 0, 3, 
	0, 0, 0, 0, 2, 2, 3, 1
};

static const char _VIMT_range_lengths[] = {
	0, 0, 2, 0, 1, 1, 1, 1, 
	1, 1, 3, 0, 0, 6, 1, 0, 
	0, 3, 1, 0, 1, 1, 1, 1, 
	1, 1, 1, 3, 0, 0, 3, 0
};

static const unsigned char _VIMT_index_offsets[] = {
	0, 0, 2, 7, 9, 13, 15, 19, 
	21, 23, 25, 29, 31, 33, 53, 56, 
	59, 61, 66, 68, 71, 82, 86, 88, 
	93, 95, 97, 99, 103, 106, 109, 116
};

static const char _VIMT_indicies[] = {
	1, 0, 2, 4, 4, 4, 3, 6, 
	5, 8, 8, 9, 7, 9, 7, 11, 
	11, 12, 10, 12, 10, 13, 10, 14, 
	10, 15, 15, 15, 10, 17, 16, 2, 
	16, 19, 20, 0, 21, 22, 5, 23, 
	24, 21, 25, 27, 28, 29, 19, 21, 
	26, 21, 28, 28, 18, 19, 19, 30, 
	2, 2, 31, 2, 31, 4, 4, 4, 
	4, 32, 2, 31, 2, 2, 33, 35, 
	36, 37, 38, 39, 36, 37, 38, 39, 
	26, 34, 41, 41, 35, 40, 9, 40, 
	35, 37, 37, 26, 34, 12, 42, 13, 
	43, 14, 44, 15, 15, 15, 45, 46, 
	46, 31, 2, 48, 47, 50, 50, 28, 
	28, 28, 28, 49, 2, 31, 0
};

static const char _VIMT_trans_targs[] = {
	1, 13, 13, 0, 17, 3, 13, 13, 
	5, 22, 13, 7, 24, 25, 26, 27, 
	13, 12, 13, 14, 15, 16, 2, 18, 
	19, 20, 23, 28, 30, 31, 13, 13, 
	13, 13, 13, 21, 8, 6, 9, 10, 
	13, 4, 13, 13, 13, 13, 29, 13, 
	11, 13, 13
};

static const char _VIMT_trans_actions[] = {
	0, 7, 13, 0, 0, 0, 9, 41, 
	0, 0, 43, 0, 0, 0, 0, 0, 
	45, 0, 15, 0, 0, 0, 0, 0, 
	0, 5, 5, 0, 0, 0, 39, 35, 
	31, 37, 27, 5, 0, 0, 0, 0, 
	23, 0, 25, 21, 19, 17, 5, 33, 
	0, 29, 11
};

static const char _VIMT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 1, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0
};

static const char _VIMT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 3, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char _VIMT_eof_trans[] = {
	0, 0, 0, 0, 8, 8, 11, 11, 
	11, 11, 11, 17, 17, 0, 31, 32, 
	32, 33, 32, 34, 35, 41, 41, 35, 
	43, 44, 45, 46, 32, 48, 50, 32
};

static const int VIMT_start = 13;
static const int VIMT_first_final = 13;
static const int VIMT_error = 0;

static const int VIMT_en_main = 13;


/* #line 108 "VIMT.c.rl" */

ok64 VIMTLexer(VIMTstate* state) {

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

    
/* #line 153 "VIMT.rl.c" */
	{
	cs = VIMT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 126 "VIMT.c.rl" */
    
/* #line 159 "VIMT.rl.c" */
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
	_acts = _VIMT_actions + _VIMT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 178 "VIMT.rl.c" */
		}
	}

	_keys = _VIMT_trans_keys + _VIMT_key_offsets[cs];
	_trans = _VIMT_index_offsets[cs];

	_klen = _VIMT_single_lengths[cs];
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

	_klen = _VIMT_range_lengths[cs];
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
	_trans = _VIMT_indicies[_trans];
_eof_trans:
	cs = _VIMT_trans_targs[_trans];

	if ( _VIMT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _VIMT_actions + _VIMT_trans_actions[_trans];
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
/* #line 28 "VIMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 4:
/* #line 34 "VIMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 5:
/* #line 46 "VIMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 52 "VIMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 52 "VIMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 40 "VIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 40 "VIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 40 "VIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 40 "VIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 40 "VIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 40 "VIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 46 "VIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 46 "VIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 52 "VIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 52 "VIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 52 "VIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 58 "VIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 40 "VIMT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 40 "VIMT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 52 "VIMT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
/* #line 406 "VIMT.rl.c" */
		}
	}

_again:
	_acts = _VIMT_actions + _VIMT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 417 "VIMT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _VIMT_eof_trans[cs] > 0 ) {
		_trans = _VIMT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 127 "VIMT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < VIMT_first_final)
        o = VIMTBAD;

    return o;
}
