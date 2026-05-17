
/* #line 1 "DARTT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "DARTT.h"

ok64 DARTTonComment (u8cs tok, DARTTstate* state);
ok64 DARTTonString (u8cs tok, DARTTstate* state);
ok64 DARTTonNumber (u8cs tok, DARTTstate* state);
ok64 DARTTonAnnotation (u8cs tok, DARTTstate* state);
ok64 DARTTonWord (u8cs tok, DARTTstate* state);
ok64 DARTTonPunct (u8cs tok, DARTTstate* state);
ok64 DARTTonSpace (u8cs tok, DARTTstate* state);


/* #line 129 "DARTT.c.rl" */



/* #line 16 "DARTT.rl.c" */
static const char _DARTT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	8, 1, 9, 1, 10, 1, 11, 1, 
	12, 1, 13, 1, 14, 1, 15, 1, 
	16, 1, 17, 1, 18, 1, 19, 1, 
	20, 1, 21, 1, 22, 1, 23, 1, 
	24, 1, 25, 1, 26, 1, 27, 1, 
	28, 1, 29, 1, 30, 1, 31, 1, 
	32, 1, 33, 1, 34, 1, 35, 1, 
	36, 1, 37, 1, 38, 2, 2, 3, 
	2, 2, 4, 2, 2, 5, 2, 2, 
	6, 2, 2, 7
};

static const short _DARTT_key_offsets[] = {
	0, 0, 2, 4, 20, 21, 27, 34, 
	41, 48, 55, 62, 63, 69, 75, 76, 
	77, 78, 80, 82, 98, 99, 105, 112, 
	119, 126, 133, 140, 141, 147, 153, 154, 
	155, 156, 160, 162, 164, 165, 167, 171, 
	173, 175, 179, 181, 183, 189, 195, 196, 
	197, 228, 231, 232, 233, 241, 243, 244, 
	246, 248, 251, 252, 257, 260, 263, 264, 
	272, 276, 281, 284, 290, 293, 300, 302, 
	304, 306, 308, 317, 327, 329
};

static const unsigned char _DARTT_trans_keys[] = {
	34u, 92u, 34u, 92u, 34u, 36u, 39u, 48u, 
	63u, 92u, 110u, 114u, 117u, 120u, 97u, 98u, 
	101u, 102u, 116u, 118u, 123u, 48u, 57u, 65u, 
	70u, 97u, 102u, 125u, 48u, 57u, 65u, 70u, 
	97u, 102u, 125u, 48u, 57u, 65u, 70u, 97u, 
	102u, 125u, 48u, 57u, 65u, 70u, 97u, 102u, 
	125u, 48u, 57u, 65u, 70u, 97u, 102u, 125u, 
	48u, 57u, 65u, 70u, 97u, 102u, 125u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 34u, 34u, 34u, 39u, 92u, 
	39u, 92u, 34u, 36u, 39u, 48u, 63u, 92u, 
	110u, 114u, 117u, 120u, 97u, 98u, 101u, 102u, 
	116u, 118u, 123u, 48u, 57u, 65u, 70u, 97u, 
	102u, 125u, 48u, 57u, 65u, 70u, 97u, 102u, 
	125u, 48u, 57u, 65u, 70u, 97u, 102u, 125u, 
	48u, 57u, 65u, 70u, 97u, 102u, 125u, 48u, 
	57u, 65u, 70u, 97u, 102u, 125u, 48u, 57u, 
	65u, 70u, 97u, 102u, 125u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 39u, 39u, 39u, 43u, 45u, 48u, 57u, 
	48u, 57u, 48u, 57u, 42u, 42u, 47u, 43u, 
	45u, 48u, 57u, 48u, 57u, 48u, 57u, 43u, 
	45u, 48u, 57u, 48u, 57u, 48u, 57u, 48u, 
	57u, 65u, 70u, 97u, 102u, 36u, 95u, 65u, 
	90u, 97u, 122u, 34u, 39u, 32u, 33u, 34u, 
	36u, 37u, 38u, 39u, 42u, 43u, 45u, 46u, 
	47u, 48u, 60u, 61u, 62u, 63u, 64u, 94u, 
	95u, 114u, 124u, 126u, 9u, 13u, 49u, 57u, 
	65u, 90u, 97u, 122u, 32u, 9u, 13u, 61u, 
	34u, 36u, 95u, 48u, 57u, 65u, 90u, 97u, 
	122u, 38u, 61u, 39u, 43u, 61u, 45u, 61u, 
	46u, 48u, 57u, 46u, 69u, 95u, 101u, 48u, 
	57u, 95u, 48u, 57u, 42u, 47u, 61u, 10u, 
	46u, 69u, 88u, 95u, 101u, 120u, 48u, 57u, 
	69u, 101u, 48u, 57u, 69u, 95u, 101u, 48u, 
	57u, 95u, 48u, 57u, 46u, 69u, 95u, 101u, 
	48u, 57u, 95u, 48u, 57u, 95u, 48u, 57u, 
	65u, 70u, 97u, 102u, 60u, 61u, 61u, 62u, 
	61u, 62u, 46u, 63u, 36u, 46u, 95u, 48u, 
	57u, 65u, 90u, 97u, 122u, 34u, 36u, 39u, 
	95u, 48u, 57u, 65u, 90u, 97u, 122u, 61u, 
	124u, 47u, 0
};

static const char _DARTT_single_lengths[] = {
	0, 2, 2, 10, 1, 0, 1, 1, 
	1, 1, 1, 1, 0, 0, 1, 1, 
	1, 2, 2, 10, 1, 0, 1, 1, 
	1, 1, 1, 1, 0, 0, 1, 1, 
	1, 2, 0, 0, 1, 2, 2, 0, 
	0, 2, 0, 0, 0, 2, 1, 1, 
	23, 1, 1, 1, 2, 2, 1, 2, 
	2, 1, 1, 3, 1, 3, 1, 6, 
	2, 3, 1, 4, 1, 1, 2, 0, 
	2, 2, 3, 4, 2, 1
};

static const char _DARTT_range_lengths[] = {
	0, 0, 0, 3, 0, 3, 3, 3, 
	3, 3, 3, 0, 3, 3, 0, 0, 
	0, 0, 0, 3, 0, 3, 3, 3, 
	3, 3, 3, 0, 3, 3, 0, 0, 
	0, 1, 1, 1, 0, 0, 1, 1, 
	1, 1, 1, 1, 3, 2, 0, 0, 
	4, 1, 0, 0, 3, 0, 0, 0, 
	0, 1, 0, 1, 1, 0, 0, 1, 
	1, 1, 1, 1, 1, 3, 0, 1, 
	0, 0, 3, 3, 0, 0
};

static const short _DARTT_index_offsets[] = {
	0, 0, 3, 6, 20, 22, 26, 31, 
	36, 41, 46, 51, 53, 57, 61, 63, 
	65, 67, 70, 73, 87, 89, 93, 98, 
	103, 108, 113, 118, 120, 124, 128, 130, 
	132, 134, 138, 140, 142, 144, 147, 151, 
	153, 155, 159, 161, 163, 167, 172, 174, 
	176, 204, 207, 209, 211, 217, 220, 222, 
	225, 228, 231, 233, 238, 241, 245, 247, 
	255, 259, 264, 267, 273, 276, 281, 284, 
	286, 289, 292, 299, 307, 310
};

static const char _DARTT_indicies[] = {
	1, 2, 0, 3, 2, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 5, 6, 
	0, 0, 0, 4, 7, 4, 8, 8, 
	8, 4, 0, 9, 9, 9, 4, 0, 
	10, 10, 10, 4, 0, 11, 11, 11, 
	4, 0, 12, 12, 12, 4, 0, 13, 
	13, 13, 4, 0, 4, 14, 14, 14, 
	4, 0, 0, 0, 4, 17, 16, 18, 
	16, 19, 16, 21, 22, 20, 23, 22, 
	20, 20, 20, 20, 20, 20, 20, 20, 
	20, 24, 25, 20, 20, 20, 4, 26, 
	4, 27, 27, 27, 4, 20, 28, 28, 
	28, 4, 20, 29, 29, 29, 4, 20, 
	30, 30, 30, 4, 20, 31, 31, 31, 
	4, 20, 32, 32, 32, 4, 20, 4, 
	33, 33, 33, 4, 20, 20, 20, 4, 
	36, 35, 37, 35, 38, 35, 40, 40, 
	41, 39, 41, 39, 42, 39, 45, 44, 
	45, 46, 44, 48, 48, 49, 47, 49, 
	47, 50, 47, 52, 52, 53, 51, 53, 
	54, 55, 51, 56, 56, 56, 54, 57, 
	57, 57, 57, 4, 60, 59, 62, 61, 
	64, 65, 66, 67, 65, 68, 69, 65, 
	70, 71, 72, 73, 74, 75, 76, 77, 
	78, 79, 65, 67, 80, 81, 82, 64, 
	55, 67, 67, 63, 64, 64, 83, 84, 
	54, 16, 85, 67, 67, 67, 67, 67, 
	86, 84, 84, 87, 35, 88, 84, 84, 
	87, 84, 84, 87, 90, 42, 89, 84, 
	91, 93, 94, 93, 42, 92, 40, 41, 
	92, 44, 95, 84, 87, 96, 95, 98, 
	99, 100, 101, 99, 100, 55, 97, 103, 
	103, 50, 102, 103, 104, 103, 50, 102, 
	48, 49, 102, 98, 99, 101, 99, 55, 
	97, 52, 53, 105, 100, 56, 56, 56, 
	106, 107, 84, 87, 84, 87, 84, 107, 
	87, 84, 107, 87, 57, 57, 57, 57, 
	57, 57, 108, 59, 67, 61, 67, 67, 
	67, 67, 86, 84, 84, 87, 107, 87, 
	0
};

static const char _DARTT_trans_targs[] = {
	2, 51, 3, 48, 0, 4, 12, 5, 
	6, 7, 8, 9, 10, 11, 13, 48, 
	14, 15, 16, 48, 18, 54, 19, 48, 
	20, 28, 21, 22, 23, 24, 25, 26, 
	27, 29, 48, 30, 31, 32, 48, 48, 
	34, 60, 59, 48, 36, 37, 48, 48, 
	39, 66, 65, 48, 42, 68, 48, 67, 
	69, 74, 48, 46, 48, 47, 48, 48, 
	49, 50, 1, 52, 53, 17, 55, 56, 
	57, 61, 63, 70, 71, 72, 73, 45, 
	75, 76, 77, 48, 48, 48, 48, 48, 
	48, 48, 58, 48, 48, 33, 35, 62, 
	48, 48, 64, 41, 44, 43, 48, 38, 
	40, 48, 48, 50, 48
};

static const char _DARTT_trans_actions[] = {
	0, 5, 0, 17, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 53, 
	0, 0, 0, 9, 0, 5, 0, 19, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 55, 0, 0, 0, 11, 59, 
	0, 5, 5, 65, 0, 0, 7, 57, 
	0, 5, 5, 61, 0, 72, 67, 75, 
	69, 0, 63, 0, 13, 0, 15, 23, 
	0, 81, 0, 0, 0, 0, 0, 0, 
	0, 5, 75, 0, 0, 0, 0, 0, 
	5, 0, 0, 51, 21, 27, 43, 47, 
	29, 49, 0, 45, 35, 0, 0, 0, 
	25, 39, 5, 0, 0, 0, 33, 0, 
	0, 37, 31, 78, 41
};

static const char _DARTT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	1, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0
};

static const char _DARTT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	3, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0
};

static const short _DARTT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 16, 16, 
	16, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 35, 35, 
	35, 40, 40, 40, 44, 44, 48, 48, 
	48, 52, 55, 52, 55, 0, 59, 59, 
	0, 84, 55, 86, 87, 88, 89, 88, 
	88, 90, 92, 93, 93, 88, 97, 98, 
	103, 103, 103, 98, 106, 107, 88, 88, 
	88, 88, 109, 87, 88, 88
};

static const int DARTT_start = 48;
static const int DARTT_first_final = 48;
static const int DARTT_error = 0;

static const int DARTT_en_main = 48;


/* #line 132 "DARTT.c.rl" */

ok64 DARTTLexer(DARTTstate* state) {

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

    
/* #line 266 "DARTT.rl.c" */
	{
	cs = DARTT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 150 "DARTT.c.rl" */
    
/* #line 272 "DARTT.rl.c" */
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
	_acts = _DARTT_actions + _DARTT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 291 "DARTT.rl.c" */
		}
	}

	_keys = _DARTT_trans_keys + _DARTT_key_offsets[cs];
	_trans = _DARTT_index_offsets[cs];

	_klen = _DARTT_single_lengths[cs];
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

	_klen = _DARTT_range_lengths[cs];
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
	_trans = _DARTT_indicies[_trans];
_eof_trans:
	cs = _DARTT_trans_targs[_trans];

	if ( _DARTT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _DARTT_actions + _DARTT_trans_actions[_trans];
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
/* #line 43 "DARTT.c.rl" */
	{act = 9;}
	break;
	case 4:
/* #line 43 "DARTT.c.rl" */
	{act = 12;}
	break;
	case 5:
/* #line 43 "DARTT.c.rl" */
	{act = 13;}
	break;
	case 6:
/* #line 61 "DARTT.c.rl" */
	{act = 16;}
	break;
	case 7:
/* #line 61 "DARTT.c.rl" */
	{act = 17;}
	break;
	case 8:
/* #line 31 "DARTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 37 "DARTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 37 "DARTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 37 "DARTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 37 "DARTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 37 "DARTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 37 "DARTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 61 "DARTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 61 "DARTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 31 "DARTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 37 "DARTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 37 "DARTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 43 "DARTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 43 "DARTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 43 "DARTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 43 "DARTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 43 "DARTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 49 "DARTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonAnnotation(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 55 "DARTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 61 "DARTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 61 "DARTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 61 "DARTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 67 "DARTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 37 "DARTT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 37 "DARTT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 43 "DARTT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 43 "DARTT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 43 "DARTT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 55 "DARTT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 61 "DARTT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 9:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 13:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 16:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 17:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 659 "DARTT.rl.c" */
		}
	}

_again:
	_acts = _DARTT_actions + _DARTT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 670 "DARTT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _DARTT_eof_trans[cs] > 0 ) {
		_trans = _DARTT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 151 "DARTT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < DARTT_first_final)
        o = DARTTBAD;

    return o;
}
