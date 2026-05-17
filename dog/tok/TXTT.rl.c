
/* #line 1 "TXTT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "TXTT.h"


/* #line 45 "TXTT.c.rl" */



/* #line 8 "TXTT.rl.c" */
static const char _TXTT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4
};

static const char _TXTT_key_offsets[] = {
	0, 8, 11
};

static const unsigned char _TXTT_trans_keys[] = {
	32u, 95u, 9u, 13u, 65u, 90u, 97u, 122u, 
	32u, 9u, 13u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 0
};

static const char _TXTT_single_lengths[] = {
	2, 1, 1
};

static const char _TXTT_range_lengths[] = {
	3, 1, 3
};

static const char _TXTT_index_offsets[] = {
	0, 6, 9
};

static const char _TXTT_indicies[] = {
	1, 2, 1, 2, 2, 0, 1, 1, 
	3, 2, 2, 2, 2, 4, 0
};

static const char _TXTT_trans_targs[] = {
	0, 1, 2, 0, 0
};

static const char _TXTT_trans_actions[] = {
	5, 0, 0, 9, 7
};

static const char _TXTT_to_state_actions[] = {
	1, 0, 0
};

static const char _TXTT_from_state_actions[] = {
	3, 0, 0
};

static const char _TXTT_eof_trans[] = {
	0, 4, 5
};

static const int TXTT_start = 0;
static const int TXTT_first_final = 0;
static const int TXTT_error = -1;

static const int TXTT_en_main = 0;


/* #line 48 "TXTT.c.rl" */

ok64 TXTTLexer(TXTTstate* state) {

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

    
/* #line 83 "TXTT.rl.c" */
	{
	cs = TXTT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 66 "TXTT.c.rl" */
    
/* #line 89 "TXTT.rl.c" */
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( p == pe )
		goto _test_eof;
_resume:
	_acts = _TXTT_actions + _TXTT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 106 "TXTT.rl.c" */
		}
	}

	_keys = _TXTT_trans_keys + _TXTT_key_offsets[cs];
	_trans = _TXTT_index_offsets[cs];

	_klen = _TXTT_single_lengths[cs];
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

	_klen = _TXTT_range_lengths[cs];
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
	_trans = _TXTT_indicies[_trans];
_eof_trans:
	cs = _TXTT_trans_targs[_trans];

	if ( _TXTT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _TXTT_actions + _TXTT_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 2:
/* #line 21 "TXTT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('P', tok, state->ctx); if (o!=OK) {p++; goto _out; } }
}}
	break;
	case 3:
/* #line 16 "TXTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('S', tok, state->ctx); if (o!=OK) {p++; goto _out; } }
}}
	break;
	case 4:
/* #line 26 "TXTT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('S', tok, state->ctx); if (o!=OK) {p++; goto _out; } }
}}
	break;
/* #line 192 "TXTT.rl.c" */
		}
	}

_again:
	_acts = _TXTT_actions + _TXTT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 203 "TXTT.rl.c" */
		}
	}

	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _TXTT_eof_trans[cs] > 0 ) {
		_trans = _TXTT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 67 "TXTT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < TXTT_first_final)
        o = TXTTBAD;

    return o;
}
