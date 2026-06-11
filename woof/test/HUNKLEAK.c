//  woof/test/HUNKLEAK.c — MEM-038 repro: a long-running woof server must
//  NOT leak BASS arena on every syntax-highlighted (HUNK_TLV_TOK) response.
//
//  abc/POL.c invokes woof's pipe_cb / serve_inproc callbacks DIRECTLY
//  (POL.c:248,314) with NO enclosing call()/try() frame — and only
//  call/try/then snapshot+rewind ABC_BASS (PRO.h:82-83).  Those callbacks
//  reach WOOFRenderHunks → HUNKu8sDrain → hunk_drain_toks, which a_cquire's
//  a BASS region for the TOK array into hk->toks (dog/HUNK.c) INTENTIONALLY
//  un-wrapped so the toks survive into the renderer.  With no frame to
//  rewind it, every token-bearing hunk permanently consumed ABC_BASS for
//  the process lifetime — a self-inflicted DoS once a_carve hits NOROOM.
//
//  A BASS-arena leak is INVISIBLE to LeakSanitizer (the arena is one big
//  mmap that is never freed).  The witness is the BASS DATA head itself:
//  ABC_BASS[1] is the data boundary that a_cquire advances and that
//  call()/try() rewind on return.  We drive WOOFRenderHunks (the live
//  callback render path) over N token-bearing hunks per "request" and
//  assert the head returns to its pre-request value EVERY time — pre-fix it
//  climbed monotonically, post-fix it is flat.
//
//  We read ABC_BASS[1] directly (no call() between sample and render) so
//  the test observes the raw arena, not a frame's incidental rewind.

#include "woof/WOOF.h"

#include "abc/PRO.h"
#include "abc/TEST.h"
#include "dog/HUNK.h"
#include "dog/tok/TOK.h"

#include <stdio.h>

extern woof WOOF;

//  Current BASS data-head (the boundary a_cquire advances, call()/try()
//  rewind).  A leak shows up as this climbing across requests.
static u8 *bass_data_head(void) { return ((u8 **)ABC_BASS)[1]; }

//  Serialize one token-bearing content hunk (text + one 'S' tok spanning
//  the whole text) into `into`'s idle range.  This is exactly the shape a
//  syntax-highlighted log/diff/blame/blob/grep response carries on the
//  wire — a HUNK 'H' record with a HUNK_TLV_TOK ('K') value.
static ok64 feed_tok_hunk(Bu8 into) {
    sane(1);
    a$str(src, "int main(void) { return 0; }");
    a$str(uri, "x.c");
    tok32  toks_arr[] = { tok32Pack('S', (u32)$len(src)) };
    tok32cs toks = { toks_arr, toks_arr + 1 };
    hunk hk = { .uri  = { uri[0], uri[1] },
                .text = { src[0], src[1] },
                .toks = { toks[0], toks[1] } };
    u8s idle = { into[2], into[3] };
    call(HUNKu8sFeed, idle, &hk);
    into[2] = idle[0];
    done;
}

//  Build a conn whose pipe_in holds `hunks` complete TLV frames and whose
//  out ring is large enough to render them all.  Both views point into the
//  caller-owned BASS-carved `pool` — no real slot mmap needed.
static ok64 build_conn(conn *c, u8 *pool, size_t pipe_bytes,
                       size_t out_bytes, int hunks) {
    sane(c);
    *c = (conn){};
    u8 *p_pipe = pool;
    u8 *p_out  = pool + pipe_bytes;
    c->pipe_in[0] = p_pipe;  c->pipe_in[1] = p_pipe;
    c->pipe_in[2] = p_pipe;  c->pipe_in[3] = p_pipe + pipe_bytes;
    c->out[0] = p_out;  c->out[1] = p_out;
    c->out[2] = p_out;  c->out[3] = p_out + out_bytes;
    //  Fill pipe_in with `hunks` serialized token hunks.
    Bu8 pin = { c->pipe_in[0], c->pipe_in[1], c->pipe_in[2], c->pipe_in[3] };
    for (int i = 0; i < hunks; i++) call(feed_tok_hunk, pin);
    c->pipe_in[2] = pin[2];
    done;
}

//  Drive `requests` render passes, each over a freshly-filled pipe_in of
//  `hunks` token hunks, and assert the BASS data head is identical before
//  and after EVERY pass.  Pre-fix the head climbed by hunks*sizeof(tok32)
//  (rounded up for alignment) per pass and never came back.
static ok64 render_leak_probe(int hunks, int requests) {
    sane(1);
    size_t const PIPE = 1UL << 16;   //  64 KB framing scratch
    size_t const OUT  = 1UL << 20;   //   1 MB render ring (room for all)
    a_carve(u8, pool, PIPE + OUT);

    u8 *baseline = bass_data_head();
    for (int r = 0; r < requests; r++) {
        conn c = {};
        //  build_conn carves into BASS via feed_tok_hunk → HUNKu8sFeed;
        //  capture the head AFTER the build so we measure only the render's
        //  net effect on the arena (the build's frames are torn down by the
        //  call() returns inside build_conn).
        call(build_conn, &c, pool[2], PIPE, OUT, hunks);

        u8 *before = bass_data_head();
        //  BARE call (no call()/try() wrapper) — exactly how POL invokes
        //  woof's callbacks.  WOOFRenderHunks must leave the arena flat on
        //  its own (its internal per-hunk call() reclaims each TOK carve);
        //  a wrapper here would rewind BASS and mask the very leak we test.
        ok64 ro = WOOFRenderHunks(&c);
        u8 *after = bass_data_head();
        if (ro != OK) {
            fprintf(stderr, "MEM-038: WOOFRenderHunks returned non-OK "
                            "(pass %d/%d)\n", r + 1, requests);
            fail(ro);
        }

        if (after != before) {
            fprintf(stderr,
                    "MEM-038 LEAK: BASS data head moved +%ld bytes during "
                    "render pass %d/%d (%d tok hunks) and was NOT rewound\n",
                    (long)(after - before), r + 1, requests, hunks);
            fail(BNOROOM);
        }
        //  And it must equal the very first baseline — no slow drift.
        if (after != baseline) {
            fprintf(stderr,
                    "MEM-038 DRIFT: BASS data head drifted +%ld bytes from "
                    "baseline by request %d/%d\n",
                    (long)(after - baseline), r + 1, requests);
            fail(BNOROOM);
        }
        //  All frames must actually have rendered (the test is only a
        //  leak witness if the carve path really ran).
        if (c.pipe_in[1] != c.pipe_in[2]) {
            fprintf(stderr,
                    "MEM-038 TEST BUG: not all hunks rendered (pass %d)\n",
                    r + 1);
            fail(BNOROOM);
        }
    }
    done;
}

ok64 WOOFHunkLeakTest() {
    sane(1);
    HUNKMode = HUNKOutHtml;
    //  Many requests, each rendering several token hunks: the pre-fix
    //  arena growth is hunks*requests*sizeof(tok32)-ish, brutally obvious.
    call(render_leak_probe, /*hunks=*/8, /*requests=*/256);
    //  Single big request too, to catch a within-request leak.
    call(render_leak_probe, /*hunks=*/64, /*requests=*/16);
    done;
}

TEST(WOOFHunkLeakTest);
