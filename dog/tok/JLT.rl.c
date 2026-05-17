
/* #line 1 "JLT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "JLT.h"

ok64 JLTonComment (u8cs tok, JLTstate* state);
ok64 JLTonString (u8cs tok, JLTstate* state);
ok64 JLTonNumber (u8cs tok, JLTstate* state);
ok64 JLTonWord (u8cs tok, JLTstate* state);
ok64 JLTonPunct (u8cs tok, JLTstate* state);
ok64 JLTonSpace (u8cs tok, JLTstate* state);


/* #line 125 "JLT.c.rl" */



/* #line 15 "JLT.rl.c" */
static const char _JLT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	12, 1, 13, 1, 14, 1, 15, 1, 
	16, 1, 17, 1, 18, 1, 19, 1, 
	20, 1, 21, 1, 22, 1, 23, 1, 
	24, 1, 25, 1, 26, 1, 27, 1, 
	28, 1, 29, 1, 30, 1, 31, 1, 
	32, 1, 33, 1, 34, 1, 35, 1, 
	36, 1, 37, 1, 38, 1, 39, 1, 
	40, 1, 41, 1, 42, 1, 43, 1, 
	44, 1, 45, 1, 46, 2, 2, 3, 
	2, 2, 4, 2, 2, 5, 2, 2, 
	6, 2, 2, 7, 2, 2, 8, 2, 
	2, 9, 2, 2, 10, 2, 2, 11
	
};

static const short _JLT_key_offsets[] = {
	0, 0, 2, 4, 21, 27, 33, 39, 
	45, 51, 57, 63, 69, 70, 71, 72, 
	73, 74, 76, 77, 94, 100, 106, 112, 
	118, 124, 130, 136, 142, 143, 147, 149, 
	150, 152, 156, 158, 159, 161, 165, 167, 
	168, 170, 171, 173, 175, 181, 185, 187, 
	193, 194, 223, 226, 227, 228, 229, 231, 
	232, 234, 236, 238, 251, 252, 258, 262, 
	264, 266, 279, 284, 290, 294, 301, 305, 
	308, 311, 321, 329, 338, 341, 342, 345, 
	347, 350, 358, 367, 376, 385
};

static const unsigned char _JLT_trans_keys[] = {
	34u, 92u, 34u, 92u, 10u, 34u, 36u, 39u, 
	48u, 85u, 92u, 110u, 114u, 117u, 120u, 97u, 
	98u, 101u, 102u, 116u, 118u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 34u, 34u, 34u, 
	61u, 35u, 39u, 92u, 39u, 10u, 34u, 36u, 
	39u, 48u, 85u, 92u, 110u, 114u, 117u, 120u, 
	97u, 98u, 101u, 102u, 116u, 118u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 61u, 43u, 
	45u, 48u, 57u, 48u, 57u, 109u, 48u, 57u, 
	43u, 45u, 48u, 57u, 48u, 57u, 109u, 48u, 
	57u, 43u, 45u, 48u, 57u, 48u, 57u, 109u, 
	48u, 57u, 109u, 48u, 49u, 48u, 55u, 48u, 
	57u, 65u, 70u, 97u, 102u, 43u, 45u, 48u, 
	57u, 48u, 57u, 48u, 57u, 65u, 70u, 97u, 
	102u, 34u, 32u, 33u, 34u, 35u, 37u, 38u, 
	39u, 46u, 48u, 58u, 60u, 61u, 62u, 94u, 
	95u, 114u, 124u, 9u, 13u, 42u, 43u, 45u, 
	47u, 49u, 57u, 65u, 90u, 97u, 122u, 32u, 
	9u, 13u, 61u, 61u, 34u, 10u, 61u, 10u, 
	10u, 61u, 10u, 35u, 38u, 61u, 33u, 37u, 
	46u, 60u, 61u, 62u, 94u, 42u, 43u, 45u, 
	47u, 48u, 57u, 46u, 69u, 95u, 101u, 105u, 
	48u, 57u, 95u, 105u, 48u, 57u, 60u, 61u, 
	61u, 62u, 46u, 66u, 69u, 79u, 88u, 95u, 
	98u, 101u, 105u, 111u, 120u, 48u, 57u, 69u, 
	101u, 105u, 48u, 57u, 69u, 95u, 101u, 105u, 
	48u, 57u, 95u, 105u, 48u, 57u, 46u, 69u, 
	95u, 101u, 105u, 48u, 57u, 95u, 105u, 48u, 
	57u, 95u, 48u, 49u, 95u, 48u, 55u, 46u, 
	80u, 95u, 112u, 48u, 57u, 65u, 70u, 97u, 
	102u, 80u, 112u, 48u, 57u, 65u, 70u, 97u, 
	102u, 80u, 95u, 112u, 48u, 57u, 65u, 70u, 
	97u, 102u, 95u, 48u, 57u, 58u, 58u, 60u, 
	61u, 61u, 62u, 58u, 61u, 62u, 33u, 95u, 
	48u, 57u, 65u, 90u, 97u, 122u, 33u, 95u, 
	97u, 48u, 57u, 65u, 90u, 98u, 122u, 33u, 
	95u, 119u, 48u, 57u, 65u, 90u, 97u, 122u, 
	33u, 34u, 95u, 48u, 57u, 65u, 90u, 97u, 
	122u, 61u, 124u, 0
};

static const char _JLT_single_lengths[] = {
	0, 2, 2, 11, 0, 0, 0, 0, 
	0, 0, 0, 0, 1, 1, 1, 1, 
	1, 2, 1, 11, 0, 0, 0, 0, 
	0, 0, 0, 0, 1, 2, 0, 1, 
	0, 2, 0, 1, 0, 2, 0, 1, 
	0, 1, 0, 0, 0, 2, 0, 0, 
	1, 17, 1, 1, 1, 1, 2, 1, 
	2, 2, 2, 7, 1, 4, 2, 0, 
	0, 11, 3, 4, 2, 5, 2, 1, 
	1, 4, 2, 3, 1, 1, 3, 2, 
	3, 2, 3, 3, 3, 2
};

static const char _JLT_range_lengths[] = {
	0, 0, 0, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 0, 0, 0, 0, 
	0, 0, 0, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 0, 1, 1, 0, 
	1, 1, 1, 0, 1, 1, 1, 0, 
	1, 0, 1, 1, 3, 1, 1, 3, 
	0, 6, 1, 0, 0, 0, 0, 0, 
	0, 0, 0, 3, 0, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 3, 3, 3, 1, 0, 0, 0, 
	0, 3, 3, 3, 3, 0
};

static const short _JLT_index_offsets[] = {
	0, 0, 3, 6, 21, 25, 29, 33, 
	37, 41, 45, 49, 53, 55, 57, 59, 
	61, 63, 66, 68, 83, 87, 91, 95, 
	99, 103, 107, 111, 115, 117, 121, 123, 
	125, 127, 131, 133, 135, 137, 141, 143, 
	145, 147, 149, 151, 153, 157, 161, 163, 
	167, 169, 193, 196, 198, 200, 202, 205, 
	207, 210, 213, 216, 227, 229, 235, 239, 
	241, 243, 256, 261, 267, 271, 278, 282, 
	285, 288, 296, 302, 309, 312, 314, 318, 
	321, 325, 331, 338, 345, 352
};

static const char _JLT_indicies[] = {
	1, 2, 0, 3, 2, 0, 0, 0, 
	0, 0, 0, 5, 0, 0, 0, 6, 
	7, 0, 0, 0, 4, 8, 8, 8, 
	4, 9, 9, 9, 4, 10, 10, 10, 
	4, 6, 6, 6, 4, 11, 11, 11, 
	4, 7, 7, 7, 4, 12, 12, 12, 
	4, 0, 0, 0, 4, 15, 14, 16, 
	14, 17, 14, 20, 19, 21, 19, 4, 
	23, 22, 24, 4, 22, 22, 22, 22, 
	22, 25, 22, 22, 22, 26, 27, 22, 
	22, 22, 4, 28, 28, 28, 4, 29, 
	29, 29, 4, 30, 30, 30, 4, 26, 
	26, 26, 4, 31, 31, 31, 4, 27, 
	27, 27, 4, 32, 32, 32, 4, 22, 
	22, 22, 4, 34, 33, 36, 36, 37, 
	35, 37, 35, 38, 35, 39, 35, 41, 
	41, 42, 40, 42, 40, 43, 40, 44, 
	40, 46, 46, 47, 45, 47, 48, 50, 
	49, 51, 45, 52, 45, 53, 48, 54, 
	48, 55, 55, 55, 48, 57, 57, 58, 
	56, 58, 56, 59, 59, 59, 56, 62, 
	61, 64, 65, 66, 67, 68, 69, 70, 
	71, 72, 73, 74, 75, 76, 68, 77, 
	78, 79, 64, 68, 68, 51, 77, 77, 
	63, 64, 64, 80, 82, 81, 34, 48, 
	14, 83, 84, 86, 85, 48, 85, 19, 
	87, 86, 19, 88, 86, 34, 34, 81, 
	90, 34, 91, 92, 90, 93, 34, 34, 
	34, 39, 89, 34, 94, 96, 97, 96, 
	98, 39, 95, 36, 98, 37, 95, 34, 
	94, 34, 94, 100, 101, 102, 103, 104, 
	105, 101, 102, 106, 103, 104, 51, 99, 
	108, 108, 109, 44, 107, 108, 110, 108, 
	109, 44, 107, 41, 109, 42, 107, 100, 
	102, 105, 102, 106, 51, 99, 46, 112, 
	47, 111, 101, 53, 113, 103, 54, 114, 
	116, 117, 104, 117, 55, 55, 55, 115, 
	117, 117, 59, 59, 59, 115, 117, 118, 
	117, 59, 59, 59, 115, 57, 58, 115, 
	34, 81, 34, 82, 34, 81, 82, 34, 
	81, 34, 34, 82, 81, 77, 77, 77, 
	77, 77, 119, 77, 77, 120, 77, 77, 
	77, 119, 77, 77, 121, 77, 77, 77, 
	119, 77, 61, 77, 77, 77, 77, 119, 
	34, 34, 81, 0
};

static const char _JLT_trans_targs[] = {
	2, 53, 3, 49, 0, 4, 8, 10, 
	5, 6, 7, 9, 11, 49, 12, 13, 
	14, 49, 49, 15, 16, 49, 18, 19, 
	49, 20, 24, 26, 21, 22, 23, 25, 
	27, 49, 49, 49, 30, 62, 49, 61, 
	49, 34, 68, 49, 67, 49, 38, 70, 
	49, 49, 49, 69, 49, 71, 72, 73, 
	49, 46, 76, 75, 49, 48, 49, 49, 
	50, 51, 1, 54, 52, 58, 17, 59, 
	65, 77, 78, 79, 80, 81, 82, 85, 
	49, 49, 52, 49, 49, 55, 56, 57, 
	55, 49, 28, 60, 63, 64, 49, 49, 
	29, 32, 31, 49, 66, 42, 37, 43, 
	44, 40, 41, 49, 33, 35, 36, 49, 
	39, 49, 49, 49, 74, 45, 47, 49, 
	83, 84
};

static const char _JLT_trans_actions[] = {
	0, 5, 0, 11, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 59, 0, 0, 
	0, 9, 57, 0, 0, 7, 0, 0, 
	13, 0, 0, 0, 0, 0, 0, 0, 
	0, 73, 25, 65, 0, 5, 19, 5, 
	63, 0, 5, 17, 5, 69, 0, 92, 
	75, 67, 21, 95, 23, 89, 86, 83, 
	61, 0, 5, 5, 71, 0, 15, 27, 
	0, 0, 0, 0, 101, 0, 0, 5, 
	95, 0, 0, 0, 0, 0, 0, 0, 
	55, 51, 98, 31, 29, 80, 5, 5, 
	77, 53, 0, 0, 0, 0, 49, 41, 
	0, 0, 0, 45, 5, 0, 0, 0, 
	0, 0, 0, 39, 0, 0, 0, 43, 
	0, 37, 35, 33, 5, 0, 0, 47, 
	0, 5
};

static const char _JLT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0
};

static const char _JLT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 3, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0
};

static const short _JLT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 14, 14, 14, 19, 
	19, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 34, 36, 36, 36, 
	36, 41, 41, 41, 41, 46, 49, 50, 
	46, 46, 49, 49, 49, 57, 57, 57, 
	61, 0, 81, 82, 49, 84, 85, 49, 
	85, 85, 82, 90, 95, 96, 96, 95, 
	95, 100, 108, 108, 108, 100, 112, 114, 
	115, 116, 116, 116, 116, 82, 82, 82, 
	82, 120, 120, 120, 120, 82
};

static const int JLT_start = 49;
static const int JLT_first_final = 49;
static const int JLT_error = 0;

static const int JLT_en_main = 49;


/* #line 128 "JLT.c.rl" */

ok64 JLTLexer(JLTstate* state) {

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

    
/* #line 291 "JLT.rl.c" */
	{
	cs = JLT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 146 "JLT.c.rl" */
    
/* #line 297 "JLT.rl.c" */
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
	_acts = _JLT_actions + _JLT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 316 "JLT.rl.c" */
		}
	}

	_keys = _JLT_trans_keys + _JLT_key_offsets[cs];
	_trans = _JLT_index_offsets[cs];

	_klen = _JLT_single_lengths[cs];
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

	_klen = _JLT_range_lengths[cs];
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
	_trans = _JLT_indicies[_trans];
_eof_trans:
	cs = _JLT_trans_targs[_trans];

	if ( _JLT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _JLT_actions + _JLT_trans_actions[_trans];
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
/* #line 32 "JLT.c.rl" */
	{act = 1;}
	break;
	case 4:
/* #line 32 "JLT.c.rl" */
	{act = 2;}
	break;
	case 5:
/* #line 44 "JLT.c.rl" */
	{act = 7;}
	break;
	case 6:
/* #line 44 "JLT.c.rl" */
	{act = 8;}
	break;
	case 7:
/* #line 44 "JLT.c.rl" */
	{act = 9;}
	break;
	case 8:
/* #line 44 "JLT.c.rl" */
	{act = 12;}
	break;
	case 9:
/* #line 44 "JLT.c.rl" */
	{act = 13;}
	break;
	case 10:
/* #line 56 "JLT.c.rl" */
	{act = 15;}
	break;
	case 11:
/* #line 56 "JLT.c.rl" */
	{act = 16;}
	break;
	case 12:
/* #line 32 "JLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 38 "JLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 38 "JLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 38 "JLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 38 "JLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 44 "JLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 44 "JLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 44 "JLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 44 "JLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 56 "JLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 56 "JLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 32 "JLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 38 "JLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 44 "JLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 44 "JLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 44 "JLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 44 "JLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 44 "JLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 44 "JLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 44 "JLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 50 "JLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 56 "JLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 56 "JLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 56 "JLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 62 "JLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 32 "JLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 38 "JLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 39:
/* #line 44 "JLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 40:
/* #line 44 "JLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 41:
/* #line 44 "JLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 42:
/* #line 44 "JLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 43:
/* #line 44 "JLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 44:
/* #line 50 "JLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 45:
/* #line 56 "JLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 46:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 1:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 2:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 7:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 8:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 9:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 13:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 15:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 16:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 760 "JLT.rl.c" */
		}
	}

_again:
	_acts = _JLT_actions + _JLT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 771 "JLT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _JLT_eof_trans[cs] > 0 ) {
		_trans = _JLT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 147 "JLT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < JLT_first_final)
        o = JLTBAD;

    return o;
}
