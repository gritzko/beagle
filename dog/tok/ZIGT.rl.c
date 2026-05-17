
/* #line 1 "ZIGT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "ZIGT.h"

ok64 ZIGTonComment (u8cs tok, ZIGTstate* state);
ok64 ZIGTonString (u8cs tok, ZIGTstate* state);
ok64 ZIGTonNumber (u8cs tok, ZIGTstate* state);
ok64 ZIGTonWord (u8cs tok, ZIGTstate* state);
ok64 ZIGTonPunct (u8cs tok, ZIGTstate* state);
ok64 ZIGTonSpace (u8cs tok, ZIGTstate* state);


/* #line 120 "ZIGT.c.rl" */



/* #line 15 "ZIGT.rl.c" */
static const char _ZIGT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	10, 1, 11, 1, 12, 1, 13, 1, 
	14, 1, 15, 1, 16, 1, 17, 1, 
	18, 1, 19, 1, 20, 1, 21, 1, 
	22, 1, 23, 1, 24, 1, 25, 1, 
	26, 1, 27, 1, 28, 1, 29, 1, 
	30, 1, 31, 1, 32, 1, 33, 1, 
	34, 2, 2, 3, 2, 2, 4, 2, 
	2, 5, 2, 2, 6, 2, 2, 7, 
	2, 2, 8, 2, 2, 9
};

static const short _ZIGT_key_offsets[] = {
	0, 0, 2, 17, 18, 24, 31, 38, 
	45, 52, 59, 60, 66, 72, 74, 75, 
	90, 91, 97, 104, 111, 118, 125, 132, 
	133, 139, 145, 149, 151, 153, 157, 159, 
	161, 165, 167, 169, 171, 173, 179, 185, 
	189, 191, 196, 197, 198, 226, 229, 230, 
	232, 234, 239, 240, 245, 248, 250, 251, 
	263, 267, 272, 275, 281, 284, 287, 290, 
	300, 309, 312, 314, 316, 318, 325, 332, 
	333, 334
};

static const unsigned char _ZIGT_trans_keys[] = {
	34u, 92u, 34u, 39u, 48u, 63u, 92u, 110u, 
	114u, 117u, 120u, 97u, 98u, 101u, 102u, 116u, 
	118u, 123u, 48u, 57u, 65u, 70u, 97u, 102u, 
	125u, 48u, 57u, 65u, 70u, 97u, 102u, 125u, 
	48u, 57u, 65u, 70u, 97u, 102u, 125u, 48u, 
	57u, 65u, 70u, 97u, 102u, 125u, 48u, 57u, 
	65u, 70u, 97u, 102u, 125u, 48u, 57u, 65u, 
	70u, 97u, 102u, 125u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	39u, 92u, 39u, 34u, 39u, 48u, 63u, 92u, 
	110u, 114u, 117u, 120u, 97u, 98u, 101u, 102u, 
	116u, 118u, 123u, 48u, 57u, 65u, 70u, 97u, 
	102u, 125u, 48u, 57u, 65u, 70u, 97u, 102u, 
	125u, 48u, 57u, 65u, 70u, 97u, 102u, 125u, 
	48u, 57u, 65u, 70u, 97u, 102u, 125u, 48u, 
	57u, 65u, 70u, 97u, 102u, 125u, 48u, 57u, 
	65u, 70u, 97u, 102u, 125u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 43u, 45u, 48u, 57u, 48u, 57u, 48u, 
	57u, 43u, 45u, 48u, 57u, 48u, 57u, 48u, 
	57u, 43u, 45u, 48u, 57u, 48u, 57u, 48u, 
	57u, 48u, 49u, 48u, 55u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 43u, 45u, 48u, 57u, 48u, 57u, 95u, 
	65u, 90u, 97u, 122u, 92u, 92u, 32u, 33u, 
	34u, 37u, 38u, 39u, 42u, 43u, 45u, 46u, 
	47u, 48u, 60u, 61u, 62u, 64u, 92u, 94u, 
	95u, 124u, 9u, 13u, 49u, 57u, 65u, 90u, 
	97u, 122u, 32u, 9u, 13u, 61u, 38u, 61u, 
	43u, 61u, 42u, 46u, 63u, 48u, 57u, 46u, 
	69u, 95u, 101u, 48u, 57u, 95u, 48u, 57u, 
	47u, 61u, 10u, 46u, 66u, 69u, 79u, 88u, 
	95u, 98u, 101u, 111u, 120u, 48u, 57u, 69u, 
	101u, 48u, 57u, 69u, 95u, 101u, 48u, 57u, 
	95u, 48u, 57u, 46u, 69u, 95u, 101u, 48u, 
	57u, 95u, 48u, 57u, 95u, 48u, 49u, 95u, 
	48u, 55u, 46u, 80u, 95u, 112u, 48u, 57u, 
	65u, 70u, 97u, 102u, 80u, 95u, 112u, 48u, 
	57u, 65u, 70u, 97u, 102u, 95u, 48u, 57u, 
	60u, 61u, 61u, 62u, 61u, 62u, 95u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 48u, 57u, 
	65u, 90u, 97u, 122u, 92u, 10u, 61u, 124u, 
	0
};

static const char _ZIGT_single_lengths[] = {
	0, 2, 9, 1, 0, 1, 1, 1, 
	1, 1, 1, 0, 0, 2, 1, 9, 
	1, 0, 1, 1, 1, 1, 1, 1, 
	0, 0, 2, 0, 0, 2, 0, 0, 
	2, 0, 0, 0, 0, 0, 0, 2, 
	0, 1, 1, 1, 20, 1, 1, 2, 
	2, 3, 1, 3, 1, 2, 1, 10, 
	2, 3, 1, 4, 1, 1, 1, 4, 
	3, 1, 2, 0, 2, 1, 1, 1, 
	1, 2
};

static const char _ZIGT_range_lengths[] = {
	0, 0, 3, 0, 3, 3, 3, 3, 
	3, 3, 0, 3, 3, 0, 0, 3, 
	0, 3, 3, 3, 3, 3, 3, 0, 
	3, 3, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 3, 3, 1, 
	1, 2, 0, 0, 4, 1, 0, 0, 
	0, 1, 0, 1, 1, 0, 0, 1, 
	1, 1, 1, 1, 1, 1, 1, 3, 
	3, 1, 0, 1, 0, 3, 3, 0, 
	0, 0
};

static const short _ZIGT_index_offsets[] = {
	0, 0, 3, 16, 18, 22, 27, 32, 
	37, 42, 47, 49, 53, 57, 60, 62, 
	75, 77, 81, 86, 91, 96, 101, 106, 
	108, 112, 116, 120, 122, 124, 128, 130, 
	132, 136, 138, 140, 142, 144, 148, 152, 
	156, 158, 162, 164, 166, 191, 194, 196, 
	199, 202, 207, 209, 214, 217, 220, 222, 
	234, 238, 243, 246, 252, 255, 258, 261, 
	269, 276, 279, 282, 284, 287, 292, 297, 
	299, 301
};

static const char _ZIGT_indicies[] = {
	1, 2, 0, 0, 0, 0, 0, 0, 
	0, 0, 4, 5, 0, 0, 0, 3, 
	6, 3, 7, 7, 7, 3, 0, 8, 
	8, 8, 3, 0, 9, 9, 9, 3, 
	0, 10, 10, 10, 3, 0, 11, 11, 
	11, 3, 0, 12, 12, 12, 3, 0, 
	3, 13, 13, 13, 3, 0, 0, 0, 
	3, 3, 15, 14, 16, 3, 14, 14, 
	14, 14, 14, 14, 14, 17, 18, 14, 
	14, 14, 3, 19, 3, 20, 20, 20, 
	3, 14, 21, 21, 21, 3, 14, 22, 
	22, 22, 3, 14, 23, 23, 23, 3, 
	14, 24, 24, 24, 3, 14, 25, 25, 
	25, 3, 14, 3, 26, 26, 26, 3, 
	14, 14, 14, 3, 28, 28, 29, 27, 
	29, 27, 30, 27, 32, 32, 33, 31, 
	33, 31, 34, 31, 36, 36, 37, 35, 
	37, 38, 39, 35, 40, 38, 41, 38, 
	42, 42, 42, 38, 44, 44, 44, 43, 
	45, 45, 46, 43, 46, 43, 47, 47, 
	47, 3, 49, 48, 50, 48, 52, 53, 
	0, 53, 54, 55, 53, 56, 53, 57, 
	58, 59, 60, 61, 62, 63, 65, 53, 
	64, 66, 52, 39, 64, 64, 51, 52, 
	52, 67, 68, 38, 68, 68, 69, 68, 
	68, 69, 68, 71, 68, 30, 70, 68, 
	72, 74, 75, 74, 30, 73, 28, 29, 
	73, 76, 68, 69, 77, 76, 79, 80, 
	81, 82, 83, 84, 80, 81, 82, 83, 
	39, 78, 86, 86, 34, 85, 86, 87, 
	86, 34, 85, 32, 33, 85, 79, 81, 
	84, 81, 39, 78, 36, 37, 88, 80, 
	40, 89, 82, 41, 90, 92, 93, 83, 
	93, 42, 42, 42, 91, 93, 92, 93, 
	44, 44, 44, 91, 45, 46, 91, 94, 
	68, 69, 68, 69, 68, 94, 69, 47, 
	47, 47, 47, 95, 64, 64, 64, 64, 
	96, 50, 69, 98, 50, 68, 68, 69, 
	0
};

static const char _ZIGT_trans_targs[] = {
	1, 44, 2, 0, 3, 11, 4, 5, 
	6, 7, 8, 9, 10, 12, 14, 15, 
	44, 16, 24, 17, 18, 19, 20, 21, 
	22, 23, 25, 44, 27, 52, 51, 44, 
	30, 58, 57, 44, 33, 60, 44, 59, 
	61, 62, 63, 44, 64, 40, 65, 69, 
	44, 43, 72, 44, 45, 46, 47, 13, 
	48, 49, 53, 55, 66, 67, 68, 41, 
	70, 71, 73, 44, 44, 44, 44, 50, 
	44, 44, 26, 28, 54, 44, 44, 56, 
	35, 32, 36, 37, 34, 44, 29, 31, 
	44, 44, 44, 44, 38, 39, 46, 44, 
	44, 44, 42
};

static const char _ZIGT_trans_actions[] = {
	0, 7, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	9, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 51, 0, 5, 5, 49, 
	0, 5, 5, 53, 0, 66, 55, 69, 
	63, 60, 57, 47, 5, 0, 5, 0, 
	45, 0, 5, 13, 0, 75, 0, 0, 
	0, 0, 0, 69, 0, 0, 0, 0, 
	0, 0, 0, 43, 11, 39, 41, 0, 
	37, 27, 0, 0, 0, 15, 31, 5, 
	0, 0, 0, 0, 0, 25, 0, 0, 
	29, 23, 21, 19, 0, 0, 72, 33, 
	35, 17, 0
};

static const char _ZIGT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 1, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const char _ZIGT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 3, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const short _ZIGT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 28, 28, 28, 32, 32, 32, 
	36, 39, 36, 39, 39, 39, 44, 44, 
	44, 0, 49, 49, 0, 68, 39, 70, 
	70, 71, 73, 74, 74, 70, 78, 79, 
	86, 86, 86, 79, 89, 90, 91, 92, 
	92, 92, 70, 70, 70, 96, 97, 70, 
	98, 70
};

static const int ZIGT_start = 44;
static const int ZIGT_first_final = 44;
static const int ZIGT_error = 0;

static const int ZIGT_en_main = 44;


/* #line 123 "ZIGT.c.rl" */

ok64 ZIGTLexer(ZIGTstate* state) {

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

    
/* #line 262 "ZIGT.rl.c" */
	{
	cs = ZIGT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 141 "ZIGT.c.rl" */
    
/* #line 268 "ZIGT.rl.c" */
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
	_acts = _ZIGT_actions + _ZIGT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 287 "ZIGT.rl.c" */
		}
	}

	_keys = _ZIGT_trans_keys + _ZIGT_key_offsets[cs];
	_trans = _ZIGT_index_offsets[cs];

	_klen = _ZIGT_single_lengths[cs];
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

	_klen = _ZIGT_range_lengths[cs];
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
	_trans = _ZIGT_indicies[_trans];
_eof_trans:
	cs = _ZIGT_trans_targs[_trans];

	if ( _ZIGT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _ZIGT_actions + _ZIGT_trans_actions[_trans];
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
/* #line 42 "ZIGT.c.rl" */
	{act = 5;}
	break;
	case 4:
/* #line 42 "ZIGT.c.rl" */
	{act = 6;}
	break;
	case 5:
/* #line 42 "ZIGT.c.rl" */
	{act = 7;}
	break;
	case 6:
/* #line 42 "ZIGT.c.rl" */
	{act = 10;}
	break;
	case 7:
/* #line 42 "ZIGT.c.rl" */
	{act = 11;}
	break;
	case 8:
/* #line 54 "ZIGT.c.rl" */
	{act = 14;}
	break;
	case 9:
/* #line 54 "ZIGT.c.rl" */
	{act = 15;}
	break;
	case 10:
/* #line 36 "ZIGT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 36 "ZIGT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 54 "ZIGT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 54 "ZIGT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 30 "ZIGT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 36 "ZIGT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 42 "ZIGT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 42 "ZIGT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 42 "ZIGT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 42 "ZIGT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 42 "ZIGT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 42 "ZIGT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 42 "ZIGT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 48 "ZIGT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 48 "ZIGT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 54 "ZIGT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 54 "ZIGT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 54 "ZIGT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 60 "ZIGT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 36 "ZIGT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 42 "ZIGT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 42 "ZIGT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 42 "ZIGT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 42 "ZIGT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 5:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 6:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 7:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 10:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 11:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 14:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 15:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 629 "ZIGT.rl.c" */
		}
	}

_again:
	_acts = _ZIGT_actions + _ZIGT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 640 "ZIGT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _ZIGT_eof_trans[cs] > 0 ) {
		_trans = _ZIGT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 142 "ZIGT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < ZIGT_first_final)
        o = ZIGTBAD;

    return o;
}
