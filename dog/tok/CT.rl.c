
#line 1 "CT.c.rl"
#include "abc/INT.h"
#include "abc/PRO.h"
#include "CT.h"

// user functions (callbacks) for the parser
ok64 CTonComment (u8cs tok, CTstate* state);
ok64 CTonString (u8cs tok, CTstate* state);
ok64 CTonNumber (u8cs tok, CTstate* state);
ok64 CTonPreproc (u8cs tok, CTstate* state);
ok64 CTonWord (u8cs tok, CTstate* state);
ok64 CTonPunct (u8cs tok, CTstate* state);
ok64 CTonSpace (u8cs tok, CTstate* state);


#line 151 "CT.c.rl"



#line 17 "CT.rl.c"
static const int CT_start = 41;
static const int CT_first_final = 41;
static const int CT_error = 0;

static const int CT_en_main = 41;


#line 154 "CT.c.rl"

// the public API function
ok64 CTLexer(CTstate* state) {

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

    
#line 41 "CT.rl.c"
	{
	cs = CT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

#line 173 "CT.c.rl"
    
#line 47 "CT.rl.c"
	{
	if ( p == pe )
		goto _test_eof;
	switch ( cs )
	{
tr0:
#line 1 "NONE"
	{	switch( act ) {
	case 0:
	{{goto st0;}}
	break;
	case 5:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}
	break;
	case 6:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}
	break;
	case 7:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}
	break;
	case 8:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}
	break;
	case 11:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}
	break;
	case 13:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonPreproc(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}
	break;
	case 14:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonPreproc(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}
	break;
	case 16:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonWord(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}
	break;
	case 17:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonPunct(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}
	break;
	case 18:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonPunct(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}
	break;
	}
	}
	goto st41;
tr2:
#line 44 "CT.c.rl"
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonString(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr12:
#line 68 "CT.c.rl"
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonPunct(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr23:
#line 44 "CT.c.rl"
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonString(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr33:
#line 68 "CT.c.rl"
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonPunct(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr34:
#line 68 "CT.c.rl"
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonPunct(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr35:
#line 50 "CT.c.rl"
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr39:
#line 68 "CT.c.rl"
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonPunct(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr42:
#line 38 "CT.c.rl"
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonComment(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr44:
#line 50 "CT.c.rl"
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr52:
#line 50 "CT.c.rl"
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr58:
#line 68 "CT.c.rl"
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonPunct(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr74:
#line 74 "CT.c.rl"
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonSpace(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr75:
#line 68 "CT.c.rl"
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonPunct(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr76:
#line 56 "CT.c.rl"
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonPreproc(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr90:
#line 56 "CT.c.rl"
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonPreproc(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr105:
#line 62 "CT.c.rl"
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonWord(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr106:
#line 68 "CT.c.rl"
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonPunct(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr107:
#line 68 "CT.c.rl"
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonPunct(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr109:
#line 50 "CT.c.rl"
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr111:
#line 50 "CT.c.rl"
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr114:
#line 38 "CT.c.rl"
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonComment(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr115:
#line 50 "CT.c.rl"
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr124:
#line 50 "CT.c.rl"
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr125:
#line 50 "CT.c.rl"
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr128:
#line 50 "CT.c.rl"
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr129:
#line 50 "CT.c.rl"
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr131:
#line 50 "CT.c.rl"
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr133:
#line 50 "CT.c.rl"
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr137:
#line 50 "CT.c.rl"
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr139:
#line 50 "CT.c.rl"
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr143:
#line 50 "CT.c.rl"
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr145:
#line 50 "CT.c.rl"
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr149:
#line 50 "CT.c.rl"
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr150:
#line 50 "CT.c.rl"
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
tr152:
#line 50 "CT.c.rl"
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) {p++; cs = 41; goto _out;}
}}
	goto st41;
st41:
#line 1 "NONE"
	{ts = 0;}
#line 1 "NONE"
	{act = 0;}
	if ( ++p == pe )
		goto _test_eof41;
case 41:
#line 1 "NONE"
	{ts = p;}
#line 435 "CT.rl.c"
	switch( (*p) ) {
		case 32u: goto st42;
		case 34u: goto st1;
		case 35u: goto tr61;
		case 36u: goto st79;
		case 38u: goto st80;
		case 39u: goto st12;
		case 42u: goto tr60;
		case 43u: goto st81;
		case 45u: goto st82;
		case 46u: goto tr66;
		case 47u: goto tr67;
		case 48u: goto tr68;
		case 60u: goto st114;
		case 61u: goto tr60;
		case 62u: goto st115;
		case 76u: goto tr71;
		case 85u: goto tr71;
		case 94u: goto tr60;
		case 95u: goto st79;
		case 117u: goto tr72;
		case 124u: goto st118;
	}
	if ( (*p) < 49u ) {
		if ( (*p) > 13u ) {
			if ( 33u <= (*p) && (*p) <= 37u )
				goto tr60;
		} else if ( (*p) >= 9u )
			goto st42;
	} else if ( (*p) > 57u ) {
		if ( (*p) > 90u ) {
			if ( 97u <= (*p) && (*p) <= 122u )
				goto st79;
		} else if ( (*p) >= 65u )
			goto st79;
	} else
		goto tr43;
	goto tr58;
st42:
	if ( ++p == pe )
		goto _test_eof42;
case 42:
	if ( (*p) == 32u )
		goto st42;
	if ( 9u <= (*p) && (*p) <= 13u )
		goto st42;
	goto tr74;
tr60:
#line 1 "NONE"
	{te = p+1;}
#line 68 "CT.c.rl"
	{act = 18;}
	goto st43;
tr154:
#line 1 "NONE"
	{te = p+1;}
#line 68 "CT.c.rl"
	{act = 17;}
	goto st43;
st43:
	if ( ++p == pe )
		goto _test_eof43;
case 43:
#line 494 "CT.rl.c"
	if ( (*p) == 61u )
		goto tr34;
	goto tr0;
st1:
	if ( ++p == pe )
		goto _test_eof1;
case 1:
	switch( (*p) ) {
		case 34u: goto tr2;
		case 92u: goto st2;
	}
	goto st1;
st2:
	if ( ++p == pe )
		goto _test_eof2;
case 2:
	switch( (*p) ) {
		case 34u: goto st1;
		case 39u: goto st1;
		case 63u: goto st1;
		case 85u: goto st3;
		case 92u: goto st1;
		case 110u: goto st1;
		case 114u: goto st1;
		case 117u: goto st7;
		case 120u: goto st10;
	}
	if ( (*p) < 97u ) {
		if ( 48u <= (*p) && (*p) <= 55u )
			goto st1;
	} else if ( (*p) > 98u ) {
		if ( (*p) > 102u ) {
			if ( 116u <= (*p) && (*p) <= 118u )
				goto st1;
		} else if ( (*p) >= 101u )
			goto st1;
	} else
		goto st1;
	goto tr0;
st3:
	if ( ++p == pe )
		goto _test_eof3;
case 3:
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st4;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto st4;
	} else
		goto st4;
	goto tr0;
st4:
	if ( ++p == pe )
		goto _test_eof4;
case 4:
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st5;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto st5;
	} else
		goto st5;
	goto tr0;
st5:
	if ( ++p == pe )
		goto _test_eof5;
case 5:
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st6;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto st6;
	} else
		goto st6;
	goto tr0;
st6:
	if ( ++p == pe )
		goto _test_eof6;
case 6:
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st7;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto st7;
	} else
		goto st7;
	goto tr0;
st7:
	if ( ++p == pe )
		goto _test_eof7;
case 7:
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st8;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto st8;
	} else
		goto st8;
	goto tr0;
st8:
	if ( ++p == pe )
		goto _test_eof8;
case 8:
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st9;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto st9;
	} else
		goto st9;
	goto tr0;
st9:
	if ( ++p == pe )
		goto _test_eof9;
case 9:
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st10;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto st10;
	} else
		goto st10;
	goto tr0;
st10:
	if ( ++p == pe )
		goto _test_eof10;
case 10:
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st1;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto st1;
	} else
		goto st1;
	goto tr0;
tr61:
#line 1 "NONE"
	{te = p+1;}
	goto st44;
st44:
	if ( ++p == pe )
		goto _test_eof44;
case 44:
#line 644 "CT.rl.c"
	switch( (*p) ) {
		case 9u: goto st11;
		case 32u: goto st11;
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 100u: goto st46;
		case 101u: goto st51;
		case 105u: goto st59;
		case 108u: goto st48;
		case 112u: goto st67;
		case 117u: goto st72;
		case 119u: goto st73;
	}
	if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else if ( (*p) >= 65u )
		goto tr14;
	goto tr75;
st11:
	if ( ++p == pe )
		goto _test_eof11;
case 11:
	switch( (*p) ) {
		case 9u: goto st11;
		case 32u: goto st11;
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 100u: goto st46;
		case 101u: goto st51;
		case 105u: goto st59;
		case 108u: goto st48;
		case 112u: goto st67;
		case 117u: goto st72;
		case 119u: goto st73;
	}
	if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else if ( (*p) >= 65u )
		goto tr14;
	goto tr12;
tr14:
#line 1 "NONE"
	{te = p+1;}
#line 56 "CT.c.rl"
	{act = 14;}
	goto st45;
tr80:
#line 1 "NONE"
	{te = p+1;}
#line 56 "CT.c.rl"
	{act = 13;}
	goto st45;
st45:
	if ( ++p == pe )
		goto _test_eof45;
case 45:
#line 698 "CT.rl.c"
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr0;
st46:
	if ( ++p == pe )
		goto _test_eof46;
case 46:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 101u: goto st47;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st47:
	if ( ++p == pe )
		goto _test_eof47;
case 47:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 102u: goto st48;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st48:
	if ( ++p == pe )
		goto _test_eof48;
case 48:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 105u: goto st49;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st49:
	if ( ++p == pe )
		goto _test_eof49;
case 49:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 110u: goto st50;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st50:
	if ( ++p == pe )
		goto _test_eof50;
case 50:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 101u: goto tr80;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st51:
	if ( ++p == pe )
		goto _test_eof51;
case 51:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 108u: goto st52;
		case 110u: goto st54;
		case 114u: goto st56;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st52:
	if ( ++p == pe )
		goto _test_eof52;
case 52:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 105u: goto st53;
		case 115u: goto st50;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st53:
	if ( ++p == pe )
		goto _test_eof53;
case 53:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 102u: goto tr80;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st54:
	if ( ++p == pe )
		goto _test_eof54;
case 54:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 100u: goto st55;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st55:
	if ( ++p == pe )
		goto _test_eof55;
case 55:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 105u: goto st53;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st56:
	if ( ++p == pe )
		goto _test_eof56;
case 56:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 114u: goto st57;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st57:
	if ( ++p == pe )
		goto _test_eof57;
case 57:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 111u: goto st58;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st58:
	if ( ++p == pe )
		goto _test_eof58;
case 58:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 114u: goto tr80;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st59:
	if ( ++p == pe )
		goto _test_eof59;
case 59:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 102u: goto st60;
		case 110u: goto st63;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st60:
	if ( ++p == pe )
		goto _test_eof60;
case 60:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 100u: goto st61;
		case 110u: goto st62;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr90;
st61:
	if ( ++p == pe )
		goto _test_eof61;
case 61:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 101u: goto st53;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st62:
	if ( ++p == pe )
		goto _test_eof62;
case 62:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 100u: goto st61;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st63:
	if ( ++p == pe )
		goto _test_eof63;
case 63:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 99u: goto st64;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st64:
	if ( ++p == pe )
		goto _test_eof64;
case 64:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 108u: goto st65;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st65:
	if ( ++p == pe )
		goto _test_eof65;
case 65:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 117u: goto st66;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st66:
	if ( ++p == pe )
		goto _test_eof66;
case 66:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 100u: goto st50;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st67:
	if ( ++p == pe )
		goto _test_eof67;
case 67:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 114u: goto st68;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st68:
	if ( ++p == pe )
		goto _test_eof68;
case 68:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 97u: goto st69;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 98u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st69:
	if ( ++p == pe )
		goto _test_eof69;
case 69:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 103u: goto st70;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st70:
	if ( ++p == pe )
		goto _test_eof70;
case 70:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 109u: goto st71;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st71:
	if ( ++p == pe )
		goto _test_eof71;
case 71:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 97u: goto tr80;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 98u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st72:
	if ( ++p == pe )
		goto _test_eof72;
case 72:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 110u: goto st62;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st73:
	if ( ++p == pe )
		goto _test_eof73;
case 73:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 97u: goto st74;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 98u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st74:
	if ( ++p == pe )
		goto _test_eof74;
case 74:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 114u: goto st75;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st75:
	if ( ++p == pe )
		goto _test_eof75;
case 75:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 110u: goto st76;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st76:
	if ( ++p == pe )
		goto _test_eof76;
case 76:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 105u: goto st77;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st77:
	if ( ++p == pe )
		goto _test_eof77;
case 77:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 110u: goto st78;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st78:
	if ( ++p == pe )
		goto _test_eof78;
case 78:
	switch( (*p) ) {
		case 36u: goto tr14;
		case 95u: goto tr14;
		case 103u: goto tr80;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr14;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto tr14;
	} else
		goto tr14;
	goto tr76;
st79:
	if ( ++p == pe )
		goto _test_eof79;
case 79:
	switch( (*p) ) {
		case 36u: goto st79;
		case 95u: goto st79;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st79;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto st79;
	} else
		goto st79;
	goto tr105;
st80:
	if ( ++p == pe )
		goto _test_eof80;
case 80:
	switch( (*p) ) {
		case 38u: goto tr34;
		case 61u: goto tr34;
	}
	goto tr106;
st12:
	if ( ++p == pe )
		goto _test_eof12;
case 12:
	switch( (*p) ) {
		case 39u: goto tr23;
		case 92u: goto st13;
	}
	goto st12;
st13:
	if ( ++p == pe )
		goto _test_eof13;
case 13:
	switch( (*p) ) {
		case 34u: goto st12;
		case 39u: goto st12;
		case 63u: goto st12;
		case 85u: goto st14;
		case 92u: goto st12;
		case 110u: goto st12;
		case 114u: goto st12;
		case 117u: goto st18;
		case 120u: goto st21;
	}
	if ( (*p) < 97u ) {
		if ( 48u <= (*p) && (*p) <= 55u )
			goto st12;
	} else if ( (*p) > 98u ) {
		if ( (*p) > 102u ) {
			if ( 116u <= (*p) && (*p) <= 118u )
				goto st12;
		} else if ( (*p) >= 101u )
			goto st12;
	} else
		goto st12;
	goto tr0;
st14:
	if ( ++p == pe )
		goto _test_eof14;
case 14:
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st15;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto st15;
	} else
		goto st15;
	goto tr0;
st15:
	if ( ++p == pe )
		goto _test_eof15;
case 15:
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st16;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto st16;
	} else
		goto st16;
	goto tr0;
st16:
	if ( ++p == pe )
		goto _test_eof16;
case 16:
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st17;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto st17;
	} else
		goto st17;
	goto tr0;
st17:
	if ( ++p == pe )
		goto _test_eof17;
case 17:
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st18;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto st18;
	} else
		goto st18;
	goto tr0;
st18:
	if ( ++p == pe )
		goto _test_eof18;
case 18:
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st19;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto st19;
	} else
		goto st19;
	goto tr0;
st19:
	if ( ++p == pe )
		goto _test_eof19;
case 19:
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st20;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto st20;
	} else
		goto st20;
	goto tr0;
st20:
	if ( ++p == pe )
		goto _test_eof20;
case 20:
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st21;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto st21;
	} else
		goto st21;
	goto tr0;
st21:
	if ( ++p == pe )
		goto _test_eof21;
case 21:
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st12;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto st12;
	} else
		goto st12;
	goto tr0;
st81:
	if ( ++p == pe )
		goto _test_eof81;
case 81:
	switch( (*p) ) {
		case 43u: goto tr34;
		case 61u: goto tr34;
	}
	goto tr106;
st82:
	if ( ++p == pe )
		goto _test_eof82;
case 82:
	if ( (*p) == 45u )
		goto tr34;
	if ( 61u <= (*p) && (*p) <= 62u )
		goto tr34;
	goto tr106;
tr66:
#line 1 "NONE"
	{te = p+1;}
	goto st83;
st83:
	if ( ++p == pe )
		goto _test_eof83;
case 83:
#line 1501 "CT.rl.c"
	if ( (*p) == 46u )
		goto st22;
	if ( 48u <= (*p) && (*p) <= 57u )
		goto tr36;
	goto tr107;
st22:
	if ( ++p == pe )
		goto _test_eof22;
case 22:
	if ( (*p) == 46u )
		goto tr34;
	goto tr33;
tr36:
#line 1 "NONE"
	{te = p+1;}
	goto st84;
st84:
	if ( ++p == pe )
		goto _test_eof84;
case 84:
#line 1520 "CT.rl.c"
	switch( (*p) ) {
		case 39u: goto st23;
		case 69u: goto st24;
		case 76u: goto tr111;
		case 101u: goto st24;
		case 108u: goto tr111;
	}
	if ( (*p) < 68u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr36;
	} else if ( (*p) > 70u ) {
		if ( 100u <= (*p) && (*p) <= 102u )
			goto tr111;
	} else
		goto tr111;
	goto tr109;
st23:
	if ( ++p == pe )
		goto _test_eof23;
case 23:
	if ( 48u <= (*p) && (*p) <= 57u )
		goto tr36;
	goto tr35;
st24:
	if ( ++p == pe )
		goto _test_eof24;
case 24:
	switch( (*p) ) {
		case 43u: goto st25;
		case 45u: goto st25;
	}
	if ( 48u <= (*p) && (*p) <= 57u )
		goto tr38;
	goto tr35;
st25:
	if ( ++p == pe )
		goto _test_eof25;
case 25:
	if ( 48u <= (*p) && (*p) <= 57u )
		goto tr38;
	goto tr35;
tr38:
#line 1 "NONE"
	{te = p+1;}
	goto st85;
st85:
	if ( ++p == pe )
		goto _test_eof85;
case 85:
#line 1568 "CT.rl.c"
	switch( (*p) ) {
		case 39u: goto st25;
		case 68u: goto tr111;
		case 70u: goto tr111;
		case 76u: goto tr111;
		case 100u: goto tr111;
		case 102u: goto tr111;
		case 108u: goto tr111;
	}
	if ( 48u <= (*p) && (*p) <= 57u )
		goto tr38;
	goto tr109;
tr67:
#line 1 "NONE"
	{te = p+1;}
	goto st86;
st86:
	if ( ++p == pe )
		goto _test_eof86;
case 86:
#line 1587 "CT.rl.c"
	switch( (*p) ) {
		case 42u: goto st26;
		case 47u: goto st87;
		case 61u: goto tr34;
	}
	goto tr106;
st26:
	if ( ++p == pe )
		goto _test_eof26;
case 26:
	if ( (*p) == 42u )
		goto st27;
	goto st26;
st27:
	if ( ++p == pe )
		goto _test_eof27;
case 27:
	switch( (*p) ) {
		case 42u: goto st27;
		case 47u: goto tr42;
	}
	goto st26;
st87:
	if ( ++p == pe )
		goto _test_eof87;
case 87:
	if ( (*p) == 10u )
		goto tr114;
	goto st87;
tr68:
#line 1 "NONE"
	{te = p+1;}
#line 50 "CT.c.rl"
	{act = 12;}
	goto st88;
st88:
	if ( ++p == pe )
		goto _test_eof88;
case 88:
#line 1624 "CT.rl.c"
	switch( (*p) ) {
		case 39u: goto st28;
		case 46u: goto tr117;
		case 66u: goto st34;
		case 69u: goto st32;
		case 76u: goto st94;
		case 85u: goto st96;
		case 88u: goto st35;
		case 98u: goto st34;
		case 101u: goto st32;
		case 108u: goto st94;
		case 117u: goto st96;
		case 120u: goto st35;
	}
	if ( (*p) > 55u ) {
		if ( 56u <= (*p) && (*p) <= 57u )
			goto tr43;
	} else if ( (*p) >= 48u )
		goto tr118;
	goto tr115;
st28:
	if ( ++p == pe )
		goto _test_eof28;
case 28:
	if ( 48u <= (*p) && (*p) <= 57u )
		goto tr43;
	goto tr0;
tr43:
#line 1 "NONE"
	{te = p+1;}
#line 50 "CT.c.rl"
	{act = 12;}
	goto st89;
st89:
	if ( ++p == pe )
		goto _test_eof89;
case 89:
#line 1659 "CT.rl.c"
	switch( (*p) ) {
		case 39u: goto st28;
		case 46u: goto tr117;
		case 69u: goto st32;
		case 76u: goto st94;
		case 85u: goto st96;
		case 101u: goto st32;
		case 108u: goto st94;
		case 117u: goto st96;
	}
	if ( 48u <= (*p) && (*p) <= 57u )
		goto tr43;
	goto tr115;
tr117:
#line 1 "NONE"
	{te = p+1;}
	goto st90;
st90:
	if ( ++p == pe )
		goto _test_eof90;
case 90:
#line 1679 "CT.rl.c"
	switch( (*p) ) {
		case 69u: goto st30;
		case 76u: goto tr125;
		case 101u: goto st30;
		case 108u: goto tr125;
	}
	if ( (*p) < 68u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr45;
	} else if ( (*p) > 70u ) {
		if ( 100u <= (*p) && (*p) <= 102u )
			goto tr125;
	} else
		goto tr125;
	goto tr124;
tr45:
#line 1 "NONE"
	{te = p+1;}
	goto st91;
st91:
	if ( ++p == pe )
		goto _test_eof91;
case 91:
#line 1701 "CT.rl.c"
	switch( (*p) ) {
		case 39u: goto st29;
		case 69u: goto st30;
		case 76u: goto tr125;
		case 101u: goto st30;
		case 108u: goto tr125;
	}
	if ( (*p) < 68u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr45;
	} else if ( (*p) > 70u ) {
		if ( 100u <= (*p) && (*p) <= 102u )
			goto tr125;
	} else
		goto tr125;
	goto tr124;
st29:
	if ( ++p == pe )
		goto _test_eof29;
case 29:
	if ( 48u <= (*p) && (*p) <= 57u )
		goto tr45;
	goto tr44;
st30:
	if ( ++p == pe )
		goto _test_eof30;
case 30:
	switch( (*p) ) {
		case 43u: goto st31;
		case 45u: goto st31;
	}
	if ( 48u <= (*p) && (*p) <= 57u )
		goto tr47;
	goto tr44;
st31:
	if ( ++p == pe )
		goto _test_eof31;
case 31:
	if ( 48u <= (*p) && (*p) <= 57u )
		goto tr47;
	goto tr44;
tr47:
#line 1 "NONE"
	{te = p+1;}
	goto st92;
st92:
	if ( ++p == pe )
		goto _test_eof92;
case 92:
#line 1749 "CT.rl.c"
	switch( (*p) ) {
		case 39u: goto st31;
		case 68u: goto tr125;
		case 70u: goto tr125;
		case 76u: goto tr125;
		case 100u: goto tr125;
		case 102u: goto tr125;
		case 108u: goto tr125;
	}
	if ( 48u <= (*p) && (*p) <= 57u )
		goto tr47;
	goto tr124;
st32:
	if ( ++p == pe )
		goto _test_eof32;
case 32:
	switch( (*p) ) {
		case 43u: goto st33;
		case 45u: goto st33;
	}
	if ( 48u <= (*p) && (*p) <= 57u )
		goto tr49;
	goto tr0;
st33:
	if ( ++p == pe )
		goto _test_eof33;
case 33:
	if ( 48u <= (*p) && (*p) <= 57u )
		goto tr49;
	goto tr0;
tr49:
#line 1 "NONE"
	{te = p+1;}
#line 50 "CT.c.rl"
	{act = 11;}
	goto st93;
st93:
	if ( ++p == pe )
		goto _test_eof93;
case 93:
#line 1787 "CT.rl.c"
	switch( (*p) ) {
		case 39u: goto st33;
		case 68u: goto tr129;
		case 70u: goto tr129;
		case 76u: goto tr129;
		case 100u: goto tr129;
		case 102u: goto tr129;
		case 108u: goto tr129;
	}
	if ( 48u <= (*p) && (*p) <= 57u )
		goto tr49;
	goto tr128;
st94:
	if ( ++p == pe )
		goto _test_eof94;
case 94:
	switch( (*p) ) {
		case 76u: goto st95;
		case 85u: goto tr131;
		case 108u: goto st95;
		case 117u: goto tr131;
	}
	goto tr115;
st95:
	if ( ++p == pe )
		goto _test_eof95;
case 95:
	switch( (*p) ) {
		case 85u: goto tr131;
		case 117u: goto tr131;
	}
	goto tr115;
st96:
	if ( ++p == pe )
		goto _test_eof96;
case 96:
	switch( (*p) ) {
		case 76u: goto st97;
		case 108u: goto st97;
	}
	goto tr115;
st97:
	if ( ++p == pe )
		goto _test_eof97;
case 97:
	switch( (*p) ) {
		case 76u: goto tr131;
		case 108u: goto tr131;
	}
	goto tr115;
tr118:
#line 1 "NONE"
	{te = p+1;}
#line 50 "CT.c.rl"
	{act = 8;}
	goto st98;
st98:
	if ( ++p == pe )
		goto _test_eof98;
case 98:
#line 1845 "CT.rl.c"
	switch( (*p) ) {
		case 39u: goto st28;
		case 46u: goto tr117;
		case 69u: goto st32;
		case 76u: goto st99;
		case 85u: goto st101;
		case 101u: goto st32;
		case 108u: goto st99;
		case 117u: goto st101;
	}
	if ( (*p) > 55u ) {
		if ( 56u <= (*p) && (*p) <= 57u )
			goto tr43;
	} else if ( (*p) >= 48u )
		goto tr118;
	goto tr133;
st99:
	if ( ++p == pe )
		goto _test_eof99;
case 99:
	switch( (*p) ) {
		case 76u: goto st100;
		case 85u: goto tr137;
		case 108u: goto st100;
		case 117u: goto tr137;
	}
	goto tr133;
st100:
	if ( ++p == pe )
		goto _test_eof100;
case 100:
	switch( (*p) ) {
		case 85u: goto tr137;
		case 117u: goto tr137;
	}
	goto tr133;
st101:
	if ( ++p == pe )
		goto _test_eof101;
case 101:
	switch( (*p) ) {
		case 76u: goto st102;
		case 108u: goto st102;
	}
	goto tr133;
st102:
	if ( ++p == pe )
		goto _test_eof102;
case 102:
	switch( (*p) ) {
		case 76u: goto tr137;
		case 108u: goto tr137;
	}
	goto tr133;
st34:
	if ( ++p == pe )
		goto _test_eof34;
case 34:
	if ( 48u <= (*p) && (*p) <= 49u )
		goto tr50;
	goto tr0;
tr50:
#line 1 "NONE"
	{te = p+1;}
#line 50 "CT.c.rl"
	{act = 7;}
	goto st103;
st103:
	if ( ++p == pe )
		goto _test_eof103;
case 103:
#line 1914 "CT.rl.c"
	switch( (*p) ) {
		case 39u: goto st34;
		case 76u: goto st104;
		case 85u: goto st106;
		case 108u: goto st104;
		case 117u: goto st106;
	}
	if ( 48u <= (*p) && (*p) <= 49u )
		goto tr50;
	goto tr139;
st104:
	if ( ++p == pe )
		goto _test_eof104;
case 104:
	switch( (*p) ) {
		case 76u: goto st105;
		case 85u: goto tr143;
		case 108u: goto st105;
		case 117u: goto tr143;
	}
	goto tr139;
st105:
	if ( ++p == pe )
		goto _test_eof105;
case 105:
	switch( (*p) ) {
		case 85u: goto tr143;
		case 117u: goto tr143;
	}
	goto tr139;
st106:
	if ( ++p == pe )
		goto _test_eof106;
case 106:
	switch( (*p) ) {
		case 76u: goto st107;
		case 108u: goto st107;
	}
	goto tr139;
st107:
	if ( ++p == pe )
		goto _test_eof107;
case 107:
	switch( (*p) ) {
		case 76u: goto tr143;
		case 108u: goto tr143;
	}
	goto tr139;
st35:
	if ( ++p == pe )
		goto _test_eof35;
case 35:
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr51;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto tr51;
	} else
		goto tr51;
	goto tr0;
tr51:
#line 1 "NONE"
	{te = p+1;}
#line 50 "CT.c.rl"
	{act = 6;}
	goto st108;
st108:
	if ( ++p == pe )
		goto _test_eof108;
case 108:
#line 1983 "CT.rl.c"
	switch( (*p) ) {
		case 39u: goto st35;
		case 46u: goto st36;
		case 76u: goto st110;
		case 80u: goto st39;
		case 85u: goto st112;
		case 108u: goto st110;
		case 112u: goto st39;
		case 117u: goto st112;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto tr51;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto tr51;
	} else
		goto tr51;
	goto tr145;
st36:
	if ( ++p == pe )
		goto _test_eof36;
case 36:
	switch( (*p) ) {
		case 80u: goto st39;
		case 112u: goto st39;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st37;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto st37;
	} else
		goto st37;
	goto tr52;
st37:
	if ( ++p == pe )
		goto _test_eof37;
case 37:
	switch( (*p) ) {
		case 39u: goto st38;
		case 80u: goto st39;
		case 112u: goto st39;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st37;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto st37;
	} else
		goto st37;
	goto tr52;
st38:
	if ( ++p == pe )
		goto _test_eof38;
case 38:
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st37;
	} else if ( (*p) > 70u ) {
		if ( 97u <= (*p) && (*p) <= 102u )
			goto st37;
	} else
		goto st37;
	goto tr52;
st39:
	if ( ++p == pe )
		goto _test_eof39;
case 39:
	switch( (*p) ) {
		case 43u: goto st40;
		case 45u: goto st40;
	}
	if ( 48u <= (*p) && (*p) <= 57u )
		goto tr57;
	goto tr52;
st40:
	if ( ++p == pe )
		goto _test_eof40;
case 40:
	if ( 48u <= (*p) && (*p) <= 57u )
		goto tr57;
	goto tr0;
tr57:
#line 1 "NONE"
	{te = p+1;}
#line 50 "CT.c.rl"
	{act = 5;}
	goto st109;
st109:
	if ( ++p == pe )
		goto _test_eof109;
case 109:
#line 2076 "CT.rl.c"
	switch( (*p) ) {
		case 39u: goto st40;
		case 68u: goto tr150;
		case 70u: goto tr150;
		case 76u: goto tr150;
		case 100u: goto tr150;
		case 102u: goto tr150;
		case 108u: goto tr150;
	}
	if ( 48u <= (*p) && (*p) <= 57u )
		goto tr57;
	goto tr149;
st110:
	if ( ++p == pe )
		goto _test_eof110;
case 110:
	switch( (*p) ) {
		case 76u: goto st111;
		case 85u: goto tr152;
		case 108u: goto st111;
		case 117u: goto tr152;
	}
	goto tr145;
st111:
	if ( ++p == pe )
		goto _test_eof111;
case 111:
	switch( (*p) ) {
		case 85u: goto tr152;
		case 117u: goto tr152;
	}
	goto tr145;
st112:
	if ( ++p == pe )
		goto _test_eof112;
case 112:
	switch( (*p) ) {
		case 76u: goto st113;
		case 108u: goto st113;
	}
	goto tr145;
st113:
	if ( ++p == pe )
		goto _test_eof113;
case 113:
	switch( (*p) ) {
		case 76u: goto tr152;
		case 108u: goto tr152;
	}
	goto tr145;
st114:
	if ( ++p == pe )
		goto _test_eof114;
case 114:
	switch( (*p) ) {
		case 60u: goto tr154;
		case 61u: goto tr34;
	}
	goto tr106;
st115:
	if ( ++p == pe )
		goto _test_eof115;
case 115:
	switch( (*p) ) {
		case 61u: goto tr34;
		case 62u: goto tr154;
	}
	goto tr106;
tr71:
#line 1 "NONE"
	{te = p+1;}
#line 62 "CT.c.rl"
	{act = 16;}
	goto st116;
st116:
	if ( ++p == pe )
		goto _test_eof116;
case 116:
#line 2152 "CT.rl.c"
	switch( (*p) ) {
		case 34u: goto st1;
		case 36u: goto st79;
		case 39u: goto st12;
		case 95u: goto st79;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st79;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto st79;
	} else
		goto st79;
	goto tr105;
tr72:
#line 1 "NONE"
	{te = p+1;}
#line 62 "CT.c.rl"
	{act = 16;}
	goto st117;
st117:
	if ( ++p == pe )
		goto _test_eof117;
case 117:
#line 2175 "CT.rl.c"
	switch( (*p) ) {
		case 34u: goto st1;
		case 36u: goto st79;
		case 39u: goto st12;
		case 56u: goto tr71;
		case 95u: goto st79;
	}
	if ( (*p) < 65u ) {
		if ( 48u <= (*p) && (*p) <= 57u )
			goto st79;
	} else if ( (*p) > 90u ) {
		if ( 97u <= (*p) && (*p) <= 122u )
			goto st79;
	} else
		goto st79;
	goto tr105;
st118:
	if ( ++p == pe )
		goto _test_eof118;
case 118:
	switch( (*p) ) {
		case 61u: goto tr34;
		case 124u: goto tr34;
	}
	goto tr106;
st0:
cs = 0;
	goto _out;
	}
	_test_eof41: cs = 41; goto _test_eof; 
	_test_eof42: cs = 42; goto _test_eof; 
	_test_eof43: cs = 43; goto _test_eof; 
	_test_eof1: cs = 1; goto _test_eof; 
	_test_eof2: cs = 2; goto _test_eof; 
	_test_eof3: cs = 3; goto _test_eof; 
	_test_eof4: cs = 4; goto _test_eof; 
	_test_eof5: cs = 5; goto _test_eof; 
	_test_eof6: cs = 6; goto _test_eof; 
	_test_eof7: cs = 7; goto _test_eof; 
	_test_eof8: cs = 8; goto _test_eof; 
	_test_eof9: cs = 9; goto _test_eof; 
	_test_eof10: cs = 10; goto _test_eof; 
	_test_eof44: cs = 44; goto _test_eof; 
	_test_eof11: cs = 11; goto _test_eof; 
	_test_eof45: cs = 45; goto _test_eof; 
	_test_eof46: cs = 46; goto _test_eof; 
	_test_eof47: cs = 47; goto _test_eof; 
	_test_eof48: cs = 48; goto _test_eof; 
	_test_eof49: cs = 49; goto _test_eof; 
	_test_eof50: cs = 50; goto _test_eof; 
	_test_eof51: cs = 51; goto _test_eof; 
	_test_eof52: cs = 52; goto _test_eof; 
	_test_eof53: cs = 53; goto _test_eof; 
	_test_eof54: cs = 54; goto _test_eof; 
	_test_eof55: cs = 55; goto _test_eof; 
	_test_eof56: cs = 56; goto _test_eof; 
	_test_eof57: cs = 57; goto _test_eof; 
	_test_eof58: cs = 58; goto _test_eof; 
	_test_eof59: cs = 59; goto _test_eof; 
	_test_eof60: cs = 60; goto _test_eof; 
	_test_eof61: cs = 61; goto _test_eof; 
	_test_eof62: cs = 62; goto _test_eof; 
	_test_eof63: cs = 63; goto _test_eof; 
	_test_eof64: cs = 64; goto _test_eof; 
	_test_eof65: cs = 65; goto _test_eof; 
	_test_eof66: cs = 66; goto _test_eof; 
	_test_eof67: cs = 67; goto _test_eof; 
	_test_eof68: cs = 68; goto _test_eof; 
	_test_eof69: cs = 69; goto _test_eof; 
	_test_eof70: cs = 70; goto _test_eof; 
	_test_eof71: cs = 71; goto _test_eof; 
	_test_eof72: cs = 72; goto _test_eof; 
	_test_eof73: cs = 73; goto _test_eof; 
	_test_eof74: cs = 74; goto _test_eof; 
	_test_eof75: cs = 75; goto _test_eof; 
	_test_eof76: cs = 76; goto _test_eof; 
	_test_eof77: cs = 77; goto _test_eof; 
	_test_eof78: cs = 78; goto _test_eof; 
	_test_eof79: cs = 79; goto _test_eof; 
	_test_eof80: cs = 80; goto _test_eof; 
	_test_eof12: cs = 12; goto _test_eof; 
	_test_eof13: cs = 13; goto _test_eof; 
	_test_eof14: cs = 14; goto _test_eof; 
	_test_eof15: cs = 15; goto _test_eof; 
	_test_eof16: cs = 16; goto _test_eof; 
	_test_eof17: cs = 17; goto _test_eof; 
	_test_eof18: cs = 18; goto _test_eof; 
	_test_eof19: cs = 19; goto _test_eof; 
	_test_eof20: cs = 20; goto _test_eof; 
	_test_eof21: cs = 21; goto _test_eof; 
	_test_eof81: cs = 81; goto _test_eof; 
	_test_eof82: cs = 82; goto _test_eof; 
	_test_eof83: cs = 83; goto _test_eof; 
	_test_eof22: cs = 22; goto _test_eof; 
	_test_eof84: cs = 84; goto _test_eof; 
	_test_eof23: cs = 23; goto _test_eof; 
	_test_eof24: cs = 24; goto _test_eof; 
	_test_eof25: cs = 25; goto _test_eof; 
	_test_eof85: cs = 85; goto _test_eof; 
	_test_eof86: cs = 86; goto _test_eof; 
	_test_eof26: cs = 26; goto _test_eof; 
	_test_eof27: cs = 27; goto _test_eof; 
	_test_eof87: cs = 87; goto _test_eof; 
	_test_eof88: cs = 88; goto _test_eof; 
	_test_eof28: cs = 28; goto _test_eof; 
	_test_eof89: cs = 89; goto _test_eof; 
	_test_eof90: cs = 90; goto _test_eof; 
	_test_eof91: cs = 91; goto _test_eof; 
	_test_eof29: cs = 29; goto _test_eof; 
	_test_eof30: cs = 30; goto _test_eof; 
	_test_eof31: cs = 31; goto _test_eof; 
	_test_eof92: cs = 92; goto _test_eof; 
	_test_eof32: cs = 32; goto _test_eof; 
	_test_eof33: cs = 33; goto _test_eof; 
	_test_eof93: cs = 93; goto _test_eof; 
	_test_eof94: cs = 94; goto _test_eof; 
	_test_eof95: cs = 95; goto _test_eof; 
	_test_eof96: cs = 96; goto _test_eof; 
	_test_eof97: cs = 97; goto _test_eof; 
	_test_eof98: cs = 98; goto _test_eof; 
	_test_eof99: cs = 99; goto _test_eof; 
	_test_eof100: cs = 100; goto _test_eof; 
	_test_eof101: cs = 101; goto _test_eof; 
	_test_eof102: cs = 102; goto _test_eof; 
	_test_eof34: cs = 34; goto _test_eof; 
	_test_eof103: cs = 103; goto _test_eof; 
	_test_eof104: cs = 104; goto _test_eof; 
	_test_eof105: cs = 105; goto _test_eof; 
	_test_eof106: cs = 106; goto _test_eof; 
	_test_eof107: cs = 107; goto _test_eof; 
	_test_eof35: cs = 35; goto _test_eof; 
	_test_eof108: cs = 108; goto _test_eof; 
	_test_eof36: cs = 36; goto _test_eof; 
	_test_eof37: cs = 37; goto _test_eof; 
	_test_eof38: cs = 38; goto _test_eof; 
	_test_eof39: cs = 39; goto _test_eof; 
	_test_eof40: cs = 40; goto _test_eof; 
	_test_eof109: cs = 109; goto _test_eof; 
	_test_eof110: cs = 110; goto _test_eof; 
	_test_eof111: cs = 111; goto _test_eof; 
	_test_eof112: cs = 112; goto _test_eof; 
	_test_eof113: cs = 113; goto _test_eof; 
	_test_eof114: cs = 114; goto _test_eof; 
	_test_eof115: cs = 115; goto _test_eof; 
	_test_eof116: cs = 116; goto _test_eof; 
	_test_eof117: cs = 117; goto _test_eof; 
	_test_eof118: cs = 118; goto _test_eof; 

	_test_eof: {}
	if ( p == eof )
	{
	switch ( cs ) {
	case 42: goto tr74;
	case 43: goto tr0;
	case 1: goto tr0;
	case 2: goto tr0;
	case 3: goto tr0;
	case 4: goto tr0;
	case 5: goto tr0;
	case 6: goto tr0;
	case 7: goto tr0;
	case 8: goto tr0;
	case 9: goto tr0;
	case 10: goto tr0;
	case 44: goto tr75;
	case 11: goto tr12;
	case 45: goto tr0;
	case 46: goto tr76;
	case 47: goto tr76;
	case 48: goto tr76;
	case 49: goto tr76;
	case 50: goto tr76;
	case 51: goto tr76;
	case 52: goto tr76;
	case 53: goto tr76;
	case 54: goto tr76;
	case 55: goto tr76;
	case 56: goto tr76;
	case 57: goto tr76;
	case 58: goto tr76;
	case 59: goto tr76;
	case 60: goto tr90;
	case 61: goto tr76;
	case 62: goto tr76;
	case 63: goto tr76;
	case 64: goto tr76;
	case 65: goto tr76;
	case 66: goto tr76;
	case 67: goto tr76;
	case 68: goto tr76;
	case 69: goto tr76;
	case 70: goto tr76;
	case 71: goto tr76;
	case 72: goto tr76;
	case 73: goto tr76;
	case 74: goto tr76;
	case 75: goto tr76;
	case 76: goto tr76;
	case 77: goto tr76;
	case 78: goto tr76;
	case 79: goto tr105;
	case 80: goto tr106;
	case 12: goto tr0;
	case 13: goto tr0;
	case 14: goto tr0;
	case 15: goto tr0;
	case 16: goto tr0;
	case 17: goto tr0;
	case 18: goto tr0;
	case 19: goto tr0;
	case 20: goto tr0;
	case 21: goto tr0;
	case 81: goto tr106;
	case 82: goto tr106;
	case 83: goto tr107;
	case 22: goto tr33;
	case 84: goto tr109;
	case 23: goto tr35;
	case 24: goto tr35;
	case 25: goto tr35;
	case 85: goto tr109;
	case 86: goto tr106;
	case 26: goto tr39;
	case 27: goto tr39;
	case 87: goto tr114;
	case 88: goto tr115;
	case 28: goto tr0;
	case 89: goto tr115;
	case 90: goto tr124;
	case 91: goto tr124;
	case 29: goto tr44;
	case 30: goto tr44;
	case 31: goto tr44;
	case 92: goto tr124;
	case 32: goto tr0;
	case 33: goto tr0;
	case 93: goto tr128;
	case 94: goto tr115;
	case 95: goto tr115;
	case 96: goto tr115;
	case 97: goto tr115;
	case 98: goto tr133;
	case 99: goto tr133;
	case 100: goto tr133;
	case 101: goto tr133;
	case 102: goto tr133;
	case 34: goto tr0;
	case 103: goto tr139;
	case 104: goto tr139;
	case 105: goto tr139;
	case 106: goto tr139;
	case 107: goto tr139;
	case 35: goto tr0;
	case 108: goto tr145;
	case 36: goto tr52;
	case 37: goto tr52;
	case 38: goto tr52;
	case 39: goto tr52;
	case 40: goto tr0;
	case 109: goto tr149;
	case 110: goto tr145;
	case 111: goto tr145;
	case 112: goto tr145;
	case 113: goto tr145;
	case 114: goto tr106;
	case 115: goto tr106;
	case 116: goto tr105;
	case 117: goto tr105;
	case 118: goto tr106;
	}
	}

	_out: {}
	}

#line 174 "CT.c.rl"

    state->data[0] = p;
    if (o==OK && cs < CT_first_final)
        o = CTBAD;

    return o;
}
