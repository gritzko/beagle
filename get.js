//  get.js — `be get` as a pure-JS extension (JS-038/041).  Reproduces native
//  `be get <remote>` output-equivalently: fetch/redirect, checkout the tip
//  tree into the wt (new/del/mod, files+dirs), record the wtlog anchor, and
//  print the HUNK-table summary (a `get ?<branch>#<hashlet>` state banner +
//  one row per changed path).  Pure JS over JABC + bin/lib/* (libdog+abc
//  ONLY; the git-wire convo is reimplemented in JS — keeper/ is unlinkable).
//
//  Transports:
//    file:///P?/<proj>, keeper://local/P  → store-backed worktree (redirect:
//        the wt's `.be` row-0 points at the shared source store; no pack copy)
//    be://host/P, //host/P, git://…        → clone: wire-fetch a pack, ingest
//        into the wt's OWN store, then checkout (JS-040)  [in progress]
//
//  Usage:  be get <remote>      (clone/update the cwd worktree)

"use strict";

const self = process.argv[1];
const here = self.slice(0, self.lastIndexOf("/"));
const be       = require(here + "/lib/be.js");
const wtlog    = require(here + "/lib/wtlog.js");
const store    = require(here + "/lib/store.js");
const wire     = require(here + "/lib/wire.js");
const checkout = require(here + "/lib/checkout.js");
const dag      = require(here + "/lib/dag.js");
const ingest   = require(here + "/lib/ingest.js");
const pathlib  = require(here + "/lib/path.js");
const ulog     = require(here + "/lib/ulog.js");
const render   = require(here + "/lib/render.js");
const join = pathlib.join, dirname = pathlib.dirname;
const dateCol = render.dateCol, verbCol = render.verbCol,
      writeStdout = render.writeStdout;

//  --- wtlog (the wt's `.be` ULOG) writer ---------------------------------
//  The crash-safe, monotonic ULOG writer now lives in bin/lib/ulog.js
//  (JS-048, folded in from wtwrite.js): writeWtlog/appendWtlog are
//  ulog.write/append.  Row shapes mirror native `be get`'s secondary-wt
//  anchor:
//    get  file:<srcBe>/?/<proj>        (row 0: redirect to the shared store)
//    get  ?<branch>#<40hex>            (row 1: the checked-out tip)
const writeWtlog = ulog.write;
const appendWtlog = ulog.append;

//  --- remote URI → { local, srcRoot, srcBe, proj, branch } ---------------
function parseRemote(uri) {
  const u = new URI(uri);
  const scheme = u.scheme || "";
  const host = u.host || u.authority || "";
  const path = u.path || "";
  const query = u.query || "";
  //  proj/branch from a `?/<proj>[/<branch>]` selector.
  let proj = "", branch = "";
  if (query && query[0] === "/") {
    const segs = query.slice(1).split("/");
    proj = segs[0] || "";
    branch = segs.slice(1).join("/");
  } else if (query) {
    branch = query;                       // `?<branch>` (no project)
  }
  const localish = scheme === "file" || scheme === "" ||
                   (scheme === "keeper" && (host === "" || host === "local" ||
                                            host === "localhost"));
  //  srcBe = the source `.be` dir (path ends in `.be`); srcRoot its parent.
  let srcBe = path, srcRoot = path;
  if (localish) {
    srcBe = path.replace(/\/+$/, "");
    srcRoot = srcBe.replace(/\/\.be$/, "");
    if (srcRoot === srcBe) srcRoot = dirname(srcBe);
  }
  return { local: localish, scheme, host, srcRoot, srcBe, proj, branch,
           raw: uri };
}

//  --- local redirect clone/update ----------------------------------------
function getLocal(rem, wt) {
  //  Open the shared source store read-side.
  const k = store.open(rem.srcRoot, rem.proj);
  const tip = k.resolveRef(rem.branch || "");
  if (!tip || !wire.isFullSha(tip))
    throw "be get: cannot resolve " + (rem.branch || "trunk") +
          " in " + rem.srcBe;

  const bePath = join(wt, ".be");
  const fresh = !exists(bePath);
  const oldTip = fresh ? "" : oldTipOf(bePath);

  const res = checkout.apply(k, tip, wt);

  //  Anchor rows.  Fresh clone: redirect row-0 + tip row.  Update: just the
  //  new tip row appended.
  const redirect = "file:" + rem.srcBe + "/?/" + rem.proj;
  const tipRow = { verb: "get", uri: "?" + (rem.branch || "") + "#" + tip };
  if (fresh) {
    writeWtlog(bePath, [{ verb: "get", uri: redirect }, tipRow]);
  } else {
    appendWtlog(bePath, [tipRow]);
  }
  return { tip, oldTip, fresh, branch: rem.branch || "", rows: res.rows, k };
}

function exists(p) { try { io.stat(p); return true; } catch (e) { return false; } }

//  --- remote clone/update (be:/ssh:/git:) --------------------------------
//  v1 always full-fetches (no haves) → the peer ships an OFS-only verbatim
//  pack (no REF_DELTA), landed as a new `NNNNN.keeper` pack-log.  Thin/incr
//  fetch (haves → REF_DELTA) is the JS-035/036/037-backed follow-up.
function getRemote(rem, wt) {
  const beDir = join(wt, ".be");
  const fresh = !exists(beDir);
  const proj = rem.proj || "repo";

  const f = wire.fetch(rem.raw, rem.branch || "");
  const tip = f.want;
  if (!tip || !wire.isFullSha(tip)) throw "be get: peer gave no tip";
  //  Branch label = the URI's explicit branch, else the peer's default (its
  //  advertised HEAD: keeper trunk → "", git → "master").
  const branch = rem.branch || f.branch || "";

  const shard = join(beDir, proj);
  const wtl = join(beDir, "wtlog");
  let oldTip = "";

  if (fresh) {
    ingest.clone(f.pack, beDir, proj, tip, rem.raw);
    const anchor = "file:" + beDir + "/" + proj + "/";
    writeWtlog(wtl, [{ verb: "get", uri: anchor },
                     { verb: "get", uri: "?" + branch + "#" + tip }]);
  } else {
    oldTip = oldTipOf(wtl);
    ingest.add(f.pack, shard, rem.raw, tip);
    appendWtlog(wtl, [{ verb: "get", uri: "?" + branch + "#" + tip }]);
  }

  const k = store.open(wt, proj);
  const res = checkout.apply(k, tip, wt);
  return { tip, oldTip, fresh, branch, rows: res.rows, k };
}

//  Newest tip sha recorded in an existing wtlog (the last `#<40hex>` row).
function oldTipOf(bePath) {
  let tip = "";
  ulog.each(bePath, function (log) {
    const u = new URI(log.uri);
    let f = u.fragment || "";
    if (f[0] === "?") f = f.slice(1);
    if (wire.isFullSha(f)) tip = f;
  });
  return tip;
}

//  --- summary render -----------------------------------------------------
//  Native `be get` layout (sniff PATCH banner + tree-diff rows):
//    <ts> get  ?<branch>#<hashlet8>                 state banner
//    <ats> post ?<hashlet8>#<subject>               one per pulled commit
//                                                   (UPDATE only; author ts)
//    <ts> new|upd|del <path>                        file rows: [new,upd] in
//                                                   lex order, THEN [del] lex
function byPath(a, b) { return a.path < b.path ? -1 : a.path > b.path ? 1 : 0; }

function emit(out) {
  const ts = ron.now();
  let body = "";
  body += dateCol(ts) + " " + verbCol("get") + " ?" + (out.branch || "") +
          "#" + out.tip.slice(0, 8) + "\n";

  //  Pulled-commit rows (UPDATE only): commits reachable from the new tip
  //  but not the old — KEEPEmitCommitsSince, newest-first by author ts.
  if (!out.fresh && out.oldTip && out.oldTip !== out.tip) {
    const ahead = dag.aheadBehind(out.k, out.tip, out.oldTip).ahead;
    for (const c of ahead)
      body += dateCol(c.ts) + " " + verbCol("post") + " ?" + c.hashlet +
              (c.subject ? "#" + c.subject : "") + "\n";
  }

  //  File rows: new+upd first (lex), then del (lex).
  const nu = out.rows.filter(function (r) { return r.verb !== "del"; }).sort(byPath);
  const dl = out.rows.filter(function (r) { return r.verb === "del"; }).sort(byPath);
  for (const r of nu.concat(dl))
    body += dateCol(ts) + " " + verbCol(r.verb) + " " + r.path + "\n";
  writeStdout(body);
}

function main() {
  const argv = process.argv.slice(2);
  const args = argv.filter(function (a) { return a[0] !== "-"; });
  const remote = args[0];
  if (!remote) throw "be get: a remote URI is required (v1)";

  const rem = parseRemote(remote);
  const wt = io.cwd();

  let out;
  if (rem.local) {
    out = getLocal(rem, wt);
  } else {
    out = getRemote(rem, wt);
  }
  emit(out);
}

main();
