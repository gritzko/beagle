
/* #line 1 "NIMT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "NIMT.h"

ok64 NIMTonComment (u8cs tok, NIMTstate* state);
ok64 NIMTonString (u8cs tok, NIMTstate* state);
ok64 NIMTonNumber (u8cs tok, NIMTstate* state);
ok64 NIMTonWord (u8cs tok, NIMTstate* state);
ok64 NIMTonPunct (u8cs tok, NIMTstate* state);
ok64 NIMTonSpace (u8cs tok, NIMTstate* state);


/* #line 120 "NIMT.c.rl" */



/* #line 15 "NIMT.rl.c" */
static const char _NIMT_actions[] = {
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

static const short _NIMT_key_offsets[] = {
	0, 0, 2, 4, 21, 27, 33, 34, 
	35, 36, 37, 38, 40, 41, 58, 61, 
	64, 70, 76, 80, 81, 82, 86, 87, 
	91, 92, 93, 97, 98, 102, 104, 106, 
	110, 112, 116, 117, 118, 122, 123, 125, 
	127, 131, 132, 133, 137, 138, 140, 144, 
	145, 146, 150, 151, 157, 161, 162, 163, 
	167, 168, 169, 170, 205, 208, 209, 210, 
	212, 213, 215, 217, 219, 221, 222, 223, 
	236, 238, 242, 247, 249, 253, 259, 263, 
	270, 274, 276, 280, 284, 286, 290, 294, 
	296, 300, 308, 310, 314, 316, 323, 331, 
	339, 346, 354, 361, 369, 377, 384, 391, 
	399, 406
};

static const unsigned char _NIMT_trans_keys[] = {
	34u, 92u, 34u, 92u, 10u, 34u, 39u, 63u, 
	92u, 108u, 110u, 114u, 116u, 118u, 120u, 48u, 
	57u, 97u, 99u, 101u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 34u, 34u, 34u, 93u, 35u, 39u, 92u, 
	39u, 10u, 34u, 39u, 63u, 92u, 108u, 110u, 
	114u, 116u, 118u, 120u, 48u, 57u, 97u, 99u, 
	101u, 102u, 39u, 48u, 57u, 39u, 48u, 57u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 100u, 102u, 105u, 117u, 
	50u, 52u, 49u, 51u, 54u, 56u, 54u, 100u, 
	102u, 105u, 117u, 50u, 52u, 49u, 51u, 54u, 
	56u, 54u, 43u, 45u, 48u, 57u, 48u, 57u, 
	48u, 57u, 43u, 45u, 48u, 57u, 48u, 57u, 
	100u, 102u, 105u, 117u, 50u, 52u, 49u, 51u, 
	54u, 56u, 54u, 48u, 57u, 48u, 49u, 100u, 
	102u, 105u, 117u, 50u, 52u, 49u, 51u, 54u, 
	56u, 54u, 48u, 55u, 100u, 102u, 105u, 117u, 
	50u, 52u, 49u, 51u, 54u, 56u, 54u, 48u, 
	57u, 65u, 70u, 97u, 102u, 100u, 102u, 105u, 
	117u, 50u, 52u, 49u, 51u, 54u, 56u, 54u, 
	34u, 34u, 32u, 33u, 34u, 35u, 38u, 39u, 
	42u, 43u, 45u, 46u, 47u, 48u, 60u, 82u, 
	94u, 95u, 97u, 100u, 109u, 110u, 111u, 114u, 
	115u, 120u, 124u, 9u, 13u, 49u, 57u, 61u, 
	62u, 65u, 90u, 98u, 122u, 32u, 9u, 13u, 
	61u, 34u, 10u, 91u, 10u, 10u, 93u, 10u, 
	35u, 42u, 61u, 61u, 62u, 46u, 46u, 39u, 
	46u, 66u, 69u, 79u, 88u, 95u, 98u, 101u, 
	111u, 120u, 48u, 57u, 51u, 54u, 49u, 51u, 
	54u, 56u, 39u, 69u, 101u, 48u, 57u, 51u, 
	54u, 49u, 51u, 54u, 56u, 39u, 69u, 95u, 
	101u, 48u, 57u, 39u, 95u, 48u, 57u, 39u, 
	46u, 69u, 95u, 101u, 48u, 57u, 39u, 95u, 
	48u, 57u, 51u, 54u, 49u, 51u, 54u, 56u, 
	39u, 95u, 48u, 49u, 51u, 54u, 49u, 51u, 
	54u, 56u, 39u, 95u, 48u, 55u, 51u, 54u, 
	49u, 51u, 54u, 56u, 39u, 95u, 48u, 57u, 
	65u, 70u, 97u, 102u, 51u, 54u, 49u, 51u, 
	54u, 56u, 60u, 61u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 34u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 95u, 110u, 48u, 57u, 65u, 
	90u, 97u, 122u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 95u, 105u, 48u, 57u, 65u, 90u, 
	97u, 122u, 95u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 111u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 111u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 48u, 57u, 65u, 90u, 97u, 122u, 
	95u, 48u, 57u, 65u, 90u, 97u, 122u, 95u, 
	104u, 48u, 57u, 65u, 90u, 97u, 122u, 95u, 
	48u, 57u, 65u, 90u, 97u, 122u, 95u, 111u, 
	48u, 57u, 65u, 90u, 97u, 122u, 0
};

static const char _NIMT_single_lengths[] = {
	0, 2, 2, 11, 0, 0, 1, 1, 
	1, 1, 1, 2, 1, 11, 1, 1, 
	0, 0, 4, 1, 1, 4, 1, 4, 
	1, 1, 4, 1, 2, 0, 0, 2, 
	0, 4, 1, 1, 4, 1, 0, 0, 
	4, 1, 1, 4, 1, 0, 4, 1, 
	1, 4, 1, 0, 4, 1, 1, 4, 
	1, 1, 1, 25, 1, 1, 1, 2, 
	1, 2, 2, 2, 0, 1, 1, 11, 
	2, 4, 3, 2, 4, 4, 2, 5, 
	2, 2, 4, 2, 2, 4, 2, 2, 
	4, 2, 2, 4, 0, 1, 2, 2, 
	1, 2, 1, 2, 2, 1, 1, 2, 
	1, 2
};

static const char _NIMT_range_lengths[] = {
	0, 0, 0, 3, 3, 3, 0, 0, 
	0, 0, 0, 0, 0, 3, 1, 1, 
	3, 3, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 1, 1, 1, 1, 
	1, 0, 0, 0, 0, 0, 1, 1, 
	0, 0, 0, 0, 0, 1, 0, 0, 
	0, 0, 0, 3, 0, 0, 0, 0, 
	0, 0, 0, 5, 1, 0, 0, 0, 
	0, 0, 0, 0, 1, 0, 0, 1, 
	0, 0, 1, 0, 0, 1, 1, 1, 
	1, 0, 0, 1, 0, 0, 1, 0, 
	0, 3, 0, 0, 1, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 
	3, 3
};

static const short _NIMT_index_offsets[] = {
	0, 0, 3, 6, 21, 25, 29, 31, 
	33, 35, 37, 39, 42, 44, 59, 62, 
	65, 69, 73, 78, 80, 82, 87, 89, 
	94, 96, 98, 103, 105, 109, 111, 113, 
	117, 119, 124, 126, 128, 133, 135, 137, 
	139, 144, 146, 148, 153, 155, 157, 162, 
	164, 166, 171, 173, 177, 182, 184, 186, 
	191, 193, 195, 197, 228, 231, 233, 235, 
	238, 240, 243, 246, 249, 251, 253, 255, 
	268, 271, 276, 281, 284, 289, 295, 299, 
	306, 310, 313, 318, 322, 325, 330, 334, 
	337, 342, 348, 351, 356, 358, 363, 369, 
	375, 380, 386, 391, 397, 403, 408, 413, 
	419, 424
};

static const unsigned char _NIMT_indicies[] = {
	1, 2, 0, 3, 2, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	5, 0, 0, 0, 4, 6, 6, 6, 
	4, 0, 0, 0, 4, 9, 8, 10, 
	8, 11, 8, 14, 13, 15, 13, 4, 
	17, 16, 18, 4, 16, 16, 16, 16, 
	16, 16, 16, 16, 16, 16, 20, 19, 
	16, 16, 4, 18, 21, 4, 18, 16, 
	4, 22, 22, 22, 4, 16, 16, 16, 
	4, 24, 25, 26, 27, 23, 24, 23, 
	24, 23, 28, 29, 30, 24, 23, 24, 
	23, 32, 33, 34, 35, 31, 32, 31, 
	32, 31, 36, 37, 38, 32, 31, 32, 
	31, 39, 39, 40, 31, 40, 31, 41, 
	31, 42, 42, 43, 23, 43, 7, 45, 
	46, 47, 48, 44, 45, 44, 45, 44, 
	49, 50, 51, 45, 44, 45, 44, 52, 
	23, 53, 7, 55, 56, 57, 58, 54, 
	55, 54, 55, 54, 59, 60, 61, 55, 
	54, 55, 54, 62, 7, 64, 65, 66, 
	67, 63, 64, 63, 64, 63, 68, 69, 
	70, 64, 63, 64, 63, 71, 71, 71, 
	7, 73, 74, 75, 76, 72, 73, 72, 
	73, 72, 77, 78, 79, 73, 72, 73, 
	72, 82, 81, 83, 81, 85, 86, 87, 
	88, 86, 89, 90, 86, 91, 92, 86, 
	93, 94, 96, 86, 95, 97, 98, 99, 
	100, 101, 96, 102, 103, 86, 85, 52, 
	91, 95, 95, 84, 85, 85, 104, 106, 
	105, 8, 7, 107, 109, 108, 7, 108, 
	13, 110, 109, 13, 111, 109, 106, 106, 
	105, 106, 105, 113, 112, 106, 114, 116, 
	117, 118, 119, 120, 121, 122, 118, 119, 
	120, 121, 52, 115, 29, 30, 115, 28, 
	29, 30, 24, 115, 124, 125, 125, 41, 
	123, 37, 38, 123, 36, 37, 38, 32, 
	123, 124, 125, 126, 125, 41, 123, 124, 
	39, 40, 123, 116, 117, 119, 122, 119, 
	52, 115, 128, 42, 43, 127, 50, 51, 
	127, 49, 50, 51, 45, 127, 130, 118, 
	53, 129, 60, 61, 129, 59, 60, 61, 
	55, 129, 132, 120, 62, 131, 69, 70, 
	131, 68, 69, 70, 64, 131, 134, 121, 
	71, 71, 71, 133, 78, 79, 133, 77, 
	78, 79, 73, 133, 106, 105, 95, 95, 
	95, 95, 135, 136, 95, 95, 95, 95, 
	135, 95, 137, 95, 95, 95, 135, 95, 
	95, 95, 95, 135, 95, 138, 95, 95, 
	95, 135, 95, 95, 95, 95, 135, 95, 
	137, 95, 95, 95, 135, 95, 139, 95, 
	95, 95, 135, 95, 95, 95, 95, 135, 
	95, 95, 95, 95, 135, 95, 140, 95, 
	95, 95, 135, 95, 95, 95, 95, 135, 
	95, 101, 95, 95, 95, 135, 0
};

static const char _NIMT_trans_targs[] = {
	2, 62, 3, 59, 0, 4, 5, 59, 
	6, 7, 8, 59, 59, 9, 10, 59, 
	12, 13, 59, 14, 16, 15, 17, 59, 
	59, 72, 21, 73, 22, 19, 20, 59, 
	59, 75, 26, 76, 27, 24, 25, 29, 
	78, 77, 32, 80, 59, 59, 81, 36, 
	82, 37, 34, 35, 79, 83, 59, 59, 
	84, 43, 85, 44, 41, 42, 86, 59, 
	59, 87, 49, 88, 50, 47, 48, 89, 
	59, 59, 90, 55, 91, 56, 53, 54, 
	59, 58, 62, 59, 59, 60, 61, 1, 
	63, 11, 67, 68, 69, 71, 92, 93, 
	94, 95, 97, 99, 100, 102, 103, 105, 
	59, 59, 59, 59, 64, 65, 66, 64, 
	59, 70, 59, 59, 18, 74, 39, 31, 
	45, 51, 38, 59, 23, 28, 30, 59, 
	33, 59, 40, 59, 46, 59, 52, 59, 
	57, 96, 98, 101, 104
};

static const char _NIMT_trans_actions[] = {
	0, 84, 0, 13, 0, 0, 0, 73, 
	0, 0, 0, 9, 57, 0, 0, 7, 
	0, 0, 15, 0, 0, 0, 0, 69, 
	27, 5, 0, 5, 0, 0, 0, 65, 
	23, 5, 0, 5, 0, 0, 0, 0, 
	5, 5, 0, 96, 67, 25, 5, 0, 
	5, 0, 0, 0, 99, 93, 63, 21, 
	5, 0, 5, 0, 0, 0, 90, 61, 
	19, 5, 0, 5, 0, 0, 0, 87, 
	59, 17, 5, 0, 5, 0, 0, 0, 
	71, 0, 81, 11, 31, 0, 0, 0, 
	0, 0, 0, 0, 0, 99, 0, 0, 
	5, 0, 0, 0, 0, 0, 0, 0, 
	55, 51, 29, 33, 78, 5, 5, 75, 
	53, 0, 49, 45, 0, 5, 0, 0, 
	0, 0, 0, 41, 0, 0, 0, 43, 
	0, 39, 0, 37, 0, 35, 0, 47, 
	0, 0, 0, 0, 0
};

static const char _NIMT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const char _NIMT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 3, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const short _NIMT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 8, 8, 
	8, 13, 13, 0, 0, 0, 0, 0, 
	0, 0, 24, 24, 24, 24, 24, 32, 
	32, 32, 32, 32, 32, 32, 32, 24, 
	8, 45, 45, 45, 45, 45, 24, 8, 
	55, 55, 55, 55, 55, 8, 64, 64, 
	64, 64, 64, 8, 73, 73, 73, 73, 
	73, 81, 81, 0, 105, 106, 8, 108, 
	8, 108, 108, 106, 106, 113, 115, 116, 
	116, 116, 124, 124, 124, 124, 124, 116, 
	128, 128, 128, 130, 130, 130, 132, 132, 
	132, 134, 134, 134, 106, 136, 136, 136, 
	136, 136, 136, 136, 136, 136, 136, 136, 
	136, 136
};

static const int NIMT_start = 59;
static const int NIMT_first_final = 59;
static const int NIMT_error = 0;

static const int NIMT_en_main = 59;


/* #line 123 "NIMT.c.rl" */

ok64 NIMTLexer(NIMTstate* state) {

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

    
/* #line 327 "NIMT.rl.c" */
	{
	cs = NIMT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 141 "NIMT.c.rl" */
    
/* #line 333 "NIMT.rl.c" */
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
	_acts = _NIMT_actions + _NIMT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 352 "NIMT.rl.c" */
		}
	}

	_keys = _NIMT_trans_keys + _NIMT_key_offsets[cs];
	_trans = _NIMT_index_offsets[cs];

	_klen = _NIMT_single_lengths[cs];
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

	_klen = _NIMT_range_lengths[cs];
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
	_trans = _NIMT_indicies[_trans];
_eof_trans:
	cs = _NIMT_trans_targs[_trans];

	if ( _NIMT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _NIMT_actions + _NIMT_trans_actions[_trans];
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
/* #line 29 "NIMT.c.rl" */
	{act = 1;}
	break;
	case 4:
/* #line 29 "NIMT.c.rl" */
	{act = 2;}
	break;
	case 5:
/* #line 35 "NIMT.c.rl" */
	{act = 4;}
	break;
	case 6:
/* #line 35 "NIMT.c.rl" */
	{act = 5;}
	break;
	case 7:
/* #line 41 "NIMT.c.rl" */
	{act = 7;}
	break;
	case 8:
/* #line 41 "NIMT.c.rl" */
	{act = 8;}
	break;
	case 9:
/* #line 41 "NIMT.c.rl" */
	{act = 9;}
	break;
	case 10:
/* #line 41 "NIMT.c.rl" */
	{act = 11;}
	break;
	case 11:
/* #line 41 "NIMT.c.rl" */
	{act = 12;}
	break;
	case 12:
/* #line 29 "NIMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 35 "NIMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 35 "NIMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 35 "NIMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 35 "NIMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 41 "NIMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 41 "NIMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 41 "NIMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 41 "NIMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 41 "NIMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 41 "NIMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 53 "NIMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 53 "NIMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 29 "NIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 41 "NIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 41 "NIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 41 "NIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 41 "NIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 41 "NIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 41 "NIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 47 "NIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 53 "NIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 53 "NIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 53 "NIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 59 "NIMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 29 "NIMT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 41 "NIMT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 39:
/* #line 41 "NIMT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 40:
/* #line 41 "NIMT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 41:
/* #line 41 "NIMT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 42:
/* #line 41 "NIMT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 43:
/* #line 41 "NIMT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 44:
/* #line 47 "NIMT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 45:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 1:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 2:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 4:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 5:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 7:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 8:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 9:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 11:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 788 "NIMT.rl.c" */
		}
	}

_again:
	_acts = _NIMT_actions + _NIMT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 799 "NIMT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _NIMT_eof_trans[cs] > 0 ) {
		_trans = _NIMT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 142 "NIMT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < NIMT_first_final)
        o = NIMTBAD;

    return o;
}
