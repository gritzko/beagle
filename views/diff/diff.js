//  diff.js — the `diff:` read-only VIEW as a resident-loop handler (JAB-014).
//  Pure JS over the libabc/libdog bindings: the `weave` 2-layer diff
//  (fold/scope/emitDiff/emitFull) single-sourced with C, store.js object/tree
//  reads, wtlog baseline, sha-skip + binary-probe in plain JS, core/recurse.js
//  for in-process sub recursion.  NO dog binary spawn, NO /proc.  Mirrors
//  graf/DIFFREF.c (GRAFDiff2Layer / GRAFDiffWtTree / GRAFDiffTreeRefs) +
//  GRAF.exe.c's URI shape table + be bediff sub pin-range relay.
//
//  handle(row, ctx): the seed (resolve.js classifyView) pinned this arg's
//  from/to shas + navver + scope into ctx.views[row.uri]; the handler does
//  ZERO ref resolution.  Output is HUNK bytes via ctx.out.chunk (the HUNK
//  `.plain`/`.color` cursor's diff:-scheme line render — NOT bro's pager).

"use strict";

const be      = require("../../core/discover.js");
const store   = require("../../shared/store.js");
const shalib  = require("../../shared/util/sha.js");
const recurse = require("../../core/recurse.js");

const isFullSha = shalib.isFullSha;
const frameSha  = shalib.frameSha;

//  JAB-014: plain-JS basename suffix (the weave lexer's language key) —
//  PATHu8sExt twin: the bytes after the LAST '.' in the basename, "" if none.
function extOf(path) {
  const base = path.slice(path.lastIndexOf("/") + 1);
  const dot = base.lastIndexOf(".");
  return dot > 0 ? base.slice(dot + 1) : "";
}

//  JAB-014: git's binary heuristic (BLAME-006b) — a blob is binary iff a NUL
//  byte appears in its first 8000 bytes.  Skip the tokenise + doomed emit.
const BIN_PROBE = 8000;
function isBinary(bytes) {
  if (!bytes || !bytes.length) return false;
  const n = Math.min(BIN_PROBE, bytes.length);
  for (let i = 0; i < n; i++) if (bytes[i] === 0) return true;
  return false;
}

//  Two distinct 16-hex hashlet ids for the from/to weave layers (any two
//  distinct values; the predicates only care about !=).
const ID_FROM = "0000000000000001", ID_TO = "0000000000000002";

//  Build the 2-layer weave for one file pair and render its hunks into `out`
//  via the HUNK `.plain`/`.color` cursor (the EXACT weave.js:75-99 path).
//  from==to → skip (byte-identical); binary either side → skip.  `full` picks
//  emitFull (file scope, whole file) vs emitDiff (tree scope, windowed).
//
//  JAB-014 empty-from ADDITION: `fold(null,"")` makes a WEAVEEmpty layer, so a
//  second fold COLLAPSES it (WEAVENext discards an empty base) — no diff.  The
//  C builds two from-blobs + WEAVEDiff, which the binding doesn't expose; the
//  faithful workaround is to fold the layers in the OTHER order (content as the
//  base, empty as the diff) and INVERT the from/to scopes — the SAME 'H'
//  records the C path produces (the +/- sides come from the scope roles).
function diffFile(name, fromBytes, toBytes, full, navver, color, out) {
  const f = fromBytes || new Uint8Array(0);
  const t = toBytes || new Uint8Array(0);
  if (f.length === t.length && bytesEq(f, t)) return;          // from==to skip
  if (isBinary(f) || isBinary(t)) return;                      // binary skip

  const ext = extOf(name);
  const wA = abc.ram("WEAVE", 1 << 18);
  const wB = abc.ram("WEAVE", 1 << 18);
  let from, to;
  if (f.length === 0) {
    //  Addition: base layer = the to-content (ID_FROM), diff layer = empty
    //  (ID_TO); invert the scope roles so `from` is the empty side.
    wA.fold(null, t, ext, ID_FROM);
    wB.fold(wA, f, ext, ID_TO);
    from = wB.scope([ID_FROM, ID_TO]); to = wB.scope([ID_FROM]);
  } else {
    //  Normal / deletion: base layer = from-content, diff layer = to.
    wA.fold(null, f, ext, ID_FROM);
    wB.fold(wA, t, ext, ID_TO);
    from = wB.scope([ID_FROM]); to = wB.scope([ID_FROM, ID_TO]);
  }

  const hd = abc.ram("HUNK", 1 << 18);
  if (full) wB.emitFull(from, to, name, "diff:", navver, hd);
  else      wB.emitDiff(from, to, name, navver, hd);

  emitHunks(hd, color, out);
}

//  Render every record in a HUNK container through the diff:-scheme cursor.
//  hunk_uri_is_diff routes a `diff:`-URI hunk to the unified line render in
//  both `.plain` and `.color`; a text-only hunk (gitlink line, empty uri)
//  renders verbatim.  Each rendered chunk owns its newlines (out.chunk).
function emitHunks(hd, color, out) {
  hd.rewind();
  while (hd.next()) {
    const o = io.buf(1 << 18);
    if (color) hd.color(o); else hd.plain(o);
    out.chunk(utf8.Decode(o.data()));
  }
}

function bytesEq(a, b) {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

//  Blob bytes at a tree leaf sha, or undefined.  A non-blob / missing object
//  yields undefined (diffFile treats it as the empty side).
function blobBytes(k, sha) {
  if (!sha || !isFullSha(sha)) return undefined;
  const obj = k.getObject(sha);
  if (!obj || obj.type !== "blob") return undefined;
  return obj.bytes;
}

//  --- gitlink pin-bump line (DIFF-001) ----------------------------------
//  Render `<path> <old>..<new>\n` as a TEXT-ONLY hunk (empty uri) so it
//  travels the SAME render channel as every file hunk (diffref_emit_gitlink).
function emitGitlink(path, oldSha, newSha, color, out) {
  const o = (oldSha && isFullSha(oldSha)) ? oldSha : "";
  const n = (newSha && isFullSha(newSha)) ? newSha : "";
  const line = path + " " + o + ".." + n + "\n";
  const hd = abc.ram("HUNK", 1 << 14);
  hd.feed("", utf8.Encode(line), new Uint32Array(0));   // text-only hunk
  emitHunks(hd, color, out);
}

//  --- tree map: leaf path -> { sha, kind } (files + gitlinks) -----------
//  readTreeRecursive yields file/exe/symlink leaves (kind f/x/l) and gitlinks
//  (kind s).  We keep files (blob diff) and subs (pin bump) separately.
function treeMap(k, treeSha) {
  const files = {}, subs = {};
  if (!treeSha) return { files: files, subs: subs };
  k.readTreeRecursive(treeSha, function (leaf) {
    if (leaf.kind === "s") subs[leaf.path] = leaf.sha;
    else files[leaf.path] = leaf.sha;
  });
  return { files: files, subs: subs };
}

//  --- ref-vs-ref whole-tree diff (GRAFDiffTreeRefs) ---------------------
//  Pair every to-entry with its from-entry by path: same sha → skip; gitlink
//  → pin-bump line (+ sub recursion); else blob diff.  Then from-only entries
//  (deletions) diff vs empty.  to-entries first (lex), then from-only (lex) —
//  the C iteration order.
function diffTreeRefs(k, fromTreeSha, toTreeSha, navver, color, ctx, repo,
                      prefix, out) {
  const F = treeMap(k, fromTreeSha), T = treeMap(k, toTreeSha);

  const toPaths = Object.keys(T.files).sort();
  for (const p of toPaths) {
    const fsha = F.files[p], tsha = T.files[p];
    if (fsha && fsha === tsha) continue;                       // unchanged
    diffFile(p, blobBytes(k, fsha), blobBytes(k, tsha), false, navver,
             color, out);
  }
  //  Gitlinks: pin-bump line + recurse the sub for its content diff over the
  //  pin range (be bediff relay, in-process).  Sub paths in lex order.
  const subPaths = uniqSorted(Object.keys(T.subs), Object.keys(F.subs));
  for (const p of subPaths) {
    const oldPin = F.subs[p], newPin = T.subs[p];
    if (oldPin && newPin && oldPin === newPin) continue;       // unchanged
    emitGitlink(p, oldPin, newPin, color, out);
    recurseSubPins(p, oldPin, newPin, color, ctx, repo, prefix, out);
  }
  //  from-only files (deletions): blob vs empty, lex order.
  for (const p of Object.keys(F.files).sort()) {
    if (T.files[p] !== undefined) continue;
    diffFile(p, blobBytes(k, F.files[p]), undefined, false, navver, color, out);
  }
}

//  union of two key lists, sorted unique.
function uniqSorted(a, b) {
  const seen = {}, out = [];
  for (const x of a.concat(b)) if (!seen[x]) { seen[x] = 1; out.push(x); }
  out.sort();
  return out;
}

//  --- wt-vs-base whole-tree diff (GRAFDiffWtTree) -----------------------
//  Base tree leaves heap-merged with a wt walk: BOTH (sha-skip on the wt's
//  freshly-computed blob sha) | BASE_ONLY (deletion) | WT_ONLY (untracked,
//  SKIPPED — diff: is wt-vs-base, not wt-vs-empty).  Gitlinks prune their
//  descendants and render a pin bump only on a wt-side change (rare — usually
//  a clean mount has no wt pin move here, matching native's wt path).
function diffWtTree(k, baseTreeSha, repo, color, ctx, prefix, out) {
  //  Base-tree-driven (tracked files only): an UNTRACKED wt file has no base
  //  side, so diff: skips it (wt-vs-base, not wt-vs-empty) — no .gitignore
  //  wt-walk needed here.  A gitlink prunes its descendants from the walk.
  const F = treeMap(k, baseTreeSha);
  const subPrefixes = Object.keys(F.subs).map(function (p) { return p + "/"; });
  function underSub(p) {
    for (const sp of subPrefixes) if (p.indexOf(sp) === 0) return true;
    return false;
  }

  const basePaths = Object.keys(F.files).sort();
  for (const p of basePaths) {
    if (underSub(p)) continue;
    const fsha = F.files[p];
    const wtPath = join(repo.wt, p);
    const wt = readWtFile(wtPath);
    if (wt === undefined) {
      //  BASE_ONLY: deleted in wt → blob vs empty.
      diffFile(p, blobBytes(k, fsha), undefined, false, "", color, out);
      continue;
    }
    //  BOTH: sha-skip on the wt blob sha vs the base entry sha.
    const wtSha = frameSha("blob", wt);
    if (wtSha === fsha) continue;
    diffFile(p, blobBytes(k, fsha), wt, false, "", color, out);
  }
}

//  Read a wt file's bytes, or undefined when absent/unreadable.
function readWtFile(path) {
  try { return io.mmap(path, "r").data().slice(); } catch (e) { return undefined; }
}

function join(dir, name) {
  return (dir.endsWith("/") ? dir : dir + "/") + name;
}

//  --- sub recursion (be bediff pin-range relay, in-process) -------------
//  Per bumped gitlink recurse `diff:?<old>#<new>` UNDER the path prefix: open
//  the mounted sub, diff its pin range, path-prefix every hunk's name.  The
//  prefix join happens by re-running the tree diff with names joined under
//  `<prefix>/<subpath>` (the recurse.js emit-prefix discipline).
function recurseSubPins(subPath, oldPin, newPin, color, ctx, parentRepo,
                        prefix, out) {
  const flags = (ctx && ctx.flags) || [];
  if (flags.indexOf("--nosub") >= 0) return;          // sub content suppressed
  if (!isFullSha(oldPin) || !isFullSha(newPin) || oldPin === newPin) return;
  if (!recurse.isMount(parentRepo.wt, subPath)) return;
  let subRepo;
  try { subRepo = be.find(join(parentRepo.wt, subPath)); } catch (e) { return; }
  const subK = store.open(subRepo.storePath, subRepo.project);
  const fromTree = subK.commitTree(oldPin), toTree = subK.commitTree(newPin);
  if (!fromTree || !toTree) return;
  const subPrefix = recurse.joinPrefix(prefix, subPath);
  //  Wrap `out.chunk` so every name the sub emits is path-prefixed.  The
  //  hunk render embeds the name in `--- a/<name>` / `+++ b/<name>` lines and
  //  the color banner `diff:<name>…`; a prefixing sink rewrites those.
  const subOut = prefixingSink(out, subPrefix);
  const navver = oldPin + ".." + newPin;
  diffTreeRefs(subK, fromTree, toTree, navver, color, ctx, subRepo,
               subPrefix, subOut);
}

//  A sink that path-prefixes the file NAME in each rendered diff chunk under
//  `prefix` (JAB-004 emit-prefix).  The producer emitted bare `<name>`; the
//  recursion rewrites `--- a/<name>`, `+++ b/<name>`, and the `diff:<name>`
//  color banner to `<name>` under the mount.  A plain JS string rewrite over
//  the already-rendered chunk (the name set is the sub's own leaf paths).
function prefixingSink(out, prefix) {
  if (!prefix) return out;
  return { chunk: function (text) {
    let s = text;
    s = s.split("--- a/").join("--- a/" + prefix + "/");
    s = s.split("+++ b/").join("+++ b/" + prefix + "/");
    s = s.split("diff:").join("diff:" + prefix + "/");
    out.chunk(s);
  } };
}

//  --- the handler -------------------------------------------------------
module.exports = function handle(row, ctx) {
  const out = ctx && ctx.out;
  const flags = (ctx && ctx.flags) || [];
  const color = flags.indexOf("--color") >= 0;
  const spec = (ctx && ctx.views && ctx.views[row.uri]) || null;
  if (!spec) return;                                  // no pinned view spec

  const repo = (ctx && ctx.repo) || be.find();
  const k = store.open(repo.storePath, repo.project);
  const navver = spec.navver || "";

  if (spec.mode === "range") {
    //  ref-vs-ref: resolve each side's TREE sha (commit → tree), then diff.
    const fromTree = spec.fromSha ? k.commitTree(spec.fromSha) : null;
    const toTree   = spec.toSha   ? k.commitTree(spec.toSha)   : null;
    if (spec.path) {
      //  File scope → whole-file view (emitFull), full=YES.
      const fB = blobAtTree(k, fromTree, spec.path);
      const tB = blobAtTree(k, toTree, spec.path);
      diffFile(spec.path, fB, tB, true, navver, color, out);
    } else {
      diffTreeRefs(k, fromTree, toTree, navver, color, ctx, repo, "", out);
    }
  } else {
    //  wt-vs-base: baseline tree from the seed-pinned baseline sha.
    const baseSha = spec.baselineSha || "";
    const baseTree = baseSha ? k.commitTree(baseSha) : null;
    if (spec.path) {
      //  File scope full: base blob vs wt file.
      const fB = blobAtTree(k, baseTree, spec.path);
      const tB = readWtFile(join(repo.wt, spec.path));
      diffFile(spec.path, fB, tB, true, "", color, out);
    } else {
      diffWtTree(k, baseTree, repo, color, ctx, "", out);
    }
  }
};

//  Blob bytes for `path` inside a tree (descend by path segments) — the
//  store.js readTree walk to the leaf, then the blob.  undefined when absent.
function blobAtTree(k, treeSha, path) {
  if (!treeSha) return undefined;
  const segs = path.split("/");
  let cur = treeSha;
  for (let i = 0; i < segs.length; i++) {
    const ents = k.readTree(cur);
    if (!ents) return undefined;
    let hit;
    for (const e of ents) if (e.name === segs[i]) { hit = e; break; }
    if (!hit) return undefined;
    if (i === segs.length - 1) return blobBytes(k, hit.sha);
    cur = hit.sha;
  }
  return undefined;
}
