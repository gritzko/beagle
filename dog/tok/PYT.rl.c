
/* #line 1 "PYT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "PYT.h"

ok64 PYTonComment (u8cs tok, PYTstate* state);
ok64 PYTonString (u8cs tok, PYTstate* state);
ok64 PYTonNumber (u8cs tok, PYTstate* state);
ok64 PYTonDecorator (u8cs tok, PYTstate* state);
ok64 PYTonWord (u8cs tok, PYTstate* state);
ok64 PYTonPunct (u8cs tok, PYTstate* state);
ok64 PYTonSpace (u8cs tok, PYTstate* state);


/* #line 126 "PYT.c.rl" */



/* #line 16 "PYT.rl.c" */
static const char _PYT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19, 1, 20, 1, 21, 1, 22, 1, 
	23, 1, 24, 1, 25, 1, 26, 1, 
	27, 1, 28, 1, 29, 1, 30, 1, 
	31, 1, 32, 1, 33, 1, 34, 1, 
	35, 1, 36, 1, 37, 1, 38, 1, 
	39, 1, 40, 1, 41, 1, 42, 1, 
	43, 2, 2, 3, 2, 2, 4, 2, 
	2, 5, 2, 2, 6, 2, 2, 7, 
	2, 2, 8, 2, 2, 9, 2, 2, 
	10
};

static const unsigned char _PYT_key_offsets[] = {
	0, 2, 2, 3, 4, 5, 7, 7, 
	8, 9, 10, 11, 15, 17, 19, 23, 
	25, 27, 31, 33, 35, 37, 39, 45, 
	47, 49, 86, 89, 90, 92, 93, 94, 
	96, 98, 99, 101, 103, 106, 113, 118, 
	120, 134, 140, 147, 152, 160, 165, 168, 
	171, 178, 180, 182, 188, 196, 203, 220, 
	229
};

static const unsigned char _PYT_trans_keys[] = {
	34u, 92u, 34u, 34u, 34u, 39u, 92u, 39u, 
	39u, 39u, 46u, 43u, 45u, 48u, 57u, 48u, 
	57u, 48u, 57u, 43u, 45u, 48u, 57u, 48u, 
	57u, 48u, 57u, 43u, 45u, 48u, 57u, 48u, 
	57u, 48u, 57u, 48u, 49u, 48u, 55u, 48u, 
	57u, 65u, 70u, 97u, 102u, 34u, 92u, 39u, 
	92u, 32u, 33u, 34u, 35u, 37u, 38u, 39u, 
	42u, 43u, 45u, 46u, 47u, 48u, 58u, 60u, 
	61u, 62u, 64u, 66u, 70u, 82u, 85u, 94u, 
	95u, 98u, 102u, 114u, 117u, 124u, 9u, 13u, 
	49u, 57u, 65u, 90u, 97u, 122u, 32u, 9u, 
	13u, 61u, 34u, 92u, 34u, 10u, 38u, 61u, 
	39u, 92u, 39u, 42u, 61u, 61u, 62u, 46u, 
	48u, 57u, 69u, 74u, 95u, 101u, 106u, 48u, 
	57u, 74u, 95u, 106u, 48u, 57u, 47u, 61u, 
	46u, 66u, 69u, 74u, 79u, 88u, 95u, 98u, 
	101u, 106u, 111u, 120u, 48u, 57u, 69u, 74u, 
	101u, 106u, 48u, 57u, 69u, 74u, 95u, 101u, 
	106u, 48u, 57u, 74u, 95u, 106u, 48u, 57u, 
	46u, 69u, 74u, 95u, 101u, 106u, 48u, 57u, 
	74u, 95u, 106u, 48u, 57u, 95u, 48u, 49u, 
	95u, 48u, 55u, 95u, 48u, 57u, 65u, 70u, 
	97u, 102u, 60u, 61u, 61u, 62u, 61u, 95u, 
	65u, 90u, 97u, 122u, 46u, 95u, 48u, 57u, 
	65u, 90u, 97u, 122u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 34u, 39u, 66u, 70u, 82u, 
	85u, 95u, 98u, 102u, 114u, 117u, 48u, 57u, 
	65u, 90u, 97u, 122u, 34u, 39u, 95u, 48u, 
	57u, 65u, 90u, 97u, 122u, 61u, 124u, 0
};

static const char _PYT_single_lengths[] = {
	2, 0, 1, 1, 1, 2, 0, 1, 
	1, 1, 1, 2, 0, 0, 2, 0, 
	0, 2, 0, 0, 0, 0, 0, 2, 
	2, 29, 1, 1, 2, 1, 1, 2, 
	2, 1, 2, 0, 1, 5, 3, 2, 
	12, 4, 5, 3, 6, 3, 1, 1, 
	1, 2, 2, 2, 2, 1, 11, 3, 
	2
};

static const char _PYT_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 3, 0, 
	0, 4, 1, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 1, 1, 1, 0, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	3, 0, 0, 2, 3, 3, 3, 3, 
	0
};

static const short _PYT_index_offsets[] = {
	0, 3, 4, 6, 8, 10, 13, 14, 
	16, 18, 20, 22, 26, 28, 30, 34, 
	36, 38, 42, 44, 46, 48, 50, 54, 
	57, 60, 94, 97, 99, 102, 104, 106, 
	109, 112, 114, 117, 119, 122, 129, 134, 
	137, 151, 157, 164, 169, 177, 182, 185, 
	188, 193, 196, 199, 204, 210, 215, 230, 
	237
};

static const char _PYT_indicies[] = {
	2, 3, 1, 1, 6, 5, 7, 5, 
	8, 5, 10, 11, 9, 9, 14, 13, 
	15, 13, 16, 13, 18, 17, 20, 20, 
	21, 19, 21, 19, 22, 19, 24, 24, 
	25, 23, 25, 23, 26, 23, 28, 28, 
	29, 27, 29, 0, 30, 27, 31, 0, 
	32, 0, 33, 33, 33, 0, 35, 3, 
	1, 36, 11, 9, 38, 39, 40, 41, 
	39, 42, 43, 44, 39, 45, 46, 47, 
	48, 39, 49, 39, 50, 51, 53, 53, 
	53, 53, 39, 52, 53, 53, 53, 53, 
	54, 38, 30, 52, 52, 37, 38, 38, 
	55, 18, 0, 35, 3, 1, 5, 57, 
	58, 41, 18, 18, 56, 36, 11, 9, 
	13, 59, 60, 18, 56, 18, 56, 62, 
	22, 61, 64, 65, 66, 64, 65, 22, 
	63, 65, 20, 65, 21, 63, 60, 18, 
	56, 68, 69, 70, 71, 72, 73, 74, 
	69, 70, 71, 72, 73, 30, 67, 76, 
	77, 76, 77, 26, 75, 76, 77, 78, 
	76, 77, 26, 75, 77, 24, 77, 25, 
	75, 68, 70, 71, 74, 70, 71, 30, 
	67, 80, 28, 80, 29, 79, 69, 31, 
	81, 72, 32, 82, 73, 33, 33, 33, 
	83, 60, 18, 56, 18, 60, 56, 18, 
	84, 84, 84, 56, 84, 84, 84, 84, 
	84, 85, 52, 52, 52, 52, 86, 87, 
	88, 89, 89, 89, 89, 52, 89, 89, 
	89, 89, 52, 52, 52, 86, 87, 88, 
	52, 52, 52, 52, 86, 18, 18, 56, 
	0
};

static const char _PYT_trans_targs[] = {
	25, 0, 25, 1, 25, 2, 3, 4, 
	25, 5, 25, 6, 25, 7, 8, 9, 
	25, 25, 25, 25, 12, 38, 37, 25, 
	15, 43, 42, 25, 18, 45, 44, 46, 
	47, 48, 25, 29, 33, 25, 26, 27, 
	28, 30, 31, 32, 34, 35, 36, 39, 
	40, 49, 50, 51, 53, 54, 56, 25, 
	25, 25, 25, 25, 27, 25, 10, 25, 
	11, 25, 13, 25, 41, 20, 17, 25, 
	21, 22, 19, 25, 14, 25, 16, 25, 
	25, 25, 25, 25, 52, 25, 25, 23, 
	24, 55
};

static const char _PYT_trans_actions[] = {
	71, 0, 11, 0, 57, 0, 0, 0, 
	7, 0, 13, 0, 59, 0, 0, 0, 
	9, 69, 23, 63, 0, 5, 5, 61, 
	0, 5, 5, 65, 0, 82, 85, 79, 
	76, 73, 67, 5, 5, 25, 0, 94, 
	94, 0, 0, 94, 0, 0, 5, 0, 
	85, 0, 0, 0, 0, 88, 0, 55, 
	51, 29, 27, 31, 91, 53, 0, 41, 
	0, 17, 0, 45, 5, 0, 0, 21, 
	0, 0, 0, 39, 0, 15, 0, 43, 
	19, 37, 35, 33, 0, 47, 49, 0, 
	0, 88
};

static const char _PYT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const char _PYT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 3, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const short _PYT_eof_trans[] = {
	1, 1, 5, 5, 5, 1, 1, 13, 
	13, 13, 18, 20, 20, 20, 24, 24, 
	24, 28, 1, 28, 1, 1, 1, 35, 
	35, 0, 56, 1, 57, 58, 59, 57, 
	57, 60, 57, 57, 62, 64, 64, 57, 
	68, 76, 76, 76, 68, 80, 82, 83, 
	84, 57, 57, 57, 86, 87, 87, 87, 
	57
};

static const int PYT_start = 25;
static const int PYT_first_final = 25;
static const int PYT_error = -1;

static const int PYT_en_main = 25;


/* #line 129 "PYT.c.rl" */

ok64 PYTLexer(PYTstate* state) {

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

    
/* #line 228 "PYT.rl.c" */
	{
	cs = PYT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 147 "PYT.c.rl" */
    
/* #line 234 "PYT.rl.c" */
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( p == pe )
		goto _test_eof;
_resume:
	_acts = _PYT_actions + _PYT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 251 "PYT.rl.c" */
		}
	}

	_keys = _PYT_trans_keys + _PYT_key_offsets[cs];
	_trans = _PYT_index_offsets[cs];

	_klen = _PYT_single_lengths[cs];
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

	_klen = _PYT_range_lengths[cs];
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
	_trans = _PYT_indicies[_trans];
_eof_trans:
	cs = _PYT_trans_targs[_trans];

	if ( _PYT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _PYT_actions + _PYT_trans_actions[_trans];
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
/* #line 44 "PYT.c.rl" */
	{act = 6;}
	break;
	case 4:
/* #line 44 "PYT.c.rl" */
	{act = 7;}
	break;
	case 5:
/* #line 44 "PYT.c.rl" */
	{act = 8;}
	break;
	case 6:
/* #line 44 "PYT.c.rl" */
	{act = 11;}
	break;
	case 7:
/* #line 44 "PYT.c.rl" */
	{act = 12;}
	break;
	case 8:
/* #line 56 "PYT.c.rl" */
	{act = 14;}
	break;
	case 9:
/* #line 62 "PYT.c.rl" */
	{act = 15;}
	break;
	case 10:
/* #line 62 "PYT.c.rl" */
	{act = 16;}
	break;
	case 11:
/* #line 38 "PYT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 38 "PYT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 38 "PYT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 38 "PYT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 44 "PYT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 44 "PYT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 44 "PYT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 44 "PYT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 62 "PYT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 62 "PYT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 32 "PYT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 38 "PYT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 38 "PYT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 44 "PYT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 44 "PYT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 44 "PYT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 44 "PYT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 44 "PYT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 44 "PYT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 44 "PYT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 50 "PYT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonDecorator(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 56 "PYT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 62 "PYT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 62 "PYT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 68 "PYT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 38 "PYT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 38 "PYT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 44 "PYT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 39:
/* #line 44 "PYT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 40:
/* #line 44 "PYT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 41:
/* #line 56 "PYT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 42:
/* #line 62 "PYT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 43:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 6:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 7:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 8:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 11:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 14:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 15:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 16:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 668 "PYT.rl.c" */
		}
	}

_again:
	_acts = _PYT_actions + _PYT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 679 "PYT.rl.c" */
		}
	}

	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _PYT_eof_trans[cs] > 0 ) {
		_trans = _PYT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 148 "PYT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < PYT_first_final)
        o = PYTBAD;

    return o;
}
