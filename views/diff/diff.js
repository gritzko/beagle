//  diff.js — the `diff:` read-only VIEW as a resident-loop handler (JAB-014).
//  Pure JS over the libabc/libdog bindings: the `weave` 2-layer diff
//  (fold/scope/emitDiff/emitFull) single-sourced with C, store.js object/tree
//  reads, wtlog baseline, sha-skip + binary-probe in plain JS, core/recurse.js
//  for in-process sub recursion.  NO dog binary spawn, NO /proc.  Mirrors
//  graf/DIFFREF.c (GRAFDiff2Layer / GRAFDiffWtTree / GRAFDiffTreeRefs) +
//  GRAF.exe.c's URI shape table + be bediff sub pin-range relay.
//
//  JAB-004: a PURE plain-args verb — diffOne(arg) self-parses the whole
//  `diff:<uri>` string, reads be.repo/be.sink/be.out/be.flags/ambient.format
//  off the GLOBAL `be` only (no ctx).  A caller (commit.js, COMMIT-006) that
//  pins be.views[be.uri] to a spec still wins — pinned spec ?? reparse; a normal
//  CLI call has empty be.views so it reparses.  Output is HUNK bytes via
//  be.out.chunk (the HUNK `.plain`/`.color` cursor's diff:-scheme line render —
//  NOT bro's pager).

"use strict";

const store   = require("../../shared/store.js");
//  BE-030: worktree fs paths go THROUGH resolve() (context-confined wtpath).
const discover = require("../../core/discover.js");
const wtpath = discover.wtpath;
const wtlog   = require("../../shared/wtlog.js");
const shalib  = require("../../shared/util/sha.js");
const recurse = require("../../core/recurse.js");
//  DIFF-012: the no-arg / dir-scoped wt diff sources its dirty list from the
//  classifier (as `status`/bare `put` do), not a bespoke wt walk.
const classify = require("../../shared/classify.js");
//  DIFF-010: the shared grow-on-"out full" WEAVE/HUNK fold retry (mirrors
//  loop.js:128-142) — a large diff fold no longer throws "out full".
const weave   = require("../../shared/weave.js");
const ambient = require("../../shared/ambient.js");   // JAB-004: ctx→be bridge
const navlib  = require("../../shared/nav.js");        // URI-014: word-form re-bake of baked diff: URIs
const pathlib = require("../../shared/util/path.js");  // BE-011: join + wtJoin confinement

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
  //  Over the source cap → a BLOB: not tokenised, not diffed (weave.js policy).
  if (f.length > weave.MAX_SOURCE_SIZE || t.length > weave.MAX_SOURCE_SIZE) return;

  const ext = extOf(name);
  //  The fold/emit/render buffers are fixed at MAX_SOURCE_MARKED_UP (lazy mmap,
  //  no dynamic growth).  If a (sub-cap but token-dense) source overflows even
  //  that, there is no point diffing it — err out and treat it as a BLOB (skip).
  try {
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
    const hd = abc.ram("HUNK", weave.MAX_SOURCE_MARKED_UP);
    if (full) wB.emitFull(from, to, name, "diff:", navver, hd);
    else      wB.emitDiff(from, to, name, navver, hd);
    emitHunks(hd, color, out);
  } catch (e) {
    if (("" + e).includes("full")) return;                     // over cap → blob
    throw e;
  }
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
    //  The per-record render buffer is the fixed markup size (lazy mmap) — one
    //  hunk's plain/color bytes can't exceed it, so no dynamic growth.
    const o = io.ram(weave.MAX_SOURCE_MARKED_UP);
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

//  --- tree map: leaf path -> { sha, kind } (files + gitlinks + links) ---
//  readTreeRecursive yields file/exe/symlink leaves (kind f/x/l) and gitlinks
//  (kind s).  files (blob diff), subs (pin bump), and links (target-string
//  diff, never mmap'd) are kept separate.
//  JS-069: symlink leaves (kind "l") go to `links`, NOT `files` — a `files`
//  entry gets io.mmap'd wt-side, which FOLLOWS the link and leaks its target.
function treeMap(k, treeSha) {
  const files = {}, subs = {}, links = {};
  if (!treeSha) return { files: files, subs: subs, links: links };
  k.readTreeRecursive(treeSha, function (leaf) {
    if (leaf.kind === "s") subs[leaf.path] = leaf.sha;
    else if (leaf.kind === "l") links[leaf.path] = leaf.sha;
    else files[leaf.path] = leaf.sha;
  });
  return { files: files, subs: subs, links: links };
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
  //  JS-069: symlinks — diff their stored target-string blobs (both sides are
  //  tree leaves here, no wt read), to-first then from-only deletions (lex).
  for (const p of Object.keys(T.links).sort()) {
    const fsha = F.links[p], tsha = T.links[p];
    if (fsha && fsha === tsha) continue;                       // unchanged
    diffFile(p, blobBytes(k, fsha), blobBytes(k, tsha), false, navver, color, out);
  }
  for (const p of Object.keys(F.links).sort()) {
    if (T.links[p] !== undefined) continue;
    diffFile(p, blobBytes(k, F.links[p]), undefined, false, navver, color, out);
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
//  DIFF-012: enumerate the classifier's dirty paths (optionally under `prefix`)
//  and render each base-present one through the per-file wt-vs-base diff (lex order).
function diffWtTree(k, baseTreeSha, repo, color, ctx, prefix, out) {
  const F = treeMap(k, baseTreeSha);          // base tree: from-side blob shas
  const subPrefixes = Object.keys(F.subs).map(function (p) { return p + "/"; });
  function underSub(p) {
    for (const sp of subPrefixes) if (p.indexOf(sp) === 0) return true;
    return false;
  }

  //  Dirty set from the classifier — one source of truth for "what changed".
  const log = wtlog.open(repo);
  const res = classify.classify(repo, log, k);
  const paths = [];
  for (const r of res.rows) {
    if (r.bucket === "eq" || r.bucket === "ok") continue;   // clean → no diff
    if (prefix && r.path.indexOf(prefix) !== 0) continue;   // DIFF-012: dir scope
    if (underSub(r.path)) continue;           // under a mount → its own recurse
    if (paths.indexOf(r.path) < 0) paths.push(r.path);
  }
  paths.sort();

  for (const p of paths) {
    //  JS-069: a base symlink leaf reads its wt side via readWtLink (lstat/
    //  readlink), NEVER mmap — mmap follows the link and leaks the target file.
    const isLink = F.links[p] !== undefined;
    const fsha = isLink ? F.links[p] : F.files[p];
    if (fsha === undefined) continue;          // no base side → wholly-new, skip
    const wt = isLink ? readWtLink(wtpath(repo.wt, p))
                      : readWtFile(wtpath(repo.wt, p));   // BE-011: classifier path, confined
    if (wt === undefined) {
      //  BASE_ONLY: deleted/missing in wt → base blob vs empty.
      diffFile(p, blobBytes(k, fsha), undefined, false, "", color, out);
      continue;
    }
    //  BOTH: sha-skip on the wt blob sha vs the base entry sha (a no-op edit
    //  that the classifier still bucketed leaves no diff).
    if (frameSha("blob", wt) === fsha) continue;
    diffFile(p, blobBytes(k, fsha), wt, false, "", color, out);
  }

  //  DIFF-012/[Submodules]: recurse mounted subs — each sub's own wt-vs-base
  //  diff, names prefixed under the sub path (suppressed by --nosub).
  const flags = (ctx && ctx.flags) || [];
  if (flags.indexOf("--nosub") >= 0) return;
  for (const sp of Object.keys(F.subs).sort()) {
    let subScope = "";
    if (prefix) {
      if (prefix === sp + "/") subScope = "";
      else if (prefix.indexOf(sp + "/") === 0) subScope = prefix.slice(sp.length + 1);
      else if ((sp + "/").indexOf(prefix) !== 0) continue;   // out of dir scope
    }
    if (!recurse.isMount(repo.wt, sp)) continue;
    let subRepo; try { subRepo = be.treeAt(wtpath(repo.wt, sp)); } catch (e) { continue; }
    const subK = store.open(subRepo.storePath, subRepo.project);
    const subBase = (wtlog.open(subRepo).baselineTip() || {}).sha || "";
    const subTree = subBase ? subK.commitTree(subBase) : null;
    diffWtTree(subK, subTree, subRepo, color, ctx, subScope, prefixingSink(out, sp));
  }
}

//  Read a wt file's bytes, or undefined when absent/unreadable.
//  JS-069: only ever called for a NON-link leaf (symlinks route through
//  readWtLink) — the caller gates by leaf kind so this never follows a link.
function readWtFile(path) {
  try { return io.mmap(path, "r").data().slice(); } catch (e) { return undefined; }
}

//  JS-069: wt-side symlink read — lstat kind "lnk" → readlink; the target
//  STRING bytes ARE the git blob body of a 120000 leaf.  Never mmap (no follow).
//  Returns undefined when absent or not a symlink on disk.
function readWtLink(path) {
  try {
    if (io.lstat(path).kind !== "lnk") return undefined;
    return utf8.Encode(io.readlink(path));
  } catch (e) { return undefined; }
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
  try { subRepo = be.treeAt(pathlib.join(parentRepo.wt, subPath)); } catch (e) { return; }
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
    feed: out.feed ? function (hunkUri, text, toks) {
      //  URI-011: prefix the hunk URI's PATH under the mount via the URI class
      //  (parse once, rebuild) — the same rewrite as the `.split("diff:")` join
      //  it replaces, but scheme/authority/query/#L<n> are preserved structurally.
      //  An empty uri (text-only gitlink hunk) has no `diff:` to rewrite: pass it
      //  through UNCHANGED, exactly as `.split("diff:")` did (byte-parity).
      let pfxed = hunkUri;
      if (hunkUri) {
        const u = uri._parse(hunkUri);
        pfxed = URI.make(u.scheme, u.authority, prefix + "/" + u.path,
                         u.query, u.fragment);
      }
      out.feed(pfxed, text, toks);
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
      //  URI-014: --tlv (bro pager) re-bakes the C `diff:` uri to the word spell
      //  `diff //name/path?v#L<n>`; --plain/--color KEEP scheme-form (the C HUNK
      //  hunk_uri_is_diff render gate needs it — C follow-up), just authority-scoped.
      uri = wantU ? navlib.navRelink(uri) : navlib.navAuthorize(uri);
      const aug = wantU ? withUTarget(uri, text, toks) : { text: text, toks: toks };
      sink.feed(uri, aug.text, aug.toks, "", 0n);
    } : undefined,
  };
}

//  --- JS-071: re-parse the `diff:` URI off the plain arg -----------------
//  Empty be.views ⇒ re-parse the one-shot `diff:<uri>` like log/commit/tree.
//  Resolve a ref (branch-FIRST, then full-sha / hashlet) to
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
  //  URI-011: parse the arg ONCE, then branch on its scheme instead of the old
  //  prepend-`diff:`-then-reparse dance.  Already a `diff:` address → use its
  //  slots directly; a bare/other address → default the `diff:` scheme via
  //  URI.make and read the slots back.  Only path/query/#frag are consumed.
  let u = new URI(String(raw || ""));
  if (u.scheme !== "diff")
    u = new URI(URI.make("diff", u.authority, u.path, u.query, u.fragment) || "diff:");
  //  BE-032: the path slot resolves against the run's CONTEXT DIR (argRel keeps
  //  BE-037's canonical form + NAVESCAPE); the root dir-form `./` is the whole wt.
  let path = discover.argRel(repo, u.path || "");
  if (path === "./") path = "";
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
//  JAB-004: diff ONE arg — self-parse `diff:<uri>`, read be repo/sink/out/flags +
//  ambient.format() PURELY off the global `be` (no ctx param).
function diffOne(arg) {
  const _be = (typeof be !== "undefined") ? be : null;
  const dmode = ambient.format();
  //  JAB-004: be.out is the guarded chunk sink (no-op at the loop edge — output
  //  flows via be.sink.feed); be.sink is the HUNK feed the edge renders.
  const out = diffOut(_be && _be.out, _be && _be.sink, dmode);
  const flags = (_be && _be.flags) || [];     // JAB-004: --nosub off be
  const fctx = { flags: flags };
  const color = dmode === "color";

  const repo = (_be && _be.repo) || be.treeAt();
  const k = store.open(repo.storePath, repo.project);

  //  JS-071: a pinned be.views spec (commit.js / COMMIT-006, keyed by be.uri =
  //  the diff URI) wins; else re-parse `diff:<uri>` off the arg.  DIFF-012: a
  //  no-arg `jab diff` defaults to the whole-wt `diff:` spec (path "").  A normal
  //  CLI `diff:` call has empty be.views, so it always reparses the arg.
  let spec = (_be && _be.views && _be.views[_be.uri]) || null;
  if (!spec) {
    const raw = (arg !== undefined && arg !== null && String(arg).length)
              ? String(arg) : "diff:";
    spec = parseDiffArg(k, repo, raw);
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
      diffTreeRefs(k, fromTree, toTree, navver, color, fctx, repo, "", out);
    }
  } else {
    //  wt-vs-base: baseline tree from the seed-pinned baseline sha.
    const baseSha = spec.baselineSha || "";
    const baseTree = baseSha ? k.commitTree(baseSha) : null;
    const rel = spec.path.replace(/\/+$/, "");
    let dir = spec.path !== "" && spec.path !== rel;        // trailing slash
    if (spec.path && !dir) { try { dir = io.lstat(wtpath(repo.wt, rel)).kind === "dir"; } catch (e) {} }
    if (spec.path && dir) {
      //  DIFF-012: a DIR path scopes the wt diff to that subtree (the classifier
      //  dirty set under `<dir>/`), not a single-file read.
      diffWtTree(k, baseTree, repo, color, fctx, rel + "/", out);
    } else if (spec.path) {
      //  DIFF-011: a file UNDER a mounted sub reads its FROM side from the sub's
      //  own baseline tree (the parent tree has only the gitlink).
      const m = subMountSplit(k, baseTree, repo, spec.path, fctx);
      let fB;
      if (m) {
        const subBase = (wtlog.open(m.subRepo).baselineTip() || {}).sha || "";
        const subTree = subBase ? m.subK.commitTree(subBase) : null;
        fB = blobAtTree(m.subK, subTree, m.rest);
      } else {
        fB = blobAtTree(k, baseTree, spec.path);
      }
      //  JS-069: a symlink on disk reads via readWtLink (lstat/readlink), never
      //  mmap — mmap follows the link and leaks the target file's bytes.
      //  BE-011: wtJoin confines the untrusted spec.path; a `..` climb above the
      //  wt root throws NAVESCAPE — refuse (never a silent outside read).
      let wtAbs;
      try { wtAbs = wtpath(repo.wt, spec.path); }
      catch (e) { io.log("diff: " + e + "\n"); return; }
      let onLink = false;
      try { onLink = io.lstat(wtAbs).kind === "lnk"; } catch (e) {}
      const tB = onLink ? readWtLink(wtAbs) : readWtFile(wtAbs);
      diffFile(spec.path, fB, tB, true, "", color, out);
    } else {
      diffWtTree(k, baseTree, repo, color, fctx, "", out);
    }
  }
}

//  JAB-004: PURE plain-args verb (`.jab="args"`) — loops args reading `be` only;
//  diff recurses in-process (direct calls, NOT {enqueue}) so run() once suffices.
function diff() {
  //  DIFF-013: no-positional `jab diff` scopes to the run's CONTEXT DIR (be.ctxDir()
  //  via discover.ctxSub, ROOTED + dir-form so parseDiffArg's argRel skips the ctx
  //  re-resolve); a subdir cwd/nav diffs that subtree, the wt root (ctxSub "") the
  //  whole wt (the DIFF-012 "" spec).
  let argv = arguments;
  if (!arguments.length) {
    const _be = (typeof be !== "undefined") ? be : null;
    const c = _be && _be.repo ? discover.ctxSub(_be.repo) : "";
    argv = [c ? "/" + c + "/" : ""];
  }
  for (let i = 0; i < argv.length; i++) diffOne(argv[i]);
}
diff.jab = "args";
module.exports = diff;

//  --- DIFF-011: mount-aware scoped-path resolution ---------------------
//  `diff:<sub>/<file>` (a file UNDER a mounted submodule) must read its
//  from/baseline side from the SUB's OWN shard, not the parent tree — the
//  parent tree has only a `160000` gitlink at `<sub>`, which blobAtTree can't
//  descend (→ undefined → a false WHOLLY-ADDED).  This mirrors recurseSubPins
//  (the recursive whole-tree path): enumerate the parent tree's gitlinks, gate
//  each on recurse.isMount, open the sub via be.treeAt/store.open.
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
  try { subRepo = be.treeAt(wtpath(repo.wt, best)); } catch (e) { return null; }
  let subK;
  try { subK = store.open(subRepo.storePath, subRepo.project); }
  catch (e) { return null; }
  const rest = path.slice(best.length + 1);
  //  DIFF-014: NESTED mount — `rest` may itself lie under a mount inside the
  //  sub (dog/abc/FILE.h: abc is a gitlink in dog's tree too).  Re-split on the
  //  sub's OWN baseline tree so the from-side reads from the DEEPEST sub that
  //  directly holds the file; same routes (recurse.isMount/be.treeAt/store.open).
  const subBase = (wtlog.open(subRepo).baselineTip() || {}).sha || "";
  const subTree = subBase ? subK.commitTree(subBase) : null;
  const deeper = subTree ? subMountSplit(subK, subTree, subRepo, rest, ctx) : null;
  if (deeper) return deeper;
  return { subK: subK, subRepo: subRepo, subPath: best,
           rest: rest, oldPin: subs[best] };
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
//  JS-069: repro hooks — treeMap (leaf routing) + readWtLink (no-follow wt read).
module.exports.treeMap = treeMap;
module.exports.readWtLink = readWtLink;
module.exports.readWtFile = readWtFile;
