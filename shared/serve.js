//  serve.js — the JS keeper SERVE side (GIT-020).  `jab upload-pack <sel>`
//  speaks the keeper wire protocol (v0, NO side-band) over stdin/stdout so the
//  be:/keeper: transport runs jab-to-jab, retiring the native keeper daemon.
//  Reuses wire.js's serve primitives (serveReader/markReachable/buildPushPack)
//  and pkt.js framing — no forked pack/closure impl.  Mirrors the CLIENT in
//  wire.js fetch(): advertise refs → read want/have/done → NAK + RAW pack.
"use strict";

const pkt = require("./pkt.js");
const wire = require("./wire.js");
const branchlib = require("./branch.js");   // SUBS-050: the ONE branch codec
const isFullSha = require("./util/sha.js").isFullSha;

const ZERO_SHA = "0000000000000000000000000000000000000000";

//  GIT-020: write one advert pkt-line `<sha> <name>[\0caps]\n` on the first
//  ref; caps carried once (keeper advertises NO side-band-64k).
function advLine(sha, name, caps) {
  let s = sha + " " + name;
  if (caps != null) s += "\0" + caps;
  return pkt.frame(s + "\n");
}

//  GIT-020: write fd — Uint8Array out to a blocking fd (fd 1 = stdout).
function w(fd, bytes) { io.writeAll(fd, bytes); }

//  GIT-020: the upload-pack (FETCH) serve loop over (rfd, wfd).  Advertise the
//  store's refs (+ a HEAD alias for the trunk tip), read want/have/done, then
//  write `NAK\n` and stream the RAW packfile.  No side-band demux.
function uploadPack(selector, rfd, wfd) {
  const reader = wire.serveReader(selector);

  //  1. advertisement: HEAD (trunk tip) first so a no-branch fetch resolves via
  //  pickWant, then every local branch tip.  Caps ride the FIRST line only.
  const trunk = reader.resolveRef("");
  const tips = [];
  reader.eachTip(function (t) { tips.push(t); });
  const caps = "ofs-delta";                     // NO side-band-64k advertised
  let first = true;
  function emitRef(sha, name) {
    w(wfd, advLine(sha, name, first ? caps : null));
    first = false;
  }
  if (trunk && isFullSha(trunk)) emitRef(trunk, "HEAD");
  for (const t of tips)          // SUBS-050: trunk (branch "") advertises as refs/heads/main
    emitRef(t.sha, branchlib.wireRef(branchlib.parse(t.branch || "", "")));
  //  A store with no tips at all still needs a valid (empty) advert.
  if (first) w(wfd, advLine(ZERO_SHA, "capabilities^{}", caps));
  w(wfd, pkt.flushPkt());

  //  2. negotiation: want <sha> [caps]… flush, optional have <sha>…, done.
  const reader2 = pkt.Reader(rfd);
  const wants = [], haves = [];
  for (;;) {
    const ev = reader2.next();
    if (ev.kind === pkt.EOF) break;
    if (ev.kind === pkt.FLUSH) continue;
    if (ev.kind !== pkt.LINE) continue;
    const s = utf8.Decode(ev.payload).replace(/\n$/, "");
    if (s.indexOf("want ") === 0) {
      const sha = s.slice(5).split(" ")[0];
      if (isFullSha(sha)) wants.push(sha);
    } else if (s.indexOf("have ") === 0) {
      const sha = s.slice(5).split(" ")[0];
      if (isFullSha(sha)) haves.push(sha);
    } else if (s === "done") break;
  }

  //  3. NAK then the RAW pack (buildPushPack = the shared want-minus-have
  //  closure + emit).  No wants (advert-only probe) → NAK + empty flush-close.
  if (!wants.length) {
    w(wfd, pkt.frame("NAK\n"));
    return;
  }
  const pack = wire.buildPushPack(selector, wants[0], haves);
  w(wfd, pkt.frame("NAK\n"));
  w(wfd, pack);
}

module.exports = { uploadPack: uploadPack };
