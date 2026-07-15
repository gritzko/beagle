//  views/size/size.js — the `size:` read-only VIEW (JAB-010).  Resolve a URI to
//  a single git object and emit its INFLATED byte size as ONE row: `<n>\n`.
//  Pure JS over the libabc/libdog bindings: shared/store.js (object/ref read +
//  the descendPath path descender + getObject's inflated bytes), core/resolve.js
//  (the hex/short-sha classifier + resolveHex), shared/wtlog.js (the empty-`?`
//  cur-tip default), the URI binding (the structured scheme/path/query/frag
//  split).  NO dog binary, NO /proc.
//
//  The C side is a STUB (keeper/PROJ.c::KEEPProjDispatch routes `size`→keeper
//  but has no handler: it prints "projector 'size:' not implemented" + fails
//  PROJNONE, exit 206, NO stdout).  So there is no C stdout to byte-diff; this
//  view implements the INTENDED contract (wiki/Projector.mkd, the ticket Design):
//  the size is the INFLATED object length — the SAME quantity C's KEEPGetSize
//  (KEEP.c:769, `u8bDataLen(buf3)` after the KEEPGet inflate) would report —
//  which store.getObject(sha).bytes is, so the size is simply `bytes.length`.
//
//  RESOLUTION (mirrors the sibling keeper object projectors KEEPProjDispatch):
//    `#<hex>` / `?<hex>` (full or 6..40 short sha; ?<hex> promoted to fragment,
//        KEEPProjDispatch :664) → that object DIRECTLY (NO commit→tree deref —
//        size: reports the size of the named object, unlike tree: which derefs).
//    `?<ref>` (non-hex query)  → REFSResolve to a commit; a trailing `./path`
//        descends the commit's tree to the leaf object; bare → the commit object.
//    `./path` (no query/frag)  → descend the cur-tip's tree to the leaf object.
//    empty (`size:` / `size:?`) → the cur-tip commit object.
//
//  OUTPUT CONTRACT (JAB-010): a single fixed-format row pushed VERBATIM through
//  the emit sink's `out.raw(text)` (the SAME row-hunk path the landed tree: view
//  uses) — it renders the row unchanged in BOTH the plain and the colour render
//  (a bare decimal has no token to theme, so plain == colour, byte-for-byte).
//  The handler NEVER writes fd 1 / io.log / bypasses core/emit.js.
//
//  Error edges (NO stdout + a THROW → nonzero exit, matching the C
//  PROJNONE/KEEPFAIL stderr + nonzero — the exact dog exit code/stderr text is
//  dog-internal and not reproduced; stdout parity is exact, the landed tree:
//  view's edge convention):
//    bad ref / unresolvable sha / missing path segment   -> throw "SIZENONE"
//    can't descend through a non-tree mid-path           -> throw "SIZENONE"

"use strict";

const store   = require("../../shared/store.js");
const wtlog   = require("../../shared/wtlog.js");
const resolve = require("../../core/resolve.js");
const isFullSha = require("../../shared/util/sha.js").isFullSha;
const ambient = require("../../shared/ambient.js");   // JAB-004: ctx→be bridge
//  JAB-003: emit a TRUE hunk via the shared sink (retiring ctx.out) through the
//  shared columnar→HUNK adapter.
const hunkrows = require("../../shared/hunkrows.js");
const navlib  = require("../../shared/nav.js");        // URI-014: word-URI banner

//  JS-082: a FULL 40-hex sha passes through verbatim iff the object exists; a
//  short prefix goes through resolveHexAny ({1,39} prefix scanner rejects 40).
function resolveHexOrFull(k, hex) {
  if (isFullSha(hex)) return k.getObject(hex) ? hex : null;
  return k.resolveHexAny(hex) || null;
}

//  Resolve the URI to a single OBJECT sha (the object whose size we report).
//  Returns the 40-hex sha, or null when unresolvable (→ SIZENONE at the caller).
function resolveObject(k, wtl, path, query, frag) {
  const segs = path.split("/").filter(function (s) { return s !== "" && s !== "."; });

  //  A hex (full sha or 6..40 short prefix) in EITHER slot is a direct object
  //  id: native promotes `?<hex>`→fragment (KEEPProjDispatch :664).  Fragment
  //  wins when both are set.  size: reports the NAMED object — no commit deref.
  const hex = resolve.isHexish(frag) ? frag
            : resolve.isHexish(query) ? query
            : null;
  if (hex) {
    //  Store-wide prefix resolve (ANY object, not just tips) — C resolves a
    //  `#<hex>` against any stored object (proj_resolve_object_sha), so a short
    //  blob/tree sha must resolve too (resolve.resolveHex scans tips only).
    //  JS-082: a full 40-hex sha short-circuits resolveHexAny's {1,39} gate.
    const full = resolveHexOrFull(k, hex);
    if (!full) return null;
    //  A `#<hex>./path` descends FROM the named object's tree (commit→tree, or
    //  a tree used directly); bare → the object itself.
    if (segs.length) return descendFrom(k, full, segs);
    return full;
  }

  //  A non-hex query is a branch/ref name: resolve → commit.  With a `./path`,
  //  descend the commit's tree to the leaf; bare → the commit object itself.
  if (query) {
    const sha = k.resolveRef(query);
    if (!sha) return null;
    if (segs.length) return descendFrom(k, sha, segs);
    return sha;
  }

  //  No query/frag, but a `./path`: descend the cur-tip's tree to the leaf.
  const cur = wtl.curTip();
  if (!cur || !cur.sha) return null;
  if (segs.length) return descendFrom(k, cur.sha, segs);

  //  Empty `?` (+ empty `#`): the cur tip COMMIT object (HOME.cur_sha).
  return cur.sha;
}

//  Descend `segs` from a root object (a commit deref's to its tree first, a tree
//  is used directly) and return the LEAF entry's sha.  null when the root is not
//  a tree-bearing object, or a segment is absent / mid-path is a non-tree.
function descendFrom(k, rootSha, segs) {
  const rootTree = treeOf(k, rootSha);
  if (!rootTree) return null;
  const leaf = k.descendPath(rootTree, segs);   // undefined: missing/non-tree mid
  if (!leaf) return null;
  return leaf.sha;
}

//  An object sha → the TREE sha to descend from: a commit derefs to its tree, a
//  tree is used directly.  A blob/tag/missing → null (nothing to descend into).
function treeOf(k, sha) {
  const obj = k.getObject(sha);
  if (!obj) return null;
  if (obj.type === "tree") return sha;
  if (obj.type === "commit") return k.commitTree(sha) || null;
  return null;
}

//  JAB-004: size ONE arg — self-parse size:<uri>, read be.repo/be.sink.
//  ambient.format() is read for parity with the sibling views though a bare
//  decimal has no token to theme.
function sizeOne(arg) {
  const _be = (typeof be !== "undefined") ? be : null;
  ambient.format();                              // JAB-004: mode read (plain==color here)
  const repo = (_be && _be.repo) || ((_be && _be.treeAt) ? _be.treeAt() : null);
  const sink = (_be && _be.sink) || null;
  if (!repo || !sink) return;

  //  URI-013: ONE structured parse of the whole `size:<uri>` — the URI binding
  //  reads `.path`/`.query`/`.fragment` off the scheme'd form (no strip-then-
  //  reparse; the analogue of cat's/blob's collapse).
  const u = uri._parse(String(arg || ""));
  const path  = u.path || "";
  const query = u.query || "";
  const frag  = u.fragment || "";

  const k   = store.open(repo.storePath, repo.project);
  const wtl = wtlog.open(repo);

  //  1) resolve the URI to a single object sha.
  const sha = resolveObject(k, wtl, path, query, frag);
  if (!sha) throw "SIZENONE";                    // bad ref / unresolvable / missing

  //  2) read the object; its INFLATED byte length is the size (== C's KEEPGetSize
  //     u8bDataLen of the inflated buf).  A missing object → SIZENONE.
  const obj = k.getObject(sha);
  if (!obj) throw "SIZENONE";

  //  3) emit ONE row: the decimal size, as a TRUE hunk banner.
  //     URI-014: the word-URI spell `size <uri>` — verb OUT of the scheme,
  //     navLink injects the nav authority (derived off be.context) + roots the path (strip a leading '/').
  const out = hunkrows(sink, navlib.navLink("size", path.replace(/^\//, ""), query, frag));
  out.raw(String(obj.bytes.length));
  out.done();
  //  Read-only leaf: no fan-out (per-arg; the dispatcher fans out over args).
}

//  JAB-004: PLAIN verb (`.jab="args"`) loops its args reading `be`.
function size() {
  for (let i = 0; i < arguments.length; i++) sizeOne(arguments[i]);
}
size.jab = "args";
module.exports = size;
