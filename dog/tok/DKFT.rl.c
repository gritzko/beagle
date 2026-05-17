
/* #line 1 "DKFT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "DKFT.h"

ok64 DKFTonComment (u8cs tok, DKFTstate* state);
ok64 DKFTonString (u8cs tok, DKFTstate* state);
ok64 DKFTonNumber (u8cs tok, DKFTstate* state);
ok64 DKFTonVar (u8cs tok, DKFTstate* state);
ok64 DKFTonWord (u8cs tok, DKFTstate* state);
ok64 DKFTonPunct (u8cs tok, DKFTstate* state);
ok64 DKFTonSpace (u8cs tok, DKFTstate* state);


/* #line 96 "DKFT.c.rl" */



/* #line 16 "DKFT.rl.c" */
static const char _DKFT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 1, 13
};

static const char _DKFT_key_offsets[] = {
	0, 0, 2, 2, 8, 9, 10, 12, 
	27, 30, 31, 38, 41, 43
};

static const unsigned char _DKFT_trans_keys[] = {
	34u, 92u, 95u, 123u, 65u, 90u, 97u, 122u, 
	125u, 39u, 48u, 57u, 32u, 34u, 35u, 36u, 
	39u, 46u, 95u, 9u, 13u, 48u, 57u, 65u, 
	90u, 97u, 122u, 32u, 9u, 13u, 10u, 95u, 
	48u, 57u, 65u, 90u, 97u, 122u, 46u, 48u, 
	57u, 48u, 57u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 0
};

static const char _DKFT_single_lengths[] = {
	0, 2, 0, 2, 1, 1, 0, 7, 
	1, 1, 1, 1, 0, 1
};

static const char _DKFT_range_lengths[] = {
	0, 0, 0, 2, 0, 0, 1, 4, 
	1, 0, 3, 1, 1, 3
};

static const char _DKFT_index_offsets[] = {
	0, 0, 3, 4, 9, 11, 13, 15, 
	27, 30, 32, 37, 40, 42
};

static const char _DKFT_trans_targs[] = {
	7, 2, 1, 1, 10, 4, 10, 10, 
	0, 7, 4, 7, 5, 12, 7, 8, 
	1, 9, 3, 5, 7, 13, 8, 11, 
	13, 13, 7, 8, 8, 7, 7, 9, 
	10, 10, 10, 10, 7, 6, 11, 7, 
	12, 7, 13, 13, 13, 13, 7, 7, 
	7, 7, 7, 7, 7, 7, 0
};

static const char _DKFT_trans_actions[] = {
	7, 0, 0, 0, 0, 0, 0, 0, 
	0, 11, 0, 9, 0, 0, 27, 0, 
	0, 0, 0, 0, 15, 0, 0, 5, 
	0, 0, 13, 0, 0, 25, 17, 0, 
	0, 0, 0, 0, 19, 0, 5, 21, 
	0, 21, 0, 0, 0, 0, 23, 27, 
	25, 17, 19, 21, 21, 23, 0
};

static const char _DKFT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 1, 
	0, 0, 0, 0, 0, 0
};

static const char _DKFT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 3, 
	0, 0, 0, 0, 0, 0
};

static const char _DKFT_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 48, 0, 
	49, 50, 51, 53, 53, 54
};

static const int DKFT_start = 7;
static const int DKFT_first_final = 7;
static const int DKFT_error = 0;

static const int DKFT_en_main = 7;


/* #line 99 "DKFT.c.rl" */

ok64 DKFTLexer(DKFTstate* state) {

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

    
/* #line 111 "DKFT.rl.c" */
	{
	cs = DKFT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 117 "DKFT.c.rl" */
    
/* #line 117 "DKFT.rl.c" */
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
	_acts = _DKFT_actions + _DKFT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 136 "DKFT.rl.c" */
		}
	}

	_keys = _DKFT_trans_keys + _DKFT_key_offsets[cs];
	_trans = _DKFT_index_offsets[cs];

	_klen = _DKFT_single_lengths[cs];
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

	_klen = _DKFT_range_lengths[cs];
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
	cs = _DKFT_trans_targs[_trans];

	if ( _DKFT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _DKFT_actions + _DKFT_trans_actions[_trans];
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
/* #line 31 "DKFT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DKFTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 4:
/* #line 31 "DKFT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DKFTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 5:
/* #line 43 "DKFT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DKFTonVar(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 55 "DKFT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DKFTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 55 "DKFT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DKFTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 25 "DKFT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DKFTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 43 "DKFT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DKFTonVar(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 37 "DKFT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DKFTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 49 "DKFT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DKFTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 61 "DKFT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DKFTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 37 "DKFT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DKFTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
/* #line 291 "DKFT.rl.c" */
		}
	}

_again:
	_acts = _DKFT_actions + _DKFT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 302 "DKFT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _DKFT_eof_trans[cs] > 0 ) {
		_trans = _DKFT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 118 "DKFT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < DKFT_first_final)
        o = DKFTBAD;

    return o;
}
