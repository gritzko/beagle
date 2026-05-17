//  PSTR: streaming pack-stitcher.  See PSTR.h for the contract.

#include "PSTR.h"

#include <unistd.h>

#include "dog/git/PACK.h"
#include "dog/git/SHA1.h"
#include "abc/FILE.h"
#include "abc/PRO.h"

//  64 KiB scratch.  Big enough to amortize syscall overhead, small
//  enough to live on the stack inside fuzzer-friendly bounds.
#define PSTR_BUF_LOG2 16
#define PSTR_BUF      (1u << PSTR_BUF_LOG2)

//  Drain `length` bytes from `fd` starting at `offset`, hashing them
//  into `hs` and writing them to `out_fd`.  Uses pread so caller's
//  fd seek positions are untouched.
static ok64 PSTRPipeSeg(int out_fd, int fd, u64 offset, u64 length,
                        SHA1state *hs) {
    sane(FILEok(out_fd) && FILEok(fd) && hs);
    a_pad(u8, scratch, PSTR_BUF);
    u64 left = length;
    u64 cur = offset;
    while (left > 0) {
        u64 want = left < PSTR_BUF ? left : PSTR_BUF;
        u8 **idle = u8bIdle(scratch);
        ssize_t got = pread(fd, idle[0], (size_t)want, (off_t)cur);
        if (got <= 0) return PSTRFAIL;
        u8cs chunk = {idle[0], idle[0] + got};
        SHA1Feed(hs, chunk);
        call(FILEFeedAll, out_fd, chunk);
        left -= (u64)got;
        cur += (u64)got;
    }
    done;
}

ok64 PSTRWrite(int out_fd, pstr_segcs segs) {
    sane(FILEok(out_fd));

    //  Sum object counts with u64 overflow check.
    u64 total = 0;
    for (pstr_segcp p = segs[0]; p < segs[1]; ++p) {
        total += (u64)p->count;
        if (total > 0xffffffffu) return PSTRFAIL;
    }

    //  Build the 12-byte PACK header into stack scratch, then
    //  hash + write it.
    a_pad(u8, hdr, 12);
    u8 *hstart = *u8bIdle(hdr);
    call(PACKu8sFeedHdr, u8bIdle(hdr), (u32)total);
    u8cs hbytes = {hstart, *u8bIdle(hdr)};

    SHA1state hs;
    SHA1Open(&hs);
    SHA1Feed(&hs, hbytes);
    call(FILEFeedAll, out_fd, hbytes);

    //  Stream each segment through pread → SHA1Feed → write.
    for (pstr_segcp p = segs[0]; p < segs[1]; ++p) {
        if (p->length == 0) continue;
        call(PSTRPipeSeg, out_fd, p->fd, p->offset, p->length, &hs);
    }

    //  Finalize and append the 20-byte SHA1 trailer.
    sha1 digest = {};
    SHA1Close(&hs, &digest);
    {
        u8cs dslice = {digest.data, digest.data + 20};
        call(FILEFeedAll, out_fd, dslice);
    }

    done;
}
