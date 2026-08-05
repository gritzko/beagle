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
const store = require("./store.js");        // POST-028: receive-pack ref CAS
const ingest = require("./ingest.js");      // POST-028: land the pushed pack
const dag = require("./dag.js");            // POST-028: server-side FF gate
const pathlib = require("./util/path.js");
const discover = require("../core/discover.js");
const join = pathlib.join;

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

//  POST-028: resolve a serve path to the store reader it addresses.  The PATH
//  selects the project (RULED 2026-07-16): a WORKTREE (its `.be` anchor is a
//  regular FILE — the GET-038 probe) serves its BACKING store+project via the
//  ONE anchor resolver (core/discover.treeAt); anything else is a store root,
//  a `.be` dir or a shard, opened directly (store.shardDir handles all three).
//  A legacy absolute `?/proj` selector still picks the shard; a `?branch`
//  query is IGNORED here (the branch is in-band, never a serve-path part).
function serveStore(selector) {
  const u = new URI(selector);
  let path = (u.path || "").replace(/\/+$/, "") || "/";
  if (path[0] !== "/") path = join(io.cwd(), path);
  let proj = "";
  if (u.query && u.query[0] === "/") proj = u.query.slice(1);
  const beFile = (path.slice(-3) === ".be") ? path : join(path, ".be");
  let kind; try { kind = io.stat(beFile).kind; } catch (e) { kind = undefined; }
  if (kind === "reg") {
    const wt = (beFile === path) ? pathlib.dirname(path) : path;
    const t = discover.treeAt(wt);
    return openServe(t.storePath, proj || t.project || "");
  }
  return openServe(path, proj);
}

//  GIT-020 flat-store retry (mirrors wire.serveReader): a colocated FLAT store
//  has no named shard — fall back to auto-detect when the named one is empty.
function openServe(root, proj) {
  let reader = store.open(root, proj);
  if (proj && reader.resolveRef("") === undefined && !reader.refs().length)
    reader = store.open(root, "");
  return reader;
}

//  POST-028: the receive-pack (PUSH) serve loop over (rfd, wfd) — the twin of
//  uploadPack, mirroring what wire.js's push client sends (pushSession/
//  buildPushBody): advertise the store's branch tips, read the `<old> <new>
//  <ref>[\0caps]` update commands to the flush, stream the raw pack to EOF
//  (verified) into the shard (ingest.land), then FF-gate + CAS each ref and
//  report status (`unpack ok`, `ok|ng <ref>`).  A bare flush (advert-only
//  probe / client-side refusal) exits clean with no report — GIT-019 parity.
function receivePack(selector, rfd, wfd) {
  const reader = serveStore(selector);

  //  1. advertisement: every local branch tip; caps ride the FIRST line only.
  const tips = [];
  reader.eachTip(function (t) { tips.push(t); });
  const caps = "report-status ofs-delta";
  let first = true;
  for (const t of tips) {
    w(wfd, advLine(t.sha,
                   branchlib.wireRef(branchlib.parse(t.branch || "", "")),
                   first ? caps : null));
    first = false;
  }
  if (first) w(wfd, advLine(ZERO_SHA, "capabilities^{}", caps));
  w(wfd, pkt.flushPkt());

  //  2. update commands up to the flush (caps after \0 on the first line).
  const rd = pkt.Reader(rfd);
  const updates = [];
  for (;;) {
    const ev = rd.next();
    if (ev.kind === pkt.EOF || ev.kind === pkt.FLUSH) break;
    if (ev.kind !== pkt.LINE) continue;
    let s = utf8.Decode(ev.payload).replace(/\n$/, "");
    const nul = s.indexOf("\0");
    if (nul >= 0) s = s.slice(0, nul);
    const p = s.split(" ");
    if (p.length === 3 && isFullSha(p[0]) && isFullSha(p[1]))
      updates.push({ old: p[0], neu: p[1], ref: p[2] });
  }
  if (!updates.length) return;              // flush-close = clean no-op exit

  //  3. stream-verify the raw pack into the shard's own FS, then land it
  //  (objects only — refs move in step 4).  Re-open to see the new objects.
  const pf = wire.drainToFile(rfd, rd.rest(), reader.shard);
  ingest.land({ packFile: pf.packFile, packLen: pf.packLen,
                verified: pf.verified }, reader.shard);
  const reader2 = store.open(reader.shard, "");

  //  4. per-ref CAS + FF gate, then report-status.
  const out = [pkt.frame("unpack ok\n")];
  for (const u of updates)
    out.push(pkt.frame(refUpdate(reader2, u) + "\n"));
  out.push(pkt.flushPkt());
  for (const b of out) w(wfd, b);
}

//  POST-028: one ref update — CAS the client's `old` against the live tip,
//  FF-only gate over the landed DAG (POST.mkd: every tip motion is a FF),
//  then the ref append (store.set).  Returns the report-status line.
function refUpdate(reader, u) {
  if (u.ref.indexOf("refs/heads/") !== 0)
    return "ng " + u.ref + " only refs/heads are served";
  if (u.neu === ZERO_SHA)
    return "ng " + u.ref + " ref deletion is not served";
  const key = branchlib.key(branchlib.fromWireRef(u.ref, ""));
  const cur = reader.resolveRef(key) || "";
  const old = (u.old === ZERO_SHA) ? "" : u.old;
  if (cur !== old) return "ng " + u.ref + " ref advanced concurrently, retry";
  if (cur === u.neu) return "ok " + u.ref;            // idempotent at-tip
  if (!reader.getObject(u.neu))
    return "ng " + u.ref + " pack lacks the new tip object";
  if (cur && !dag.isAncestor(reader, cur, u.neu))
    return "ng " + u.ref + " non-fast-forward";
  store.set(reader.shard, key, u.neu);
  return "ok " + u.ref;
}

module.exports = { uploadPack: uploadPack, receivePack: receivePack,
                   serveStore: serveStore };
