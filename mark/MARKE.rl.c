
/* #line 1 "MARKE.c.rl" */
//  MARKE — HTML escaper for the mark dog.
//  Feeds `text` into `out`, mapping & < > " to entities; every other run
//  is copied verbatim.  Table-driven so the special-char detection is a
//  ragel scanner, never a hand-rolled byte scan.
//
//  Build: ragel -C MARKE.c.rl -o MARKE.rl.c -L

#include "abc/INT.h"
#include "abc/PRO.h"
#include "MARK.h"


/* #line 24 "MARKE.c.rl" */



/* #line 15 "MARKE.rl.c" */
static const char _marke_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6
};

static const char _marke_key_offsets[] = {
	0, 4
};

static const unsigned char _marke_trans_keys[] = {
	34u, 38u, 60u, 62u, 34u, 38u, 60u, 62u, 
	0
};

static const char _marke_single_lengths[] = {
	4, 4
};

static const char _marke_range_lengths[] = {
	0, 0
};

static const char _marke_index_offsets[] = {
	0, 5
};

static const char _marke_trans_targs[] = {
	0, 0, 0, 0, 1, 0, 0, 0, 
	0, 1, 0, 0
};

static const char _marke_trans_actions[] = {
	11, 5, 7, 9, 0, 13, 13, 13, 
	13, 0, 13, 0
};

static const char _marke_to_state_actions[] = {
	1, 0
};

static const char _marke_from_state_actions[] = {
	3, 0
};

static const char _marke_eof_trans[] = {
	0, 11
};

static const int marke_start = 0;
static const int marke_first_final = 0;
static const int marke_error = -1;

static const int marke_en_main = 0;


/* #line 27 "MARKE.c.rl" */

ok64 MARKu8bFeedEsc(u8bp out, u8csc text) {
    sane($ok(text));
    a_dup(u8c, data, text);

    int cs;
    int act = 0;
    u8c *p = (u8c *)data[0];
    u8c *pe = (u8c *)data[1];
    u8c *eof = pe;
    u8c *ts = NULL;
    u8c *te = NULL;
    ok64 o = OK;

    
/* #line 83 "MARKE.rl.c" */
	{
	cs = marke_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 42 "MARKE.c.rl" */
    
/* #line 89 "MARKE.rl.c" */
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( p == pe )
		goto _test_eof;
_resume:
	_acts = _marke_actions + _marke_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 106 "MARKE.rl.c" */
		}
	}

	_keys = _marke_trans_keys + _marke_key_offsets[cs];
	_trans = _marke_index_offsets[cs];

	_klen = _marke_single_lengths[cs];
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

	_klen = _marke_range_lengths[cs];
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
	cs = _marke_trans_targs[_trans];

	if ( _marke_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _marke_actions + _marke_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 2:
/* #line 17 "MARKE.c.rl" */
	{te = p+1;{ o = MARKu8bLit(out, "&amp;");  if (o != OK) {p++; goto _out; } }}
	break;
	case 3:
/* #line 18 "MARKE.c.rl" */
	{te = p+1;{ o = MARKu8bLit(out, "&lt;");   if (o != OK) {p++; goto _out; } }}
	break;
	case 4:
/* #line 19 "MARKE.c.rl" */
	{te = p+1;{ o = MARKu8bLit(out, "&gt;");   if (o != OK) {p++; goto _out; } }}
	break;
	case 5:
/* #line 20 "MARKE.c.rl" */
	{te = p+1;{ o = MARKu8bLit(out, "&quot;"); if (o != OK) {p++; goto _out; } }}
	break;
	case 6:
/* #line 21 "MARKE.c.rl" */
	{te = p;p--;{ u8cs s = {(u8c *)ts, (u8c *)te};
                             o = u8bFeed(out, s); if (o != OK) {p++; goto _out; } }}
	break;
/* #line 186 "MARKE.rl.c" */
		}
	}

_again:
	_acts = _marke_actions + _marke_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 197 "MARKE.rl.c" */
		}
	}

	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _marke_eof_trans[cs] > 0 ) {
		_trans = _marke_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 43 "MARKE.c.rl" */

    (void)act;
    return o;
}
