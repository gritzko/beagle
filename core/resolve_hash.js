//  resolve_hash.js — THE URI->hash resolver ([/wiki/URI] §URI->hash resolution).
//  URI-016: every case of URI->hash resolution — commit, tree or blob — goes
//  through resolve_hash(context_uri, uri); no verb re-derives its own half.
//  The six numbered steps below ARE the spec's six steps, in its order.
//
//    resolve_hash(context_uri, uri) -> {
//        store, mpath,                     // step 1: the project root
//        shard, wtree, spath, rpath,       // steps 2-4: the fs path, fixed
//        chash,                            // step 5: the commit
//        otype, ohash,                     // step 6: the object at rpath
//    }
//
//  The context URI comes from `$PWD` on a CLI run (discover.navCwd), from the
//  TUI shell on a pager run.  Failures throw an ok64 code — never a partial
//  record:
//    PROJNONE   step 1: no `.be` below $HOME above $PWD — there is no repo
//    NAVESCAPE  steps 2-3: the path climbs out of its tree (via resolveInTree)
//    WTNONE     step 4: `//name` anchors no worktree under $SRC_ROOT/work/
//    URISCHEME  a transport URI (file:/be:/ssh:/https:) — not a worktree path
//    HASHNONE   step 5: the hashlet/sha names no object in store/shard
//    NOTACOMMIT step 5: the object resolved, but it is a blob/tree/tag
//    REFNONE    step 5.3/5.4: the query resolves as neither branch nor hashlet
//    BASENONE   step 5.5: the tree has no get/post record to fall back on
//    PATHNONE   step 6: rpath does not exist in the tree at chash
"use strict";

const pathlib  = require("../shared/util/path.js");
const discover = require("./discover.js");
const store    = require("../shared/store.js");
const ulog     = require("../shared/ulog.js");
const wtlog    = require("../shared/wtlog.js");
const isFullSha = require("../shared/util/sha.js").isFullSha;

const BE = ".be";

//  A hashlet is 6..40 hex ([/meta/abc] §Hashes): any prefix in that range may
//  address an object; 40 is the full sha.
function isHashlet(s) { return /^[0-9a-f]{6,40}$/.test(s || ""); }

//  Accept a URI object, a URI string, or nothing — the context arrives as
//  either (discover.navCwd yields a string, the pager holds an object).
function asURI(x) {
  if (x === undefined || x === null || x === "") return {};
  if (typeof x === "object") return x;
  try { return uri._parse(String(x)) || {}; } catch (e) { return {}; }
}

function slash(p) { return p && p[p.length - 1] !== "/" ? p + "/" : p; }

//  A worktree anchor's own store dir: storePath is the store's PARENT (its
//  `.be` holds the shards) — unless it already names the `.be` dir itself.
function storeOf(anchor) {
  const sp = anchor.storePath;
  return slash(pathlib.basename(sp) === BE ? sp : pathlib.join(sp, BE));
}

//  A dir anchors the project iff it carries a `.be` that resolves to a store —
//  the spec's own test: "it either has its own store in `.be/` or its `.be` file
//  references the repo in the first line".  An EMPTY `.be` file references
//  nothing and anchors NOTHING (the tests' hermetic firewall is exactly that).
function anchors(dir) {
  const be = pathlib.join(dir, BE);
  let kind; try { kind = io.stat(be).kind; } catch (e) { return false; }
  if (kind === "dir") return true;                    // its own store in `.be/`
  if (kind !== "reg") return false;
  let row0;                                           // the `.be` file's first row
  try { ulog.each(be, function (l) { if (row0 === undefined) row0 = l.uri; }); }
  catch (e) { return false; }
  return !!row0;
}

//  URI-016: THE `.be` climb — the ONE walk up the chain, and there is no other.
//  Both of the spec's anchor questions ask THIS chain, with the SAME anchors()
//  test; they differ in ONE word: step 1 keeps the TOPMOST anchor (the project
//  root), step 4 takes the NEAREST (the tree a path sits in).  `topmost` is that
//  word.  The limit is the spec's own: an anchor is "still lower than $BE_ROOT",
//  so the break PRECEDES the probe — $BE_ROOT/.be is the STORE, and a store is
//  neither a project root nor a worktree.  Returns the anchor dir, or null.
function climb(from, topmost) {
  const home = discover.beRoot();
  let dir = from, hit = null;
  for (;;) {
    if (home && dir === home) break;                  // "still lower than $BE_ROOT"
    if (anchors(dir)) { if (!topmost) return dir; hit = dir; }   // topmost: keep going
    const up = pathlib.dirname(dir);
    if (!up || up === dir || up === "." || up === "/") break;
    dir = up;
  }
  return hit;
}

//  step 1: the project root = the TOPMOST dir carrying a `.be`, still lower than
//  $BE_ROOT (default $HOME).  A worktree in no project is its OWN root (a plain
//  clone: its `.be` is the topmost).  NO `.be` anywhere below $HOME = no repo at
//  all, and nothing to resolve: PROJNONE.  The root's `.be` gives BOTH `store`
//  (its own `.be/`, or the repo its first row references) and `mpath`.
//  ARGLESS and cwd-based: the project root is a property of the RUN, not of a
//  caller's dir, and it can not change mid-run.  DETECTED, never declared: no env
//  var can name it, because an env var that disagreed with the climb would just
//  be a second, lying answer.  $BE_ROOT only bounds the climb.  Cached per
//  process.  EVERY project-root question resolves through this.
function projectRoot() {
  if (typeof be !== "undefined" && be.projectRootDir !== undefined)
    return be.projectRootDir;
  const root = climb(io.cwd(), true);
  if (typeof be !== "undefined") be.projectRootDir = root;
  return root;
}

//  The project root's fixed layout, each the ONE place its segment is spelled:
//    workRoot() = <root>/work   worktrees ([/wiki/URI] step 2: `//WT/wtrel` is
//                               `$SRC_ROOT/work/WT/wtrel`)
//    todoRoot() = <root>/todo   tickets
//    metaRoot() = <root>/meta   the manuals
//  Never join(<the nearest worktree>, …): that synthesised a dir off whichever
//  tree the caller happened to stand in, which need not exist and is not the
//  project root.  ARGLESS, like projectRoot(): the layout is a property of the
//  RUN, so there is no start dir to pass and no per-caller answer to give.
function under(seg) {
  return function () {
    const r = projectRoot();
    return r ? pathlib.join(r, seg) : null;
  };
}
const workRoot = under("work"), todoRoot = under("todo"), metaRoot = under("meta");

//  URI-016: topOf(dir) — the OUTERMOST worktree `dir` belongs to: the WORKTREE
//  `<workRoot>/NAME` ([/wiki/URI] step 2, `//WT` IS `$SRC_ROOT/work/WT`), else the
//  project root (the main tree; a submodule of it tops out THERE).  This REPLACES
//  topWt's climb-past-submodules loop: the outermost tree is not something to
//  search for by re-probing every ancestor — it is FIXED by the project layout,
//  so this is pure path arithmetic on projectRoot()/workRoot() with no `.be` walk
//  at all.  The boundary is workRoot(), THE one spelling of the `work`
//  segment — never a second `basename(dirname(wt)) === "work"` reading of it.
//  null when `dir` lies outside the project (a scratch dir / store edge).
function topOf(dir) {
  const work = workRoot();
  if (work && dir.indexOf(work + "/") === 0) {         // inside work/: the WORKTREE
    const rest = dir.slice(work.length + 1);           // NAME[/sub…]
    const i = rest.indexOf("/");
    return pathlib.join(work, i < 0 ? rest : rest.slice(0, i));
  }
  const root = projectRoot();                          // else the main tree itself
  if (root && (dir === root || dir.indexOf(root + "/") === 0)) return root;
  return null;
}

//  URI-016: treeAt(path[, from]) — [/wiki/URI] step 4, "at this point the
//  filesystem path is fixed, we may derive `shard`, `wtree`, `spath`, `rpath`".
//  THE step-4 routine: the NEAREST anchor at/above the path, returned as the
//  spec's RECORD rather than an ad-hoc handle.  It shares climb()/anchors() with
//  step 1's projectRoot() — one chain, one anchor test, one limit.
//  `from` (default `path`) is where the CLIMB starts, `path` is what `rpath` is
//  measured from.  They differ at exactly ONE spot: frame()'s bare-`sub`
//  re-anchor, which wants the mount as its PARENT names it.
//  `topDir` (default topOf) is the TREE TOP `spath` counts from — frame passes the
//  top its CONTEXT declares, so a context and a cwd probe measure the same way.
//  The record's leading fields are the spec's; `wt`/`bePath`/`storePath`/
//  `project`/`root` are the ANCHOR ITSELF, which store.open/wtlog.open read and
//  which the verb/view call sites reach ambiently as `be.treeAt`.
function treeAt(path, from, topDir) {
  const p = path || io.cwd();
  const wt = climb(from || p, false);
  if (!wt) throw "WTNONE: no .be worktree anchor from '" + p + "'";
  const a = discover.resolveAnchor(wt);                // the anchor READER, not a climb
  const top = topDir || topOf(wt) || wt;
  const work = workRoot();
  //  `wtree` is the worktree's NAME; the main tree has no `//name` address ("").
  const wtree = work && top.indexOf(work + "/") === 0 ? top.slice(work.length + 1) : "";
  //  `spath` = the INNERMOST submodule under its tree top, `rpath` = within it.
  const spath = wt === top ? "" : slash(wt.slice(top.length + 1));
  const rpath = p === wt ? "" : p.slice(wt.length + 1);
  const root = projectRoot();
  return { store: storeOf(a), mpath: root ? slash(root) : "", shard: a.project,
           wtree: wtree, spath: spath, rpath: rpath,
           wt: wt, bePath: a.bePath, storePath: a.storePath,
           project: a.project, root: a.root };
}

function step1() {
  const root = projectRoot();
  if (!root) throw "PROJNONE: no .be below $BE_ROOT above " + io.cwd() + " — no repo";
  return { mpath: slash(root), store: storeOf(discover.resolveAnchor(root)) };
}

//  steps 2-4: fix the filesystem path, then read `wtree`/`spath`/`rpath`/`shard`
//  off it.  Step 2 (`//WT/wtrel` -> $SRC_ROOT/work/WT/wtrel, `///mtrel` ->
//  $SRC_ROOT/mtrel) and step 3's confinement both live in discover.resolve.
function frame(cu, u) {
  //  3.1: the worktree comes from the context when the uri lacks one.  3.2: a
  //  relative path resolves against the context's path — an OWN `//WT` reroots.
  const own   = u.authority !== undefined;
  const wtree = own ? (u.host || "") : (cu.host || "");
  const base  = own ? "" : (cu.path || "");
  //  The TRAILING SLASH says ENTERED — `sub/` is a dir you are INSIDE, `sub` is
  //  the entry as its PARENT names it.  It must be read off the RAW path: the
  //  resolve below normalises it away (argRel re-adds it for the same reason).
  const raw = u.path || "";
  const entered = raw.length > 1 && raw[raw.length - 1] === "/";
  //  discover.resolve reads .host/.path off a plain object and throws NAVESCAPE
  //  on any climb-out; `//`/`//.` name the main tree, `//WT` the worktree.
  const abs = discover.resolve({ host: wtree, path: base }, raw);
  const top = discover.resolve({ host: wtree, path: "" }, "");

  //  step 4: treeAt climbs from the path to its INNERMOST anchor — that IS the
  //  submodule descent (a mounted sub is a wt) — and derives shard/spath/rpath
  //  off it.  This is the SAME routine the CLI reaches as `be.treeAt`; nothing
  //  is recomputed here.
  let r;
  try { r = treeAt(abs, undefined, top); }
  catch (e) { throw "WTNONE: no worktree anchors '" + abs + "'"; }
  if (r.wt !== top && r.wt.indexOf(top + "/") !== 0)
    throw "WTNONE: //" + wtree + " anchors no worktree at " + top;

  //  THE TRAILING-SLASH CONVENTION, at a mount root.  `sub/` = entered: keep the
  //  sub, so step 5.5 reads ITS wtlog -> the DE-FACTO checkout.  Bare `sub` = the
  //  entry its PARENT names: re-anchor to the parent, so step 6 reads the gitlink
  //  -> otype "commit" + the DE-JURE pin.  A path THROUGH a mount is already
  //  inside (it carries a `/` after the mount), so only the root is in question.
  //  `from` = the parent: climb from THERE, but keep measuring rpath from `abs`.
  if (r.wt === abs && abs !== top && !entered) {
    try { r = treeAt(abs, pathlib.dirname(abs), top); }
    catch (e) { throw "WTNONE: no worktree anchors '" + abs + "'"; }
  }
  return { wtree: wtree, spath: r.spath, rpath: r.rpath, shard: r.shard,
           _repo: r, _abs: abs };
}

//  step 5.1/5.2/5.4: a hex token -> a sha.  5.1 takes a full sha verbatim;
//  5.2/5.4 resolve a 6..40 hashlet against the store/shard index first.
function shaOfHex(k, hexish) {
  if (isFullSha(hexish)) return hexish;                      // 5.1
  if (isHashlet(hexish)) return k.resolveHexAny(hexish);     // 5.2 / 5.4
  return undefined;
}

//  EVERY rung of step 5 lands here: `chash` is a COMMIT in all cases — 5.1/5.2/
//  5.4 say so, 5.5 says "the tree's current commit", and step 6 can only follow
//  a path from a commit.  The check must READ the object, and a commit carries
//  its root tree — so step 6's tree comes out of the same read, for free.
function commitAt(k, sha, how) {
  if (!sha) throw "HASHNONE: " + how + " names no object in the shard index";
  const o = k.getObject(sha);
  if (!o) throw "HASHNONE: no object " + sha + " in the shard index";
  if (o.type !== "commit") throw "NOTACOMMIT: " + how + " is a " + o.type;
  const tree = git.parseCommit(o.bytes).tree;
  if (!tree) throw "NOTACOMMIT: the commit " + sha + " carries no tree";
  return { chash: sha, tree: tree };
}

//  step 5.5: no revision slot -> the tree's current commit, i.e. its last get
//  or post record.  `wtree`+`spath` already picked the wtlog: the sub's own.
function baseAt(repo) {
  let cur;
  try { cur = wtlog.open(repo).curTip(); } catch (e) { cur = undefined; }
  if (!cur || !isFullSha(cur.sha))
    throw "BASENONE: " + repo.wt + " has no get/post record to resolve against";
  return cur.sha;
}

//  step 5: the commit, in the spec's order — `#sha`/`#hashlet` (5.1/5.2), then
//  `?branch` (5.3) then `?hashlet` (5.4), then the tree's own tip (5.5).  Each
//  rung ends in commitAt, so every path out of here is a checked commit.
function chashOf(k, u, repo) {
  const frag = u.fragment;
  if (frag !== undefined && frag !== "")                             // 5.1 / 5.2
    return commitAt(k, shaOfHex(k, frag), "#" + frag);
  const q = u.query;
  if (q !== undefined) {                                             // `?` = trunk
    const ref = k.resolveRef(q);                                     // 5.3
    if (ref && isFullSha(ref)) return commitAt(k, ref, "?" + q);
    if (isHashlet(q)) return commitAt(k, shaOfHex(k, q), "?" + q);   // 5.4
    throw "REFNONE: '?" + q + "' resolves as neither branch nor hashlet";
  }
  return commitAt(k, baseAt(repo), "the worktree base");             // 5.5
}

//  step 6: follow rpath from the commit's root tree -> otype/ohash.  The tree
//  came free from step 5's commit check; an empty rpath IS that root tree
//  (descendPath's no-op case), never the commit.
function objectAt(k, c, rpath) {
  const leaf = k.descendPath(c.tree, String(rpath || "").split("/"));
  if (!leaf) throw "PATHNONE: no '" + rpath + "' in the tree at " + c.chash;
  //  descendPath reports the MODE class (exe/link/blob); the git OBJECT type is
  //  what the record names — a gitlink is a commit, every file kind a blob.
  const otype = leaf.kind === "tree" ? "tree"
              : leaf.kind === "commit" ? "commit" : "blob";
  return { otype: otype, ohash: leaf.sha };
}

//  resolve_hash(context_uri, uri) -> the record.  THE entry point: steps 1-6.
function resolve_hash(context_uri, uri) {
  const cu = asURI(context_uri);
  const u  = asURI(uri);
  if (u.scheme !== undefined)
    throw "URISCHEME: '" + uri + "' is a transport URI, not a worktree path";

  const s1 = step1();                        // store, mpath
  const f  = frame(cu, u);                   // wtree, spath, rpath, shard
  const k  = store.open(f._repo.storePath, f._repo.project);
  const c  = chashOf(k, u, f._repo);         // step 5: { chash, tree }
  const o  = objectAt(k, c, f.rpath);        // step 6: descends c.tree

  //  `shard` is "" for a FLAT single-shard store (the `.be` dir holds the packs
  //  directly): then store/shard composes to the store itself, which is true.
  return { store: s1.store, mpath: s1.mpath, shard: f.shard,
           wtree: f.wtree, spath: f.spath, rpath: f.rpath, chash: c.chash,
           otype: o.otype, ohash: o.ohash };
}

module.exports = { resolve_hash: resolve_hash,
                   projectRoot: projectRoot, workRoot: workRoot,
                   todoRoot: todoRoot, metaRoot: metaRoot,
                   //  URI-016: step 4 + the layout — THE `.be` climb lives here
                   //  and nowhere else; discover.treeAt delegates to treeAt.
                   treeAt: treeAt, topOf: topOf };
