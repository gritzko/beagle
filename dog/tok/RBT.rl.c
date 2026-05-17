
/* #line 1 "RBT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "RBT.h"

ok64 RBTonComment (u8cs tok, RBTstate* state);
ok64 RBTonString (u8cs tok, RBTstate* state);
ok64 RBTonNumber (u8cs tok, RBTstate* state);
ok64 RBTonWord (u8cs tok, RBTstate* state);
ok64 RBTonPunct (u8cs tok, RBTstate* state);
ok64 RBTonSpace (u8cs tok, RBTstate* state);


/* #line 125 "RBT.c.rl" */



/* #line 15 "RBT.rl.c" */
static const char _RBT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	10, 1, 11, 1, 12, 1, 13, 1, 
	14, 1, 15, 1, 16, 1, 17, 1, 
	18, 1, 19, 1, 20, 1, 21, 1, 
	22, 1, 23, 1, 24, 1, 25, 1, 
	26, 1, 27, 1, 28, 1, 29, 1, 
	30, 1, 31, 1, 32, 1, 33, 1, 
	34, 1, 35, 1, 36, 1, 37, 1, 
	38, 1, 39, 1, 40, 1, 41, 1, 
	42, 1, 43, 1, 44, 2, 2, 3, 
	2, 2, 4, 2, 2, 5, 2, 2, 
	6, 2, 2, 7, 2, 2, 8, 2, 
	2, 9
};

static const short _RBT_key_offsets[] = {
	0, 0, 2, 16, 23, 29, 35, 41, 
	47, 54, 61, 68, 75, 82, 83, 99, 
	101, 101, 105, 107, 109, 111, 111, 115, 
	117, 119, 123, 125, 127, 129, 131, 137, 
	143, 145, 159, 166, 172, 178, 184, 190, 
	197, 204, 211, 218, 225, 226, 227, 228, 
	229, 230, 231, 232, 233, 234, 235, 240, 
	270, 273, 275, 276, 283, 284, 287, 289, 
	291, 294, 295, 302, 307, 310, 314, 316, 
	330, 336, 343, 348, 356, 361, 364, 367, 
	374, 383, 385, 386, 390, 391, 393, 400, 
	409
};

static const unsigned char _RBT_trans_keys[] = {
	34u, 92u, 10u, 34u, 39u, 48u, 92u, 110u, 
	117u, 120u, 97u, 98u, 101u, 102u, 114u, 118u, 
	123u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 125u, 
	48u, 57u, 65u, 70u, 97u, 102u, 125u, 48u, 
	57u, 65u, 70u, 97u, 102u, 125u, 48u, 57u, 
	65u, 70u, 97u, 102u, 125u, 48u, 57u, 65u, 
	70u, 97u, 102u, 125u, 48u, 57u, 65u, 70u, 
	97u, 102u, 125u, 36u, 92u, 95u, 126u, 33u, 
	34u, 38u, 39u, 42u, 44u, 46u, 64u, 65u, 
	90u, 97u, 122u, 39u, 92u, 43u, 45u, 48u, 
	57u, 48u, 57u, 48u, 57u, 47u, 92u, 43u, 
	45u, 48u, 57u, 48u, 57u, 48u, 57u, 43u, 
	45u, 48u, 57u, 48u, 57u, 48u, 57u, 48u, 
	49u, 48u, 55u, 48u, 57u, 65u, 70u, 97u, 
	102u, 34u, 95u, 65u, 90u, 97u, 122u, 34u, 
	92u, 10u, 34u, 39u, 48u, 92u, 110u, 117u, 
	120u, 97u, 98u, 101u, 102u, 114u, 118u, 123u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 125u, 48u, 
	57u, 65u, 70u, 97u, 102u, 125u, 48u, 57u, 
	65u, 70u, 97u, 102u, 125u, 48u, 57u, 65u, 
	70u, 97u, 102u, 125u, 48u, 57u, 65u, 70u, 
	97u, 102u, 125u, 48u, 57u, 65u, 70u, 97u, 
	102u, 125u, 101u, 103u, 105u, 110u, 10u, 61u, 
	101u, 110u, 100u, 95u, 65u, 90u, 97u, 122u, 
	32u, 33u, 34u, 35u, 36u, 37u, 38u, 39u, 
	42u, 43u, 45u, 46u, 47u, 48u, 58u, 60u, 
	61u, 62u, 64u, 94u, 95u, 124u, 9u, 13u, 
	49u, 57u, 65u, 90u, 97u, 122u, 32u, 9u, 
	13u, 61u, 126u, 10u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 61u, 38u, 46u, 61u, 42u, 
	61u, 61u, 62u, 46u, 48u, 57u, 46u, 69u, 
	95u, 101u, 105u, 114u, 48u, 57u, 95u, 105u, 
	114u, 48u, 57u, 47u, 61u, 92u, 105u, 109u, 
	111u, 120u, 47u, 92u, 46u, 66u, 69u, 79u, 
	88u, 95u, 98u, 101u, 105u, 111u, 114u, 120u, 
	48u, 57u, 69u, 101u, 105u, 114u, 48u, 57u, 
	69u, 95u, 101u, 105u, 114u, 48u, 57u, 95u, 
	105u, 114u, 48u, 57u, 46u, 69u, 95u, 101u, 
	105u, 114u, 48u, 57u, 95u, 105u, 114u, 48u, 
	57u, 95u, 48u, 49u, 95u, 48u, 55u, 95u, 
	48u, 57u, 65u, 70u, 97u, 102u, 33u, 63u, 
	95u, 48u, 57u, 65u, 90u, 97u, 122u, 60u, 
	61u, 62u, 98u, 126u, 61u, 62u, 10u, 61u, 
	62u, 95u, 48u, 57u, 65u, 90u, 97u, 122u, 
	33u, 63u, 95u, 48u, 57u, 65u, 90u, 97u, 
	122u, 61u, 124u, 0
};

static const char _RBT_single_lengths[] = {
	0, 2, 8, 1, 0, 0, 0, 0, 
	1, 1, 1, 1, 1, 1, 4, 2, 
	0, 2, 0, 0, 2, 0, 2, 0, 
	0, 2, 0, 0, 0, 0, 0, 2, 
	2, 8, 1, 0, 0, 0, 0, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 22, 
	1, 2, 1, 1, 1, 3, 2, 0, 
	1, 1, 5, 3, 3, 4, 2, 12, 
	4, 5, 3, 6, 3, 1, 1, 1, 
	3, 2, 1, 2, 1, 2, 1, 3, 
	2
};

static const char _RBT_range_lengths[] = {
	0, 0, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 0, 6, 0, 
	0, 1, 1, 1, 0, 0, 1, 1, 
	1, 1, 1, 1, 1, 1, 3, 2, 
	0, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 2, 4, 
	1, 0, 0, 3, 0, 0, 0, 1, 
	1, 0, 1, 1, 0, 0, 0, 1, 
	1, 1, 1, 1, 1, 1, 1, 3, 
	3, 0, 0, 1, 0, 0, 3, 3, 
	0
};

static const short _RBT_index_offsets[] = {
	0, 0, 3, 15, 20, 24, 28, 32, 
	36, 41, 46, 51, 56, 61, 63, 74, 
	77, 78, 82, 84, 86, 89, 90, 94, 
	96, 98, 102, 104, 106, 108, 110, 114, 
	119, 122, 134, 139, 143, 147, 151, 155, 
	160, 165, 170, 175, 180, 182, 184, 186, 
	188, 190, 192, 194, 196, 198, 200, 204, 
	231, 234, 237, 239, 244, 246, 250, 253, 
	255, 258, 260, 267, 272, 276, 281, 284, 
	298, 304, 311, 316, 324, 329, 332, 335, 
	340, 347, 350, 352, 356, 358, 361, 366, 
	373
};

static const char _RBT_indicies[] = {
	1, 2, 0, 0, 0, 0, 0, 0, 
	0, 4, 5, 0, 0, 0, 3, 7, 
	6, 6, 6, 3, 5, 5, 5, 3, 
	8, 8, 8, 3, 0, 0, 0, 3, 
	9, 9, 9, 3, 0, 10, 10, 10, 
	3, 0, 11, 11, 11, 3, 0, 12, 
	12, 12, 3, 0, 13, 13, 13, 3, 
	0, 14, 14, 14, 3, 0, 3, 15, 
	15, 16, 15, 15, 15, 15, 15, 16, 
	16, 3, 18, 19, 17, 17, 21, 21, 
	22, 20, 22, 20, 23, 20, 26, 27, 
	25, 25, 29, 29, 30, 28, 30, 28, 
	31, 28, 33, 33, 34, 32, 34, 24, 
	35, 32, 36, 24, 37, 24, 38, 38, 
	38, 24, 39, 40, 40, 40, 3, 41, 
	42, 39, 39, 39, 39, 39, 39, 39, 
	43, 44, 39, 39, 39, 3, 46, 45, 
	45, 45, 3, 44, 44, 44, 3, 47, 
	47, 47, 3, 39, 39, 39, 3, 48, 
	48, 48, 3, 39, 49, 49, 49, 3, 
	39, 50, 50, 50, 3, 39, 51, 51, 
	51, 3, 39, 52, 52, 52, 3, 39, 
	53, 53, 53, 3, 39, 3, 55, 54, 
	56, 54, 57, 54, 58, 54, 59, 58, 
	60, 59, 61, 59, 62, 59, 63, 59, 
	64, 64, 64, 3, 66, 67, 0, 68, 
	69, 70, 71, 17, 72, 70, 73, 74, 
	75, 76, 77, 78, 79, 80, 81, 70, 
	82, 83, 66, 35, 82, 82, 65, 66, 
	66, 84, 86, 86, 85, 87, 68, 16, 
	16, 16, 16, 88, 86, 24, 89, 86, 
	86, 85, 89, 86, 85, 86, 85, 91, 
	23, 90, 86, 92, 94, 95, 94, 96, 
	96, 23, 93, 21, 96, 96, 22, 93, 
	85, 97, 27, 25, 26, 26, 26, 26, 
	98, 26, 27, 25, 100, 101, 102, 103, 
	104, 105, 101, 102, 106, 103, 106, 104, 
	35, 99, 108, 108, 109, 109, 31, 107, 
	108, 110, 108, 109, 109, 31, 107, 29, 
	109, 109, 30, 107, 100, 102, 105, 102, 
	106, 106, 35, 99, 33, 112, 112, 34, 
	111, 101, 36, 113, 103, 37, 114, 104, 
	38, 38, 38, 115, 117, 117, 40, 40, 
	40, 40, 116, 89, 118, 85, 86, 92, 
	119, 86, 86, 85, 120, 63, 86, 89, 
	85, 64, 64, 64, 64, 121, 123, 123, 
	82, 82, 82, 82, 122, 86, 89, 85, 
	0
};

static const char _RBT_trans_targs[] = {
	1, 55, 2, 0, 3, 5, 4, 7, 
	6, 8, 9, 10, 11, 12, 13, 55, 
	59, 15, 55, 16, 55, 18, 67, 66, 
	55, 20, 69, 21, 55, 23, 74, 73, 
	55, 26, 76, 75, 77, 78, 79, 32, 
	80, 55, 33, 34, 36, 35, 38, 37, 
	39, 40, 41, 42, 43, 44, 55, 46, 
	47, 48, 49, 50, 51, 52, 53, 84, 
	86, 55, 56, 57, 58, 14, 60, 61, 
	62, 63, 64, 68, 71, 31, 81, 83, 
	85, 54, 87, 88, 55, 55, 55, 55, 
	55, 60, 55, 65, 55, 55, 17, 19, 
	55, 70, 55, 55, 72, 28, 25, 29, 
	30, 27, 55, 55, 22, 55, 24, 55, 
	55, 55, 55, 55, 55, 55, 82, 45, 
	55, 55, 55, 55
};

static const char _RBT_trans_actions[] = {
	0, 7, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 23, 
	0, 0, 9, 0, 69, 0, 5, 5, 
	75, 0, 0, 0, 67, 0, 5, 5, 
	71, 0, 86, 89, 83, 80, 77, 0, 
	0, 13, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 73, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 29, 0, 0, 0, 0, 95, 0, 
	0, 0, 0, 95, 89, 0, 0, 5, 
	0, 0, 0, 0, 65, 61, 27, 31, 
	55, 92, 63, 0, 59, 47, 0, 0, 
	17, 92, 35, 51, 5, 0, 0, 0, 
	0, 0, 21, 45, 0, 15, 0, 49, 
	19, 43, 41, 39, 37, 11, 0, 0, 
	33, 53, 57, 25
};

static const char _RBT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 1, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const char _RBT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 3, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const short _RBT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 21, 21, 21, 25, 25, 29, 29, 
	29, 33, 25, 33, 25, 25, 25, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 55, 55, 55, 
	55, 55, 55, 55, 55, 55, 0, 0, 
	85, 86, 88, 89, 25, 86, 86, 86, 
	91, 93, 94, 94, 86, 99, 93, 100, 
	108, 108, 108, 100, 112, 114, 115, 116, 
	117, 86, 93, 86, 121, 86, 122, 123, 
	86
};

static const int RBT_start = 55;
static const int RBT_first_final = 55;
static const int RBT_error = 0;

static const int RBT_en_main = 55;


/* #line 128 "RBT.c.rl" */

ok64 RBTLexer(RBTstate* state) {

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

    
/* #line 303 "RBT.rl.c" */
	{
	cs = RBT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 146 "RBT.c.rl" */
    
/* #line 309 "RBT.rl.c" */
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
	_acts = _RBT_actions + _RBT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 328 "RBT.rl.c" */
		}
	}

	_keys = _RBT_trans_keys + _RBT_key_offsets[cs];
	_trans = _RBT_index_offsets[cs];

	_klen = _RBT_single_lengths[cs];
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

	_klen = _RBT_range_lengths[cs];
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
	_trans = _RBT_indicies[_trans];
_eof_trans:
	cs = _RBT_trans_targs[_trans];

	if ( _RBT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _RBT_actions + _RBT_trans_actions[_trans];
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
/* #line 44 "RBT.c.rl" */
	{act = 8;}
	break;
	case 4:
/* #line 44 "RBT.c.rl" */
	{act = 9;}
	break;
	case 5:
/* #line 44 "RBT.c.rl" */
	{act = 10;}
	break;
	case 6:
/* #line 44 "RBT.c.rl" */
	{act = 13;}
	break;
	case 7:
/* #line 44 "RBT.c.rl" */
	{act = 14;}
	break;
	case 8:
/* #line 56 "RBT.c.rl" */
	{act = 19;}
	break;
	case 9:
/* #line 56 "RBT.c.rl" */
	{act = 20;}
	break;
	case 10:
/* #line 38 "RBT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 38 "RBT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 38 "RBT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 38 "RBT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 44 "RBT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 44 "RBT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 44 "RBT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 44 "RBT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 50 "RBT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 50 "RBT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 56 "RBT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 56 "RBT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 32 "RBT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 32 "RBT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 38 "RBT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 38 "RBT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 44 "RBT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 44 "RBT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 44 "RBT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 44 "RBT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 44 "RBT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 44 "RBT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 44 "RBT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 50 "RBT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 50 "RBT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 50 "RBT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 56 "RBT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 56 "RBT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 56 "RBT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 39:
/* #line 62 "RBT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 40:
/* #line 44 "RBT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 41:
/* #line 44 "RBT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 42:
/* #line 44 "RBT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 43:
/* #line 56 "RBT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 44:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 8:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 9:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 10:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 13:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 14:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 19:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 20:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 750 "RBT.rl.c" */
		}
	}

_again:
	_acts = _RBT_actions + _RBT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 761 "RBT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _RBT_eof_trans[cs] > 0 ) {
		_trans = _RBT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 147 "RBT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < RBT_first_final)
        o = RBTBAD;

    return o;
}
