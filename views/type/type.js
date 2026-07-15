//  views/type/type.js — the `type:` read-only VIEW (JAB-011).  Resolve a URI to
//  a git object and emit ONE word — its object type (commit|tree|blob|tag) —
//  followed by a newline.  Pure JS over the libabc/libdog bindings:
//  shared/store.js (object/ref read — getObject hands the type word straight
//  from the wh128 index key, no parse), core/resolve.js (the hex/short-sha
//  classifier + resolver), shared/wtlog.js (the empty-`?` cur-tip default).
//  NO dog binary, NO /proc, NO C.
//
//  In C this projector is a STUB (keeper/PROJ.c::KEEPProjDispatch has no
//  `type` arm — it prints "projector 'type:' not implemented" to stderr and
//  fails PROJNONE), so there is NO native stdout to diff against.  We emit the
//  INTENDED one-word output (wiki/Projector.mkd, GIT.c::GITTypeName), mirroring
//  the LANDED sibling tree: for the URI resolution + framing.
//
//  URI slots honored (mirror keeper's sibling object projectors):
//    #<hex>   fragment = an object id (full sha, or a 6..40 short prefix)
//    ?<hex>   query    = a hex prefix native PROMOTES to the fragment
//    ?<ref>   query    = a branch/ref name (REFSResolve)
//    empty            = the cur tip (HOME.cur_sha), like bare `tree:`
//  UNLIKE tree:, type: does NOT deref a commit to its tree — it reports the
//  RESOLVED object's OWN type, so a commit id yields "commit", a tree id
//  "tree", a tag id "tag", a blob id "blob".  `./path` / `//remote` are not
//  part of a bare type lookup (ticket Context) and are not honored here.
//
//  OUTPUT CONTRACT: the type word is a fixed one-token row, not the emit
//  date/verb columns, so it is pushed VERBATIM through the emit sink's
//  `out.raw(text)` (rendered unchanged in BOTH plain and colour — the C THEME
//  paints the type word with no SGR, so a bare raw row is byte-correct in
//  either mode).  The view NEVER writes fd 1 / io.log / core/emit bypass.
//
//  Error edge (NO stdout + a THROW → nonzero exit, matching native's
//  PROJNONE/KEEPFAIL stderr + nonzero; the exact dog exit code/stderr text is
//  dog-internal and not reproduced — stdout parity is exact):
//    bad ref / unresolvable sha / missing object  -> throw "TYPENONE"

"use strict";

const store  = require("../../shared/store.js");
const wtlog  = require("../../shared/wtlog.js");
const resolve = require("../../core/resolve.js");
const isFullSha = require("../../shared/util/sha.js").isFullSha;
//  JAB-003: TRUE-hunk output via the shared columnar→hunk adapter (be.sink),
//  retiring the ctx.out columnar path for this view.
const hunkrows = require("../../shared/hunkrows.js");
const navlib   = require("../../shared/nav.js");        // URI-014: word-URI banner

//  Resolve the URI to a full object sha (KEEPResolveTree's sibling, minus the
//  commit→tree deref).  A hex (full sha or 6..40 short prefix) in EITHER slot
//  is a sha: native promotes `?<hex>`→fragment (KEEPProjDispatch) and resolves
//  it directly; fragment wins when both are set.  A non-hex query is a
//  branch/ref name (REFSResolve).  An empty query+frag defaults to the cur tip
//  (HOME.cur_sha).  Returns the full sha, or null when unresolvable.
function resolveObjectSha(k, wtl, query, frag) {
  //  A hex (full sha or 6..40 short prefix) in EITHER slot is a sha and
  //  resolves against ALL objects (store.resolveHexAny), not only branch tips
  //  (core/resolve.js::resolveHex) — the C honors any unique 6..40-hex prefix,
  //  so a mid-history / cur-tip object id must resolve too.  Fragment wins.
  const hex = resolve.isHexish(frag) ? frag
            : resolve.isHexish(query) ? query
            : null;
  //  JS-082: a FULL 40-hex sha passes through verbatim iff present; resolveHexAny's
  //  {1,39} prefix scanner rejects 40, so short-circuit it for the full sha.
  if (hex) return isFullSha(hex) ? (k.getObject(hex) ? hex : null)
                                 : (k.resolveHexAny(hex) || null);
  //  A non-empty, non-hex FRAGMENT (`#d6`, `#xyz` — a too-short / non-hex id)
  //  is an explicit-but-unresolvable object request: fail, do NOT fall through
  //  to the cur tip (matches the C `tree:#d6` → KEEPFAIL, not the HEAD tree).
  if (frag) return null;
  //  A non-hex QUERY is a branch/ref name (REFSResolve).
  if (query) return k.resolveRef(query) || null;
  //  Empty `?` (+ empty `#`): the cur tip (HOME.cur_sha).
  const cur = wtl.curTip();
  return (cur && cur.sha) || null;
}

//  JAB-004: type ONE arg — self-parse type:<uri>, read be.repo/be.sink, feed the
//  same sink; `ctx` = direct-handler fallback (no global be).
function typeOne(arg, ctx) {
  const _be = (typeof be !== "undefined") ? be : null;
  const repo = (_be && _be.repo) || (ctx && ctx.repo) || (_be && _be.treeAt && _be.treeAt()) || null;
  const sink = (_be && _be.sink) || (ctx && ctx.sink) || null;
  if (!repo) return;

  //  URI-013: ONE structured parse of the whole `type:<uri>` — the URI binding
  //  reads `.query`/`.fragment` off the scheme'd form (no strip-then-reparse).
  const u = uri._parse(String(arg || ""));
  const query = u.query || "";
  const frag  = u.fragment || "";

  const k   = store.open(repo.storePath, repo.project);
  const wtl = wtlog.open(repo);

  //  1) resolve the URI to an object sha (ref/sha/cur-tip).
  const sha = resolveObjectSha(k, wtl, query, frag);
  if (!sha) throw "TYPENONE";                   // bad ref / unresolvable sha

  //  2) read the object — getObject hands the type word straight from the
  //  wh128 index key (no body parse).  A missing object → TYPENONE.
  const obj = k.getObject(sha);
  if (!obj || !obj.type) throw "TYPENONE";

  //  3) emit the type word as ONE raw row into a TRUE hunk banner.
  //  URI-014: the word-URI spell `type <uri>` — verb OUT of the scheme;
  //  navLink injects the nav authority (derived off be.context) + roots the path (strip a leading '/').
  if (sink) {
    const out = hunkrows(sink, navlib.navLink("type", (u.path || "").replace(/^\//, ""), u.query, u.fragment));
    out.raw(obj.type);
    out.done();
  }
  //  Read-only leaf: no fan-out.
}

//  JAB-004: PLAIN verb (`.jab="args"`) loops its STRING args reading `be`.
function type() {
  for (let i = 0; i < arguments.length; i++) typeOne(arguments[i]);
}
type.jab = "args";
module.exports = type;
