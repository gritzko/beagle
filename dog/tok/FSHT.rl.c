
/* #line 1 "FSHT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "FSHT.h"

ok64 FSHTonComment (u8cs tok, FSHTstate* state);
ok64 FSHTonString (u8cs tok, FSHTstate* state);
ok64 FSHTonNumber (u8cs tok, FSHTstate* state);
ok64 FSHTonWord (u8cs tok, FSHTstate* state);
ok64 FSHTonPunct (u8cs tok, FSHTstate* state);
ok64 FSHTonSpace (u8cs tok, FSHTstate* state);


/* #line 119 "FSHT.c.rl" */



/* #line 15 "FSHT.rl.c" */
static const char _FSHT_actions[] = {
	0, 1, 2, 1, 3, 1, 10, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19, 1, 20, 1, 21, 1, 22, 1, 
	23, 1, 24, 1, 25, 1, 26, 1, 
	27, 1, 28, 1, 29, 1, 30, 1, 
	31, 1, 32, 1, 33, 1, 34, 1, 
	35, 1, 36, 2, 0, 1, 2, 3, 
	4, 2, 3, 5, 2, 3, 6, 2, 
	3, 7, 2, 3, 8, 2, 3, 9
	
};

static const short _FSHT_key_offsets[] = {
	0, 0, 2, 4, 19, 25, 31, 37, 
	43, 49, 55, 61, 67, 68, 69, 70, 
	72, 73, 88, 94, 100, 106, 112, 118, 
	124, 130, 136, 137, 139, 143, 145, 147, 
	151, 153, 155, 159, 161, 163, 165, 167, 
	173, 174, 175, 176, 203, 206, 207, 208, 
	209, 210, 212, 215, 216, 221, 224, 226, 
	227, 239, 243, 248, 251, 257, 260, 263, 
	266, 273, 276, 280, 281, 289
};

static const unsigned char _FSHT_trans_keys[] = {
	34u, 92u, 34u, 92u, 34u, 39u, 48u, 85u, 
	92u, 110u, 114u, 117u, 120u, 97u, 98u, 101u, 
	102u, 116u, 118u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 34u, 34u, 34u, 39u, 92u, 
	39u, 34u, 39u, 48u, 85u, 92u, 110u, 114u, 
	117u, 120u, 97u, 98u, 101u, 102u, 116u, 118u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	42u, 41u, 42u, 43u, 45u, 48u, 57u, 48u, 
	57u, 48u, 57u, 43u, 45u, 48u, 57u, 48u, 
	57u, 48u, 57u, 43u, 45u, 48u, 57u, 48u, 
	57u, 48u, 57u, 48u, 49u, 48u, 55u, 48u, 
	57u, 65u, 70u, 97u, 102u, 62u, 34u, 34u, 
	32u, 33u, 34u, 38u, 39u, 40u, 45u, 46u, 
	47u, 48u, 58u, 60u, 61u, 62u, 64u, 95u, 
	124u, 9u, 13u, 42u, 43u, 49u, 57u, 65u, 
	90u, 97u, 122u, 32u, 9u, 13u, 61u, 34u, 
	38u, 42u, 61u, 62u, 46u, 48u, 57u, 46u, 
	69u, 95u, 101u, 48u, 57u, 95u, 48u, 57u, 
	47u, 61u, 10u, 46u, 66u, 69u, 79u, 88u, 
	95u, 98u, 101u, 111u, 120u, 48u, 57u, 69u, 
	101u, 48u, 57u, 69u, 95u, 101u, 48u, 57u, 
	95u, 48u, 57u, 46u, 69u, 95u, 101u, 48u, 
	57u, 95u, 48u, 57u, 95u, 48u, 49u, 95u, 
	48u, 55u, 95u, 48u, 57u, 65u, 70u, 97u, 
	102u, 58u, 62u, 63u, 45u, 124u, 60u, 62u, 
	34u, 39u, 95u, 48u, 57u, 65u, 90u, 97u, 
	122u, 62u, 124u, 0
};

static const char _FSHT_single_lengths[] = {
	0, 2, 2, 9, 0, 0, 0, 0, 
	0, 0, 0, 0, 1, 1, 1, 2, 
	1, 9, 0, 0, 0, 0, 0, 0, 
	0, 0, 1, 2, 2, 0, 0, 2, 
	0, 0, 2, 0, 0, 0, 0, 0, 
	1, 1, 1, 17, 1, 1, 1, 1, 
	1, 0, 1, 1, 3, 1, 2, 1, 
	10, 2, 3, 1, 4, 1, 1, 1, 
	1, 3, 2, 1, 2, 2
};

static const char _FSHT_range_lengths[] = {
	0, 0, 0, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 0, 0, 0, 0, 
	0, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 0, 0, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 3, 
	0, 0, 0, 5, 1, 0, 0, 0, 
	0, 1, 1, 0, 1, 1, 0, 0, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	3, 0, 1, 0, 3, 0
};

static const short _FSHT_index_offsets[] = {
	0, 0, 3, 6, 19, 23, 27, 31, 
	35, 39, 43, 47, 51, 53, 55, 57, 
	60, 62, 75, 79, 83, 87, 91, 95, 
	99, 103, 107, 109, 112, 116, 118, 120, 
	124, 126, 128, 132, 134, 136, 138, 140, 
	144, 146, 148, 150, 173, 176, 178, 180, 
	182, 184, 186, 189, 191, 196, 199, 202, 
	204, 216, 220, 225, 228, 234, 237, 240, 
	243, 248, 252, 256, 258, 264
};

static const char _FSHT_indicies[] = {
	1, 2, 0, 3, 2, 0, 0, 0, 
	0, 5, 0, 0, 0, 6, 7, 0, 
	0, 0, 4, 8, 8, 8, 4, 9, 
	9, 9, 4, 10, 10, 10, 4, 6, 
	6, 6, 4, 11, 11, 11, 4, 7, 
	7, 7, 4, 12, 12, 12, 4, 0, 
	0, 0, 4, 15, 14, 16, 14, 17, 
	14, 4, 19, 18, 20, 4, 18, 18, 
	18, 21, 18, 18, 18, 22, 23, 18, 
	18, 18, 4, 24, 24, 24, 4, 25, 
	25, 25, 4, 26, 26, 26, 4, 22, 
	22, 22, 4, 27, 27, 27, 4, 23, 
	23, 23, 4, 28, 28, 28, 4, 18, 
	18, 18, 4, 31, 30, 32, 31, 30, 
	34, 34, 35, 33, 35, 33, 36, 33, 
	38, 38, 39, 37, 39, 37, 40, 37, 
	42, 42, 43, 41, 43, 44, 45, 41, 
	46, 44, 47, 44, 48, 48, 48, 44, 
	49, 29, 50, 4, 51, 50, 53, 54, 
	55, 56, 57, 58, 59, 60, 61, 62, 
	63, 64, 54, 59, 65, 66, 67, 53, 
	54, 45, 66, 66, 52, 53, 53, 68, 
	49, 69, 14, 70, 49, 69, 30, 69, 
	49, 69, 72, 36, 71, 49, 73, 75, 
	76, 75, 36, 74, 34, 35, 74, 77, 
	49, 69, 78, 77, 80, 81, 82, 83, 
	84, 85, 81, 82, 83, 84, 45, 79, 
	87, 87, 40, 86, 87, 88, 87, 40, 
	86, 38, 39, 86, 80, 82, 85, 82, 
	45, 79, 42, 43, 89, 81, 46, 90, 
	83, 47, 91, 84, 48, 48, 48, 92, 
	49, 49, 93, 69, 49, 49, 49, 69, 
	50, 94, 66, 66, 66, 66, 66, 95, 
	49, 49, 69, 0
};

static const char _FSHT_trans_targs[] = {
	2, 46, 3, 43, 0, 4, 8, 10, 
	5, 6, 7, 9, 11, 43, 12, 13, 
	14, 43, 16, 17, 43, 18, 22, 24, 
	19, 20, 21, 23, 25, 43, 26, 27, 
	43, 43, 29, 53, 52, 43, 32, 59, 
	58, 43, 35, 61, 43, 60, 62, 63, 
	64, 43, 42, 67, 43, 44, 45, 1, 
	47, 15, 48, 49, 50, 54, 56, 65, 
	66, 41, 68, 69, 43, 43, 43, 43, 
	51, 43, 43, 28, 30, 55, 43, 43, 
	57, 37, 34, 38, 39, 36, 43, 31, 
	33, 43, 43, 43, 43, 40, 43, 43
};

static const char _FSHT_trans_actions[] = {
	0, 3, 0, 9, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 47, 0, 0, 
	0, 7, 0, 0, 11, 0, 0, 0, 
	0, 0, 0, 0, 0, 55, 0, 0, 
	5, 51, 0, 3, 3, 49, 0, 3, 
	3, 53, 0, 74, 57, 77, 71, 68, 
	65, 13, 0, 62, 15, 0, 0, 0, 
	0, 0, 3, 0, 0, 0, 77, 3, 
	0, 0, 0, 0, 45, 41, 21, 43, 
	0, 39, 31, 0, 0, 0, 17, 35, 
	3, 0, 0, 0, 0, 0, 29, 0, 
	0, 33, 27, 25, 23, 0, 19, 37
};

static const char _FSHT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 59, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0
};

static const char _FSHT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0
};

static const short _FSHT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 14, 14, 14, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 30, 30, 34, 34, 34, 38, 
	38, 38, 42, 45, 42, 45, 45, 45, 
	30, 0, 45, 0, 69, 70, 71, 70, 
	70, 70, 72, 74, 75, 75, 70, 79, 
	80, 87, 87, 87, 80, 90, 91, 92, 
	93, 70, 70, 95, 96, 70
};

static const int FSHT_start = 43;
static const int FSHT_first_final = 43;
static const int FSHT_error = 0;

static const int FSHT_en_main = 43;


/* #line 122 "FSHT.c.rl" */

ok64 FSHTLexer(FSHTstate* state) {

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

    
/* #line 243 "FSHT.rl.c" */
	{
	cs = FSHT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 140 "FSHT.c.rl" */
    
/* #line 249 "FSHT.rl.c" */
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
	_acts = _FSHT_actions + _FSHT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 2:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 268 "FSHT.rl.c" */
		}
	}

	_keys = _FSHT_trans_keys + _FSHT_key_offsets[cs];
	_trans = _FSHT_index_offsets[cs];

	_klen = _FSHT_single_lengths[cs];
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

	_klen = _FSHT_range_lengths[cs];
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
	_trans = _FSHT_indicies[_trans];
_eof_trans:
	cs = _FSHT_trans_targs[_trans];

	if ( _FSHT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _FSHT_actions + _FSHT_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 3:
/* #line 1 "NONE" */
	{te = p+1;}
	break;
	case 4:
/* #line 37 "FSHT.c.rl" */
	{act = 4;}
	break;
	case 5:
/* #line 43 "FSHT.c.rl" */
	{act = 7;}
	break;
	case 6:
/* #line 43 "FSHT.c.rl" */
	{act = 8;}
	break;
	case 7:
/* #line 43 "FSHT.c.rl" */
	{act = 9;}
	break;
	case 8:
/* #line 43 "FSHT.c.rl" */
	{act = 12;}
	break;
	case 9:
/* #line 43 "FSHT.c.rl" */
	{act = 13;}
	break;
	case 10:
/* #line 31 "FSHT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 37 "FSHT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 37 "FSHT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 37 "FSHT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 55 "FSHT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 55 "FSHT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 31 "FSHT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 37 "FSHT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 37 "FSHT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 43 "FSHT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 43 "FSHT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 43 "FSHT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 43 "FSHT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 43 "FSHT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 43 "FSHT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 43 "FSHT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 49 "FSHT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 55 "FSHT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 55 "FSHT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 55 "FSHT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 61 "FSHT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 37 "FSHT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 43 "FSHT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 43 "FSHT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 43 "FSHT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 55 "FSHT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 0:
	{{cs = 0;goto _again;}}
	break;
	case 4:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 7:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 8:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 9:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 13:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 618 "FSHT.rl.c" */
		}
	}

_again:
	_acts = _FSHT_actions + _FSHT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
	case 1:
/* #line 1 "NONE" */
	{act = 0;}
	break;
/* #line 632 "FSHT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _FSHT_eof_trans[cs] > 0 ) {
		_trans = _FSHT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 141 "FSHT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < FSHT_first_final)
        o = FSHTBAD;

    return o;
}
