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
const store = require("./store.js");   // GIT-018: JS store reader for the push-pack closure walk

//  --- transport classify -------------------------------------------------
//  Decide the peer spawn from the remote URI, mirroring WIRECLI wcli_spawn:
//    file:///P, keeper://local/P, scheme-less local → exec keeper locally
//    be://host/P, keeper://host/P                   → ssh host keeper …
//    //host/P, //host/P.git, git://, ssh://         → ssh host git-upload-pack
//    http(s)://host/owner/repo.git                  → curl smart-HTTP (GIT-012)
//  Returns { bin, argv } (spawn), or { http, url } for the curl adapter.
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

  //  GIT-012: http(s) rides a spawned curl (jab has no TLS), smart-protocol.
  //  The base URL is the remote verbatim minus any in-band `?<sel>`/`?ref`.
  if (scheme === "http" || scheme === "https") {
    let base = remoteUri;
    const q = base.indexOf("?");
    if (q >= 0) base = base.slice(0, q);
    return { http: true, url: base, ssh: false };
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

//  --- GIT-012: smart-HTTP transport over a spawned curl ------------------
//  jab has no TLS, so https rides curl.  Both requests are stateless; curl's
//  stdout is read NON-BLOCKING via pol.watch, pumped by pol.run(budget) until
//  the watched fd hits EOF (handler returns 0 → loop drains → run returns).
const CURL_BIN = io.getenv("CURL_BIN") || "curl";

//  Spawn `curl argv`, drain its stdout to EOF over pol, reap, return the
//  bytes.  Throws on a non-zero curl exit (missing curl, HTTP error via -f).
function curlRun(argv) {
  let child;
  try { child = io.spawn(CURL_BIN, argv); }
  catch (e) { throw "wire.fetch: cannot spawn '" + CURL_BIN + "' (" + e + ")"; }
  const rfd = child.stdout, pid = child.pid;
  io.close(child.stdin);                         // no request body on this pipe
  const chunks = []; let total = 0, done = false;
  const scratch = new Uint8Array(1 << 16);
  pol.watch(rfd, pol.IN, (fd, rev) => {
    const n = io._read(fd, scratch);
    if (n <= 0) { done = true; io.close(fd); return 0; }
    chunks.push(scratch.slice(0, n)); total += n;
    return pol.IN;
  });
  let guard = 0;
  while (!done && guard++ < 1000000) pol.run(50 * pol.MS);
  let rc = {};
  try { rc = io.reap(pid); } catch (e) {}
  if (!done) { try { pol.unwatch(rfd); io.close(rfd); } catch (e) {} }
  if (rc.code !== 0 || rc.signal != null)
    throw "wire.fetch: curl failed (code=" + rc.code + " signal=" + rc.signal +
          ") — check the URL / curl is installed";
  const out = new Uint8Array(total); let off = 0;
  for (const c of chunks) { out.set(c, off); off += c.length; }
  return out;
}

//  CODE-020: POST `body` to `url` as `ctype` via a spawned curl, staging the
//  body in a TMPDIR .req tmpfile (avoids a request-body pipe deadlock) that is
//  unlinked in finally.  Returns curl's raw response bytes.  Collapses the
//  identical tmpfile+curl dance in fetchHttp and pushHttp.
function curlPost(url, ctype, body) {
  const tmp = (io.getenv("TMPDIR") || "/tmp") + "/wire-" + io.getpid() + "-" +
              (Date.now() & 0xffffff) + ".req";
  const fd = io.open(tmp, "c");
  io.writeAll(fd, body); io.close(fd);
  try {
    return curlRun(["-sSf", "-A", "git/2.0", "-H", "Content-Type: " + ctype,
      "--data-binary", "@" + tmp, url]);
  } finally { try { io.unlink(tmp); } catch (e) {} }
}

//  A pull cursor (like pkt.Reader) over an in-memory buffer — feeds the same
//  parse loop with already-fetched curl bytes (the stateless body).
function memReader(buf) {
  let pos = 0;
  return {
    //  CODE-020: reuse pkt.decodeAt (shared len 0/1/2/short/line dispatch).
    next() {
      const { ev, next } = pkt.decodeAt(buf, pos, buf.length - pos);
      pos = next;
      return ev;
    },
    rest() { return buf.slice(pos); }
  };
}

//  CODE-020: drain a refs advertisement into { refs, headSha }, collapsing the
//  three copies of the HEAD/peeled/remotes/head-or-tag filter.  `skipPreamble`
//  drops the smart-HTTP `# service=…\n`+flush head; `keepTags` = fetch mode
//  (capture HEAD sha, keep ONLY heads+tags) vs receive-pack mode (drop HEAD,
//  keep every non-remote non-peeled ref).  `who` tags the preamble error.
function drainAdvert(reader, opts) {
  const keepTags = !!(opts && opts.keepTags);
  const skipPreamble = !!(opts && opts.skipPreamble);
  const who = (opts && opts.who) || "wire.fetch";
  if (skipPreamble) {
    let skipped = false;
    for (let i = 0; i < 2; i++) {
      const ev = reader.next();
      if (ev.kind === pkt.LINE) {
        const s = utf8.Decode(ev.payload);
        if (s.indexOf("# service=") === 0) { skipped = true; continue; }
      }
      if (ev.kind === pkt.FLUSH && skipped) break;
      throw who + ": malformed smart-HTTP advert preamble";
    }
  }
  const refs = []; let headSha = "";
  for (;;) {
    const ev = reader.next();
    if (ev.kind === pkt.FLUSH || ev.kind === pkt.EOF) break;
    if (ev.kind !== pkt.LINE) continue;
    const a = parseAdvLine(ev.payload);
    if (!a) continue;
    if (a.name === "HEAD") { if (keepTags) headSha = a.sha; continue; }
    if (/\^\{\}$/.test(a.name)) continue;                   // peeled tag
    if (a.name.indexOf("refs/remotes/") === 0) continue;
    if (keepTags && !isHead(a.name) && a.name.indexOf("refs/tags/") !== 0)
      continue;
    refs.push({ sha: a.sha, name: a.name });
  }
  return { refs, headSha };
}

//  Smart-HTTP fetch: GET info/refs (skip the `# service…`+flush preamble),
//  pick the want, POST git-upload-pack, return the pack after NAK.  Reuses
//  parseAdvLine/pickWant verbatim — same negotiation as the spawn path.
function fetchHttp(url, wantRef, haves) {
  const advert = curlRun(["-sSf", "-A", "git/2.0",
                          url + "/info/refs?service=git-upload-pack"]);
  //  CODE-020: shared advert drain (skip HTTP preamble, keep heads+tags).
  const { refs, headSha } = drainAdvert(memReader(advert),
    { keepTags: true, skipPreamble: true });
  const want = pickWant(refs, headSha, wantRef || "");
  if (!want) throw "wire.fetch: peer advertised no usable ref";

  //  POST the want/have/done body from a temp file (avoids a pipe deadlock).
  const reqs = [];
  reqs.push(pkt.frame("want " + want.sha + " ofs-delta\n"));
  reqs.push(pkt.flushPkt());
  if (haves) for (const h of haves) if (isFullSha(h))
    reqs.push(pkt.frame("have " + h + "\n"));
  reqs.push(pkt.frame("done\n"));
  let blen = 0; for (const r of reqs) blen += r.length;
  const body = new Uint8Array(blen); { let o = 0;
    for (const r of reqs) { body.set(r, o); o += r.length; } }
  //  CODE-020: shared TMPDIR-staged curl POST.
  const res = curlPost(url + "/git-upload-pack",
    "application/x-git-upload-pack-request", body);
  const rr = memReader(res);
  for (;;) {
    const ev = rr.next();
    if (ev.kind === pkt.EOF) throw "wire.fetch: peer closed before pack";
    if (ev.kind === pkt.LINE) {
      const s = utf8.Decode(ev.payload).replace(/\n$/, "");
      if (s === "NAK" || s.indexOf("ACK") === 0) break;
      continue;
    }
    if (ev.kind === pkt.FLUSH) continue;
  }
  const pack = rr.rest();
  return { pack, refs, want: want.sha, refname: want.name,
           branch: want.branch };
}

//  --- the fetch ----------------------------------------------------------
function fetch(remoteUri, wantRef, haves) {
  const sp = classify(remoteUri, "upload-pack");
  if (sp.http) return fetchHttp(sp.url, wantRef, haves);   // GIT-012
  const child = io.spawn(sp.bin, sp.argv);
  const wfd = child.stdin, rfd = child.stdout, pid = child.pid;

  let result;
  try {
    //  1. drain the refs advertisement up to the first flush.
    const reader = pkt.Reader(rfd);
    //  CODE-020: shared advert drain (spawn: no preamble, keep heads+tags).
    const { refs, headSha } = drainAdvert(reader, { keepTags: true });

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

//  --- GIT-013: PUSH (receive-pack) ---------------------------------------
//  send-pack write direction: advertise git-receive-pack, send
//  `<old> <new> <ref>` update commands + a packfile of objects the remote
//  lacks, parse report-status.  ssh/local spawn `git-receive-pack`; http
//  rides curl POST (reusing the GIT-012 curlRun/memReader/pol path).  POST
//  stays FF-only; the gate runs in the caller (post.js) BEFORE any write.
//
//  GIT-018: the pack is built in PURE JS (buildPushPack below): a closure
//  walk over the store reader (want-minus-haves) + a git.pack emit — NO
//  keeper spawn, no subprocess.

//  Drain a receive-pack advertisement (pkt.Reader over a spawned fd, or a
//  memReader over curl bytes for http).  Returns { refs:[{sha,name}] }.
//  `skipPreamble` drops the smart-HTTP `# service=…\n`+flush head.
function drainRecvAdvert(rd, skipPreamble) {
  //  CODE-020: receive-pack mode of the shared drain (drop HEAD, keep all).
  const { refs } = drainAdvert(rd,
    { keepTags: false, skipPreamble: skipPreamble, who: "wire.push" });
  return { refs };
}

//  GIT-018: build the push pack in PURE JS — no keeper spawn.  Walk the object
//  closure reachable from `wantSha` but NOT from any `have` over the JS store
//  reader, then emit a self-contained (non-thin) git wire pack via git.pack.
//  `localServe` is `<storeroot>?/<project>` (the keeper serve selector); we
//  split it with URI (never a regex) and open the store reader.  Returns the
//  raw packfile bytes (PACK hdr … 20-byte sha1 trailer).

//  serveReader(localServe): open the JS store reader for a `<store>?/<proj>`
//  selector.  URI splits path=storePath, query=`/<proj>` (empty → auto-detect).
function serveReader(localServe) {
  const u = new URI(localServe);
  const storePath = u.path || "";
  let proj = u.query || "";
  if (proj && proj[0] === "/") proj = proj.slice(1);
  return store.open(storePath, proj);
}

//  markReachable(reader, roots, seen): flood the object closure reachable from
//  each commit in `roots` (commit→tree→subtrees→blobs, + parents) into the
//  `seen` Set.  A gitlink (submodule) commit is recorded but NEVER descended
//  into (its objects live in the sub shard).  Missing objects terminate that
//  branch quietly (a shallow shard or a remote-only `have`), matching dag.js.
function markReachable(reader, roots, seen) {
  const treeStack = [];
  function pushTree(sha) { if (sha && !seen.has(sha)) { seen.add(sha); treeStack.push(sha); } }
  const commits = [];
  const cseen = new Set();
  for (const r of roots) if (isFullSha(r) && !cseen.has(r)) { cseen.add(r); commits.push(r); }
  let h = 0;
  while (h < commits.length) {
    const csha = commits[h++];
    let pc;
    try { pc = reader.parseCommit(csha); } catch (e) { pc = undefined; }
    if (!pc) continue;                          // missing/remote-only commit
    seen.add(csha);
    pushTree(pc.tree);
    for (const p of (pc.parents || [])) if (isFullSha(p) && !cseen.has(p)) {
      cseen.add(p); commits.push(p);
    }
  }
  //  Drain the tree stack: enqueue subtrees, mark blob leaves, skip gitlinks.
  while (treeStack.length) {
    const tsha = treeStack.pop();
    let ents;
    try { ents = reader.readTree(tsha); } catch (e) { ents = undefined; }
    if (!ents) continue;
    for (const e of ents) {
      if (e.mode === 0o40000) { pushTree(e.sha); continue; }          // subtree
      if (e.mode === 0o160000) continue;        // GIT-018: gitlink — never descend
      seen.add(e.sha);                          // blob / exe / symlink leaf
    }
  }
}

//  emitPack(reader, order): write a RAW (non-delta) self-contained git wire
//  pack of the objects in `order` (feed-order shas) via git.pack.over into a
//  JS-owned buffer, append the 20-byte sha1 trailer, and return the bytes.
//  GIT-018: RAW per this pass (correctness first; OFS_DELTA is [PACK-002]).
function emitPack(reader, order) {
  const objs = [];
  let cap = 1024;
  for (const sha of order) {
    const o = reader.getObject(sha);
    if (!o) throw "wire.push: closure object missing from store — " + sha;
    objs.push(o);
    cap += o.bytes.length + 256;                // per-record header+zlib slack
  }
  const ta = new Uint8Array(cap);
  const pk = git.pack.over(ta);
  pk.header();
  for (const o of objs) pk.feed(o.type, o.bytes, -1, null);   // raw record
  pk.finish();
  const wm = Number(pk.buffer.watermark);
  const full = new Uint8Array(wm + 20);
  full.set(ta.subarray(0, wm), 0);
  full.set(sha1(ta.subarray(0, wm)), wm);       // git pack trailer = sha1(body)
  return full;
}

function buildPushPack(localServe, wantSha, haves) {
  if (!isFullSha(wantSha)) throw "wire.push: bad want sha " + wantSha;
  const reader = serveReader(localServe);
  //  Exclude everything reachable from the remote's haves, then walk want.
  const excl = new Set();
  const haveRoots = (haves || []).filter(isFullSha);
  if (haveRoots.length) markReachable(reader, haveRoots, excl);
  const want = new Set();
  markReachable(reader, [wantSha], want);
  //  Feed order: commits first, then trees, then blobs (git-friendly; RAW so
  //  order is not load-bearing).  Classify each want-object by its store type.
  const commits = [], trees = [], blobs = [];
  for (const sha of want) {
    if (excl.has(sha)) continue;                // already on the remote
    const o = reader.getObject(sha);
    if (!o) continue;                           // unreadable → drop (thin base)
    if (o.type === "commit") commits.push(sha);
    else if (o.type === "tree") trees.push(sha);
    else blobs.push(sha);
  }
  const order = commits.concat(trees, blobs);
  const pack = emitPack(reader, order);
  if (!pack || pack.length < 32) throw "wire.push: empty push pack";
  return pack;
}

//  Frame one update command: `<old> <new> <ref>\0<caps>\n` (caps on the
//  first only — caller passes caps for index 0, "" otherwise).
function updateLine(old, neu, ref, caps) {
  let s = old + " " + neu + " " + ref;
  if (caps) s += "\0" + caps;
  return pkt.frame(s + "\n");
}

//  Parse report-status pkt-lines (no side-band; minimal caps): expect
//  `unpack ok` then `ok <ref>` / `ng <ref> <reason>`.  Throws on failure
//  surfacing the remote's reason.  `rd` is a pkt cursor (spawn or mem).
function parseReportStatus(rd) {
  let unpackOk = false; const ng = [];
  for (;;) {
    const ev = rd.next();
    if (ev.kind === pkt.FLUSH || ev.kind === pkt.EOF) break;
    if (ev.kind !== pkt.LINE) continue;
    const s = utf8.Decode(ev.payload).replace(/\n$/, "");
    if (s === "unpack ok") { unpackOk = true; continue; }
    if (s.indexOf("unpack ") === 0)
      throw "wire.push: remote rejected pack — " + s.slice(7);
    if (s.indexOf("ng ") === 0) ng.push(s.slice(3));
  }
  if (!unpackOk) throw "wire.push: remote did not report 'unpack ok'";
  if (ng.length) throw "wire.push: ref update rejected — " + ng.join("; ");
}

//  http push: GET info/refs?service=git-receive-pack (skip preamble) →
//  POST git-receive-pack with the update cmds + flush + pack body, via the
//  GIT-012 curlRun.  `updates` already carry resolved old/new shas.
function pushHttp(url, updates, packBytes) {
  const advert = curlRun(["-sSf", "-A", "git/2.0",
                          url + "/info/refs?service=git-receive-pack"]);
  const { refs } = drainRecvAdvert(memReader(advert), true);
  const body = buildPushBody(updates, refs, packBytes);
  //  CODE-020: shared TMPDIR-staged curl POST.
  const res = curlPost(url + "/git-receive-pack",
    "application/x-git-receive-pack-request", body);
  parseReportStatus(memReader(res));
  return { refs };
}

//  Assemble the receive-pack request body: update cmd lines (caps on the
//  first) + flush + pack.  `refs` is the drained advert; each update's
//  `old` defaults to the remote's advertised tip (zero-sha for a new ref).
function buildPushBody(updates, refs, packBytes) {
  const parts = [];
  let first = true;
  for (const u of updates) {
    const adv = refs.find(r => r.name === u.ref);
    const old = u.old || (adv ? adv.sha : ZERO_SHA);
    parts.push(updateLine(old, u.neu, u.ref,
                          first ? "report-status ofs-delta" : ""));
    first = false;
  }
  parts.push(pkt.flushPkt());
  let blen = 0; for (const p of parts) blen += p.length;
  blen += packBytes.length;
  const body = new Uint8Array(blen); let off = 0;
  for (const p of parts) { body.set(p, off); off += p.length; }
  body.set(packBytes, off);
  return body;
}

const ZERO_SHA = "0000000000000000000000000000000000000000";

//  advertRefs(remoteUri, verb): open the peer's advertisement for `verb`
//  (default "receive-pack") and return { refs:[{sha,name}] } WITHOUT sending
//  anything else.  Lets the caller read the remote's old tip for the FF gate
//  before building a pack.  ssh/local spawn + drain; http GET info/refs.
function advertRefs(remoteUri, verb) {
  const v = verb || "receive-pack";
  const sp = classify(remoteUri, v);
  if (sp.http) {
    const advert = curlRun(["-sSf", "-A", "git/2.0",
                            sp.url + "/info/refs?service=git-" + v]);
    return drainRecvAdvert(memReader(advert), true);
  }
  const child = io.spawn(sp.bin, sp.argv);
  const wfd = child.stdin, rfd = child.stdout, pid = child.pid;
  try {
    const reader = pkt.Reader(rfd);
    return drainRecvAdvert(reader, false);
  } finally {
    //  We only wanted the advert; closing both ends + reaping aborts the peer
    //  cleanly (a flush would also do, but the peer tolerates a hangup here).
    try { io.close(wfd); } catch (e) {}
    try { io.close(rfd); } catch (e) {}
    try { io.reap(pid); } catch (e) {}
  }
}

//  push(remoteUri, updates, packBytes): FF-advance a remote branch.
//    updates  [{ ref:"refs/heads/X", neu:"<40hex>", old?:"<40hex>" }]
//             `old` omitted ⇒ taken from the remote's advert (new ref ⇒
//             zero-sha).  The FF gate is the CALLER's job (post.js): it
//             must verify each `old` is an ancestor of `neu` before calling.
//    packBytes the packfile (build via buildPushPack) of objects the remote
//             lacks.  Returns { refs } (the remote's advertised refs).
function push(remoteUri, updates, packBytes) {
  const sp = classify(remoteUri, "receive-pack");
  if (sp.http) return pushHttp(sp.url, updates, packBytes);
  const child = io.spawn(sp.bin, sp.argv);
  const wfd = child.stdin, rfd = child.stdout, pid = child.pid;
  let result;
  try {
    const reader = pkt.Reader(rfd);
    const { refs } = drainRecvAdvert(reader, false);
    const body = buildPushBody(updates, refs, packBytes);
    io.writeAll(wfd, body);
    io.close(wfd);
    parseReportStatus(reader);
    result = { refs };
  } finally {
    try { io.close(rfd); } catch (e) {}
    try { io.reap(pid); } catch (e) {}
  }
  return result;
}

module.exports = { fetch, push, advertRefs, buildPushPack, classify,
                   parseAdvLine, pickWant, isFullSha };
