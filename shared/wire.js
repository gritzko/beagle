//  wire.js — git upload-pack CLIENT (fetch), pure JS (JS-039).  Spawns the
//  peer (local `keeper upload-pack`, or `ssh <host> keeper upload-pack`, or
//  vanilla `git-upload-pack`), drains the v0/v1 refs advertisement, sends
//  want/have/done, and returns the raw packfile bytes.  Mirrors
//  `keeper/WIRECLI.c` (unlinkable from JABC) over `io.spawn`+`io._read/write`
//  + `pkt.js`.  No side-band requested → keeper/git stream the pack RAW after
//  `NAK\n` (keeper advertises no side-band-64k), so no demux needed.  No dog.
//
//  fetch(remoteUri, wantRef?, haves?) -> { pack, refs, want, refname, branch }
//    pack    Uint8Array   the packfile (PACK hdr + records + sha trailer)
//    refs    [{sha, name}] advertised refs (heads + tags, HEAD/peeled dropped)
//    want    40-hex        the want sha actually requested
//    refname "refs/heads/<x>" | "refs/tags/<x>"  the matched ref (or "")
//    branch  be-side branch label ("" = trunk/main)

"use strict";

//  Sibling lib: this is a required module, so `require` is bound to this
//  file's dir (require.cpp) — resolve siblings relative, not via argv[1].
const pkt = require("./pkt.js");
const isFullSha = require("./util/sha.js").isFullSha;   // JSQUE-016: -> shared/util/
const shq = require("../view/render.js").shQuote;       // JSQUE-016: render -> view/

//  --- transport classify -------------------------------------------------
//  Decide the peer spawn from the remote URI, mirroring WIRECLI wcli_spawn:
//    file:///P, keeper://local/P, scheme-less local → exec keeper locally
//    be://host/P, keeper://host/P                   → ssh host keeper …
//    //host/P, //host/P.git, git://, ssh://         → ssh host git-upload-pack
//  Returns { bin, argv } for io.spawn.  `verb` = "upload-pack"/"receive-pack".
function classify(remoteUri, verb) {
  const u = new URI(remoteUri);
  const scheme = u.scheme || "";
  const host = u.host || u.authority || "";
  let path = u.path || "";
  const query = u.query || "";
  const keeperBin = io.getenv("KEEPER_BIN") || "keeper";

  //  Serve-path arg the peer sees: path + `?<sel>` when the query is an
  //  absolute `/<project>` selector (WIREServePath).  A bare `?ref` is the
  //  in-band want, never appended here.
  function servePath(p) {
    return (query && query[0] === "/") ? (p + "?" + query) : p;
  }

  //  DIS-058: a host-less `be:`/`keeper:` (an ABSOLUTE local store path, NO
  //  authority) is a LOCAL keeper exec, NOT ssh — matching native `be get
  //  be:/abs/store?/proj` and the spec ("be: runs beagle's keeper wire").  A
  //  `be://host` (authority PRESENT, incl. `localhost`) still routes to ssh
  //  below (parity with native + the be-js-get-be ssh case).  `keeper` keeps
  //  its historical local/localhost local-exec alias.
  const noAuth = (u.authority == null || u.authority === "");
  const localish = scheme === "file" || scheme === "" ||
                   (scheme === "keeper" && (host === "" || host === "local" ||
                                            host === "localhost")) ||
                   (scheme === "be" && noAuth);
  if (localish) {
    return { bin: keeperBin, argv: [keeperBin, verb, servePath(path)],
             ssh: false };
  }

  //  Remote: ssh.  Strip a leading '/' (HOME-relative convention).
  if (path[0] === "/") path = path.slice(1);
  const sshBin = io.getenv("SSH_BIN") || "ssh";
  const isKeeper = scheme === "be" || scheme === "keeper";
  if (isKeeper) {
    //  `ssh host keeper <verb> <path>?<sel>` — keeper protocol.  Honour
    //  $DOG_REMOTE_PATH so a non-login ssh shell still finds `keeper`.
    const sp = servePath(path);
    const remPath = io.getenv("DOG_REMOTE_PATH") || "";
    const cmd = remPath
      ? ("PATH=" + remPath + ":$PATH exec keeper " + verb + " " + shq(sp))
      : ("keeper " + verb + " " + shq(sp));
    return { bin: sshBin, argv: [sshBin, host, cmd], ssh: true };
  }
  //  Vanilla git over ssh.
  return { bin: sshBin, argv: [sshBin, host, "git-" + verb + " " + shq(path)],
           ssh: true };
}

//  --- advert parse -------------------------------------------------------
//  One advertised line: `<40-hex> SP <refname>[\0caps]`.  Returns
//  {sha, name, caps} or null (flush/HEAD/peeled-tag/non-head-tag dropped by
//  the caller).
function parseAdvLine(payload) {
  //  payload is a Uint8Array; the line is ASCII up to an optional \0 (caps)
  //  then a trailing \n.  Decode the pre-NUL part as the `<sha> <name>` head.
  let nul = payload.indexOf(0);
  const headEnd = nul < 0 ? payload.length : nul;
  let head = utf8.Decode(payload.subarray(0, headEnd));
  let caps = nul < 0 ? "" : utf8.Decode(payload.subarray(nul + 1));
  head = head.replace(/\n$/, ""); caps = caps.replace(/\n$/, "");
  const sp = head.indexOf(" ");
  if (sp < 0) return null;
  const sha = head.slice(0, sp);
  const name = head.slice(sp + 1);
  if (!isFullSha(sha)) return null;
  return { sha, name, caps };
}

//  --- ref matching -------------------------------------------------------
//  Pick the want sha from the advert.  wantRef forms: "" (default branch),
//  "heads/x"/"tags/x"/"refs/...", a 40-hex (want-by-hash).  Mirrors
//  wcli_match_advert's preference: exact ref → HEAD sha → main → first head.
function pickWant(refs, headSha, wantRef) {
  if (isFullSha(wantRef)) return { sha: wantRef, name: "", branch: "" };
  if (wantRef) {
    const full = wantRef.indexOf("refs/") === 0 ? wantRef : ("refs/" + wantRef);
    for (const r of refs) if (r.name === full) return refReturn(r);
  }
  if (headSha) for (const r of refs) if (r.sha === headSha && isHead(r.name))
    return refReturn(r);
  for (const r of refs) if (r.name === "refs/heads/main") return refReturn(r);
  for (const r of refs) if (r.name === "refs/heads/master") return refReturn(r);
  for (const r of refs) if (isHead(r.name)) return refReturn(r);
  if (refs.length) return refReturn(refs[0]);
  return null;
}
function isHead(name) { return name.indexOf("refs/heads/") === 0; }
function refReturn(r) {
  const branch = isHead(r.name) ? r.name.slice("refs/heads/".length) : "";
  return { sha: r.sha, name: r.name, branch: branch === "main" ? "" : branch };
}

//  --- read the raw pack to EOF ------------------------------------------
function readToEof(fd, head) {
  const chunks = [];
  let total = 0;
  if (head && head.length) { chunks.push(head); total += head.length; }
  const scratch = new Uint8Array(1 << 16);
  for (;;) {
    const n = io._read(fd, scratch);
    if (n <= 0) break;
    chunks.push(scratch.slice(0, n)); total += n;
  }
  const out = new Uint8Array(total);
  let off = 0;
  for (const c of chunks) { out.set(c, off); off += c.length; }
  return out;
}

//  --- the fetch ----------------------------------------------------------
function fetch(remoteUri, wantRef, haves) {
  const sp = classify(remoteUri, "upload-pack");
  const child = io.spawn(sp.bin, sp.argv);
  const wfd = child.stdin, rfd = child.stdout, pid = child.pid;

  let result;
  try {
    //  1. drain the refs advertisement up to the first flush.
    const reader = pkt.Reader(rfd);
    const refs = [];
    let headSha = "";
    for (;;) {
      const ev = reader.next();
      if (ev.kind === pkt.FLUSH) break;
      if (ev.kind === pkt.EOF) break;
      if (ev.kind !== pkt.LINE) continue;
      const a = parseAdvLine(ev.payload);
      if (!a) continue;
      if (a.name === "HEAD") { headSha = a.sha; continue; }
      if (/\^\{\}$/.test(a.name)) continue;                 // peeled tag
      if (a.name.indexOf("refs/remotes/") === 0) continue;
      if (!isHead(a.name) && a.name.indexOf("refs/tags/") !== 0) continue;
      refs.push({ sha: a.sha, name: a.name });
    }

    const want = pickWant(refs, headSha, wantRef || "");
    if (!want) throw "wire.fetch: peer advertised no usable ref";

    //  2. send want (caps on the first line), flush, haves, done.
    const reqs = [];
    reqs.push(pkt.frame("want " + want.sha + " ofs-delta\n"));
    reqs.push(pkt.flushPkt());
    if (haves) for (const h of haves) if (isFullSha(h))
      reqs.push(pkt.frame("have " + h + "\n"));
    reqs.push(pkt.frame("done\n"));
    for (const r of reqs) io.writeAll(wfd, r);
    io.close(wfd);

    //  3. read NAK pkt-line, then the raw pack to EOF.
    for (;;) {
      const ev = reader.next();
      if (ev.kind === pkt.EOF) throw "wire.fetch: peer closed before pack";
      if (ev.kind === pkt.LINE) {
        const s = utf8.Decode(ev.payload).replace(/\n$/, "");
        if (s === "NAK" || s.indexOf("ACK") === 0) break;
        //  side-band band-2/3 text or unexpected line — ignore and keep
        //  scanning for NAK/ACK (we did NOT request side-band).
        continue;
      }
      if (ev.kind === pkt.FLUSH) continue;
    }
    const pack = readToEof(rfd, reader.rest());
    result = { pack, refs, want: want.sha, refname: want.name,
               branch: want.branch };
  } finally {
    try { io.close(rfd); } catch (e) {}
    try { io.reap(pid); } catch (e) {}
  }
  return result;
}

module.exports = { fetch, classify, parseAdvLine, pickWant, isFullSha };
