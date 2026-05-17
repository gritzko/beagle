
/* #line 1 "GQLT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "GQLT.h"

ok64 GQLTonComment (u8cs tok, GQLTstate* state);
ok64 GQLTonString (u8cs tok, GQLTstate* state);
ok64 GQLTonNumber (u8cs tok, GQLTstate* state);
ok64 GQLTonWord (u8cs tok, GQLTstate* state);
ok64 GQLTonPunct (u8cs tok, GQLTstate* state);
ok64 GQLTonSpace (u8cs tok, GQLTstate* state);


/* #line 94 "GQLT.c.rl" */



/* #line 15 "GQLT.rl.c" */
static const char _GQLT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19, 1, 20, 1, 21, 1, 22
};

static const char _GQLT_key_offsets[] = {
	0, 0, 3, 6, 15, 21, 27, 33, 
	39, 40, 41, 42, 44, 48, 50, 54, 
	56, 60, 62, 63, 77, 80, 81, 82, 
	83, 84, 87, 91, 93, 98, 102, 104, 
	106, 109
};

static const unsigned char _GQLT_trans_keys[] = {
	10u, 34u, 92u, 10u, 34u, 92u, 34u, 47u, 
	92u, 98u, 102u, 110u, 114u, 116u, 117u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 34u, 
	34u, 34u, 48u, 57u, 43u, 45u, 48u, 57u, 
	48u, 57u, 43u, 45u, 48u, 57u, 48u, 57u, 
	43u, 45u, 48u, 57u, 48u, 57u, 46u, 32u, 
	34u, 35u, 45u, 46u, 95u, 9u, 13u, 48u, 
	57u, 65u, 90u, 97u, 122u, 32u, 9u, 13u, 
	34u, 34u, 34u, 10u, 46u, 48u, 57u, 69u, 
	101u, 48u, 57u, 48u, 57u, 46u, 69u, 101u, 
	48u, 57u, 69u, 101u, 48u, 57u, 48u, 57u, 
	48u, 57u, 46u, 48u, 57u, 95u, 48u, 57u, 
	65u, 90u, 97u, 122u, 0
};

static const char _GQLT_single_lengths[] = {
	0, 3, 3, 9, 0, 0, 0, 0, 
	1, 1, 1, 0, 2, 0, 2, 0, 
	2, 0, 1, 6, 1, 1, 1, 1, 
	1, 1, 2, 0, 3, 2, 0, 0, 
	1, 1
};

static const char _GQLT_range_lengths[] = {
	0, 0, 0, 0, 3, 3, 3, 3, 
	0, 0, 0, 1, 1, 1, 1, 1, 
	1, 1, 0, 4, 1, 0, 0, 0, 
	0, 1, 1, 1, 1, 1, 1, 1, 
	1, 3
};

static const unsigned char _GQLT_index_offsets[] = {
	0, 0, 4, 8, 18, 22, 26, 30, 
	34, 36, 38, 40, 42, 46, 48, 52, 
	54, 58, 60, 62, 73, 76, 78, 80, 
	82, 84, 87, 91, 93, 98, 102, 104, 
	106, 109
};

static const char _GQLT_indicies[] = {
	1, 2, 3, 0, 1, 4, 3, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	5, 1, 6, 6, 6, 1, 7, 7, 
	7, 1, 8, 8, 8, 1, 0, 0, 
	0, 1, 11, 10, 12, 10, 13, 10, 
	15, 14, 17, 17, 18, 16, 18, 16, 
	20, 20, 21, 19, 21, 19, 23, 23, 
	24, 22, 24, 22, 26, 25, 28, 29, 
	30, 31, 32, 34, 28, 33, 34, 34, 
	27, 28, 28, 35, 10, 36, 38, 37, 
	39, 37, 40, 30, 42, 33, 41, 44, 
	44, 15, 43, 18, 43, 46, 47, 47, 
	33, 45, 49, 49, 46, 48, 21, 48, 
	24, 45, 51, 15, 50, 34, 34, 34, 
	34, 52, 0
};

static const char _GQLT_trans_targs[] = {
	2, 0, 21, 3, 19, 4, 5, 6, 
	7, 19, 8, 9, 10, 22, 19, 26, 
	19, 13, 27, 19, 15, 30, 19, 17, 
	31, 19, 19, 19, 20, 1, 24, 25, 
	32, 28, 33, 19, 19, 19, 23, 19, 
	19, 19, 11, 19, 12, 19, 29, 16, 
	19, 14, 19, 18, 19
};

static const char _GQLT_trans_actions[] = {
	0, 0, 5, 0, 9, 0, 0, 0, 
	0, 35, 0, 0, 0, 0, 43, 5, 
	39, 0, 0, 37, 0, 0, 41, 0, 
	0, 45, 11, 13, 0, 0, 0, 5, 
	5, 5, 0, 33, 19, 17, 0, 7, 
	15, 29, 0, 23, 0, 25, 5, 0, 
	21, 0, 31, 0, 27
};

static const char _GQLT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const char _GQLT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 3, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const unsigned char _GQLT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	10, 10, 10, 15, 17, 17, 20, 20, 
	23, 23, 26, 0, 36, 37, 38, 38, 
	41, 42, 44, 44, 46, 49, 49, 46, 
	51, 53
};

static const int GQLT_start = 19;
static const int GQLT_first_final = 19;
static const int GQLT_error = 0;

static const int GQLT_en_main = 19;


/* #line 97 "GQLT.c.rl" */

ok64 GQLTLexer(GQLTstate* state) {

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

    
/* #line 159 "GQLT.rl.c" */
	{
	cs = GQLT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 115 "GQLT.c.rl" */
    
/* #line 165 "GQLT.rl.c" */
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
	_acts = _GQLT_actions + _GQLT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 184 "GQLT.rl.c" */
		}
	}

	_keys = _GQLT_trans_keys + _GQLT_key_offsets[cs];
	_trans = _GQLT_index_offsets[cs];

	_klen = _GQLT_single_lengths[cs];
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

	_klen = _GQLT_range_lengths[cs];
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
	_trans = _GQLT_indicies[_trans];
_eof_trans:
	cs = _GQLT_trans_targs[_trans];

	if ( _GQLT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _GQLT_actions + _GQLT_trans_actions[_trans];
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
/* #line 32 "GQLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 4:
/* #line 32 "GQLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 5:
/* #line 50 "GQLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 50 "GQLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 26 "GQLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 32 "GQLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 32 "GQLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 38 "GQLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 38 "GQLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 38 "GQLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 44 "GQLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 50 "GQLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 50 "GQLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 56 "GQLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 32 "GQLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 38 "GQLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 38 "GQLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 38 "GQLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 50 "GQLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 50 "GQLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
/* #line 412 "GQLT.rl.c" */
		}
	}

_again:
	_acts = _GQLT_actions + _GQLT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 423 "GQLT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _GQLT_eof_trans[cs] > 0 ) {
		_trans = _GQLT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 116 "GQLT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < GQLT_first_final)
        o = GQLTBAD;

    return o;
}
