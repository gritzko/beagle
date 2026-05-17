
/* #line 1 "GOT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "GOT.h"

ok64 GOTonComment (u8cs tok, GOTstate* state);
ok64 GOTonString (u8cs tok, GOTstate* state);
ok64 GOTonNumber (u8cs tok, GOTstate* state);
ok64 GOTonWord (u8cs tok, GOTstate* state);
ok64 GOTonPunct (u8cs tok, GOTstate* state);
ok64 GOTonSpace (u8cs tok, GOTstate* state);


/* #line 123 "GOT.c.rl" */



/* #line 15 "GOT.rl.c" */
static const char _GOT_actions[] = {
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

static const short _GOT_key_offsets[] = {
	0, 0, 2, 18, 24, 30, 36, 42, 
	48, 54, 60, 66, 68, 84, 90, 96, 
	102, 108, 114, 120, 126, 132, 133, 137, 
	139, 141, 142, 144, 148, 150, 152, 156, 
	158, 160, 162, 164, 170, 174, 176, 182, 
	183, 210, 213, 214, 217, 219, 222, 225, 
	231, 235, 238, 239, 252, 257, 263, 267, 
	274, 278, 282, 286, 297, 306, 316, 320, 
	322, 325, 327, 334
};

static const unsigned char _GOT_trans_keys[] = {
	34u, 92u, 34u, 39u, 85u, 92u, 110u, 114u, 
	117u, 120u, 48u, 55u, 97u, 98u, 101u, 102u, 
	116u, 118u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 39u, 92u, 34u, 39u, 85u, 92u, 
	110u, 114u, 117u, 120u, 48u, 55u, 97u, 98u, 
	101u, 102u, 116u, 118u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 46u, 43u, 45u, 48u, 
	57u, 48u, 57u, 48u, 57u, 42u, 42u, 47u, 
	43u, 45u, 48u, 57u, 48u, 57u, 48u, 57u, 
	43u, 45u, 48u, 57u, 48u, 57u, 48u, 57u, 
	48u, 49u, 48u, 55u, 48u, 57u, 65u, 70u, 
	97u, 102u, 43u, 45u, 48u, 57u, 48u, 57u, 
	48u, 57u, 65u, 70u, 97u, 102u, 96u, 32u, 
	33u, 34u, 37u, 38u, 39u, 42u, 43u, 45u, 
	46u, 47u, 48u, 58u, 60u, 61u, 62u, 94u, 
	96u, 124u, 9u, 13u, 49u, 57u, 65u, 90u, 
	95u, 122u, 32u, 9u, 13u, 61u, 38u, 61u, 
	94u, 43u, 61u, 45u, 61u, 62u, 46u, 48u, 
	57u, 69u, 95u, 101u, 105u, 48u, 57u, 95u, 
	105u, 48u, 57u, 42u, 47u, 61u, 10u, 46u, 
	66u, 69u, 79u, 88u, 95u, 98u, 101u, 105u, 
	111u, 120u, 48u, 57u, 69u, 101u, 105u, 48u, 
	57u, 69u, 95u, 101u, 105u, 48u, 57u, 95u, 
	105u, 48u, 57u, 46u, 69u, 95u, 101u, 105u, 
	48u, 57u, 95u, 105u, 48u, 57u, 95u, 105u, 
	48u, 49u, 95u, 105u, 48u, 55u, 46u, 80u, 
	95u, 105u, 112u, 48u, 57u, 65u, 70u, 97u, 
	102u, 80u, 105u, 112u, 48u, 57u, 65u, 70u, 
	97u, 102u, 80u, 95u, 105u, 112u, 48u, 57u, 
	65u, 70u, 97u, 102u, 95u, 105u, 48u, 57u, 
	58u, 61u, 45u, 60u, 61u, 61u, 62u, 95u, 
	48u, 57u, 65u, 90u, 97u, 122u, 61u, 124u, 
	0
};

static const char _GOT_single_lengths[] = {
	0, 2, 8, 0, 0, 0, 0, 0, 
	0, 0, 0, 2, 8, 0, 0, 0, 
	0, 0, 0, 0, 0, 1, 2, 0, 
	0, 1, 2, 2, 0, 0, 2, 0, 
	0, 0, 0, 0, 2, 0, 0, 1, 
	19, 1, 1, 3, 2, 1, 1, 4, 
	2, 3, 1, 11, 3, 4, 2, 5, 
	2, 2, 2, 5, 3, 4, 2, 2, 
	3, 2, 1, 2
};

static const char _GOT_range_lengths[] = {
	0, 0, 4, 3, 3, 3, 3, 3, 
	3, 3, 3, 0, 4, 3, 3, 3, 
	3, 3, 3, 3, 3, 0, 1, 1, 
	1, 0, 0, 1, 1, 1, 1, 1, 
	1, 1, 1, 3, 1, 1, 3, 0, 
	4, 1, 0, 0, 0, 1, 1, 1, 
	1, 0, 0, 1, 1, 1, 1, 1, 
	1, 1, 1, 3, 3, 3, 1, 0, 
	0, 0, 3, 0
};

static const short _GOT_index_offsets[] = {
	0, 0, 3, 16, 20, 24, 28, 32, 
	36, 40, 44, 48, 51, 64, 68, 72, 
	76, 80, 84, 88, 92, 96, 98, 102, 
	104, 106, 108, 111, 115, 117, 119, 123, 
	125, 127, 129, 131, 135, 139, 141, 145, 
	147, 171, 174, 176, 180, 183, 186, 189, 
	195, 199, 203, 205, 218, 223, 229, 233, 
	240, 244, 248, 252, 261, 268, 276, 280, 
	283, 287, 290, 295
};

static const char _GOT_indicies[] = {
	1, 2, 0, 0, 0, 4, 0, 0, 
	0, 5, 6, 0, 0, 0, 0, 3, 
	7, 7, 7, 3, 8, 8, 8, 3, 
	9, 9, 9, 3, 5, 5, 5, 3, 
	10, 10, 10, 3, 6, 6, 6, 3, 
	11, 11, 11, 3, 0, 0, 0, 3, 
	13, 14, 12, 12, 12, 15, 12, 12, 
	12, 16, 17, 12, 12, 12, 12, 3, 
	18, 18, 18, 3, 19, 19, 19, 3, 
	20, 20, 20, 3, 16, 16, 16, 3, 
	21, 21, 21, 3, 17, 17, 17, 3, 
	22, 22, 22, 3, 12, 12, 12, 3, 
	24, 23, 26, 26, 27, 25, 27, 25, 
	28, 25, 31, 30, 31, 32, 30, 34, 
	34, 35, 33, 35, 33, 36, 33, 38, 
	38, 39, 37, 39, 40, 41, 37, 42, 
	40, 43, 40, 44, 44, 44, 40, 46, 
	46, 47, 45, 47, 45, 48, 48, 48, 
	45, 50, 49, 52, 53, 0, 53, 54, 
	12, 53, 55, 56, 57, 58, 59, 60, 
	61, 53, 62, 53, 49, 64, 52, 41, 
	63, 63, 51, 52, 52, 65, 24, 40, 
	24, 24, 67, 66, 24, 24, 66, 24, 
	24, 66, 69, 28, 68, 71, 72, 71, 
	73, 28, 70, 26, 73, 27, 70, 30, 
	74, 24, 66, 75, 74, 77, 78, 79, 
	80, 81, 82, 78, 79, 83, 80, 81, 
	41, 76, 85, 85, 86, 36, 84, 85, 
	87, 85, 86, 36, 84, 34, 86, 35, 
	84, 77, 79, 82, 79, 83, 41, 76, 
	38, 89, 39, 88, 78, 91, 42, 90, 
	80, 93, 43, 92, 95, 96, 81, 97, 
	96, 44, 44, 44, 94, 96, 97, 96, 
	48, 48, 48, 94, 96, 98, 97, 96, 
	48, 48, 48, 94, 46, 97, 47, 94, 
	24, 24, 66, 24, 67, 24, 66, 24, 
	67, 66, 63, 63, 63, 63, 99, 24, 
	24, 66, 0
};

static const char _GOT_trans_targs[] = {
	1, 40, 2, 0, 3, 7, 9, 4, 
	5, 6, 8, 10, 11, 40, 12, 13, 
	17, 19, 14, 15, 16, 18, 20, 40, 
	40, 40, 23, 48, 47, 40, 25, 26, 
	40, 40, 28, 54, 53, 40, 31, 56, 
	40, 55, 57, 58, 59, 40, 37, 62, 
	61, 39, 40, 40, 41, 42, 43, 44, 
	45, 46, 49, 51, 63, 64, 65, 66, 
	67, 40, 40, 42, 40, 21, 40, 22, 
	24, 40, 50, 40, 40, 52, 33, 30, 
	34, 35, 32, 40, 40, 27, 40, 29, 
	40, 40, 40, 40, 40, 40, 40, 60, 
	36, 40, 38, 40
};

static const char _GOT_trans_actions[] = {
	0, 9, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 13, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 67, 
	29, 61, 0, 5, 5, 65, 0, 0, 
	7, 59, 0, 5, 5, 63, 0, 80, 
	69, 83, 77, 74, 71, 57, 0, 5, 
	5, 0, 11, 31, 0, 89, 0, 0, 
	0, 5, 5, 83, 0, 0, 0, 0, 
	0, 55, 51, 86, 53, 0, 43, 0, 
	0, 23, 0, 33, 47, 5, 0, 0, 
	0, 0, 0, 27, 41, 0, 21, 0, 
	45, 25, 39, 19, 37, 17, 35, 5, 
	0, 15, 0, 49
};

static const char _GOT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	1, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0
};

static const char _GOT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	3, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0
};

static const short _GOT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 24, 26, 26, 
	26, 30, 30, 34, 34, 34, 38, 41, 
	38, 41, 41, 41, 46, 46, 46, 0, 
	0, 66, 41, 67, 67, 67, 69, 71, 
	71, 67, 76, 77, 85, 85, 85, 77, 
	89, 91, 93, 95, 95, 95, 95, 67, 
	67, 67, 100, 67
};

static const int GOT_start = 40;
static const int GOT_first_final = 40;
static const int GOT_error = 0;

static const int GOT_en_main = 40;


/* #line 126 "GOT.c.rl" */

ok64 GOTLexer(GOTstate* state) {

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

    
/* #line 256 "GOT.rl.c" */
	{
	cs = GOT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 144 "GOT.c.rl" */
    
/* #line 262 "GOT.rl.c" */
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
	_acts = _GOT_actions + _GOT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 281 "GOT.rl.c" */
		}
	}

	_keys = _GOT_trans_keys + _GOT_key_offsets[cs];
	_trans = _GOT_index_offsets[cs];

	_klen = _GOT_single_lengths[cs];
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

	_klen = _GOT_range_lengths[cs];
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
	_trans = _GOT_indicies[_trans];
_eof_trans:
	cs = _GOT_trans_targs[_trans];

	if ( _GOT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _GOT_actions + _GOT_trans_actions[_trans];
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
/* #line 44 "GOT.c.rl" */
	{act = 6;}
	break;
	case 4:
/* #line 44 "GOT.c.rl" */
	{act = 7;}
	break;
	case 5:
/* #line 44 "GOT.c.rl" */
	{act = 8;}
	break;
	case 6:
/* #line 44 "GOT.c.rl" */
	{act = 11;}
	break;
	case 7:
/* #line 44 "GOT.c.rl" */
	{act = 12;}
	break;
	case 8:
/* #line 56 "GOT.c.rl" */
	{act = 14;}
	break;
	case 9:
/* #line 56 "GOT.c.rl" */
	{act = 15;}
	break;
	case 10:
/* #line 32 "GOT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 38 "GOT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 38 "GOT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 38 "GOT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 44 "GOT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 44 "GOT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 44 "GOT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 44 "GOT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 44 "GOT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 44 "GOT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 44 "GOT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 56 "GOT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 56 "GOT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 32 "GOT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 44 "GOT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 44 "GOT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 44 "GOT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 44 "GOT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 44 "GOT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 44 "GOT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 44 "GOT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 50 "GOT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 56 "GOT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 56 "GOT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 62 "GOT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 44 "GOT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 44 "GOT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 44 "GOT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 44 "GOT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 39:
/* #line 56 "GOT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 40:
/* #line 56 "GOT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonPunct(tok, state);
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
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 7:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 8:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 11:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 14:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 15:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 679 "GOT.rl.c" */
		}
	}

_again:
	_acts = _GOT_actions + _GOT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 690 "GOT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _GOT_eof_trans[cs] > 0 ) {
		_trans = _GOT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 145 "GOT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < GOT_first_final)
        o = GOTBAD;

    return o;
}
