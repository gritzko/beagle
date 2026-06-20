
/* #line 1 "MKDTB.c.rl" */
//  MKDTB — the StrictMark block grammar, in 4-char blocks.
//
//  Per wiki/StrictMark the block layer is a regular language: every line's
//  structural prefix is a run of 4-space indent (div) blocks followed by at
//  most one marker, and the whole-line leaf shapes (heading, code fence,
//  ruler, reference definition) are likewise fixed.  This one machine owns all
//  of it — there is no hand-rolled line classification left in MKDT.c.
//
//  A marker is exactly 4 chars wide: a single '>' (quote) or '-' (bullet)
//  padded with spaces in any of the four columns, 1-3 digits then '.' filling
//  the slot (numbered), or a -[ ]/-[v]/-[-]/-[x] todo.  Anything off-grammar (e.g.
//  "-- ", a two-dash run) leaves marker NONE, so the line is a paragraph.  A
//  header needs the gap space the spec mandates ("####" alone is not one); a
//  ruler is 3-4 dashes with a blank rest (the 3-dash short exception); a code
//  fence is a run of >=3 backticks (the wrapper accepts only 3 or 4).
//
//  Fields are set by FINISHING actions (@), which fire on the transition that
//  consumes a token's last byte — so they fire even when the next byte is
//  ordinary content (the common case), unlike leaving actions (%), which do
//  not fire on a dead-end transition.  `content` is set to p+1 (one past the
//  matched byte) = the first content byte.  One line, one classification.
//
//  Build: ragel -C MKDTB.c.rl -o MKDTB.rl.c -L

#include "MKDT.h"
#include "abc/PRO.h"


/* #line 81 "MKDTB.c.rl" */



/* #line 31 "MKDTB.rl.c" */
static const char _mkdtb_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11
};

static const char _mkdtb_key_offsets[] = {
	0, 0, 5, 10, 13, 14, 15, 16, 
	17, 20, 21, 22, 24, 26, 28, 29, 
	32, 33, 38, 42, 48, 49, 50, 53, 
	54, 55, 61, 62, 63, 64, 65, 73, 
	73, 74
};

static const unsigned char _mkdtb_trans_keys[] = {
	32u, 45u, 62u, 48u, 57u, 32u, 45u, 62u, 
	48u, 57u, 32u, 45u, 62u, 32u, 46u, 32u, 
	32u, 46u, 48u, 57u, 32u, 32u, 32u, 35u, 
	32u, 35u, 32u, 35u, 32u, 32u, 45u, 91u, 
	45u, 9u, 10u, 13u, 32u, 45u, 9u, 10u, 
	13u, 32u, 32u, 45u, 86u, 88u, 118u, 120u, 
	93u, 32u, 46u, 48u, 57u, 32u, 32u, 48u, 
	57u, 65u, 90u, 97u, 122u, 93u, 58u, 96u, 
	96u, 32u, 35u, 45u, 62u, 91u, 96u, 48u, 
	57u, 32u, 96u, 0
};

static const char _mkdtb_single_lengths[] = {
	0, 3, 3, 3, 1, 1, 1, 1, 
	1, 1, 1, 2, 2, 2, 1, 3, 
	1, 5, 4, 6, 1, 1, 1, 1, 
	1, 0, 1, 1, 1, 1, 6, 0, 
	1, 1
};

static const char _mkdtb_range_lengths[] = {
	0, 1, 1, 0, 0, 0, 0, 0, 
	1, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 1, 0, 
	0, 3, 0, 0, 0, 0, 1, 0, 
	0, 0
};

static const unsigned char _mkdtb_index_offsets[] = {
	0, 0, 5, 10, 14, 16, 18, 20, 
	22, 25, 27, 29, 32, 35, 38, 40, 
	44, 46, 52, 57, 64, 66, 68, 71, 
	73, 75, 79, 81, 83, 85, 87, 95, 
	96, 98
};

static const char _mkdtb_indicies[] = {
	0, 2, 4, 3, 1, 5, 6, 8, 
	7, 1, 9, 10, 11, 1, 10, 1, 
	12, 1, 11, 1, 6, 1, 13, 7, 
	1, 12, 1, 8, 1, 14, 15, 1, 
	14, 16, 1, 14, 17, 1, 14, 1, 
	2, 18, 19, 1, 20, 1, 21, 22, 
	21, 21, 21, 1, 21, 22, 21, 21, 
	1, 23, 23, 23, 23, 23, 23, 1, 
	24, 1, 25, 1, 26, 3, 1, 13, 
	1, 4, 1, 27, 27, 27, 1, 28, 
	1, 29, 1, 30, 1, 31, 1, 32, 
	33, 34, 36, 37, 38, 35, 1, 1, 
	39, 1, 31, 1, 0
};

static const char _mkdtb_trans_targs[] = {
	2, 0, 7, 8, 10, 3, 4, 5, 
	6, 30, 31, 31, 31, 9, 32, 12, 
	13, 14, 16, 19, 17, 18, 31, 20, 
	21, 31, 23, 26, 27, 31, 29, 33, 
	1, 11, 15, 22, 24, 25, 28, 32
};

static const char _mkdtb_trans_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 5, 3, 7, 0, 13, 0, 
	0, 0, 0, 0, 0, 0, 21, 0, 
	0, 9, 0, 0, 0, 23, 0, 19, 
	0, 11, 0, 0, 0, 0, 17, 15
};

static const int mkdtb_start = 30;
static const int mkdtb_first_final = 30;
static const int mkdtb_error = 0;

static const int mkdtb_en_main = 30;


/* #line 84 "MKDTB.c.rl" */

void MKDTBlock(u8csc line, mkdtblock *b) {
    b->depth = 0;
    b->marker = MKDT_MARK_NONE;
    b->heading = 0;
    b->fence = 0;
    b->fence_blank = NO;
    b->hrule = NO;
    b->refdef = NO;
    a_dup(u8c, data, line);
    const unsigned char *p = (const unsigned char *)data[0];
    const unsigned char *pe = (const unsigned char *)data[1];
    const unsigned char *eof = pe;
    const unsigned char *hs = p, *fs = p;
    b->content = (const u8c *)p;   // default: content begins after the indents
    int cs;
    
/* #line 137 "MKDTB.rl.c" */
	{
	cs = mkdtb_start;
	}

/* #line 101 "MKDTB.c.rl" */
    
/* #line 140 "MKDTB.rl.c" */
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
	_keys = _mkdtb_trans_keys + _mkdtb_key_offsets[cs];
	_trans = _mkdtb_index_offsets[cs];

	_klen = _mkdtb_single_lengths[cs];
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

	_klen = _mkdtb_range_lengths[cs];
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
	_trans = _mkdtb_indicies[_trans];
	cs = _mkdtb_trans_targs[_trans];

	if ( _mkdtb_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _mkdtb_actions + _mkdtb_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
/* #line 32 "MKDTB.c.rl" */
	{ b->depth += 1; b->content = (const u8c *)(p + 1); }
	break;
	case 1:
/* #line 33 "MKDTB.c.rl" */
	{ b->marker = MKDT_MARK_QUOTE; b->content = (const u8c *)(p + 1); }
	break;
	case 2:
/* #line 34 "MKDTB.c.rl" */
	{ b->marker = MKDT_MARK_ULIST; b->content = (const u8c *)(p + 1); }
	break;
	case 3:
/* #line 35 "MKDTB.c.rl" */
	{ b->marker = MKDT_MARK_OLIST; b->content = (const u8c *)(p + 1); }
	break;
	case 4:
/* #line 36 "MKDTB.c.rl" */
	{ b->marker = MKDT_MARK_TODO;  b->content = (const u8c *)(p + 1); }
	break;
	case 5:
/* #line 37 "MKDTB.c.rl" */
	{ hs = p; }
	break;
	case 6:
/* #line 38 "MKDTB.c.rl" */
	{ b->heading = (int)(p - hs); b->content = (const u8c *)(p + 1); }
	break;
	case 7:
/* #line 39 "MKDTB.c.rl" */
	{ b->content = (const u8c *)(p + 1); }
	break;
	case 8:
/* #line 40 "MKDTB.c.rl" */
	{ fs = p; }
	break;
	case 9:
/* #line 41 "MKDTB.c.rl" */
	{ b->fence = (int)(p + 1 - fs); b->content = (const u8c *)(p + 1); }
	break;
	case 10:
/* #line 42 "MKDTB.c.rl" */
	{ b->hrule = YES; }
	break;
	case 11:
/* #line 43 "MKDTB.c.rl" */
	{ b->refdef = YES; }
	break;
/* #line 249 "MKDTB.rl.c" */
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

/* #line 102 "MKDTB.c.rl" */

    //  A fence closes a block iff the rest after its backtick run is blank; an
    //  info string makes it an opener.  `content` is the byte past the run.
    if (b->fence >= 3) {
        b->fence_blank = YES;
        for (const unsigned char *q = (const unsigned char *)b->content; q < pe;
             ++q)
            if (*q != ' ' && *q != '\t' && *q != '\r' && *q != '\n') {
                b->fence_blank = NO;
                break;
            }
    }
    (void)eof; (void)hs; (void)fs;
    (void)mkdtb_en_main; (void)mkdtb_error; (void)mkdtb_first_final;
}
