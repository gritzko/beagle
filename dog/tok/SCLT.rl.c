
/* #line 1 "SCLT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "SCLT.h"

ok64 SCLTonComment (u8cs tok, SCLTstate* state);
ok64 SCLTonString (u8cs tok, SCLTstate* state);
ok64 SCLTonNumber (u8cs tok, SCLTstate* state);
ok64 SCLTonAnnotation (u8cs tok, SCLTstate* state);
ok64 SCLTonWord (u8cs tok, SCLTstate* state);
ok64 SCLTonPunct (u8cs tok, SCLTstate* state);
ok64 SCLTonSpace (u8cs tok, SCLTstate* state);


/* #line 134 "SCLT.c.rl" */



/* #line 16 "SCLT.rl.c" */
static const char _SCLT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	9, 1, 10, 1, 11, 1, 12, 1, 
	13, 1, 14, 1, 15, 1, 16, 1, 
	17, 1, 18, 1, 19, 1, 20, 1, 
	21, 1, 22, 1, 23, 1, 24, 1, 
	25, 1, 26, 1, 27, 1, 28, 1, 
	29, 1, 30, 1, 31, 1, 32, 1, 
	33, 1, 34, 1, 35, 1, 36, 1, 
	37, 1, 38, 1, 39, 1, 40, 1, 
	41, 1, 42, 2, 2, 3, 2, 2, 
	4, 2, 2, 5, 2, 2, 6, 2, 
	2, 7, 2, 2, 8
};

static const short _SCLT_key_offsets[] = {
	0, 0, 2, 4, 20, 26, 32, 38, 
	44, 45, 46, 47, 49, 65, 71, 77, 
	83, 89, 93, 95, 97, 98, 100, 104, 
	106, 108, 112, 114, 116, 118, 124, 129, 
	131, 147, 153, 159, 165, 171, 172, 203, 
	206, 207, 208, 210, 212, 215, 217, 226, 
	233, 236, 237, 249, 257, 266, 273, 281, 
	288, 293, 302, 303, 306, 308, 310, 318, 
	325, 333, 342, 350, 358
};

static const unsigned char _SCLT_trans_keys[] = {
	34u, 92u, 34u, 92u, 34u, 36u, 39u, 48u, 
	63u, 92u, 110u, 114u, 117u, 120u, 97u, 98u, 
	101u, 102u, 116u, 118u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 34u, 34u, 34u, 39u, 
	92u, 34u, 36u, 39u, 48u, 63u, 92u, 110u, 
	114u, 117u, 120u, 97u, 98u, 101u, 102u, 116u, 
	118u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 43u, 45u, 48u, 57u, 48u, 57u, 48u, 
	57u, 42u, 42u, 47u, 43u, 45u, 48u, 57u, 
	48u, 57u, 48u, 57u, 43u, 45u, 48u, 57u, 
	48u, 57u, 48u, 57u, 48u, 49u, 48u, 57u, 
	65u, 70u, 97u, 102u, 95u, 65u, 90u, 97u, 
	122u, 34u, 92u, 34u, 36u, 39u, 48u, 63u, 
	92u, 110u, 114u, 117u, 120u, 97u, 98u, 101u, 
	102u, 116u, 118u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 34u, 32u, 33u, 34u, 37u, 
	38u, 39u, 42u, 43u, 45u, 46u, 47u, 48u, 
	58u, 60u, 61u, 62u, 64u, 94u, 95u, 102u, 
	114u, 115u, 124u, 9u, 13u, 49u, 57u, 65u, 
	90u, 97u, 122u, 32u, 9u, 13u, 61u, 34u, 
	38u, 61u, 43u, 61u, 45u, 61u, 62u, 48u, 
	57u, 69u, 95u, 101u, 48u, 57u, 68u, 70u, 
	100u, 102u, 68u, 70u, 95u, 100u, 102u, 48u, 
	57u, 42u, 47u, 61u, 10u, 46u, 66u, 69u, 
	76u, 88u, 95u, 98u, 101u, 108u, 120u, 48u, 
	57u, 69u, 101u, 48u, 57u, 68u, 70u, 100u, 
	102u, 69u, 95u, 101u, 48u, 57u, 68u, 70u, 
	100u, 102u, 68u, 70u, 95u, 100u, 102u, 48u, 
	57u, 46u, 69u, 76u, 95u, 101u, 108u, 48u, 
	57u, 68u, 70u, 95u, 100u, 102u, 48u, 57u, 
	76u, 95u, 108u, 48u, 49u, 76u, 95u, 108u, 
	48u, 57u, 65u, 70u, 97u, 102u, 58u, 45u, 
	60u, 61u, 61u, 62u, 61u, 62u, 46u, 95u, 
	48u, 57u, 65u, 90u, 97u, 122u, 95u, 48u, 
	57u, 65u, 90u, 97u, 122u, 34u, 95u, 48u, 
	57u, 65u, 90u, 97u, 122u, 34u, 95u, 97u, 
	48u, 57u, 65u, 90u, 98u, 122u, 95u, 119u, 
	48u, 57u, 65u, 90u, 97u, 122u, 34u, 95u, 
	48u, 57u, 65u, 90u, 97u, 122u, 61u, 124u, 
	0
};

static const char _SCLT_single_lengths[] = {
	0, 2, 2, 10, 0, 0, 0, 0, 
	1, 1, 1, 2, 10, 0, 0, 0, 
	0, 2, 0, 0, 1, 2, 2, 0, 
	0, 2, 0, 0, 0, 0, 1, 2, 
	10, 0, 0, 0, 0, 1, 23, 1, 
	1, 1, 2, 2, 1, 0, 3, 5, 
	3, 1, 10, 2, 3, 5, 6, 5, 
	3, 3, 1, 3, 0, 2, 2, 1, 
	2, 3, 2, 2, 2
};

static const char _SCLT_range_lengths[] = {
	0, 0, 0, 3, 3, 3, 3, 3, 
	0, 0, 0, 0, 3, 3, 3, 3, 
	3, 1, 1, 1, 0, 0, 1, 1, 
	1, 1, 1, 1, 1, 3, 2, 0, 
	3, 3, 3, 3, 3, 0, 4, 1, 
	0, 0, 0, 0, 1, 1, 3, 1, 
	0, 0, 1, 3, 3, 1, 1, 1, 
	1, 3, 0, 0, 1, 0, 3, 3, 
	3, 3, 3, 3, 0
};

static const short _SCLT_index_offsets[] = {
	0, 0, 3, 6, 20, 24, 28, 32, 
	36, 38, 40, 42, 45, 59, 63, 67, 
	71, 75, 79, 81, 83, 85, 88, 92, 
	94, 96, 100, 102, 104, 106, 110, 114, 
	117, 131, 135, 139, 143, 147, 149, 177, 
	180, 182, 184, 187, 190, 193, 195, 202, 
	209, 213, 215, 227, 233, 240, 247, 255, 
	262, 267, 274, 276, 280, 282, 285, 291, 
	296, 302, 309, 315, 321
};

static const char _SCLT_indicies[] = {
	1, 2, 0, 3, 2, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 5, 6, 
	0, 0, 0, 4, 7, 7, 7, 4, 
	6, 6, 6, 4, 8, 8, 8, 4, 
	0, 0, 0, 4, 11, 10, 12, 10, 
	13, 10, 15, 16, 14, 14, 14, 14, 
	14, 14, 14, 14, 14, 17, 18, 14, 
	14, 14, 4, 19, 19, 19, 4, 18, 
	18, 18, 4, 20, 20, 20, 4, 14, 
	14, 14, 4, 22, 22, 23, 21, 23, 
	21, 24, 21, 27, 26, 27, 28, 26, 
	30, 30, 31, 29, 31, 29, 32, 29, 
	34, 34, 35, 33, 35, 36, 37, 33, 
	38, 36, 39, 39, 39, 36, 40, 40, 
	40, 4, 43, 44, 42, 42, 42, 42, 
	42, 42, 42, 42, 42, 45, 46, 42, 
	42, 42, 41, 47, 47, 47, 41, 46, 
	46, 46, 41, 48, 48, 48, 41, 42, 
	42, 42, 41, 50, 49, 52, 53, 54, 
	53, 55, 14, 53, 56, 57, 58, 59, 
	60, 61, 62, 63, 64, 65, 53, 66, 
	67, 68, 67, 69, 52, 37, 66, 66, 
	51, 52, 52, 70, 71, 36, 10, 72, 
	71, 71, 73, 71, 71, 73, 71, 71, 
	73, 24, 74, 77, 78, 77, 24, 76, 
	76, 75, 76, 76, 22, 76, 76, 23, 
	75, 26, 79, 71, 73, 80, 79, 82, 
	83, 84, 85, 86, 87, 83, 84, 85, 
	86, 37, 81, 90, 90, 32, 89, 89, 
	88, 90, 91, 90, 32, 89, 89, 88, 
	89, 89, 30, 89, 89, 31, 88, 82, 
	84, 85, 87, 84, 85, 37, 81, 93, 
	93, 34, 93, 93, 35, 92, 95, 83, 
	95, 38, 94, 97, 86, 97, 39, 39, 
	39, 96, 71, 73, 71, 98, 71, 73, 
	71, 73, 71, 98, 73, 40, 40, 40, 
	40, 40, 99, 66, 66, 66, 66, 100, 
	42, 66, 66, 66, 66, 100, 42, 66, 
	101, 66, 66, 66, 100, 66, 102, 66, 
	66, 66, 100, 49, 66, 66, 66, 66, 
	100, 71, 71, 73, 0
};

static const char _SCLT_trans_targs[] = {
	2, 41, 3, 38, 0, 4, 6, 5, 
	7, 38, 8, 9, 10, 38, 11, 38, 
	12, 13, 15, 14, 16, 38, 18, 47, 
	46, 38, 20, 21, 38, 38, 23, 53, 
	52, 38, 26, 55, 38, 54, 56, 57, 
	62, 38, 31, 38, 32, 33, 35, 34, 
	36, 37, 38, 38, 39, 40, 1, 42, 
	43, 44, 45, 48, 50, 58, 59, 60, 
	61, 30, 63, 64, 65, 68, 38, 38, 
	38, 38, 38, 38, 38, 17, 19, 49, 
	38, 38, 51, 28, 25, 38, 29, 27, 
	38, 38, 22, 24, 38, 38, 38, 38, 
	38, 38, 40, 38, 38, 66, 67
};

static const char _SCLT_trans_actions[] = {
	0, 5, 0, 15, 0, 0, 0, 0, 
	0, 61, 0, 0, 0, 9, 0, 17, 
	0, 0, 0, 0, 0, 65, 0, 5, 
	5, 71, 0, 0, 7, 63, 0, 5, 
	5, 67, 0, 81, 73, 84, 78, 75, 
	0, 69, 0, 11, 0, 0, 0, 0, 
	0, 0, 13, 33, 0, 90, 0, 0, 
	0, 0, 0, 5, 84, 0, 0, 0, 
	0, 0, 0, 5, 5, 0, 59, 31, 
	37, 55, 57, 45, 25, 0, 0, 0, 
	35, 49, 5, 0, 0, 29, 0, 0, 
	43, 23, 0, 0, 47, 27, 41, 21, 
	39, 19, 87, 51, 53, 0, 5
};

static const char _SCLT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 1, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0
};

static const char _SCLT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 3, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0
};

static const short _SCLT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	10, 10, 10, 0, 0, 0, 0, 0, 
	0, 22, 22, 22, 26, 26, 30, 30, 
	30, 34, 37, 34, 37, 37, 0, 42, 
	42, 42, 42, 42, 42, 42, 0, 71, 
	37, 73, 74, 74, 74, 75, 76, 76, 
	74, 81, 82, 89, 89, 89, 82, 93, 
	95, 97, 74, 74, 74, 74, 100, 101, 
	101, 101, 101, 101, 74
};

static const int SCLT_start = 38;
static const int SCLT_first_final = 38;
static const int SCLT_error = 0;

static const int SCLT_en_main = 38;


/* #line 137 "SCLT.c.rl" */

ok64 SCLTLexer(SCLTstate* state) {

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

    
/* #line 263 "SCLT.rl.c" */
	{
	cs = SCLT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 155 "SCLT.c.rl" */
    
/* #line 269 "SCLT.rl.c" */
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
	_acts = _SCLT_actions + _SCLT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 288 "SCLT.rl.c" */
		}
	}

	_keys = _SCLT_trans_keys + _SCLT_key_offsets[cs];
	_trans = _SCLT_index_offsets[cs];

	_klen = _SCLT_single_lengths[cs];
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

	_klen = _SCLT_range_lengths[cs];
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
	_trans = _SCLT_indicies[_trans];
_eof_trans:
	cs = _SCLT_trans_targs[_trans];

	if ( _SCLT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _SCLT_actions + _SCLT_trans_actions[_trans];
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
/* #line 43 "SCLT.c.rl" */
	{act = 8;}
	break;
	case 4:
/* #line 43 "SCLT.c.rl" */
	{act = 9;}
	break;
	case 5:
/* #line 43 "SCLT.c.rl" */
	{act = 12;}
	break;
	case 6:
/* #line 43 "SCLT.c.rl" */
	{act = 13;}
	break;
	case 7:
/* #line 61 "SCLT.c.rl" */
	{act = 16;}
	break;
	case 8:
/* #line 61 "SCLT.c.rl" */
	{act = 17;}
	break;
	case 9:
/* #line 31 "SCLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 37 "SCLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 37 "SCLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 37 "SCLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 37 "SCLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 37 "SCLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 43 "SCLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 43 "SCLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 43 "SCLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 43 "SCLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 43 "SCLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 43 "SCLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 61 "SCLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 61 "SCLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 31 "SCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 37 "SCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 43 "SCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 43 "SCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 43 "SCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 43 "SCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 43 "SCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 43 "SCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 49 "SCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonAnnotation(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 55 "SCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 61 "SCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 61 "SCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 67 "SCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 37 "SCLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 43 "SCLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 43 "SCLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 39:
/* #line 43 "SCLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 40:
/* #line 55 "SCLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 41:
/* #line 61 "SCLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 42:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 8:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 9:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 13:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 16:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 17:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 691 "SCLT.rl.c" */
		}
	}

_again:
	_acts = _SCLT_actions + _SCLT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 702 "SCLT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _SCLT_eof_trans[cs] > 0 ) {
		_trans = _SCLT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 156 "SCLT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < SCLT_first_final)
        o = SCLTBAD;

    return o;
}
