//  KEEPu8ssDrain — N-way merge cursor over sorted newline-separated
//  path lists.  See keeper/KEEP.h for the contract.
//
//  Each test case feeds a hand-built `inputs` slice (u8css) to the
//  primitive in a loop, asserting the (path, mask) sequence the
//  drain emits and the empty-tail invariant after exhaustion.

#include "keeper/KEEP.h"

#include "abc/PRO.h"
#include "abc/TEST.h"

//  Helper: exhaust the merge into a writable buffer.  After the call,
//  `out` carries the concatenated lines tagged by their masks in the
//  format `<mask>:<path>\n`, one record per drain step.  Returns the
//  number of records emitted.
static u32 drain_all(u8css inputs, u8b out) {
    u32 n = 0;
    for (;;) {
        u8cs path = {};
        u64  mask = 0;
        ok64 r = KEEPu8ssDrain(inputs, path, &mask);
        if (r != OK) break;
        u8bPrintf(out, "%llx:", (unsigned long long)mask);
        u8bFeed(out, path);
        u8bFeed1(out, '\n');
        n++;
    }
    return n;
}

//  Compose a single u8cs cursor from a C string (test fixture).
//  The string must already be sorted and newline-separated.
#define A_INPUT(name, str) a_cstr(name, str)

ok64 MERGEbasic2way() {
    sane(1);

    //  Two sorted inputs with overlap.
    A_INPUT(la, "a\nb\nc\n");
    A_INPUT(lb, "b\nc\nd\n");

    a_pad(u8cs, ins, 2);
    u8cssFeed1(ins_idle, la);
    u8cssFeed1(ins_idle, lb);
    a_dup(u8cs, view, u8csbData(ins));

    a_pad(u8, out, 256);
    u32 n = drain_all(view, out);
    want(n == 4);
    a_cstr(want_str, "1:a\n3:b\n3:c\n2:d\n");
    a_dup(u8c, got, u8bData(out));
    want(u8cscmp(&got, &want_str) == 0);

    //  Inputs are exhausted (empty tails) but slot count unchanged.
    want($len(view) == 2);
    want(u8csEmpty(*$atp(view, 0)));
    want(u8csEmpty(*$atp(view, 1)));
    done;
}

ok64 MERGEthreeWayWithGap() {
    sane(1);

    A_INPUT(la, "alpha\ngamma\n");
    A_INPUT(lb, "beta\ngamma\n");
    A_INPUT(lc, "alpha\nbeta\nzulu\n");

    a_pad(u8cs, ins, 3);
    u8cssFeed1(ins_idle, la);
    u8cssFeed1(ins_idle, lb);
    u8cssFeed1(ins_idle, lc);
    a_dup(u8cs, view, u8csbData(ins));

    a_pad(u8, out, 256);
    u32 n = drain_all(view, out);
    want(n == 4);

    //  Expected order: alpha (a + c → 0b101), beta (b + c → 0b110),
    //  gamma (a + b → 0b011), zulu (c only → 0b100).
    a_cstr(want_str, "5:alpha\n6:beta\n3:gamma\n4:zulu\n");
    a_dup(u8c, got, u8bData(out));
    want(u8cscmp(&got, &want_str) == 0);
    done;
}

ok64 MERGEoneSideOnly() {
    sane(1);

    A_INPUT(la, "a\nb\nc\n");
    A_INPUT(lb, "");

    a_pad(u8cs, ins, 2);
    u8cssFeed1(ins_idle, la);
    u8cssFeed1(ins_idle, lb);
    a_dup(u8cs, view, u8csbData(ins));

    a_pad(u8, out, 256);
    u32 n = drain_all(view, out);
    want(n == 3);
    a_cstr(want_str, "1:a\n1:b\n1:c\n");
    a_dup(u8c, got, u8bData(out));
    want(u8cscmp(&got, &want_str) == 0);
    done;
}

ok64 MERGEnoTrailingNewline() {
    sane(1);

    //  Input "b" lacks a final newline — the drain still consumes it
    //  as one record (and reports the input as exhausted afterwards).
    A_INPUT(la, "a\nb\nc\n");
    A_INPUT(lb, "b\nc");

    a_pad(u8cs, ins, 2);
    u8cssFeed1(ins_idle, la);
    u8cssFeed1(ins_idle, lb);
    a_dup(u8cs, view, u8csbData(ins));

    a_pad(u8, out, 256);
    u32 n = drain_all(view, out);
    want(n == 3);
    a_cstr(want_str, "1:a\n3:b\n3:c\n");
    a_dup(u8c, got, u8bData(out));
    want(u8cscmp(&got, &want_str) == 0);
    done;
}

ok64 MERGEallEmpty() {
    sane(1);
    a_pad(u8cs, ins, 2);
    u8cs e0 = {};
    u8cs e1 = {};
    u8cssFeed1(ins_idle, e0);
    u8cssFeed1(ins_idle, e1);
    a_dup(u8cs, view, u8csbData(ins));

    u8cs path = {};
    u64  mask = 0;
    want(KEEPu8ssDrain(view, path, &mask) == KEEPNONE);
    want(mask == 0);
    done;
}

ok64 maintest() {
    sane(1);
    fprintf(stderr, "MERGEbasic2way...\n");
    call(MERGEbasic2way);
    fprintf(stderr, "MERGEthreeWayWithGap...\n");
    call(MERGEthreeWayWithGap);
    fprintf(stderr, "MERGEoneSideOnly...\n");
    call(MERGEoneSideOnly);
    fprintf(stderr, "MERGEnoTrailingNewline...\n");
    call(MERGEnoTrailingNewline);
    fprintf(stderr, "MERGEallEmpty...\n");
    call(MERGEallEmpty);
    fprintf(stderr, "all passed\n");
    done;
}

TEST(maintest);
