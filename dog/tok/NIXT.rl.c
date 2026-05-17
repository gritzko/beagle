
/* #line 1 "NIXT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "NIXT.h"

ok64 NIXTonComment (u8cs tok, NIXTstate* state);
ok64 NIXTonString (u8cs tok, NIXTstate* state);
ok64 NIXTonNumber (u8cs tok, NIXTstate* state);
ok64 NIXTonWord (u8cs tok, NIXTstate* state);
ok64 NIXTonPunct (u8cs tok, NIXTstate* state);
ok64 NIXTonSpace (u8cs tok, NIXTstate* state);


/* #line 99 "NIXT.c.rl" */



/* #line 15 "NIXT.rl.c" */
static const char _NIXT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 1, 13, 1, 14
};

static const char _NIXT_key_offsets[] = {
	0, 0, 2, 8, 9, 10, 11, 19, 
	27, 28, 30, 38, 46, 47, 54, 80, 
	83, 84, 85, 86, 87, 88, 95, 102, 
	104, 105, 113, 114
};

static const unsigned char _NIXT_trans_keys[] = {
	34u, 92u, 34u, 36u, 92u, 110u, 114u, 116u, 
	39u, 39u, 39u, 46u, 95u, 45u, 57u, 65u, 
	90u, 97u, 122u, 42u, 95u, 45u, 57u, 65u, 
	90u, 97u, 122u, 42u, 42u, 47u, 61u, 95u, 
	45u, 57u, 65u, 90u, 97u, 122u, 62u, 95u, 
	45u, 57u, 65u, 90u, 97u, 122u, 47u, 95u, 
	45u, 57u, 65u, 90u, 97u, 122u, 32u, 33u, 
	34u, 35u, 38u, 39u, 43u, 45u, 46u, 47u, 
	58u, 60u, 63u, 95u, 124u, 126u, 9u, 13u, 
	48u, 57u, 61u, 62u, 65u, 90u, 97u, 122u, 
	32u, 9u, 13u, 61u, 10u, 38u, 43u, 62u, 
	95u, 45u, 57u, 65u, 90u, 97u, 122u, 95u, 
	45u, 57u, 65u, 90u, 97u, 122u, 48u, 57u, 
	46u, 45u, 95u, 48u, 57u, 65u, 90u, 97u, 
	122u, 124u, 95u, 45u, 57u, 65u, 90u, 97u, 
	122u, 0
};

static const char _NIXT_single_lengths[] = {
	0, 2, 6, 1, 1, 1, 2, 2, 
	1, 2, 2, 2, 1, 1, 16, 1, 
	1, 1, 1, 1, 1, 1, 1, 0, 
	1, 2, 1, 1
};

static const char _NIXT_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 3, 3, 
	0, 0, 3, 3, 0, 3, 5, 1, 
	0, 0, 0, 0, 0, 3, 3, 1, 
	0, 3, 0, 3
};

static const unsigned char _NIXT_index_offsets[] = {
	0, 0, 3, 10, 12, 14, 16, 22, 
	28, 30, 33, 39, 45, 47, 52, 74, 
	77, 79, 81, 83, 85, 87, 92, 97, 
	99, 101, 107, 109
};

static const char _NIXT_indicies[] = {
	1, 2, 0, 0, 0, 0, 0, 0, 
	0, 3, 4, 3, 5, 4, 6, 4, 
	8, 7, 7, 7, 7, 3, 9, 7, 
	7, 7, 7, 3, 10, 9, 10, 11, 
	9, 13, 12, 12, 12, 12, 3, 14, 
	12, 12, 12, 12, 3, 15, 3, 16, 
	16, 16, 16, 3, 18, 19, 0, 20, 
	21, 22, 23, 24, 25, 26, 13, 28, 
	29, 30, 31, 32, 18, 27, 19, 30, 
	30, 17, 18, 18, 33, 13, 34, 35, 
	20, 13, 34, 13, 34, 13, 34, 7, 
	7, 7, 7, 36, 7, 7, 7, 7, 
	36, 27, 37, 13, 34, 30, 30, 30, 
	30, 30, 38, 13, 34, 16, 16, 16, 
	16, 39, 0
};

static const char _NIXT_trans_targs[] = {
	1, 14, 2, 0, 4, 5, 14, 21, 
	22, 8, 9, 14, 11, 14, 14, 13, 
	27, 14, 15, 16, 17, 18, 3, 19, 
	20, 6, 7, 23, 10, 24, 25, 26, 
	12, 14, 14, 14, 14, 14, 14, 14
};

static const char _NIXT_trans_actions[] = {
	0, 9, 0, 0, 0, 0, 7, 0, 
	0, 0, 0, 5, 0, 13, 11, 0, 
	0, 15, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 29, 27, 17, 19, 23, 25, 21
};

static const char _NIXT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 1, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0
};

static const char _NIXT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 3, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0
};

static const unsigned char _NIXT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 34, 
	35, 36, 35, 35, 35, 37, 37, 38, 
	35, 39, 35, 40
};

static const int NIXT_start = 14;
static const int NIXT_first_final = 14;
static const int NIXT_error = 0;

static const int NIXT_en_main = 14;


/* #line 102 "NIXT.c.rl" */

ok64 NIXTLexer(NIXTstate* state) {

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

    
/* #line 147 "NIXT.rl.c" */
	{
	cs = NIXT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 120 "NIXT.c.rl" */
    
/* #line 153 "NIXT.rl.c" */
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
	_acts = _NIXT_actions + _NIXT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 172 "NIXT.rl.c" */
		}
	}

	_keys = _NIXT_trans_keys + _NIXT_key_offsets[cs];
	_trans = _NIXT_index_offsets[cs];

	_klen = _NIXT_single_lengths[cs];
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

	_klen = _NIXT_range_lengths[cs];
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
	_trans = _NIXT_indicies[_trans];
_eof_trans:
	cs = _NIXT_trans_targs[_trans];

	if ( _NIXT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _NIXT_actions + _NIXT_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 2:
/* #line 26 "NIXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIXTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 3:
/* #line 32 "NIXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIXTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 4:
/* #line 32 "NIXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIXTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 5:
/* #line 32 "NIXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIXTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 50 "NIXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIXTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 50 "NIXT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIXTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 26 "NIXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIXTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 44 "NIXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIXTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 44 "NIXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIXTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 38 "NIXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIXTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 44 "NIXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIXTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 50 "NIXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIXTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 56 "NIXT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIXTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
/* #line 341 "NIXT.rl.c" */
		}
	}

_again:
	_acts = _NIXT_actions + _NIXT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 352 "NIXT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _NIXT_eof_trans[cs] > 0 ) {
		_trans = _NIXT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 121 "NIXT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < NIXT_first_final)
        o = NIXTBAD;

    return o;
}
