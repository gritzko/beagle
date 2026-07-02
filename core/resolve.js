//  core/resolve.js — resolution-at-entry seed (JSQUE-004, JS side of DIS-053).
//  Turn argv into the SEED job rows with `?branch`/ref pinned to a commit sha
//  ONCE at entry, so every downstream row is branch-free and order-independent
//  (`path?<new>#<old>`).  Resolve cur->absolute-branch + ref->sha by calling the
//  existing helpers (wtlog.curTip/baselineTip, store.resolveRef) ONCE at seed —
//  NEVER per-row.  A handler MUST NOT re-resolve a ref live (JSQUE-004).
//
//  seedCtx(repo, wtl, k[, opts]) -> the once-resolved constant context:
//    { curBranch, curSha, baselineSha, anyPd, ignore, _resolveRef }
//  where curBranch is cur's ABSOLUTE branch label, curSha/baselineSha are the
//  pinned tips, anyPd (commit-all bit) and the .gitignore snapshot are also
//  seed-resolved constants.  `_resolveRef` is the pinned ref->sha closure: it
//  is the ONLY resolver and is called only here, at the seed.
//
//  seed(verb, argv, ctx, repo) -> { rows, refs } branch-free SEED rows.  Each
//  path row is `path?<new>#<old>` shaped; a `?branch[#sha]` ref-write form is
//  pinned to a 40-hex sha via the seed resolver and emitted as a `refs` op.
//  The multi-path batch (`put a b c`) fans to one row per arg.  PATCH's
//  (ours, theirs, fork) triple is pinned once via patchscope.resolve.
//
//  libabc+libdog ONLY: pure JS over wtlog/store/patchscope/ignore + the URI
//  binding.  No hand-rolled URI parsing — `new URI(arg)` does the split.

"use strict";

const wtlog       = require("shared/wtlog.js");        // JSQUE-016: lib/ -> shared/
const patchscope  = require("shared/patchscope.js");
const ignore      = require("shared/util/ignore.js");
const shalib      = require("shared/util/sha.js");
const isFullSha   = shalib.isFullSha;

//  A 6..40 hex hashlet (short sha) — the `?br#<hashlet>` / `?<hashlet>` form.
function isHexish(s) {
  return !!s && s.length >= 6 && s.length <= 40 && /^[0-9a-f]+$/.test(s);
}

//  COMMIT-004 (Defect A): the read-only OBJECT/CONTENT projectors re-parse their
//  whole `<scheme>:<uri>` (with its OWN `?`/`#` slots) off ctx.args[0], so their
//  token must ride through as a path — NEVER be consumed as a branch ref-write by
//  classifyArg (columnar ls/status/diff/refs keep classifyArg; they read row.uri).
//  DIFF-012: `diff` joins the re-parse views — its `?from..to`/`?branch` slots are
//  the view's own range/ref, not a branch ref-write, so the token rides through.
const VIEW_REPARSE = { commit:1, size:1, type:1, blob:1, sha1:1,
                       cat:1, tree:1, log:1, diff:1 };
function isReparseView(verb) { return VIEW_REPARSE[verb] === 1; }

//  KEEPResolveHex twin: a full sha passes through iff the object exists; a
//  short hashlet scans the local tips + remotes for a unique-prefix sha.
//  Used ONLY by the seed resolver below (never per-row).
function resolveHex(k, hexish) {
  if (isFullSha(hexish)) return k.getObject(hexish) ? hexish : undefined;
  let hit;
  k.eachTip(function (t) { if (!hit && t.sha.indexOf(hexish) === 0) hit = t.sha; });
  if (!hit) k.eachRemote(function (rt) { if (!hit && rt.sha.indexOf(hexish) === 0) hit = rt.sha; });
  return hit;
}

//  --- seedCtx: pin every ambient coordinate ONCE ------------------------
//  cur->absolute-branch + ref->sha + the commit-all bit + a .gitignore
//  snapshot are seed-resolved constants threaded in `ctx`; a handler reads
//  ctx, never the live wtlog/REFS (JSQUE-004).  `opts.skipIgnore` lets a
//  caller (or a fresh/pre-clone wt) skip the FS snapshot.
function seedCtx(repo, wtl, k, opts) {
  opts = opts || {};
  const cur = wtl.curTip();
  const base = wtl.baselineTip();
  //  cur.branch is the PARSED absolute branch (sha chunks dropped); cur.query
  //  is the raw label native carries.  Both pinned here, once.
  const ctx = {
    curBranch: (cur && cur.branch) || "",
    curQuery:  (cur && cur.query) || "",
    curSha:    (cur && cur.sha) || "",
    baselineSha: (base && base.sha) || "",
    //  anyPd (commit-all bit): true when NO put/delete lies after the last
    //  get/post boundary — a seed-resolved constant (POST's selective gate).
    anyPd: anyPutDelete(wtl),
    //  the .gitignore snapshot, loaded once at the wt root.
    ignore: opts.skipIgnore ? null : ignore.load(repo.wt),
    //  the ONLY ref->sha resolver; pinned to this store reader.  A row may
    //  carry the resulting sha but NEVER this closure.
    _resolveRef: function (refOrBranch) { return k.resolveRef(refOrBranch); },
    _resolveHex: function (hexish) { return resolveHex(k, hexish); },
    //  the wtl + reader the ctx was pinned to — reused by PATCH's one-shot
    //  triple resolve so it never opens a second live resolver (JSQUE-004).
    _wtl: wtl, _reader: k
  };
  return ctx;
}

//  anyPd: a put/delete row lies strictly AFTER the last get/post boundary →
//  the next post is selective (commit-all = !anyPd).  Reuses the drained rows.
function anyPutDelete(wtl) {
  const rows = wtl.rows;
  for (let i = rows.length - 1; i >= 0; i--) {
    const v = rows[i].verb;
    if (v === "put" || v === "delete") return true;
    if (v === "get" || v === "post") return false;
  }
  return false;
}

//  --- per-arg slot classification (URI binding; no hand-rolled parsing) --
//  Returns one of (all branch-free after the seed pin):
//    { kind:"ref", op:"create"|"set", branch, sha? }   ref-write form
//    { kind:"path", path, newSha?, oldSha?, dst? }     a `path?<new>#<old>` row
//  ref forms pin branch->sha via ctx._resolveRef / _resolveHex at the seed.
function classifyArg(arg, ctx) {
  const u = new URI(arg);
  const q = u.query || "", path = u.path || "", frag = u.fragment || "",
        auth = u.authority || "", data = u.href || "";
  const hasQ = q !== "", hasPath = path !== "", hasFrag = frag !== "",
        hasAuth = auth !== "";

  //  Trunk reset: `?#<sha>` — empty query, hex fragment, no path/auth.
  if (!hasQ && !hasPath && !hasAuth && hasFrag && data[0] === "?" && isHexish(frag)) {
    const full = ctx._resolveHex(frag);
    if (!full) throw "RESOLVE: cannot resolve ?#" + frag;
    return { kind: "ref", op: "set", branch: "", sha: full };
  }
  //  `?<40hex>` (no fragment) — set cur's ABSOLUTE branch to this sha.
  if (hasQ && !hasPath && !hasAuth && !hasFrag && isFullSha(q)) {
    const full = ctx._resolveHex(q);
    if (!full) throw "RESOLVE: cannot resolve ?" + q;
    return { kind: "ref", op: "set", branch: ctx.curQuery || "", sha: full };
  }
  //  `?br` / `?br#<sha>`.
  if (hasQ && !hasPath && !hasAuth) {
    if (isHexish(frag)) {
      const full = ctx._resolveHex(frag);
      if (!full) throw "RESOLVE: cannot resolve ?" + q + "#" + frag;
      return { kind: "ref", op: "set", branch: q, sha: full };
    }
    return { kind: "ref", op: "create", branch: q };
  }
  //  Move-form: non-empty path AND fragment whose frag is a DEST path (PUT)
  //  — we keep both raw; the verb decides hash-vs-path.
  if (hasPath && hasFrag) return { kind: "path", path: path, dst: frag };
  //  Plain path / dir / bareword.
  return { kind: "path", path: path || q };
}

//  --- the PATH-row pinner -----------------------------------------------
//  Pin a per-arg path into the canonical branch-free tree-update form
//  `path?<newSha>#<oldSha>` using the seed ctx (baseline = ctx.baselineSha).
//  When the caller supplies a target-tree reader it resolves new/old blob
//  shas; absent one, the row carries the path + the pinned baseline commit so
//  a downstream handler still never re-resolves a branch.  Order-independent.
function pinPath(c, ctx) {
  return { path: c.path, dst: c.dst,
           newSha: c.newSha, oldSha: c.oldSha,
           baseline: ctx.baselineSha };
}

//  --- seed: argv -> branch-free SEED rows --------------------------------
//  `verb` selects the row vocabulary; `argv` is the user arg list (flags
//  already stripped by the caller); `ctx` is seedCtx's pinned constants.
//  Returns { rows, refs }: `rows` the branch-free path/leaf seed rows (one per
//  path arg — the multi-path batch fan-out), `refs` the ref-write ops.  For
//  PATCH the (ours, theirs, fork) triple is pinned once into `triple`.
function seed(verb, argv, ctx, repo) {
  const rows = [], refs = [];
  let triple;

  //  PATCH pins the ours/theirs/fork commit-triple ONCE via the same wtl +
  //  reader the ctx pinned (per-file leaves stay pure; no second live resolve).
  if (verb === "patch") {
    const arg = argv.length ? argv[0] : "";
    triple = patchscope.resolve(arg, ctx._wtl, ctx._reader);
    return { rows: rows, refs: refs, triple: triple };
  }

  //  JAB-003: POST's message rides ctx.args as a STRING — classifyArg would
  //  URI-split a `T: log: msg` message into a malformed path row (silent drop).
  if (verb === "post") return { rows: rows, refs: refs, triple: triple };

  //  JSQUE-009: GET's arg is a whole REMOTE URI (scheme/authority/store path),
  //  not a wt path — classifyArg would mangle it.  Ride it verbatim as the
  //  seed row; the get handler resolves the remote at entry (its own pin).
  //  COMMIT-004 (Defect A): a re-parse projector is the same — its `?`/`#` are the
  //  view's OWN slots, never a branch ref-write, so ride the token through as a
  //  path for the view to re-parse (see isReparseView).
  //  GIT-016: HEAD is GET's non-persisting twin — its arg is likewise a whole
  //  remote URI the handler resolves at entry, so ride it verbatim (not mangled).
  if (verb === "get" || verb === "head" || isReparseView(verb)) {
    for (const arg of argv) rows.push({ path: arg });
    return { rows: rows, refs: refs, triple: triple };
  }

  for (const arg of argv) {
    const c = classifyArg(arg, ctx);
    if (c.kind === "ref") {
      refs.push({ op: c.op, branch: c.branch, sha: c.sha });
    } else {
      rows.push(pinPath(c, ctx));
    }
  }
  return { rows: rows, refs: refs, triple: triple };
}

module.exports = {
  seedCtx: seedCtx,
  seed: seed,
  classifyArg: classifyArg,
  pinPath: pinPath,
  isHexish: isHexish,
  resolveHex: resolveHex
};
