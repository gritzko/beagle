
/* #line 1 "FREE.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "FREE.h"


/* #line 75 "FREE.c.rl" */



/* #line 8 "FREE.rl.c" */
static const char _FREE_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	5, 1, 6, 1, 7, 1, 8, 1, 
	9, 1, 10, 1, 11, 1, 12, 1, 
	13, 1, 14, 1, 15, 2, 2, 3, 
	2, 2, 4
};

static const char _FREE_key_offsets[] = {
	0, 6, 8, 29, 33, 35, 40, 42, 
	45, 51, 65, 67
};

static const unsigned char _FREE_trans_keys[] = {
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	10u, 32u, 46u, 48u, 96u, 0u, 8u, 9u, 
	13u, 14u, 47u, 49u, 57u, 58u, 64u, 65u, 
	90u, 91u, 94u, 123u, 127u, 9u, 32u, 11u, 
	13u, 48u, 57u, 46u, 88u, 120u, 48u, 57u, 
	48u, 57u, 46u, 48u, 57u, 48u, 57u, 65u, 
	70u, 97u, 102u, 45u, 95u, 0u, 47u, 48u, 
	57u, 58u, 64u, 65u, 90u, 91u, 96u, 123u, 
	127u, 48u, 57u, 96u, 0u, 47u, 58u, 64u, 
	91u, 94u, 123u, 127u, 0
};

static const char _FREE_single_lengths[] = {
	0, 0, 5, 2, 0, 3, 0, 1, 
	0, 2, 0, 1
};

static const char _FREE_range_lengths[] = {
	3, 1, 8, 1, 1, 1, 1, 1, 
	3, 6, 1, 4
};

static const char _FREE_index_offsets[] = {
	0, 4, 6, 20, 24, 26, 31, 33, 
	36, 40, 49, 51
};

static const char _FREE_indicies[] = {
	1, 1, 1, 0, 3, 2, 6, 5, 
	7, 8, 4, 4, 5, 4, 9, 4, 
	10, 4, 4, 11, 5, 5, 5, 12, 
	14, 13, 16, 17, 17, 9, 15, 16, 
	18, 16, 9, 15, 1, 1, 1, 19, 
	21, 10, 20, 10, 20, 10, 20, 20, 
	11, 3, 22, 20, 20, 20, 20, 20, 
	11, 0
};

static const char _FREE_trans_targs[] = {
	2, 8, 2, 10, 2, 3, 2, 4, 
	5, 7, 9, 11, 2, 2, 4, 2, 
	6, 0, 2, 2, 2, 1, 2
};

static const char _FREE_trans_actions[] = {
	23, 0, 25, 0, 9, 0, 7, 32, 
	5, 0, 5, 0, 21, 27, 29, 17, 
	0, 0, 15, 13, 19, 0, 11
};

static const char _FREE_to_state_actions[] = {
	0, 0, 1, 0, 0, 0, 0, 0, 
	0, 0, 0, 0
};

static const char _FREE_from_state_actions[] = {
	0, 0, 3, 0, 0, 0, 0, 0, 
	0, 0, 0, 0
};

static const char _FREE_eof_trans[] = {
	1, 3, 0, 13, 14, 16, 19, 16, 
	20, 21, 23, 21
};

static const int FREE_start = 2;
static const int FREE_first_final = 2;
static const int FREE_error = -1;

static const int FREE_en_main = 2;


/* #line 78 "FREE.c.rl" */

ok64 FREELexer(FREEstate* state) {

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

    
/* #line 110 "FREE.rl.c" */
	{
	cs = FREE_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 96 "FREE.c.rl" */
    
/* #line 116 "FREE.rl.c" */
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( p == pe )
		goto _test_eof;
_resume:
	_acts = _FREE_actions + _FREE_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 133 "FREE.rl.c" */
		}
	}

	_keys = _FREE_trans_keys + _FREE_key_offsets[cs];
	_trans = _FREE_index_offsets[cs];

	_klen = _FREE_single_lengths[cs];
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

	_klen = _FREE_range_lengths[cs];
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
	_trans = _FREE_indicies[_trans];
_eof_trans:
	cs = _FREE_trans_targs[_trans];

	if ( _FREE_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _FREE_actions + _FREE_trans_actions[_trans];
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
/* #line 34 "FREE.c.rl" */
	{act = 4;}
	break;
	case 4:
/* #line 39 "FREE.c.rl" */
	{act = 9;}
	break;
	case 5:
/* #line 44 "FREE.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('W', tok, state->ctx); if (o!=OK) {p++; goto _out; } }
}}
	break;
	case 6:
/* #line 39 "FREE.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('P', tok, state->ctx); if (o!=OK) {p++; goto _out; } }
}}
	break;
	case 7:
/* #line 29 "FREE.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('F', tok, state->ctx); if (o!=OK) {p++; goto _out; } }
}}
	break;
	case 8:
/* #line 34 "FREE.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('L', tok, state->ctx); if (o!=OK) {p++; goto _out; } }
}}
	break;
	case 9:
/* #line 34 "FREE.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('L', tok, state->ctx); if (o!=OK) {p++; goto _out; } }
}}
	break;
	case 10:
/* #line 34 "FREE.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('L', tok, state->ctx); if (o!=OK) {p++; goto _out; } }
}}
	break;
	case 11:
/* #line 24 "FREE.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('S', tok, state->ctx); if (o!=OK) {p++; goto _out; } }
}}
	break;
	case 12:
/* #line 44 "FREE.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('W', tok, state->ctx); if (o!=OK) {p++; goto _out; } }
}}
	break;
	case 13:
/* #line 34 "FREE.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('L', tok, state->ctx); if (o!=OK) {p++; goto _out; } }
}}
	break;
	case 14:
/* #line 24 "FREE.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('S', tok, state->ctx); if (o!=OK) {p++; goto _out; } }
}}
	break;
	case 15:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 4:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('L', tok, state->ctx); if (o!=OK) {p++; goto _out; } }
}
	break;
	case 9:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('P', tok, state->ctx); if (o!=OK) {p++; goto _out; } }
}
	break;
	}
	}
	break;
/* #line 296 "FREE.rl.c" */
		}
	}

_again:
	_acts = _FREE_actions + _FREE_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 307 "FREE.rl.c" */
		}
	}

	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _FREE_eof_trans[cs] > 0 ) {
		_trans = _FREE_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 97 "FREE.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < FREE_first_final)
        o = FREEBAD;

    return o;
}
