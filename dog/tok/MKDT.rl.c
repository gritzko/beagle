
/* #line 1 "MKDT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "MKDT.h"

ok64 MKDTonEmph (u8cs tok, MKDTstate* state);
ok64 MKDTonCode (u8cs tok, MKDTstate* state);
ok64 MKDTonLink (u8cs tok, MKDTstate* state);
ok64 MKDTonNumber (u8cs tok, MKDTstate* state);
ok64 MKDTonWord (u8cs tok, MKDTstate* state);
ok64 MKDTonPunct (u8cs tok, MKDTstate* state);
ok64 MKDTonSpace (u8cs tok, MKDTstate* state);
ok64 MKDTonEscape (u8cs tok, MKDTstate* state);


/* #line 138 "MKDT.c.rl" */



/* #line 17 "MKDT.rl.c" */
static const char _MKDT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19, 1, 20, 1, 21, 1, 22, 1, 
	23, 1, 24, 1, 25, 1, 26, 1, 
	27, 1, 28, 1, 29, 1, 30, 1, 
	31, 1, 32, 1, 33, 2, 2, 3, 
	2, 2, 4, 2, 2, 5, 2, 2, 
	6
};

static const unsigned char _MKDT_key_offsets[] = {
	0, 2, 4, 5, 11, 12, 14, 20, 
	22, 28, 29, 31, 33, 35, 37, 39, 
	41, 78, 82, 83, 87, 89, 94, 96, 
	99, 105, 112, 114, 115, 120, 130, 138, 
	140, 141, 145
};

static const unsigned char _MKDT_trans_keys[] = {
	10u, 93u, 10u, 93u, 91u, 48u, 57u, 65u, 
	90u, 97u, 122u, 93u, 10u, 42u, 48u, 57u, 
	65u, 70u, 97u, 102u, 10u, 93u, 48u, 57u, 
	65u, 90u, 97u, 122u, 93u, 10u, 42u, 10u, 
	95u, 10u, 95u, 10u, 96u, 10u, 126u, 10u, 
	126u, 10u, 32u, 33u, 42u, 46u, 48u, 63u, 
	91u, 92u, 95u, 96u, 126u, 127u, 0u, 8u, 
	9u, 13u, 14u, 31u, 34u, 35u, 36u, 37u, 
	38u, 47u, 49u, 57u, 58u, 64u, 65u, 90u, 
	93u, 94u, 97u, 122u, 123u, 125u, 9u, 32u, 
	11u, 13u, 91u, 32u, 42u, 9u, 13u, 48u, 
	57u, 46u, 88u, 120u, 48u, 57u, 48u, 57u, 
	46u, 48u, 57u, 48u, 57u, 65u, 70u, 97u, 
	102u, 95u, 48u, 57u, 65u, 90u, 97u, 122u, 
	10u, 93u, 91u, 32u, 42u, 95u, 9u, 13u, 
	32u, 95u, 9u, 13u, 48u, 57u, 65u, 90u, 
	97u, 122u, 10u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 10u, 96u, 126u, 32u, 126u, 9u, 
	13u, 128u, 191u, 0
};

static const char _MKDT_single_lengths[] = {
	2, 2, 1, 0, 1, 2, 0, 2, 
	0, 1, 2, 2, 2, 2, 2, 2, 
	13, 2, 1, 2, 0, 3, 0, 1, 
	0, 1, 2, 1, 3, 2, 2, 2, 
	1, 2, 0
};

static const char _MKDT_range_lengths[] = {
	0, 0, 0, 3, 0, 0, 3, 0, 
	3, 0, 0, 0, 0, 0, 0, 0, 
	12, 1, 0, 1, 1, 1, 1, 1, 
	3, 3, 0, 0, 1, 4, 3, 0, 
	0, 1, 1
};

static const unsigned char _MKDT_index_offsets[] = {
	0, 3, 6, 8, 12, 14, 17, 21, 
	24, 28, 30, 33, 36, 39, 42, 45, 
	48, 74, 78, 80, 84, 86, 91, 93, 
	96, 100, 105, 108, 110, 115, 122, 128, 
	131, 133, 137
};

static const char _MKDT_indicies[] = {
	0, 0, 1, 0, 2, 1, 3, 0, 
	4, 4, 4, 0, 5, 0, 0, 7, 
	6, 9, 9, 9, 8, 0, 11, 10, 
	13, 13, 13, 12, 14, 12, 0, 16, 
	15, 0, 18, 17, 19, 21, 20, 0, 
	23, 22, 24, 26, 25, 24, 27, 25, 
	30, 29, 31, 33, 34, 35, 28, 38, 
	39, 40, 41, 42, 28, 28, 29, 28, 
	32, 28, 32, 36, 32, 37, 32, 37, 
	32, 43, 29, 29, 29, 44, 46, 45, 
	45, 45, 45, 6, 48, 47, 50, 51, 
	51, 36, 49, 50, 52, 50, 36, 49, 
	9, 9, 9, 53, 37, 37, 37, 37, 
	47, 45, 45, 10, 55, 54, 45, 15, 
	17, 45, 56, 57, 37, 57, 58, 58, 
	58, 20, 57, 59, 58, 58, 58, 20, 
	45, 45, 22, 60, 45, 61, 61, 61, 
	25, 43, 62, 0
};

static const char _MKDT_trans_targs[] = {
	16, 1, 2, 3, 4, 16, 5, 16, 
	16, 24, 7, 27, 16, 9, 16, 10, 
	16, 11, 16, 16, 12, 16, 13, 16, 
	16, 14, 15, 16, 16, 17, 16, 18, 
	16, 19, 20, 21, 23, 25, 26, 28, 
	29, 31, 32, 34, 16, 16, 0, 16, 
	20, 16, 22, 6, 16, 16, 16, 8, 
	16, 16, 30, 25, 33, 16, 16
};

static const char _MKDT_trans_actions[] = {
	57, 0, 0, 0, 0, 23, 0, 15, 
	51, 0, 0, 5, 49, 0, 21, 0, 
	9, 0, 11, 55, 0, 17, 0, 7, 
	53, 0, 0, 19, 29, 0, 27, 5, 
	25, 5, 70, 5, 0, 67, 5, 5, 
	5, 5, 0, 0, 45, 43, 0, 59, 
	64, 37, 0, 0, 35, 33, 31, 0, 
	13, 41, 5, 61, 5, 39, 47
};

static const char _MKDT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	1, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const char _MKDT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	3, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const unsigned char _MKDT_eof_trans[] = {
	1, 1, 1, 1, 1, 1, 9, 1, 
	13, 13, 1, 1, 20, 1, 25, 25, 
	0, 45, 46, 46, 48, 50, 53, 50, 
	54, 48, 46, 55, 46, 58, 58, 46, 
	46, 62, 63
};

static const int MKDT_start = 16;
static const int MKDT_first_final = 16;
static const int MKDT_error = -1;

static const int MKDT_en_main = 16;


/* #line 141 "MKDT.c.rl" */

ok64 MKDTInlineLexer(MKDTstate* state) {

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

    
/* #line 174 "MKDT.rl.c" */
	{
	cs = MKDT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 159 "MKDT.c.rl" */
    
/* #line 180 "MKDT.rl.c" */
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( p == pe )
		goto _test_eof;
_resume:
	_acts = _MKDT_actions + _MKDT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 197 "MKDT.rl.c" */
		}
	}

	_keys = _MKDT_trans_keys + _MKDT_key_offsets[cs];
	_trans = _MKDT_index_offsets[cs];

	_klen = _MKDT_single_lengths[cs];
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

	_klen = _MKDT_range_lengths[cs];
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
	_trans = _MKDT_indicies[_trans];
_eof_trans:
	cs = _MKDT_trans_targs[_trans];

	if ( _MKDT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _MKDT_actions + _MKDT_trans_actions[_trans];
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
/* #line 29 "MKDT.c.rl" */
	{act = 6;}
	break;
	case 4:
/* #line 47 "MKDT.c.rl" */
	{act = 13;}
	break;
	case 5:
/* #line 53 "MKDT.c.rl" */
	{act = 16;}
	break;
	case 6:
/* #line 59 "MKDT.c.rl" */
	{act = 17;}
	break;
	case 7:
/* #line 35 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonCode(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 71 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonEscape(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 71 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonEscape(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 71 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonEscape(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 29 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonEmph(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 29 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonEmph(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 29 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonEmph(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 41 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonLink(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 41 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonLink(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 59 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 65 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 59 "MKDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 41 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonLink(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 47 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 47 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 47 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 59 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 53 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 59 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 65 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 53 "MKDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 41 "MKDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonLink(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 47 "MKDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 59 "MKDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 53 "MKDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 59 "MKDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 6:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonEmph(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 13:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 16:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 17:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 522 "MKDT.rl.c" */
		}
	}

_again:
	_acts = _MKDT_actions + _MKDT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 533 "MKDT.rl.c" */
		}
	}

	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _MKDT_eof_trans[cs] > 0 ) {
		_trans = _MKDT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 160 "MKDT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < MKDT_first_final)
        o = MKDTBAD;

    return o;
}

//  ---- inline span decomposer (folded in from the former mark/MARKG) ----
//
//  MKDTInlineLexer isolates an emphasis/link/image span as one 'G' token; this
//  second machine splits that span into (kind, text, label) so a renderer emits
//  <strong>/<em>/<del>/<a>/<img> without re-scanning.  The explicit label l is
//  one symbol; a shortcut [page] carries none, so it keys on the bracket text.


/* #line 197 "MKDT.c.rl" */



/* #line 566 "MKDT.rl.c" */
static const char _mkdtg_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	6, 2, 0, 1, 2, 1, 4, 2, 
	1, 5, 2, 1, 7, 2, 3, 7, 
	2, 3, 8, 3, 0, 1, 4, 3, 
	0, 1, 5, 3, 0, 1, 7
};

static const char _mkdtg_key_offsets[] = {
	0, 0, 5, 6, 8, 10, 11, 17, 
	18, 20, 22, 24, 26, 32, 33, 35, 
	37, 38, 40, 42, 44, 44
};

static const unsigned char _mkdtg_trans_keys[] = {
	33u, 42u, 91u, 95u, 126u, 91u, 10u, 93u, 
	10u, 93u, 91u, 48u, 57u, 65u, 90u, 97u, 
	122u, 93u, 10u, 42u, 10u, 42u, 10u, 93u, 
	10u, 93u, 48u, 57u, 65u, 90u, 97u, 122u, 
	93u, 10u, 95u, 10u, 95u, 126u, 10u, 126u, 
	10u, 126u, 10u, 126u, 91u, 0
};

static const char _mkdtg_single_lengths[] = {
	0, 5, 1, 2, 2, 1, 0, 1, 
	2, 2, 2, 2, 0, 1, 2, 2, 
	1, 2, 2, 2, 0, 1
};

static const char _mkdtg_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 3, 0, 
	0, 0, 0, 0, 3, 0, 0, 0, 
	0, 0, 0, 0, 0, 0
};

static const char _mkdtg_index_offsets[] = {
	0, 0, 6, 8, 11, 14, 16, 20, 
	22, 25, 28, 31, 34, 38, 40, 43, 
	46, 48, 51, 54, 57, 58
};

static const char _mkdtg_trans_targs[] = {
	2, 8, 10, 14, 16, 0, 3, 0, 
	0, 5, 4, 0, 5, 4, 6, 0, 
	7, 7, 7, 0, 20, 0, 0, 20, 
	9, 0, 20, 9, 0, 21, 11, 0, 
	21, 11, 13, 13, 13, 0, 20, 0, 
	0, 20, 15, 0, 20, 15, 17, 0, 
	0, 19, 18, 0, 19, 18, 0, 20, 
	18, 0, 12, 0, 0
};

static const char _mkdtg_trans_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 9, 1, 0, 3, 0, 0, 0, 
	5, 5, 5, 0, 24, 0, 0, 27, 
	1, 0, 12, 0, 0, 35, 1, 0, 
	18, 0, 5, 5, 5, 0, 21, 0, 
	0, 31, 1, 0, 15, 0, 0, 0, 
	0, 9, 1, 0, 3, 0, 0, 7, 
	0, 0, 0, 0, 0
};

static const int mkdtg_start = 1;
static const int mkdtg_first_final = 20;
static const int mkdtg_error = 0;

static const int mkdtg_en_main = 1;


/* #line 200 "MKDT.c.rl" */

ok64 MKDTDecomposeSpan(mkdtspan *g, u8csc tok) {
    a_dup(u8c, data, tok);

    int cs;
    u8c *p = (u8c *)data[0];
    u8c *pe = (u8c *)data[1];
    u8c *eof = pe;
    u8c *txt0 = NULL, *txt1 = NULL, *lbl0 = NULL, *lbl1 = NULL;
    u8 kind = 0;

    
/* #line 646 "MKDT.rl.c" */
	{
	cs = mkdtg_start;
	}

/* #line 212 "MKDT.c.rl" */
    
/* #line 649 "MKDT.rl.c" */
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
	_keys = _mkdtg_trans_keys + _mkdtg_key_offsets[cs];
	_trans = _mkdtg_index_offsets[cs];

	_klen = _mkdtg_single_lengths[cs];
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

	_klen = _mkdtg_range_lengths[cs];
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
	cs = _mkdtg_trans_targs[_trans];

	if ( _mkdtg_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _mkdtg_actions + _mkdtg_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
/* #line 179 "MKDT.c.rl" */
	{ txt0 = (u8c *)p; }
	break;
	case 1:
/* #line 180 "MKDT.c.rl" */
	{ txt1 = (u8c *)p; }
	break;
	case 2:
/* #line 181 "MKDT.c.rl" */
	{ lbl0 = (u8c *)p; }
	break;
	case 3:
/* #line 182 "MKDT.c.rl" */
	{ lbl1 = (u8c *)p; }
	break;
	case 4:
/* #line 183 "MKDT.c.rl" */
	{ kind = 'B'; }
	break;
	case 5:
/* #line 184 "MKDT.c.rl" */
	{ kind = 'I'; }
	break;
	case 6:
/* #line 185 "MKDT.c.rl" */
	{ kind = 'D'; }
	break;
	case 7:
/* #line 186 "MKDT.c.rl" */
	{ kind = 'A'; }
	break;
	case 8:
/* #line 187 "MKDT.c.rl" */
	{ kind = 'M'; }
	break;
/* #line 748 "MKDT.rl.c" */
		}
	}

_again:
	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	_out: {}
	}

/* #line 213 "MKDT.c.rl" */

    if (cs < mkdtg_first_final) {
        g->kind = 0;
        return OK;
    }
    g->kind = kind;
    g->text[0] = txt0 ? txt0 : (u8c *)data[1];
    g->text[1] = txt1 ? txt1 : (u8c *)data[1];
    if (lbl0 != NULL) {
        g->label[0] = lbl0;
        g->label[1] = lbl1;
    } else {
        //  shortcut: key the link on its bracket text
        g->label[0] = g->text[0];
        g->label[1] = g->text[1];
    }
    (void)eof;
    return OK;
}
