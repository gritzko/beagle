
/* #line 1 "HTMT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "HTMT.h"

ok64 HTMTonComment (u8cs tok, HTMTstate* state);
ok64 HTMTonString (u8cs tok, HTMTstate* state);
ok64 HTMTonTag (u8cs tok, HTMTstate* state);
ok64 HTMTonPunct (u8cs tok, HTMTstate* state);
ok64 HTMTonText (u8cs tok, HTMTstate* state);
ok64 HTMTonSpace (u8cs tok, HTMTstate* state);


/* #line 103 "HTMT.c.rl" */



/* #line 15 "HTMT.rl.c" */
static const char _HTMT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18
};

static const unsigned char _HTMT_key_offsets[] = {
	0, 0, 1, 6, 10, 13, 19, 26, 
	31, 32, 35, 36, 37, 38, 39, 41, 
	43, 45, 47, 49, 51, 53, 58, 67, 
	69, 71, 86, 99, 102, 116, 123, 133
};

static const unsigned char _HTMT_trans_keys[] = {
	34u, 35u, 65u, 90u, 97u, 122u, 88u, 120u, 
	48u, 57u, 59u, 48u, 57u, 48u, 57u, 65u, 
	70u, 97u, 102u, 59u, 48u, 57u, 65u, 70u, 
	97u, 102u, 59u, 65u, 90u, 97u, 122u, 39u, 
	45u, 68u, 100u, 45u, 45u, 45u, 62u, 79u, 
	111u, 67u, 99u, 84u, 116u, 89u, 121u, 80u, 
	112u, 69u, 101u, 10u, 62u, 95u, 65u, 90u, 
	97u, 122u, 45u, 62u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 47u, 62u, 47u, 62u, 32u, 
	34u, 38u, 39u, 47u, 60u, 61u, 62u, 95u, 
	9u, 13u, 65u, 90u, 97u, 122u, 32u, 34u, 
	95u, 9u, 13u, 38u, 39u, 60u, 62u, 65u, 
	90u, 97u, 122u, 32u, 9u, 13u, 32u, 34u, 
	62u, 95u, 9u, 13u, 38u, 39u, 60u, 61u, 
	65u, 90u, 97u, 122u, 33u, 47u, 95u, 65u, 
	90u, 97u, 122u, 45u, 47u, 62u, 95u, 48u, 
	57u, 65u, 90u, 97u, 122u, 45u, 95u, 48u, 
	57u, 65u, 90u, 97u, 122u, 0
};

static const char _HTMT_single_lengths[] = {
	0, 1, 1, 2, 1, 0, 1, 1, 
	1, 3, 1, 1, 1, 1, 2, 2, 
	2, 2, 2, 2, 2, 1, 3, 2, 
	2, 9, 3, 1, 4, 3, 4, 2
};

static const char _HTMT_range_lengths[] = {
	0, 0, 2, 1, 1, 3, 3, 2, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 2, 3, 0, 
	0, 3, 5, 1, 5, 2, 3, 3
};

static const unsigned char _HTMT_index_offsets[] = {
	0, 0, 2, 6, 10, 13, 17, 22, 
	26, 28, 32, 34, 36, 38, 40, 43, 
	46, 49, 52, 55, 58, 61, 65, 72, 
	75, 78, 91, 100, 103, 113, 119, 127
};

static const char _HTMT_indicies[] = {
	1, 0, 2, 4, 4, 3, 6, 6, 
	5, 3, 7, 5, 3, 8, 8, 8, 
	3, 7, 8, 8, 8, 3, 7, 4, 
	4, 3, 10, 9, 12, 13, 13, 11, 
	14, 11, 15, 14, 16, 14, 17, 14, 
	18, 18, 11, 19, 19, 11, 20, 20, 
	11, 21, 21, 11, 22, 22, 11, 23, 
	23, 11, 11, 24, 23, 25, 25, 25, 
	11, 25, 26, 25, 25, 25, 25, 11, 
	29, 27, 28, 29, 30, 28, 32, 0, 
	33, 9, 34, 35, 36, 37, 38, 32, 
	38, 38, 31, 39, 39, 39, 39, 39, 
	39, 39, 39, 31, 32, 32, 40, 41, 
	41, 37, 41, 41, 41, 41, 41, 41, 
	31, 42, 43, 44, 44, 44, 41, 44, 
	29, 45, 44, 44, 44, 44, 28, 38, 
	38, 38, 38, 38, 46, 0
};

static const char _HTMT_trans_targs[] = {
	1, 25, 3, 0, 7, 4, 5, 25, 
	6, 8, 25, 25, 10, 14, 11, 12, 
	13, 25, 15, 16, 17, 18, 19, 20, 
	25, 22, 25, 25, 23, 24, 25, 26, 
	27, 2, 28, 29, 25, 25, 31, 25, 
	25, 25, 9, 21, 30, 25, 25
};

static const char _HTMT_trans_actions[] = {
	0, 15, 0, 0, 0, 0, 0, 21, 
	0, 0, 17, 37, 0, 0, 0, 0, 
	0, 7, 0, 0, 0, 0, 0, 0, 
	9, 0, 11, 35, 0, 0, 13, 0, 
	0, 0, 0, 5, 23, 19, 0, 31, 
	33, 29, 0, 0, 5, 25, 27
};

static const char _HTMT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 0, 0, 0, 0, 0, 0
};

static const char _HTMT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 3, 0, 0, 0, 0, 0, 0
};

static const unsigned char _HTMT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 12, 12, 12, 12, 12, 12, 12, 
	12, 12, 12, 12, 12, 12, 12, 28, 
	28, 0, 40, 41, 42, 42, 46, 47
};

static const int HTMT_start = 25;
static const int HTMT_first_final = 25;
static const int HTMT_error = 0;

static const int HTMT_en_main = 25;


/* #line 106 "HTMT.c.rl" */

ok64 HTMTLexer(HTMTstate* state) {

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

    
/* #line 154 "HTMT.rl.c" */
	{
	cs = HTMT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 124 "HTMT.c.rl" */
    
/* #line 160 "HTMT.rl.c" */
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
	_acts = _HTMT_actions + _HTMT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 179 "HTMT.rl.c" */
		}
	}

	_keys = _HTMT_trans_keys + _HTMT_key_offsets[cs];
	_trans = _HTMT_index_offsets[cs];

	_klen = _HTMT_single_lengths[cs];
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

	_klen = _HTMT_range_lengths[cs];
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
	_trans = _HTMT_indicies[_trans];
_eof_trans:
	cs = _HTMT_trans_targs[_trans];

	if ( _HTMT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _HTMT_actions + _HTMT_trans_actions[_trans];
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
/* #line 25 "HTMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 4:
/* #line 37 "HTMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonTag(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 5:
/* #line 37 "HTMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonTag(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 37 "HTMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonTag(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 31 "HTMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 31 "HTMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 43 "HTMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 43 "HTMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 43 "HTMT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 37 "HTMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonTag(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 49 "HTMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonText(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 43 "HTMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 49 "HTMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonText(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 55 "HTMT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 37 "HTMT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonTag(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 43 "HTMT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
/* #line 375 "HTMT.rl.c" */
		}
	}

_again:
	_acts = _HTMT_actions + _HTMT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 386 "HTMT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _HTMT_eof_trans[cs] > 0 ) {
		_trans = _HTMT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 125 "HTMT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < HTMT_first_final)
        o = HTMTBAD;

    return o;
}
