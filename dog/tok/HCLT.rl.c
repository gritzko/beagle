
/* #line 1 "HCLT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "HCLT.h"

ok64 HCLTonComment (u8cs tok, HCLTstate* state);
ok64 HCLTonString (u8cs tok, HCLTstate* state);
ok64 HCLTonNumber (u8cs tok, HCLTstate* state);
ok64 HCLTonWord (u8cs tok, HCLTstate* state);
ok64 HCLTonPunct (u8cs tok, HCLTstate* state);
ok64 HCLTonSpace (u8cs tok, HCLTstate* state);


/* #line 93 "HCLT.c.rl" */



/* #line 15 "HCLT.rl.c" */
static const char _HCLT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19, 1, 20
};

static const unsigned char _HCLT_key_offsets[] = {
	0, 0, 2, 9, 15, 21, 27, 33, 
	39, 45, 51, 57, 58, 62, 64, 65, 
	67, 71, 73, 77, 79, 99, 102, 103, 
	104, 105, 108, 112, 114, 116, 117, 122, 
	126, 128, 130, 132, 139
};

static const unsigned char _HCLT_trans_keys[] = {
	34u, 92u, 34u, 85u, 92u, 110u, 114u, 116u, 
	117u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 46u, 43u, 45u, 48u, 57u, 48u, 57u, 
	42u, 42u, 47u, 43u, 45u, 48u, 57u, 48u, 
	57u, 43u, 45u, 48u, 57u, 48u, 57u, 32u, 
	33u, 34u, 35u, 38u, 46u, 47u, 61u, 95u, 
	124u, 9u, 13u, 48u, 57u, 60u, 62u, 65u, 
	90u, 97u, 122u, 32u, 9u, 13u, 61u, 10u, 
	38u, 46u, 48u, 57u, 69u, 101u, 48u, 57u, 
	48u, 57u, 42u, 47u, 10u, 46u, 69u, 101u, 
	48u, 57u, 69u, 101u, 48u, 57u, 48u, 57u, 
	48u, 57u, 61u, 62u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 124u, 0
};

static const char _HCLT_single_lengths[] = {
	0, 2, 7, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 2, 0, 1, 2, 
	2, 0, 2, 0, 10, 1, 1, 1, 
	1, 1, 2, 0, 2, 1, 3, 2, 
	0, 0, 0, 1, 1
};

static const char _HCLT_range_lengths[] = {
	0, 0, 0, 3, 3, 3, 3, 3, 
	3, 3, 3, 0, 1, 1, 0, 0, 
	1, 1, 1, 1, 5, 1, 0, 0, 
	0, 1, 1, 1, 0, 0, 1, 1, 
	1, 1, 1, 3, 0
};

static const unsigned char _HCLT_index_offsets[] = {
	0, 0, 3, 11, 15, 19, 23, 27, 
	31, 35, 39, 43, 45, 49, 51, 53, 
	56, 60, 62, 66, 68, 84, 87, 89, 
	91, 93, 96, 100, 102, 105, 107, 112, 
	116, 118, 120, 122, 127
};

static const char _HCLT_indicies[] = {
	1, 2, 0, 0, 4, 0, 0, 0, 
	0, 5, 3, 6, 6, 6, 3, 7, 
	7, 7, 3, 8, 8, 8, 3, 5, 
	5, 5, 3, 9, 9, 9, 3, 10, 
	10, 10, 3, 11, 11, 11, 3, 0, 
	0, 0, 3, 13, 12, 15, 15, 16, 
	14, 16, 14, 19, 18, 19, 20, 18, 
	22, 22, 23, 21, 23, 21, 25, 25, 
	26, 24, 26, 24, 28, 29, 0, 30, 
	31, 32, 33, 35, 36, 37, 28, 34, 
	29, 36, 36, 27, 28, 28, 38, 13, 
	39, 40, 30, 13, 39, 42, 43, 41, 
	45, 45, 43, 44, 16, 44, 18, 46, 
	39, 47, 46, 49, 50, 50, 34, 48, 
	52, 52, 49, 51, 23, 51, 26, 48, 
	13, 39, 36, 36, 36, 36, 53, 13, 
	39, 0
};

static const char _HCLT_trans_targs[] = {
	1, 20, 2, 0, 3, 7, 4, 5, 
	6, 8, 9, 10, 20, 20, 20, 13, 
	27, 20, 14, 15, 20, 20, 17, 32, 
	20, 19, 33, 20, 21, 22, 23, 24, 
	25, 28, 30, 34, 35, 36, 20, 20, 
	20, 20, 11, 26, 20, 12, 29, 20, 
	20, 31, 18, 20, 16, 20
};

static const char _HCLT_trans_actions[] = {
	0, 9, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 41, 11, 35, 0, 
	0, 39, 0, 0, 7, 33, 0, 0, 
	37, 0, 0, 13, 0, 0, 0, 0, 
	5, 5, 5, 0, 0, 0, 31, 27, 
	15, 29, 0, 5, 21, 0, 0, 17, 
	23, 5, 0, 19, 0, 25
};

static const char _HCLT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 1, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0
};

static const char _HCLT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 3, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0
};

static const unsigned char _HCLT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 13, 15, 15, 18, 18, 
	22, 22, 25, 25, 0, 39, 40, 41, 
	40, 42, 45, 45, 40, 48, 49, 52, 
	52, 49, 40, 54, 40
};

static const int HCLT_start = 20;
static const int HCLT_first_final = 20;
static const int HCLT_error = 0;

static const int HCLT_en_main = 20;


/* #line 96 "HCLT.c.rl" */

ok64 HCLTLexer(HCLTstate* state) {

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

    
/* #line 164 "HCLT.rl.c" */
	{
	cs = HCLT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 114 "HCLT.c.rl" */
    
/* #line 170 "HCLT.rl.c" */
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
	_acts = _HCLT_actions + _HCLT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 189 "HCLT.rl.c" */
		}
	}

	_keys = _HCLT_trans_keys + _HCLT_key_offsets[cs];
	_trans = _HCLT_index_offsets[cs];

	_klen = _HCLT_single_lengths[cs];
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

	_klen = _HCLT_range_lengths[cs];
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
	_trans = _HCLT_indicies[_trans];
_eof_trans:
	cs = _HCLT_trans_targs[_trans];

	if ( _HCLT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _HCLT_actions + _HCLT_trans_actions[_trans];
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
/* #line 26 "HCLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 4:
/* #line 32 "HCLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 5:
/* #line 50 "HCLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 50 "HCLT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 26 "HCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 26 "HCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 38 "HCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 38 "HCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 38 "HCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 44 "HCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 50 "HCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 50 "HCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 56 "HCLT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 38 "HCLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 38 "HCLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 38 "HCLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 50 "HCLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 50 "HCLT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
/* #line 401 "HCLT.rl.c" */
		}
	}

_again:
	_acts = _HCLT_actions + _HCLT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 412 "HCLT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _HCLT_eof_trans[cs] > 0 ) {
		_trans = _HCLT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 115 "HCLT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < HCLT_first_final)
        o = HCLTBAD;

    return o;
}
