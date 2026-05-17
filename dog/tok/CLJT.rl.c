
/* #line 1 "CLJT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "CLJT.h"

ok64 CLJTonComment (u8cs tok, CLJTstate* state);
ok64 CLJTonString (u8cs tok, CLJTstate* state);
ok64 CLJTonNumber (u8cs tok, CLJTstate* state);
ok64 CLJTonWord (u8cs tok, CLJTstate* state);
ok64 CLJTonPunct (u8cs tok, CLJTstate* state);
ok64 CLJTonSpace (u8cs tok, CLJTstate* state);


/* #line 112 "CLJT.c.rl" */



/* #line 15 "CLJT.rl.c" */
static const char _CLJT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19, 1, 20, 1, 21, 1, 22, 1, 
	23, 1, 24, 1, 25, 1, 26, 1, 
	27, 1, 28, 1, 29, 1, 30, 1, 
	31, 1, 32, 2, 2, 3, 2, 2, 
	4, 2, 2, 5, 2, 2, 6
};

static const unsigned char _CLJT_key_offsets[] = {
	0, 0, 2, 2, 4, 4, 8, 10, 
	14, 16, 18, 22, 24, 30, 44, 71, 
	75, 89, 94, 96, 102, 106, 118, 124, 
	128, 130, 140, 148, 152, 158, 172, 173
};

static const unsigned char _CLJT_trans_keys[] = {
	34u, 92u, 34u, 92u, 43u, 45u, 48u, 57u, 
	48u, 57u, 43u, 45u, 48u, 57u, 48u, 57u, 
	48u, 57u, 43u, 45u, 48u, 57u, 48u, 57u, 
	48u, 57u, 65u, 70u, 97u, 102u, 33u, 95u, 
	37u, 38u, 42u, 43u, 45u, 57u, 60u, 63u, 
	65u, 90u, 97u, 122u, 32u, 33u, 34u, 35u, 
	39u, 44u, 46u, 48u, 58u, 59u, 64u, 95u, 
	126u, 9u, 13u, 37u, 38u, 42u, 47u, 49u, 
	57u, 60u, 90u, 94u, 96u, 97u, 122u, 32u, 
	44u, 9u, 13u, 33u, 95u, 37u, 38u, 42u, 
	43u, 45u, 57u, 60u, 63u, 65u, 90u, 97u, 
	122u, 34u, 95u, 123u, 39u, 40u, 48u, 57u, 
	69u, 101u, 48u, 57u, 77u, 78u, 48u, 57u, 
	77u, 78u, 46u, 47u, 69u, 88u, 101u, 120u, 
	48u, 55u, 56u, 57u, 77u, 78u, 69u, 101u, 
	48u, 57u, 77u, 78u, 48u, 57u, 77u, 78u, 
	48u, 57u, 46u, 47u, 69u, 101u, 48u, 55u, 
	56u, 57u, 77u, 78u, 46u, 47u, 69u, 101u, 
	48u, 57u, 77u, 78u, 48u, 57u, 77u, 78u, 
	48u, 57u, 65u, 70u, 97u, 102u, 33u, 95u, 
	37u, 38u, 42u, 43u, 45u, 57u, 60u, 63u, 
	65u, 90u, 97u, 122u, 10u, 64u, 0
};

static const char _CLJT_single_lengths[] = {
	0, 2, 0, 2, 0, 2, 0, 2, 
	0, 0, 2, 0, 0, 2, 13, 2, 
	2, 3, 0, 2, 0, 6, 2, 0, 
	0, 4, 4, 0, 0, 2, 1, 1
};

static const char _CLJT_range_lengths[] = {
	0, 0, 0, 0, 0, 1, 1, 1, 
	1, 1, 1, 1, 3, 6, 7, 1, 
	6, 1, 1, 2, 2, 3, 2, 2, 
	1, 3, 2, 2, 3, 6, 0, 0
};

static const unsigned char _CLJT_index_offsets[] = {
	0, 0, 3, 4, 7, 8, 12, 14, 
	18, 20, 22, 26, 28, 32, 41, 62, 
	66, 75, 80, 82, 87, 90, 100, 105, 
	108, 110, 118, 125, 128, 132, 141, 143
};

static const char _CLJT_indicies[] = {
	1, 2, 0, 0, 5, 6, 4, 4, 
	8, 8, 9, 7, 9, 7, 11, 11, 
	12, 10, 12, 10, 14, 13, 15, 15, 
	16, 13, 16, 13, 18, 18, 18, 17, 
	19, 19, 19, 19, 19, 19, 19, 19, 
	20, 22, 23, 0, 24, 25, 26, 27, 
	28, 30, 31, 25, 23, 32, 22, 23, 
	23, 29, 23, 25, 23, 21, 22, 22, 
	22, 13, 23, 23, 23, 23, 23, 23, 
	23, 23, 33, 4, 35, 25, 25, 34, 
	36, 34, 38, 38, 36, 39, 37, 9, 
	39, 37, 41, 42, 44, 46, 44, 46, 
	43, 29, 45, 40, 48, 48, 41, 49, 
	47, 12, 49, 47, 14, 50, 41, 42, 
	44, 44, 43, 29, 45, 51, 41, 42, 
	44, 44, 29, 45, 40, 16, 53, 52, 
	18, 18, 18, 54, 19, 19, 19, 19, 
	19, 19, 19, 19, 55, 56, 31, 25, 
	57, 0
};

static const char _CLJT_trans_targs[] = {
	1, 14, 2, 14, 3, 14, 4, 14, 
	6, 20, 14, 8, 23, 14, 24, 11, 
	27, 14, 28, 29, 0, 14, 15, 16, 
	17, 14, 15, 18, 21, 26, 13, 30, 
	31, 14, 14, 14, 19, 14, 5, 14, 
	14, 22, 9, 25, 10, 14, 12, 14, 
	7, 14, 14, 14, 14, 14, 14, 14, 
	14, 14
};

static const char _CLJT_trans_actions[] = {
	0, 9, 0, 55, 0, 11, 0, 51, 
	0, 0, 49, 0, 0, 57, 0, 0, 
	0, 53, 0, 0, 0, 23, 68, 0, 
	5, 21, 65, 0, 62, 62, 0, 0, 
	0, 43, 47, 7, 5, 35, 0, 15, 
	39, 5, 0, 59, 0, 19, 0, 33, 
	0, 13, 31, 29, 37, 17, 27, 41, 
	25, 45
};

static const char _CLJT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 1, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0
};

static const char _CLJT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 3, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char _CLJT_eof_trans[] = {
	0, 0, 0, 4, 4, 8, 8, 11, 
	11, 14, 14, 14, 18, 0, 0, 14, 
	34, 35, 35, 38, 38, 41, 48, 48, 
	51, 52, 41, 53, 55, 56, 57, 58
};

static const int CLJT_start = 14;
static const int CLJT_first_final = 14;
static const int CLJT_error = 0;

static const int CLJT_en_main = 14;


/* #line 115 "CLJT.c.rl" */

ok64 CLJTLexer(CLJTstate* state) {

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

    
/* #line 168 "CLJT.rl.c" */
	{
	cs = CLJT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 133 "CLJT.c.rl" */
    
/* #line 174 "CLJT.rl.c" */
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
	_acts = _CLJT_actions + _CLJT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 193 "CLJT.rl.c" */
		}
	}

	_keys = _CLJT_trans_keys + _CLJT_key_offsets[cs];
	_trans = _CLJT_index_offsets[cs];

	_klen = _CLJT_single_lengths[cs];
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

	_klen = _CLJT_range_lengths[cs];
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
	_trans = _CLJT_indicies[_trans];
_eof_trans:
	cs = _CLJT_trans_targs[_trans];

	if ( _CLJT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _CLJT_actions + _CLJT_trans_actions[_trans];
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
/* #line 42 "CLJT.c.rl" */
	{act = 6;}
	break;
	case 4:
/* #line 42 "CLJT.c.rl" */
	{act = 11;}
	break;
	case 5:
/* #line 54 "CLJT.c.rl" */
	{act = 15;}
	break;
	case 6:
/* #line 60 "CLJT.c.rl" */
	{act = 17;}
	break;
	case 7:
/* #line 30 "CLJT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 36 "CLJT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 36 "CLJT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 42 "CLJT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 42 "CLJT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 42 "CLJT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 42 "CLJT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 54 "CLJT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 54 "CLJT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 30 "CLJT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 42 "CLJT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 42 "CLJT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 42 "CLJT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 42 "CLJT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 42 "CLJT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 42 "CLJT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 42 "CLJT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 36 "CLJT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 48 "CLJT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 54 "CLJT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 54 "CLJT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 42 "CLJT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 42 "CLJT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 42 "CLJT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 54 "CLJT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 6:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 11:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 15:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 17:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 510 "CLJT.rl.c" */
		}
	}

_again:
	_acts = _CLJT_actions + _CLJT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 521 "CLJT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _CLJT_eof_trans[cs] > 0 ) {
		_trans = _CLJT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 134 "CLJT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < CLJT_first_final)
        o = CLJTBAD;

    return o;
}
