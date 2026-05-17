
/* #line 1 "DT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "DT.h"

ok64 DTonComment (u8cs tok, DTstate* state);
ok64 DTonString (u8cs tok, DTstate* state);
ok64 DTonNumber (u8cs tok, DTstate* state);
ok64 DTonWord (u8cs tok, DTstate* state);
ok64 DTonPunct (u8cs tok, DTstate* state);
ok64 DTonSpace (u8cs tok, DTstate* state);


/* #line 133 "DT.c.rl" */



/* #line 15 "DT.rl.c" */
static const char _DT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	9, 1, 10, 1, 11, 1, 12, 1, 
	13, 1, 14, 1, 15, 1, 16, 1, 
	17, 1, 18, 1, 19, 1, 20, 1, 
	21, 1, 22, 1, 23, 1, 24, 1, 
	25, 1, 26, 1, 27, 1, 28, 1, 
	29, 1, 30, 1, 31, 1, 32, 1, 
	33, 1, 34, 1, 35, 1, 36, 1, 
	37, 1, 38, 1, 39, 1, 40, 1, 
	41, 1, 42, 1, 43, 2, 2, 3, 
	2, 2, 4, 2, 2, 5, 2, 2, 
	6, 2, 2, 7, 2, 2, 8
};

static const short _DT_key_offsets[] = {
	0, 0, 2, 19, 25, 31, 37, 43, 
	49, 55, 61, 67, 69, 86, 92, 98, 
	104, 110, 116, 122, 128, 134, 138, 140, 
	142, 143, 145, 146, 148, 152, 154, 156, 
	160, 162, 164, 166, 172, 176, 178, 184, 
	185, 186, 214, 217, 218, 220, 222, 224, 
	227, 235, 241, 245, 246, 259, 266, 274, 
	280, 289, 295, 296, 297, 303, 304, 305, 
	318, 327, 337, 343, 344, 345, 347, 349, 
	351, 353, 360, 368
};

static const unsigned char _DT_trans_keys[] = {
	34u, 92u, 34u, 39u, 63u, 85u, 92u, 110u, 
	114u, 117u, 120u, 48u, 55u, 97u, 98u, 101u, 
	102u, 116u, 118u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 39u, 92u, 34u, 39u, 63u, 
	85u, 92u, 110u, 114u, 117u, 120u, 48u, 55u, 
	97u, 98u, 101u, 102u, 116u, 118u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 43u, 45u, 
	48u, 57u, 48u, 57u, 48u, 57u, 42u, 42u, 
	47u, 43u, 43u, 47u, 43u, 45u, 48u, 57u, 
	48u, 57u, 48u, 57u, 43u, 45u, 48u, 57u, 
	48u, 57u, 48u, 57u, 48u, 49u, 48u, 57u, 
	65u, 70u, 97u, 102u, 43u, 45u, 48u, 57u, 
	48u, 57u, 48u, 57u, 65u, 70u, 97u, 102u, 
	96u, 34u, 32u, 33u, 34u, 37u, 38u, 39u, 
	42u, 43u, 45u, 46u, 47u, 48u, 60u, 61u, 
	62u, 94u, 96u, 114u, 124u, 126u, 9u, 13u, 
	49u, 57u, 65u, 90u, 95u, 122u, 32u, 9u, 
	13u, 61u, 38u, 61u, 43u, 61u, 45u, 61u, 
	46u, 48u, 57u, 69u, 70u, 76u, 95u, 101u, 
	102u, 48u, 57u, 70u, 76u, 95u, 102u, 48u, 
	57u, 42u, 43u, 47u, 61u, 10u, 46u, 66u, 
	69u, 76u, 85u, 88u, 95u, 98u, 101u, 117u, 
	120u, 48u, 57u, 69u, 70u, 76u, 101u, 102u, 
	48u, 57u, 69u, 70u, 76u, 95u, 101u, 102u, 
	48u, 57u, 70u, 76u, 95u, 102u, 48u, 57u, 
	46u, 69u, 76u, 85u, 95u, 101u, 117u, 48u, 
	57u, 70u, 76u, 95u, 102u, 48u, 57u, 117u, 
	76u, 76u, 85u, 95u, 117u, 48u, 49u, 117u, 
	76u, 46u, 76u, 80u, 85u, 95u, 112u, 117u, 
	48u, 57u, 65u, 70u, 97u, 102u, 76u, 80u, 
	112u, 48u, 57u, 65u, 70u, 97u, 102u, 76u, 
	80u, 95u, 112u, 48u, 57u, 65u, 70u, 97u, 
	102u, 70u, 76u, 95u, 102u, 48u, 57u, 117u, 
	76u, 60u, 61u, 61u, 62u, 61u, 62u, 61u, 
	62u, 95u, 48u, 57u, 65u, 90u, 97u, 122u, 
	34u, 95u, 48u, 57u, 65u, 90u, 97u, 122u, 
	61u, 124u, 0
};

static const char _DT_single_lengths[] = {
	0, 2, 9, 0, 0, 0, 0, 0, 
	0, 0, 0, 2, 9, 0, 0, 0, 
	0, 0, 0, 0, 0, 2, 0, 0, 
	1, 2, 1, 2, 2, 0, 0, 2, 
	0, 0, 0, 0, 2, 0, 0, 1, 
	1, 20, 1, 1, 2, 2, 2, 1, 
	6, 4, 4, 1, 11, 5, 6, 4, 
	7, 4, 1, 1, 4, 1, 1, 7, 
	3, 4, 4, 1, 1, 2, 0, 2, 
	2, 1, 2, 2
};

static const char _DT_range_lengths[] = {
	0, 0, 4, 3, 3, 3, 3, 3, 
	3, 3, 3, 0, 4, 3, 3, 3, 
	3, 3, 3, 3, 3, 1, 1, 1, 
	0, 0, 0, 0, 1, 1, 1, 1, 
	1, 1, 1, 3, 1, 1, 3, 0, 
	0, 4, 1, 0, 0, 0, 0, 1, 
	1, 1, 0, 0, 1, 1, 1, 1, 
	1, 1, 0, 0, 1, 0, 0, 3, 
	3, 3, 1, 0, 0, 0, 1, 0, 
	0, 3, 3, 0
};

static const short _DT_index_offsets[] = {
	0, 0, 3, 17, 21, 25, 29, 33, 
	37, 41, 45, 49, 52, 66, 70, 74, 
	78, 82, 86, 90, 94, 98, 102, 104, 
	106, 108, 111, 113, 116, 120, 122, 124, 
	128, 130, 132, 134, 138, 142, 144, 148, 
	150, 152, 177, 180, 182, 185, 188, 191, 
	194, 202, 208, 213, 215, 228, 235, 243, 
	249, 258, 264, 266, 268, 274, 276, 278, 
	289, 296, 304, 310, 312, 314, 317, 319, 
	322, 325, 330, 336
};

static const char _DT_indicies[] = {
	1, 2, 0, 0, 0, 0, 4, 0, 
	0, 0, 5, 6, 0, 0, 0, 0, 
	3, 7, 7, 7, 3, 8, 8, 8, 
	3, 9, 9, 9, 3, 5, 5, 5, 
	3, 10, 10, 10, 3, 6, 6, 6, 
	3, 11, 11, 11, 3, 0, 0, 0, 
	3, 13, 14, 12, 12, 12, 12, 15, 
	12, 12, 12, 16, 17, 12, 12, 12, 
	12, 3, 18, 18, 18, 3, 19, 19, 
	19, 3, 20, 20, 20, 3, 16, 16, 
	16, 3, 21, 21, 21, 3, 17, 17, 
	17, 3, 22, 22, 22, 3, 12, 12, 
	12, 3, 24, 24, 25, 23, 25, 23, 
	26, 23, 29, 28, 29, 30, 28, 32, 
	31, 32, 33, 31, 35, 35, 36, 34, 
	36, 34, 37, 34, 39, 39, 40, 38, 
	40, 41, 42, 38, 43, 41, 44, 44, 
	44, 41, 46, 46, 47, 45, 47, 45, 
	48, 48, 48, 45, 50, 49, 53, 52, 
	55, 56, 0, 56, 57, 12, 56, 58, 
	59, 60, 61, 62, 63, 64, 65, 56, 
	49, 67, 68, 56, 55, 42, 66, 66, 
	54, 55, 55, 69, 70, 41, 70, 70, 
	71, 70, 70, 71, 70, 70, 71, 70, 
	26, 72, 74, 75, 75, 76, 74, 75, 
	26, 73, 75, 75, 24, 75, 25, 73, 
	28, 31, 77, 70, 71, 78, 77, 80, 
	81, 82, 83, 84, 85, 86, 81, 82, 
	87, 85, 42, 79, 89, 90, 90, 89, 
	90, 37, 88, 89, 90, 90, 91, 89, 
	90, 37, 88, 90, 90, 35, 90, 36, 
	88, 80, 82, 83, 84, 86, 82, 87, 
	42, 79, 93, 93, 39, 93, 40, 92, 
	84, 79, 84, 79, 95, 96, 81, 97, 
	43, 94, 96, 94, 96, 94, 99, 100, 
	101, 102, 85, 101, 103, 44, 44, 44, 
	98, 104, 101, 101, 48, 48, 48, 98, 
	104, 101, 105, 101, 48, 48, 48, 98, 
	104, 104, 46, 104, 47, 98, 102, 98, 
	102, 106, 107, 70, 71, 70, 71, 70, 
	108, 71, 70, 107, 109, 66, 66, 66, 
	66, 110, 52, 66, 66, 66, 66, 110, 
	70, 70, 71, 0
};

static const char _DT_trans_targs[] = {
	1, 41, 2, 0, 3, 7, 9, 4, 
	5, 6, 8, 10, 11, 41, 12, 13, 
	17, 19, 14, 15, 16, 18, 20, 41, 
	22, 49, 48, 41, 24, 25, 41, 26, 
	27, 41, 41, 29, 55, 54, 41, 32, 
	57, 41, 56, 60, 63, 41, 37, 66, 
	65, 39, 41, 41, 40, 41, 41, 42, 
	43, 44, 45, 46, 47, 50, 52, 69, 
	70, 71, 73, 74, 75, 41, 41, 41, 
	41, 41, 21, 41, 23, 51, 41, 41, 
	53, 34, 31, 58, 41, 35, 33, 59, 
	41, 28, 41, 30, 41, 41, 41, 61, 
	41, 62, 41, 64, 67, 36, 41, 68, 
	41, 38, 41, 43, 72, 41, 41
};

static const char _DT_trans_actions[] = {
	0, 13, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 17, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 67, 
	0, 5, 5, 73, 0, 0, 7, 0, 
	0, 9, 65, 0, 5, 5, 69, 0, 
	83, 75, 86, 80, 77, 63, 0, 5, 
	5, 0, 11, 71, 0, 15, 35, 0, 
	92, 0, 0, 0, 0, 5, 86, 0, 
	0, 0, 0, 5, 0, 61, 33, 57, 
	59, 47, 0, 27, 0, 0, 37, 51, 
	5, 0, 0, 0, 31, 0, 0, 0, 
	45, 0, 25, 0, 49, 29, 43, 0, 
	23, 0, 39, 5, 0, 0, 21, 0, 
	19, 0, 41, 89, 0, 55, 53
};

static const char _DT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0
};

static const char _DT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 3, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0
};

static const short _DT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 24, 24, 24, 
	28, 28, 28, 28, 35, 35, 35, 39, 
	42, 39, 42, 42, 46, 46, 46, 0, 
	52, 0, 70, 42, 72, 72, 72, 73, 
	74, 74, 72, 79, 80, 89, 89, 89, 
	80, 93, 80, 80, 95, 95, 95, 99, 
	99, 99, 99, 99, 107, 72, 72, 72, 
	110, 111, 111, 72
};

static const int DT_start = 41;
static const int DT_first_final = 41;
static const int DT_error = 0;

static const int DT_en_main = 41;


/* #line 136 "DT.c.rl" */

ok64 DTLexer(DTstate* state) {

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

    
/* #line 274 "DT.rl.c" */
	{
	cs = DT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 154 "DT.c.rl" */
    
/* #line 280 "DT.rl.c" */
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
	_acts = _DT_actions + _DT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 299 "DT.rl.c" */
		}
	}

	_keys = _DT_trans_keys + _DT_key_offsets[cs];
	_trans = _DT_index_offsets[cs];

	_klen = _DT_single_lengths[cs];
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

	_klen = _DT_range_lengths[cs];
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
	_trans = _DT_indicies[_trans];
_eof_trans:
	cs = _DT_trans_targs[_trans];

	if ( _DT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _DT_actions + _DT_trans_actions[_trans];
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
/* #line 44 "DT.c.rl" */
	{act = 8;}
	break;
	case 4:
/* #line 44 "DT.c.rl" */
	{act = 10;}
	break;
	case 5:
/* #line 44 "DT.c.rl" */
	{act = 13;}
	break;
	case 6:
/* #line 44 "DT.c.rl" */
	{act = 14;}
	break;
	case 7:
/* #line 56 "DT.c.rl" */
	{act = 16;}
	break;
	case 8:
/* #line 56 "DT.c.rl" */
	{act = 17;}
	break;
	case 9:
/* #line 32 "DT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 32 "DT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 38 "DT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 38 "DT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 38 "DT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 38 "DT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 44 "DT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 44 "DT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 44 "DT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 44 "DT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 44 "DT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 44 "DT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 44 "DT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 56 "DT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 56 "DT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 32 "DT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 44 "DT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 44 "DT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 44 "DT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 44 "DT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 44 "DT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 44 "DT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 44 "DT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 50 "DT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 56 "DT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 56 "DT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 56 "DT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 62 "DT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 44 "DT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 44 "DT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 39:
/* #line 44 "DT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 40:
/* #line 44 "DT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 41:
/* #line 50 "DT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 42:
/* #line 56 "DT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 43:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 8:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 10:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 13:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 14:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 16:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 17:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 710 "DT.rl.c" */
		}
	}

_again:
	_acts = _DT_actions + _DT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 721 "DT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _DT_eof_trans[cs] > 0 ) {
		_trans = _DT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 155 "DT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < DT_first_final)
        o = DTBAD;

    return o;
}
