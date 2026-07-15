//  fetchleg.js — PATCH-011: patch's fetch-first leg.  A fetchable source
//  (`file:<store>[?/<proj>[/<br>]][#pin]`, ssh:, http(s):, be://) has
//  its objects landed in the wt's OWN shard BEFORE the all-local triple
//  resolve (the wt→orig cross-store merge, RULING 2026-07-10).  Pure REUSE of
//  get's transfer machinery (parseRemote/wire/ingest) — objects only, NO local
//  tip moves, no wt touch.  Routing lives in patch.js resolveSource.
"use strict";

const store  = require("../../shared/store.js");
const wire   = require("../../shared/wire.js");
const ingest = require("../../shared/ingest.js");
const get    = require("../get/get.js");
const pathlib = require("../../shared/util/path.js");
const isFullSha = require("../../shared/util/sha.js").isFullSha;
const join = pathlib.join;

//  PATCH-011: the fetch-leg URI classes.  RULED 2026-07-10: be: rides the
//  WIRE leg (keeper-over-ssh / local keeper exec — wire.js parity with get);
//  a query-less `file:<path>` routes via the verb's wt-vs-store probe below.
function isFetchable(arg) {
  let u; try { u = new URI(String(arg || "")); } catch (e) { return false; }
  const s = u.scheme, noAuth = (u.authority === undefined || u.authority === "");
  if (s === "ssh" || s === "http" || s === "https" || s === "be") return true;
  return s === "file" && noAuth && !!u.path &&
         u.query !== undefined && u.query[0] === "/";   // file:<store>?/<proj>
}

//  PATCH-011: YES iff the arg carries a SCHEME — a source URI never rides
//  be.treeAt (it is not a context path; the repo stays the ambient/cwd context).
function isSchemed(arg) {
  if (!arg) return false;
  let u; try { u = new URI(String(arg)); } catch (e) { return false; }
  return u.scheme !== undefined;
}

//  GET-038 probe: a WORKTREE's `.be` anchor is a regular FILE; a store's is a
//  DIR.  `path` may name the tree or its `.be` directly (abs or cwd-relative).
function isWtPath(path) {
  const p = String(path);
  const beFile = (p.slice(-3) === ".be") ? p : join(p, ".be");
  let k; try { k = io.stat(beFile).kind; } catch (e) { return false; }
  return k === "reg";
}

//  PATCH-011: land the closure of `tip` from a LOCAL source store into the
//  wt's own shard (objects ONLY), then re-cover EVERY log — the rolling idx
//  may hide the wt's own history, which the ours/fork resolve is about to
//  read (the TEST-003 quirk).  No-op when the tip is already local.
function landTip(info, srcStorePath, srcProj, tip) {
  const mine = store.open(info.storePath, info.project);
  if (mine.getObject(tip)) return;
  const shard = store.shardDir(info.storePath, info.project);
  const serve = URI.make(undefined, undefined, srcStorePath,
                         srcProj ? "/" + srcProj : undefined);
  const haves = [];
  mine.eachTip(function (t) { haves.push(t.sha); });
  //  JS-116: reindexShard retired — land() covers the new log, and the open
  //  path (store.js diskIndex maintenance) persists any missing tail run.
  ingest.land(wire.buildPushPack(serve, tip, haves), shard);
}

//  PATCH-011: the wt-address cross-store arm — the addressed worktree anchors
//  ANOTHER store (its `.be` redirect), so fetch its cur tip's closure from
//  THAT store first; the absorb afterwards is the ordinary local patch.
function fetchWtTip(info, src, tip, arg) {
  try { landTip(info, src.storePath, src.project, tip); }
  catch (e) { throw "PATCHFETCH: cannot fetch patch source " + arg + " — " + e; }
}

//  PATCH-011: fetch a STORE/WIRE source's objects into the wt's own shard;
//  returns the now-local { tip, branch }.  Throws PATCHFETCH loudly BEFORE
//  any wt mutation on an unreachable/unresolvable source.
function fetchSource(info, arg) {
  const rem = get.parseRemote(String(arg));
  let tip;
  try {
    if (rem.local) {
      //  PATCH-011: a local store source resolves to its REAL store (GET-038
      //  redirect), then ships the closure our shard lacks (GIT-018).
      const src = get.resolveLocalSource(rem);
      const serve = URI.make(undefined, undefined, src.storeRoot,
                             src.proj ? "/" + src.proj : undefined);
      const k = wire.serveReader(serve);
      tip = (rem.pin && k.resolveHexAny(rem.pin)) || k.resolveRef(rem.branch || "");
      if (!tip || !isFullSha(tip))
        throw "no " + (rem.pin ? "#" + rem.pin : (rem.branch || "trunk")) +
              " tip in " + src.storeBe;
      landTip(info, src.storeRoot, src.proj, tip);
    } else {
      //  PATCH-011: the wire leg IS get's (seedRemote): fetch, then land.
      const mine = store.open(info.storePath, info.project);
      const f = wire.fetch(rem.raw, rem.branch || "");
      tip = f.want;
      if (!tip || !isFullSha(tip)) throw "peer gave no tip";
      if (!mine.getObject(tip)) {
        const shard = store.shardDir(info.storePath, info.project);
        ingest.land(f.pack, shard);   // JS-116: reindexShard retired (see above)
      }
    }
  } catch (e) {
    throw "PATCHFETCH: cannot fetch patch source " + arg + " — " + e;
  }
  //  PATCH-011: remote-tracking refs row for an AUTHORITY-carrying source only
  //  (a host-less row would read as a LOCAL tip — store.js resolveRef).
  if (rem.authority)
    ingest.saveRemoteRef(store.shardDir(info.storePath, info.project),
                         rem.raw, tip);
  return { tip: tip, branch: rem.branch || "" };
}

module.exports = { isFetchable: isFetchable, isSchemed: isSchemed,
                   isWtPath: isWtPath, fetchSource: fetchSource,
                   fetchWtTip: fetchWtTip };
