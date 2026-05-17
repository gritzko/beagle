
/* #line 1 "MDT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "MDT.h"

ok64 MDTonEmph (u8cs tok, MDTstate* state);
ok64 MDTonCode (u8cs tok, MDTstate* state);
ok64 MDTonComment (u8cs tok, MDTstate* state);
ok64 MDTonLink (u8cs tok, MDTstate* state);
ok64 MDTonNumber (u8cs tok, MDTstate* state);
ok64 MDTonWord (u8cs tok, MDTstate* state);
ok64 MDTonPunct (u8cs tok, MDTstate* state);
ok64 MDTonSpace (u8cs tok, MDTstate* state);


/* #line 142 "MDT.c.rl" */



/* #line 17 "MDT.rl.c" */
static const char _MDT_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	9, 1, 10, 1, 11, 1, 12, 1, 
	13, 1, 14, 1, 15, 1, 16, 1, 
	17, 1, 18, 1, 19, 1, 20, 1, 
	21, 1, 22, 1, 23, 1, 24, 1, 
	25, 1, 26, 1, 27, 1, 28, 1, 
	29, 1, 30, 1, 31, 1, 32, 1, 
	33, 1, 34, 1, 35, 1, 36, 1, 
	37, 1, 38, 2, 2, 3, 2, 2, 
	4, 2, 2, 5, 2, 2, 6, 2, 
	2, 7, 2, 2, 8
};

static const unsigned char _MDT_key_offsets[] = {
	0, 10, 11, 12, 14, 16, 18, 20, 
	22, 28, 29, 30, 31, 32, 33, 35, 
	37, 39, 41, 42, 43, 45, 47, 86, 
	90, 94, 99, 103, 105, 107, 112, 114, 
	117, 123, 124, 131, 141, 149, 159, 167, 
	175, 177, 178, 182
};

static const unsigned char _MDT_trans_keys[] = {
	43u, 58u, 45u, 46u, 48u, 57u, 65u, 90u, 
	97u, 122u, 47u, 47u, 10u, 41u, 10u, 41u, 
	10u, 42u, 10u, 42u, 10u, 42u, 48u, 57u, 
	65u, 70u, 97u, 102u, 45u, 45u, 45u, 45u, 
	62u, 10u, 95u, 10u, 95u, 10u, 95u, 10u, 
	96u, 96u, 96u, 10u, 126u, 10u, 126u, 10u, 
	32u, 40u, 42u, 43u, 45u, 46u, 48u, 60u, 
	62u, 63u, 95u, 96u, 126u, 127u, 0u, 8u, 
	9u, 13u, 14u, 31u, 33u, 35u, 36u, 37u, 
	38u, 47u, 49u, 57u, 58u, 64u, 65u, 90u, 
	91u, 94u, 97u, 122u, 123u, 125u, 9u, 32u, 
	11u, 13u, 65u, 90u, 97u, 122u, 9u, 32u, 
	42u, 10u, 13u, 32u, 42u, 9u, 13u, 9u, 
	32u, 48u, 57u, 46u, 88u, 120u, 48u, 57u, 
	48u, 57u, 46u, 48u, 57u, 48u, 57u, 65u, 
	70u, 97u, 102u, 33u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 32u, 95u, 9u, 13u, 48u, 
	57u, 65u, 90u, 97u, 122u, 10u, 95u, 48u, 
	57u, 65u, 90u, 97u, 122u, 32u, 95u, 9u, 
	13u, 48u, 57u, 65u, 90u, 97u, 122u, 10u, 
	95u, 48u, 57u, 65u, 90u, 97u, 122u, 10u, 
	95u, 48u, 57u, 65u, 90u, 97u, 122u, 10u, 
	96u, 126u, 32u, 126u, 9u, 13u, 128u, 191u, 
	0
};

static const char _MDT_single_lengths[] = {
	2, 1, 1, 2, 2, 2, 2, 2, 
	0, 1, 1, 1, 1, 1, 2, 2, 
	2, 2, 1, 1, 2, 2, 15, 2, 
	0, 3, 2, 2, 0, 3, 0, 1, 
	0, 1, 1, 2, 2, 2, 2, 2, 
	2, 1, 2, 0
};

static const char _MDT_range_lengths[] = {
	4, 0, 0, 0, 0, 0, 0, 0, 
	3, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 12, 1, 
	2, 1, 1, 0, 1, 1, 1, 1, 
	3, 0, 3, 4, 3, 4, 3, 3, 
	0, 0, 1, 1
};

static const unsigned char _MDT_index_offsets[] = {
	0, 7, 9, 11, 14, 17, 20, 23, 
	26, 30, 32, 34, 36, 38, 40, 43, 
	46, 49, 52, 54, 56, 59, 62, 90, 
	94, 97, 102, 106, 109, 111, 116, 118, 
	121, 125, 127, 132, 139, 145, 152, 158, 
	164, 167, 169, 173
};

static const char _MDT_indicies[] = {
	1, 2, 1, 1, 1, 1, 0, 3, 
	0, 4, 0, 0, 0, 5, 0, 6, 
	5, 0, 8, 7, 9, 11, 10, 9, 
	12, 10, 14, 14, 14, 13, 15, 0, 
	16, 0, 17, 16, 18, 16, 19, 16, 
	20, 22, 21, 23, 25, 24, 23, 26, 
	24, 0, 28, 27, 30, 29, 31, 29, 
	32, 34, 33, 32, 35, 33, 38, 37, 
	40, 41, 42, 42, 43, 44, 46, 47, 
	36, 49, 50, 51, 36, 36, 37, 36, 
	39, 36, 39, 45, 39, 48, 39, 48, 
	39, 52, 37, 37, 37, 53, 1, 1, 
	54, 55, 55, 56, 54, 7, 57, 57, 
	57, 10, 55, 55, 54, 58, 23, 60, 
	61, 61, 45, 59, 60, 62, 60, 45, 
	59, 14, 14, 14, 63, 64, 54, 48, 
	48, 48, 48, 23, 65, 67, 65, 66, 
	66, 66, 21, 65, 68, 66, 66, 66, 
	21, 69, 48, 69, 70, 70, 70, 24, 
	65, 71, 70, 70, 70, 24, 65, 72, 
	70, 70, 70, 24, 54, 29, 27, 73, 
	54, 74, 74, 74, 33, 52, 75, 0
};

static const char _MDT_trans_targs[] = {
	22, 0, 1, 2, 3, 4, 22, 5, 
	22, 22, 6, 7, 22, 22, 32, 10, 
	11, 12, 13, 22, 22, 14, 22, 22, 
	15, 16, 22, 17, 22, 18, 19, 22, 
	22, 20, 21, 22, 22, 23, 22, 22, 
	24, 25, 27, 28, 29, 31, 33, 22, 
	34, 35, 40, 41, 43, 22, 22, 22, 
	26, 22, 28, 22, 30, 8, 22, 22, 
	9, 22, 36, 37, 34, 22, 38, 39, 
	34, 42, 22, 22
};

static const char _MDT_trans_actions[] = {
	63, 0, 0, 0, 0, 0, 23, 0, 
	15, 57, 0, 0, 13, 55, 0, 0, 
	0, 0, 0, 11, 61, 0, 19, 65, 
	0, 0, 17, 0, 9, 0, 0, 7, 
	59, 0, 0, 21, 33, 0, 31, 29, 
	5, 5, 0, 82, 5, 0, 5, 27, 
	79, 5, 5, 0, 0, 51, 49, 25, 
	5, 41, 73, 39, 0, 0, 37, 35, 
	0, 47, 5, 76, 70, 43, 79, 79, 
	67, 5, 45, 53
};

static const char _MDT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 1, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0
};

static const char _MDT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 3, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0
};

static const unsigned char _MDT_eof_trans[] = {
	1, 1, 1, 1, 1, 1, 10, 10, 
	14, 1, 1, 1, 1, 1, 21, 24, 
	24, 1, 1, 1, 33, 33, 0, 54, 
	55, 55, 58, 55, 24, 60, 63, 60, 
	64, 55, 24, 66, 66, 70, 66, 66, 
	55, 55, 75, 76
};

static const int MDT_start = 22;
static const int MDT_first_final = 22;
static const int MDT_error = -1;

static const int MDT_en_main = 22;


/* #line 145 "MDT.c.rl" */

ok64 MDTInlineLexer(MDTstate* state) {

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

    
/* #line 195 "MDT.rl.c" */
	{
	cs = MDT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 163 "MDT.c.rl" */
    
/* #line 201 "MDT.rl.c" */
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( p == pe )
		goto _test_eof;
_resume:
	_acts = _MDT_actions + _MDT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 218 "MDT.rl.c" */
		}
	}

	_keys = _MDT_trans_keys + _MDT_key_offsets[cs];
	_trans = _MDT_index_offsets[cs];

	_klen = _MDT_single_lengths[cs];
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

	_klen = _MDT_range_lengths[cs];
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
	_trans = _MDT_indicies[_trans];
_eof_trans:
	cs = _MDT_trans_targs[_trans];

	if ( _MDT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _MDT_actions + _MDT_trans_actions[_trans];
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
/* #line 29 "MDT.c.rl" */
	{act = 6;}
	break;
	case 4:
/* #line 29 "MDT.c.rl" */
	{act = 7;}
	break;
	case 5:
/* #line 53 "MDT.c.rl" */
	{act = 12;}
	break;
	case 6:
/* #line 65 "MDT.c.rl" */
	{act = 15;}
	break;
	case 7:
/* #line 59 "MDT.c.rl" */
	{act = 17;}
	break;
	case 8:
/* #line 65 "MDT.c.rl" */
	{act = 20;}
	break;
	case 9:
/* #line 35 "MDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonCode(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 35 "MDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonCode(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 41 "MDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 29 "MDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonEmph(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 29 "MDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonEmph(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 29 "MDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonEmph(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 29 "MDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonEmph(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 29 "MDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonEmph(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 47 "MDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonLink(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 65 "MDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 65 "MDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 65 "MDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 71 "MDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 65 "MDT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 53 "MDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 53 "MDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 53 "MDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 65 "MDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 65 "MDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 65 "MDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 59 "MDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 65 "MDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 71 "MDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 59 "MDT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 53 "MDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 65 "MDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 65 "MDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 59 "MDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 65 "MDT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 6:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonEmph(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 7:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonEmph(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 15:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 17:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 20:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 589 "MDT.rl.c" */
		}
	}

_again:
	_acts = _MDT_actions + _MDT_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 600 "MDT.rl.c" */
		}
	}

	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _MDT_eof_trans[cs] > 0 ) {
		_trans = _MDT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 164 "MDT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < MDT_first_final)
        o = MDTBAD;

    return o;
}
