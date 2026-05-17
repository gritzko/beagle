
/* #line 1 "KTT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "KTT.h"

ok64 KTTonComment (u8cs tok, KTTstate* state);
ok64 KTTonString (u8cs tok, KTTstate* state);
ok64 KTTonNumber (u8cs tok, KTTstate* state);
ok64 KTTonAnnotation (u8cs tok, KTTstate* state);
ok64 KTTonWord (u8cs tok, KTTstate* state);
ok64 KTTonPunct (u8cs tok, KTTstate* state);
ok64 KTTonSpace (u8cs tok, KTTstate* state);


/* #line 130 "KTT.c.rl" */



/* #line 16 "KTT.rl.c" */
static const char _KTT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	9, 1, 10, 1, 11, 1, 12, 1, 
	13, 1, 14, 1, 15, 1, 16, 1, 
	17, 1, 18, 1, 19, 1, 20, 1, 
	21, 1, 22, 1, 23, 1, 24, 1, 
	25, 1, 26, 1, 27, 1, 28, 1, 
	29, 1, 30, 1, 31, 1, 32, 1, 
	33, 1, 34, 1, 35, 1, 36, 1, 
	37, 1, 38, 1, 39, 2, 2, 3, 
	2, 2, 4, 2, 2, 5, 2, 2, 
	6, 2, 2, 7, 2, 2, 8
};

static const short _KTT_key_offsets[] = {
	0, 0, 2, 4, 20, 26, 32, 38, 
	44, 45, 46, 47, 49, 65, 71, 77, 
	83, 89, 93, 95, 97, 98, 100, 104, 
	106, 108, 112, 114, 116, 118, 124, 129, 
	158, 161, 162, 163, 165, 167, 170, 173, 
	182, 189, 192, 193, 207, 215, 224, 231, 
	241, 248, 255, 266, 267, 269, 271, 273, 
	275, 283, 290
};

static const unsigned char _KTT_trans_keys[] = {
	34u, 92u, 34u, 92u, 34u, 36u, 39u, 48u, 
	63u, 92u, 110u, 114u, 117u, 120u, 97u, 98u, 
	101u, 102u, 116u, 118u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 34u, 34u, 34u, 39u, 
	92u, 34u, 36u, 39u, 48u, 63u, 92u, 110u, 
	114u, 117u, 120u, 97u, 98u, 101u, 102u, 116u, 
	118u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 43u, 45u, 48u, 57u, 48u, 57u, 48u, 
	57u, 42u, 42u, 47u, 43u, 45u, 48u, 57u, 
	48u, 57u, 48u, 57u, 43u, 45u, 48u, 57u, 
	48u, 57u, 48u, 57u, 48u, 49u, 48u, 57u, 
	65u, 70u, 97u, 102u, 95u, 65u, 90u, 97u, 
	122u, 32u, 33u, 34u, 37u, 38u, 39u, 42u, 
	43u, 45u, 46u, 47u, 48u, 58u, 60u, 61u, 
	62u, 63u, 64u, 94u, 95u, 124u, 9u, 13u, 
	49u, 57u, 65u, 90u, 97u, 122u, 32u, 9u, 
	13u, 61u, 34u, 38u, 61u, 43u, 61u, 45u, 
	61u, 62u, 46u, 48u, 57u, 69u, 95u, 101u, 
	48u, 57u, 68u, 70u, 100u, 102u, 68u, 70u, 
	95u, 100u, 102u, 48u, 57u, 42u, 47u, 61u, 
	10u, 46u, 66u, 69u, 76u, 85u, 88u, 95u, 
	98u, 101u, 108u, 117u, 120u, 48u, 57u, 69u, 
	101u, 48u, 57u, 68u, 70u, 100u, 102u, 69u, 
	95u, 101u, 48u, 57u, 68u, 70u, 100u, 102u, 
	68u, 70u, 95u, 100u, 102u, 48u, 57u, 46u, 
	69u, 76u, 85u, 95u, 101u, 108u, 117u, 48u, 
	57u, 68u, 70u, 95u, 100u, 102u, 48u, 57u, 
	76u, 85u, 95u, 108u, 117u, 48u, 49u, 76u, 
	85u, 95u, 108u, 117u, 48u, 57u, 65u, 70u, 
	97u, 102u, 58u, 60u, 61u, 61u, 62u, 61u, 
	62u, 46u, 58u, 46u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 61u, 124u, 0
};

static const char _KTT_single_lengths[] = {
	0, 2, 2, 10, 0, 0, 0, 0, 
	1, 1, 1, 2, 10, 0, 0, 0, 
	0, 2, 0, 0, 1, 2, 2, 0, 
	0, 2, 0, 0, 0, 0, 1, 21, 
	1, 1, 1, 2, 2, 1, 1, 3, 
	5, 3, 1, 12, 2, 3, 5, 8, 
	5, 5, 5, 1, 2, 0, 2, 2, 
	2, 1, 2
};

static const char _KTT_range_lengths[] = {
	0, 0, 0, 3, 3, 3, 3, 3, 
	0, 0, 0, 0, 3, 3, 3, 3, 
	3, 1, 1, 1, 0, 0, 1, 1, 
	1, 1, 1, 1, 1, 3, 2, 4, 
	1, 0, 0, 0, 0, 1, 1, 3, 
	1, 0, 0, 1, 3, 3, 1, 1, 
	1, 1, 3, 0, 0, 1, 0, 0, 
	3, 3, 0
};

static const short _KTT_index_offsets[] = {
	0, 0, 3, 6, 20, 24, 28, 32, 
	36, 38, 40, 42, 45, 59, 63, 67, 
	71, 75, 79, 81, 83, 85, 88, 92, 
	94, 96, 100, 102, 104, 106, 110, 114, 
	140, 143, 145, 147, 150, 153, 156, 159, 
	166, 173, 177, 179, 193, 199, 206, 213, 
	223, 230, 237, 246, 248, 251, 253, 256, 
	259, 265, 270
};

static const char _KTT_indicies[] = {
	1, 2, 0, 3, 2, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 5, 6, 
	0, 0, 0, 4, 7, 7, 7, 4, 
	6, 6, 6, 4, 8, 8, 8, 4, 
	0, 0, 0, 4, 11, 10, 12, 10, 
	13, 10, 15, 16, 14, 14, 14, 14, 
	14, 14, 14, 14, 14, 17, 18, 14, 
	14, 14, 4, 19, 19, 19, 4, 18, 
	18, 18, 4, 20, 20, 20, 4, 14, 
	14, 14, 4, 22, 22, 23, 21, 23, 
	21, 24, 21, 27, 26, 27, 28, 26, 
	30, 30, 31, 29, 31, 29, 32, 29, 
	34, 34, 35, 33, 35, 36, 37, 33, 
	38, 36, 39, 39, 39, 36, 40, 40, 
	40, 4, 42, 43, 44, 43, 45, 14, 
	43, 46, 47, 48, 49, 50, 51, 52, 
	53, 54, 55, 56, 43, 57, 58, 42, 
	37, 57, 57, 41, 42, 42, 59, 60, 
	36, 10, 61, 60, 60, 62, 60, 60, 
	62, 60, 60, 62, 60, 24, 63, 66, 
	67, 66, 24, 65, 65, 64, 65, 65, 
	22, 65, 65, 23, 64, 26, 68, 60, 
	62, 69, 68, 71, 72, 73, 74, 74, 
	75, 76, 72, 73, 74, 74, 75, 37, 
	70, 79, 79, 32, 78, 78, 77, 79, 
	80, 79, 32, 78, 78, 77, 78, 78, 
	30, 78, 78, 31, 77, 71, 73, 74, 
	74, 76, 73, 74, 74, 37, 70, 82, 
	82, 34, 82, 82, 35, 81, 84, 84, 
	72, 84, 84, 38, 83, 86, 86, 75, 
	86, 86, 39, 39, 39, 85, 60, 62, 
	87, 60, 62, 60, 62, 60, 87, 62, 
	60, 60, 62, 40, 40, 40, 40, 40, 
	88, 57, 57, 57, 57, 89, 60, 60, 
	62, 0
};

static const char _KTT_trans_targs[] = {
	2, 34, 3, 31, 0, 4, 6, 5, 
	7, 31, 8, 9, 10, 31, 11, 31, 
	12, 13, 15, 14, 16, 31, 18, 40, 
	39, 31, 20, 21, 31, 31, 23, 46, 
	45, 31, 26, 48, 31, 47, 49, 50, 
	56, 31, 32, 33, 1, 35, 36, 37, 
	38, 41, 43, 51, 52, 53, 54, 55, 
	30, 57, 58, 31, 31, 31, 31, 31, 
	31, 31, 17, 19, 42, 31, 31, 44, 
	28, 25, 31, 29, 27, 31, 31, 22, 
	24, 31, 31, 31, 31, 31, 31, 33, 
	31, 31
};

static const char _KTT_trans_actions[] = {
	0, 5, 0, 11, 0, 0, 0, 0, 
	0, 57, 0, 0, 0, 9, 0, 13, 
	0, 0, 0, 0, 0, 61, 0, 5, 
	5, 65, 0, 0, 7, 59, 0, 5, 
	5, 63, 0, 75, 67, 78, 72, 69, 
	0, 29, 0, 84, 0, 0, 0, 0, 
	0, 5, 78, 0, 0, 0, 0, 0, 
	0, 0, 0, 55, 27, 33, 51, 53, 
	41, 21, 0, 0, 0, 31, 45, 5, 
	0, 0, 25, 0, 0, 39, 19, 0, 
	0, 43, 23, 37, 17, 35, 15, 81, 
	47, 49
};

static const char _KTT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 1, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const char _KTT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 3, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const short _KTT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	10, 10, 10, 0, 0, 0, 0, 0, 
	0, 22, 22, 22, 26, 26, 30, 30, 
	30, 34, 37, 34, 37, 37, 0, 0, 
	60, 37, 62, 63, 63, 63, 64, 65, 
	65, 63, 70, 71, 78, 78, 78, 71, 
	82, 84, 86, 63, 63, 63, 63, 63, 
	89, 90, 63
};

static const int KTT_start = 31;
static const int KTT_first_final = 31;
static const int KTT_error = 0;

static const int KTT_en_main = 31;


/* #line 133 "KTT.c.rl" */

ok64 KTTLexer(KTTstate* state) {

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

    
/* #line 238 "KTT.rl.c" */
	{
	cs = KTT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 151 "KTT.c.rl" */
    
/* #line 244 "KTT.rl.c" */
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
	_acts = _KTT_actions + _KTT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 263 "KTT.rl.c" */
		}
	}

	_keys = _KTT_trans_keys + _KTT_key_offsets[cs];
	_trans = _KTT_index_offsets[cs];

	_klen = _KTT_single_lengths[cs];
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

	_klen = _KTT_range_lengths[cs];
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
	_trans = _KTT_indicies[_trans];
_eof_trans:
	cs = _KTT_trans_targs[_trans];

	if ( _KTT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _KTT_actions + _KTT_trans_actions[_trans];
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
/* #line 43 "KTT.c.rl" */
	{act = 6;}
	break;
	case 4:
/* #line 43 "KTT.c.rl" */
	{act = 7;}
	break;
	case 5:
/* #line 43 "KTT.c.rl" */
	{act = 10;}
	break;
	case 6:
/* #line 43 "KTT.c.rl" */
	{act = 11;}
	break;
	case 7:
/* #line 61 "KTT.c.rl" */
	{act = 14;}
	break;
	case 8:
/* #line 61 "KTT.c.rl" */
	{act = 15;}
	break;
	case 9:
/* #line 31 "KTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 37 "KTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 37 "KTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 37 "KTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 43 "KTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 43 "KTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 43 "KTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 43 "KTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 43 "KTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 43 "KTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 61 "KTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 61 "KTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 31 "KTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 37 "KTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 43 "KTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 43 "KTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 43 "KTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 43 "KTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 43 "KTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 43 "KTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 49 "KTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonAnnotation(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 55 "KTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 61 "KTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 61 "KTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 67 "KTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 37 "KTT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 43 "KTT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 43 "KTT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 43 "KTT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 61 "KTT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 39:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 6:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 7:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 10:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 11:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 14:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 15:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 642 "KTT.rl.c" */
		}
	}

_again:
	_acts = _KTT_actions + _KTT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 653 "KTT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _KTT_eof_trans[cs] > 0 ) {
		_trans = _KTT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 152 "KTT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < KTT_first_final)
        o = KTTBAD;

    return o;
}
