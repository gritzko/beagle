
/* #line 1 "AGDT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "AGDT.h"

ok64 AGDTonComment (u8cs tok, AGDTstate* state);
ok64 AGDTonString (u8cs tok, AGDTstate* state);
ok64 AGDTonNumber (u8cs tok, AGDTstate* state);
ok64 AGDTonPragma (u8cs tok, AGDTstate* state);
ok64 AGDTonWord (u8cs tok, AGDTstate* state);
ok64 AGDTonPunct (u8cs tok, AGDTstate* state);
ok64 AGDTonSpace (u8cs tok, AGDTstate* state);


/* #line 111 "AGDT.c.rl" */



/* #line 16 "AGDT.rl.c" */
static const char _AGDT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19, 1, 20, 1, 21
};

static const unsigned char _AGDT_key_offsets[] = {
	0, 0, 2, 16, 22, 24, 25, 39, 
	42, 48, 55, 57, 59, 63, 65, 69, 
	71, 77, 78, 80, 81, 82, 84, 85, 
	86, 88, 89, 90, 91, 125, 128, 129, 
	130, 137, 141, 143, 148, 150, 156, 157, 
	158, 171, 172
};

static const unsigned char _AGDT_trans_keys[] = {
	34u, 92u, 34u, 39u, 92u, 110u, 114u, 116u, 
	118u, 120u, 48u, 57u, 97u, 98u, 101u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 39u, 92u, 
	39u, 34u, 39u, 92u, 110u, 114u, 116u, 118u, 
	120u, 48u, 57u, 97u, 98u, 101u, 102u, 39u, 
	48u, 57u, 48u, 57u, 65u, 70u, 97u, 102u, 
	39u, 48u, 57u, 65u, 70u, 97u, 102u, 45u, 
	62u, 48u, 57u, 43u, 45u, 48u, 57u, 48u, 
	57u, 43u, 45u, 48u, 57u, 48u, 57u, 48u, 
	57u, 65u, 70u, 97u, 102u, 45u, 35u, 45u, 
	45u, 125u, 35u, 45u, 45u, 125u, 35u, 125u, 
	35u, 45u, 125u, 32u, 33u, 34u, 39u, 45u, 
	46u, 48u, 58u, 60u, 61u, 62u, 94u, 96u, 
	123u, 0u, 8u, 9u, 13u, 14u, 38u, 40u, 
	41u, 42u, 47u, 49u, 57u, 59u, 64u, 91u, 
	93u, 124u, 125u, 126u, 127u, 32u, 9u, 13u, 
	10u, 46u, 46u, 69u, 88u, 101u, 120u, 48u, 
	57u, 69u, 101u, 48u, 57u, 48u, 57u, 46u, 
	69u, 101u, 48u, 57u, 48u, 57u, 48u, 57u, 
	65u, 70u, 97u, 102u, 58u, 62u, 96u, 0u, 
	38u, 40u, 44u, 46u, 47u, 58u, 64u, 91u, 
	94u, 123u, 127u, 45u, 35u, 0
};

static const char _AGDT_single_lengths[] = {
	0, 2, 8, 0, 2, 1, 8, 1, 
	0, 1, 2, 0, 2, 0, 2, 0, 
	0, 1, 2, 1, 1, 2, 1, 1, 
	2, 1, 1, 1, 14, 1, 1, 1, 
	5, 2, 0, 3, 0, 0, 1, 1, 
	1, 1, 1
};

static const char _AGDT_range_lengths[] = {
	0, 0, 3, 3, 0, 0, 3, 1, 
	3, 3, 0, 1, 1, 1, 1, 1, 
	3, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 10, 1, 0, 0, 
	1, 1, 1, 1, 1, 3, 0, 0, 
	6, 0, 0
};

static const unsigned char _AGDT_index_offsets[] = {
	0, 0, 3, 15, 19, 22, 24, 36, 
	39, 43, 48, 51, 53, 57, 59, 63, 
	65, 69, 71, 74, 76, 78, 81, 83, 
	85, 88, 90, 92, 94, 119, 122, 124, 
	126, 133, 137, 139, 144, 146, 150, 152, 
	154, 162, 164
};

static const char _AGDT_indicies[] = {
	1, 2, 0, 0, 0, 0, 0, 0, 
	0, 0, 4, 0, 0, 0, 3, 0, 
	0, 0, 3, 3, 6, 5, 7, 3, 
	5, 5, 5, 5, 5, 5, 5, 9, 
	8, 5, 5, 3, 7, 8, 3, 10, 
	10, 10, 3, 7, 10, 10, 10, 3, 
	11, 12, 3, 14, 13, 16, 16, 17, 
	15, 17, 15, 18, 18, 19, 13, 19, 
	13, 20, 20, 20, 13, 12, 3, 23, 
	24, 22, 24, 22, 25, 22, 26, 27, 
	23, 28, 23, 29, 23, 26, 30, 23, 
	33, 32, 34, 32, 29, 32, 35, 36, 
	0, 37, 38, 39, 40, 42, 43, 44, 
	3, 3, 3, 46, 3, 35, 3, 36, 
	3, 41, 36, 36, 36, 3, 45, 35, 
	35, 47, 48, 11, 12, 49, 51, 52, 
	53, 52, 53, 41, 50, 55, 55, 14, 
	54, 17, 54, 51, 52, 52, 41, 50, 
	19, 56, 20, 20, 20, 57, 12, 49, 
	12, 49, 58, 58, 58, 58, 58, 58, 
	58, 45, 59, 49, 33, 32, 0
};

static const char _AGDT_trans_targs[] = {
	1, 28, 2, 0, 3, 5, 6, 28, 
	7, 8, 9, 30, 28, 28, 33, 28, 
	13, 34, 15, 36, 37, 28, 19, 21, 
	20, 28, 22, 24, 23, 28, 42, 28, 
	25, 26, 27, 29, 28, 4, 10, 31, 
	32, 35, 38, 17, 39, 40, 41, 28, 
	28, 28, 28, 11, 14, 16, 28, 12, 
	28, 28, 28, 18, 28
};

static const char _AGDT_trans_actions[] = {
	0, 11, 0, 0, 0, 0, 0, 13, 
	0, 0, 0, 0, 17, 41, 5, 39, 
	0, 0, 0, 0, 0, 43, 0, 0, 
	0, 9, 0, 0, 0, 7, 5, 37, 
	0, 0, 0, 0, 15, 0, 0, 0, 
	5, 5, 0, 0, 0, 0, 5, 35, 
	21, 33, 29, 0, 0, 0, 25, 0, 
	27, 23, 31, 0, 19
};

static const char _AGDT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 1, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const char _AGDT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 3, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const unsigned char _AGDT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 14, 16, 16, 14, 14, 
	14, 0, 22, 22, 22, 22, 22, 22, 
	22, 32, 32, 32, 0, 48, 49, 50, 
	51, 55, 55, 51, 57, 58, 50, 50, 
	59, 50, 61
};

static const int AGDT_start = 28;
static const int AGDT_first_final = 28;
static const int AGDT_error = 0;

static const int AGDT_en_main = 28;


/* #line 114 "AGDT.c.rl" */

ok64 AGDTLexer(AGDTstate* state) {

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

    
/* #line 182 "AGDT.rl.c" */
	{
	cs = AGDT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 132 "AGDT.c.rl" */
    
/* #line 188 "AGDT.rl.c" */
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
	_acts = _AGDT_actions + _AGDT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 207 "AGDT.rl.c" */
		}
	}

	_keys = _AGDT_trans_keys + _AGDT_key_offsets[cs];
	_trans = _AGDT_index_offsets[cs];

	_klen = _AGDT_single_lengths[cs];
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

	_klen = _AGDT_range_lengths[cs];
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
	_trans = _AGDT_indicies[_trans];
_eof_trans:
	cs = _AGDT_trans_targs[_trans];

	if ( _AGDT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _AGDT_actions + _AGDT_trans_actions[_trans];
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
/* #line 51 "AGDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonPragma(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 4:
/* #line 33 "AGDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 5:
/* #line 39 "AGDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 39 "AGDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 63 "AGDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 63 "AGDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 33 "AGDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 33 "AGDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 45 "AGDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 45 "AGDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 45 "AGDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 45 "AGDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 57 "AGDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 63 "AGDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 69 "AGDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 33 "AGDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 45 "AGDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 45 "AGDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 63 "AGDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
/* #line 427 "AGDT.rl.c" */
		}
	}

_again:
	_acts = _AGDT_actions + _AGDT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 438 "AGDT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _AGDT_eof_trans[cs] > 0 ) {
		_trans = _AGDT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 133 "AGDT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < AGDT_first_final)
        o = AGDTBAD;

    return o;
}
