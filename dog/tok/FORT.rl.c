
/* #line 1 "FORT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "FORT.h"

ok64 FORTonComment (u8cs tok, FORTstate* state);
ok64 FORTonString (u8cs tok, FORTstate* state);
ok64 FORTonNumber (u8cs tok, FORTstate* state);
ok64 FORTonWord (u8cs tok, FORTstate* state);
ok64 FORTonPunct (u8cs tok, FORTstate* state);
ok64 FORTonSpace (u8cs tok, FORTstate* state);


/* #line 103 "FORT.c.rl" */



/* #line 15 "FORT.rl.c" */
static const char _FORT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19, 1, 20
};

static const char _FORT_key_offsets[] = {
	0, 1, 2, 6, 8, 9, 10, 11, 
	12, 14, 15, 16, 17, 18, 20, 22, 
	24, 25, 26, 27, 28, 29, 33, 35, 
	39, 41, 48, 67, 70, 71, 72, 82, 
	88, 90, 92, 100, 106, 108, 110, 117, 
	118, 119
};

static const unsigned char _FORT_trans_keys[] = {
	34u, 39u, 43u, 45u, 48u, 57u, 48u, 57u, 
	110u, 100u, 46u, 113u, 46u, 118u, 97u, 108u, 
	115u, 101u, 101u, 116u, 101u, 111u, 46u, 113u, 
	118u, 116u, 114u, 114u, 117u, 43u, 45u, 48u, 
	57u, 48u, 57u, 43u, 45u, 48u, 57u, 48u, 
	57u, 95u, 48u, 57u, 65u, 90u, 97u, 122u, 
	32u, 33u, 34u, 39u, 42u, 46u, 47u, 58u, 
	95u, 9u, 13u, 48u, 57u, 60u, 62u, 65u, 
	90u, 97u, 122u, 32u, 9u, 13u, 10u, 42u, 
	97u, 101u, 102u, 103u, 108u, 110u, 111u, 116u, 
	48u, 57u, 48u, 57u, 68u, 69u, 100u, 101u, 
	48u, 57u, 47u, 61u, 46u, 95u, 48u, 57u, 
	68u, 69u, 100u, 101u, 48u, 57u, 68u, 69u, 
	100u, 101u, 48u, 57u, 48u, 57u, 95u, 48u, 
	57u, 65u, 90u, 97u, 122u, 58u, 61u, 95u, 
	48u, 57u, 65u, 90u, 97u, 122u, 0
};

static const char _FORT_single_lengths[] = {
	1, 1, 2, 0, 1, 1, 1, 1, 
	2, 1, 1, 1, 1, 2, 2, 2, 
	1, 1, 1, 1, 1, 2, 0, 2, 
	0, 1, 9, 1, 1, 1, 8, 0, 
	0, 2, 2, 0, 0, 0, 1, 1, 
	1, 1
};

static const char _FORT_range_lengths[] = {
	0, 0, 1, 1, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 1, 1, 1, 
	1, 3, 5, 1, 0, 0, 1, 3, 
	1, 0, 3, 3, 1, 1, 3, 0, 
	0, 3
};

static const unsigned char _FORT_index_offsets[] = {
	0, 2, 4, 8, 10, 12, 14, 16, 
	18, 21, 23, 25, 27, 29, 32, 35, 
	38, 40, 42, 44, 46, 48, 52, 54, 
	58, 60, 65, 80, 83, 85, 87, 97, 
	101, 103, 106, 112, 116, 118, 120, 125, 
	127, 129
};

static const char _FORT_indicies[] = {
	1, 0, 3, 2, 5, 5, 6, 4, 
	6, 4, 8, 7, 9, 7, 10, 7, 
	11, 7, 10, 9, 7, 12, 7, 13, 
	7, 14, 7, 9, 7, 9, 9, 7, 
	15, 16, 7, 10, 17, 7, 9, 7, 
	9, 7, 9, 7, 18, 7, 14, 7, 
	20, 20, 21, 19, 21, 19, 23, 23, 
	24, 22, 24, 22, 25, 25, 25, 25, 
	22, 27, 28, 0, 2, 29, 30, 31, 
	33, 35, 27, 32, 34, 35, 35, 26, 
	27, 27, 36, 37, 28, 10, 38, 41, 
	42, 43, 44, 44, 45, 46, 47, 40, 
	39, 40, 49, 49, 48, 6, 48, 10, 
	10, 38, 51, 53, 32, 52, 52, 50, 
	51, 55, 55, 54, 21, 54, 24, 56, 
	25, 25, 25, 25, 57, 10, 38, 10, 
	38, 35, 35, 35, 35, 58, 0
};

static const char _FORT_trans_targs[] = {
	0, 26, 1, 26, 26, 3, 32, 26, 
	5, 6, 26, 8, 10, 11, 12, 15, 
	17, 16, 20, 26, 22, 36, 26, 24, 
	37, 38, 26, 27, 28, 29, 30, 33, 
	34, 39, 40, 41, 26, 26, 26, 26, 
	31, 4, 7, 9, 13, 14, 18, 19, 
	26, 2, 26, 35, 23, 25, 26, 21, 
	26, 26, 26
};

static const char _FORT_trans_actions[] = {
	0, 7, 0, 9, 37, 0, 0, 41, 
	0, 0, 11, 0, 0, 0, 0, 0, 
	0, 0, 0, 35, 0, 0, 39, 0, 
	0, 0, 13, 0, 0, 0, 5, 0, 
	5, 0, 0, 0, 33, 15, 29, 31, 
	5, 0, 0, 0, 0, 0, 0, 0, 
	19, 0, 25, 5, 0, 0, 17, 0, 
	21, 23, 27
};

static const char _FORT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 1, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const char _FORT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 3, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const unsigned char _FORT_eof_trans[] = {
	0, 0, 5, 5, 8, 8, 8, 8, 
	8, 8, 8, 8, 8, 8, 8, 8, 
	8, 8, 8, 8, 8, 20, 20, 23, 
	23, 23, 0, 37, 38, 39, 40, 49, 
	49, 39, 51, 55, 55, 57, 58, 39, 
	39, 59
};

static const int FORT_start = 26;
static const int FORT_first_final = 26;
static const int FORT_error = -1;

static const int FORT_en_main = 26;


/* #line 106 "FORT.c.rl" */

ok64 FORTLexer(FORTstate* state) {

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

    
/* #line 171 "FORT.rl.c" */
	{
	cs = FORT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 124 "FORT.c.rl" */
    
/* #line 177 "FORT.rl.c" */
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( p == pe )
		goto _test_eof;
_resume:
	_acts = _FORT_actions + _FORT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 194 "FORT.rl.c" */
		}
	}

	_keys = _FORT_trans_keys + _FORT_key_offsets[cs];
	_trans = _FORT_index_offsets[cs];

	_klen = _FORT_single_lengths[cs];
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

	_klen = _FORT_range_lengths[cs];
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
	_trans = _FORT_indicies[_trans];
_eof_trans:
	cs = _FORT_trans_targs[_trans];

	if ( _FORT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _FORT_actions + _FORT_trans_actions[_trans];
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
/* #line 32 "FORT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 4:
/* #line 32 "FORT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 5:
/* #line 50 "FORT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 50 "FORT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 26 "FORT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 38 "FORT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 38 "FORT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 38 "FORT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 38 "FORT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 38 "FORT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 44 "FORT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 50 "FORT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 50 "FORT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 56 "FORT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 38 "FORT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 38 "FORT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 38 "FORT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 50 "FORT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
/* #line 406 "FORT.rl.c" */
		}
	}

_again:
	_acts = _FORT_actions + _FORT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 417 "FORT.rl.c" */
		}
	}

	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _FORT_eof_trans[cs] > 0 ) {
		_trans = _FORT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 125 "FORT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < FORT_first_final)
        o = FORTBAD;

    return o;
}
