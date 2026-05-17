
/* #line 1 "ERLT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "ERLT.h"

ok64 ERLTonComment (u8cs tok, ERLTstate* state);
ok64 ERLTonString (u8cs tok, ERLTstate* state);
ok64 ERLTonNumber (u8cs tok, ERLTstate* state);
ok64 ERLTonPreproc (u8cs tok, ERLTstate* state);
ok64 ERLTonWord (u8cs tok, ERLTstate* state);
ok64 ERLTonPunct (u8cs tok, ERLTstate* state);
ok64 ERLTonSpace (u8cs tok, ERLTstate* state);


/* #line 120 "ERLT.c.rl" */



/* #line 16 "ERLT.rl.c" */
static const char _ERLT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	5, 1, 6, 1, 7, 1, 8, 1, 
	9, 1, 10, 1, 11, 1, 12, 1, 
	13, 1, 14, 1, 15, 1, 16, 1, 
	17, 1, 18, 1, 19, 1, 20, 1, 
	21, 1, 22, 1, 23, 1, 24, 1, 
	25, 1, 26, 2, 2, 3, 2, 2, 
	4
};

static const unsigned char _ERLT_key_offsets[] = {
	0, 0, 2, 18, 24, 30, 31, 31, 
	33, 49, 55, 61, 62, 63, 64, 65, 
	66, 67, 69, 70, 71, 72, 73, 74, 
	75, 76, 77, 78, 79, 80, 81, 82, 
	85, 86, 87, 88, 89, 90, 91, 92, 
	93, 94, 95, 96, 99, 101, 102, 103, 
	104, 105, 106, 107, 108, 109, 110, 111, 
	112, 113, 114, 115, 116, 117, 118, 119, 
	120, 121, 122, 123, 124, 125, 126, 127, 
	128, 129, 130, 131, 132, 138, 140, 144, 
	146, 147, 171, 174, 175, 176, 189, 190, 
	191, 192, 193, 197, 203, 207, 209, 211, 
	213, 217, 219, 226, 233
};

static const unsigned char _ERLT_trans_keys[] = {
	34u, 92u, 10u, 34u, 39u, 63u, 92u, 110u, 
	118u, 120u, 48u, 57u, 97u, 98u, 100u, 102u, 
	114u, 116u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 92u, 39u, 
	92u, 10u, 34u, 39u, 63u, 92u, 110u, 118u, 
	120u, 48u, 57u, 97u, 98u, 100u, 102u, 114u, 
	116u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 101u, 104u, 97u, 
	118u, 105u, 111u, 114u, 117u, 114u, 97u, 108u, 
	108u, 98u, 97u, 99u, 107u, 101u, 102u, 105u, 
	110u, 101u, 108u, 110u, 120u, 115u, 100u, 105u, 
	102u, 112u, 111u, 114u, 116u, 116u, 121u, 112u, 
	102u, 109u, 110u, 100u, 110u, 101u, 100u, 112u, 
	111u, 114u, 116u, 99u, 108u, 117u, 100u, 101u, 
	108u, 105u, 98u, 111u, 100u, 117u, 108u, 112u, 
	97u, 113u, 117u, 101u, 99u, 111u, 114u, 100u, 
	112u, 101u, 99u, 110u, 48u, 57u, 65u, 90u, 
	97u, 122u, 48u, 57u, 43u, 45u, 48u, 57u, 
	48u, 57u, 61u, 32u, 33u, 34u, 36u, 37u, 
	39u, 43u, 45u, 46u, 47u, 58u, 60u, 61u, 
	62u, 95u, 124u, 9u, 13u, 48u, 57u, 65u, 
	90u, 97u, 122u, 32u, 9u, 13u, 10u, 43u, 
	45u, 62u, 98u, 99u, 100u, 101u, 105u, 109u, 
	111u, 114u, 115u, 116u, 117u, 95u, 95u, 46u, 
	61u, 35u, 46u, 48u, 57u, 48u, 57u, 65u, 
	90u, 97u, 122u, 69u, 101u, 48u, 57u, 48u, 
	57u, 58u, 61u, 45u, 60u, 47u, 58u, 60u, 
	62u, 61u, 62u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 95u, 48u, 57u, 65u, 90u, 97u, 
	122u, 124u, 0
};

static const char _ERLT_single_lengths[] = {
	0, 2, 8, 0, 0, 1, 0, 2, 
	8, 0, 0, 1, 1, 1, 1, 1, 
	1, 2, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 3, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 3, 2, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 0, 0, 2, 0, 
	1, 16, 1, 1, 1, 13, 1, 1, 
	1, 1, 2, 0, 2, 0, 2, 2, 
	2, 0, 1, 1, 1
};

static const char _ERLT_range_lengths[] = {
	0, 0, 4, 3, 3, 0, 0, 0, 
	4, 3, 3, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 3, 1, 1, 1, 
	0, 4, 1, 0, 0, 0, 0, 0, 
	0, 0, 1, 3, 1, 1, 0, 0, 
	1, 1, 3, 3, 0
};

static const short _ERLT_index_offsets[] = {
	0, 0, 3, 16, 20, 24, 26, 27, 
	30, 43, 47, 51, 53, 55, 57, 59, 
	61, 63, 66, 68, 70, 72, 74, 76, 
	78, 80, 82, 84, 86, 88, 90, 92, 
	96, 98, 100, 102, 104, 106, 108, 110, 
	112, 114, 116, 118, 122, 125, 127, 129, 
	131, 133, 135, 137, 139, 141, 143, 145, 
	147, 149, 151, 153, 155, 157, 159, 161, 
	163, 165, 167, 169, 171, 173, 175, 177, 
	179, 181, 183, 185, 187, 191, 193, 197, 
	199, 201, 222, 225, 227, 229, 243, 245, 
	247, 249, 251, 255, 259, 263, 265, 268, 
	271, 275, 277, 282, 287
};

static const char _ERLT_indicies[] = {
	1, 2, 0, 0, 0, 0, 0, 0, 
	0, 0, 4, 0, 0, 0, 0, 3, 
	5, 5, 5, 3, 0, 0, 0, 3, 
	7, 6, 8, 10, 11, 9, 9, 9, 
	9, 9, 9, 9, 9, 12, 9, 9, 
	9, 9, 3, 13, 13, 13, 3, 9, 
	9, 9, 3, 15, 14, 16, 14, 17, 
	14, 18, 14, 19, 14, 20, 14, 21, 
	22, 14, 21, 14, 23, 14, 24, 14, 
	25, 14, 26, 14, 27, 14, 28, 14, 
	21, 14, 29, 14, 30, 14, 31, 14, 
	32, 14, 21, 33, 34, 35, 36, 14, 
	32, 14, 37, 14, 38, 14, 21, 14, 
	39, 14, 40, 14, 41, 14, 42, 14, 
	44, 43, 45, 33, 32, 33, 46, 47, 
	48, 14, 49, 50, 14, 38, 14, 49, 
	14, 51, 14, 52, 14, 53, 14, 21, 
	14, 54, 14, 55, 14, 56, 14, 57, 
	14, 58, 14, 59, 43, 60, 43, 21, 
	43, 61, 14, 62, 14, 63, 14, 32, 
	14, 64, 14, 65, 14, 66, 14, 32, 
	14, 67, 14, 68, 14, 69, 14, 70, 
	14, 21, 14, 71, 14, 72, 14, 21, 
	14, 50, 14, 74, 74, 74, 73, 75, 
	73, 77, 77, 78, 76, 78, 76, 79, 
	14, 81, 79, 0, 82, 83, 9, 84, 
	85, 86, 87, 89, 90, 91, 92, 93, 
	95, 81, 88, 93, 94, 80, 81, 81, 
	96, 97, 83, 79, 98, 79, 79, 99, 
	100, 101, 102, 103, 104, 105, 106, 107, 
	44, 108, 98, 110, 109, 111, 109, 79, 
	112, 79, 98, 114, 115, 88, 113, 74, 
	74, 74, 116, 118, 118, 75, 117, 78, 
	117, 79, 79, 98, 79, 79, 98, 119, 
	119, 79, 98, 79, 98, 93, 93, 93, 
	93, 120, 94, 94, 94, 94, 121, 79, 
	98, 0
};

static const char _ERLT_trans_targs[] = {
	1, 81, 2, 0, 3, 4, 81, 6, 
	81, 7, 81, 8, 9, 10, 81, 12, 
	13, 14, 15, 16, 17, 81, 18, 20, 
	21, 22, 23, 24, 25, 27, 28, 29, 
	30, 81, 32, 33, 36, 34, 35, 37, 
	38, 39, 86, 81, 41, 42, 44, 47, 
	51, 45, 46, 48, 49, 50, 52, 53, 
	54, 55, 87, 57, 58, 60, 61, 62, 
	64, 65, 66, 68, 69, 70, 71, 73, 
	74, 81, 91, 92, 81, 79, 93, 81, 
	81, 82, 5, 83, 84, 85, 88, 89, 
	90, 94, 95, 96, 97, 98, 99, 100, 
	81, 81, 81, 11, 19, 26, 31, 43, 
	59, 63, 67, 72, 75, 81, 40, 56, 
	81, 81, 76, 77, 81, 81, 78, 80, 
	81, 81
};

static const char _ERLT_trans_actions[] = {
	0, 7, 0, 0, 0, 0, 13, 0, 
	11, 0, 9, 0, 0, 0, 47, 0, 
	0, 0, 0, 0, 0, 15, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 49, 0, 0, 0, 0, 0, 0, 
	0, 0, 51, 45, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 5, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 43, 0, 5, 41, 0, 0, 17, 
	19, 0, 0, 0, 0, 54, 0, 0, 
	5, 0, 0, 5, 0, 0, 0, 0, 
	39, 21, 35, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 29, 0, 0, 
	37, 27, 0, 0, 23, 25, 0, 0, 
	33, 31
};

static const char _ERLT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0
};

static const char _ERLT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 3, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0
};

static const short _ERLT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 15, 15, 15, 15, 15, 
	15, 15, 15, 15, 15, 15, 15, 15, 
	15, 15, 15, 15, 15, 15, 34, 15, 
	15, 15, 15, 15, 15, 15, 15, 15, 
	44, 34, 34, 15, 15, 15, 15, 15, 
	15, 15, 15, 15, 15, 15, 15, 15, 
	44, 44, 44, 15, 15, 15, 15, 15, 
	15, 15, 15, 15, 15, 15, 15, 15, 
	15, 15, 15, 15, 74, 74, 77, 77, 
	15, 0, 97, 98, 99, 99, 110, 110, 
	113, 99, 114, 117, 118, 118, 99, 99, 
	99, 99, 121, 122, 99
};

static const int ERLT_start = 81;
static const int ERLT_first_final = 81;
static const int ERLT_error = 0;

static const int ERLT_en_main = 81;


/* #line 123 "ERLT.c.rl" */

ok64 ERLTLexer(ERLTstate* state) {

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

    
/* #line 273 "ERLT.rl.c" */
	{
	cs = ERLT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 141 "ERLT.c.rl" */
    
/* #line 279 "ERLT.rl.c" */
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
	_acts = _ERLT_actions + _ERLT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 298 "ERLT.rl.c" */
		}
	}

	_keys = _ERLT_trans_keys + _ERLT_key_offsets[cs];
	_trans = _ERLT_index_offsets[cs];

	_klen = _ERLT_single_lengths[cs];
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

	_klen = _ERLT_range_lengths[cs];
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
	_trans = _ERLT_indicies[_trans];
_eof_trans:
	cs = _ERLT_trans_targs[_trans];

	if ( _ERLT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _ERLT_actions + _ERLT_trans_actions[_trans];
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
/* #line 48 "ERLT.c.rl" */
	{act = 9;}
	break;
	case 4:
/* #line 60 "ERLT.c.rl" */
	{act = 13;}
	break;
	case 5:
/* #line 36 "ERLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 36 "ERLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 42 "ERLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 42 "ERLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 48 "ERLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 60 "ERLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 60 "ERLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 30 "ERLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 42 "ERLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 42 "ERLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 42 "ERLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 48 "ERLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 54 "ERLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 54 "ERLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 60 "ERLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 60 "ERLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 66 "ERLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 42 "ERLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 42 "ERLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 48 "ERLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 60 "ERLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 9:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 13:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 561 "ERLT.rl.c" */
		}
	}

_again:
	_acts = _ERLT_actions + _ERLT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 572 "ERLT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _ERLT_eof_trans[cs] > 0 ) {
		_trans = _ERLT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 142 "ERLT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < ERLT_first_final)
        o = ERLTBAD;

    return o;
}
