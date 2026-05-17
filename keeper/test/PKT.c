#include "dog/git/PKT.h"

#include <string.h>

#include "abc/PRO.h"
#include "abc/TEST.h"

// ---- Test 1: drain a normal pkt-line ----

ok64 PKTtest1() {
    sane(1);
    // 0x001a = 26 bytes total (4 prefix + 22 payload)
    a_cstr(pkt, "001agit-upload-pack /repo\n");
    u8cs line = {};

    ok64 o = PKTu8sDrain(pkt, line);
    want(o == OK);
    want($len(line) == 22);
    want(memcmp(line[0], "git-upload-pack /repo\n", 22) == 0);

    // input consumed
    want($empty(pkt));

    done;
}

// ---- Test 2: drain flush packet ----

ok64 PKTtest2() {
    sane(1);
    a_cstr(pkt, "0000");
    u8cs line = {};

    ok64 o = PKTu8sDrain(pkt, line);
    want(o == PKTFLUSH);
    want($empty(pkt));

    done;
}

// ---- Test 3: drain delim packet ----

ok64 PKTtest3() {
    sane(1);
    a_cstr(pkt, "0001");
    u8cs line = {};

    ok64 o = PKTu8sDrain(pkt, line);
    want(o == PKTDELIM);

    done;
}

// ---- Test 4: drain two pkt-lines in sequence ----

ok64 PKTtest4() {
    sane(1);
    a_cstr(pkt, "0008abcd0006xy0000");
    u8cs line = {};

    ok64 o = PKTu8sDrain(pkt, line);
    want(o == OK);
    want($len(line) == 4);
    want(memcmp(line[0], "abcd", 4) == 0);

    o = PKTu8sDrain(pkt, line);
    want(o == OK);
    want($len(line) == 2);
    want(memcmp(line[0], "xy", 2) == 0);

    o = PKTu8sDrain(pkt, line);
    want(o == PKTFLUSH);

    want($empty(pkt));

    done;
}

// ---- Test 5: feed a pkt-line ----

ok64 PKTtest5() {
    sane(1);
    a_pad(u8, buf, 128);
    a_cstr(payload, "hello\n");

    ok64 o = PKTu8sFeed(u8bIdle(buf), payload);
    want(o == OK);

    // should have written "000ahello\n"  (4 + 6 = 10 = 0x000a)
    want(memcmp(buf[1], "000ahello\n", 10) == 0);

    done;
}

// ---- Test 6: feed flush ----

ok64 PKTtest6() {
    sane(1);
    a_pad(u8, buf, 16);

    ok64 o = PKTu8sFeedFlush(u8bIdle(buf));
    want(o == OK);
    want(memcmp(buf[1], "0000", 4) == 0);

    done;
}

// ---- Test 7: truncated input ----

ok64 PKTtest7() {
    sane(1);
    a_cstr(pkt, "00");
    u8cs line = {};

    ok64 o = PKTu8sDrain(pkt, line);
    want(o == NODATA);

    done;
}

// ---- Test 8: bad hex ----

ok64 PKTtest8() {
    sane(1);
    a_cstr(pkt, "gggg");
    u8cs line = {};

    ok64 o = PKTu8sDrain(pkt, line);
    want(o == PKTBADFMT);

    done;
}

ok64 maintest() {
    sane(1);
    call(PKTtest1);
    call(PKTtest2);
    call(PKTtest3);
    call(PKTtest4);
    call(PKTtest5);
    call(PKTtest6);
    call(PKTtest7);
    call(PKTtest8);
    done;
}

TEST(maintest)
