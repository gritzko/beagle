//  diff.js — the `diff:` read-only VIEW as a resident-loop handler (JAB-014).
//  Pure JS over the libabc/libdog bindings: the `weave` 2-layer diff
//  (fold/scope/emitDiff/emitFull) single-sourced with C, store.js object/tree
//  reads, wtlog baseline, sha-skip + binary-probe in plain JS, core/recurse.js
//  for in-process sub recursion.  NO dog binary spawn, NO /proc.  Mirrors
//  graf/DIFFREF.c (GRAFDiff2Layer / GRAFDiffWtTree / GRAFDiffTreeRefs) +
//  GRAF.exe.c's URI shape table + be bediff sub pin-range relay.
//
//  handle(row, ctx): JS-071 — the loop NEVER sets ctx.views, so the handler
//  RE-PARSES the whole `diff:<uri>` off ctx.args[0] (the loop's one-shot
//  scheme:uri form), exactly like log.js/commit.js/tree.js.  parseDiffArg
//  builds the spec (mode range|wt, from/to/baseline shas, navver, path) in
//  the SAME shape :278-302 consume.  A caller (commit.js, COMMIT-006) that
//  pins ctx.views[row.uri] still wins — pinned spec ?? reparse.  Output is
//  HUNK bytes via ctx.out.chunk (the HUNK `.plain`/`.color` cursor's diff:-
//  scheme line render — NOT bro's pager).

"use strict";

const be      = require("../../core/discover.js");
const store   = require("../../shared/store.js");
const wtlog   = require("../../shared/wtlog.js");
const shalib  = require("../../shared/util/sha.js");
const recurse = require("../../core/recurse.js");
//  DIFF-010: the shared grow-on-"out full" WEAVE/HUNK fold retry (mirrors
//  loop.js:128-142) — a large diff fold no longer throws "out full".
const weave   = require("../../shared/weave.js");

const isFullSha = shalib.isFullSha;
const frameSha  = shalib.frameSha;

//  BRO-006: tok32 pack + the `U` (URI click-target) tag.  Mirrors C
//  graf/GRAF.c:522/535 (`tok32Pack('U', u8bDataLen(out))`): a visible token
//  followed by a `U`-tagged token whose appended TEXT bytes ARE the nav URI.
//  tag `U` = 'U'-'A' = 20; the bytes stay hidden in plain/color (HUNK.c skips
//  'U' spans), the pager's _uriAt reads them as the click target.
const TAG_U = 20;
function tok(tag, end) { return ((tag & 0x1f) << 27) | (end & 0xffffff); }

//  BRO-006: append a `U` click-target to one HUNK record.  `uri` is the hunk's
//  own navigable file URI (`diff:<path>?<navver>#L<n>`); the bytes go after the
//  visible text and a single `U` token covers them, so a pager left-click on
//  the diff body opens that file at the line.  An empty uri (text-only gitlink
//  hunk) gets no target.  Returns the augmented { text, toks } pair.
function withUTarget(uri, text, toks) {
  if (!uri || !uri.length) return { text: text, toks: toks };
  const uriBytes = utf8.Encode(uri);
  const full = new Uint8Array(text.length + uriBytes.length);
  full.set(text, 0);
  full.set(uriBytes, text.length);
  const out = new Uint32Array(toks.length + 1);
  out.set(toks, 0);
  out[toks.length] = tok(TAG_U, full.length);
  return { text: full, toks: out };
}

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
  //  DIFF-010: each fold grows its own WEAVE buffer on "out full" (weave.fold),
  //  so a large blob no longer throws.
  let wA, wB, from, to;
  if (f.length === 0) {
    //  Addition: base layer = the to-content (ID_FROM), diff layer = empty
    //  (ID_TO); invert the scope roles so `from` is the empty side.
    wA = weave.fold(null, t, ext, ID_FROM);
    wB = weave.fold(wA, f, ext, ID_TO);
    from = wB.scope([ID_FROM, ID_TO]); to = wB.scope([ID_FROM]);
  } else {
    //  Normal / deletion: base layer = from-content, diff layer = to.
    wA = weave.fold(null, f, ext, ID_FROM);
    wB = weave.fold(wA, t, ext, ID_TO);
    from = wB.scope([ID_FROM]); to = wB.scope([ID_FROM, ID_TO]);
  }

  //  DIFF-010: the HUNK emit buffer also grows on "out full" (a large windowed
  //  or whole-file diff overflows a fixed HUNK cap, like the fold above).
  const hint = (f.length + t.length) * 2 + 1024;
  const hd = weave.growOnFull(function (cap) { return abc.ram("HUNK", cap); },
    function (h) {
      if (full) wB.emitFull(from, to, name, "diff:", navver, h);
      else      wB.emitDiff(from, to, name, navver, h);
      return h;
    }, 1 << 18, hint);

  emitHunks(hd, color, out);
}

//  Render every record in a HUNK container through the diff:-scheme cursor.
//  hunk_uri_is_diff routes a `diff:`-URI hunk to the unified line render in
//  both `.plain` and `.color`; a text-only hunk (gitlink line, empty uri)
//  renders verbatim.  Each rendered chunk owns its newlines (out.chunk).
//
//  BRO-006: each hunk also gets a `U` click-target (its own `diff:<path>#L<n>`
//  uri) re-fed to ctx.sink, so a pager left-click opens the file at the line
//  (mirrors C graf/GRAF.c:522/535).  The U bytes are hidden in plain/color
//  (HUNK.c skips 'U' spans), so out.chunk's rendered text stays byte-identical.
function emitHunks(hd, color, out) {
  hd.rewind();
  while (hd.next()) {
    //  BRO-006: feed the raw record to the toks sink (it appends the U target);
    //  the rendered text still goes to out.chunk for the plain/color channel.
    if (out.feed) out.feed(utf8.Decode(hd.uri), hd.text.slice(), hd.toks.slice());
    //  DIFF-010: the per-record render buffer grows on "out full" — one large
    //  hunk's plain/color bytes can exceed a fixed io.buf cap.
    const hint = hd.text.length * 4 + 1024;
    const o = weave.growOnFull(function (cap) { return io.buf(cap); },
      function (b) { if (color) hd.color(b); else hd.plain(b); return b; },
      1 << 18, hint);
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
//  BRO-006: `feed` is prefixed too — the raw hunk uri `diff:<subname>#L<n>`
//  becomes `diff:<prefix>/<subname>#L<n>` BEFORE the sink appends its U target,
//  so the click opens the mounted path.
function prefixingSink(out, prefix) {
  if (!prefix) return out;
  return {
    chunk: function (text) {
      let s = text;
      s = s.split("--- a/").join("--- a/" + prefix + "/");
      s = s.split("+++ b/").join("+++ b/" + prefix + "/");
      s = s.split("diff:").join("diff:" + prefix + "/");
      out.chunk(s);
    },
    feed: out.feed ? function (uri, text, toks) {
      out.feed(uri.split("diff:").join("diff:" + prefix + "/"), text, toks);
    } : undefined,
  };
}

//  BRO-006: wrap the run's two sinks into the one `out` the diff emit chain
//  threads.  `chunk` is the plain/color rendered-text channel (ctx.out); `feed`
//  carries the toks sink (ctx.sink) for the pager.  JS-071: the per-hunk `U`
//  click-target rides the body bytes ONLY in --tlv (the bro pager's per-line
//  nav, hunksFromTlv reads text+toks).  In plain/color the body U-target leaks
//  a phantom trailing line in the HUNK line-walk (C graf has no body U-target;
//  it navigates via the hunk's own `uri`), so we feed the bare record there —
//  byte-parity with native, click still follows hunk.uri (pager.js _followRow).
function diffOut(ctxOut, sink, mode) {
  const wantU = mode === "tlv";
  return {
    chunk: function (text) { if (ctxOut && ctxOut.chunk) ctxOut.chunk(text); },
    feed: sink ? function (uri, text, toks) {
      const aug = wantU ? withUTarget(uri, text, toks) : { text: text, toks: toks };
      sink.feed(uri, aug.text, aug.toks, "", 0n);
    } : undefined,
  };
}

//  --- JS-071: re-parse the `diff:` URI off ctx.args[0] -------------------
//  The loop NEVER sets ctx.views; re-parse the one-shot `diff:<uri>` like
//  log/commit/tree.  Resolve a ref (branch-FIRST, then full-sha / hashlet) to
//  a commit sha via the store reader.  Returns undefined for a bare sha that
//  is no commit (the caller treats a null tree as the empty side).
function resolveCommit(k, ref) {
  if (!ref) return undefined;
  const byRef = k.resolveRef(ref);                    // branch / tag FIRST
  if (byRef && isFullSha(byRef)) return byRef;
  if (isFullSha(ref)) return k.getObject(ref) ? ref : undefined;
  if (/^[0-9a-f]{1,39}$/.test(ref)) return k.resolveHexAny(ref);
  return undefined;
}

//  Build the diff spec from `diff:<path>?<query>#<frag>` (the C GRAF.exe.c URI
//  shape table, :352-358).  Range forms (`?from..to`, legacy `?from#to`) need
//  no baseline; the no-range forms (`diff:`, `diff:file`, `diff:?branch`) take
//  the wt baseline.  A no-path `diff:?<hashlet>` is a commit-show (rev vs its
//  first parent), matching `git show`.  Returns { mode, fromSha, toSha,
//  baselineSha, navver, path } in the existing spec shape, or null when a ref
//  is unresolvable.
function parseDiffArg(k, repo, raw) {
  let first = String(raw || "");
  if (first.indexOf("diff:") !== 0) first = "diff:" + first;
  const u = new URI(first);
  const path = u.path || "";
  const query = u.query || "";
  const frag = u.fragment || "";

  //  RANGE: `?from..to` canonical (navver = the verbatim query) or legacy
  //  `?from#to` (frag = the range `to`, no navver / no #L anchor).
  const dots = query.indexOf("..");
  let fromRef, toRef, navver = "";
  if (dots > 0 && dots < query.length - 2) {
    fromRef = query.slice(0, dots); toRef = query.slice(dots + 2); navver = query;
  } else if (query && frag) {
    fromRef = query; toRef = frag;
  }
  if (fromRef !== undefined) {
    const fromSha = resolveCommit(k, fromRef);
    const toSha   = resolveCommit(k, toRef);
    if (!fromSha || !toSha) return null;              // unresolvable ref → nothing
    return { mode: "range", fromSha: fromSha, toSha: toSha,
             baselineSha: "", navver: navver, path: path };
  }

  const baseSha = (wtlog.open(repo).baselineTip() || {}).sha || "";
  if (query) {
    //  `?branch` (no range): commit-show when no path AND a hashlet that is NOT
    //  a branch name (rev vs first parent, `git show`); else branch-vs-base.
    const isName = !!k.resolveRef(query);
    if (!path && !isName && /^[0-9a-f]{6,40}$/.test(query)) {
      const sha = resolveCommit(k, query);
      const parents = sha ? (k.commitParents(sha) || []) : [];
      if (sha && parents.length) {
        return { mode: "range", fromSha: parents[0], toSha: sha,
                 baselineSha: "", navver: parents[0] + ".." + sha, path: "" };
      }
    }
    const branchSha = resolveCommit(k, query);
    if (!branchSha || !baseSha) return null;
    return { mode: "range", fromSha: branchSha, toSha: baseSha,
             baselineSha: "", navver: "", path: path };
  }
  //  No query → wt vs base (the loop's `diff:` / `diff:<file>`).
  return { mode: "wt", fromSha: "", toSha: "",
           baselineSha: baseSha, navver: "", path: path };
}

//  --- the handler -------------------------------------------------------
module.exports = function handle(row, ctx) {
  const out = diffOut(ctx && ctx.out, ctx && ctx.sink, (ctx && ctx.mode) || "plain");
  const flags = (ctx && ctx.flags) || [];
  const color = flags.indexOf("--color") >= 0;

  const repo = (ctx && ctx.repo) || be.find();
  const k = store.open(repo.storePath, repo.project);

  //  JS-071: a pinned ctx.views spec (commit.js / COMMIT-006) wins; else
  //  re-parse the one-shot `diff:<uri>` off ctx.args[0] (never row.uri).
  let spec = (ctx && ctx.views && ctx.views[row.uri]) || null;
  if (!spec) {
    const rawArgs = (ctx && ctx.args && ctx.args.length) ? ctx.args : [row.uri];
    spec = parseDiffArg(k, repo, rawArgs[0]);
  }
  if (!spec) return;                                  // unresolvable / no spec

  const navver = spec.navver || "";

  if (spec.mode === "range") {
    //  ref-vs-ref: resolve each side's TREE sha (commit → tree), then diff.
    const fromTree = spec.fromSha ? k.commitTree(spec.fromSha) : null;
    const toTree   = spec.toSha   ? k.commitTree(spec.toSha)   : null;
    if (spec.path) {
      //  File scope → whole-file view (emitFull), full=YES.  DIFF-011: a
      //  scoped path is read straight from the parent commit trees — native
      //  `be diff:<sub>/<file>?<from>..<to>` does NOT recurse a gitlink for a
      //  range (GRAFPathDescend dead-ends at the 160000 and GRAFWeaveDiff
      //  emits nothing), so the JS stays parent-tree-only here for byte-parity
      //  (mount awareness lives in the wt+path branch, where native recurses
      //  the live mount).  blobAtTree returns undefined at the gitlink → from
      //  and to both empty → diffFile skips → empty output, matching native.
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
      //  File scope full: base blob vs wt file.  DIFF-011: a file UNDER a
      //  mounted sub takes its FROM side from the SUB's OWN baseline tree (the
      //  parent tree has only the gitlink) — the to side stays the wt file.
      const m = subMountSplit(k, baseTree, repo, spec.path, ctx);
      let fB;
      if (m) {
        const subBase = (wtlog.open(m.subRepo).baselineTip() || {}).sha || "";
        const subTree = subBase ? m.subK.commitTree(subBase) : null;
        fB = blobAtTree(m.subK, subTree, m.rest);
      } else {
        fB = blobAtTree(k, baseTree, spec.path);
      }
      const tB = readWtFile(join(repo.wt, spec.path));
      diffFile(spec.path, fB, tB, true, "", color, out);
    } else {
      diffWtTree(k, baseTree, repo, color, ctx, "", out);
    }
  }
};

//  --- DIFF-011: mount-aware scoped-path resolution ---------------------
//  `diff:<sub>/<file>` (a file UNDER a mounted submodule) must read its
//  from/baseline side from the SUB's OWN shard, not the parent tree — the
//  parent tree has only a `160000` gitlink at `<sub>`, which blobAtTree can't
//  descend (→ undefined → a false WHOLLY-ADDED).  This mirrors recurseSubPins
//  (the recursive whole-tree path): enumerate the parent tree's gitlinks, gate
//  each on recurse.isMount, open the sub via be.find/store.open.
//
//  subMountSplit(k, parentTreeSha, repo, path, ctx) → null when `path` is NOT
//  under a live mount (caller keeps the plain parent-tree read).  Else
//      { subK, subRepo, subPath, rest, oldPin }
//  where subPath is the deepest matching gitlink prefix, rest the in-sub
//  remainder, oldPin the parent gitlink sha (the sub's pin in THIS tree), and
//  subK/subRepo the opened sub shard + handle (for the from-side tree read).
function subMountSplit(k, parentTreeSha, repo, path, ctx) {
  if (!parentTreeSha || !path) return null;
  const flags = (ctx && ctx.flags) || [];
  if (flags.indexOf("--nosub") >= 0) return null;     // sub content suppressed
  const subs = treeMap(k, parentTreeSha).subs;        // { subpath -> pin sha }
  //  Deepest gitlink prefix that `path` lies under (`<sub>/…`).
  let best = "";
  for (const sp of Object.keys(subs)) {
    if (path === sp || path.indexOf(sp + "/") === 0) {
      if (sp.length > best.length) best = sp;
    }
  }
  if (!best) return null;
  //  A declared-but-unmounted sub degrades sanely: no false wholly-added — we
  //  just don't have the sub's baseline, so leave the from-side as the parent
  //  read (undefined at the gitlink) → diffFile's empty-from path, same as a
  //  genuinely added file.  Only a LIVE mount resolves to the sub shard.
  if (!recurse.isMount(repo.wt, best)) return null;
  let subRepo;
  try { subRepo = be.find(join(repo.wt, best)); } catch (e) { return null; }
  let subK;
  try { subK = store.open(subRepo.storePath, subRepo.project); }
  catch (e) { return null; }
  return { subK: subK, subRepo: subRepo, subPath: best,
           rest: path.slice(best.length + 1), oldPin: subs[best] };
}

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

//  BRO-006: the loop dispatches the module AS the handler; the test reaches the
//  U click-target builder via these named exports (the LOG-001 repro pattern).
module.exports.withUTarget = withUTarget;
module.exports.tok = tok;
module.exports.TAG_U = TAG_U;
