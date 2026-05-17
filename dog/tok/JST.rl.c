
/* #line 1 "tok/JST.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "JST.h"

ok64 JSTonComment (u8cs tok, JSTstate* state);
ok64 JSTonString (u8cs tok, JSTstate* state);
ok64 JSTonNumber (u8cs tok, JSTstate* state);
ok64 JSTonWord (u8cs tok, JSTstate* state);
ok64 JSTonPunct (u8cs tok, JSTstate* state);
ok64 JSTonSpace (u8cs tok, JSTstate* state);


/* #line 129 "tok/JST.c.rl" */



/* #line 15 "tok/JST.rl.c" */
static const char _JST_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	12, 1, 13, 1, 14, 1, 15, 1, 
	16, 1, 17, 1, 18, 1, 19, 1, 
	20, 1, 21, 1, 22, 1, 23, 1, 
	24, 1, 25, 1, 26, 1, 27, 1, 
	28, 1, 29, 1, 30, 1, 31, 1, 
	32, 1, 33, 1, 34, 1, 35, 1, 
	36, 1, 37, 1, 38, 1, 39, 1, 
	40, 1, 41, 1, 42, 1, 43, 1, 
	44, 1, 45, 2, 2, 3, 2, 2, 
	4, 2, 2, 5, 2, 2, 6, 2, 
	2, 7, 2, 2, 8, 2, 2, 9, 
	2, 2, 10, 2, 2, 11
};

static const short _JST_key_offsets[] = {
	0, 0, 2, 18, 25, 31, 37, 43, 
	49, 56, 63, 70, 77, 84, 85, 92, 
	94, 110, 117, 123, 129, 135, 141, 148, 
	155, 162, 169, 176, 177, 178, 182, 184, 
	186, 189, 189, 193, 194, 196, 200, 201, 
	205, 207, 209, 213, 215, 217, 219, 221, 
	227, 229, 229, 258, 261, 262, 263, 264, 
	272, 274, 276, 278, 280, 283, 289, 293, 
	298, 306, 315, 316, 319, 332, 337, 343, 
	347, 354, 358, 362, 366, 374, 376, 378, 
	380, 382, 384
};

static const unsigned char _JST_trans_keys[] = {
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
	65u, 70u, 97u, 102u, 125u, 33u, 36u, 95u, 
	65u, 90u, 97u, 122u, 39u, 92u, 34u, 39u, 
	63u, 92u, 110u, 114u, 117u, 120u, 47u, 55u, 
	97u, 98u, 101u, 102u, 116u, 118u, 123u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 125u, 48u, 57u, 
	65u, 70u, 97u, 102u, 125u, 48u, 57u, 65u, 
	70u, 97u, 102u, 125u, 48u, 57u, 65u, 70u, 
	97u, 102u, 125u, 48u, 57u, 65u, 70u, 97u, 
	102u, 125u, 48u, 57u, 65u, 70u, 97u, 102u, 
	125u, 46u, 43u, 45u, 48u, 57u, 48u, 57u, 
	48u, 57u, 10u, 47u, 92u, 10u, 42u, 47u, 
	92u, 42u, 42u, 47u, 10u, 42u, 47u, 92u, 
	42u, 43u, 45u, 48u, 57u, 48u, 57u, 48u, 
	57u, 43u, 45u, 48u, 57u, 48u, 57u, 48u, 
	57u, 48u, 49u, 48u, 55u, 48u, 57u, 65u, 
	70u, 97u, 102u, 92u, 96u, 32u, 33u, 34u, 
	35u, 36u, 37u, 38u, 39u, 42u, 43u, 45u, 
	46u, 47u, 48u, 60u, 61u, 62u, 63u, 94u, 
	96u, 124u, 9u, 13u, 49u, 57u, 65u, 90u, 
	95u, 122u, 32u, 9u, 13u, 61u, 61u, 10u, 
	36u, 95u, 48u, 57u, 65u, 90u, 97u, 122u, 
	38u, 61u, 42u, 61u, 43u, 61u, 45u, 61u, 
	46u, 48u, 57u, 69u, 95u, 101u, 110u, 48u, 
	57u, 95u, 110u, 48u, 57u, 10u, 42u, 47u, 
	61u, 92u, 100u, 103u, 105u, 109u, 115u, 121u, 
	117u, 118u, 42u, 100u, 103u, 105u, 109u, 115u, 
	121u, 117u, 118u, 10u, 10u, 47u, 92u, 46u, 
	66u, 69u, 79u, 88u, 95u, 98u, 101u, 110u, 
	111u, 120u, 48u, 57u, 69u, 101u, 110u, 48u, 
	57u, 69u, 95u, 101u, 110u, 48u, 57u, 95u, 
	110u, 48u, 57u, 46u, 69u, 95u, 101u, 110u, 
	48u, 57u, 95u, 110u, 48u, 57u, 95u, 110u, 
	48u, 49u, 95u, 110u, 48u, 55u, 95u, 110u, 
	48u, 57u, 65u, 70u, 97u, 102u, 60u, 61u, 
	61u, 62u, 61u, 62u, 61u, 62u, 46u, 63u, 
	61u, 124u, 0
};

static const char _JST_single_lengths[] = {
	0, 2, 8, 1, 0, 0, 0, 0, 
	1, 1, 1, 1, 1, 1, 3, 2, 
	8, 1, 0, 0, 0, 0, 1, 1, 
	1, 1, 1, 1, 1, 2, 0, 0, 
	3, 0, 4, 1, 2, 4, 1, 2, 
	0, 0, 2, 0, 0, 0, 0, 0, 
	2, 0, 21, 1, 1, 1, 1, 2, 
	2, 2, 2, 2, 1, 4, 2, 5, 
	6, 7, 1, 3, 11, 3, 4, 2, 
	5, 2, 2, 2, 2, 2, 2, 2, 
	2, 2, 2
};

static const char _JST_range_lengths[] = {
	0, 0, 4, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 0, 2, 0, 
	4, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 0, 0, 1, 1, 1, 
	0, 0, 0, 0, 0, 0, 0, 1, 
	1, 1, 1, 1, 1, 1, 1, 3, 
	0, 0, 4, 1, 0, 0, 0, 3, 
	0, 0, 0, 0, 1, 1, 1, 0, 
	1, 1, 0, 0, 1, 1, 1, 1, 
	1, 1, 1, 1, 3, 0, 0, 0, 
	0, 0, 0
};

static const short _JST_index_offsets[] = {
	0, 0, 3, 16, 21, 25, 29, 33, 
	37, 42, 47, 52, 57, 62, 64, 70, 
	73, 86, 91, 95, 99, 103, 107, 112, 
	117, 122, 127, 132, 134, 136, 140, 142, 
	144, 148, 149, 154, 156, 159, 164, 166, 
	170, 172, 174, 178, 180, 182, 184, 186, 
	190, 193, 194, 220, 223, 225, 227, 229, 
	235, 238, 241, 244, 247, 250, 256, 260, 
	266, 274, 283, 285, 289, 302, 307, 313, 
	317, 324, 328, 332, 336, 342, 345, 348, 
	351, 354, 357
};

static const char _JST_indicies[] = {
	1, 2, 0, 0, 0, 0, 0, 0, 
	0, 4, 5, 0, 0, 0, 0, 3, 
	7, 6, 6, 6, 3, 5, 5, 5, 
	3, 8, 8, 8, 3, 0, 0, 0, 
	3, 9, 9, 9, 3, 0, 10, 10, 
	10, 3, 0, 11, 11, 11, 3, 0, 
	12, 12, 12, 3, 0, 13, 13, 13, 
	3, 0, 14, 14, 14, 3, 0, 3, 
	15, 16, 16, 16, 16, 3, 18, 19, 
	17, 17, 17, 17, 17, 17, 17, 20, 
	21, 17, 17, 17, 17, 3, 23, 22, 
	22, 22, 3, 21, 21, 21, 3, 24, 
	24, 24, 3, 17, 17, 17, 3, 25, 
	25, 25, 3, 17, 26, 26, 26, 3, 
	17, 27, 27, 27, 3, 17, 28, 28, 
	28, 3, 17, 29, 29, 29, 3, 17, 
	30, 30, 30, 3, 17, 3, 32, 31, 
	34, 34, 35, 33, 35, 33, 36, 33, 
	37, 39, 40, 38, 38, 43, 44, 45, 
	46, 42, 47, 43, 47, 48, 43, 43, 
	44, 49, 46, 42, 44, 42, 51, 51, 
	52, 50, 52, 50, 53, 50, 55, 55, 
	56, 54, 56, 37, 57, 54, 58, 37, 
	59, 37, 60, 60, 60, 37, 62, 63, 
	61, 61, 65, 66, 0, 67, 16, 68, 
	69, 17, 70, 71, 72, 73, 74, 75, 
	76, 77, 78, 79, 68, 61, 80, 65, 
	57, 16, 16, 64, 65, 65, 81, 83, 
	82, 32, 37, 84, 15, 16, 16, 16, 
	16, 16, 85, 83, 32, 82, 83, 32, 
	82, 32, 32, 82, 32, 32, 82, 87, 
	36, 86, 89, 90, 89, 91, 36, 88, 
	34, 91, 35, 88, 82, 42, 92, 93, 
	40, 38, 39, 39, 39, 39, 39, 39, 
	39, 37, 47, 45, 45, 45, 45, 45, 
	45, 45, 43, 95, 92, 96, 39, 40, 
	38, 98, 99, 100, 101, 102, 103, 99, 
	100, 104, 101, 102, 57, 97, 106, 106, 
	107, 53, 105, 106, 108, 106, 107, 53, 
	105, 51, 107, 52, 105, 98, 100, 103, 
	100, 104, 57, 97, 55, 110, 56, 109, 
	99, 112, 58, 111, 101, 114, 59, 113, 
	102, 116, 60, 60, 60, 115, 83, 32, 
	82, 83, 32, 82, 32, 117, 82, 32, 
	83, 96, 32, 83, 82, 32, 83, 82, 
	0
};

static const char _JST_trans_targs[] = {
	1, 50, 2, 0, 3, 5, 4, 7, 
	6, 8, 9, 10, 11, 12, 13, 54, 
	55, 15, 50, 16, 17, 19, 18, 21, 
	20, 22, 23, 24, 25, 26, 27, 50, 
	50, 50, 30, 62, 61, 50, 32, 64, 
	33, 50, 34, 35, 37, 65, 38, 36, 
	50, 64, 50, 40, 71, 70, 50, 43, 
	73, 72, 74, 75, 76, 48, 49, 50, 
	50, 51, 52, 14, 53, 56, 57, 58, 
	59, 60, 63, 68, 77, 78, 79, 81, 
	82, 50, 50, 53, 50, 50, 50, 28, 
	50, 29, 31, 50, 66, 67, 50, 50, 
	50, 50, 69, 45, 42, 46, 47, 44, 
	50, 50, 39, 50, 41, 50, 50, 50, 
	50, 50, 50, 50, 50, 80
};

static const char _JST_trans_actions[] = {
	0, 11, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 13, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 71, 
	29, 65, 0, 5, 5, 73, 0, 78, 
	0, 69, 0, 0, 0, 78, 0, 0, 
	7, 75, 63, 0, 5, 5, 67, 0, 
	90, 93, 87, 84, 81, 0, 0, 9, 
	31, 0, 0, 0, 99, 0, 0, 0, 
	0, 5, 99, 93, 0, 0, 0, 0, 
	0, 61, 57, 96, 35, 53, 59, 0, 
	47, 0, 0, 23, 0, 96, 37, 33, 
	55, 51, 5, 0, 0, 0, 0, 0, 
	27, 45, 0, 21, 0, 49, 25, 43, 
	19, 41, 17, 39, 15, 0
};

static const char _JST_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 1, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const char _JST_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 3, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const short _JST_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 32, 34, 34, 34, 
	38, 38, 42, 38, 38, 42, 42, 51, 
	51, 51, 55, 38, 55, 38, 38, 38, 
	0, 0, 0, 82, 83, 38, 85, 86, 
	83, 83, 83, 83, 87, 89, 89, 83, 
	38, 95, 96, 97, 98, 106, 106, 106, 
	98, 110, 112, 114, 116, 83, 83, 83, 
	97, 83, 83
};

static const int JST_start = 50;
static const int JST_first_final = 50;
static const int JST_error = 0;

static const int JST_en_main = 50;


/* #line 132 "tok/JST.c.rl" */

ok64 JSTLexer(JSTstate* state) {

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

    
/* #line 289 "tok/JST.rl.c" */
	{
	cs = JST_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 150 "tok/JST.c.rl" */
    
/* #line 295 "tok/JST.rl.c" */
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
	_acts = _JST_actions + _JST_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 314 "tok/JST.rl.c" */
		}
	}

	_keys = _JST_trans_keys + _JST_key_offsets[cs];
	_trans = _JST_index_offsets[cs];

	_klen = _JST_single_lengths[cs];
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

	_klen = _JST_range_lengths[cs];
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
	_trans = _JST_indicies[_trans];
_eof_trans:
	cs = _JST_trans_targs[_trans];

	if ( _JST_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _JST_actions + _JST_trans_actions[_trans];
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
/* #line 32 "tok/JST.c.rl" */
	{act = 2;}
	break;
	case 4:
/* #line 38 "tok/JST.c.rl" */
	{act = 7;}
	break;
	case 5:
/* #line 44 "tok/JST.c.rl" */
	{act = 8;}
	break;
	case 6:
/* #line 44 "tok/JST.c.rl" */
	{act = 9;}
	break;
	case 7:
/* #line 44 "tok/JST.c.rl" */
	{act = 10;}
	break;
	case 8:
/* #line 44 "tok/JST.c.rl" */
	{act = 13;}
	break;
	case 9:
/* #line 44 "tok/JST.c.rl" */
	{act = 14;}
	break;
	case 10:
/* #line 56 "tok/JST.c.rl" */
	{act = 16;}
	break;
	case 11:
/* #line 56 "tok/JST.c.rl" */
	{act = 17;}
	break;
	case 12:
/* #line 32 "tok/JST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 38 "tok/JST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 38 "tok/JST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 38 "tok/JST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 44 "tok/JST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 44 "tok/JST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 44 "tok/JST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 44 "tok/JST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 44 "tok/JST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 44 "tok/JST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 44 "tok/JST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 56 "tok/JST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 56 "tok/JST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 32 "tok/JST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 32 "tok/JST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 38 "tok/JST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 44 "tok/JST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 44 "tok/JST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 44 "tok/JST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 44 "tok/JST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 44 "tok/JST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 44 "tok/JST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 44 "tok/JST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 50 "tok/JST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 56 "tok/JST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 56 "tok/JST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 56 "tok/JST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 39:
/* #line 62 "tok/JST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 40:
/* #line 44 "tok/JST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 41:
/* #line 44 "tok/JST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 42:
/* #line 44 "tok/JST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 43:
/* #line 56 "tok/JST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 44:
/* #line 56 "tok/JST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 45:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 2:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 7:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 8:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 9:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 10:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 13:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 14:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 16:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 17:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 750 "tok/JST.rl.c" */
		}
	}

_again:
	_acts = _JST_actions + _JST_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 761 "tok/JST.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _JST_eof_trans[cs] > 0 ) {
		_trans = _JST_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 151 "tok/JST.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < JST_first_final)
        o = JSTBAD;

    return o;
}
