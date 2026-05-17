
/* #line 1 "VERT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "VERT.h"

ok64 VERTonComment (u8cs tok, VERTstate* state);
ok64 VERTonString (u8cs tok, VERTstate* state);
ok64 VERTonNumber (u8cs tok, VERTstate* state);
ok64 VERTonPreproc (u8cs tok, VERTstate* state);
ok64 VERTonWord (u8cs tok, VERTstate* state);
ok64 VERTonPunct (u8cs tok, VERTstate* state);
ok64 VERTonSpace (u8cs tok, VERTstate* state);


/* #line 117 "VERT.c.rl" */



/* #line 16 "VERT.rl.c" */
static const char _VERT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19
};

static const unsigned char _VERT_key_offsets[] = {
	0, 0, 2, 17, 23, 35, 43, 44, 
	46, 56, 68, 76, 78, 82, 84, 85, 
	91, 120, 123, 124, 126, 127, 135, 143, 
	151, 152, 154, 164, 176, 177, 180, 183, 
	187, 190, 191, 197, 209, 213, 215, 216, 
	219, 221, 223, 225, 227, 235
};

static const unsigned char _VERT_trans_keys[] = {
	34u, 92u, 34u, 39u, 63u, 92u, 110u, 114u, 
	116u, 118u, 120u, 48u, 55u, 97u, 98u, 101u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 63u, 
	88u, 90u, 95u, 120u, 122u, 48u, 57u, 65u, 
	70u, 97u, 102u, 66u, 68u, 72u, 79u, 98u, 
	100u, 104u, 111u, 42u, 42u, 47u, 66u, 68u, 
	72u, 79u, 83u, 98u, 100u, 104u, 111u, 115u, 
	63u, 88u, 90u, 95u, 120u, 122u, 48u, 57u, 
	65u, 70u, 97u, 102u, 66u, 68u, 72u, 79u, 
	98u, 100u, 104u, 111u, 48u, 57u, 43u, 45u, 
	48u, 57u, 48u, 57u, 62u, 36u, 95u, 65u, 
	90u, 97u, 122u, 32u, 33u, 34u, 35u, 36u, 
	37u, 38u, 39u, 40u, 42u, 43u, 45u, 46u, 
	47u, 58u, 60u, 61u, 62u, 94u, 96u, 124u, 
	9u, 13u, 48u, 57u, 65u, 90u, 95u, 122u, 
	32u, 9u, 13u, 61u, 61u, 63u, 35u, 36u, 
	95u, 48u, 57u, 65u, 90u, 97u, 122u, 36u, 
	95u, 48u, 57u, 65u, 90u, 97u, 122u, 36u, 
	95u, 48u, 57u, 65u, 90u, 97u, 122u, 61u, 
	38u, 61u, 66u, 68u, 72u, 79u, 83u, 98u, 
	100u, 104u, 111u, 115u, 63u, 88u, 90u, 95u, 
	120u, 122u, 48u, 57u, 65u, 70u, 97u, 102u, 
	42u, 41u, 61u, 62u, 43u, 58u, 61u, 45u, 
	58u, 61u, 62u, 42u, 47u, 61u, 10u, 39u, 
	46u, 69u, 101u, 48u, 57u, 63u, 88u, 90u, 
	95u, 120u, 122u, 48u, 57u, 65u, 70u, 97u, 
	102u, 69u, 101u, 48u, 57u, 48u, 57u, 58u, 
	45u, 60u, 61u, 60u, 61u, 61u, 62u, 61u, 
	62u, 61u, 62u, 36u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 61u, 124u, 0
};

static const char _VERT_single_lengths[] = {
	0, 2, 9, 0, 6, 8, 1, 2, 
	10, 6, 8, 0, 2, 0, 1, 2, 
	21, 1, 1, 2, 1, 2, 2, 2, 
	1, 2, 10, 6, 1, 1, 3, 2, 
	3, 1, 4, 6, 2, 0, 1, 3, 
	0, 2, 2, 0, 2, 2
};

static const char _VERT_range_lengths[] = {
	0, 0, 3, 3, 3, 0, 0, 0, 
	0, 3, 0, 1, 1, 1, 0, 2, 
	4, 1, 0, 0, 0, 3, 3, 3, 
	0, 0, 0, 3, 0, 1, 0, 1, 
	0, 0, 1, 3, 1, 1, 0, 0, 
	1, 0, 0, 1, 3, 0
};

static const short _VERT_index_offsets[] = {
	0, 0, 3, 16, 20, 30, 39, 41, 
	44, 55, 65, 74, 76, 80, 82, 84, 
	89, 115, 118, 120, 123, 125, 131, 137, 
	143, 145, 148, 159, 169, 171, 174, 178, 
	182, 186, 188, 194, 204, 208, 210, 212, 
	216, 218, 221, 224, 226, 232
};

static const char _VERT_indicies[] = {
	1, 2, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 4, 0, 0, 0, 3, 
	0, 0, 0, 3, 6, 6, 6, 6, 
	6, 6, 6, 6, 6, 5, 7, 7, 
	7, 7, 7, 7, 7, 7, 5, 9, 
	8, 9, 10, 8, 12, 12, 12, 12, 
	13, 12, 12, 12, 12, 13, 11, 14, 
	14, 14, 14, 14, 14, 14, 14, 14, 
	11, 12, 12, 12, 12, 12, 12, 12, 
	12, 11, 15, 11, 16, 16, 17, 11, 
	17, 11, 18, 5, 19, 19, 19, 19, 
	3, 21, 22, 0, 23, 24, 25, 26, 
	27, 28, 29, 30, 31, 32, 33, 35, 
	36, 37, 38, 25, 40, 41, 21, 34, 
	39, 39, 20, 21, 21, 42, 44, 43, 
	18, 18, 45, 18, 43, 47, 47, 39, 
	47, 47, 46, 47, 47, 47, 47, 47, 
	48, 39, 39, 39, 39, 39, 46, 18, 
	43, 18, 18, 43, 7, 7, 7, 7, 
	49, 7, 7, 7, 7, 49, 43, 6, 
	6, 6, 6, 6, 6, 6, 6, 6, 
	50, 18, 43, 18, 18, 43, 18, 18, 
	18, 43, 18, 18, 18, 43, 8, 51, 
	18, 43, 52, 51, 54, 55, 56, 56, 
	34, 53, 14, 14, 14, 14, 14, 14, 
	14, 14, 14, 57, 56, 56, 15, 53, 
	17, 53, 18, 43, 58, 59, 18, 43, 
	18, 45, 44, 18, 43, 18, 60, 43, 
	18, 45, 19, 19, 19, 19, 19, 61, 
	18, 18, 43, 0
};

static const char _VERT_trans_targs[] = {
	1, 16, 2, 0, 3, 16, 27, 4, 
	6, 7, 16, 16, 9, 10, 35, 36, 
	13, 37, 16, 44, 16, 17, 18, 20, 
	21, 24, 25, 26, 28, 29, 30, 31, 
	16, 32, 34, 38, 39, 41, 42, 23, 
	15, 45, 16, 16, 19, 16, 16, 22, 
	16, 5, 16, 33, 16, 16, 8, 11, 
	12, 16, 14, 40, 43, 16
};

static const char _VERT_trans_actions[] = {
	0, 9, 0, 0, 0, 39, 0, 0, 
	0, 0, 7, 37, 0, 0, 0, 5, 
	0, 0, 11, 0, 13, 0, 0, 0, 
	0, 0, 0, 5, 0, 0, 0, 0, 
	15, 5, 5, 0, 5, 0, 0, 0, 
	0, 0, 35, 33, 0, 31, 29, 0, 
	27, 0, 21, 0, 17, 23, 0, 0, 
	0, 19, 0, 0, 0, 25
};

static const char _VERT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	1, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0
};

static const char _VERT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	3, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0
};

static const short _VERT_eof_trans[] = {
	0, 0, 0, 0, 6, 6, 6, 6, 
	12, 12, 12, 12, 12, 12, 6, 0, 
	0, 43, 44, 46, 44, 47, 49, 47, 
	44, 44, 44, 51, 44, 44, 44, 44, 
	44, 53, 54, 58, 54, 54, 44, 44, 
	46, 44, 44, 46, 62, 44
};

static const int VERT_start = 16;
static const int VERT_first_final = 16;
static const int VERT_error = 0;

static const int VERT_en_main = 16;


/* #line 120 "VERT.c.rl" */

ok64 VERTLexer(VERTstate* state) {

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

    
/* #line 199 "VERT.rl.c" */
	{
	cs = VERT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 138 "VERT.c.rl" */
    
/* #line 205 "VERT.rl.c" */
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
	_acts = _VERT_actions + _VERT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 224 "VERT.rl.c" */
		}
	}

	_keys = _VERT_trans_keys + _VERT_key_offsets[cs];
	_trans = _VERT_index_offsets[cs];

	_klen = _VERT_single_lengths[cs];
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

	_klen = _VERT_range_lengths[cs];
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
	_trans = _VERT_indicies[_trans];
_eof_trans:
	cs = _VERT_trans_targs[_trans];

	if ( _VERT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _VERT_actions + _VERT_trans_actions[_trans];
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
/* #line 31 "VERT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 4:
/* #line 37 "VERT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 5:
/* #line 61 "VERT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 61 "VERT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 61 "VERT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 31 "VERT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 43 "VERT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 43 "VERT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 43 "VERT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 49 "VERT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 55 "VERT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 55 "VERT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 61 "VERT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 61 "VERT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 67 "VERT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 43 "VERT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 61 "VERT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
/* #line 428 "VERT.rl.c" */
		}
	}

_again:
	_acts = _VERT_actions + _VERT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 439 "VERT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _VERT_eof_trans[cs] > 0 ) {
		_trans = _VERT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 139 "VERT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < VERT_first_final)
        o = VERTBAD;

    return o;
}
