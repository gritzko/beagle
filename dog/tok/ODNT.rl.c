
/* #line 1 "ODNT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "ODNT.h"

ok64 ODNTonComment (u8cs tok, ODNTstate* state);
ok64 ODNTonString (u8cs tok, ODNTstate* state);
ok64 ODNTonNumber (u8cs tok, ODNTstate* state);
ok64 ODNTonWord (u8cs tok, ODNTstate* state);
ok64 ODNTonPunct (u8cs tok, ODNTstate* state);
ok64 ODNTonSpace (u8cs tok, ODNTstate* state);


/* #line 116 "ODNT.c.rl" */



/* #line 15 "ODNT.rl.c" */
static const char _ODNT_actions[] = {
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

static const short _ODNT_key_offsets[] = {
	0, 0, 2, 17, 23, 29, 35, 41, 
	47, 53, 59, 65, 67, 68, 83, 89, 
	95, 101, 107, 113, 119, 125, 131, 132, 
	136, 138, 140, 141, 143, 147, 149, 151, 
	155, 157, 159, 161, 163, 169, 170, 198, 
	201, 202, 204, 206, 209, 211, 216, 219, 
	222, 223, 235, 239, 244, 247, 253, 256, 
	259, 262, 269, 272, 274, 281
};

static const unsigned char _ODNT_trans_keys[] = {
	34u, 92u, 34u, 39u, 48u, 85u, 92u, 110u, 
	114u, 117u, 120u, 97u, 98u, 101u, 102u, 116u, 
	118u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 39u, 92u, 39u, 34u, 39u, 48u, 85u, 
	92u, 110u, 114u, 117u, 120u, 97u, 98u, 101u, 
	102u, 116u, 118u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 32u, 43u, 45u, 48u, 57u, 
	48u, 57u, 48u, 57u, 42u, 42u, 47u, 43u, 
	45u, 48u, 57u, 48u, 57u, 48u, 57u, 43u, 
	45u, 48u, 57u, 48u, 57u, 48u, 57u, 48u, 
	49u, 48u, 55u, 48u, 57u, 65u, 70u, 97u, 
	102u, 96u, 32u, 33u, 34u, 37u, 38u, 39u, 
	45u, 46u, 47u, 48u, 58u, 60u, 61u, 62u, 
	94u, 96u, 124u, 126u, 9u, 13u, 42u, 43u, 
	49u, 57u, 65u, 90u, 95u, 122u, 32u, 9u, 
	13u, 61u, 38u, 61u, 61u, 62u, 46u, 48u, 
	57u, 60u, 61u, 69u, 95u, 101u, 48u, 57u, 
	95u, 48u, 57u, 42u, 47u, 61u, 10u, 46u, 
	66u, 69u, 79u, 88u, 95u, 98u, 101u, 111u, 
	120u, 48u, 57u, 69u, 101u, 48u, 57u, 69u, 
	95u, 101u, 48u, 57u, 95u, 48u, 57u, 46u, 
	69u, 95u, 101u, 48u, 57u, 95u, 48u, 57u, 
	95u, 48u, 49u, 95u, 48u, 55u, 95u, 48u, 
	57u, 65u, 70u, 97u, 102u, 45u, 60u, 61u, 
	61u, 62u, 95u, 48u, 57u, 65u, 90u, 97u, 
	122u, 61u, 124u, 0
};

static const char _ODNT_single_lengths[] = {
	0, 2, 9, 0, 0, 0, 0, 0, 
	0, 0, 0, 2, 1, 9, 0, 0, 
	0, 0, 0, 0, 0, 0, 1, 2, 
	0, 0, 1, 2, 2, 0, 0, 2, 
	0, 0, 0, 0, 0, 1, 18, 1, 
	1, 2, 0, 1, 2, 3, 1, 3, 
	1, 10, 2, 3, 1, 4, 1, 1, 
	1, 1, 3, 2, 1, 2
};

static const char _ODNT_range_lengths[] = {
	0, 0, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 0, 0, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 0, 1, 
	1, 1, 0, 0, 1, 1, 1, 1, 
	1, 1, 1, 1, 3, 0, 5, 1, 
	0, 0, 1, 1, 0, 1, 1, 0, 
	0, 1, 1, 1, 1, 1, 1, 1, 
	1, 3, 0, 0, 3, 0
};

static const short _ODNT_index_offsets[] = {
	0, 0, 3, 16, 20, 24, 28, 32, 
	36, 40, 44, 48, 51, 53, 66, 70, 
	74, 78, 82, 86, 90, 94, 98, 100, 
	104, 106, 108, 110, 113, 117, 119, 121, 
	125, 127, 129, 131, 133, 137, 139, 163, 
	166, 168, 171, 173, 176, 179, 184, 187, 
	191, 193, 205, 209, 214, 217, 223, 226, 
	229, 232, 237, 241, 244, 249
};

static const char _ODNT_indicies[] = {
	1, 2, 0, 0, 0, 0, 4, 0, 
	0, 0, 5, 6, 0, 0, 0, 3, 
	7, 7, 7, 3, 8, 8, 8, 3, 
	9, 9, 9, 3, 5, 5, 5, 3, 
	10, 10, 10, 3, 6, 6, 6, 3, 
	11, 11, 11, 3, 0, 0, 0, 3, 
	3, 13, 12, 14, 3, 12, 12, 12, 
	15, 12, 12, 12, 16, 17, 12, 12, 
	12, 3, 18, 18, 18, 3, 19, 19, 
	19, 3, 20, 20, 20, 3, 16, 16, 
	16, 3, 21, 21, 21, 3, 17, 17, 
	17, 3, 22, 22, 22, 3, 12, 12, 
	12, 3, 24, 23, 26, 26, 27, 25, 
	27, 25, 28, 25, 31, 30, 31, 32, 
	30, 34, 34, 35, 33, 35, 33, 36, 
	33, 38, 38, 39, 37, 39, 40, 41, 
	37, 42, 40, 43, 40, 44, 44, 44, 
	40, 46, 45, 48, 49, 0, 49, 50, 
	51, 52, 53, 54, 55, 49, 56, 49, 
	57, 49, 45, 59, 49, 48, 49, 41, 
	58, 58, 47, 48, 48, 60, 24, 40, 
	24, 24, 61, 24, 61, 63, 28, 62, 
	65, 24, 64, 67, 68, 67, 28, 66, 
	26, 27, 66, 30, 69, 24, 61, 70, 
	69, 72, 73, 74, 75, 76, 77, 73, 
	74, 75, 76, 41, 71, 79, 79, 36, 
	78, 79, 80, 79, 36, 78, 34, 35, 
	78, 72, 74, 77, 74, 41, 71, 38, 
	39, 81, 73, 42, 82, 75, 43, 83, 
	76, 44, 44, 44, 84, 24, 85, 24, 
	61, 24, 85, 61, 58, 58, 58, 58, 
	86, 24, 24, 61, 0
};

static const char _ODNT_trans_targs[] = {
	1, 38, 2, 0, 3, 7, 9, 4, 
	5, 6, 8, 10, 12, 13, 38, 14, 
	18, 20, 15, 16, 17, 19, 21, 38, 
	38, 38, 24, 46, 45, 38, 26, 27, 
	38, 38, 29, 52, 51, 38, 32, 54, 
	38, 53, 55, 56, 57, 37, 38, 38, 
	39, 40, 41, 11, 42, 43, 47, 49, 
	58, 59, 60, 61, 38, 38, 38, 44, 
	38, 22, 38, 23, 25, 48, 38, 38, 
	50, 34, 31, 35, 36, 33, 38, 28, 
	30, 38, 38, 38, 38, 40, 38
};

static const char _ODNT_trans_actions[] = {
	0, 9, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 13, 0, 
	0, 0, 0, 0, 0, 0, 0, 51, 
	15, 47, 0, 5, 5, 53, 0, 0, 
	7, 45, 0, 5, 5, 49, 0, 66, 
	55, 69, 63, 60, 57, 0, 11, 17, 
	0, 75, 0, 0, 0, 0, 5, 69, 
	0, 0, 0, 0, 43, 39, 41, 5, 
	37, 0, 29, 0, 0, 0, 19, 33, 
	5, 0, 0, 0, 0, 0, 27, 0, 
	0, 31, 25, 23, 21, 72, 35
};

static const char _ODNT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 1, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0
};

static const char _ODNT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 3, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0
};

static const short _ODNT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 24, 26, 
	26, 26, 30, 30, 34, 34, 34, 38, 
	41, 38, 41, 41, 41, 0, 0, 61, 
	41, 62, 62, 63, 65, 67, 67, 62, 
	71, 72, 79, 79, 79, 72, 82, 83, 
	84, 85, 62, 62, 87, 62
};

static const int ODNT_start = 38;
static const int ODNT_first_final = 38;
static const int ODNT_error = 0;

static const int ODNT_en_main = 38;


/* #line 119 "ODNT.c.rl" */

ok64 ODNTLexer(ODNTstate* state) {

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

    
/* #line 230 "ODNT.rl.c" */
	{
	cs = ODNT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 137 "ODNT.c.rl" */
    
/* #line 236 "ODNT.rl.c" */
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
	_acts = _ODNT_actions + _ODNT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 255 "ODNT.rl.c" */
		}
	}

	_keys = _ODNT_trans_keys + _ODNT_key_offsets[cs];
	_trans = _ODNT_index_offsets[cs];

	_klen = _ODNT_single_lengths[cs];
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

	_klen = _ODNT_range_lengths[cs];
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
	_trans = _ODNT_indicies[_trans];
_eof_trans:
	cs = _ODNT_trans_targs[_trans];

	if ( _ODNT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _ODNT_actions + _ODNT_trans_actions[_trans];
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
/* #line 43 "ODNT.c.rl" */
	{act = 6;}
	break;
	case 4:
/* #line 43 "ODNT.c.rl" */
	{act = 7;}
	break;
	case 5:
/* #line 43 "ODNT.c.rl" */
	{act = 8;}
	break;
	case 6:
/* #line 43 "ODNT.c.rl" */
	{act = 11;}
	break;
	case 7:
/* #line 43 "ODNT.c.rl" */
	{act = 12;}
	break;
	case 8:
/* #line 55 "ODNT.c.rl" */
	{act = 14;}
	break;
	case 9:
/* #line 55 "ODNT.c.rl" */
	{act = 15;}
	break;
	case 10:
/* #line 31 "ODNT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 37 "ODNT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 37 "ODNT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 37 "ODNT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 55 "ODNT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 55 "ODNT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 31 "ODNT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 43 "ODNT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 43 "ODNT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 43 "ODNT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 43 "ODNT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 43 "ODNT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 43 "ODNT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 43 "ODNT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 49 "ODNT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 55 "ODNT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 55 "ODNT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 55 "ODNT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 61 "ODNT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 43 "ODNT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 43 "ODNT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 43 "ODNT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 55 "ODNT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 55 "ODNT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 6:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 7:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 8:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 11:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 14:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 15:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ODNTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 597 "ODNT.rl.c" */
		}
	}

_again:
	_acts = _ODNT_actions + _ODNT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 608 "ODNT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _ODNT_eof_trans[cs] > 0 ) {
		_trans = _ODNT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 138 "ODNT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < ODNT_first_final)
        o = ODNTBAD;

    return o;
}
