
/* #line 1 "SWFT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "SWFT.h"

ok64 SWFTonComment (u8cs tok, SWFTstate* state);
ok64 SWFTonString (u8cs tok, SWFTstate* state);
ok64 SWFTonNumber (u8cs tok, SWFTstate* state);
ok64 SWFTonWord (u8cs tok, SWFTstate* state);
ok64 SWFTonPunct (u8cs tok, SWFTstate* state);
ok64 SWFTonSpace (u8cs tok, SWFTstate* state);


/* #line 120 "SWFT.c.rl" */



/* #line 15 "SWFT.rl.c" */
static const char _SWFT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	10, 1, 11, 1, 12, 1, 13, 1, 
	14, 1, 15, 1, 16, 1, 17, 1, 
	18, 1, 19, 1, 20, 1, 21, 1, 
	22, 1, 23, 1, 24, 1, 25, 1, 
	26, 1, 27, 1, 28, 1, 29, 1, 
	30, 1, 31, 1, 32, 1, 33, 1, 
	34, 1, 35, 1, 36, 2, 2, 3, 
	2, 2, 4, 2, 2, 5, 2, 2, 
	6, 2, 2, 7, 2, 2, 8, 2, 
	2, 9
};

static const unsigned char _SWFT_key_offsets[] = {
	0, 0, 2, 4, 20, 21, 27, 34, 
	41, 48, 55, 62, 69, 76, 77, 83, 
	89, 90, 91, 92, 93, 94, 98, 100, 
	102, 103, 105, 109, 111, 113, 117, 119, 
	121, 123, 125, 131, 136, 143, 170, 173, 
	174, 175, 176, 178, 180, 183, 184, 189, 
	192, 195, 196, 208, 212, 217, 220, 226, 
	229, 232, 235, 242, 244, 246, 248, 255
};

static const unsigned char _SWFT_trans_keys[] = {
	34u, 92u, 34u, 92u, 10u, 34u, 39u, 48u, 
	63u, 92u, 110u, 114u, 117u, 120u, 97u, 98u, 
	101u, 102u, 116u, 118u, 123u, 48u, 57u, 65u, 
	70u, 97u, 102u, 125u, 48u, 57u, 65u, 70u, 
	97u, 102u, 125u, 48u, 57u, 65u, 70u, 97u, 
	102u, 125u, 48u, 57u, 65u, 70u, 97u, 102u, 
	125u, 48u, 57u, 65u, 70u, 97u, 102u, 125u, 
	48u, 57u, 65u, 70u, 97u, 102u, 125u, 48u, 
	57u, 65u, 70u, 97u, 102u, 125u, 48u, 57u, 
	65u, 70u, 97u, 102u, 125u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 34u, 34u, 34u, 34u, 35u, 43u, 45u, 
	48u, 57u, 48u, 57u, 48u, 57u, 42u, 42u, 
	47u, 43u, 45u, 48u, 57u, 48u, 57u, 48u, 
	57u, 43u, 45u, 48u, 57u, 48u, 57u, 48u, 
	57u, 48u, 49u, 48u, 55u, 48u, 57u, 65u, 
	70u, 97u, 102u, 95u, 65u, 90u, 97u, 122u, 
	96u, 48u, 57u, 65u, 90u, 95u, 122u, 32u, 
	33u, 34u, 35u, 37u, 38u, 45u, 46u, 47u, 
	48u, 60u, 61u, 62u, 63u, 94u, 96u, 124u, 
	9u, 13u, 42u, 43u, 49u, 57u, 65u, 90u, 
	95u, 122u, 32u, 9u, 13u, 61u, 34u, 34u, 
	38u, 61u, 61u, 62u, 46u, 48u, 57u, 46u, 
	69u, 95u, 101u, 48u, 57u, 95u, 48u, 57u, 
	42u, 47u, 61u, 10u, 46u, 66u, 69u, 79u, 
	88u, 95u, 98u, 101u, 111u, 120u, 48u, 57u, 
	69u, 101u, 48u, 57u, 69u, 95u, 101u, 48u, 
	57u, 95u, 48u, 57u, 46u, 69u, 95u, 101u, 
	48u, 57u, 95u, 48u, 57u, 95u, 48u, 49u, 
	95u, 48u, 55u, 95u, 48u, 57u, 65u, 70u, 
	97u, 102u, 60u, 61u, 61u, 62u, 46u, 63u, 
	95u, 48u, 57u, 65u, 90u, 97u, 122u, 61u, 
	124u, 0
};

static const char _SWFT_single_lengths[] = {
	0, 2, 2, 10, 1, 0, 1, 1, 
	1, 1, 1, 1, 1, 1, 0, 0, 
	1, 1, 1, 1, 1, 2, 0, 0, 
	1, 2, 2, 0, 0, 2, 0, 0, 
	0, 0, 0, 1, 1, 17, 1, 1, 
	1, 1, 2, 0, 1, 1, 3, 1, 
	3, 1, 10, 2, 3, 1, 4, 1, 
	1, 1, 1, 2, 2, 2, 1, 2
};

static const char _SWFT_range_lengths[] = {
	0, 0, 0, 3, 0, 3, 3, 3, 
	3, 3, 3, 3, 3, 0, 3, 3, 
	0, 0, 0, 0, 0, 1, 1, 1, 
	0, 0, 1, 1, 1, 1, 1, 1, 
	1, 1, 3, 2, 3, 5, 1, 0, 
	0, 0, 0, 1, 1, 0, 1, 1, 
	0, 0, 1, 1, 1, 1, 1, 1, 
	1, 1, 3, 0, 0, 0, 3, 0
};

static const short _SWFT_index_offsets[] = {
	0, 0, 3, 6, 20, 22, 26, 31, 
	36, 41, 46, 51, 56, 61, 63, 67, 
	71, 73, 75, 77, 79, 81, 85, 87, 
	89, 91, 94, 98, 100, 102, 106, 108, 
	110, 112, 114, 118, 122, 127, 150, 153, 
	155, 157, 159, 162, 164, 167, 169, 174, 
	177, 181, 183, 195, 199, 204, 207, 213, 
	216, 219, 222, 227, 230, 233, 236, 241
};

static const char _SWFT_indicies[] = {
	1, 2, 0, 3, 2, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 5, 6, 
	0, 0, 0, 4, 7, 4, 8, 8, 
	8, 4, 0, 9, 9, 9, 4, 0, 
	10, 10, 10, 4, 0, 11, 11, 11, 
	4, 0, 12, 12, 12, 4, 0, 13, 
	13, 13, 4, 0, 14, 14, 14, 4, 
	0, 15, 15, 15, 4, 0, 4, 16, 
	16, 16, 4, 0, 0, 0, 4, 19, 
	18, 20, 18, 21, 18, 24, 23, 25, 
	23, 27, 27, 28, 26, 28, 26, 29, 
	26, 31, 30, 31, 32, 30, 34, 34, 
	35, 33, 35, 33, 36, 33, 38, 38, 
	39, 37, 39, 40, 41, 37, 42, 40, 
	43, 40, 44, 44, 44, 40, 45, 45, 
	45, 4, 46, 45, 45, 45, 4, 48, 
	49, 50, 51, 49, 52, 53, 54, 55, 
	56, 57, 49, 58, 59, 49, 61, 62, 
	48, 49, 41, 60, 60, 47, 48, 48, 
	63, 64, 40, 18, 65, 23, 66, 64, 
	64, 66, 64, 66, 68, 29, 67, 64, 
	69, 71, 72, 71, 29, 70, 27, 28, 
	70, 30, 73, 64, 66, 74, 73, 76, 
	77, 78, 79, 80, 81, 77, 78, 79, 
	80, 41, 75, 83, 83, 36, 82, 83, 
	84, 83, 36, 82, 34, 35, 82, 76, 
	78, 81, 78, 41, 75, 38, 39, 85, 
	77, 42, 86, 79, 43, 87, 80, 44, 
	44, 44, 88, 89, 64, 66, 64, 89, 
	66, 64, 64, 66, 60, 60, 60, 60, 
	90, 64, 64, 66, 0
};

static const char _SWFT_trans_targs[] = {
	2, 40, 3, 37, 0, 4, 14, 5, 
	6, 7, 8, 9, 10, 11, 12, 13, 
	15, 37, 16, 17, 18, 37, 37, 19, 
	20, 37, 37, 22, 47, 46, 24, 25, 
	37, 37, 27, 53, 52, 37, 30, 55, 
	37, 54, 56, 57, 58, 36, 37, 37, 
	38, 39, 1, 41, 42, 43, 44, 48, 
	50, 59, 60, 61, 62, 35, 63, 37, 
	37, 37, 37, 37, 45, 37, 37, 21, 
	23, 49, 37, 37, 51, 32, 29, 33, 
	34, 31, 37, 26, 28, 37, 37, 37, 
	37, 39, 37
};

static const char _SWFT_trans_actions[] = {
	0, 5, 0, 13, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 49, 0, 0, 0, 11, 57, 0, 
	0, 9, 53, 0, 5, 5, 0, 0, 
	7, 51, 0, 5, 5, 55, 0, 70, 
	59, 73, 67, 64, 61, 0, 15, 19, 
	0, 79, 0, 5, 0, 0, 0, 5, 
	73, 0, 0, 0, 0, 0, 0, 47, 
	17, 23, 43, 45, 0, 41, 33, 0, 
	0, 0, 21, 37, 5, 0, 0, 0, 
	0, 0, 31, 0, 0, 35, 29, 27, 
	25, 76, 39
};

static const char _SWFT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 1, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0
};

static const char _SWFT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 3, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0
};

static const short _SWFT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	18, 18, 18, 23, 23, 27, 27, 27, 
	23, 23, 34, 34, 34, 38, 41, 38, 
	41, 41, 41, 0, 0, 0, 64, 41, 
	66, 67, 67, 67, 68, 70, 71, 71, 
	67, 75, 76, 83, 83, 83, 76, 86, 
	87, 88, 89, 67, 67, 67, 91, 67
};

static const int SWFT_start = 37;
static const int SWFT_first_final = 37;
static const int SWFT_error = 0;

static const int SWFT_en_main = 37;


/* #line 123 "SWFT.c.rl" */

ok64 SWFTLexer(SWFTstate* state) {

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

    
/* #line 229 "SWFT.rl.c" */
	{
	cs = SWFT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 141 "SWFT.c.rl" */
    
/* #line 235 "SWFT.rl.c" */
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
	_acts = _SWFT_actions + _SWFT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 254 "SWFT.rl.c" */
		}
	}

	_keys = _SWFT_trans_keys + _SWFT_key_offsets[cs];
	_trans = _SWFT_index_offsets[cs];

	_klen = _SWFT_single_lengths[cs];
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

	_klen = _SWFT_range_lengths[cs];
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
	_trans = _SWFT_indicies[_trans];
_eof_trans:
	cs = _SWFT_trans_targs[_trans];

	if ( _SWFT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _SWFT_actions + _SWFT_trans_actions[_trans];
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
/* #line 43 "SWFT.c.rl" */
	{act = 6;}
	break;
	case 4:
/* #line 43 "SWFT.c.rl" */
	{act = 7;}
	break;
	case 5:
/* #line 43 "SWFT.c.rl" */
	{act = 8;}
	break;
	case 6:
/* #line 43 "SWFT.c.rl" */
	{act = 11;}
	break;
	case 7:
/* #line 43 "SWFT.c.rl" */
	{act = 12;}
	break;
	case 8:
/* #line 55 "SWFT.c.rl" */
	{act = 15;}
	break;
	case 9:
/* #line 55 "SWFT.c.rl" */
	{act = 16;}
	break;
	case 10:
/* #line 31 "SWFT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 37 "SWFT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 37 "SWFT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 37 "SWFT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 49 "SWFT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 55 "SWFT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 55 "SWFT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 31 "SWFT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 37 "SWFT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 43 "SWFT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 43 "SWFT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 43 "SWFT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 43 "SWFT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 43 "SWFT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 43 "SWFT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 43 "SWFT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 49 "SWFT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 55 "SWFT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 55 "SWFT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 55 "SWFT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 61 "SWFT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 37 "SWFT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 43 "SWFT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 43 "SWFT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 43 "SWFT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 55 "SWFT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 6:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 7:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 8:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 11:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 15:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 16:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 612 "SWFT.rl.c" */
		}
	}

_again:
	_acts = _SWFT_actions + _SWFT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 623 "SWFT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _SWFT_eof_trans[cs] > 0 ) {
		_trans = _SWFT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 142 "SWFT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < SWFT_first_final)
        o = SWFTBAD;

    return o;
}
