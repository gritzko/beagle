
/* #line 1 "PHPT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "PHPT.h"

ok64 PHPTonComment (u8cs tok, PHPTstate* state);
ok64 PHPTonString (u8cs tok, PHPTstate* state);
ok64 PHPTonNumber (u8cs tok, PHPTstate* state);
ok64 PHPTonPreproc (u8cs tok, PHPTstate* state);
ok64 PHPTonWord (u8cs tok, PHPTstate* state);
ok64 PHPTonPunct (u8cs tok, PHPTstate* state);
ok64 PHPTonSpace (u8cs tok, PHPTstate* state);


/* #line 125 "PHPT.c.rl" */



/* #line 16 "PHPT.rl.c" */
static const char _PHPT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	10, 1, 11, 1, 12, 1, 13, 1, 
	14, 1, 15, 1, 16, 1, 17, 1, 
	18, 1, 19, 1, 20, 1, 21, 1, 
	22, 1, 23, 1, 24, 1, 25, 1, 
	26, 1, 27, 1, 28, 1, 29, 1, 
	30, 1, 31, 1, 32, 1, 33, 1, 
	34, 1, 35, 1, 36, 1, 37, 1, 
	38, 2, 2, 3, 2, 2, 4, 2, 
	2, 5, 2, 2, 6, 2, 2, 7, 
	2, 2, 8, 2, 2, 9
};

static const unsigned char _PHPT_key_offsets[] = {
	0, 0, 2, 18, 19, 25, 32, 39, 
	46, 53, 60, 61, 67, 72, 74, 76, 
	77, 81, 83, 85, 86, 88, 92, 94, 
	96, 100, 102, 104, 106, 108, 114, 115, 
	116, 117, 147, 150, 151, 152, 153, 160, 
	162, 164, 166, 169, 173, 178, 181, 184, 
	185, 197, 201, 206, 209, 215, 218, 221, 
	224, 231, 232, 235, 236, 238, 240, 242, 
	245, 252
};

static const unsigned char _PHPT_trans_keys[] = {
	34u, 92u, 34u, 36u, 39u, 92u, 110u, 114u, 
	117u, 120u, 48u, 55u, 97u, 98u, 101u, 102u, 
	116u, 118u, 123u, 48u, 57u, 65u, 70u, 97u, 
	102u, 125u, 48u, 57u, 65u, 70u, 97u, 102u, 
	125u, 48u, 57u, 65u, 70u, 97u, 102u, 125u, 
	48u, 57u, 65u, 70u, 97u, 102u, 125u, 48u, 
	57u, 65u, 70u, 97u, 102u, 125u, 48u, 57u, 
	65u, 70u, 97u, 102u, 125u, 48u, 57u, 65u, 
	70u, 97u, 102u, 95u, 65u, 90u, 97u, 122u, 
	39u, 92u, 39u, 92u, 46u, 43u, 45u, 48u, 
	57u, 48u, 57u, 48u, 57u, 42u, 42u, 47u, 
	43u, 45u, 48u, 57u, 48u, 57u, 48u, 57u, 
	43u, 45u, 48u, 57u, 48u, 57u, 48u, 57u, 
	48u, 49u, 48u, 55u, 48u, 57u, 65u, 70u, 
	97u, 102u, 104u, 112u, 62u, 32u, 33u, 34u, 
	35u, 36u, 37u, 38u, 39u, 42u, 43u, 45u, 
	46u, 47u, 48u, 58u, 60u, 61u, 62u, 63u, 
	94u, 95u, 124u, 9u, 13u, 49u, 57u, 65u, 
	90u, 97u, 122u, 32u, 9u, 13u, 61u, 61u, 
	10u, 95u, 48u, 57u, 65u, 90u, 97u, 122u, 
	38u, 61u, 42u, 61u, 43u, 61u, 45u, 61u, 
	62u, 46u, 61u, 48u, 57u, 69u, 95u, 101u, 
	48u, 57u, 95u, 48u, 57u, 42u, 47u, 61u, 
	10u, 46u, 66u, 69u, 79u, 88u, 95u, 98u, 
	101u, 111u, 120u, 48u, 57u, 69u, 101u, 48u, 
	57u, 69u, 95u, 101u, 48u, 57u, 95u, 48u, 
	57u, 46u, 69u, 95u, 101u, 48u, 57u, 95u, 
	48u, 57u, 95u, 48u, 49u, 95u, 48u, 55u, 
	95u, 48u, 57u, 65u, 70u, 97u, 102u, 58u, 
	60u, 61u, 63u, 62u, 61u, 112u, 61u, 62u, 
	61u, 62u, 45u, 62u, 63u, 95u, 48u, 57u, 
	65u, 90u, 97u, 122u, 61u, 124u, 0
};

static const char _PHPT_single_lengths[] = {
	0, 2, 8, 1, 0, 1, 1, 1, 
	1, 1, 1, 0, 1, 2, 2, 1, 
	2, 0, 0, 1, 2, 2, 0, 0, 
	2, 0, 0, 0, 0, 0, 1, 1, 
	1, 22, 1, 1, 1, 1, 1, 2, 
	2, 2, 1, 2, 3, 1, 3, 1, 
	10, 2, 3, 1, 4, 1, 1, 1, 
	1, 1, 3, 1, 2, 2, 2, 3, 
	1, 2
};

static const char _PHPT_range_lengths[] = {
	0, 0, 4, 0, 3, 3, 3, 3, 
	3, 3, 0, 3, 2, 0, 0, 0, 
	1, 1, 1, 0, 0, 1, 1, 1, 
	1, 1, 1, 1, 1, 3, 0, 0, 
	0, 4, 1, 0, 0, 0, 3, 0, 
	0, 0, 1, 1, 1, 1, 0, 0, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	3, 0, 0, 0, 0, 0, 0, 0, 
	3, 0
};

static const short _PHPT_index_offsets[] = {
	0, 0, 3, 16, 18, 22, 27, 32, 
	37, 42, 47, 49, 53, 57, 60, 63, 
	65, 69, 71, 73, 75, 78, 82, 84, 
	86, 90, 92, 94, 96, 98, 102, 104, 
	106, 108, 135, 138, 140, 142, 144, 149, 
	152, 155, 158, 161, 165, 170, 173, 177, 
	179, 191, 195, 200, 203, 209, 212, 215, 
	218, 223, 225, 229, 231, 234, 237, 240, 
	244, 249
};

static const char _PHPT_indicies[] = {
	1, 2, 0, 0, 0, 0, 0, 0, 
	0, 4, 5, 0, 0, 0, 0, 3, 
	6, 3, 7, 7, 7, 3, 0, 8, 
	8, 8, 3, 0, 9, 9, 9, 3, 
	0, 10, 10, 10, 3, 0, 11, 11, 
	11, 3, 0, 12, 12, 12, 3, 0, 
	3, 0, 0, 0, 3, 13, 13, 13, 
	3, 15, 16, 14, 14, 14, 3, 18, 
	17, 20, 20, 21, 19, 21, 19, 22, 
	19, 25, 24, 25, 26, 24, 28, 28, 
	29, 27, 29, 27, 30, 27, 32, 32, 
	33, 31, 33, 34, 35, 31, 36, 34, 
	37, 34, 38, 38, 38, 34, 40, 39, 
	41, 39, 18, 23, 43, 44, 0, 45, 
	46, 47, 48, 14, 49, 50, 51, 52, 
	53, 54, 55, 56, 57, 58, 59, 47, 
	60, 61, 43, 35, 60, 60, 42, 43, 
	43, 62, 64, 63, 18, 34, 65, 45, 
	13, 13, 13, 13, 66, 18, 18, 63, 
	64, 18, 63, 18, 18, 63, 18, 18, 
	63, 68, 18, 22, 67, 70, 71, 70, 
	22, 69, 20, 21, 69, 24, 72, 18, 
	63, 73, 72, 75, 76, 77, 78, 79, 
	80, 76, 77, 78, 79, 35, 74, 82, 
	82, 30, 81, 82, 83, 82, 30, 81, 
	28, 29, 81, 75, 77, 80, 77, 35, 
	74, 32, 33, 84, 76, 36, 85, 78, 
	37, 86, 79, 38, 38, 38, 87, 18, 
	63, 64, 88, 89, 63, 18, 90, 41, 
	92, 91, 64, 18, 63, 18, 64, 63, 
	93, 41, 64, 63, 60, 60, 60, 60, 
	94, 18, 18, 63, 0
};

static const char _PHPT_trans_targs[] = {
	1, 33, 2, 0, 3, 11, 4, 5, 
	6, 7, 8, 9, 10, 38, 13, 33, 
	14, 33, 33, 33, 17, 45, 44, 33, 
	19, 20, 33, 33, 22, 51, 50, 33, 
	25, 53, 33, 52, 54, 55, 56, 33, 
	31, 33, 33, 34, 35, 37, 12, 36, 
	39, 40, 41, 42, 43, 46, 48, 57, 
	58, 61, 62, 63, 64, 65, 33, 33, 
	36, 33, 33, 33, 15, 33, 16, 18, 
	47, 33, 33, 49, 27, 24, 28, 29, 
	26, 33, 21, 23, 33, 33, 33, 33, 
	59, 60, 33, 33, 30, 32, 33
};

static const char _PHPT_trans_actions[] = {
	0, 11, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 13, 
	0, 61, 15, 55, 0, 5, 5, 59, 
	0, 0, 9, 53, 0, 5, 5, 57, 
	0, 74, 63, 77, 71, 68, 65, 51, 
	0, 7, 17, 0, 0, 0, 0, 83, 
	0, 0, 0, 0, 5, 5, 77, 0, 
	0, 0, 0, 5, 0, 0, 49, 45, 
	80, 23, 39, 47, 0, 33, 0, 0, 
	0, 21, 37, 5, 0, 0, 0, 0, 
	0, 31, 0, 0, 35, 29, 27, 25, 
	0, 5, 43, 19, 0, 0, 41
};

static const char _PHPT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const char _PHPT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 3, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const short _PHPT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 18, 
	20, 20, 20, 24, 24, 28, 28, 28, 
	32, 35, 32, 35, 35, 35, 40, 40, 
	24, 0, 63, 64, 35, 66, 67, 64, 
	64, 64, 64, 68, 70, 70, 64, 74, 
	75, 82, 82, 82, 75, 85, 86, 87, 
	88, 64, 64, 91, 92, 64, 64, 64, 
	95, 64
};

static const int PHPT_start = 33;
static const int PHPT_first_final = 33;
static const int PHPT_error = 0;

static const int PHPT_en_main = 33;


/* #line 128 "PHPT.c.rl" */

ok64 PHPTLexer(PHPTstate* state) {

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

    
/* #line 237 "PHPT.rl.c" */
	{
	cs = PHPT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 146 "PHPT.c.rl" */
    
/* #line 243 "PHPT.rl.c" */
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
	_acts = _PHPT_actions + _PHPT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 262 "PHPT.rl.c" */
		}
	}

	_keys = _PHPT_trans_keys + _PHPT_key_offsets[cs];
	_trans = _PHPT_index_offsets[cs];

	_klen = _PHPT_single_lengths[cs];
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

	_klen = _PHPT_range_lengths[cs];
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
	_trans = _PHPT_indicies[_trans];
_eof_trans:
	cs = _PHPT_trans_targs[_trans];

	if ( _PHPT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _PHPT_actions + _PHPT_trans_actions[_trans];
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
/* #line 44 "PHPT.c.rl" */
	{act = 7;}
	break;
	case 4:
/* #line 44 "PHPT.c.rl" */
	{act = 8;}
	break;
	case 5:
/* #line 44 "PHPT.c.rl" */
	{act = 9;}
	break;
	case 6:
/* #line 44 "PHPT.c.rl" */
	{act = 12;}
	break;
	case 7:
/* #line 44 "PHPT.c.rl" */
	{act = 13;}
	break;
	case 8:
/* #line 62 "PHPT.c.rl" */
	{act = 16;}
	break;
	case 9:
/* #line 62 "PHPT.c.rl" */
	{act = 17;}
	break;
	case 10:
/* #line 50 "PHPT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 32 "PHPT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 38 "PHPT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 38 "PHPT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 62 "PHPT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 62 "PHPT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 50 "PHPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 32 "PHPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 32 "PHPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 44 "PHPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 44 "PHPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 44 "PHPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 44 "PHPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 44 "PHPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 44 "PHPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 44 "PHPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 56 "PHPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 56 "PHPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 62 "PHPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 62 "PHPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 62 "PHPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 68 "PHPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 50 "PHPT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 44 "PHPT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 44 "PHPT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 44 "PHPT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 62 "PHPT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 62 "PHPT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 7:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 8:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 9:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 13:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 16:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 17:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 636 "PHPT.rl.c" */
		}
	}

_again:
	_acts = _PHPT_actions + _PHPT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 647 "PHPT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _PHPT_eof_trans[cs] > 0 ) {
		_trans = _PHPT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 147 "PHPT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < PHPT_first_final)
        o = PHPTBAD;

    return o;
}
