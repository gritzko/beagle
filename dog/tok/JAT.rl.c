
/* #line 1 "JAT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "JAT.h"

ok64 JATonComment (u8cs tok, JATstate* state);
ok64 JATonString (u8cs tok, JATstate* state);
ok64 JATonNumber (u8cs tok, JATstate* state);
ok64 JATonAnnotation (u8cs tok, JATstate* state);
ok64 JATonWord (u8cs tok, JATstate* state);
ok64 JATonPunct (u8cs tok, JATstate* state);
ok64 JATonSpace (u8cs tok, JATstate* state);


/* #line 134 "JAT.c.rl" */



/* #line 16 "JAT.rl.c" */
static const char _JAT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	9, 1, 10, 1, 11, 1, 12, 1, 
	13, 1, 14, 1, 15, 1, 16, 1, 
	17, 1, 18, 1, 19, 1, 20, 1, 
	21, 1, 22, 1, 23, 1, 24, 1, 
	25, 1, 26, 1, 27, 1, 28, 1, 
	29, 1, 30, 1, 31, 1, 32, 1, 
	33, 1, 34, 1, 35, 1, 36, 1, 
	37, 1, 38, 1, 39, 1, 40, 1, 
	41, 1, 42, 1, 43, 2, 2, 3, 
	2, 2, 4, 2, 2, 5, 2, 2, 
	6, 2, 2, 7, 2, 2, 8
};

static const short _JAT_key_offsets[] = {
	0, 0, 2, 4, 20, 26, 32, 38, 
	44, 45, 46, 47, 49, 65, 71, 77, 
	83, 89, 90, 94, 96, 98, 99, 101, 
	105, 107, 109, 113, 115, 117, 119, 125, 
	129, 131, 137, 143, 172, 175, 176, 177, 
	185, 187, 189, 192, 195, 204, 211, 214, 
	215, 227, 235, 244, 251, 259, 266, 271, 
	283, 291, 300, 307, 308, 310, 312, 314, 
	323
};

static const unsigned char _JAT_trans_keys[] = {
	34u, 92u, 34u, 92u, 34u, 39u, 63u, 92u, 
	110u, 114u, 117u, 120u, 48u, 55u, 97u, 98u, 
	101u, 102u, 116u, 118u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 34u, 34u, 34u, 39u, 
	92u, 34u, 39u, 63u, 92u, 110u, 114u, 117u, 
	120u, 48u, 55u, 97u, 98u, 101u, 102u, 116u, 
	118u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 46u, 43u, 45u, 48u, 57u, 48u, 57u, 
	48u, 57u, 42u, 42u, 47u, 43u, 45u, 48u, 
	57u, 48u, 57u, 48u, 57u, 43u, 45u, 48u, 
	57u, 48u, 57u, 48u, 57u, 48u, 49u, 48u, 
	57u, 65u, 70u, 97u, 102u, 43u, 45u, 48u, 
	57u, 48u, 57u, 48u, 57u, 65u, 70u, 97u, 
	102u, 36u, 95u, 65u, 90u, 97u, 122u, 32u, 
	33u, 34u, 36u, 37u, 38u, 39u, 42u, 43u, 
	45u, 46u, 47u, 48u, 58u, 60u, 61u, 62u, 
	64u, 94u, 95u, 124u, 9u, 13u, 49u, 57u, 
	65u, 90u, 97u, 122u, 32u, 9u, 13u, 61u, 
	34u, 36u, 95u, 48u, 57u, 65u, 90u, 97u, 
	122u, 38u, 61u, 43u, 61u, 45u, 61u, 62u, 
	46u, 48u, 57u, 69u, 95u, 101u, 48u, 57u, 
	68u, 70u, 100u, 102u, 68u, 70u, 95u, 100u, 
	102u, 48u, 57u, 42u, 47u, 61u, 10u, 46u, 
	66u, 69u, 76u, 88u, 95u, 98u, 101u, 108u, 
	120u, 48u, 57u, 69u, 101u, 48u, 57u, 68u, 
	70u, 100u, 102u, 69u, 95u, 101u, 48u, 57u, 
	68u, 70u, 100u, 102u, 68u, 70u, 95u, 100u, 
	102u, 48u, 57u, 46u, 69u, 76u, 95u, 101u, 
	108u, 48u, 57u, 68u, 70u, 95u, 100u, 102u, 
	48u, 57u, 76u, 95u, 108u, 48u, 49u, 46u, 
	76u, 80u, 95u, 108u, 112u, 48u, 57u, 65u, 
	70u, 97u, 102u, 80u, 112u, 48u, 57u, 65u, 
	70u, 97u, 102u, 80u, 95u, 112u, 48u, 57u, 
	65u, 70u, 97u, 102u, 68u, 70u, 95u, 100u, 
	102u, 48u, 57u, 58u, 60u, 61u, 61u, 62u, 
	61u, 62u, 36u, 46u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 61u, 124u, 0
};

static const char _JAT_single_lengths[] = {
	0, 2, 2, 8, 0, 0, 0, 0, 
	1, 1, 1, 2, 8, 0, 0, 0, 
	0, 1, 2, 0, 0, 1, 2, 2, 
	0, 0, 2, 0, 0, 0, 0, 2, 
	0, 0, 2, 21, 1, 1, 1, 2, 
	2, 2, 1, 1, 3, 5, 3, 1, 
	10, 2, 3, 5, 6, 5, 3, 6, 
	2, 3, 5, 1, 2, 2, 2, 3, 
	2
};

static const char _JAT_range_lengths[] = {
	0, 0, 0, 4, 3, 3, 3, 3, 
	0, 0, 0, 0, 4, 3, 3, 3, 
	3, 0, 1, 1, 1, 0, 0, 1, 
	1, 1, 1, 1, 1, 1, 3, 1, 
	1, 3, 2, 4, 1, 0, 0, 3, 
	0, 0, 1, 1, 3, 1, 0, 0, 
	1, 3, 3, 1, 1, 1, 1, 3, 
	3, 3, 1, 0, 0, 0, 0, 3, 
	0
};

static const short _JAT_index_offsets[] = {
	0, 0, 3, 6, 19, 23, 27, 31, 
	35, 37, 39, 41, 44, 57, 61, 65, 
	69, 73, 75, 79, 81, 83, 85, 88, 
	92, 94, 96, 100, 102, 104, 106, 110, 
	114, 116, 120, 125, 151, 154, 156, 158, 
	164, 167, 170, 173, 176, 183, 190, 194, 
	196, 208, 214, 221, 228, 236, 243, 248, 
	258, 264, 271, 278, 280, 283, 286, 289, 
	296
};

static const char _JAT_indicies[] = {
	1, 2, 0, 3, 2, 0, 0, 0, 
	0, 0, 0, 0, 5, 6, 0, 0, 
	0, 0, 4, 7, 7, 7, 4, 8, 
	8, 8, 4, 6, 6, 6, 4, 0, 
	0, 0, 4, 11, 10, 12, 10, 13, 
	10, 15, 16, 14, 14, 14, 14, 14, 
	14, 14, 17, 18, 14, 14, 14, 14, 
	4, 19, 19, 19, 4, 20, 20, 20, 
	4, 18, 18, 18, 4, 14, 14, 14, 
	4, 22, 21, 24, 24, 25, 23, 25, 
	23, 26, 23, 29, 28, 29, 30, 28, 
	32, 32, 33, 31, 33, 31, 34, 31, 
	36, 36, 37, 35, 37, 38, 39, 35, 
	40, 38, 41, 41, 41, 38, 43, 43, 
	44, 42, 44, 42, 45, 45, 45, 42, 
	46, 46, 46, 46, 4, 48, 49, 50, 
	51, 49, 52, 14, 49, 53, 54, 55, 
	56, 57, 58, 59, 49, 60, 61, 49, 
	51, 62, 48, 39, 51, 51, 47, 48, 
	48, 63, 22, 38, 10, 64, 51, 51, 
	51, 51, 51, 65, 22, 22, 66, 22, 
	22, 66, 22, 22, 66, 68, 26, 67, 
	71, 72, 71, 26, 70, 70, 69, 70, 
	70, 24, 70, 70, 25, 69, 28, 73, 
	22, 66, 74, 73, 76, 77, 78, 79, 
	80, 81, 77, 78, 79, 80, 39, 75, 
	84, 84, 34, 83, 83, 82, 84, 85, 
	84, 34, 83, 83, 82, 83, 83, 32, 
	83, 83, 33, 82, 76, 78, 79, 81, 
	78, 79, 39, 75, 87, 87, 36, 87, 
	87, 37, 86, 89, 77, 89, 40, 88, 
	91, 92, 93, 80, 92, 93, 41, 41, 
	41, 90, 93, 93, 45, 45, 45, 90, 
	93, 94, 93, 45, 45, 45, 90, 95, 
	95, 43, 95, 95, 44, 90, 22, 66, 
	96, 22, 66, 22, 97, 66, 22, 96, 
	98, 46, 46, 46, 46, 46, 46, 99, 
	22, 22, 66, 0
};

static const char _JAT_trans_targs[] = {
	2, 38, 3, 35, 0, 4, 7, 5, 
	6, 35, 8, 9, 10, 35, 11, 35, 
	12, 13, 16, 14, 15, 35, 35, 35, 
	19, 45, 44, 35, 21, 22, 35, 35, 
	24, 51, 50, 35, 27, 53, 35, 52, 
	54, 55, 35, 32, 58, 57, 63, 35, 
	36, 37, 1, 39, 40, 41, 42, 43, 
	46, 48, 59, 60, 61, 34, 64, 35, 
	35, 35, 35, 35, 17, 35, 35, 18, 
	20, 47, 35, 35, 49, 29, 26, 35, 
	30, 28, 35, 35, 23, 25, 35, 35, 
	35, 35, 35, 56, 35, 31, 33, 35, 
	37, 62, 35, 35
};

static const char _JAT_trans_actions[] = {
	0, 5, 0, 11, 0, 0, 0, 0, 
	0, 61, 0, 0, 0, 9, 0, 13, 
	0, 0, 0, 0, 0, 73, 29, 67, 
	0, 5, 5, 71, 0, 0, 7, 65, 
	0, 5, 5, 69, 0, 83, 75, 86, 
	80, 77, 63, 0, 5, 5, 0, 31, 
	0, 92, 0, 0, 0, 0, 0, 5, 
	5, 86, 0, 0, 0, 0, 0, 59, 
	35, 51, 55, 57, 0, 43, 23, 0, 
	0, 0, 33, 47, 5, 0, 0, 27, 
	0, 0, 41, 21, 0, 0, 45, 25, 
	39, 19, 37, 5, 17, 0, 0, 15, 
	89, 0, 53, 49
};

static const char _JAT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const char _JAT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 3, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const short _JAT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	10, 10, 10, 0, 0, 0, 0, 0, 
	0, 22, 24, 24, 24, 28, 28, 32, 
	32, 32, 36, 39, 36, 39, 39, 43, 
	43, 43, 0, 0, 64, 39, 65, 66, 
	67, 67, 67, 68, 70, 70, 67, 75, 
	76, 83, 83, 83, 76, 87, 89, 91, 
	91, 91, 91, 67, 67, 67, 99, 100, 
	67
};

static const int JAT_start = 35;
static const int JAT_first_final = 35;
static const int JAT_error = 0;

static const int JAT_en_main = 35;


/* #line 137 "JAT.c.rl" */

ok64 JATLexer(JATstate* state) {

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

    
/* #line 255 "JAT.rl.c" */
	{
	cs = JAT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 155 "JAT.c.rl" */
    
/* #line 261 "JAT.rl.c" */
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
	_acts = _JAT_actions + _JAT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 280 "JAT.rl.c" */
		}
	}

	_keys = _JAT_trans_keys + _JAT_key_offsets[cs];
	_trans = _JAT_index_offsets[cs];

	_klen = _JAT_single_lengths[cs];
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

	_klen = _JAT_range_lengths[cs];
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
	_trans = _JAT_indicies[_trans];
_eof_trans:
	cs = _JAT_trans_targs[_trans];

	if ( _JAT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _JAT_actions + _JAT_trans_actions[_trans];
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
/* #line 44 "JAT.c.rl" */
	{act = 6;}
	break;
	case 4:
/* #line 44 "JAT.c.rl" */
	{act = 8;}
	break;
	case 5:
/* #line 44 "JAT.c.rl" */
	{act = 11;}
	break;
	case 6:
/* #line 44 "JAT.c.rl" */
	{act = 12;}
	break;
	case 7:
/* #line 62 "JAT.c.rl" */
	{act = 15;}
	break;
	case 8:
/* #line 62 "JAT.c.rl" */
	{act = 16;}
	break;
	case 9:
/* #line 32 "JAT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 38 "JAT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 38 "JAT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 38 "JAT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 44 "JAT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 44 "JAT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 44 "JAT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 44 "JAT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 44 "JAT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 44 "JAT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 44 "JAT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 62 "JAT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 62 "JAT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 32 "JAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 38 "JAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 44 "JAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 44 "JAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 44 "JAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 44 "JAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 44 "JAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 44 "JAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 50 "JAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonAnnotation(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 56 "JAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 62 "JAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 62 "JAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 62 "JAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 68 "JAT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 38 "JAT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 44 "JAT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 44 "JAT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 39:
/* #line 44 "JAT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 40:
/* #line 44 "JAT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 41:
/* #line 62 "JAT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 42:
/* #line 62 "JAT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 43:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 6:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 8:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 11:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 15:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 16:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 691 "JAT.rl.c" */
		}
	}

_again:
	_acts = _JAT_actions + _JAT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 702 "JAT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _JAT_eof_trans[cs] > 0 ) {
		_trans = _JAT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 156 "JAT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < JAT_first_final)
        o = JATBAD;

    return o;
}
