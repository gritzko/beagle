
/* #line 1 "PWST.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "PWST.h"

ok64 PWSTonComment (u8cs tok, PWSTstate* state);
ok64 PWSTonString (u8cs tok, PWSTstate* state);
ok64 PWSTonNumber (u8cs tok, PWSTstate* state);
ok64 PWSTonVar (u8cs tok, PWSTstate* state);
ok64 PWSTonWord (u8cs tok, PWSTstate* state);
ok64 PWSTonPunct (u8cs tok, PWSTstate* state);
ok64 PWSTonSpace (u8cs tok, PWSTstate* state);


/* #line 116 "PWST.c.rl" */



/* #line 16 "PWST.rl.c" */
static const char _PWST_actions[] = {
	0, 1, 2, 1, 3, 1, 7, 1, 
	8, 1, 9, 1, 10, 1, 11, 1, 
	12, 1, 13, 1, 14, 1, 15, 1, 
	16, 1, 17, 1, 18, 1, 19, 1, 
	20, 1, 21, 1, 22, 1, 23, 1, 
	24, 1, 25, 1, 26, 1, 27, 1, 
	28, 1, 29, 1, 30, 1, 31, 1, 
	32, 1, 33, 1, 34, 2, 0, 1, 
	2, 3, 4, 2, 3, 5, 2, 3, 
	6
};

static const unsigned char _PWST_key_offsets[] = {
	0, 0, 3, 3, 9, 10, 11, 12, 
	13, 16, 17, 18, 19, 20, 21, 22, 
	23, 24, 25, 26, 28, 29, 30, 31, 
	34, 35, 36, 37, 38, 39, 40, 42, 
	43, 44, 45, 46, 47, 48, 49, 50, 
	51, 52, 53, 57, 59, 61, 65, 67, 
	71, 73, 79, 80, 82, 83, 84, 85, 
	86, 91, 119, 122, 123, 124, 131, 132, 
	133, 135, 149, 152, 154, 158, 160, 167, 
	171, 173, 178, 180, 186, 188, 190, 192, 
	200
};

static const unsigned char _PWST_trans_keys[] = {
	34u, 92u, 96u, 95u, 123u, 65u, 90u, 97u, 
	122u, 125u, 39u, 110u, 100u, 97u, 111u, 120u, 
	114u, 111u, 111u, 110u, 116u, 97u, 105u, 110u, 
	115u, 113u, 101u, 116u, 111u, 105u, 110u, 101u, 
	105u, 116u, 107u, 101u, 97u, 116u, 99u, 104u, 
	101u, 111u, 116u, 105u, 101u, 112u, 108u, 97u, 
	99u, 112u, 108u, 105u, 116u, 43u, 45u, 48u, 
	57u, 48u, 57u, 48u, 57u, 43u, 45u, 48u, 
	57u, 48u, 57u, 43u, 45u, 48u, 57u, 48u, 
	57u, 48u, 57u, 65u, 70u, 97u, 102u, 35u, 
	35u, 62u, 34u, 64u, 39u, 64u, 95u, 65u, 
	90u, 97u, 122u, 32u, 34u, 35u, 36u, 38u, 
	39u, 42u, 43u, 45u, 46u, 47u, 48u, 60u, 
	64u, 95u, 124u, 9u, 13u, 33u, 37u, 49u, 
	57u, 61u, 62u, 65u, 90u, 97u, 122u, 32u, 
	9u, 13u, 61u, 10u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 38u, 39u, 43u, 61u, 45u, 
	61u, 97u, 98u, 99u, 101u, 103u, 106u, 108u, 
	109u, 110u, 111u, 114u, 115u, 99u, 108u, 109u, 
	48u, 57u, 69u, 101u, 48u, 57u, 48u, 57u, 
	46u, 69u, 88u, 101u, 120u, 48u, 57u, 69u, 
	101u, 48u, 57u, 48u, 57u, 46u, 69u, 101u, 
	48u, 57u, 48u, 57u, 48u, 57u, 65u, 70u, 
	97u, 102u, 35u, 61u, 61u, 62u, 34u, 39u, 
	45u, 95u, 48u, 57u, 65u, 90u, 97u, 122u, 
	124u, 0
};

static const char _PWST_single_lengths[] = {
	0, 3, 0, 2, 1, 1, 1, 1, 
	3, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 2, 1, 1, 1, 3, 
	1, 1, 1, 1, 1, 1, 2, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 2, 0, 0, 2, 0, 2, 
	0, 0, 1, 2, 1, 1, 1, 1, 
	1, 16, 1, 1, 1, 1, 1, 1, 
	2, 14, 3, 0, 2, 0, 5, 2, 
	0, 3, 0, 0, 2, 0, 2, 2, 
	1
};

static const char _PWST_range_lengths[] = {
	0, 0, 0, 2, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 1, 1, 1, 1, 1, 1, 
	1, 3, 0, 0, 0, 0, 0, 0, 
	2, 6, 1, 0, 0, 3, 0, 0, 
	0, 0, 0, 1, 1, 1, 1, 1, 
	1, 1, 1, 3, 0, 1, 0, 3, 
	0
};

static const short _PWST_index_offsets[] = {
	0, 0, 4, 5, 10, 12, 14, 16, 
	18, 22, 24, 26, 28, 30, 32, 34, 
	36, 38, 40, 42, 45, 47, 49, 51, 
	55, 57, 59, 61, 63, 65, 67, 70, 
	72, 74, 76, 78, 80, 82, 84, 86, 
	88, 90, 92, 96, 98, 100, 104, 106, 
	110, 112, 116, 118, 121, 123, 125, 127, 
	129, 133, 156, 159, 161, 163, 168, 170, 
	172, 175, 190, 194, 196, 200, 202, 209, 
	213, 215, 220, 222, 226, 229, 231, 234, 
	240
};

static const char _PWST_indicies[] = {
	1, 2, 2, 0, 0, 3, 5, 3, 
	3, 4, 6, 5, 9, 8, 11, 10, 
	12, 10, 13, 14, 15, 10, 12, 10, 
	14, 10, 16, 7, 17, 7, 18, 7, 
	19, 7, 20, 7, 21, 7, 12, 7, 
	12, 10, 12, 12, 10, 22, 10, 23, 
	10, 12, 10, 12, 24, 12, 10, 25, 
	7, 12, 7, 26, 7, 27, 7, 28, 
	7, 12, 7, 12, 29, 10, 30, 10, 
	24, 31, 32, 10, 33, 10, 34, 10, 
	35, 10, 25, 10, 36, 10, 37, 10, 
	38, 10, 12, 10, 40, 40, 41, 39, 
	41, 39, 43, 42, 45, 45, 46, 44, 
	46, 44, 47, 47, 48, 42, 48, 42, 
	49, 49, 49, 42, 51, 50, 51, 52, 
	50, 55, 54, 56, 54, 58, 57, 59, 
	57, 61, 61, 61, 60, 63, 0, 65, 
	66, 67, 8, 64, 68, 69, 70, 64, 
	71, 73, 75, 61, 76, 63, 64, 72, 
	74, 61, 61, 62, 63, 63, 77, 12, 
	78, 79, 65, 3, 3, 3, 3, 80, 
	12, 78, 8, 81, 12, 12, 78, 12, 
	12, 13, 82, 83, 84, 85, 86, 87, 
	88, 89, 14, 90, 91, 78, 83, 93, 
	88, 92, 95, 94, 97, 97, 95, 96, 
	41, 96, 99, 100, 101, 100, 101, 72, 
	98, 103, 103, 43, 102, 46, 102, 99, 
	100, 100, 72, 98, 48, 104, 49, 49, 
	49, 105, 50, 12, 78, 12, 78, 54, 
	57, 94, 107, 61, 61, 61, 61, 106, 
	12, 92, 0
};

static const char _PWST_trans_targs[] = {
	1, 57, 2, 61, 0, 4, 57, 57, 
	5, 63, 57, 7, 57, 6, 9, 10, 
	12, 13, 14, 15, 16, 17, 21, 22, 
	24, 25, 27, 28, 29, 31, 66, 57, 
	34, 35, 36, 37, 39, 40, 41, 57, 
	43, 69, 57, 71, 57, 46, 72, 48, 
	74, 75, 50, 51, 57, 57, 52, 53, 
	57, 54, 55, 57, 57, 79, 57, 58, 
	59, 60, 3, 62, 64, 65, 67, 70, 
	73, 76, 77, 78, 80, 57, 57, 57, 
	57, 57, 8, 11, 18, 19, 20, 23, 
	26, 30, 33, 38, 57, 32, 57, 68, 
	57, 42, 57, 44, 47, 49, 57, 45, 
	57, 57, 57, 56
};

static const char _PWST_trans_actions[] = {
	0, 11, 0, 0, 0, 0, 13, 59, 
	0, 64, 55, 0, 15, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 67, 53, 
	0, 0, 0, 0, 0, 0, 0, 47, 
	0, 0, 49, 3, 45, 0, 0, 0, 
	0, 0, 0, 0, 5, 57, 0, 0, 
	7, 0, 0, 9, 51, 3, 17, 0, 
	0, 0, 0, 0, 0, 70, 0, 3, 
	3, 3, 0, 3, 0, 43, 39, 19, 
	23, 21, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 37, 0, 41, 3, 
	29, 0, 33, 0, 0, 0, 27, 0, 
	31, 25, 35, 0
};

static const char _PWST_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 61, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const char _PWST_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const short _PWST_eof_trans[] = {
	0, 0, 0, 0, 0, 8, 11, 11, 
	11, 11, 11, 8, 8, 8, 8, 8, 
	8, 8, 11, 11, 11, 11, 11, 11, 
	8, 8, 8, 8, 8, 8, 11, 11, 
	32, 11, 11, 11, 11, 11, 11, 11, 
	11, 11, 40, 40, 43, 45, 45, 43, 
	43, 43, 11, 11, 54, 54, 54, 54, 
	61, 0, 78, 79, 80, 81, 79, 82, 
	79, 79, 93, 95, 97, 97, 99, 103, 
	103, 99, 105, 106, 79, 79, 95, 107, 
	93
};

static const int PWST_start = 57;
static const int PWST_first_final = 57;
static const int PWST_error = 0;

static const int PWST_en_main = 57;


/* #line 119 "PWST.c.rl" */

ok64 PWSTLexer(PWSTstate* state) {

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

    
/* #line 247 "PWST.rl.c" */
	{
	cs = PWST_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 137 "PWST.c.rl" */
    
/* #line 253 "PWST.rl.c" */
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
	_acts = _PWST_actions + _PWST_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 2:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 272 "PWST.rl.c" */
		}
	}

	_keys = _PWST_trans_keys + _PWST_key_offsets[cs];
	_trans = _PWST_index_offsets[cs];

	_klen = _PWST_single_lengths[cs];
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

	_klen = _PWST_range_lengths[cs];
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
	_trans = _PWST_indicies[_trans];
_eof_trans:
	cs = _PWST_trans_targs[_trans];

	if ( _PWST_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _PWST_actions + _PWST_trans_actions[_trans];
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
/* #line 32 "PWST.c.rl" */
	{act = 6;}
	break;
	case 5:
/* #line 56 "PWST.c.rl" */
	{act = 15;}
	break;
	case 6:
/* #line 56 "PWST.c.rl" */
	{act = 16;}
	break;
	case 7:
/* #line 26 "PWST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 32 "PWST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 32 "PWST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 32 "PWST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 44 "PWST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonVar(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 56 "PWST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 56 "PWST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 26 "PWST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 32 "PWST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 44 "PWST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonVar(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 38 "PWST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 38 "PWST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 38 "PWST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 38 "PWST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 38 "PWST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 50 "PWST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 56 "PWST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 56 "PWST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 56 "PWST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 62 "PWST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 38 "PWST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 38 "PWST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 38 "PWST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 50 "PWST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 56 "PWST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 56 "PWST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 56 "PWST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 0:
	{{cs = 0;goto _again;}}
	break;
	case 6:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 15:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 16:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 597 "PWST.rl.c" */
		}
	}

_again:
	_acts = _PWST_actions + _PWST_to_state_actions[cs];
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
/* #line 611 "PWST.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _PWST_eof_trans[cs] > 0 ) {
		_trans = _PWST_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 138 "PWST.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < PWST_first_final)
        o = PWSTBAD;

    return o;
}
