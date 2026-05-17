
/* #line 1 "GLST.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "GLST.h"

ok64 GLSTonComment (u8cs tok, GLSTstate* state);
ok64 GLSTonNumber (u8cs tok, GLSTstate* state);
ok64 GLSTonPreproc (u8cs tok, GLSTstate* state);
ok64 GLSTonWord (u8cs tok, GLSTstate* state);
ok64 GLSTonPunct (u8cs tok, GLSTstate* state);
ok64 GLSTonSpace (u8cs tok, GLSTstate* state);


/* #line 104 "GLST.c.rl" */



/* #line 15 "GLST.rl.c" */
static const char _GLST_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	9, 1, 10, 1, 11, 1, 12, 1, 
	13, 1, 14, 1, 15, 1, 16, 1, 
	17, 1, 18, 1, 19, 1, 20, 1, 
	21, 1, 22, 1, 23, 1, 24, 1, 
	25, 1, 26, 1, 27, 1, 28, 1, 
	29, 2, 2, 3, 2, 2, 4, 2, 
	2, 5, 2, 2, 6, 2, 2, 7, 
	2, 2, 8
};

static const short _GLST_key_offsets[] = {
	0, 0, 14, 18, 20, 21, 23, 27, 
	29, 33, 35, 41, 66, 69, 70, 77, 
	85, 93, 101, 109, 117, 128, 137, 145, 
	153, 161, 169, 177, 185, 193, 201, 209, 
	217, 225, 233, 241, 249, 258, 266, 274, 
	282, 290, 298, 306, 314, 322, 330, 338, 
	340, 342, 344, 346, 350, 352, 355, 356, 
	365, 369, 371, 378, 383, 385, 391, 393, 
	395, 402
};

static const unsigned char _GLST_trans_keys[] = {
	9u, 32u, 95u, 100u, 101u, 105u, 108u, 112u, 
	117u, 118u, 65u, 90u, 97u, 122u, 43u, 45u, 
	48u, 57u, 48u, 57u, 42u, 42u, 47u, 43u, 
	45u, 48u, 57u, 48u, 57u, 43u, 45u, 48u, 
	57u, 48u, 57u, 48u, 57u, 65u, 70u, 97u, 
	102u, 32u, 33u, 35u, 37u, 38u, 42u, 43u, 
	45u, 46u, 47u, 48u, 60u, 61u, 62u, 94u, 
	95u, 124u, 9u, 13u, 49u, 57u, 65u, 90u, 
	97u, 122u, 32u, 9u, 13u, 61u, 95u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 101u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 102u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 105u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 110u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 101u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 108u, 110u, 
	114u, 120u, 48u, 57u, 65u, 90u, 97u, 122u, 
	95u, 105u, 115u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 102u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 100u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 105u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 114u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 111u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 114u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 116u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 101u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 110u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 115u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 105u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 111u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 110u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 102u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 100u, 110u, 48u, 57u, 65u, 90u, 
	97u, 122u, 95u, 101u, 48u, 57u, 65u, 90u, 
	97u, 122u, 95u, 100u, 48u, 57u, 65u, 90u, 
	97u, 122u, 95u, 114u, 48u, 57u, 65u, 90u, 
	97u, 122u, 95u, 97u, 48u, 57u, 65u, 90u, 
	98u, 122u, 95u, 103u, 48u, 57u, 65u, 90u, 
	97u, 122u, 95u, 109u, 48u, 57u, 65u, 90u, 
	97u, 122u, 95u, 97u, 48u, 57u, 65u, 90u, 
	98u, 122u, 95u, 110u, 48u, 57u, 65u, 90u, 
	97u, 122u, 95u, 101u, 48u, 57u, 65u, 90u, 
	97u, 122u, 95u, 114u, 48u, 57u, 65u, 90u, 
	97u, 122u, 38u, 61u, 43u, 61u, 45u, 61u, 
	48u, 57u, 69u, 101u, 48u, 57u, 48u, 57u, 
	42u, 47u, 61u, 10u, 46u, 69u, 88u, 101u, 
	120u, 48u, 55u, 56u, 57u, 69u, 101u, 48u, 
	57u, 48u, 57u, 46u, 69u, 101u, 48u, 55u, 
	56u, 57u, 46u, 69u, 101u, 48u, 57u, 48u, 
	57u, 48u, 57u, 65u, 70u, 97u, 102u, 60u, 
	61u, 61u, 62u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 61u, 124u, 0
};

static const char _GLST_single_lengths[] = {
	0, 10, 2, 0, 1, 2, 2, 0, 
	2, 0, 0, 17, 1, 1, 1, 2, 
	2, 2, 2, 2, 5, 3, 2, 2, 
	2, 2, 2, 2, 2, 2, 2, 2, 
	2, 2, 2, 2, 3, 2, 2, 2, 
	2, 2, 2, 2, 2, 2, 2, 2, 
	2, 2, 0, 2, 0, 3, 1, 5, 
	2, 0, 3, 3, 0, 0, 2, 2, 
	1, 2
};

static const char _GLST_range_lengths[] = {
	0, 2, 1, 1, 0, 0, 1, 1, 
	1, 1, 3, 4, 1, 0, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 0, 
	0, 0, 1, 1, 1, 0, 0, 2, 
	1, 1, 2, 1, 1, 3, 0, 0, 
	3, 0
};

static const short _GLST_index_offsets[] = {
	0, 0, 13, 17, 19, 21, 24, 28, 
	30, 34, 36, 40, 62, 65, 67, 72, 
	78, 84, 90, 96, 102, 111, 118, 124, 
	130, 136, 142, 148, 154, 160, 166, 172, 
	178, 184, 190, 196, 202, 209, 215, 221, 
	227, 233, 239, 245, 251, 257, 263, 269, 
	272, 275, 278, 280, 284, 286, 290, 292, 
	300, 304, 306, 312, 317, 319, 323, 326, 
	329, 334
};

static const char _GLST_indicies[] = {
	0, 0, 2, 3, 4, 5, 6, 7, 
	8, 9, 2, 2, 1, 11, 11, 12, 
	10, 12, 10, 15, 14, 15, 16, 14, 
	18, 18, 19, 17, 19, 17, 21, 21, 
	22, 20, 22, 20, 24, 24, 24, 23, 
	26, 27, 0, 27, 28, 27, 29, 30, 
	31, 32, 33, 35, 27, 36, 27, 37, 
	38, 26, 34, 37, 37, 25, 26, 26, 
	39, 40, 20, 2, 2, 2, 2, 20, 
	2, 42, 2, 2, 2, 41, 2, 6, 
	2, 2, 2, 41, 2, 43, 2, 2, 
	2, 41, 2, 44, 2, 2, 2, 41, 
	2, 45, 2, 2, 2, 41, 2, 46, 
	47, 48, 49, 2, 2, 2, 41, 2, 
	50, 44, 2, 2, 2, 41, 2, 45, 
	2, 2, 2, 41, 2, 51, 2, 2, 
	2, 41, 2, 50, 2, 2, 2, 41, 
	2, 52, 2, 2, 2, 41, 2, 53, 
	2, 2, 2, 41, 2, 45, 2, 2, 
	2, 41, 2, 54, 2, 2, 2, 41, 
	2, 55, 2, 2, 2, 41, 2, 56, 
	2, 2, 2, 41, 2, 57, 2, 2, 
	2, 41, 2, 58, 2, 2, 2, 41, 
	2, 59, 2, 2, 2, 41, 2, 45, 
	2, 2, 2, 41, 2, 60, 2, 2, 
	2, 41, 2, 62, 63, 2, 2, 2, 
	61, 2, 50, 2, 2, 2, 41, 2, 
	62, 2, 2, 2, 41, 2, 64, 2, 
	2, 2, 41, 2, 65, 2, 2, 2, 
	41, 2, 66, 2, 2, 2, 41, 2, 
	67, 2, 2, 2, 41, 2, 45, 2, 
	2, 2, 41, 2, 63, 2, 2, 2, 
	41, 2, 68, 2, 2, 2, 41, 2, 
	56, 2, 2, 2, 41, 40, 40, 69, 
	40, 40, 69, 40, 40, 69, 71, 70, 
	73, 73, 71, 72, 12, 72, 14, 74, 
	40, 69, 75, 74, 77, 79, 80, 79, 
	80, 78, 34, 76, 82, 82, 77, 81, 
	19, 81, 77, 79, 79, 78, 34, 83, 
	77, 79, 79, 34, 76, 22, 84, 24, 
	24, 24, 85, 86, 40, 69, 40, 86, 
	69, 37, 37, 37, 37, 87, 40, 40, 
	69, 0
};

static const char _GLST_trans_targs[] = {
	1, 0, 14, 15, 20, 35, 17, 39, 
	44, 45, 11, 3, 52, 11, 4, 5, 
	11, 11, 7, 57, 11, 9, 60, 11, 
	61, 11, 12, 13, 47, 48, 49, 50, 
	53, 55, 59, 62, 63, 64, 65, 11, 
	11, 11, 16, 18, 19, 14, 21, 23, 
	25, 28, 22, 24, 26, 27, 29, 30, 
	31, 32, 33, 34, 36, 11, 37, 38, 
	40, 41, 42, 43, 46, 11, 11, 51, 
	11, 2, 54, 11, 11, 56, 58, 8, 
	10, 11, 6, 11, 11, 11, 13, 11
};

static const char _GLST_trans_actions[] = {
	0, 0, 52, 0, 0, 0, 0, 0, 
	0, 0, 41, 0, 0, 45, 0, 0, 
	7, 39, 0, 0, 47, 0, 0, 43, 
	0, 11, 0, 64, 0, 0, 0, 0, 
	5, 58, 58, 0, 0, 0, 0, 37, 
	9, 17, 0, 0, 0, 49, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 15, 0, 0, 
	0, 0, 0, 0, 0, 33, 35, 5, 
	25, 0, 0, 13, 29, 5, 55, 0, 
	0, 23, 0, 21, 27, 19, 61, 31
};

static const char _GLST_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const char _GLST_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 3, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const short _GLST_eof_trans[] = {
	0, 0, 11, 11, 14, 14, 18, 18, 
	21, 21, 24, 0, 40, 21, 21, 42, 
	42, 42, 42, 42, 42, 42, 42, 42, 
	42, 42, 42, 42, 42, 42, 42, 42, 
	42, 42, 42, 42, 62, 42, 42, 42, 
	42, 42, 42, 42, 42, 42, 42, 70, 
	70, 70, 71, 73, 73, 70, 76, 77, 
	82, 82, 84, 77, 85, 86, 70, 70, 
	88, 70
};

static const int GLST_start = 11;
static const int GLST_first_final = 11;
static const int GLST_error = 0;

static const int GLST_en_main = 11;


/* #line 107 "GLST.c.rl" */

ok64 GLSTLexer(GLSTstate* state) {

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

    
/* #line 262 "GLST.rl.c" */
	{
	cs = GLST_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 125 "GLST.c.rl" */
    
/* #line 268 "GLST.rl.c" */
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
	_acts = _GLST_actions + _GLST_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 287 "GLST.rl.c" */
		}
	}

	_keys = _GLST_trans_keys + _GLST_key_offsets[cs];
	_trans = _GLST_index_offsets[cs];

	_klen = _GLST_single_lengths[cs];
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

	_klen = _GLST_range_lengths[cs];
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
	_trans = _GLST_indicies[_trans];
_eof_trans:
	cs = _GLST_trans_targs[_trans];

	if ( _GLST_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _GLST_actions + _GLST_trans_actions[_trans];
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
/* #line 38 "GLST.c.rl" */
	{act = 3;}
	break;
	case 4:
/* #line 38 "GLST.c.rl" */
	{act = 4;}
	break;
	case 5:
/* #line 32 "GLST.c.rl" */
	{act = 6;}
	break;
	case 6:
/* #line 32 "GLST.c.rl" */
	{act = 10;}
	break;
	case 7:
/* #line 50 "GLST.c.rl" */
	{act = 12;}
	break;
	case 8:
/* #line 50 "GLST.c.rl" */
	{act = 13;}
	break;
	case 9:
/* #line 26 "GLST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 50 "GLST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 50 "GLST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 26 "GLST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 38 "GLST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 38 "GLST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 32 "GLST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 32 "GLST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 32 "GLST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 32 "GLST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 32 "GLST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 32 "GLST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 44 "GLST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 50 "GLST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 50 "GLST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 56 "GLST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 32 "GLST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 32 "GLST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 32 "GLST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 50 "GLST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 3:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 4:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 6:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 10:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 13:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 586 "GLST.rl.c" */
		}
	}

_again:
	_acts = _GLST_actions + _GLST_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 597 "GLST.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _GLST_eof_trans[cs] > 0 ) {
		_trans = _GLST_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 126 "GLST.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < GLST_first_final)
        o = GLSTBAD;

    return o;
}
