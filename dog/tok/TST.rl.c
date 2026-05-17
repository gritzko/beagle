
/* #line 1 "TST.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "TST.h"

ok64 TSTonComment (u8cs tok, TSTstate* state);
ok64 TSTonString (u8cs tok, TSTstate* state);
ok64 TSTonNumber (u8cs tok, TSTstate* state);
ok64 TSTonWord (u8cs tok, TSTstate* state);
ok64 TSTonPunct (u8cs tok, TSTstate* state);
ok64 TSTonSpace (u8cs tok, TSTstate* state);


/* #line 123 "TST.c.rl" */



/* #line 15 "TST.rl.c" */
static const char _TST_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	10, 1, 11, 1, 12, 1, 13, 1, 
	14, 1, 15, 1, 16, 1, 17, 1, 
	18, 1, 19, 1, 20, 1, 21, 1, 
	22, 1, 23, 1, 24, 1, 25, 1, 
	26, 1, 27, 1, 28, 1, 29, 1, 
	30, 1, 31, 1, 32, 1, 33, 1, 
	34, 1, 35, 1, 36, 1, 37, 1, 
	38, 1, 39, 1, 40, 1, 41, 2, 
	2, 3, 2, 2, 4, 2, 2, 5, 
	2, 2, 6, 2, 2, 7, 2, 2, 
	8, 2, 2, 9
};

static const short _TST_key_offsets[] = {
	0, 0, 2, 18, 25, 31, 37, 43, 
	49, 56, 63, 70, 77, 84, 85, 91, 
	93, 109, 116, 122, 128, 134, 140, 147, 
	154, 161, 168, 175, 176, 177, 181, 183, 
	185, 186, 188, 192, 194, 196, 200, 202, 
	204, 206, 208, 214, 216, 216, 245, 248, 
	249, 250, 258, 260, 262, 264, 266, 269, 
	275, 279, 282, 283, 296, 301, 307, 311, 
	318, 322, 326, 330, 338, 340, 342, 344, 
	346, 348
};

static const unsigned char _TST_trans_keys[] = {
	34u, 92u, 34u, 39u, 63u, 92u, 110u, 114u, 
	117u, 120u, 47u, 55u, 97u, 98u, 101u, 102u, 
	116u, 118u, 123u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 125u, 48u, 57u, 65u, 70u, 97u, 102u, 
	125u, 48u, 57u, 65u, 70u, 97u, 102u, 125u, 
	48u, 57u, 65u, 70u, 97u, 102u, 125u, 48u, 
	57u, 65u, 70u, 97u, 102u, 125u, 48u, 57u, 
	65u, 70u, 97u, 102u, 125u, 36u, 95u, 65u, 
	90u, 97u, 122u, 39u, 92u, 34u, 39u, 63u, 
	92u, 110u, 114u, 117u, 120u, 47u, 55u, 97u, 
	98u, 101u, 102u, 116u, 118u, 123u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 125u, 48u, 57u, 65u, 
	70u, 97u, 102u, 125u, 48u, 57u, 65u, 70u, 
	97u, 102u, 125u, 48u, 57u, 65u, 70u, 97u, 
	102u, 125u, 48u, 57u, 65u, 70u, 97u, 102u, 
	125u, 48u, 57u, 65u, 70u, 97u, 102u, 125u, 
	46u, 43u, 45u, 48u, 57u, 48u, 57u, 48u, 
	57u, 42u, 42u, 47u, 43u, 45u, 48u, 57u, 
	48u, 57u, 48u, 57u, 43u, 45u, 48u, 57u, 
	48u, 57u, 48u, 57u, 48u, 49u, 48u, 55u, 
	48u, 57u, 65u, 70u, 97u, 102u, 92u, 96u, 
	32u, 33u, 34u, 35u, 36u, 37u, 38u, 39u, 
	42u, 43u, 45u, 46u, 47u, 48u, 60u, 61u, 
	62u, 63u, 94u, 96u, 124u, 9u, 13u, 49u, 
	57u, 65u, 90u, 95u, 122u, 32u, 9u, 13u, 
	61u, 61u, 36u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 38u, 61u, 42u, 61u, 43u, 61u, 
	45u, 61u, 46u, 48u, 57u, 69u, 95u, 101u, 
	110u, 48u, 57u, 95u, 110u, 48u, 57u, 42u, 
	47u, 61u, 10u, 46u, 66u, 69u, 79u, 88u, 
	95u, 98u, 101u, 110u, 111u, 120u, 48u, 57u, 
	69u, 101u, 110u, 48u, 57u, 69u, 95u, 101u, 
	110u, 48u, 57u, 95u, 110u, 48u, 57u, 46u, 
	69u, 95u, 101u, 110u, 48u, 57u, 95u, 110u, 
	48u, 57u, 95u, 110u, 48u, 49u, 95u, 110u, 
	48u, 55u, 95u, 110u, 48u, 57u, 65u, 70u, 
	97u, 102u, 60u, 61u, 61u, 62u, 61u, 62u, 
	61u, 62u, 46u, 63u, 61u, 124u, 0
};

static const char _TST_single_lengths[] = {
	0, 2, 8, 1, 0, 0, 0, 0, 
	1, 1, 1, 1, 1, 1, 2, 2, 
	8, 1, 0, 0, 0, 0, 1, 1, 
	1, 1, 1, 1, 1, 2, 0, 0, 
	1, 2, 2, 0, 0, 2, 0, 0, 
	0, 0, 0, 2, 0, 21, 1, 1, 
	1, 2, 2, 2, 2, 2, 1, 4, 
	2, 3, 1, 11, 3, 4, 2, 5, 
	2, 2, 2, 2, 2, 2, 2, 2, 
	2, 2
};

static const char _TST_range_lengths[] = {
	0, 0, 4, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 0, 2, 0, 
	4, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 0, 0, 1, 1, 1, 
	0, 0, 1, 1, 1, 1, 1, 1, 
	1, 1, 3, 0, 0, 4, 1, 0, 
	0, 3, 0, 0, 0, 0, 1, 1, 
	1, 0, 0, 1, 1, 1, 1, 1, 
	1, 1, 1, 3, 0, 0, 0, 0, 
	0, 0
};

static const short _TST_index_offsets[] = {
	0, 0, 3, 16, 21, 25, 29, 33, 
	37, 42, 47, 52, 57, 62, 64, 69, 
	72, 85, 90, 94, 98, 102, 106, 111, 
	116, 121, 126, 131, 133, 135, 139, 141, 
	143, 145, 148, 152, 154, 156, 160, 162, 
	164, 166, 168, 172, 175, 176, 202, 205, 
	207, 209, 215, 218, 221, 224, 227, 230, 
	236, 240, 244, 246, 259, 264, 270, 274, 
	281, 285, 289, 293, 299, 302, 305, 308, 
	311, 314
};

static const char _TST_indicies[] = {
	1, 2, 0, 0, 0, 0, 0, 0, 
	0, 4, 5, 0, 0, 0, 0, 3, 
	7, 6, 6, 6, 3, 5, 5, 5, 
	3, 8, 8, 8, 3, 0, 0, 0, 
	3, 9, 9, 9, 3, 0, 10, 10, 
	10, 3, 0, 11, 11, 11, 3, 0, 
	12, 12, 12, 3, 0, 13, 13, 13, 
	3, 0, 14, 14, 14, 3, 0, 3, 
	15, 15, 15, 15, 3, 17, 18, 16, 
	16, 16, 16, 16, 16, 16, 19, 20, 
	16, 16, 16, 16, 3, 22, 21, 21, 
	21, 3, 20, 20, 20, 3, 23, 23, 
	23, 3, 16, 16, 16, 3, 24, 24, 
	24, 3, 16, 25, 25, 25, 3, 16, 
	26, 26, 26, 3, 16, 27, 27, 27, 
	3, 16, 28, 28, 28, 3, 16, 29, 
	29, 29, 3, 16, 3, 31, 30, 33, 
	33, 34, 32, 34, 32, 35, 32, 38, 
	37, 38, 39, 37, 41, 41, 42, 40, 
	42, 40, 43, 40, 45, 45, 46, 44, 
	46, 47, 48, 44, 49, 47, 50, 47, 
	51, 51, 51, 47, 53, 54, 52, 52, 
	56, 57, 0, 58, 15, 59, 60, 16, 
	61, 62, 63, 64, 65, 66, 67, 68, 
	69, 70, 59, 52, 71, 56, 48, 15, 
	15, 55, 56, 56, 72, 74, 73, 31, 
	47, 15, 15, 15, 15, 15, 75, 74, 
	31, 73, 74, 31, 73, 31, 31, 73, 
	31, 31, 73, 77, 35, 76, 79, 80, 
	79, 81, 35, 78, 33, 81, 34, 78, 
	37, 82, 31, 73, 83, 82, 85, 86, 
	87, 88, 89, 90, 86, 87, 91, 88, 
	89, 48, 84, 93, 93, 94, 43, 92, 
	93, 95, 93, 94, 43, 92, 41, 94, 
	42, 92, 85, 87, 90, 87, 91, 48, 
	84, 45, 97, 46, 96, 86, 99, 49, 
	98, 88, 101, 50, 100, 89, 103, 51, 
	51, 51, 102, 74, 31, 73, 74, 31, 
	73, 31, 104, 73, 31, 74, 105, 31, 
	74, 73, 31, 74, 73, 0
};

static const char _TST_trans_targs[] = {
	1, 45, 2, 0, 3, 5, 4, 7, 
	6, 8, 9, 10, 11, 12, 13, 49, 
	15, 45, 16, 17, 19, 18, 21, 20, 
	22, 23, 24, 25, 26, 27, 45, 45, 
	45, 30, 56, 55, 45, 32, 33, 45, 
	45, 35, 62, 61, 45, 38, 64, 45, 
	63, 65, 66, 67, 43, 44, 45, 45, 
	46, 47, 14, 48, 50, 51, 52, 53, 
	54, 57, 59, 68, 69, 70, 72, 73, 
	45, 45, 48, 45, 45, 28, 45, 29, 
	31, 45, 58, 45, 45, 60, 40, 37, 
	41, 42, 39, 45, 45, 34, 45, 36, 
	45, 45, 45, 45, 45, 45, 45, 45, 
	71, 45
};

static const char _TST_trans_actions[] = {
	0, 11, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 13, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 67, 29, 
	61, 0, 5, 5, 65, 0, 0, 7, 
	59, 0, 5, 5, 63, 0, 80, 69, 
	83, 77, 74, 71, 0, 0, 9, 31, 
	0, 0, 0, 89, 0, 0, 0, 0, 
	5, 5, 83, 0, 0, 0, 0, 0, 
	57, 53, 86, 49, 55, 0, 43, 0, 
	0, 23, 0, 33, 47, 5, 0, 0, 
	0, 0, 0, 27, 41, 0, 21, 0, 
	45, 25, 39, 19, 37, 17, 35, 15, 
	0, 51
};

static const char _TST_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 1, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const char _TST_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 3, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const short _TST_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 31, 33, 33, 33, 
	37, 37, 41, 41, 41, 45, 48, 45, 
	48, 48, 48, 0, 0, 0, 73, 74, 
	48, 76, 74, 74, 74, 74, 77, 79, 
	79, 74, 84, 85, 93, 93, 93, 85, 
	97, 99, 101, 103, 74, 74, 74, 106, 
	74, 74
};

static const int TST_start = 45;
static const int TST_first_final = 45;
static const int TST_error = 0;

static const int TST_en_main = 45;


/* #line 126 "TST.c.rl" */

ok64 TSTLexer(TSTstate* state) {

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

    
/* #line 268 "TST.rl.c" */
	{
	cs = TST_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 144 "TST.c.rl" */
    
/* #line 274 "TST.rl.c" */
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
	_acts = _TST_actions + _TST_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 293 "TST.rl.c" */
		}
	}

	_keys = _TST_trans_keys + _TST_key_offsets[cs];
	_trans = _TST_index_offsets[cs];

	_klen = _TST_single_lengths[cs];
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

	_klen = _TST_range_lengths[cs];
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
	_trans = _TST_indicies[_trans];
_eof_trans:
	cs = _TST_trans_targs[_trans];

	if ( _TST_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _TST_actions + _TST_trans_actions[_trans];
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
/* #line 44 "TST.c.rl" */
	{act = 6;}
	break;
	case 4:
/* #line 44 "TST.c.rl" */
	{act = 7;}
	break;
	case 5:
/* #line 44 "TST.c.rl" */
	{act = 8;}
	break;
	case 6:
/* #line 44 "TST.c.rl" */
	{act = 11;}
	break;
	case 7:
/* #line 44 "TST.c.rl" */
	{act = 12;}
	break;
	case 8:
/* #line 56 "TST.c.rl" */
	{act = 14;}
	break;
	case 9:
/* #line 56 "TST.c.rl" */
	{act = 15;}
	break;
	case 10:
/* #line 32 "TST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 38 "TST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 38 "TST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 38 "TST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 44 "TST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 44 "TST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 44 "TST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 44 "TST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 44 "TST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 44 "TST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 44 "TST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 56 "TST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 56 "TST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 32 "TST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 44 "TST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 44 "TST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 44 "TST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 44 "TST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 44 "TST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 44 "TST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 44 "TST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 50 "TST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 56 "TST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 56 "TST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 56 "TST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 62 "TST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 44 "TST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 44 "TST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 44 "TST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 39:
/* #line 56 "TST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 40:
/* #line 56 "TST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 41:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 6:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 7:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 8:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 11:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 14:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 15:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 691 "TST.rl.c" */
		}
	}

_again:
	_acts = _TST_actions + _TST_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 702 "TST.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _TST_eof_trans[cs] > 0 ) {
		_trans = _TST_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 145 "TST.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < TST_first_final)
        o = TSTBAD;

    return o;
}
