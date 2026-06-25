//  views/blob/blob.js — the `blob:` read-only VIEW (JAB-007).  The by-OBJECT-SHA
//  twin of the landed `cat:` view: where cat: reads a wt FILE by path, blob:
//  reads a tracked BLOB by the sha a URI resolves to, then emits its content as a
//  HUNK exactly the way cat: does.  Pure JS over the libabc/libdog bindings:
//  shared/store.js (object read + `descendPath` path descender + the canonical
//  `resolveHexAny` sha/prefix→full-sha resolver), shared/wtlog.js (the empty-`?`
//  cur-tip default), the URI binding (the structured scheme/path/query/frag
//  split).  NO dog binary, NO /proc.
//
//  RULING (gritzko): blob: produces a HUNK like the other views (cat:/tree:/log:),
//  NOT a raw byte dump.  So this view is modelled CLOSELY on views/cat/cat.js:
//  it builds a HUNK (body + tok32) and feeds it through the SAME caller-owned
//  in-memory HUNK sink (`ctx.sink`); the loop EDGE renders that sink in the
//  active mode (plain/color/tlv) via view/bro.js renderHunkLog.  The handler
//  NEVER calls io.log/io.write/raw-stdout, NEVER uses a raw-bytes channel and
//  NEVER bypasses core/emit.js.  The hunk carries a banner naming the verb
//  (`blob <sha>#L<n>`, the analogue of cat's `cat <path>#L<n>`), so blob: PLAIN
//  looks like cat: PLAIN (banner + body + trailing newline) on the SAME bytes —
//  it does NOT byte-match the C blob:'s banner-less dump, and that is INTENDED.
//
//  Forms (KEEPGetByURI / the URI slots):
//    `?<hex>` / `?#<hex>` / `#<hex>`  bare object by a 1..40 hex sha-prefix;
//                                     the object MUST be a blob.
//    `<path>` / `<path>?<ref>`        the blob at `path` in the tree of `?ref`
//                                     (a branch, a sha-prefix, or empty → cur tip).
//    `#L42` (a non-hex fragment)      a bro line-anchor — IGNORED for resolution;
//                                     the FULL blob is always emitted.
//    `//host…` (a host-bearing URI)   remote NYI → fail (as C KEEPFAIL).
//
//  Error edges (NO hunk fed + a THROW → nonzero exit, matching native's
//  KEEPFAIL/KEEPNONE; the exact dog exit code/text is not reproduced):
//    missing path segment / bad ref / unresolvable sha   -> throw "BLOBNONE"
//    dir-as-blob / non-blob object / ambiguous prefix     -> throw "BLOBFAIL"
//    host-bearing URI (remote NYI)                        -> throw "BLOBFAIL"

"use strict";

const be    = require("../../core/discover.js");
const store = require("../../shared/store.js");
const wtlog = require("../../shared/wtlog.js");
const bro   = require("../../view/bro.js");
const resolve = require("../../core/resolve.js");

const EMPTY32 = new Uint32Array(0);
const CAP = 1 << 20;   // 1 MiB/hunk cap; a bigger blob splits with a #L<n> rebanner

//  A 1..40 lowercase-hex string (a sha or sha-prefix).  Looser than
//  resolve.isHexish (which floors at 6) because the bare-object form takes an
//  ARBITRARY-length prefix (C WHIFFHexHashlet60 zero-pads any width).
function isHexPrefix(s) { return !!s && s.length <= 40 && /^[0-9a-f]+$/.test(s); }

//  Resolve the URI's root TREE sha for a path-bearing form (KEEPResolveTree):
//  `?ref` is a branch name, a sha-prefix (commit→tree, or a tree sha used
//  directly), or empty (→ the cur tip, HOME.cur_sha).  Returns the tree sha, or
//  null when unresolvable (→ BLOBNONE at the caller).  Mirrors tree.js's
//  resolveRootTree; a sha-prefix resolves via store.resolveHexAny (the canonical
//  prefix resolver — scans the OBJECT index, so a non-tip short commit sha works).
function resolveRootTree(k, wtl, query, frag) {
  const hex = isHexPrefix(frag) ? frag : isHexPrefix(query) ? query : null;
  if (hex) {
    const sha = k.resolveHexAny(hex);
    if (!sha) return null;                      // none, or ambiguous prefix (null)
    const obj = k.getObject(sha);
    return obj ? commitOrTree(k, { sha: sha, type: obj.type }) : null;
  }
  if (query) {
    let sha = k.resolveRef(query);
    if (!sha) { try { sha = resolve.resolveHex(k, query); } catch (e) {} }
    if (!sha) return null;
    const obj = k.getObject(sha);
    return obj ? commitOrTree(k, { sha: sha, type: obj.type }) : null;
  }
  const cur = wtl.curTip();
  if (!cur || !cur.sha) return null;
  const obj = k.getObject(cur.sha);
  return obj ? commitOrTree(k, { sha: cur.sha, type: obj.type }) : null;
}

//  A located object {sha,type} → its TREE sha: a commit deref's to its tree, a
//  tree is used directly.  A blob/tag → null (no tree to descend → BLOBNONE).
function commitOrTree(k, o) {
  if (o.type === "tree") return o.sha;
  if (o.type === "commit") return k.commitTree(o.sha) || null;
  return null;
}

module.exports = function handle(row, ctx) {
  const mode = (ctx && ctx.mode) || "plain";
  const repo = (ctx && ctx.repo) || be.find();
  if (!repo) return;

  //  The whole projector URI rides ctx.args (a fragment-only URI lowers to a "."
  //  placeholder in the queue row), exactly like cat:/tree:.  Strip the `blob:`
  //  scheme so the URI binding sees the bare body.
  const rawArgs = (ctx && ctx.args && ctx.args.length) ? ctx.args : [row.uri];
  let first = String(rawArgs[0] || "");
  if (first.indexOf("blob:") === 0) first = first.slice("blob:".length);
  const u = new URI(first);
  const path  = u.path || "";
  const query = u.query || "";
  const frag  = u.fragment || "";
  const auth  = u.authority || "";

  //  A host-bearing URI (`//remote…`) is a remote read — NYI, as native KEEPFAIL.
  if (auth) throw "BLOBFAIL";

  const k   = store.open(repo.storePath, repo.project);
  const wtl = wtlog.open(repo);

  //  Resolve the URI to a BLOB sha (the by-object-sha resolution — the only part
  //  that differs from cat:'s by-path read).  `sha` names the blob; `bannerKey`
  //  is the banner stem (the full sha for a bare-object form, else `<path>` —
  //  the analogue of cat:'s `<path>` banner; cat: appends `#L<n>` per hunk).
  let sha, bannerKey;
  if (!path) {
    //  Bare object by a sha-prefix: `?<hex>`, `?#<hex>` or `#<hex>`.  A non-hex
    //  fragment (`#L42` bro line-anchor) is NOT an object id — fall through to
    //  the empty-path error (no full blob to name).  The object MUST be a blob.
    const hex = isHexPrefix(query) ? query : isHexPrefix(frag) ? frag : null;
    if (!hex) throw "BLOBNONE";                 // empty/`#label`-only → nothing
    sha = k.resolveHexAny(hex);
    if (sha === null) throw "BLOBFAIL";         // ambiguous prefix
    if (!sha) throw "BLOBNONE";                 // no object with that prefix
    bannerKey = sha;                            // banner the FULL object sha
  } else {
    //  Path-bearing: resolve the root tree (`?ref`/sha/cur-tip), descend the
    //  `./path` segments, require a BLOB (non-dir) leaf.  A non-hex fragment
    //  (`#L42`) is ignored — resolveRootTree only reads query/frag for a sha.
    const rootTree = resolveRootTree(k, wtl, query, frag);
    if (!rootTree) throw "BLOBNONE";            // bad ref / unresolvable sha
    const segs = path.split("/").filter(function (s) { return s !== "" && s !== "."; });
    const leaf = k.descendPath(rootTree, segs);
    if (!leaf) throw "BLOBNONE";                // missing segment / can't descend
    //  The leaf must be a file-like blob (100644/100755/120000), NOT a dir
    //  (040000) or a gitlink (160000 — a submodule commit, not a readable blob).
    if (leaf.kind === "tree" || leaf.kind === "commit") throw "BLOBFAIL";
    sha = leaf.sha;
    bannerKey = path;                           // banner the path (like cat:)
  }

  //  Fetch the blob bytes by sha (store.getObject — inflate + delta chase).
  const obj = k.getObject(sha);
  if (!obj) throw "BLOBNONE";                   // sha unreadable
  if (obj.type !== "blob") throw "BLOBFAIL";    // not a blob object
  const bytes = obj.bytes;

  //  An EMPTY blob (0 bytes) emits NOTHING — no banner — exactly like cat:'s
  //  empty-file case (and the C `KEEPProjBlob` 0-byte case).
  if (!bytes || bytes.length === 0) return;

  //  Feed the blob into the caller-owned in-memory HUNK sink (ctx.sink) — NO fd 1
  //  here; the loop edge (cli) renders sink.log to fd 1 in the mode (plain/color/
  //  tlv) via bro.renderHunkLog.  This is the SAME sink path cat:/grep/spot feed,
  //  and the SAME chunking/CAP cat: uses (1 MiB/hunk, backed up to a line bound so
  //  a line never splits; a #L<n> rebanner per chunk).  The banner verb is "blob"
  //  so a hunk reads `blob <bannerKey>#L<n>` — the cat: `cat <path>#L<n>` twin.
  const sink = ctx && ctx.sink;
  if (!sink) return;
  const ext = bro.pathExt(bannerKey);           // "js"/"" — drives tok.parse (path form)
  let off = 0, line = 1;
  while (off < bytes.length) {
    //  1 MiB hunk, backed up to the last line boundary so a line never splits.
    let end = off + CAP < bytes.length ? off + CAP : bytes.length;
    if (end < bytes.length) {
      let nl = end; while (nl > off && bytes[nl - 1] !== 10) nl--;
      if (nl > off) end = nl;
    }
    const body = bytes.slice(off, end);
    let toks = EMPTY32;
    if (mode !== "plain" && ext) { try { toks = tok.parse(body, ext); } catch (e) { toks = EMPTY32; } }
    sink.feed(bannerKey + "#L" + line, body, toks, "blob", 0n);   // banner: `blob <key>#L<n>`
    for (let i = off; i < end; i++) if (bytes[i] === 10) line++;
    off = end;
  }
  //  Read-only leaf: no fan-out.
};
