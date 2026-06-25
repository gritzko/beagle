//  views/sha1/sha1.js — the `sha1:` read-only VIEW (JAB-006).  Resolve a URI to
//  a resource and emit its 40-hex SHA-1 + ONE '\n' (41 bytes), a shell-friendly
//  one-liner.  Pure JS over the libabc/libdog bindings: shared/store.js (object/
//  ref read + the descendPath path descender + the resolveHexAny any-object prefix
//  resolver), shared/util/sha.js (isFullSha), the URI binding (the structured
//  scheme/path/query/frag split).  NO dog binary, NO /proc.  Mirrors
//  keeper/PROJ.c::KEEPProjSha1 (the 3-shape dispatch) + KEEPResolveTree (the
//  `?branch`/`?<hex>`/`#<hex>`/cur-tip resolution) + WALK.c::KEEPTreeDescend
//  (the `./path` segment walk) + the KEEPProjDispatch `?<hex>`→fragment promotion.
//
//  THE THREE SHAPES (KEEPProjSha1 :577-605):
//    1) path-bearing `<path>?<ref>`  -> KEEPResolveTree -> proj_descend; the
//       tree-entry sha at that path VERBATIM (blob OR subtree — NOT re-hashed).
//       `.`/`./` (no real segment) collapse to the ROOT TREE's own sha.
//    2) ref/hex `?<ref>` | `?#<hex>` | `#<hex>` -> proj_resolve_object_sha:
//       a `?<ref>` resolves to the COMMIT sha; a FULL 40-hex frag passes through
//       VERBATIM (existence UNCHECKED, like C sha1FromHex); a SHORT hex frag
//       resolves to the unique full-object sha via store.resolveHexAny (KEEPGet twin).
//    3) bare `sha1:` -> the keeper TRUNK ref via store.resolveRef("?") (the
//       REFSResolve("?") twin) = trunk commit sha.  NOT wtlog.curTip() — native
//       reads the keeper `refs` ULOG, not the worktree log, for the trunk pin.
//
//  THE `?<hex>` PROMOTION (KEEPProjDispatch :664-668): a `?<query>` that is a
//  hex prefix of HASH_MIN_HEX(=4)..40 chars is MOVED into the fragment slot (a
//  hash always lives in `?` semantically but routes through the fragment/hashlet
//  index); below 4 chars it stays a ref name.  A `#<frag>` given explicitly is
//  used DIRECTLY with NO min-length gate.  So `?93` (2 chars) fails as a ref but
//  `#93` resolves, and `?1822`/`#1822` (4 chars) both resolve.
//
//  EMPTY-RESULT CONTRACT (PROJNONE :77,602): a missing object / unresolvable ref
//  / absent path segment -> write NOTHING (no trailing '\n'), NOT an empty line.
//  The handler simply returns without an out.raw call.
//
//  OUTPUT (Design :90-93): the 41-byte line is pushed VERBATIM through the emit
//  sink's `out.raw(text)` — which renders the line UNCHANGED in BOTH the plain
//  AND the colour render (a raw line that is not a `status:` header is emitted
//  byte-for-byte; the loop edge owns the fd-1 flush).  A bare sha has no syntax
//  to paint, so plain == colour here (41 bytes each).  The handler NEVER calls
//  io.log / io.write / bypasses core/emit.js.
//
//  C-SPEC DIVERGENCES (deliberate, per the JAB-006 "match the SPEC, note it"
//  ruling — see the report):
//    a) `.`/`./`  : the SPEC + proj_descend's own comment say ROOT TREE sha; the
//       C BINARY emits the COMMIT sha because its URI parser collapses `.` to an
//       EMPTY path BEFORE KEEPProjSha1's path check, so it never enters the
//       path-bearing branch.  This view emits the ROOT TREE sha (the spec).
//    b) `--color`: the C BINARY emits ZERO bytes under --color (its raw write()
//       is swallowed by the colour HUNK pipe).  This view emits the SAME 41 bytes
//       in every mode (the content delivered as a hunk, per the contract).

"use strict";

const be     = require("../../core/discover.js");
const store  = require("../../shared/store.js");
const shalib = require("../../shared/util/sha.js");
const isFullSha = shalib.isFullSha;

const HASH_MIN_HEX = 4;   // keeper/KEEP.h: the `?<hex>`→fragment promotion floor

//  proj_is_hex_prefix (PROJ.c:628): hex-only, HASH_MIN_HEX..40 chars.  Gates the
//  `?<query>`→fragment promotion (a shorter/non-hex query stays a ref name).
function isHexPrefix(s) {
  return !!s && s.length >= HASH_MIN_HEX && s.length <= 40 && /^[0-9a-f]+$/i.test(s);
}

//  A resolved object id -> its COMMIT sha as-is, or a tree/blob as-is.  For the
//  path-bearing root resolution KEEPResolveTree derefs a commit to its TREE; the
//  caller does that explicitly (commitTree) — this just confirms the object is
//  present.  Returns the sha when the object exists, undefined otherwise.
function objIfPresent(k, sha) {
  return k.getObject(sha) ? sha : undefined;
}

//  proj_resolve_object_sha (PROJ.c:41): the ref/hex shape.  A FULL 40-hex frag
//  passes through VERBATIM (existence UNCHECKED — C sha1FromHex never reads the
//  object).  A SHORT hex frag resolves to the unique full-object sha (resolveHexAny,
//  the KEEPGet twin).  An empty frag + a `?<ref>` resolves to the COMMIT sha via
//  resolveRef.  Returns the 40-hex target, or undefined (PROJNONE).
function resolveObjectSha(k, query, frag) {
  if (frag) {
    if (frag.length >= 40) return isFullSha(frag) ? frag : undefined;  // verbatim
    return k.resolveHexAny(frag);                                       // short scan
  }
  if (!query) return undefined;                       // KEEPFAIL (no query, no frag)
  //  `?<ref>` -> REFSResolve to the commit sha.  (A hex query that should be a
  //  hash was already promoted to `frag` by the caller; what's left is a ref.)
  return k.resolveRef(query);
}

//  KEEPResolveTree (the path-bearing root): resolve the URI's ref/hex/cur-tip to
//  a TREE sha (commit derefs to its tree).  Returns the root tree sha, or
//  undefined (PROJNONE at the caller).
function resolveRootTree(k, query, frag) {
  let sha;
  if (frag && isFullSha(frag)) sha = objIfPresent(k, frag);
  else if (frag) sha = k.resolveHexAny(frag);
  else if (query) sha = k.resolveRef(query) || k.resolveHexAny(query);
  else sha = k.resolveRef("?");                        // cur-tip = trunk ref
  if (!sha) return undefined;
  return commitOrTree(k, sha);
}

//  A resolved object -> its TREE sha: a commit derefs to its tree, a tree is
//  used directly.  A blob/other -> undefined.
function commitOrTree(k, sha) {
  const obj = k.getObject(sha);
  if (!obj) return undefined;
  if (obj.type === "tree") return sha;
  if (obj.type === "commit") return k.commitTree(sha) || undefined;
  return undefined;
}

module.exports = function handle(row, ctx) {
  const out  = ctx && ctx.out;
  const repo = (ctx && ctx.repo) || be.find();
  if (!repo) return;

  //  The whole projector URI rides ctx.args (a fragment-only URI lowers to a "."
  //  placeholder in the queue row), exactly like cat:/tree:.  Strip the `sha1:`
  //  scheme so the URI binding sees the bare body.
  const rawArgs = (ctx && ctx.args && ctx.args.length) ? ctx.args : [row.uri];
  let first = String(rawArgs[0] || "");
  if (first.indexOf("sha1:") === 0) first = first.slice("sha1:".length);
  const u = new URI("sha1:" + first);   // re-scheme so URI splits path/query/frag
  const path  = u.path || "";
  let   query = u.query || "";
  let   frag  = u.fragment || "";

  //  KEEPProjDispatch :664-668: a `?<query>` that is a HASH_MIN_HEX..40 hex
  //  prefix is promoted into the fragment slot (routed through the hashlet index);
  //  the query is then cleared.  A shorter/non-hex query stays a ref name.
  if (isHexPrefix(query)) { frag = query; query = ""; }

  const k = store.open(repo.storePath, repo.project);

  //  The real path segments ("."/""/"./"-tail collapsed away).  When NONE remain
  //  the URI is NOT path-bearing in C (its parser strips `.` to empty); but the
  //  SPEC + proj_descend's comment say `.`/`./` -> ROOT TREE sha — so we DO
  //  enter the path-bearing branch for a `.`/`./`-only path and let descendPath
  //  collapse to the root tree.  A truly empty path (bare `sha1:`, `?ref`, `#hex`)
  //  takes the object/ref shape below.
  const rawSegs = path.split("/");
  const realSegs = rawSegs.filter(function (s) { return s !== "" && s !== "."; });
  const pathBearing = realSegs.length > 0 ||
                      //  a `.`/`./`-only path (no real segment) is path-bearing
                      //  per the SPEC (root tree), unlike the C binary.
                      (path !== "" && path !== "/" );

  let target;
  if (pathBearing) {
    //  Shape 1: KEEPResolveTree -> proj_descend.  Root tree from the ref/hex/cur,
    //  then descend the real segments; the leaf entry sha VERBATIM.  An absent
    //  ref / segment, or a mid-path through a non-tree, -> PROJNONE (nothing).
    const rootTree = resolveRootTree(k, query, frag);
    if (!rootTree) return;
    const leaf = k.descendPath(rootTree, realSegs);
    if (!leaf) return;                  // missing segment / can't descend
    target = leaf.sha;                  // blob/subtree/root-tree sha verbatim
  } else if (query || frag) {
    //  Shape 2: ref/hex object resolution.
    target = resolveObjectSha(k, query, frag);
  } else {
    //  Shape 3: bare `sha1:` -> the keeper TRUNK ref (REFSResolve("?")).
    target = k.resolveRef("?");
  }

  //  PROJNONE: unresolvable -> write NOTHING (no '\n').
  if (!target || !isFullSha(target)) return;

  //  Emit the 41-byte line through the emit sink's raw lane (verbatim in plain
  //  AND colour; the loop edge owns the fd-1 flush).  out.raw appends the '\n'.
  if (out) out.raw(target);
};
