
/* #line 1 "MAKT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "MAKT.h"

ok64 MAKTonComment (u8cs tok, MAKTstate* state);
ok64 MAKTonString (u8cs tok, MAKTstate* state);
ok64 MAKTonNumber (u8cs tok, MAKTstate* state);
ok64 MAKTonVar (u8cs tok, MAKTstate* state);
ok64 MAKTonWord (u8cs tok, MAKTstate* state);
ok64 MAKTonPunct (u8cs tok, MAKTstate* state);
ok64 MAKTonSpace (u8cs tok, MAKTstate* state);


/* #line 104 "MAKT.c.rl" */



/* #line 16 "MAKT.rl.c" */
static const char _MAKT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18
};

static const char _MAKT_key_offsets[] = {
	0, 0, 2, 2, 10, 11, 12, 13, 
	14, 34, 37, 38, 39, 44, 51, 56, 
	63, 65, 67
};

static const unsigned char _MAKT_trans_keys[] = {
	34u, 92u, 37u, 40u, 42u, 60u, 94u, 123u, 
	63u, 64u, 41u, 125u, 39u, 61u, 32u, 33u, 
	34u, 35u, 36u, 39u, 43u, 45u, 46u, 58u, 
	63u, 95u, 9u, 13u, 48u, 57u, 65u, 90u, 
	97u, 122u, 32u, 9u, 13u, 61u, 10u, 95u, 
	65u, 90u, 97u, 122u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 95u, 65u, 90u, 97u, 122u, 
	95u, 48u, 57u, 65u, 90u, 97u, 122u, 48u, 
	57u, 58u, 61u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 0
};

static const char _MAKT_single_lengths[] = {
	0, 2, 0, 6, 1, 1, 1, 1, 
	12, 1, 1, 1, 1, 1, 1, 1, 
	0, 2, 1
};

static const char _MAKT_range_lengths[] = {
	0, 0, 0, 1, 0, 0, 0, 0, 
	4, 1, 0, 0, 2, 3, 2, 3, 
	1, 0, 3
};

static const char _MAKT_index_offsets[] = {
	0, 0, 3, 4, 12, 14, 16, 18, 
	20, 37, 40, 42, 44, 48, 53, 57, 
	62, 64, 67
};

static const char _MAKT_indicies[] = {
	1, 2, 0, 0, 3, 5, 3, 3, 
	3, 6, 3, 4, 7, 5, 8, 6, 
	10, 9, 12, 11, 14, 15, 0, 16, 
	17, 9, 15, 18, 19, 21, 15, 22, 
	14, 20, 22, 22, 13, 14, 14, 23, 
	12, 24, 25, 16, 27, 27, 27, 26, 
	27, 27, 27, 27, 28, 29, 29, 29, 
	26, 29, 29, 29, 29, 30, 20, 31, 
	32, 12, 24, 22, 22, 22, 22, 33, 
	0
};

static const char _MAKT_trans_targs[] = {
	1, 8, 2, 8, 0, 4, 5, 8, 
	8, 6, 8, 8, 8, 8, 9, 10, 
	11, 3, 12, 14, 16, 17, 18, 8, 
	8, 8, 8, 13, 8, 15, 8, 8, 
	7, 8
};

static const char _MAKT_trans_actions[] = {
	0, 7, 0, 15, 0, 0, 0, 11, 
	13, 0, 9, 37, 17, 19, 0, 0, 
	0, 0, 0, 0, 0, 5, 0, 35, 
	31, 21, 33, 0, 29, 0, 25, 23, 
	0, 27
};

static const char _MAKT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	1, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const char _MAKT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	3, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const char _MAKT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 12, 
	0, 24, 25, 26, 27, 29, 27, 31, 
	32, 25, 34
};

static const int MAKT_start = 8;
static const int MAKT_first_final = 8;
static const int MAKT_error = 0;

static const int MAKT_en_main = 8;


/* #line 107 "MAKT.c.rl" */

ok64 MAKTLexer(MAKTstate* state) {

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

    
/* #line 131 "MAKT.rl.c" */
	{
	cs = MAKT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 125 "MAKT.c.rl" */
    
/* #line 137 "MAKT.rl.c" */
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
	_acts = _MAKT_actions + _MAKT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 156 "MAKT.rl.c" */
		}
	}

	_keys = _MAKT_trans_keys + _MAKT_key_offsets[cs];
	_trans = _MAKT_index_offsets[cs];

	_klen = _MAKT_single_lengths[cs];
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

	_klen = _MAKT_range_lengths[cs];
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
	_trans = _MAKT_indicies[_trans];
_eof_trans:
	cs = _MAKT_trans_targs[_trans];

	if ( _MAKT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _MAKT_actions + _MAKT_trans_actions[_trans];
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
/* #line 31 "MAKT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 4:
/* #line 31 "MAKT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 5:
/* #line 43 "MAKT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonVar(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 43 "MAKT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonVar(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 43 "MAKT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonVar(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 55 "MAKT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 55 "MAKT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 25 "MAKT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 37 "MAKT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 49 "MAKT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 49 "MAKT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 49 "MAKT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 55 "MAKT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 55 "MAKT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 61 "MAKT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 55 "MAKT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
/* #line 352 "MAKT.rl.c" */
		}
	}

_again:
	_acts = _MAKT_actions + _MAKT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 363 "MAKT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _MAKT_eof_trans[cs] > 0 ) {
		_trans = _MAKT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 126 "MAKT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < MAKT_first_final)
        o = MAKTBAD;

    return o;
}
