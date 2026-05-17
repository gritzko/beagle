
/* #line 1 "ELXT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "ELXT.h"

ok64 ELXTonComment (u8cs tok, ELXTstate* state);
ok64 ELXTonString (u8cs tok, ELXTstate* state);
ok64 ELXTonNumber (u8cs tok, ELXTstate* state);
ok64 ELXTonDecorator (u8cs tok, ELXTstate* state);
ok64 ELXTonWord (u8cs tok, ELXTstate* state);
ok64 ELXTonPunct (u8cs tok, ELXTstate* state);
ok64 ELXTonSpace (u8cs tok, ELXTstate* state);


/* #line 133 "ELXT.c.rl" */



/* #line 16 "ELXT.rl.c" */
static const char _ELXT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	10, 1, 11, 1, 12, 1, 13, 1, 
	14, 1, 15, 1, 16, 1, 17, 1, 
	18, 1, 19, 1, 20, 1, 21, 1, 
	22, 1, 23, 1, 24, 1, 25, 1, 
	26, 1, 27, 1, 28, 1, 29, 1, 
	30, 1, 31, 1, 32, 1, 33, 1, 
	34, 1, 35, 1, 36, 1, 37, 1, 
	38, 1, 39, 1, 40, 1, 41, 1, 
	42, 2, 2, 3, 2, 2, 4, 2, 
	2, 5, 2, 2, 6, 2, 2, 7, 
	2, 2, 8, 2, 2, 9
};

static const short _ELXT_key_offsets[] = {
	0, 0, 2, 4, 19, 25, 31, 37, 
	43, 44, 45, 46, 48, 50, 50, 51, 
	52, 53, 57, 59, 61, 65, 67, 69, 
	71, 73, 79, 81, 96, 102, 108, 114, 
	120, 125, 135, 137, 139, 139, 141, 141, 
	142, 170, 173, 174, 175, 176, 177, 178, 
	180, 181, 183, 185, 188, 189, 190, 202, 
	206, 211, 214, 220, 223, 226, 229, 236, 
	243, 252, 257, 258, 259, 261, 263, 270, 
	279, 281, 283, 287
};

static const unsigned char _ELXT_trans_keys[] = {
	34u, 92u, 34u, 92u, 10u, 34u, 39u, 48u, 
	63u, 92u, 110u, 117u, 120u, 97u, 98u, 101u, 
	102u, 114u, 118u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 34u, 34u, 34u, 39u, 92u, 
	39u, 92u, 39u, 39u, 39u, 43u, 45u, 48u, 
	57u, 48u, 57u, 48u, 57u, 43u, 45u, 48u, 
	57u, 48u, 57u, 48u, 57u, 48u, 49u, 48u, 
	55u, 48u, 57u, 65u, 70u, 97u, 102u, 34u, 
	92u, 10u, 34u, 39u, 48u, 63u, 92u, 110u, 
	117u, 120u, 97u, 98u, 101u, 102u, 114u, 118u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	95u, 65u, 90u, 97u, 122u, 62u, 67u, 87u, 
	99u, 119u, 126u, 82u, 83u, 114u, 115u, 34u, 
	47u, 34u, 92u, 47u, 92u, 126u, 32u, 33u, 
	34u, 35u, 38u, 39u, 42u, 43u, 45u, 46u, 
	47u, 48u, 58u, 60u, 61u, 62u, 64u, 95u, 
	124u, 126u, 9u, 13u, 49u, 57u, 65u, 90u, 
	97u, 122u, 32u, 9u, 13u, 61u, 61u, 34u, 
	10u, 38u, 38u, 61u, 39u, 42u, 61u, 43u, 
	61u, 45u, 61u, 62u, 46u, 46u, 46u, 66u, 
	69u, 79u, 88u, 95u, 98u, 101u, 111u, 120u, 
	48u, 57u, 69u, 101u, 48u, 57u, 69u, 95u, 
	101u, 48u, 57u, 95u, 48u, 57u, 46u, 69u, 
	95u, 101u, 48u, 57u, 95u, 48u, 57u, 95u, 
	48u, 49u, 95u, 48u, 55u, 95u, 48u, 57u, 
	65u, 70u, 97u, 102u, 34u, 58u, 95u, 65u, 
	90u, 97u, 122u, 33u, 63u, 95u, 48u, 57u, 
	65u, 90u, 97u, 122u, 45u, 60u, 126u, 61u, 
	62u, 60u, 62u, 61u, 62u, 61u, 62u, 95u, 
	48u, 57u, 65u, 90u, 97u, 122u, 33u, 63u, 
	95u, 48u, 57u, 65u, 90u, 97u, 122u, 62u, 
	124u, 61u, 124u, 65u, 90u, 97u, 122u, 65u, 
	90u, 97u, 122u, 0
};

static const char _ELXT_single_lengths[] = {
	0, 2, 2, 9, 0, 0, 0, 0, 
	1, 1, 1, 2, 2, 0, 1, 1, 
	1, 2, 0, 0, 2, 0, 0, 0, 
	0, 0, 2, 9, 0, 0, 0, 0, 
	1, 6, 2, 2, 0, 2, 0, 1, 
	20, 1, 1, 1, 1, 1, 1, 2, 
	1, 2, 2, 1, 1, 1, 10, 2, 
	3, 1, 4, 1, 1, 1, 1, 3, 
	3, 3, 1, 1, 2, 2, 1, 3, 
	2, 2, 0, 0
};

static const char _ELXT_range_lengths[] = {
	0, 0, 0, 3, 3, 3, 3, 3, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 1, 1, 1, 1, 1, 1, 
	1, 3, 0, 3, 3, 3, 3, 3, 
	2, 2, 0, 0, 0, 0, 0, 0, 
	4, 1, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 0, 0, 1, 1, 
	1, 1, 1, 1, 1, 1, 3, 2, 
	3, 1, 0, 0, 0, 0, 3, 3, 
	0, 0, 2, 2
};

static const short _ELXT_index_offsets[] = {
	0, 0, 3, 6, 19, 23, 27, 31, 
	35, 37, 39, 41, 44, 47, 48, 50, 
	52, 54, 58, 60, 62, 66, 68, 70, 
	72, 74, 78, 81, 94, 98, 102, 106, 
	110, 114, 123, 126, 129, 130, 133, 134, 
	136, 161, 164, 166, 168, 170, 172, 174, 
	177, 179, 182, 185, 188, 190, 192, 204, 
	208, 213, 216, 222, 225, 228, 231, 236, 
	242, 249, 254, 256, 258, 261, 264, 269, 
	276, 279, 282, 285
};

static const char _ELXT_indicies[] = {
	1, 2, 0, 3, 2, 0, 0, 0, 
	0, 0, 0, 0, 0, 5, 6, 0, 
	0, 0, 4, 7, 7, 7, 4, 6, 
	6, 6, 4, 8, 8, 8, 4, 0, 
	0, 0, 4, 11, 10, 12, 10, 13, 
	10, 15, 16, 14, 17, 16, 14, 14, 
	20, 19, 21, 19, 22, 19, 24, 24, 
	25, 23, 25, 23, 26, 23, 28, 28, 
	29, 27, 29, 30, 31, 27, 32, 30, 
	33, 30, 34, 34, 34, 30, 37, 38, 
	36, 36, 36, 36, 36, 36, 36, 36, 
	39, 40, 36, 36, 36, 35, 41, 41, 
	41, 35, 40, 40, 40, 35, 42, 42, 
	42, 35, 36, 36, 36, 35, 43, 43, 
	43, 4, 44, 45, 45, 45, 45, 46, 
	45, 45, 4, 47, 48, 4, 49, 50, 
	47, 47, 51, 52, 48, 48, 44, 4, 
	54, 55, 56, 57, 58, 59, 60, 61, 
	62, 63, 64, 65, 66, 67, 68, 69, 
	70, 71, 72, 73, 54, 31, 71, 71, 
	53, 54, 54, 74, 76, 75, 44, 30, 
	10, 77, 78, 57, 79, 75, 44, 44, 
	80, 19, 81, 44, 44, 75, 44, 44, 
	75, 44, 44, 75, 83, 82, 44, 80, 
	85, 86, 87, 88, 89, 90, 86, 87, 
	88, 89, 31, 84, 92, 92, 26, 91, 
	92, 93, 92, 26, 91, 24, 25, 91, 
	85, 87, 90, 87, 31, 84, 28, 29, 
	94, 86, 32, 95, 88, 33, 96, 89, 
	34, 34, 34, 97, 36, 44, 98, 98, 
	98, 82, 100, 100, 98, 98, 98, 98, 
	99, 44, 101, 102, 44, 75, 44, 80, 
	44, 80, 76, 44, 75, 44, 102, 75, 
	43, 43, 43, 43, 103, 105, 105, 71, 
	71, 71, 71, 104, 44, 106, 75, 44, 
	44, 80, 49, 49, 107, 51, 51, 108, 
	0
};

static const char _ELXT_trans_targs[] = {
	2, 44, 3, 40, 0, 4, 6, 5, 
	7, 40, 8, 9, 10, 40, 12, 48, 
	13, 40, 40, 14, 15, 16, 40, 40, 
	18, 57, 56, 40, 21, 59, 40, 58, 
	60, 61, 62, 40, 26, 40, 27, 28, 
	30, 29, 31, 70, 40, 34, 39, 35, 
	37, 74, 36, 75, 38, 40, 41, 42, 
	1, 45, 46, 11, 49, 50, 51, 52, 
	43, 54, 63, 65, 68, 69, 32, 71, 
	72, 33, 40, 40, 43, 40, 40, 47, 
	40, 40, 40, 53, 40, 55, 23, 20, 
	24, 25, 22, 40, 17, 19, 40, 40, 
	40, 40, 64, 40, 40, 66, 67, 40, 
	40, 40, 73, 40, 40
};

static const char _ELXT_trans_actions[] = {
	0, 5, 0, 11, 0, 0, 0, 0, 
	0, 61, 0, 0, 0, 7, 0, 5, 
	0, 13, 63, 0, 0, 0, 9, 65, 
	0, 5, 5, 67, 0, 82, 71, 85, 
	79, 76, 73, 69, 0, 17, 0, 0, 
	0, 0, 0, 0, 21, 0, 0, 0, 
	0, 0, 0, 0, 0, 23, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	91, 85, 5, 0, 0, 0, 0, 0, 
	0, 0, 59, 55, 88, 31, 25, 0, 
	53, 33, 57, 0, 47, 5, 0, 0, 
	0, 0, 0, 43, 0, 0, 45, 41, 
	39, 37, 0, 35, 15, 0, 0, 49, 
	51, 19, 0, 29, 27
};

static const char _ELXT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	1, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0
};

static const char _ELXT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	3, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0
};

static const short _ELXT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	10, 10, 10, 0, 0, 0, 19, 19, 
	19, 24, 24, 24, 28, 31, 28, 31, 
	31, 31, 36, 36, 36, 36, 36, 36, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 75, 76, 31, 78, 79, 76, 81, 
	82, 76, 76, 76, 83, 81, 85, 92, 
	92, 92, 85, 95, 96, 97, 98, 83, 
	100, 76, 81, 81, 76, 76, 104, 105, 
	76, 81, 108, 109
};

static const int ELXT_start = 40;
static const int ELXT_first_final = 40;
static const int ELXT_error = 0;

static const int ELXT_en_main = 40;


/* #line 136 "ELXT.c.rl" */

ok64 ELXTLexer(ELXTstate* state) {

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

    
/* #line 259 "ELXT.rl.c" */
	{
	cs = ELXT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 154 "ELXT.c.rl" */
    
/* #line 265 "ELXT.rl.c" */
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
	_acts = _ELXT_actions + _ELXT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 284 "ELXT.rl.c" */
		}
	}

	_keys = _ELXT_trans_keys + _ELXT_key_offsets[cs];
	_trans = _ELXT_index_offsets[cs];

	_klen = _ELXT_single_lengths[cs];
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

	_klen = _ELXT_range_lengths[cs];
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
	_trans = _ELXT_indicies[_trans];
_eof_trans:
	cs = _ELXT_trans_targs[_trans];

	if ( _ELXT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _ELXT_actions + _ELXT_trans_actions[_trans];
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
/* #line 42 "ELXT.c.rl" */
	{act = 10;}
	break;
	case 4:
/* #line 42 "ELXT.c.rl" */
	{act = 11;}
	break;
	case 5:
/* #line 42 "ELXT.c.rl" */
	{act = 12;}
	break;
	case 6:
/* #line 42 "ELXT.c.rl" */
	{act = 14;}
	break;
	case 7:
/* #line 42 "ELXT.c.rl" */
	{act = 15;}
	break;
	case 8:
/* #line 60 "ELXT.c.rl" */
	{act = 18;}
	break;
	case 9:
/* #line 60 "ELXT.c.rl" */
	{act = 19;}
	break;
	case 10:
/* #line 36 "ELXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 36 "ELXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 36 "ELXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 36 "ELXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 36 "ELXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 36 "ELXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 54 "ELXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 60 "ELXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 60 "ELXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 30 "ELXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 36 "ELXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 36 "ELXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 36 "ELXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 36 "ELXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 36 "ELXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 42 "ELXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 42 "ELXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 42 "ELXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 42 "ELXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 42 "ELXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 42 "ELXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 48 "ELXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonDecorator(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 54 "ELXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 60 "ELXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 60 "ELXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 60 "ELXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 66 "ELXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 36 "ELXT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 36 "ELXT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 39:
/* #line 42 "ELXT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 40:
/* #line 42 "ELXT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 41:
/* #line 60 "ELXT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 42:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 10:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 11:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 14:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 15:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 18:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 19:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 690 "ELXT.rl.c" */
		}
	}

_again:
	_acts = _ELXT_actions + _ELXT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 701 "ELXT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _ELXT_eof_trans[cs] > 0 ) {
		_trans = _ELXT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 155 "ELXT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < ELXT_first_final)
        o = ELXTBAD;

    return o;
}
