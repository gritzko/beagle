
/* #line 1 "TYST.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "TYST.h"

ok64 TYSTonComment (u8cs tok, TYSTstate* state);
ok64 TYSTonString (u8cs tok, TYSTstate* state);
ok64 TYSTonNumber (u8cs tok, TYSTstate* state);
ok64 TYSTonMath (u8cs tok, TYSTstate* state);
ok64 TYSTonLabel (u8cs tok, TYSTstate* state);
ok64 TYSTonPreproc (u8cs tok, TYSTstate* state);
ok64 TYSTonWord (u8cs tok, TYSTstate* state);
ok64 TYSTonPunct (u8cs tok, TYSTstate* state);
ok64 TYSTonSpace (u8cs tok, TYSTstate* state);


/* #line 129 "TYST.c.rl" */



/* #line 18 "TYST.rl.c" */
static const char _TYST_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19, 1, 20, 1, 21, 1, 22, 1, 
	23, 1, 24, 1, 25
};

static const unsigned char _TYST_key_offsets[] = {
	0, 0, 2, 16, 22, 28, 34, 40, 
	41, 42, 43, 44, 45, 48, 49, 50, 
	51, 52, 53, 54, 55, 56, 57, 59, 
	60, 61, 62, 63, 64, 66, 66, 67, 
	68, 69, 70, 72, 74, 75, 76, 77, 
	78, 79, 80, 86, 95, 117, 120, 121, 
	127, 129, 132, 139, 142, 143, 151, 158, 
	160
};

static const unsigned char _TYST_trans_keys[] = {
	34u, 92u, 34u, 39u, 48u, 92u, 110u, 114u, 
	117u, 120u, 97u, 98u, 101u, 102u, 116u, 118u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	108u, 115u, 101u, 111u, 114u, 102u, 109u, 110u, 
	112u, 111u, 114u, 116u, 99u, 108u, 117u, 100u, 
	101u, 101u, 104u, 111u, 119u, 104u, 105u, 108u, 
	36u, 92u, 109u, 110u, 116u, 42u, 42u, 47u, 
	48u, 57u, 109u, 110u, 116u, 109u, 110u, 116u, 
	61u, 95u, 65u, 90u, 97u, 122u, 45u, 62u, 
	95u, 48u, 57u, 65u, 90u, 97u, 122u, 32u, 
	33u, 34u, 35u, 36u, 45u, 46u, 47u, 60u, 
	61u, 62u, 95u, 9u, 13u, 42u, 43u, 48u, 
	57u, 65u, 90u, 97u, 122u, 32u, 9u, 13u, 
	61u, 101u, 102u, 105u, 108u, 115u, 119u, 36u, 
	92u, 46u, 48u, 57u, 99u, 101u, 105u, 109u, 
	112u, 48u, 57u, 42u, 47u, 61u, 10u, 46u, 
	99u, 101u, 105u, 109u, 112u, 48u, 57u, 99u, 
	101u, 105u, 109u, 112u, 48u, 57u, 61u, 62u, 
	45u, 95u, 48u, 57u, 65u, 90u, 97u, 122u, 
	0
};

static const char _TYST_single_lengths[] = {
	0, 2, 8, 0, 0, 0, 0, 1, 
	1, 1, 1, 1, 3, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 2, 1, 
	1, 1, 1, 1, 2, 0, 1, 1, 
	1, 1, 2, 0, 1, 1, 1, 1, 
	1, 1, 2, 3, 12, 1, 1, 6, 
	2, 1, 5, 3, 1, 6, 5, 0, 
	2
};

static const char _TYST_range_lengths[] = {
	0, 0, 3, 3, 3, 3, 3, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 0, 0, 0, 0, 
	0, 0, 2, 3, 5, 1, 0, 0, 
	0, 1, 1, 0, 0, 1, 1, 1, 
	3
};

static const unsigned char _TYST_index_offsets[] = {
	0, 0, 3, 15, 19, 23, 27, 31, 
	33, 35, 37, 39, 41, 45, 47, 49, 
	51, 53, 55, 57, 59, 61, 63, 66, 
	68, 70, 72, 74, 76, 79, 80, 82, 
	84, 86, 88, 91, 93, 95, 97, 99, 
	101, 103, 105, 110, 117, 135, 138, 140, 
	147, 150, 153, 160, 164, 166, 174, 181, 
	183
};

static const char _TYST_indicies[] = {
	1, 2, 0, 0, 0, 0, 0, 0, 
	0, 4, 5, 0, 0, 0, 3, 6, 
	6, 6, 3, 5, 5, 5, 3, 7, 
	7, 7, 3, 0, 0, 0, 3, 9, 
	8, 10, 8, 11, 8, 12, 8, 11, 
	8, 11, 13, 14, 8, 15, 8, 16, 
	8, 17, 8, 11, 8, 18, 8, 19, 
	8, 20, 8, 10, 8, 17, 8, 17, 
	21, 8, 22, 8, 11, 8, 23, 8, 
	24, 8, 10, 8, 27, 28, 26, 26, 
	30, 29, 30, 29, 30, 29, 32, 31, 
	32, 33, 31, 35, 34, 37, 36, 37, 
	36, 37, 36, 38, 34, 38, 34, 38, 
	34, 39, 40, 40, 40, 3, 40, 41, 
	40, 40, 40, 40, 3, 43, 44, 0, 
	45, 46, 44, 47, 48, 50, 51, 44, 
	52, 43, 44, 49, 52, 52, 42, 43, 
	43, 53, 39, 54, 56, 57, 58, 59, 
	60, 61, 55, 27, 28, 26, 39, 62, 
	55, 64, 64, 65, 64, 66, 62, 63, 
	31, 67, 39, 54, 68, 67, 70, 71, 
	71, 72, 71, 73, 49, 69, 75, 75, 
	76, 75, 77, 35, 74, 39, 54, 52, 
	52, 52, 52, 52, 78, 0
};

static const char _TYST_trans_targs[] = {
	1, 44, 2, 0, 3, 5, 4, 6, 
	44, 8, 9, 44, 11, 13, 17, 14, 
	15, 16, 18, 19, 20, 23, 24, 26, 
	27, 44, 28, 44, 29, 44, 44, 33, 
	34, 44, 44, 54, 44, 44, 44, 44, 
	43, 44, 44, 45, 46, 47, 48, 49, 
	51, 53, 42, 55, 56, 44, 44, 44, 
	7, 10, 12, 21, 22, 25, 50, 44, 
	30, 31, 32, 52, 44, 44, 35, 39, 
	40, 41, 44, 36, 37, 38, 44
};

static const char _TYST_trans_actions[] = {
	0, 9, 0, 0, 0, 0, 0, 0, 
	51, 0, 0, 15, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 49, 0, 11, 0, 45, 19, 0, 
	0, 7, 47, 5, 43, 17, 21, 23, 
	0, 13, 25, 0, 0, 5, 5, 0, 
	5, 5, 0, 0, 0, 41, 37, 39, 
	0, 0, 0, 0, 0, 0, 5, 31, 
	0, 0, 0, 0, 27, 33, 0, 0, 
	0, 0, 29, 0, 0, 0, 35
};

static const char _TYST_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 1, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const char _TYST_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 3, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const unsigned char _TYST_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 9, 
	9, 9, 9, 9, 9, 9, 9, 9, 
	9, 9, 9, 9, 9, 9, 9, 9, 
	9, 9, 9, 9, 26, 26, 30, 30, 
	30, 26, 26, 35, 37, 37, 37, 35, 
	35, 35, 0, 0, 0, 54, 55, 56, 
	55, 56, 64, 55, 69, 70, 75, 55, 
	79
};

static const int TYST_start = 44;
static const int TYST_first_final = 44;
static const int TYST_error = 0;

static const int TYST_en_main = 44;


/* #line 132 "TYST.c.rl" */

ok64 TYSTLexer(TYSTstate* state) {

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

    
/* #line 206 "TYST.rl.c" */
	{
	cs = TYST_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 150 "TYST.c.rl" */
    
/* #line 212 "TYST.rl.c" */
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
	_acts = _TYST_actions + _TYST_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 231 "TYST.rl.c" */
		}
	}

	_keys = _TYST_trans_keys + _TYST_key_offsets[cs];
	_trans = _TYST_index_offsets[cs];

	_klen = _TYST_single_lengths[cs];
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

	_klen = _TYST_range_lengths[cs];
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
	_trans = _TYST_indicies[_trans];
_eof_trans:
	cs = _TYST_trans_targs[_trans];

	if ( _TYST_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _TYST_actions + _TYST_trans_actions[_trans];
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
/* #line 32 "TYST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 4:
/* #line 38 "TYST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 5:
/* #line 50 "TYST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonMath(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 56 "TYST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonLabel(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 62 "TYST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 44 "TYST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 44 "TYST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 44 "TYST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 74 "TYST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 74 "TYST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 32 "TYST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 44 "TYST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 44 "TYST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 44 "TYST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 68 "TYST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 74 "TYST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 74 "TYST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 80 "TYST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 44 "TYST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 44 "TYST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 44 "TYST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 74 "TYST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 74 "TYST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
/* #line 483 "TYST.rl.c" */
		}
	}

_again:
	_acts = _TYST_actions + _TYST_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 494 "TYST.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _TYST_eof_trans[cs] > 0 ) {
		_trans = _TYST_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 151 "TYST.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < TYST_first_final)
        o = TYSTBAD;

    return o;
}
