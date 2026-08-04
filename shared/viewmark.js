//  shared/viewmark.js — CI-004 (TODO 8): the per-view MARKS a view leaves for
//  the pager.  A view is a plain verb: it emits its hunks and returns, so
//  anything it wants BRO to keep doing afterwards has to be said somewhere —
//  here.  The marks are a PAGER MECHANISM, not ci code; the ci log tail is
//  merely the first user, and any view may set them.
//    tick(ms)  re-run this view every ~ms with NO keypress (a LIVE view)
//    end()     show this view at its LAST page (a tail)
//  One-shot by construction: the view sets a mark while it renders and the
//  pager TAKES it right after the drive, hanging it on THAT view object — so a
//  mark belongs to one view and bro honours only the TOPMOST one.  An unmarked
//  view takes nothing away: take() reports zeros, the pager waits on the key
//  alone (no timeout, no idle work), exactly as it always has.
"use strict";

//  `jsrc` is a symlink to `.`, so this file can load as TWO module instances —
//  the pending marks live in ONE globalThis slot (shared/ci.js's rule).
const M = globalThis.__BE_VIEWMARK__ ||
          (globalThis.__BE_VIEWMARK__ = { tick: 0, end: false });

//  Refresh this view every ~`ms`; 0 (or less) is "quiet", the default.
function tick(ms) { M.tick = ms > 0 ? ms : 0; }
//  Open/keep this view at its last page.
function end() { M.end = true; }

//  CONSUME the pending marks and disarm — the pager calls this ONCE per drive,
//  so a view that set nothing clears whatever the previous one set.
function take() {
  const m = { tick: M.tick, end: M.end };
  M.tick = 0; M.end = false;
  return m;
}

module.exports = { tick: tick, end: end, take: take };
