//  WHY-001 why.js — the `why:<path>` read-only blame VIEW.  STEPS the file weave
//  (shared/weave.js reconstruction, shared with patch.js) and emits, per
//  ORIGIN-commit run, a background-shaded span (hue=f(inserter sha), view/bro.js
//  colorWhyHunk) carrying a `commit ?<hashlet>` U-target.  `?<rev>` blames as of a
//  rev; `?<a>..<b>` colours ONLY that range's changes (incl deletes via `rms`).
//  Presentation over the EXISTING weave — NOT a new engine; not emitDiff/emitFull.

"use strict";

const store     = require("../../shared/store.js");
const wtlog     = require("../../shared/wtlog.js");
const shalib    = require("../../shared/util/sha.js");
const weave     = require("../../shared/weave.js");
const dag       = require("../../shared/dag.js");
const navlib    = require("../../shared/nav.js");
//  BE-030: worktree fs paths go THROUGH resolve() (context-confined wtpath).
const discover = require("../../core/discover.js");
const wtpath = discover.wtpath;
const ambient   = require("../../shared/ambient.js");
const isFullSha = shalib.isFullSha;

//  WHY-001: synthetic weave id for the WORKING-TREE layer — NOT a real commit, so
//  it's absent from idToSha and its tokens render PLAIN (uncommitted, unattributed).
const WT_ID = "ffffffffffffffff";
//  WHY-001: working-tree file bytes (diff.js readWtFile twin), or undefined.
function readWtFile(path) {
  try { return io.mmap(path, "r").data().slice(); } catch (e) { return undefined; }
}

//  WHY-001 tok32 here is JUST tag(5)|end(24) — the origin COLOUR+CLICK ride a
//  HIDDEN `O` (origin) token (a `U` sibling), never tok bits, so a `why` hunk
//  can't trip the diff-side wash.  Each washed token is `[visible][O]`; O bytes =
//  `#rrggbb commit ?<hashlet>` — a LEADING baked bg (the VIEW owns the colour: hue
//  f(sha) + age paleness) then the click spell.  The renderer just applies the
//  `#rrggbb`; the pager strips it at the first space → `commit ?<hashlet>`.
//  TAG_O = 'O'-'A' = 14 (hidden like TAG_U=20); TAG_S=18 the default.
const TAG_S = 18, TAG_U = 20, TAG_O = 14;
//  12-hex hashlet click target (commit: resolves any 6..40, abc.mkd).
const HASHLET = 12;
function tok(tag, end) { return ((tag & 0x1f) << 27) | (end & 0xffffff); }

//  WHY-001: per-commit HUE — 12 vivid xterm-cube directions picked by the sha —
//  blended toward white by the age shade (0 newest .. 255 oldest) so older=paler,
//  baked to an explicit `rrggbb` the renderer applies verbatim (Design (b): the
//  VIEW resolves the bg).  cube8: xterm cube level 0..5 → its exact 8-bit value.
const WHY_HUES = [[5,0,0],[5,3,0],[5,5,0],[3,5,0],[0,5,0],[0,5,3],
                  [0,5,5],[0,3,5],[0,0,5],[3,0,5],[5,0,5],[5,0,3]];
function cube8(l) { return l <= 0 ? 0 : 55 + 40 * l; }
function hex2(n) { return (n < 16 ? "0" : "") + (n & 0xff).toString(16); }
function whyRgb(sha, shade) {
  let h = 0;
  for (let i = 0; i < 12 && i < sha.length; i++) h = (h * 31 + sha.charCodeAt(i)) >>> 0;
  const c = WHY_HUES[h % WHY_HUES.length];
  //  readable band: newest ~0.3 (saturated pastel) .. oldest ~0.8 (faint tint,
  //  never pure white) so even the oldest keeps a visible wash under the fg.
  const p = 0.3 + 0.5 * (Math.max(0, Math.min(255, shade | 0)) / 255);
  const R = Math.round(c[0] + p * (5 - c[0])), G = Math.round(c[1] + p * (5 - c[1])),
        B = Math.round(c[2] + p * (5 - c[2]));
  return hex2(cube8(R)) + hex2(cube8(G)) + hex2(cube8(B));
}

//  WHY-001: resolve a ref (branch FIRST, then full-sha / hashlet) to a commit
//  sha (diff.js:resolveCommit twin — reuse the convention, never hand-parse).
function resolveCommit(k, ref) {
  if (!ref) return undefined;
  const byRef = k.resolveRef(ref);
  if (byRef && isFullSha(byRef)) return byRef;
  if (isFullSha(ref)) return k.getObject(ref) ? ref : undefined;
  if (/^[0-9a-f]{1,39}$/.test(ref)) return k.resolveHexAny(ref);
  return undefined;
}

//  WHY-001: parse `why:<path>?<query>` — query "" / branch / <rev> / <a>..<b>.
//  Reuse the diff: `?<a>..<b>` split; a bare/no query blames the current tip.
//  Returns { path, tip, from, to } | null (unresolvable rev).  from/to set only
//  for a range (colour ONLY (from,to] changes).
function parseArg(k, repo, raw) {
  let u = new URI(String(raw || ""));
  if (u.scheme !== "why")
    u = new URI(URI.make("why", u.authority, u.path, u.query, u.fragment) || "why:");
  //  BE-032: the path slot resolves against the run's CONTEXT dir (rooted `/x`
  //  = wt root), re-anchored wt-root-relative; NAVESCAPE on climb-out.
  const path = discover.argRel(repo, u.path || "");
  const query = u.query || "";
  const dots = query.indexOf("..");
  if (dots > 0 && dots < query.length - 2) {
    const from = resolveCommit(k, query.slice(0, dots));
    const to = resolveCommit(k, query.slice(dots + 2));
    if (!from || !to) return null;
    return { path: path, tip: to, from: from, to: to };
  }
  if (query) {
    const tip = resolveCommit(k, query);
    if (!tip) return null;
    return { path: path, tip: tip, from: undefined, to: undefined };
  }
  //  WHY-001: no query → blame the WORKING TREE (diff:'s wt-vs-base twin): the
  //  committed base is the BASELINE tip, then whyOne folds the wt content on top.
  const base = (wtlog.open(repo).baselineTip() || {}).sha || "";
  return { path: path, tip: base, from: undefined, to: undefined, wt: true };
}

//  WHY-001: the reachable-commit closure of `tip` (weaveId hashlets) — the
//  membership a `?<a>..<b>` range filters on ("changed in (from,to]").
function closureIds(k, tip) {
  const ids = Object.create(null);
  const seen = Object.create(null);
  const stack = [tip];
  while (stack.length) {
    const sha = stack.pop();
    if (!sha || seen[sha]) continue;
    seen[sha] = true;
    ids[weave.weaveId(sha)] = true;
    let parents;
    try { parents = k.commitParents(sha); } catch (e) { parents = undefined; }
    for (const p of (parents || [])) stack.push(p);
  }
  return ids;
}

//  WHY-001: per-commit log-age SHADE (0 newest .. 255 oldest) over the file's own
//  commit-time span (dag.identEpoch); the render blends the sha-hue toward white
//  by shade/255 — the older the commit, the paler its wash.
function ageShade(k, idToSha) {
  const time = Object.create(null);
  let tMin = Infinity, tMax = -Infinity;
  for (const id in idToSha) {
    let t = 0;
    try { const pc = k.parseCommit(idToSha[id]); t = dag.identEpoch(pc && (pc.author || pc.committer) || ""); }
    catch (e) { t = 0; }
    time[id] = t;
    if (t > 0) { if (t < tMin) tMin = t; if (t > tMax) tMax = t; }
  }
  const span = (tMax > tMin) ? (tMax - tMin) : 1;
  const lg = Math.log(1 + span);
  const shade = Object.create(null);
  for (const id in time) {
    const age = time[id] > 0 ? (tMax - time[id]) : span;   // unknown time → oldest
    const p = lg > 0 ? Math.log(1 + age) / lg : 0;         // 0 newest .. 1 oldest
    shade[id] = Math.max(0, Math.min(255, Math.round(p * 255)));
  }
  return shade;
}

//  WHY-001: the hidden `O` spell per commit id — `#rrggbb commit ?<hashlet>`: a
//  LEADING baked bg (hue f(sha) + age paleness), then the click spell.  The pager
//  strips the `#rrggbb ` prefix at the first space → `commit ?<hashlet>`.
function originTargets(idToSha, shade) {
  const o = Object.create(null);
  for (const id in idToSha) {
    const sha = idToSha[id];
    if (!sha) continue;
    const hashlet = sha.slice(0, HASHLET);
    o[id] = "#" + whyRgb(hashlet, shade[id] | 0) + " " +
            navlib.navLink("commit", "", hashlet, undefined);
  }
  return o;
}

//  WHY-001: STREAM the body straight into a HUNK buffer (io.ram, lazy mmap — NO
//  per-token JS objects/slices).  Each ORIGIN-attributed token is emitted as its
//  visible bytes + a hidden `O` token holding oTarget[id] (`#rrggbb commit ?<hashlet>`);
//  the renderer applies the leading #rgb, the pager strips it for the click.  A token with NO in-scope
//  origin — working-tree/uncommitted, or out-of-range base in a `?a..b` hunk —
//  gets NO O → renders white.  Returns { body, toks, commits } (buffer views).
function buildBody(w, idToSha, rangeIds, oTarget) {
  const commits = w.commits;                 // index -> 16-hex hashlet
  oTarget = oTarget || Object.create(null);
  const body = io.ram(64 << 20);             // only touched pages fault in
  let toks = new Uint32Array(4096), nt = 0;
  function pushTok(t) {
    if (nt >= toks.length) { const g = new Uint32Array(toks.length * 2); g.set(toks); toks = g; }
    toks[nt++] = t;
  }
  //  A commit index is in the (from,to] range iff its hashlet is in rangeIds.
  function inRange(idx) { if (!rangeIds) return true; const h = commits[idx]; return !!(h && rangeIds[h]); }
  const commitSet = Object.create(null);
  let off = 0;

  w.rewind();
  while (w.next()) {
    const txt = w.tokText;                    // Uint8Array subarray view (transient)
    if (!txt || txt.length === 0) continue;
    const rms = w.rms || [];
    const alive = rms.length === 0;
    let id = "";
    if (alive) {
      const ins = w.hasIn ? w.inserter : 0;   // live token: its inserter commit
      if (inRange(ins)) id = commits[ins] || "";
    } else if (rangeIds) {
      //  A REMOVED token: surface it (delete stays visible) iff a remover is in range.
      let rm = -1;
      for (let i = 0; i < rms.length; i++) if (rangeIds[commits[rms[i]]]) { rm = rms[i]; break; }
      if (rm < 0) continue;                   // removed outside the range → gone
      id = commits[rm] || "";
    } else {
      continue;                               // whole-file blame shows only alive
    }
    const o = (id && oTarget[id]) ? oTarget[id] : null;   // null → no origin → white
    body.feed(txt); off += txt.length;
    //  the token's own syntax tag ('A'+idx from w.tag) → the fg; O carries the bg.
    const tagIdx = (typeof w.tag === "number" ? (w.tag - 65) : TAG_S) & 0x1f;
    pushTok(tok(tagIdx, off));
    if (o) {
      commitSet[id] = true;
      off += body.feedStr(o);                 // utf8 straight into the buffer (no JS array)
      pushTok(tok(TAG_O, off));
    }
  }
  return { body: body.data(), toks: toks.subarray(0, nt), commits: Object.keys(commitSet) };
}

//  WHY-001: blame ONE `why:<path>` arg — parse, reconstruct the file weave as of
//  the tip, step it into a shaded+U body, feed ONE content hunk to be.sink.
function whyOne(arg) {
  const _be = (typeof be !== "undefined") ? be : null;
  const sink = _be && _be.sink;
  const repo = (_be && _be.repo) || be.treeAt();
  if (!sink || !repo) return;

  let first = String(arg || "");
  if (first.indexOf("why:") !== 0) first = "why:" + first;
  const k = store.open(repo.storePath, repo.project);
  const spec = parseArg(k, repo, first);
  if (!spec || !spec.path) return;            // no path / unresolvable rev → nothing

  const built = (spec.tip && isFullSha(spec.tip))
    ? weave.build(k, spec.path, spec.tip)
    : { weave: undefined, idToSha: Object.create(null) };

  //  WHY-001: wt mode (no query) — fold the WORKING-TREE content as the top layer
  //  (WT_ID) over the committed weave, so blame reflects UNCOMMITTED changes: the
  //  displayed text is the wt file, committed tokens keep their commit hue, new/
  //  edited (incl. a wholly-new file) tokens render plain.  Twin of diff:'s wt-vs-base.
  let w = built.weave;
  if (spec.wt && repo.wt) {
    //  BE-011: wtJoin confines spec.path to the wt root; an untrusted `..` climb
    //  throws NAVESCAPE — refuse (never fold a silent outside read into blame).
    let full;
    try { full = wtpath(repo.wt, spec.path); }
    catch (e) { io.log("why: " + e + "\n"); return; }
    const wtBytes = readWtFile(full);
    if (wtBytes !== undefined && wtBytes.length <= weave.MAX_SOURCE_SIZE)
      w = weave.fold(w || null, wtBytes, weave.extOf(spec.path), WT_ID);
  }
  if (!w) return;                             // no history AND no wt bytes → nothing

  //  Range: the commits changed in (from,to] = tip's closure minus from's.
  let rangeIds = null;
  if (spec.from) {
    const tipIds = closureIds(k, spec.tip);
    const fromIds = closureIds(k, spec.from);
    rangeIds = Object.create(null);
    for (const h in tipIds) if (!fromIds[h]) rangeIds[h] = true;
  }

  const oTarget = originTargets(built.idToSha, ageShade(k, built.idToSha));
  const body = buildBody(w, built.idToSha, rangeIds, oTarget);
  //  URI-011: the banner URI carries the nav authority (navAuthorize twin path).
  const banner = navlib.navUri("why", spec.path, spec.from ? spec.from.slice(0, 12) + ".." + spec.to.slice(0, 12) : undefined, undefined);
  sink.feed(banner || ("why:" + spec.path), body.body, body.toks, "", 0n);
}

//  DIS-061: a bare `why` (no file operand) blames the pager's CURRENT file — the
//  ambient `be.prev_uri` the pager stashes for a single-hunk FILE view (already
//  normalized to a typed-arg-shaped URI).  An empty stash keeps today's no-arg
//  no-op.  The file-vs-dir distinction lives HERE, not in the pager.
function whyArgs(args) {
  if (args.length) return args;
  const prev = (typeof be !== "undefined" && be && be.prev_uri) || "";
  return prev ? [prev] : [];
}

//  WHY-001: PLAIN-args verb (registry contract) — loop args off `be`.
function why() {
  const args = whyArgs(Array.prototype.slice.call(arguments));
  for (let i = 0; i < args.length; i++) whyOne(args[i]);
}
why.jab = "args";
module.exports = why;

//  WHY-001: repro hooks (the commit/links test pattern) — the golden reaches the
//  body builder + tok packer without a full loop drive.
module.exports.buildBody = buildBody;
module.exports.tok = tok;
module.exports.parseArg = parseArg;
module.exports.whyRgb = whyRgb;   // WHY-001: baked-colour hook (hue f(sha)+age paleness)
module.exports.whyArgs = whyArgs; // DIS-061: no-arg → be.prev_uri fallback (test hook)
