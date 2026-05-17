
/* #line 1 "PRTT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "PRTT.h"

ok64 PRTTonComment (u8cs tok, PRTTstate* state);
ok64 PRTTonString (u8cs tok, PRTTstate* state);
ok64 PRTTonNumber (u8cs tok, PRTTstate* state);
ok64 PRTTonWord (u8cs tok, PRTTstate* state);
ok64 PRTTonPunct (u8cs tok, PRTTstate* state);
ok64 PRTTonSpace (u8cs tok, PRTTstate* state);


/* #line 96 "PRTT.c.rl" */



/* #line 15 "PRTT.rl.c" */
static const char _PRTT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	5, 1, 6, 1, 7, 1, 8, 1, 
	9, 1, 10, 1, 11, 1, 12, 1, 
	13, 1, 14, 1, 15, 1, 16, 1, 
	17, 1, 18, 1, 19, 1, 20, 1, 
	21, 1, 22, 1, 23, 2, 2, 3, 
	2, 2, 4
};

static const unsigned char _PRTT_key_offsets[] = {
	0, 0, 3, 16, 22, 28, 31, 44, 
	50, 56, 60, 62, 63, 65, 69, 71, 
	75, 77, 83, 98, 101, 103, 107, 109, 
	111, 112, 121, 125, 127, 134, 139, 141, 
	147
};

static const unsigned char _PRTT_trans_keys[] = {
	10u, 34u, 92u, 34u, 39u, 92u, 102u, 110u, 
	114u, 116u, 118u, 120u, 48u, 55u, 97u, 98u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 10u, 39u, 92u, 34u, 
	39u, 92u, 102u, 110u, 114u, 116u, 118u, 120u, 
	48u, 55u, 97u, 98u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	43u, 45u, 48u, 57u, 48u, 57u, 42u, 42u, 
	47u, 43u, 45u, 48u, 57u, 48u, 57u, 43u, 
	45u, 48u, 57u, 48u, 57u, 48u, 57u, 65u, 
	70u, 97u, 102u, 32u, 34u, 39u, 46u, 47u, 
	48u, 95u, 9u, 13u, 49u, 57u, 65u, 90u, 
	97u, 122u, 32u, 9u, 13u, 48u, 57u, 69u, 
	101u, 48u, 57u, 48u, 57u, 42u, 47u, 10u, 
	46u, 69u, 88u, 101u, 120u, 48u, 55u, 56u, 
	57u, 69u, 101u, 48u, 57u, 48u, 57u, 46u, 
	69u, 101u, 48u, 55u, 56u, 57u, 46u, 69u, 
	101u, 48u, 57u, 48u, 57u, 48u, 57u, 65u, 
	70u, 97u, 102u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 0
};

static const char _PRTT_single_lengths[] = {
	0, 3, 9, 0, 0, 3, 9, 0, 
	0, 2, 0, 1, 2, 2, 0, 2, 
	0, 0, 7, 1, 0, 2, 0, 2, 
	1, 5, 2, 0, 3, 3, 0, 0, 
	1
};

static const char _PRTT_range_lengths[] = {
	0, 0, 2, 3, 3, 0, 2, 3, 
	3, 1, 1, 0, 0, 1, 1, 1, 
	1, 3, 4, 1, 1, 1, 1, 0, 
	0, 2, 1, 1, 2, 1, 1, 3, 
	3
};

static const unsigned char _PRTT_index_offsets[] = {
	0, 0, 4, 16, 20, 24, 28, 40, 
	44, 48, 52, 54, 56, 59, 63, 65, 
	69, 71, 75, 87, 90, 92, 96, 98, 
	101, 103, 111, 115, 117, 123, 128, 130, 
	134
};

static const char _PRTT_indicies[] = {
	1, 2, 3, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 4, 0, 0, 1, 
	5, 5, 5, 1, 0, 0, 0, 1, 
	1, 7, 8, 6, 6, 6, 6, 6, 
	6, 6, 6, 6, 9, 6, 6, 1, 
	10, 10, 10, 1, 6, 6, 6, 1, 
	12, 12, 13, 11, 13, 11, 16, 15, 
	16, 17, 15, 19, 19, 20, 18, 20, 
	18, 22, 22, 23, 21, 23, 21, 25, 
	25, 25, 24, 27, 0, 6, 28, 29, 
	30, 32, 27, 31, 32, 32, 26, 27, 
	27, 33, 35, 34, 37, 37, 35, 36, 
	13, 36, 15, 39, 38, 40, 39, 42, 
	44, 45, 44, 45, 43, 31, 41, 47, 
	47, 42, 46, 20, 46, 42, 44, 44, 
	43, 31, 48, 42, 44, 44, 31, 41, 
	23, 41, 25, 25, 25, 49, 32, 32, 
	32, 32, 50, 0
};

static const char _PRTT_trans_targs[] = {
	1, 0, 18, 2, 3, 4, 5, 18, 
	6, 7, 8, 18, 10, 22, 18, 11, 
	12, 18, 18, 14, 27, 18, 16, 30, 
	18, 31, 18, 19, 20, 23, 25, 29, 
	32, 18, 18, 21, 18, 9, 18, 24, 
	18, 18, 26, 28, 15, 17, 18, 13, 
	18, 18, 18
};

static const char _PRTT_trans_actions[] = {
	0, 0, 9, 0, 0, 0, 0, 11, 
	0, 0, 0, 37, 0, 0, 41, 0, 
	0, 7, 35, 0, 0, 43, 0, 0, 
	39, 0, 13, 0, 0, 5, 48, 48, 
	0, 33, 31, 5, 23, 0, 29, 0, 
	15, 25, 5, 45, 0, 0, 21, 0, 
	19, 17, 27
};

static const char _PRTT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 1, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const char _PRTT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 3, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const unsigned char _PRTT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 12, 12, 15, 15, 19, 19, 22, 
	22, 25, 0, 34, 35, 37, 37, 39, 
	41, 42, 47, 47, 49, 42, 42, 50, 
	51
};

static const int PRTT_start = 18;
static const int PRTT_first_final = 18;
static const int PRTT_error = 0;

static const int PRTT_en_main = 18;


/* #line 99 "PRTT.c.rl" */

ok64 PRTTLexer(PRTTstate* state) {

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

    
/* #line 168 "PRTT.rl.c" */
	{
	cs = PRTT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 117 "PRTT.c.rl" */
    
/* #line 174 "PRTT.rl.c" */
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
	_acts = _PRTT_actions + _PRTT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 193 "PRTT.rl.c" */
		}
	}

	_keys = _PRTT_trans_keys + _PRTT_key_offsets[cs];
	_trans = _PRTT_index_offsets[cs];

	_klen = _PRTT_single_lengths[cs];
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

	_klen = _PRTT_range_lengths[cs];
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
	_trans = _PRTT_indicies[_trans];
_eof_trans:
	cs = _PRTT_trans_targs[_trans];

	if ( _PRTT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _PRTT_actions + _PRTT_trans_actions[_trans];
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
/* #line 40 "PRTT.c.rl" */
	{act = 6;}
	break;
	case 4:
/* #line 40 "PRTT.c.rl" */
	{act = 9;}
	break;
	case 5:
/* #line 28 "PRTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 34 "PRTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 34 "PRTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 52 "PRTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 28 "PRTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 40 "PRTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 40 "PRTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 40 "PRTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 40 "PRTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 40 "PRTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 46 "PRTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 52 "PRTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 52 "PRTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 58 "PRTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 40 "PRTT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 40 "PRTT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 40 "PRTT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 52 "PRTT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 6:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 9:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 432 "PRTT.rl.c" */
		}
	}

_again:
	_acts = _PRTT_actions + _PRTT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 443 "PRTT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _PRTT_eof_trans[cs] > 0 ) {
		_trans = _PRTT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 118 "PRTT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < PRTT_first_final)
        o = PRTTBAD;

    return o;
}
