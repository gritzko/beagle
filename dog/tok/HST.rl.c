
/* #line 1 "HST.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "HST.h"

ok64 HSTonComment (u8cs tok, HSTstate* state);
ok64 HSTonString (u8cs tok, HSTstate* state);
ok64 HSTonNumber (u8cs tok, HSTstate* state);
ok64 HSTonPragma (u8cs tok, HSTstate* state);
ok64 HSTonWord (u8cs tok, HSTstate* state);
ok64 HSTonPunct (u8cs tok, HSTstate* state);
ok64 HSTonSpace (u8cs tok, HSTstate* state);


/* #line 116 "HST.c.rl" */



/* #line 16 "HST.rl.c" */
static const char _HST_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19, 1, 20, 1, 21
};

static const char _HST_key_offsets[] = {
	0, 0, 2, 2, 4, 5, 5, 7, 
	11, 13, 17, 19, 21, 23, 29, 34, 
	41, 42, 43, 66, 69, 70, 72, 73, 
	74, 75, 86, 90, 92, 97, 99, 101, 
	103, 109, 110, 113, 115, 123, 124
};

static const unsigned char _HST_trans_keys[] = {
	34u, 92u, 39u, 92u, 39u, 48u, 57u, 43u, 
	45u, 48u, 57u, 48u, 57u, 43u, 45u, 48u, 
	57u, 48u, 57u, 48u, 49u, 48u, 55u, 48u, 
	57u, 65u, 70u, 97u, 102u, 95u, 65u, 90u, 
	97u, 122u, 96u, 48u, 57u, 65u, 90u, 95u, 
	122u, 45u, 125u, 32u, 34u, 38u, 39u, 45u, 
	46u, 47u, 48u, 58u, 60u, 96u, 123u, 124u, 
	9u, 13u, 49u, 57u, 61u, 62u, 65u, 90u, 
	95u, 122u, 32u, 9u, 13u, 38u, 45u, 62u, 
	10u, 46u, 61u, 46u, 66u, 69u, 79u, 88u, 
	98u, 101u, 111u, 120u, 48u, 57u, 69u, 101u, 
	48u, 57u, 48u, 57u, 46u, 69u, 101u, 48u, 
	57u, 48u, 57u, 48u, 49u, 48u, 55u, 48u, 
	57u, 65u, 70u, 97u, 102u, 58u, 45u, 60u, 
	61u, 61u, 62u, 39u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 45u, 124u, 0
};

static const char _HST_single_lengths[] = {
	0, 2, 0, 2, 1, 0, 0, 2, 
	0, 2, 0, 0, 0, 0, 1, 1, 
	1, 1, 13, 1, 1, 2, 1, 1, 
	1, 9, 2, 0, 3, 0, 0, 0, 
	0, 1, 1, 0, 2, 1, 1
};

static const char _HST_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 1, 1, 
	1, 1, 1, 1, 1, 3, 2, 3, 
	0, 0, 5, 1, 0, 0, 0, 0, 
	0, 1, 1, 1, 1, 1, 1, 1, 
	3, 0, 1, 1, 3, 0, 0
};

static const unsigned char _HST_index_offsets[] = {
	0, 0, 3, 4, 7, 9, 10, 12, 
	16, 18, 22, 24, 26, 28, 32, 36, 
	41, 43, 45, 64, 67, 69, 72, 74, 
	76, 78, 89, 93, 95, 100, 102, 104, 
	106, 110, 112, 115, 117, 123, 125
};

static const char _HST_indicies[] = {
	1, 2, 0, 0, 4, 5, 3, 6, 
	4, 3, 8, 7, 10, 10, 11, 9, 
	11, 9, 12, 12, 13, 7, 13, 7, 
	14, 7, 15, 7, 16, 16, 16, 7, 
	17, 17, 17, 4, 18, 17, 17, 17, 
	4, 21, 20, 22, 20, 24, 0, 25, 
	26, 27, 28, 29, 30, 32, 33, 36, 
	37, 38, 24, 31, 34, 35, 35, 23, 
	24, 24, 39, 41, 40, 42, 41, 40, 
	43, 42, 41, 40, 41, 40, 45, 46, 
	47, 48, 49, 46, 47, 48, 49, 31, 
	44, 51, 51, 8, 50, 11, 50, 45, 
	47, 47, 31, 44, 13, 52, 14, 53, 
	15, 54, 16, 16, 16, 55, 41, 40, 
	41, 41, 40, 41, 40, 35, 35, 35, 
	35, 35, 56, 20, 40, 41, 40, 0
};

static const char _HST_trans_targs[] = {
	1, 18, 2, 4, 0, 5, 18, 18, 
	26, 18, 8, 27, 10, 29, 30, 31, 
	32, 15, 18, 18, 16, 17, 18, 18, 
	19, 20, 3, 21, 23, 24, 25, 28, 
	33, 34, 35, 36, 14, 37, 38, 18, 
	18, 18, 22, 18, 18, 6, 11, 9, 
	12, 13, 18, 7, 18, 18, 18, 18, 
	18
};

static const char _HST_trans_actions[] = {
	0, 9, 0, 0, 0, 0, 11, 41, 
	5, 39, 0, 0, 0, 0, 0, 0, 
	0, 0, 15, 43, 0, 0, 7, 17, 
	0, 0, 0, 0, 0, 0, 5, 5, 
	0, 0, 0, 0, 0, 5, 0, 37, 
	35, 13, 0, 19, 31, 0, 0, 0, 
	0, 0, 27, 0, 29, 25, 23, 21, 
	33
};

static const char _HST_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 1, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0
};

static const char _HST_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 3, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0
};

static const unsigned char _HST_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 8, 10, 
	10, 8, 8, 8, 8, 8, 0, 0, 
	20, 20, 0, 40, 41, 41, 44, 41, 
	41, 45, 51, 51, 45, 53, 54, 55, 
	56, 41, 41, 41, 57, 41, 41
};

static const int HST_start = 18;
static const int HST_first_final = 18;
static const int HST_error = 0;

static const int HST_en_main = 18;


/* #line 119 "HST.c.rl" */

ok64 HSTLexer(HSTstate* state) {

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

    
/* #line 164 "HST.rl.c" */
	{
	cs = HST_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 137 "HST.c.rl" */
    
/* #line 170 "HST.rl.c" */
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
	_acts = _HST_actions + _HST_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 189 "HST.rl.c" */
		}
	}

	_keys = _HST_trans_keys + _HST_key_offsets[cs];
	_trans = _HST_index_offsets[cs];

	_klen = _HST_single_lengths[cs];
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

	_klen = _HST_range_lengths[cs];
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
	_trans = _HST_indicies[_trans];
_eof_trans:
	cs = _HST_trans_targs[_trans];

	if ( _HST_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _HST_actions + _HST_trans_actions[_trans];
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
/* #line 32 "HST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 4:
/* #line 38 "HST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 5:
/* #line 38 "HST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 62 "HST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 62 "HST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 62 "HST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 32 "HST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 44 "HST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 44 "HST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 44 "HST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 44 "HST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 44 "HST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 44 "HST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 56 "HST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 62 "HST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 68 "HST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 44 "HST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 44 "HST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 62 "HST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
/* #line 409 "HST.rl.c" */
		}
	}

_again:
	_acts = _HST_actions + _HST_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 420 "HST.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _HST_eof_trans[cs] > 0 ) {
		_trans = _HST_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 138 "HST.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < HST_first_final)
        o = HSTBAD;

    return o;
}
