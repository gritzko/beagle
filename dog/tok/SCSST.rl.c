
/* #line 1 "SCSST.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "SCSST.h"

ok64 SCSSTonComment (u8cs tok, SCSSTstate* state);
ok64 SCSSTonString (u8cs tok, SCSSTstate* state);
ok64 SCSSTonNumber (u8cs tok, SCSSTstate* state);
ok64 SCSSTonAtRule (u8cs tok, SCSSTstate* state);
ok64 SCSSTonWord (u8cs tok, SCSSTstate* state);
ok64 SCSSTonVar (u8cs tok, SCSSTstate* state);
ok64 SCSSTonPunct (u8cs tok, SCSSTstate* state);
ok64 SCSSTonSpace (u8cs tok, SCSSTstate* state);


/* #line 120 "SCSST.c.rl" */



/* #line 17 "SCSST.rl.c" */
static const char _SCSST_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19, 1, 20, 1, 21
};

static const unsigned char _SCSST_key_offsets[] = {
	0, 0, 2, 2, 9, 15, 21, 22, 
	27, 29, 29, 37, 38, 40, 42, 47, 
	65, 68, 74, 80, 86, 92, 98, 106, 
	114, 116, 123, 128, 130, 131, 139, 144, 
	151, 156
};

static const unsigned char _SCSST_trans_keys[] = {
	34u, 92u, 123u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 125u, 95u, 65u, 
	90u, 97u, 122u, 39u, 92u, 45u, 95u, 48u, 
	57u, 65u, 90u, 97u, 122u, 42u, 42u, 47u, 
	48u, 57u, 45u, 65u, 90u, 97u, 122u, 32u, 
	34u, 35u, 36u, 39u, 45u, 46u, 47u, 64u, 
	95u, 9u, 13u, 48u, 57u, 65u, 90u, 97u, 
	122u, 32u, 9u, 13u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 45u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 45u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 48u, 57u, 37u, 48u, 57u, 65u, 
	90u, 97u, 122u, 37u, 65u, 90u, 97u, 122u, 
	42u, 47u, 10u, 37u, 46u, 48u, 57u, 65u, 
	90u, 97u, 122u, 37u, 65u, 90u, 97u, 122u, 
	37u, 48u, 57u, 65u, 90u, 97u, 122u, 45u, 
	65u, 90u, 97u, 122u, 45u, 95u, 48u, 57u, 
	65u, 90u, 97u, 122u, 0
};

static const char _SCSST_single_lengths[] = {
	0, 2, 0, 1, 0, 0, 1, 1, 
	2, 0, 2, 1, 2, 0, 1, 10, 
	1, 0, 0, 0, 0, 0, 2, 2, 
	0, 1, 1, 2, 1, 2, 1, 1, 
	1, 2
};

static const char _SCSST_range_lengths[] = {
	0, 0, 0, 3, 3, 3, 0, 2, 
	0, 0, 3, 0, 0, 1, 2, 4, 
	1, 3, 3, 3, 3, 3, 3, 3, 
	1, 3, 2, 0, 0, 3, 2, 3, 
	2, 3
};

static const unsigned char _SCSST_index_offsets[] = {
	0, 0, 3, 4, 9, 13, 17, 19, 
	23, 26, 27, 33, 35, 38, 40, 44, 
	59, 62, 66, 70, 74, 78, 82, 88, 
	94, 96, 101, 105, 108, 110, 116, 120, 
	125, 129
};

static const char _SCSST_indicies[] = {
	1, 2, 0, 0, 5, 3, 3, 3, 
	4, 6, 6, 6, 4, 7, 7, 7, 
	4, 8, 5, 9, 9, 9, 4, 11, 
	12, 10, 10, 13, 13, 13, 13, 13, 
	4, 16, 15, 16, 17, 15, 19, 18, 
	20, 20, 20, 4, 22, 0, 23, 24, 
	10, 25, 26, 27, 29, 30, 22, 28, 
	30, 30, 21, 22, 22, 31, 33, 33, 
	33, 32, 34, 34, 34, 32, 35, 35, 
	35, 32, 36, 36, 36, 32, 37, 37, 
	37, 32, 9, 9, 9, 9, 9, 38, 
	13, 13, 13, 13, 13, 39, 41, 40, 
	43, 41, 43, 43, 42, 43, 43, 43, 
	42, 15, 45, 44, 46, 45, 48, 49, 
	28, 48, 48, 47, 48, 48, 48, 47, 
	48, 19, 48, 48, 47, 20, 20, 20, 
	50, 30, 30, 30, 30, 30, 51, 0
};

static const char _SCSST_trans_targs[] = {
	1, 15, 2, 4, 0, 6, 5, 17, 
	15, 22, 8, 15, 9, 23, 15, 11, 
	12, 15, 15, 31, 32, 15, 16, 3, 
	7, 10, 24, 27, 29, 14, 33, 15, 
	15, 18, 19, 20, 21, 15, 15, 15, 
	15, 25, 15, 26, 15, 28, 15, 15, 
	30, 13, 15, 15
};

static const char _SCSST_trans_actions[] = {
	0, 9, 0, 0, 0, 0, 0, 0, 
	15, 0, 0, 11, 0, 0, 43, 0, 
	0, 7, 41, 0, 0, 17, 0, 0, 
	0, 0, 0, 5, 5, 0, 0, 39, 
	21, 0, 0, 0, 0, 13, 29, 33, 
	37, 0, 25, 0, 35, 0, 19, 23, 
	0, 0, 27, 31
};

static const char _SCSST_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 1, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const char _SCSST_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 3, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const unsigned char _SCSST_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 15, 15, 19, 0, 0, 
	32, 33, 33, 33, 33, 33, 39, 40, 
	41, 43, 43, 45, 47, 48, 48, 48, 
	51, 52
};

static const int SCSST_start = 15;
static const int SCSST_first_final = 15;
static const int SCSST_error = 0;

static const int SCSST_en_main = 15;


/* #line 123 "SCSST.c.rl" */

ok64 SCSSTLexer(SCSSTstate* state) {

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

    
/* #line 169 "SCSST.rl.c" */
	{
	cs = SCSST_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 141 "SCSST.c.rl" */
    
/* #line 175 "SCSST.rl.c" */
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
	_acts = _SCSST_actions + _SCSST_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 194 "SCSST.rl.c" */
		}
	}

	_keys = _SCSST_trans_keys + _SCSST_key_offsets[cs];
	_trans = _SCSST_index_offsets[cs];

	_klen = _SCSST_single_lengths[cs];
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

	_klen = _SCSST_range_lengths[cs];
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
	_trans = _SCSST_indicies[_trans];
_eof_trans:
	cs = _SCSST_trans_targs[_trans];

	if ( _SCSST_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _SCSST_actions + _SCSST_trans_actions[_trans];
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
/* #line 29 "SCSST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 4:
/* #line 35 "SCSST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 5:
/* #line 35 "SCSST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 6:
/* #line 41 "SCSST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 7:
/* #line 65 "SCSST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 8:
/* #line 65 "SCSST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 9:
/* #line 29 "SCSST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 10:
/* #line 41 "SCSST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 11:
/* #line 41 "SCSST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 41 "SCSST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 47 "SCSST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonAtRule(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 59 "SCSST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonVar(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 53 "SCSST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 53 "SCSST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 65 "SCSST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 65 "SCSST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 71 "SCSST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 41 "SCSST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 65 "SCSST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
/* #line 414 "SCSST.rl.c" */
		}
	}

_again:
	_acts = _SCSST_actions + _SCSST_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 425 "SCSST.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _SCSST_eof_trans[cs] > 0 ) {
		_trans = _SCSST_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 142 "SCSST.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < SCSST_first_final)
        o = SCSSTBAD;

    return o;
}
