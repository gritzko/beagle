
/* #line 1 "SQLT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "SQLT.h"

ok64 SQLTonComment (u8cs tok, SQLTstate* state);
ok64 SQLTonString (u8cs tok, SQLTstate* state);
ok64 SQLTonNumber (u8cs tok, SQLTstate* state);
ok64 SQLTonWord (u8cs tok, SQLTstate* state);
ok64 SQLTonPunct (u8cs tok, SQLTstate* state);
ok64 SQLTonSpace (u8cs tok, SQLTstate* state);


/* #line 93 "SQLT.c.rl" */



/* #line 15 "SQLT.rl.c" */
static const char _SQLT_actions[] = {
	0, 1, 2, 1, 3, 1, 5, 1, 
	6, 1, 7, 1, 8, 1, 9, 1, 
	10, 1, 11, 1, 12, 1, 13, 1, 
	14, 1, 15, 1, 16, 1, 17, 1, 
	18, 1, 19, 1, 20, 1, 21, 1, 
	22, 2, 0, 1, 2, 3, 4
};

static const char _SQLT_key_offsets[] = {
	0, 0, 1, 2, 6, 8, 9, 11, 
	15, 17, 21, 23, 43, 46, 47, 48, 
	49, 50, 52, 56, 58, 59, 64, 68, 
	70, 72, 73, 75, 82
};

static const unsigned char _SQLT_trans_keys[] = {
	34u, 39u, 43u, 45u, 48u, 57u, 48u, 57u, 
	42u, 42u, 47u, 43u, 45u, 48u, 57u, 48u, 
	57u, 43u, 45u, 48u, 57u, 48u, 57u, 32u, 
	33u, 34u, 39u, 45u, 46u, 47u, 58u, 60u, 
	62u, 95u, 124u, 9u, 13u, 48u, 57u, 65u, 
	90u, 97u, 122u, 32u, 9u, 13u, 61u, 39u, 
	45u, 10u, 48u, 57u, 69u, 101u, 48u, 57u, 
	48u, 57u, 42u, 46u, 69u, 101u, 48u, 57u, 
	69u, 101u, 48u, 57u, 48u, 57u, 48u, 57u, 
	58u, 61u, 62u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 124u, 0
};

static const char _SQLT_single_lengths[] = {
	0, 1, 1, 2, 0, 1, 2, 2, 
	0, 2, 0, 12, 1, 1, 1, 1, 
	1, 0, 2, 0, 1, 3, 2, 0, 
	0, 1, 0, 1, 1
};

static const char _SQLT_range_lengths[] = {
	0, 0, 0, 1, 1, 0, 0, 1, 
	1, 1, 1, 4, 1, 0, 0, 0, 
	0, 1, 1, 1, 0, 1, 1, 1, 
	1, 0, 1, 3, 0
};

static const char _SQLT_index_offsets[] = {
	0, 0, 2, 4, 8, 10, 12, 15, 
	19, 21, 25, 27, 44, 47, 49, 51, 
	53, 55, 57, 61, 63, 65, 70, 74, 
	76, 78, 80, 82, 87
};

static const char _SQLT_trans_targs[] = {
	11, 1, 14, 2, 4, 4, 19, 11, 
	19, 11, 6, 5, 6, 11, 5, 8, 
	8, 23, 11, 23, 11, 10, 10, 24, 
	11, 24, 11, 12, 13, 1, 2, 15, 
	17, 20, 25, 26, 13, 27, 28, 12, 
	21, 27, 27, 11, 12, 12, 11, 11, 
	11, 2, 11, 16, 11, 11, 16, 18, 
	11, 3, 3, 18, 11, 19, 11, 5, 
	11, 22, 9, 9, 21, 11, 7, 7, 
	22, 11, 23, 11, 24, 11, 11, 11, 
	11, 11, 27, 27, 27, 27, 11, 11, 
	11, 11, 11, 11, 11, 11, 11, 11, 
	11, 11, 11, 11, 11, 11, 11, 11, 
	11, 11, 11, 11, 11, 11, 11, 11, 
	11, 11, 11, 0
};

static const char _SQLT_trans_actions[] = {
	7, 0, 44, 0, 0, 0, 0, 33, 
	0, 33, 0, 0, 0, 5, 0, 0, 
	0, 0, 31, 0, 31, 0, 0, 0, 
	35, 0, 35, 0, 0, 0, 0, 0, 
	0, 3, 0, 0, 0, 0, 0, 0, 
	3, 0, 0, 11, 0, 0, 29, 9, 
	25, 0, 15, 0, 25, 13, 0, 3, 
	27, 0, 0, 3, 19, 0, 19, 0, 
	25, 3, 0, 0, 3, 21, 0, 0, 
	3, 17, 0, 17, 0, 21, 9, 25, 
	9, 25, 0, 0, 0, 0, 23, 9, 
	25, 39, 33, 33, 37, 37, 31, 31, 
	35, 35, 29, 25, 15, 25, 13, 27, 
	19, 19, 25, 21, 17, 17, 21, 25, 
	25, 23, 25, 0
};

static const char _SQLT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 41, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0
};

static const char _SQLT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0
};

static const char _SQLT_eof_trans[] = {
	0, 0, 90, 92, 92, 94, 94, 96, 
	96, 98, 98, 0, 99, 115, 101, 115, 
	103, 104, 106, 106, 115, 111, 110, 110, 
	111, 115, 115, 114, 115
};

static const int SQLT_start = 11;
static const int SQLT_first_final = 11;
static const int SQLT_error = 0;

static const int SQLT_en_main = 11;


/* #line 96 "SQLT.c.rl" */

ok64 SQLTLexer(SQLTstate* state) {

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

    
/* #line 146 "SQLT.rl.c" */
	{
	cs = SQLT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 114 "SQLT.c.rl" */
    
/* #line 152 "SQLT.rl.c" */
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
	_acts = _SQLT_actions + _SQLT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 2:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 171 "SQLT.rl.c" */
		}
	}

	_keys = _SQLT_trans_keys + _SQLT_key_offsets[cs];
	_trans = _SQLT_index_offsets[cs];

	_klen = _SQLT_single_lengths[cs];
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

	_klen = _SQLT_range_lengths[cs];
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
_eof_trans:
	cs = _SQLT_trans_targs[_trans];

	if ( _SQLT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _SQLT_actions + _SQLT_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 3:
/* #line 1 "NONE" */
	{te = p+1;}
	break;
	case 4:
/* #line 30 "SQLT.c.rl" */
	{act = 3;}
	break;
	case 5:
/* #line 24 "SQLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 30 "SQLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 48 "SQLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 48 "SQLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 24 "SQLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 30 "SQLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 36 "SQLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 36 "SQLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 36 "SQLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 42 "SQLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 48 "SQLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 48 "SQLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 54 "SQLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 36 "SQLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 36 "SQLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 36 "SQLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 48 "SQLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 0:
	{{cs = 0;goto _again;}}
	break;
	case 3:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 393 "SQLT.rl.c" */
		}
	}

_again:
	_acts = _SQLT_actions + _SQLT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
	case 1:
/* #line 1 "NONE" */
	{act = 0;}
	break;
/* #line 407 "SQLT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _SQLT_eof_trans[cs] > 0 ) {
		_trans = _SQLT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 115 "SQLT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < SQLT_first_final)
        o = SQLTBAD;

    return o;
}
