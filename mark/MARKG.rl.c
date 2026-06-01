
/* #line 1 "MARKG.c.rl" */
//  MARKG — inline G-token decomposer for the mark dog.
//  MKDT emits emphasis / link / image as whole-span 'G' tokens; this
//  ragel machine splits one into (kind, text, label) so the renderer can
//  emit <strong>/<em>/<del>/<a>/<img> without a hand-rolled scan.
//
//  Forms: *strong*  _emph_  ~~del~~  [text][x]  ![text][x]  [text][]  [text]
//  Reference labels (the [x] in [text][x]) stay one symbol; collapsed and
//  shortcut links carry no label, so the renderer keys them on `text`.
//
//  Build: ragel -C MARKG.c.rl -o MARKG.rl.c -L

#include "abc/INT.h"
#include "abc/PRO.h"
#include "MARK.h"


/* #line 39 "MARKG.c.rl" */



/* #line 19 "MARKG.rl.c" */
static const char _markg_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	6, 1, 7, 2, 0, 1, 2, 1, 
	4, 2, 1, 5, 2, 1, 7, 2, 
	3, 7, 2, 3, 8, 3, 0, 1, 
	4, 3, 0, 1, 5, 3, 0, 1, 
	7
};

static const char _markg_key_offsets[] = {
	0, 0, 5, 6, 8, 10, 11, 17, 
	18, 20, 22, 24, 26, 33, 34, 36, 
	38, 39, 41, 43, 45, 45
};

static const unsigned char _markg_trans_keys[] = {
	33u, 42u, 91u, 95u, 126u, 91u, 10u, 93u, 
	10u, 93u, 91u, 48u, 57u, 65u, 90u, 97u, 
	122u, 93u, 10u, 42u, 10u, 42u, 10u, 93u, 
	10u, 93u, 93u, 48u, 57u, 65u, 90u, 97u, 
	122u, 93u, 10u, 95u, 10u, 95u, 126u, 10u, 
	126u, 10u, 126u, 10u, 126u, 91u, 0
};

static const char _markg_single_lengths[] = {
	0, 5, 1, 2, 2, 1, 0, 1, 
	2, 2, 2, 2, 1, 1, 2, 2, 
	1, 2, 2, 2, 0, 1
};

static const char _markg_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 3, 0, 
	0, 0, 0, 0, 3, 0, 0, 0, 
	0, 0, 0, 0, 0, 0
};

static const char _markg_index_offsets[] = {
	0, 0, 6, 8, 11, 14, 16, 20, 
	22, 25, 28, 31, 34, 39, 41, 44, 
	47, 49, 52, 55, 58, 59
};

static const char _markg_trans_targs[] = {
	2, 8, 10, 14, 16, 0, 3, 0, 
	0, 5, 4, 0, 5, 4, 6, 0, 
	7, 7, 7, 0, 20, 0, 0, 20, 
	9, 0, 20, 9, 0, 21, 11, 0, 
	21, 11, 20, 13, 13, 13, 0, 20, 
	0, 0, 20, 15, 0, 20, 15, 17, 
	0, 0, 19, 18, 0, 19, 18, 0, 
	20, 18, 0, 12, 0, 0
};

static const char _markg_trans_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 11, 1, 0, 3, 0, 0, 0, 
	5, 5, 5, 0, 26, 0, 0, 29, 
	1, 0, 14, 0, 0, 37, 1, 0, 
	20, 0, 9, 5, 5, 5, 0, 23, 
	0, 0, 33, 1, 0, 17, 0, 0, 
	0, 0, 11, 1, 0, 3, 0, 0, 
	7, 0, 0, 0, 0, 0
};

static const int markg_start = 1;
static const int markg_first_final = 20;
static const int markg_error = 0;

static const int markg_en_main = 1;


/* #line 42 "MARKG.c.rl" */

ok64 MARKDecomposeG(markg *g, u8csc tok) {
    a_dup(u8c, data, tok);

    int cs;
    u8c *p = (u8c *)data[0];
    u8c *pe = (u8c *)data[1];
    u8c *eof = pe;
    u8c *txt0 = NULL, *txt1 = NULL, *lbl0 = NULL, *lbl1 = NULL;
    u8 kind = 0;

    
/* #line 100 "MARKG.rl.c" */
	{
	cs = markg_start;
	}

/* #line 54 "MARKG.c.rl" */
    
/* #line 103 "MARKG.rl.c" */
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
	_keys = _markg_trans_keys + _markg_key_offsets[cs];
	_trans = _markg_index_offsets[cs];

	_klen = _markg_single_lengths[cs];
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

	_klen = _markg_range_lengths[cs];
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
	cs = _markg_trans_targs[_trans];

	if ( _markg_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _markg_actions + _markg_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
/* #line 20 "MARKG.c.rl" */
	{ txt0 = (u8c *)p; }
	break;
	case 1:
/* #line 21 "MARKG.c.rl" */
	{ txt1 = (u8c *)p; }
	break;
	case 2:
/* #line 22 "MARKG.c.rl" */
	{ lbl0 = (u8c *)p; }
	break;
	case 3:
/* #line 23 "MARKG.c.rl" */
	{ lbl1 = (u8c *)p; }
	break;
	case 4:
/* #line 24 "MARKG.c.rl" */
	{ kind = 'B'; }
	break;
	case 5:
/* #line 25 "MARKG.c.rl" */
	{ kind = 'I'; }
	break;
	case 6:
/* #line 26 "MARKG.c.rl" */
	{ kind = 'D'; }
	break;
	case 7:
/* #line 27 "MARKG.c.rl" */
	{ kind = 'A'; }
	break;
	case 8:
/* #line 28 "MARKG.c.rl" */
	{ kind = 'M'; }
	break;
/* #line 202 "MARKG.rl.c" */
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

/* #line 55 "MARKG.c.rl" */

    if (cs < markg_first_final) {
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
        //  collapsed/shortcut: key the link on its text
        g->label[0] = g->text[0];
        g->label[1] = g->text[1];
    }
    return OK;
}
