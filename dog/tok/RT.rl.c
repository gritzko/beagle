
/* #line 1 "RT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "RT.h"

ok64 RTonComment (u8cs tok, RTstate* state);
ok64 RTonString (u8cs tok, RTstate* state);
ok64 RTonNumber (u8cs tok, RTstate* state);
ok64 RTonWord (u8cs tok, RTstate* state);
ok64 RTonPunct (u8cs tok, RTstate* state);
ok64 RTonSpace (u8cs tok, RTstate* state);


/* #line 110 "RT.c.rl" */



/* #line 15 "RT.rl.c" */
static const char _RT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19, 1, 20, 1, 21, 1, 22, 1, 
	23, 1, 24, 1, 25, 1, 26, 1, 
	27, 1, 28, 1, 29, 1, 30, 1, 
	31
};

static const short _RT_key_offsets[] = {
	0, 0, 2, 18, 24, 30, 36, 42, 
	48, 54, 60, 66, 67, 68, 70, 86, 
	92, 98, 104, 110, 116, 122, 128, 134, 
	135, 139, 141, 145, 147, 151, 153, 159, 
	160, 161, 162, 189, 192, 193, 194, 200, 
	201, 202, 203, 211, 217, 221, 229, 238, 
	244, 248, 255, 259, 267, 268, 269, 272, 
	280, 290
};

static const unsigned char _RT_trans_keys[] = {
	34u, 92u, 10u, 34u, 39u, 48u, 63u, 85u, 
	92u, 102u, 110u, 114u, 117u, 120u, 97u, 98u, 
	116u, 118u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 37u, 110u, 39u, 92u, 10u, 34u, 
	39u, 48u, 63u, 85u, 92u, 102u, 110u, 114u, 
	117u, 120u, 97u, 98u, 116u, 118u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 46u, 43u, 
	45u, 48u, 57u, 48u, 57u, 43u, 45u, 48u, 
	57u, 48u, 57u, 43u, 45u, 48u, 57u, 48u, 
	57u, 48u, 57u, 65u, 70u, 97u, 102u, 45u, 
	34u, 39u, 32u, 33u, 34u, 35u, 37u, 38u, 
	39u, 45u, 46u, 48u, 58u, 60u, 82u, 95u, 
	114u, 124u, 126u, 9u, 13u, 49u, 57u, 61u, 
	62u, 65u, 90u, 97u, 122u, 32u, 9u, 13u, 
	61u, 10u, 37u, 42u, 47u, 105u, 111u, 120u, 
	38u, 62u, 62u, 46u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 69u, 76u, 101u, 105u, 48u, 
	57u, 76u, 105u, 48u, 57u, 46u, 95u, 48u, 
	57u, 65u, 90u, 97u, 122u, 46u, 69u, 76u, 
	88u, 101u, 105u, 120u, 48u, 57u, 69u, 76u, 
	101u, 105u, 48u, 57u, 76u, 105u, 48u, 57u, 
	46u, 69u, 76u, 101u, 105u, 48u, 57u, 76u, 
	105u, 48u, 57u, 76u, 105u, 48u, 57u, 65u, 
	70u, 97u, 102u, 58u, 58u, 45u, 60u, 61u, 
	46u, 95u, 48u, 57u, 65u, 90u, 97u, 122u, 
	34u, 39u, 46u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 62u, 124u, 0
};

static const char _RT_single_lengths[] = {
	0, 2, 12, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 1, 2, 12, 0, 
	0, 0, 0, 0, 0, 0, 0, 1, 
	2, 0, 2, 0, 2, 0, 0, 1, 
	1, 1, 17, 1, 1, 1, 6, 1, 
	1, 1, 2, 4, 2, 2, 7, 4, 
	2, 5, 2, 2, 1, 1, 3, 2, 
	4, 2
};

static const char _RT_range_lengths[] = {
	0, 0, 2, 3, 3, 3, 3, 3, 
	3, 3, 3, 0, 0, 0, 2, 3, 
	3, 3, 3, 3, 3, 3, 3, 0, 
	1, 1, 1, 1, 1, 1, 3, 0, 
	0, 0, 5, 1, 0, 0, 0, 0, 
	0, 0, 3, 1, 1, 3, 1, 1, 
	1, 1, 1, 3, 0, 0, 0, 3, 
	3, 0
};

static const short _RT_index_offsets[] = {
	0, 0, 3, 18, 22, 26, 30, 34, 
	38, 42, 46, 50, 52, 54, 57, 72, 
	76, 80, 84, 88, 92, 96, 100, 104, 
	106, 110, 112, 116, 118, 122, 124, 128, 
	130, 132, 134, 157, 160, 162, 164, 171, 
	173, 175, 177, 183, 189, 193, 199, 208, 
	214, 218, 225, 229, 235, 237, 239, 243, 
	249, 257
};

static const char _RT_indicies[] = {
	1, 2, 0, 0, 0, 0, 0, 0, 
	4, 0, 0, 0, 0, 5, 6, 0, 
	0, 3, 7, 7, 7, 3, 8, 8, 
	8, 3, 9, 9, 9, 3, 5, 5, 
	5, 3, 10, 10, 10, 3, 6, 6, 
	6, 3, 11, 11, 11, 3, 0, 0, 
	0, 3, 13, 12, 14, 12, 16, 17, 
	15, 15, 15, 15, 15, 15, 18, 15, 
	15, 15, 15, 19, 20, 15, 15, 3, 
	21, 21, 21, 3, 22, 22, 22, 3, 
	23, 23, 23, 3, 19, 19, 19, 3, 
	24, 24, 24, 3, 20, 20, 20, 3, 
	25, 25, 25, 3, 15, 15, 15, 3, 
	13, 26, 28, 28, 29, 27, 29, 27, 
	31, 31, 32, 30, 32, 30, 34, 34, 
	35, 33, 35, 33, 36, 36, 36, 33, 
	13, 12, 39, 38, 41, 40, 43, 44, 
	0, 45, 46, 47, 15, 48, 49, 50, 
	52, 53, 55, 3, 55, 56, 13, 43, 
	51, 44, 54, 54, 42, 43, 43, 57, 
	13, 58, 59, 45, 13, 14, 14, 60, 
	14, 14, 58, 13, 58, 61, 58, 13, 
	62, 64, 66, 65, 66, 66, 63, 68, 
	69, 68, 69, 65, 67, 69, 69, 29, 
	67, 66, 66, 66, 66, 66, 70, 72, 
	73, 74, 75, 73, 74, 75, 51, 71, 
	77, 78, 77, 78, 72, 76, 78, 78, 
	32, 76, 72, 73, 74, 73, 74, 51, 
	71, 80, 80, 35, 79, 82, 82, 36, 
	36, 36, 81, 83, 58, 13, 62, 13, 
	84, 13, 58, 54, 54, 54, 54, 54, 
	85, 38, 40, 54, 54, 54, 54, 54, 
	85, 13, 13, 58, 0
};

static const char _RT_trans_targs[] = {
	1, 34, 2, 0, 3, 7, 9, 4, 
	5, 6, 8, 10, 34, 34, 11, 13, 
	34, 14, 15, 19, 21, 16, 17, 18, 
	20, 22, 34, 34, 25, 44, 34, 27, 
	48, 34, 29, 50, 51, 34, 32, 34, 
	33, 34, 34, 35, 36, 37, 38, 39, 
	40, 42, 46, 49, 52, 54, 55, 56, 
	57, 34, 34, 34, 12, 41, 34, 34, 
	23, 43, 45, 34, 24, 34, 34, 34, 
	47, 28, 34, 30, 34, 26, 34, 34, 
	34, 34, 34, 53, 31, 34
};

static const char _RT_trans_actions[] = {
	0, 11, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 61, 25, 0, 0, 
	13, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 63, 55, 0, 0, 53, 0, 
	0, 57, 0, 0, 0, 59, 0, 7, 
	0, 9, 27, 0, 0, 0, 5, 0, 
	0, 5, 5, 5, 0, 5, 0, 5, 
	0, 51, 47, 29, 0, 0, 45, 49, 
	0, 5, 0, 35, 0, 19, 43, 39, 
	5, 0, 23, 0, 33, 0, 17, 37, 
	21, 31, 15, 0, 0, 41
};

static const char _RT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 1, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const char _RT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 3, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const short _RT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 13, 13, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 27, 
	28, 28, 31, 31, 34, 34, 34, 13, 
	38, 38, 0, 58, 59, 60, 59, 59, 
	59, 63, 64, 68, 68, 71, 72, 77, 
	77, 72, 80, 82, 59, 63, 59, 86, 
	86, 59
};

static const int RT_start = 34;
static const int RT_first_final = 34;
static const int RT_error = 0;

static const int RT_en_main = 34;


/* #line 113 "RT.c.rl" */

ok64 RTLexer(RTstate* state) {

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

    
/* #line 231 "RT.rl.c" */
	{
	cs = RT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 131 "RT.c.rl" */
    
/* #line 237 "RT.rl.c" */
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
	_acts = _RT_actions + _RT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 256 "RT.rl.c" */
		}
	}

	_keys = _RT_trans_keys + _RT_key_offsets[cs];
	_trans = _RT_index_offsets[cs];

	_klen = _RT_single_lengths[cs];
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

	_klen = _RT_range_lengths[cs];
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
	_trans = _RT_indicies[_trans];
_eof_trans:
	cs = _RT_trans_targs[_trans];

	if ( _RT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _RT_actions + _RT_trans_actions[_trans];
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
/* #line 36 "RT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 4:
/* #line 36 "RT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 5:
/* #line 36 "RT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 36 "RT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 42 "RT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 42 "RT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 42 "RT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 42 "RT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 42 "RT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 54 "RT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 54 "RT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 30 "RT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 42 "RT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 42 "RT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 42 "RT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 42 "RT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 42 "RT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 48 "RT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 48 "RT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 54 "RT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 54 "RT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 54 "RT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 60 "RT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 42 "RT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 42 "RT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 42 "RT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 48 "RT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 54 "RT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 54 "RT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
/* #line 556 "RT.rl.c" */
		}
	}

_again:
	_acts = _RT_actions + _RT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 567 "RT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _RT_eof_trans[cs] > 0 ) {
		_trans = _RT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 132 "RT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < RT_first_final)
        o = RTBAD;

    return o;
}
