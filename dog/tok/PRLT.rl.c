
/* #line 1 "PRLT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "PRLT.h"

ok64 PRLTonComment (u8cs tok, PRLTstate* state);
ok64 PRLTonString (u8cs tok, PRLTstate* state);
ok64 PRLTonNumber (u8cs tok, PRLTstate* state);
ok64 PRLTonWord (u8cs tok, PRLTstate* state);
ok64 PRLTonPunct (u8cs tok, PRLTstate* state);
ok64 PRLTonSpace (u8cs tok, PRLTstate* state);


/* #line 129 "PRLT.c.rl" */



/* #line 15 "PRLT.rl.c" */
static const char _PRLT_actions[] = {
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

static const short _PRLT_key_offsets[] = {
	0, 2, 2, 4, 4, 8, 10, 12, 
	16, 18, 20, 24, 26, 28, 32, 34, 
	40, 42, 43, 44, 45, 46, 47, 48, 
	49, 50, 52, 53, 54, 55, 56, 57, 
	58, 59, 60, 61, 62, 64, 65, 66, 
	67, 68, 69, 70, 71, 72, 77, 79, 
	79, 85, 92, 92, 98, 100, 102, 102, 
	102, 138, 141, 143, 145, 146, 161, 168, 
	174, 176, 177, 179, 181, 183, 186, 190, 
	191, 196, 199, 201, 213, 217, 222, 225, 
	233, 239, 242, 245, 252, 254, 255, 266, 
	267, 269, 274, 282, 290, 298, 307, 315, 
	323, 339, 353, 367, 376, 384
};

static const unsigned char _PRLT_trans_keys[] = {
	34u, 92u, 39u, 92u, 43u, 45u, 48u, 57u, 
	48u, 57u, 48u, 57u, 43u, 45u, 48u, 57u, 
	48u, 57u, 48u, 57u, 43u, 45u, 48u, 57u, 
	48u, 57u, 48u, 57u, 48u, 55u, 56u, 57u, 
	48u, 49u, 48u, 57u, 65u, 70u, 97u, 102u, 
	97u, 101u, 99u, 107u, 103u, 105u, 110u, 117u, 
	116u, 110u, 99u, 100u, 111u, 100u, 105u, 110u, 
	103u, 111u, 114u, 101u, 97u, 100u, 49u, 52u, 
	116u, 101u, 109u, 118u, 101u, 111u, 100u, 58u, 
	95u, 65u, 90u, 97u, 122u, 47u, 92u, 33u, 
	41u, 47u, 93u, 124u, 125u, 33u, 41u, 47u, 
	92u, 93u, 124u, 125u, 33u, 41u, 47u, 93u, 
	124u, 125u, 47u, 92u, 47u, 92u, 32u, 33u, 
	34u, 35u, 36u, 37u, 38u, 39u, 42u, 43u, 
	45u, 46u, 47u, 48u, 60u, 61u, 62u, 64u, 
	94u, 95u, 101u, 103u, 108u, 109u, 110u, 113u, 
	115u, 124u, 9u, 13u, 49u, 57u, 65u, 90u, 
	97u, 122u, 32u, 9u, 13u, 61u, 126u, 34u, 
	92u, 10u, 33u, 59u, 64u, 92u, 95u, 38u, 
	39u, 43u, 44u, 46u, 57u, 65u, 90u, 97u, 
	122u, 95u, 48u, 57u, 65u, 90u, 97u, 122u, 
	61u, 95u, 65u, 90u, 97u, 122u, 38u, 61u, 
	61u, 39u, 92u, 42u, 61u, 43u, 61u, 45u, 
	61u, 62u, 46u, 61u, 48u, 57u, 46u, 69u, 
	95u, 101u, 48u, 57u, 95u, 48u, 57u, 47u, 
	61u, 46u, 66u, 69u, 88u, 95u, 98u, 101u, 
	120u, 48u, 55u, 56u, 57u, 69u, 101u, 48u, 
	57u, 69u, 95u, 101u, 48u, 57u, 95u, 48u, 
	57u, 46u, 69u, 95u, 101u, 48u, 55u, 56u, 
	57u, 46u, 69u, 95u, 101u, 48u, 57u, 95u, 
	48u, 57u, 95u, 48u, 49u, 95u, 48u, 57u, 
	65u, 70u, 97u, 102u, 60u, 61u, 62u, 98u, 
	99u, 101u, 102u, 104u, 105u, 111u, 112u, 126u, 
	61u, 62u, 10u, 61u, 62u, 95u, 65u, 90u, 
	97u, 122u, 58u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 58u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 58u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 47u, 58u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 99u, 101u, 103u, 105u, 109u, 
	112u, 115u, 120u, 58u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 33u, 40u, 47u, 58u, 91u, 
	95u, 113u, 119u, 48u, 57u, 65u, 90u, 97u, 
	122u, 123u, 124u, 33u, 40u, 47u, 58u, 91u, 
	95u, 48u, 57u, 65u, 90u, 97u, 122u, 123u, 
	124u, 33u, 40u, 47u, 58u, 91u, 95u, 48u, 
	57u, 65u, 90u, 97u, 122u, 123u, 124u, 47u, 
	58u, 95u, 48u, 57u, 65u, 90u, 97u, 122u, 
	99u, 101u, 103u, 105u, 109u, 112u, 115u, 120u, 
	61u, 124u, 0
};

static const char _PRLT_single_lengths[] = {
	2, 0, 2, 0, 2, 0, 0, 2, 
	0, 0, 2, 0, 0, 0, 0, 0, 
	2, 1, 1, 1, 1, 1, 1, 1, 
	1, 2, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 0, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 2, 0, 
	4, 5, 0, 4, 2, 2, 0, 0, 
	28, 1, 2, 2, 1, 5, 1, 2, 
	2, 1, 2, 2, 2, 1, 2, 1, 
	3, 1, 2, 8, 2, 3, 1, 4, 
	4, 1, 1, 1, 2, 1, 9, 1, 
	2, 1, 2, 2, 2, 3, 8, 2, 
	8, 6, 6, 3, 8, 2
};

static const char _PRLT_range_lengths[] = {
	0, 0, 0, 0, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 2, 1, 3, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 1, 0, 0, 0, 
	0, 0, 0, 0, 0, 2, 0, 0, 
	1, 1, 0, 1, 0, 0, 0, 0, 
	4, 1, 0, 0, 0, 5, 3, 2, 
	0, 0, 0, 0, 0, 1, 1, 0, 
	1, 1, 0, 2, 1, 1, 1, 2, 
	1, 1, 1, 3, 0, 0, 1, 0, 
	0, 2, 3, 3, 3, 3, 0, 3, 
	4, 4, 4, 3, 0, 0
};

static const short _PRLT_index_offsets[] = {
	0, 3, 4, 7, 8, 12, 14, 16, 
	20, 22, 24, 28, 30, 32, 35, 37, 
	41, 44, 46, 48, 50, 52, 54, 56, 
	58, 60, 63, 65, 67, 69, 71, 73, 
	75, 77, 79, 81, 83, 85, 87, 89, 
	91, 93, 95, 97, 99, 101, 105, 108, 
	109, 115, 122, 123, 129, 132, 135, 136, 
	137, 170, 173, 176, 179, 181, 192, 197, 
	202, 205, 207, 210, 213, 216, 219, 223, 
	225, 230, 233, 236, 247, 251, 256, 259, 
	266, 272, 275, 278, 283, 286, 288, 299, 
	301, 304, 308, 314, 320, 326, 333, 342, 
	348, 361, 372, 383, 390, 399
};

static const unsigned char _PRLT_indicies[] = {
	2, 3, 1, 1, 5, 6, 4, 4, 
	8, 8, 9, 7, 9, 7, 10, 7, 
	12, 12, 13, 11, 13, 11, 14, 11, 
	16, 16, 17, 15, 17, 15, 19, 18, 
	21, 19, 20, 22, 15, 23, 23, 23, 
	15, 24, 25, 0, 26, 0, 27, 0, 
	28, 0, 29, 0, 27, 0, 30, 0, 
	27, 0, 31, 0, 32, 27, 0, 33, 
	0, 34, 0, 35, 0, 36, 0, 27, 
	0, 37, 0, 27, 0, 38, 0, 39, 
	0, 40, 0, 27, 0, 41, 0, 42, 
	0, 27, 0, 43, 0, 37, 0, 44, 
	0, 27, 0, 46, 45, 47, 47, 47, 
	45, 49, 50, 48, 48, 52, 52, 52, 
	52, 52, 51, 54, 54, 54, 55, 54, 
	54, 53, 53, 57, 57, 57, 57, 57, 
	56, 59, 60, 58, 61, 62, 59, 59, 
	58, 64, 65, 66, 67, 68, 69, 70, 
	71, 72, 73, 74, 75, 76, 77, 78, 
	79, 80, 81, 82, 47, 83, 84, 84, 
	85, 86, 87, 88, 89, 64, 19, 47, 
	47, 63, 64, 64, 90, 92, 92, 91, 
	2, 3, 1, 93, 67, 94, 94, 94, 
	94, 95, 94, 94, 94, 95, 95, 91, 
	95, 95, 95, 95, 96, 92, 95, 95, 
	95, 91, 97, 92, 91, 92, 15, 5, 
	6, 4, 97, 92, 91, 92, 92, 91, 
	92, 92, 91, 99, 92, 10, 98, 92, 
	100, 102, 103, 102, 10, 101, 8, 9, 
	101, 97, 92, 91, 105, 106, 107, 108, 
	109, 106, 107, 108, 21, 19, 104, 111, 
	111, 14, 110, 111, 112, 111, 14, 110, 
	12, 13, 110, 105, 107, 114, 107, 21, 
	19, 113, 105, 107, 109, 107, 19, 104, 
	16, 17, 115, 106, 22, 116, 108, 23, 
	23, 23, 117, 97, 118, 91, 92, 100, 
	119, 120, 121, 122, 123, 124, 125, 126, 
	92, 92, 91, 127, 27, 92, 97, 91, 
	95, 95, 95, 91, 129, 47, 47, 47, 
	47, 128, 129, 47, 47, 47, 47, 128, 
	129, 47, 47, 47, 47, 128, 48, 129, 
	47, 47, 47, 47, 128, 49, 49, 49, 
	49, 49, 49, 49, 49, 130, 129, 47, 
	47, 47, 47, 128, 51, 51, 51, 129, 
	51, 47, 131, 132, 47, 47, 47, 51, 
	128, 53, 53, 53, 129, 53, 47, 47, 
	47, 47, 53, 128, 56, 56, 56, 129, 
	56, 47, 47, 47, 47, 56, 128, 58, 
	129, 47, 47, 47, 47, 128, 61, 61, 
	61, 61, 61, 61, 61, 61, 133, 92, 
	97, 91, 0
};

static const char _PRLT_trans_targs[] = {
	56, 0, 56, 1, 2, 56, 3, 56, 
	5, 73, 72, 56, 8, 78, 77, 56, 
	11, 81, 56, 80, 56, 79, 82, 83, 
	17, 19, 18, 87, 20, 21, 23, 25, 
	26, 27, 28, 29, 30, 32, 34, 35, 
	36, 38, 39, 41, 43, 56, 45, 90, 
	46, 94, 47, 48, 56, 49, 56, 50, 
	51, 56, 52, 53, 55, 100, 54, 56, 
	57, 58, 59, 60, 61, 63, 64, 66, 
	67, 68, 69, 70, 74, 75, 84, 86, 
	88, 89, 65, 91, 92, 93, 95, 96, 
	99, 101, 56, 56, 56, 56, 56, 62, 
	56, 65, 56, 71, 56, 56, 4, 6, 
	56, 76, 14, 10, 15, 12, 56, 7, 
	9, 56, 13, 56, 56, 56, 85, 16, 
	22, 24, 31, 33, 37, 40, 42, 56, 
	56, 44, 56, 97, 98, 56
};

static const char _PRLT_trans_actions[] = {
	67, 0, 13, 0, 0, 15, 0, 61, 
	0, 5, 5, 59, 0, 5, 5, 69, 
	0, 80, 63, 83, 57, 77, 74, 71, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 65, 0, 5, 
	0, 0, 0, 0, 11, 0, 9, 0, 
	0, 7, 0, 0, 0, 0, 0, 21, 
	0, 0, 5, 0, 0, 0, 0, 5, 
	0, 0, 0, 0, 0, 83, 0, 5, 
	0, 0, 89, 5, 5, 5, 5, 5, 
	5, 0, 55, 51, 19, 23, 17, 0, 
	45, 86, 53, 0, 49, 39, 0, 0, 
	43, 5, 0, 0, 0, 0, 37, 0, 
	0, 35, 0, 41, 33, 31, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 25, 
	47, 0, 27, 5, 5, 29
};

static const char _PRLT_to_state_actions[] = {
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
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0
};

static const char _PRLT_from_state_actions[] = {
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
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0
};

static const short _PRLT_eof_trans[] = {
	1, 1, 1, 1, 8, 8, 8, 12, 
	12, 12, 16, 16, 19, 21, 16, 16, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 46, 46, 46, 46, 
	46, 46, 46, 46, 46, 46, 46, 46, 
	0, 91, 92, 92, 94, 92, 97, 92, 
	92, 16, 92, 92, 92, 92, 99, 101, 
	102, 102, 92, 105, 111, 111, 111, 114, 
	105, 116, 117, 118, 92, 101, 92, 128, 
	92, 92, 129, 129, 129, 129, 131, 129, 
	129, 129, 129, 129, 134, 92
};

static const int PRLT_start = 56;
static const int PRLT_first_final = 56;
static const int PRLT_error = -1;

static const int PRLT_en_main = 56;


/* #line 132 "PRLT.c.rl" */

ok64 PRLTLexer(PRLTstate* state) {

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

    
/* #line 311 "PRLT.rl.c" */
	{
	cs = PRLT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 150 "PRLT.c.rl" */
    
/* #line 317 "PRLT.rl.c" */
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( p == pe )
		goto _test_eof;
_resume:
	_acts = _PRLT_actions + _PRLT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 334 "PRLT.rl.c" */
		}
	}

	_keys = _PRLT_trans_keys + _PRLT_key_offsets[cs];
	_trans = _PRLT_index_offsets[cs];

	_klen = _PRLT_single_lengths[cs];
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

	_klen = _PRLT_range_lengths[cs];
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
	_trans = _PRLT_indicies[_trans];
_eof_trans:
	cs = _PRLT_trans_targs[_trans];

	if ( _PRLT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _PRLT_actions + _PRLT_trans_actions[_trans];
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
/* #line 42 "PRLT.c.rl" */
	{act = 10;}
	break;
	case 4:
/* #line 42 "PRLT.c.rl" */
	{act = 11;}
	break;
	case 5:
/* #line 42 "PRLT.c.rl" */
	{act = 12;}
	break;
	case 6:
/* #line 42 "PRLT.c.rl" */
	{act = 15;}
	break;
	case 7:
/* #line 42 "PRLT.c.rl" */
	{act = 16;}
	break;
	case 8:
/* #line 54 "PRLT.c.rl" */
	{act = 20;}
	break;
	case 9:
/* #line 54 "PRLT.c.rl" */
	{act = 21;}
	break;
	case 10:
/* #line 36 "PRLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 36 "PRLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 36 "PRLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 36 "PRLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 36 "PRLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 48 "PRLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 54 "PRLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 54 "PRLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 30 "PRLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 30 "PRLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 36 "PRLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 36 "PRLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 42 "PRLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 42 "PRLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 42 "PRLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 42 "PRLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 42 "PRLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 42 "PRLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 42 "PRLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 48 "PRLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 48 "PRLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 54 "PRLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 54 "PRLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 54 "PRLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 60 "PRLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 42 "PRLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 42 "PRLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 42 "PRLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 42 "PRLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 39:
/* #line 48 "PRLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 40:
/* #line 54 "PRLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 41:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 10:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 11:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 15:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 16:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 20:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 21:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 732 "PRLT.rl.c" */
		}
	}

_again:
	_acts = _PRLT_actions + _PRLT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 743 "PRLT.rl.c" */
		}
	}

	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _PRLT_eof_trans[cs] > 0 ) {
		_trans = _PRLT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 151 "PRLT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < PRLT_first_final)
        o = PRLTBAD;

    return o;
}
