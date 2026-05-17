
/* #line 1 "JSONT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "JSONT.h"

ok64 JSONTonString (u8cs tok, JSONTstate* state);
ok64 JSONTonNumber (u8cs tok, JSONTstate* state);
ok64 JSONTonKeyword (u8cs tok, JSONTstate* state);
ok64 JSONTonPunct (u8cs tok, JSONTstate* state);
ok64 JSONTonSpace (u8cs tok, JSONTstate* state);


/* #line 75 "JSONT.c.rl" */



/* #line 14 "JSONT.rl.c" */
static const char _JSONT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8
};

static const char _JSONT_key_offsets[] = {
	0, 0, 2, 11, 17, 23, 29, 35, 
	37, 39, 43, 45, 46, 47, 48, 49, 
	50, 51, 52, 53, 54, 70, 73, 78, 
	82
};

static const unsigned char _JSONT_trans_keys[] = {
	34u, 92u, 34u, 47u, 92u, 98u, 102u, 110u, 
	114u, 116u, 117u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 48u, 57u, 43u, 
	45u, 48u, 57u, 48u, 57u, 97u, 108u, 115u, 
	101u, 117u, 108u, 108u, 114u, 117u, 32u, 34u, 
	44u, 45u, 58u, 91u, 93u, 102u, 110u, 116u, 
	123u, 125u, 9u, 13u, 48u, 57u, 32u, 9u, 
	13u, 46u, 69u, 101u, 48u, 57u, 69u, 101u, 
	48u, 57u, 48u, 57u, 0
};

static const char _JSONT_single_lengths[] = {
	0, 2, 9, 0, 0, 0, 0, 0, 
	0, 2, 0, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 12, 1, 3, 2, 
	0
};

static const char _JSONT_range_lengths[] = {
	0, 0, 0, 3, 3, 3, 3, 1, 
	1, 1, 1, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 2, 1, 1, 1, 
	1
};

static const char _JSONT_index_offsets[] = {
	0, 0, 3, 13, 17, 21, 25, 29, 
	31, 33, 37, 39, 41, 43, 45, 47, 
	49, 51, 53, 55, 57, 72, 75, 80, 
	84
};

static const char _JSONT_indicies[] = {
	1, 2, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 4, 3, 5, 5, 5, 
	3, 6, 6, 6, 3, 7, 7, 7, 
	3, 0, 0, 0, 3, 8, 3, 10, 
	9, 11, 11, 12, 9, 12, 9, 13, 
	3, 14, 3, 15, 3, 16, 3, 17, 
	3, 18, 3, 16, 3, 19, 3, 15, 
	3, 20, 0, 21, 22, 21, 21, 21, 
	23, 24, 25, 21, 21, 20, 8, 3, 
	20, 20, 26, 28, 29, 29, 8, 27, 
	29, 29, 10, 27, 12, 27, 0
};

static const char _JSONT_trans_targs[] = {
	1, 20, 2, 0, 3, 4, 5, 6, 
	22, 20, 23, 10, 24, 12, 13, 14, 
	20, 16, 17, 19, 21, 20, 7, 11, 
	15, 18, 20, 20, 8, 9
};

static const char _JSONT_trans_actions[] = {
	0, 7, 0, 0, 0, 0, 0, 0, 
	5, 17, 5, 0, 0, 0, 0, 0, 
	9, 0, 0, 0, 0, 11, 0, 0, 
	0, 0, 15, 13, 0, 0
};

static const char _JSONT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 1, 0, 0, 0, 
	0
};

static const char _JSONT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 3, 0, 0, 0, 
	0
};

static const char _JSONT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	10, 10, 10, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 27, 28, 28, 
	28
};

static const int JSONT_start = 20;
static const int JSONT_first_final = 20;
static const int JSONT_error = 0;

static const int JSONT_en_main = 20;


/* #line 78 "JSONT.c.rl" */

ok64 JSONTLexer(JSONTstate* state) {

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

    
/* #line 134 "JSONT.rl.c" */
	{
	cs = JSONT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 96 "JSONT.c.rl" */
    
/* #line 140 "JSONT.rl.c" */
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
	_acts = _JSONT_actions + _JSONT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 159 "JSONT.rl.c" */
		}
	}

	_keys = _JSONT_trans_keys + _JSONT_key_offsets[cs];
	_trans = _JSONT_index_offsets[cs];

	_klen = _JSONT_single_lengths[cs];
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

	_klen = _JSONT_range_lengths[cs];
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
	_trans = _JSONT_indicies[_trans];
_eof_trans:
	cs = _JSONT_trans_targs[_trans];

	if ( _JSONT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _JSONT_actions + _JSONT_trans_actions[_trans];
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
/* #line 25 "JSONT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSONTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 4:
/* #line 37 "JSONT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSONTonKeyword(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 5:
/* #line 43 "JSONT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSONTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 31 "JSONT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSONTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 49 "JSONT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSONTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 31 "JSONT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSONTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
/* #line 275 "JSONT.rl.c" */
		}
	}

_again:
	_acts = _JSONT_actions + _JSONT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 286 "JSONT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _JSONT_eof_trans[cs] > 0 ) {
		_trans = _JSONT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 97 "JSONT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < JSONT_first_final)
        o = JSONTBAD;

    return o;
}
