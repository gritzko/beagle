
/* #line 1 "TOMLT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "TOMLT.h"

ok64 TOMLTonComment (u8cs tok, TOMLTstate* state);
ok64 TOMLTonString (u8cs tok, TOMLTstate* state);
ok64 TOMLTonNumber (u8cs tok, TOMLTstate* state);
ok64 TOMLTonHeader (u8cs tok, TOMLTstate* state);
ok64 TOMLTonWord (u8cs tok, TOMLTstate* state);
ok64 TOMLTonPunct (u8cs tok, TOMLTstate* state);
ok64 TOMLTonSpace (u8cs tok, TOMLTstate* state);


/* #line 124 "TOMLT.c.rl" */



/* #line 16 "TOMLT.rl.c" */
static const char _TOMLT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19, 1, 20, 1, 21, 1, 22, 1, 
	23, 1, 24, 1, 25, 1, 26, 1, 
	27, 1, 28, 1, 29, 1, 30, 1, 
	31, 1, 32, 1, 33, 1, 34, 1, 
	35, 1, 36, 1, 37, 1, 38, 1, 
	39, 1, 40, 1, 41, 1, 42, 2, 
	2, 3, 2, 2, 4, 2, 2, 5, 
	2, 2, 6, 2, 2, 7, 2, 2, 
	8, 2, 2, 9, 2, 2, 10
};

static const short _TOMLT_key_offsets[] = {
	0, 0, 3, 6, 15, 21, 27, 33, 
	39, 45, 51, 57, 63, 65, 66, 67, 
	76, 82, 88, 94, 100, 106, 112, 118, 
	124, 126, 128, 129, 130, 131, 133, 137, 
	139, 143, 145, 147, 149, 151, 157, 158, 
	159, 160, 161, 163, 165, 166, 168, 170, 
	172, 174, 175, 177, 179, 181, 183, 184, 
	186, 188, 190, 192, 194, 204, 213, 223, 
	224, 244, 247, 248, 249, 250, 251, 252, 
	253, 254, 259, 271, 276, 279, 285, 288, 
	291, 294, 301, 313, 319, 325, 332, 334, 
	339, 344, 350, 356, 364, 374, 383, 392, 
	401
};

static const unsigned char _TOMLT_trans_keys[] = {
	10u, 34u, 92u, 10u, 34u, 92u, 34u, 85u, 
	92u, 98u, 102u, 110u, 114u, 116u, 117u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 34u, 
	92u, 34u, 34u, 34u, 85u, 92u, 98u, 102u, 
	110u, 114u, 116u, 117u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 10u, 39u, 10u, 39u, 
	39u, 39u, 39u, 48u, 57u, 43u, 45u, 48u, 
	57u, 48u, 57u, 43u, 45u, 48u, 57u, 48u, 
	57u, 48u, 57u, 48u, 49u, 48u, 55u, 48u, 
	57u, 65u, 70u, 97u, 102u, 110u, 102u, 97u, 
	110u, 48u, 57u, 48u, 57u, 45u, 48u, 57u, 
	48u, 57u, 48u, 57u, 48u, 57u, 58u, 48u, 
	57u, 48u, 57u, 48u, 57u, 48u, 57u, 58u, 
	48u, 57u, 48u, 57u, 48u, 57u, 48u, 57u, 
	48u, 57u, 93u, 95u, 45u, 46u, 48u, 57u, 
	65u, 90u, 97u, 122u, 95u, 45u, 46u, 48u, 
	57u, 65u, 90u, 97u, 122u, 93u, 95u, 45u, 
	46u, 48u, 57u, 65u, 90u, 97u, 122u, 93u, 
	32u, 34u, 35u, 39u, 43u, 45u, 46u, 48u, 
	91u, 95u, 105u, 110u, 9u, 13u, 49u, 57u, 
	65u, 90u, 97u, 122u, 32u, 9u, 13u, 34u, 
	34u, 34u, 10u, 39u, 39u, 39u, 48u, 105u, 
	110u, 49u, 57u, 46u, 66u, 69u, 79u, 88u, 
	95u, 98u, 101u, 111u, 120u, 48u, 57u, 69u, 
	95u, 101u, 48u, 57u, 95u, 48u, 57u, 46u, 
	69u, 95u, 101u, 48u, 57u, 95u, 48u, 57u, 
	95u, 48u, 49u, 95u, 48u, 55u, 95u, 48u, 
	57u, 65u, 70u, 97u, 102u, 46u, 66u, 69u, 
	79u, 88u, 95u, 98u, 101u, 111u, 120u, 48u, 
	57u, 46u, 69u, 95u, 101u, 48u, 57u, 46u, 
	69u, 95u, 101u, 48u, 57u, 45u, 46u, 69u, 
	95u, 101u, 48u, 57u, 32u, 84u, 43u, 45u, 
	58u, 90u, 122u, 43u, 45u, 46u, 90u, 122u, 
	43u, 45u, 90u, 122u, 48u, 57u, 46u, 69u, 
	95u, 101u, 48u, 57u, 45u, 95u, 48u, 57u, 
	65u, 90u, 97u, 122u, 91u, 95u, 45u, 46u, 
	48u, 57u, 65u, 90u, 97u, 122u, 45u, 95u, 
	110u, 48u, 57u, 65u, 90u, 97u, 122u, 45u, 
	95u, 102u, 48u, 57u, 65u, 90u, 97u, 122u, 
	45u, 95u, 97u, 48u, 57u, 65u, 90u, 98u, 
	122u, 45u, 95u, 110u, 48u, 57u, 65u, 90u, 
	97u, 122u, 0
};

static const char _TOMLT_single_lengths[] = {
	0, 3, 3, 9, 0, 0, 0, 0, 
	0, 0, 0, 0, 2, 1, 1, 9, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	2, 2, 1, 1, 1, 0, 2, 0, 
	2, 0, 0, 0, 0, 0, 1, 1, 
	1, 1, 0, 0, 1, 0, 0, 0, 
	0, 1, 0, 0, 0, 0, 1, 0, 
	0, 0, 0, 0, 2, 1, 2, 1, 
	12, 1, 1, 1, 1, 1, 1, 1, 
	1, 3, 10, 3, 1, 4, 1, 1, 
	1, 1, 10, 4, 4, 5, 2, 5, 
	5, 4, 4, 2, 2, 3, 3, 3, 
	3
};

static const char _TOMLT_range_lengths[] = {
	0, 0, 0, 0, 3, 3, 3, 3, 
	3, 3, 3, 3, 0, 0, 0, 0, 
	3, 3, 3, 3, 3, 3, 3, 3, 
	0, 0, 0, 0, 0, 1, 1, 1, 
	1, 1, 1, 1, 1, 3, 0, 0, 
	0, 0, 1, 1, 0, 1, 1, 1, 
	1, 0, 1, 1, 1, 1, 0, 1, 
	1, 1, 1, 1, 4, 4, 4, 0, 
	4, 1, 0, 0, 0, 0, 0, 0, 
	0, 1, 1, 1, 1, 1, 1, 1, 
	1, 3, 1, 1, 1, 1, 0, 0, 
	0, 1, 1, 3, 4, 3, 3, 3, 
	3
};

static const short _TOMLT_index_offsets[] = {
	0, 0, 4, 8, 18, 22, 26, 30, 
	34, 38, 42, 46, 50, 53, 55, 57, 
	67, 71, 75, 79, 83, 87, 91, 95, 
	99, 102, 105, 107, 109, 111, 113, 117, 
	119, 123, 125, 127, 129, 131, 135, 137, 
	139, 141, 143, 145, 147, 149, 151, 153, 
	155, 157, 159, 161, 163, 165, 167, 169, 
	171, 173, 175, 177, 179, 186, 192, 199, 
	201, 218, 221, 223, 225, 227, 229, 231, 
	233, 235, 240, 252, 257, 260, 266, 269, 
	272, 275, 280, 292, 298, 304, 311, 314, 
	320, 326, 332, 338, 344, 351, 358, 365, 
	372
};

static const unsigned char _TOMLT_indicies[] = {
	1, 2, 3, 0, 1, 4, 3, 0, 
	0, 5, 0, 0, 0, 0, 0, 0, 
	6, 1, 7, 7, 7, 1, 8, 8, 
	8, 1, 9, 9, 9, 1, 6, 6, 
	6, 1, 10, 10, 10, 1, 11, 11, 
	11, 1, 12, 12, 12, 1, 0, 0, 
	0, 1, 15, 16, 14, 17, 14, 18, 
	14, 14, 19, 14, 14, 14, 14, 14, 
	14, 20, 13, 21, 21, 21, 13, 22, 
	22, 22, 13, 23, 23, 23, 13, 20, 
	20, 20, 13, 24, 24, 24, 13, 25, 
	25, 25, 13, 26, 26, 26, 13, 14, 
	14, 14, 13, 1, 28, 27, 1, 29, 
	27, 32, 31, 33, 31, 34, 31, 36, 
	35, 38, 38, 39, 37, 39, 37, 41, 
	41, 42, 40, 42, 35, 43, 40, 44, 
	35, 45, 35, 46, 46, 46, 35, 48, 
	47, 49, 47, 50, 47, 49, 47, 51, 
	40, 52, 40, 53, 40, 54, 40, 55, 
	40, 57, 56, 58, 56, 59, 56, 60, 
	56, 61, 56, 62, 56, 63, 56, 64, 
	56, 65, 56, 66, 56, 67, 56, 68, 
	56, 69, 56, 71, 70, 70, 70, 70, 
	70, 47, 72, 72, 72, 72, 72, 47, 
	73, 72, 72, 72, 72, 72, 47, 74, 
	47, 76, 77, 78, 79, 80, 80, 81, 
	82, 85, 84, 86, 87, 76, 83, 84, 
	84, 75, 76, 76, 88, 14, 89, 91, 
	90, 92, 90, 93, 78, 31, 94, 96, 
	95, 97, 95, 99, 100, 101, 43, 98, 
	103, 104, 105, 106, 107, 108, 104, 105, 
	106, 107, 43, 102, 110, 103, 110, 36, 
	109, 38, 39, 109, 103, 105, 108, 105, 
	43, 102, 41, 42, 111, 104, 44, 112, 
	106, 45, 113, 107, 46, 46, 46, 114, 
	103, 104, 105, 106, 107, 108, 104, 105, 
	106, 107, 115, 102, 103, 105, 108, 105, 
	116, 102, 103, 105, 108, 105, 117, 102, 
	118, 103, 105, 108, 105, 43, 102, 120, 
	120, 119, 121, 121, 122, 66, 66, 119, 
	121, 121, 123, 66, 66, 119, 121, 121, 
	66, 66, 69, 119, 103, 105, 108, 105, 
	115, 102, 84, 84, 84, 84, 84, 35, 
	124, 70, 70, 70, 70, 70, 98, 84, 
	84, 126, 84, 84, 84, 125, 84, 84, 
	127, 84, 84, 84, 125, 84, 84, 128, 
	84, 84, 84, 125, 84, 84, 127, 84, 
	84, 84, 125, 0
};

static const char _TOMLT_trans_targs[] = {
	2, 0, 66, 3, 64, 4, 8, 5, 
	6, 7, 9, 10, 11, 64, 12, 13, 
	15, 14, 67, 16, 20, 17, 18, 19, 
	21, 22, 23, 25, 70, 64, 64, 26, 
	27, 28, 71, 64, 75, 64, 31, 76, 
	64, 33, 78, 77, 79, 80, 81, 64, 
	39, 64, 41, 43, 44, 45, 46, 86, 
	64, 48, 49, 50, 51, 87, 53, 54, 
	55, 56, 64, 58, 88, 89, 60, 64, 
	62, 63, 64, 64, 65, 1, 69, 24, 
	73, 64, 82, 90, 91, 92, 93, 95, 
	64, 64, 64, 68, 64, 64, 64, 64, 
	72, 64, 64, 74, 38, 40, 64, 29, 
	35, 32, 36, 37, 34, 64, 30, 64, 
	64, 64, 64, 83, 84, 85, 42, 64, 
	47, 52, 57, 59, 61, 64, 94, 91, 
	96
};

static const char _TOMLT_trans_actions[] = {
	0, 0, 5, 0, 11, 0, 0, 0, 
	0, 0, 0, 0, 0, 57, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 5, 13, 59, 0, 
	0, 0, 0, 69, 83, 61, 0, 5, 
	63, 0, 86, 89, 77, 74, 71, 67, 
	0, 15, 0, 0, 0, 0, 0, 5, 
	65, 0, 0, 0, 0, 5, 0, 0, 
	0, 0, 17, 0, 5, 5, 0, 21, 
	0, 0, 19, 23, 0, 0, 0, 0, 
	5, 25, 89, 89, 92, 5, 0, 0, 
	55, 33, 29, 0, 7, 27, 35, 31, 
	0, 9, 53, 89, 0, 0, 47, 0, 
	0, 0, 0, 0, 0, 43, 0, 45, 
	41, 39, 37, 89, 89, 89, 0, 49, 
	0, 0, 0, 0, 0, 51, 0, 80, 
	0
};

static const char _TOMLT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	1, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const char _TOMLT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	3, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const short _TOMLT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 14, 14, 14, 14, 
	14, 14, 14, 14, 14, 14, 14, 14, 
	0, 0, 31, 31, 31, 36, 38, 38, 
	41, 36, 41, 36, 36, 36, 48, 48, 
	48, 48, 41, 41, 41, 41, 41, 57, 
	57, 57, 57, 57, 57, 57, 57, 57, 
	57, 57, 57, 57, 48, 48, 48, 48, 
	0, 89, 90, 91, 91, 94, 95, 96, 
	96, 99, 103, 110, 110, 103, 112, 113, 
	114, 115, 103, 103, 103, 103, 120, 120, 
	120, 120, 103, 36, 99, 126, 126, 126, 
	126
};

static const int TOMLT_start = 64;
static const int TOMLT_first_final = 64;
static const int TOMLT_error = 0;

static const int TOMLT_en_main = 64;


/* #line 127 "TOMLT.c.rl" */

ok64 TOMLTLexer(TOMLTstate* state) {

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

    
/* #line 312 "TOMLT.rl.c" */
	{
	cs = TOMLT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 145 "TOMLT.c.rl" */
    
/* #line 318 "TOMLT.rl.c" */
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
	_acts = _TOMLT_actions + _TOMLT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 337 "TOMLT.rl.c" */
		}
	}

	_keys = _TOMLT_trans_keys + _TOMLT_key_offsets[cs];
	_trans = _TOMLT_index_offsets[cs];

	_klen = _TOMLT_single_lengths[cs];
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

	_klen = _TOMLT_range_lengths[cs];
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
	_trans = _TOMLT_indicies[_trans];
_eof_trans:
	cs = _TOMLT_trans_targs[_trans];

	if ( _TOMLT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _TOMLT_actions + _TOMLT_trans_actions[_trans];
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
/* #line 40 "TOMLT.c.rl" */
	{act = 6;}
	break;
	case 4:
/* #line 40 "TOMLT.c.rl" */
	{act = 7;}
	break;
	case 5:
/* #line 40 "TOMLT.c.rl" */
	{act = 8;}
	break;
	case 6:
/* #line 40 "TOMLT.c.rl" */
	{act = 9;}
	break;
	case 7:
/* #line 40 "TOMLT.c.rl" */
	{act = 10;}
	break;
	case 8:
/* #line 40 "TOMLT.c.rl" */
	{act = 11;}
	break;
	case 9:
/* #line 40 "TOMLT.c.rl" */
	{act = 12;}
	break;
	case 10:
/* #line 52 "TOMLT.c.rl" */
	{act = 16;}
	break;
	case 11:
/* #line 34 "TOMLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 34 "TOMLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 34 "TOMLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 34 "TOMLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 40 "TOMLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 52 "TOMLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 46 "TOMLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonHeader(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 46 "TOMLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonHeader(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 58 "TOMLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 58 "TOMLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 28 "TOMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 34 "TOMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 34 "TOMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 34 "TOMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 34 "TOMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 40 "TOMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 40 "TOMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 40 "TOMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 40 "TOMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 40 "TOMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 40 "TOMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 52 "TOMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 52 "TOMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 58 "TOMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 64 "TOMLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 34 "TOMLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 34 "TOMLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 40 "TOMLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 39:
/* #line 40 "TOMLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 40:
/* #line 52 "TOMLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 41:
/* #line 58 "TOMLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 42:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 6:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 7:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 8:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 9:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 10:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 11:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 16:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 746 "TOMLT.rl.c" */
		}
	}

_again:
	_acts = _TOMLT_actions + _TOMLT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 757 "TOMLT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _TOMLT_eof_trans[cs] > 0 ) {
		_trans = _TOMLT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 146 "TOMLT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < TOMLT_first_final)
        o = TOMLTBAD;

    return o;
}
