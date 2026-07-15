//  views/tree/tree.js — the `tree:` read-only VIEW (JAB-008).  Resolve a URI to
//  a git tree and emit ONE row per entry in raw git-tree order:
//    `<mode6> <type6> <sha40>\t<name>[/]`
//  with a leading BARE `..` row first iff the URI path descends below the tree
//  root.  Pure JS over the libabc/libdog bindings: shared/store.js (object read),
//  core/resolve_hash.js (URI-016: THE URI->hash resolver — the `?branch`/`?<hex>`/
//  `#<hex>`/empty resolution + the `./path` descent), the URI binding (the
//  structured scheme/path/query/frag split).  NO dog binary, NO /proc.
//  Mirrors keeper/PROJ.c::KEEPProjTree (resolve → descend → drain → emit) +
//  KEEP.c::KEEPResolveTree (the `?branch`/`?<hex>`/`#<hex>`/empty resolution) +
//  WALK.c::KEEPTreeDescend (the `./path` segment walk).
//
//  OUTPUT CONTRACT (JAB-008): the rows are a FIXED-FORMAT byte block, not the
//  core/emit.js date/verb columns — so each line is pushed VERBATIM through the
//  emit sink's `out.raw(text)` (which renders a raw line unchanged in BOTH the
//  plain and the colour render, never columnised).  PLAIN is the bare row text;
//  COLOUR is the row hand-painted to match native `be tree: --color` (a leading
//  `tree:<path>` banner band width-filled to 200, each entry's mode/type/sha
//  prefix in the dim "tree-meta" SGR + the name in the violet "name" SGR, the
//  `..` row name-only in the name SGR).  The view OWNS its byte shape here
//  because the C HUNK binding's generic content-hunk render does NOT reproduce
//  the keeper projector's per-token theme (verified: an empty/`tree:`-uri hunk
//  renders verbatim-with-framing, NOT the gray-rows/violet-name/200-fill block).
//
//  Error edges (NO stdout rows + a THROW → nonzero exit, matching native's
//  PROJFAIL/PROJNONE/KEEPFAIL stderr + nonzero — the exact dog exit code/stderr
//  text is dog-internal and not reproduced; stdout parity is exact):
//    file-as-tree / non-tree leaf / non-tree object   -> throw "TREEFAIL"
//    missing path segment                              -> throw "TREENONE"
//    bad ref / unresolvable sha                        -> throw "TREENONE"

"use strict";

const store  = require("../../shared/store.js");
const ambient = require("../../shared/ambient.js");   // JAB-004: ctx→be bridge
const discover = require("../../core/discover.js");   // URI-016: the nav context
const resolve_hash = require("../../core/resolve_hash.js").resolve_hash;
const recurse = require("../../core/recurse.js");   // DIS-060: gitlink → sub store
const navlib  = require("../../shared/nav.js");   // URI-011: full-URI hunk helper

//  BRO-006: a content-HUNK row carries a hidden `U`-tagged nav URI so a bro
//  pager left-click on the entry NAME opens it — mirroring native `be tree:
//  --tlv` (a tree → `tree:<path>/`, a blob → `blob:<path>`).  Row layout:
//  `<meta><name><navuri>\n`, toks D(meta) F(name) U(navuri) W(\n); the nav URI
//  rides BEFORE the '\n' so the body ends visible.  tag 'U' = 20, hidden in
//  plain/color (the pager's _uriAt reads the bytes).  See views/log/log.js.
function tok(tag, end) { return ((tag & 0x1f) << 27) | (end & 0xffffff); }
function tagCode(letter) { return letter.charCodeAt(0) - 65; }

//  Append ONE entry row's bytes + tok32 spans.  `meta` = the `<mode> <type>
//  <sha>\t` prefix, `name` the entry name (a tree keeps its trailing '/'),
//  `navUri` the hidden click target.  Returns the bytes appended.
function appendRow(textParts, spans, off, meta, name, navUri) {
  const metaB = utf8.Encode(meta);
  const nameB = utf8.Encode(name);
  const uriB  = utf8.Encode(navUri);
  const nlB   = utf8.Encode("\n");
  textParts.push(metaB); textParts.push(nameB); textParts.push(uriB); textParts.push(nlB);
  const eMeta = metaB.length;
  const eName = eMeta + nameB.length;
  const eUri  = eName + uriB.length;
  const eNL   = eUri + nlB.length;
  spans.push([tagCode("D"), off + eMeta]);   // meta (mode/type/sha) — dim
  spans.push([tagCode("F"), off + eName]);   // the visible name (violet)
  spans.push([tagCode("U"), off + eUri]);    // hidden nav URI (click target)
  spans.push([tagCode("S"), off + eNL]);     // the visible '\n'
  return metaB.length + nameB.length + uriB.length + nlB.length;
}

//  mode-class -> the row's `<mode6> <type-padded-to-6>` prefix (proj_tree_mode_
//  type :271/:274).  The type column is 6 wide ("tree "/"blob "/"commit"), then
//  proj :362 adds a single ' ' before the sha — folded into the constant here so
//  the row build is `<this><sha40>\t<name>`.
const MODE_PREFIX = {
  tree:   "040000 tree   ",   // dir       (+ trailing '/' on the name)
  blob:   "100644 blob   ",   // regular file
  exe:    "100755 blob   ",   // executable
  link:   "120000 blob   ",   // symlink
  commit: "160000 commit ",   // gitlink / submodule (type col 6 → 0 pad)
};

//  --- colour SGR (native `be tree: --color`, verified byte-for-byte) --------
//  The meta prefix (mode/type/sha) paints in the dim grey "90"; the entry name
//  (and the bare `..`) in the violet "38;5;56".  The banner band opens with the
//  pale-yellow `38;5;0;48;5;230` and width-FILLS the `tree:<path>` text to 200
//  columns (native KEEPProjTree's banner — the only width-200 fill in tree:).
const SGR = "\x1b[";
const META_OPEN  = SGR + "90m";
const NAME_OPEN  = SGR + "38;5;56m";
const RESET      = SGR + "0m";
const BANNER_OPEN = SGR + "38;5;0;48;5;230m";
const BANNER_WIDTH = 200;

//  Pad a banner's text to BANNER_WIDTH columns with spaces (the native fill).
function bannerLine(text) {
  let t = text;
  while (t.length < BANNER_WIDTH) t += " ";
  return BANNER_OPEN + t + RESET;
}

//  Build ONE entry row's bytes in the active mode.  `name` already carries the
//  trailing '/' for a dir.
//    plain:  <prefix><sha40>\t<name>
//    color:  <META>‹prefix sha40 \t›<NAME>‹name›<RESET>
function entryRow(prefix, sha, name, color) {
  const meta = prefix + sha + "\t";
  if (!color) return meta + name;
  return META_OPEN + meta + NAME_OPEN + name + RESET;
}

//  The bare `..` row: plain `..`; colour name-only in the violet name SGR.
function dotdotRow(color) {
  return color ? (NAME_OPEN + ".." + RESET) : "..";
}

//  URI-016: resolveRootTree is GONE — the ref/sha/cur-tip resolution AND the
//  `./path` descent are [/wiki/URI] §URI->hash steps 5+6, so they go through THE
//  resolver (treeAt).  It also took a TREE sha as the root, which step 5 forbids:
//  `chash` is a COMMIT in every case.

//  URI-016: resolve `tree:<path>[?rev]` to the tree to LIST, via resolve_hash.
//  `tree:` is this view's OWN scheme, so it is shed; the `[path][?ref][#frag]`
//  left over is what the spec resolves.  Returns { k, sha }: the reader for the
//  shard the object lives in, and the tree sha.  Refusals map to this view's
//  contract — unresolvable → TREENONE, a non-tree leaf → TREEFAIL.
function treeAt(repo, path, u, segs) {
  const ctx = discover.navCwd(discover.ctxDir());   // URI-016: derived off be.context
  const bare = URI.make(undefined, undefined, path, u.query, u.fragment) || path;
  let r;
  try { r = resolve_hash(ctx, bare); } catch (e) { throw "TREENONE"; }
  //  The record names the shard the object lives in; `store` IS the `.be` dir,
  //  which store.open takes directly.
  let k = store.open(r.store, r.shard);
  //  DIS-060: a gitlink leaf (bare `tree <sub>`) reads otype "commit" — the PIN
  //  the parent recorded.  That commit lives in the MOUNTED sub's shard, not in
  //  the parent's, so cross into it and list the pin's tree.  `tree <sub>/`
  //  instead reads otype "tree" off the sub's own checkout (URI-016's
  //  trailing-slash convention: named vs entered).
  if (r.otype === "commit") {
    const d = recurse.resolveRepoForPath(repo, segs.join("/"));
    if (!d.prefix) throw "TREEFAIL";            // gitlink not mounted → can't cross
    k = store.open(d.repo.storePath, d.repo.project);
    const st = commitOrTree(k, r.ohash);
    if (!st) throw "TREENONE";                  // pin absent from the sub store
    return { k: k, sha: st };
  }
  if (r.otype !== "tree") throw "TREEFAIL";     // file-as-tree / non-tree leaf
  return { k: k, sha: r.ohash };
}

//  A resolved object id → its TREE sha: a commit deref's to its tree, a tree is
//  used directly.  A blob/other → null (a non-tree object → TREEFAIL caller).
function commitOrTree(k, sha) {
  const obj = k.getObject(sha);
  if (!obj) return null;
  if (obj.type === "tree") return sha;
  if (obj.type === "commit") return k.commitTree(sha) || null;
  return null;                                  // a blob/tag is not a tree
}

//  JAB-004: emit ONE tree URI's entries — self-parse `tree:<path>[?rev]`, read
//  be.repo/out/sink + ambient.format(); `ctx` = direct-handler fallback (no be).
//  A read-only LEAF: no fan-out (the drained tree is one hunk / one row block).
function treeOne(arg, ctx) {
  const _be  = (typeof be !== "undefined") ? be : null;
  const out  = (_be && _be.out)  || (ctx && ctx.out)  || null;
  const sink = (_be && _be.sink) || (ctx && ctx.sink) || null;
  const mode = ambient.format();
  const color = mode === "color";
  //  BRO-006: color/tlv emit the U-target content hunk (pager + --tlv parity);
  //  plain keeps the byte-identical hand-painted `out.raw` rows (the content
  //  hunk's HUNKu8sFeedText render lacks tree's bespoke plain shape).
  const wantU = sink && mode !== "plain";
  const repo = (_be && _be.repo) || (ctx && ctx.repo) || (_be ? _be.treeAt() : null);
  if (!repo) return;

  //  URI-013: ONE structured parse of the whole `tree:<path>[?rev]` — the URI
  //  binding reads `.path`/`.query`/`.fragment` off the scheme'd form (no
  //  strip-then-reparse).  Empty → root/cur-tip.
  const u = uri._parse(String(arg || ""));
  const path  = u.path || "";
  const query = u.query || "";
  const frag  = u.fragment || "";

  //  URI-016: steps 5+6 — the ref/sha/cur-tip resolution AND the `./path`
  //  descent — are THE resolver's, not this view's.  "."/"./"/empty collapse to
  //  the root; below-root ⇒ a leading `..` row.
  const segs = path.split("/").filter(function (s) { return s !== "" && s !== "."; });
  const at = treeAt(repo, path, u, segs);
  const treeK = at.k;

  const belowRoot = segs.length > 0;
  const entries = treeK.readTree(at.sha);
  if (!entries) throw "TREEFAIL";               // leaf sha not a readable tree

  //  3) emit.  COLOUR leads with the banner band; PLAIN has no banner.  The
  //  banner text is `tree:` + the NORMALISED path ("."/"./"-collapsed) + a
  //  `?<rev>` suffix when a sha/ref was given — native promotes `#<hex>` to the
  //  `?<hex>` query form in the banner and keeps the VERBATIM (un-expanded)
  //  value (`?054a0d44`, `?heads/feat`).  No suffix for a pure-path/empty URI.
  const rev = query || frag;                    // frag (#hex) shows as ?<hex>
  //  URI-014: banner is the `word URI` spell (`tree //name/segs?rev`); the ?rev
  //  suffix rides after the authority-scoped addressing.
  const banner = navlib.navLink("tree", segs.join("/"), rev || undefined);
  const pathPfx = segs.length ? segs.join("/") + "/" : "";   // full-path nav prefix

  //  BRO-006 pager/--tlv path: ONE content HUNK, a hidden `U` per entry name.
  if (wantU) {
    const textParts = [], spans = [];
    let off = 0;
    if (belowRoot) {                            // the bare `..` row — no U target
      const dd = utf8.Encode("..\n");
      textParts.push(dd);
      spans.push([tagCode("F"), off + utf8.Encode("..").length]);
      spans.push([tagCode("S"), off + dd.length]);
      off += dd.length;
    }
    for (const e of entries) {
      const kind = store.modeKind(e.mode);
      const prefix = MODE_PREFIX[kind] || MODE_PREFIX.blob;
      const name = kind === "tree" ? (e.name + "/") : e.name;
      const meta = prefix + e.sha + "\t";
      //  URI-014: entry click-target is the `word URI` spell (`tree //name/p` /
      //  `blob //name/p`).
      const nav = kind === "tree"
                ? navlib.navLink("tree", pathPfx + e.name + "/")
                : navlib.navLink("blob", pathPfx + e.name);
      off += appendRow(textParts, spans, off, meta, name, nav);
    }
    const body = new Uint8Array(off);
    let p = 0;
    for (const part of textParts) { body.set(part, p); p += part.length; }
    const toks = new Uint32Array(spans.length);
    for (let i = 0; i < spans.length; i++) toks[i] = tok(spans[i][0], spans[i][1]);
    sink.feed(banner, body, toks, "", 0n);
    return;
  }

  //  Plain/hand-painted path (byte-identical to native): COLOUR leads with the
  //  banner band; PLAIN has no banner.
  if (!out) return;
  if (color) out.raw(bannerLine(banner));
  if (belowRoot) out.raw(dotdotRow(color));     // the bare `..`, never at root
  for (const e of entries) {
    const kind = store.modeKind(e.mode);
    const prefix = MODE_PREFIX[kind] || MODE_PREFIX.blob;
    const name = kind === "tree" ? (e.name + "/") : e.name;   // trailing '/' on dirs
    out.raw(entryRow(prefix, e.sha, name, color));
  }
  //  Read-only leaf: no fan-out.
}

//  JAB-004: PLAIN verb (`.jab="args"`) loops its STRING URI args reading `be`.
//  Zero positional → `[""]` (empty URI = root/cur-tip, the seed's ".").
function tree() {
  const argv = arguments.length ? arguments : [""];
  for (let i = 0; i < argv.length; i++) treeOne(argv[i]);
}
tree.jab = "args";
module.exports = tree;

//  BRO-006: expose the U-target hunk builders for the repro test.
module.exports.tok = tok;
module.exports.appendRow = appendRow;
