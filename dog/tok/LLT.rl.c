
/* #line 1 "LLT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "LLT.h"

// user functions (callbacks) for the parser
ok64 LLTonComment (u8cs tok, LLTstate* state);
ok64 LLTonString (u8cs tok, LLTstate* state);
ok64 LLTonNumber (u8cs tok, LLTstate* state);
ok64 LLTonWord (u8cs tok, LLTstate* state);
ok64 LLTonPunct (u8cs tok, LLTstate* state);
ok64 LLTonSpace (u8cs tok, LLTstate* state);


/* #line 99 "LLT.c.rl" */



/* #line 16 "LLT.rl.c" */
static const char _LLT_actions[] = {
	0, 1, 2, 1, 3, 1, 5, 1, 
	6, 1, 7, 1, 8, 1, 9, 1, 
	10, 1, 11, 1, 12, 1, 13, 1, 
	14, 1, 15, 1, 16, 1, 17, 1, 
	18, 1, 19, 2, 0, 1, 2, 3, 
	4
};

static const char _LLT_key_offsets[] = {
	0, 0, 2, 2, 3, 7, 9, 18, 
	24, 43, 46, 55, 56, 61, 65, 67, 
	70, 76, 77
};

static const unsigned char _LLT_trans_keys[] = {
	34u, 92u, 46u, 43u, 45u, 48u, 57u, 48u, 
	57u, 72u, 48u, 57u, 65u, 70u, 75u, 77u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	32u, 34u, 36u, 46u, 48u, 59u, 64u, 95u, 
	99u, 9u, 13u, 33u, 37u, 49u, 57u, 65u, 
	90u, 97u, 122u, 32u, 9u, 13u, 36u, 46u, 
	95u, 48u, 57u, 65u, 90u, 97u, 122u, 46u, 
	46u, 88u, 120u, 48u, 57u, 69u, 101u, 48u, 
	57u, 48u, 57u, 46u, 48u, 57u, 48u, 57u, 
	65u, 70u, 97u, 102u, 10u, 34u, 36u, 46u, 
	95u, 48u, 57u, 65u, 90u, 97u, 122u, 0
};

static const char _LLT_single_lengths[] = {
	0, 2, 0, 1, 2, 0, 1, 0, 
	9, 1, 3, 1, 3, 2, 0, 1, 
	0, 1, 4
};

static const char _LLT_range_lengths[] = {
	0, 0, 0, 0, 1, 1, 4, 3, 
	5, 1, 3, 0, 1, 1, 1, 1, 
	3, 0, 3
};

static const char _LLT_index_offsets[] = {
	0, 0, 3, 4, 6, 10, 12, 18, 
	22, 37, 40, 47, 49, 54, 58, 60, 
	63, 67, 69
};

static const char _LLT_indicies[] = {
	2, 3, 1, 1, 5, 4, 7, 7, 
	8, 6, 8, 6, 11, 10, 10, 11, 
	10, 9, 10, 10, 10, 9, 13, 1, 
	15, 16, 17, 19, 14, 15, 20, 13, 
	14, 18, 15, 15, 12, 13, 13, 21, 
	15, 15, 15, 15, 15, 15, 22, 24, 
	23, 26, 27, 27, 18, 25, 29, 29, 
	26, 28, 8, 28, 26, 18, 25, 10, 
	10, 10, 30, 31, 19, 1, 15, 15, 
	15, 15, 15, 15, 22, 0
};

static const char _LLT_trans_targs[] = {
	8, 1, 8, 2, 8, 8, 8, 5, 
	14, 8, 16, 7, 8, 9, 8, 10, 
	11, 12, 15, 17, 18, 8, 8, 8, 
	3, 8, 13, 6, 8, 4, 8, 8
};

static const char _LLT_trans_actions[] = {
	33, 0, 5, 0, 31, 9, 29, 0, 
	0, 27, 0, 0, 11, 0, 7, 0, 
	3, 3, 0, 0, 38, 25, 21, 23, 
	0, 17, 3, 0, 19, 0, 15, 13
};

static const char _LLT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	35, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const char _LLT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	1, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const char _LLT_eof_trans[] = {
	0, 1, 1, 5, 7, 7, 10, 10, 
	0, 22, 23, 24, 26, 29, 29, 26, 
	31, 32, 23
};

static const int LLT_start = 8;
static const int LLT_first_final = 8;
static const int LLT_error = 0;

static const int LLT_en_main = 8;


/* #line 102 "LLT.c.rl" */

// the public API function
ok64 LLTLexer(LLTstate* state) {

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

    
/* #line 132 "LLT.rl.c" */
	{
	cs = LLT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 121 "LLT.c.rl" */
    
/* #line 138 "LLT.rl.c" */
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
	_acts = _LLT_actions + _LLT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 2:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 157 "LLT.rl.c" */
		}
	}

	_keys = _LLT_trans_keys + _LLT_key_offsets[cs];
	_trans = _LLT_index_offsets[cs];

	_klen = _LLT_single_lengths[cs];
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

	_klen = _LLT_range_lengths[cs];
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
	_trans = _LLT_indicies[_trans];
_eof_trans:
	cs = _LLT_trans_targs[_trans];

	if ( _LLT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _LLT_actions + _LLT_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 3:
/* #line 1 "NONE" */
	{te = p+1;}
	break;
	case 4:
/* #line 47 "LLT.c.rl" */
	{act = 7;}
	break;
	case 5:
/* #line 35 "LLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 53 "LLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 53 "LLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 53 "LLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 29 "LLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 41 "LLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 41 "LLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 41 "LLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 47 "LLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 53 "LLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 59 "LLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 41 "LLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 41 "LLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 53 "LLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 0:
	{{cs = 0;goto _again;}}
	break;
	case 7:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 356 "LLT.rl.c" */
		}
	}

_again:
	_acts = _LLT_actions + _LLT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
	case 1:
/* #line 1 "NONE" */
	{act = 0;}
	break;
/* #line 370 "LLT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _LLT_eof_trans[cs] > 0 ) {
		_trans = _LLT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 122 "LLT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < LLT_first_final)
        o = LLTBAD;

    return o;
}
