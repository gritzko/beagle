
/* #line 1 "CMKT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "CMKT.h"

ok64 CMKTonComment (u8cs tok, CMKTstate* state);
ok64 CMKTonString (u8cs tok, CMKTstate* state);
ok64 CMKTonNumber (u8cs tok, CMKTstate* state);
ok64 CMKTonVar (u8cs tok, CMKTstate* state);
ok64 CMKTonWord (u8cs tok, CMKTstate* state);
ok64 CMKTonPunct (u8cs tok, CMKTstate* state);
ok64 CMKTonSpace (u8cs tok, CMKTstate* state);


/* #line 93 "CMKT.c.rl" */



/* #line 16 "CMKT.rl.c" */
static const char _CMKT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12
};

static const char _CMKT_key_offsets[] = {
	0, 2, 2, 3, 5, 18, 21, 22, 
	23, 26, 28
};

static const unsigned char _CMKT_trans_keys[] = {
	34u, 92u, 125u, 48u, 57u, 32u, 34u, 35u, 
	36u, 95u, 9u, 13u, 48u, 57u, 65u, 90u, 
	97u, 122u, 32u, 9u, 13u, 10u, 123u, 46u, 
	48u, 57u, 48u, 57u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 0
};

static const char _CMKT_single_lengths[] = {
	2, 0, 1, 0, 5, 1, 1, 1, 
	1, 0, 1
};

static const char _CMKT_range_lengths[] = {
	0, 0, 0, 1, 4, 1, 0, 0, 
	1, 1, 3
};

static const char _CMKT_index_offsets[] = {
	0, 3, 4, 6, 8, 18, 21, 23, 
	25, 28, 30
};

static const char _CMKT_trans_targs[] = {
	4, 1, 0, 0, 4, 2, 9, 4, 
	5, 0, 6, 7, 10, 5, 8, 10, 
	10, 4, 5, 5, 4, 4, 6, 2, 
	4, 3, 8, 4, 9, 4, 10, 10, 
	10, 10, 4, 4, 4, 4, 4, 4, 
	4, 4, 4, 0
};

static const char _CMKT_trans_actions[] = {
	7, 0, 0, 0, 9, 0, 0, 23, 
	0, 0, 0, 5, 0, 0, 5, 0, 
	0, 11, 0, 0, 21, 13, 0, 0, 
	19, 0, 5, 15, 0, 15, 0, 0, 
	0, 0, 17, 25, 23, 21, 13, 19, 
	15, 15, 17, 0
};

static const char _CMKT_to_state_actions[] = {
	0, 0, 0, 0, 1, 0, 0, 0, 
	0, 0, 0
};

static const char _CMKT_from_state_actions[] = {
	0, 0, 0, 0, 3, 0, 0, 0, 
	0, 0, 0
};

static const char _CMKT_eof_trans[] = {
	0, 0, 36, 37, 0, 38, 39, 40, 
	42, 42, 43
};

static const int CMKT_start = 4;
static const int CMKT_first_final = 4;
static const int CMKT_error = -1;

static const int CMKT_en_main = 4;


/* #line 96 "CMKT.c.rl" */

ok64 CMKTLexer(CMKTstate* state) {

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

    
/* #line 107 "CMKT.rl.c" */
	{
	cs = CMKT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 114 "CMKT.c.rl" */
    
/* #line 113 "CMKT.rl.c" */
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( p == pe )
		goto _test_eof;
_resume:
	_acts = _CMKT_actions + _CMKT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 130 "CMKT.rl.c" */
		}
	}

	_keys = _CMKT_trans_keys + _CMKT_key_offsets[cs];
	_trans = _CMKT_index_offsets[cs];

	_klen = _CMKT_single_lengths[cs];
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

	_klen = _CMKT_range_lengths[cs];
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
	cs = _CMKT_trans_targs[_trans];

	if ( _CMKT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _CMKT_actions + _CMKT_trans_actions[_trans];
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
/* #line 31 "CMKT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CMKTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 4:
/* #line 43 "CMKT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CMKTonVar(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 5:
/* #line 55 "CMKT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CMKTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 25 "CMKT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CMKTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 37 "CMKT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CMKTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 49 "CMKT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CMKTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 55 "CMKT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CMKTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 61 "CMKT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CMKTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 37 "CMKT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CMKTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 55 "CMKT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CMKTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
/* #line 277 "CMKT.rl.c" */
		}
	}

_again:
	_acts = _CMKT_actions + _CMKT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 288 "CMKT.rl.c" */
		}
	}

	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _CMKT_eof_trans[cs] > 0 ) {
		_trans = _CMKT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 115 "CMKT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < CMKT_first_final)
        o = CMKTBAD;

    return o;
}
