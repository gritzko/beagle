//  views/log/log.js — the `log:` read-only VIEW as a resident-loop handler
//  (JAB-013).  Emits commit history newest-first, ONE row per commit:
//      <sha8> <date7><summary> (<author>)\n
//  accumulated into ONE content HUNK with exactly ONE trailing blank line for
//  the WHOLE log (HUNKu8sFeedText appends the single end separator).  Pure JS
//  over the libabc/libdog bindings — NO dog binary, NO /proc.  Mirrors
//  graf/LOG.c (GRAFLog / graflog_render_commit / graflog_branch / graflog_file
//  / GRAFResolveTip / graflog_count_from_frag).
//
//  handle(row, ctx): the seed lowered `log:<uri>` into ctx.args[0] (the loop's
//  one-shot scheme:uri form — see core/loop.js); the handler re-parses the URI
//  (a `?ref` form the generic seed mis-reads as a ref-op still fires the "."
//  placeholder row, so we read ctx.args, never the queue uri).  Output is a
//  CONTENT HUNK fed to ctx.sink (uri "log:<path>?<ref>" banner, per-column
//  tok32 spans for --color); the loop edge renders it plain/color/tlv via
//  view/bro.js renderHunkLog — the view never writes fd 1.

"use strict";

const store  = require("../../shared/store.js");
const dag    = require("../../shared/dag.js");
const wtlog  = require("../../shared/wtlog.js");
const resolve = require("../../core/resolve.js");
const shalib = require("../../shared/util/sha.js");
const isFullSha = shalib.isFullSha;

const LOG_MAX_WALK = 1 << 20;   // GRAF LOG_MAX_WALK cyclic-DAG bound

//  tok32 tag indices (A=0 … Z=25), the dog/THEME palette the HUNK .color sink
//  paints: L (hashlet) = bright cyan, G (string/sep) = green, S (default) =
//  none, D (comment) = gray.  Verified against `be log: --color` (each row:
//  sha8=L sep=G date7=L sep=G summary=S " (author)"=D).
const TAG_L = 11, TAG_G = 6, TAG_S = 18, TAG_D = 3;
function tok(tag, end) { return ((tag & 0x1f) << 27) | (end & 0xffffff); }

//  --- URI parse: pull the path / ?ref / #frag off the raw `log:<uri>` arg ---
//  ./path = FILE history (graflog_strip_dotslash); ?query = branch ref tip;
//  #frag = a tip-sha (hashlet) OR a `#N` walk cap.
function parseArg(raw) {
  let s = String(raw || "");
  if (s.indexOf("log:") === 0) s = s.slice(4);
  const u = new URI("log:" + s);   // re-scheme so URI splits path/query/frag
  let path = u.path || "";
  if (path.indexOf("./") === 0) path = path.slice(2);   // strip the ./ lead
  if (path === ".") path = "";   // the loop's no-arg "." scope = whole-repo log
  //  A `?` (even with an EMPTY query) means an explicit ref resolution
  //  (`log:?` = REFSResolve trunk), distinct from bare `log:` (= cur tip).
  //  URI collapses both to query "" — recover the distinction from the raw.
  const hasQuery = s.indexOf("?") >= 0;
  return { path: path, query: u.query || "", hasQuery: hasQuery,
           frag: u.fragment || "" };
}

//  graflog_count_from_frag (HUNKu8sFragSplit twin): trailing digits, an
//  optional leading `L`, value >= 1 → the cap N.  `#0` / `#L0` / a non-numeric
//  (hashlet) frag / no frag ⇒ 0 (NO cap — and, when the frag is non-empty, it
//  is then a TIP HASHLET, not a count: VERIFIED `be log:#0` / `#L0` resolve
//  "0"/"L0" as a tip and emit nothing, while `#5` caps cur tip to 5).
function countFromFrag(frag) {
  if (!frag) return 0;
  const m = /^L?(\d+)$/.exec(frag);
  const n = m ? parseInt(m[1], 10) : 0;
  return n >= 1 ? n : 0;
}

//  Is `frag` a `#N` walk-cap (N >= 1) rather than a tip hashlet?
function isCountFrag(frag) {
  return countFromFrag(frag) >= 1;
}

//  GRAFResolveTip: with an explicit `?<ref>` the tip is REFSResolve(ref); else
//  a non-count `#frag` (a real hashlet, or `#0`/`#L0`) resolves a tip sha
//  (40-hex / hashlet / commit-prefix); a `#N` count or no frag falls to the
//  cur tip (the --at HEAD).  Unresolvable → undefined (the handler emits
//  NOTHING — not even the banner — matching `be`).
function resolveTip(k, repo, parsed) {
  const frag = parsed.frag;
  if (parsed.hasQuery) {
    //  Explicit `?<ref>` (incl. `?` = trunk): REFSResolve, then a hashlet
    //  retry for a `?<hashlet-branch>` form.  The frag (if any) is a `#N` cap.
    let sha = k.resolveRef(parsed.query);
    if (!sha) sha = resolveHashlet(k, parsed.query);
    return sha;
  }
  //  No query: a non-count `#frag` is a TIP hashlet/sha (incl. `#0`/`#L0`,
  //  which won't resolve → empty); a `#N` cap or no frag → cur tip.
  if (frag && !isCountFrag(frag)) {
    if (isFullSha(frag)) return k.getObject(frag) ? frag : undefined;
    return resolveHashlet(k, frag);
  }
  //  Bare `log:` / `log:#N`: cur tip (the --at HEAD, the wtlog curTip).
  const cur = wtlog.open(repo).curTip();
  return (cur && cur.sha) || undefined;
}

//  KEEPResolveHex twin: resolve a short hashlet to a full COMMIT sha.  First
//  the cheap tip/remote prefix scan (resolve.resolveHex); on a miss, scan the
//  store's pack commits and prefix-match the full sha — the C resolves a
//  hashlet against any object, not only tips (a `log:#<commit-hashlet>` where
//  the commit is mid-history, not a branch tip).  An AMBIGUOUS prefix (more
//  than one matching commit) resolves to NOTHING, matching KEEPResolveHex
//  (this is why `be log:#0` — prefix "0" hits 2 commits — emits nothing).
//  Bounded by the pack object count; commits only (a log tip is a commit).
function resolveHashlet(k, hexish) {
  const tip = resolve.resolveHex(k, hexish);            // tips + full-sha fast path
  if (tip) return tip;
  if (!/^[0-9a-f]{1,40}$/.test(hexish)) return undefined;
  let packs;
  try { packs = k._packs(); } catch (e) { return undefined; }
  let hit;                                              // unique commit match
  for (let fi = 0; fi < packs.length; fi++) {
    let pk;
    try { pk = git.pack.mmap(packs[fi].path, "r"); pk.buffer.watermark = pk.byteLength; }
    catch (e) { continue; }
    pk.rewind();
    const offs = [];
    while (pk.next()) offs.push(pk.offset);
    for (const off of offs) {
      pk.seek(off);
      if (pk.type === "ref-delta") continue;
      let bytes;
      try { const o = io.buf((pk.size || 0) * 4 + 256); pk.seek(off); pk.resolve(o); bytes = o.data(); }
      catch (e) { continue; }
      if (pk.type !== "commit") continue;
      const full = shalib.frameSha("commit", bytes);
      if (full.indexOf(hexish) === 0) {
        if (hit && hit !== full) return undefined;       // ambiguous → unresolvable
        hit = full;
      }
    }
  }
  if (hit) return hit;
  return undefined;
}

//  First line of a commit body: skip a leading CR/LF run, take up to the next
//  CR/LF (graflog_render_commit LOG.c:268-275 — NO 64-clip, NO tab stop; that
//  clip is the status `#subject` tail, a different context).
function firstLine(body) {
  if (!body) return "";
  let i = 0;
  while (i < body.length && (body[i] === "\n" || body[i] === "\r")) i++;
  let j = i;
  while (j < body.length && body[j] !== "\n" && body[j] !== "\r") j++;
  return body.slice(i, j);
}

//  graflog_parse_author: the author NAME only — strip ` <email> <ts> <tz>`.
//  Cut at the first " <" (the email open); fall back to the whole string.
function authorName(author) {
  const a = author || "";
  const lt = a.indexOf(" <");
  return lt >= 0 ? a.slice(0, lt) : a;
}

//  --- branch history: graflog_branch ------------------------------------
//  First-parent walk from the tip, newest-first, bounded by the walk cap +
//  the `#N` count.  commitParents (DAG) gives the parent list; on a DAG-miss
//  (commitParents empty — a non-commit / unreadable object) parse the body's
//  `parent <40hex>` lines (LOG.c:346-363).
function branchHistory(k, tip, cap) {
  const shas = [];
  const seen = new Set();
  let sha = tip;
  for (let n = 0; n < LOG_MAX_WALK; n++) {
    if (!isFullSha(sha) || seen.has(sha)) break;
    const pc = k.parseCommit(sha);
    if (!pc) break;                       // missing/non-commit → walk breaks clean
    seen.add(sha);
    shas.push(sha);
    if (cap && shas.length >= cap) break;
    let parents = k.commitParents(sha);
    if (!parents || !parents.length) parents = parentsFromBody(pc.body);
    const first = mainlineParent(k, parents);
    if (!first) break;                    // root commit → stop
    sha = first;
  }
  return shas;
}

//  The mainline "first parent" graf follows at a merge: the parent with the
//  NEWEST commit timestamp (ties → earliest listed).  VERIFIED across two
//  merges with OPPOSITE list orders: at [d6ddc6ca(older), 68180f4e(newer)]
//  `be` follows 68180f4e; at [2e420a6b(newer), bb77c105(older)] `be` follows
//  2e420a6b — i.e. argmax(commitTs), the recency simplification (the same
//  newest-first rule dag.js::aheadBehind uses), NOT git's positional
//  parents[0].  A single-parent commit returns it directly.
function mainlineParent(k, parents) {
  if (!parents || !parents.length) return undefined;
  if (parents.length === 1) return isFullSha(parents[0]) ? parents[0] : undefined;
  let best, bestTs = -1n;
  for (const p of parents) {
    if (!isFullSha(p)) continue;
    const ts = dag.commitTs(k, p);
    if (best === undefined || ts > bestTs) { best = p; bestTs = ts; }
  }
  return best;
}

//  Parse every `parent <40hex>` line out of a commit header (the DAG-miss
//  fallback; defensive twin of the C body scan — fires only when commitParents
//  is empty).  Returns them in header order for mainlineParent to rank.
function parentsFromBody(body) {
  const out = [];
  if (!body) return out;
  const re = /(?:^|\n)parent ([0-9a-f]{40})/g;
  let m;
  while ((m = re.exec(body))) out.push(m[1]);
  return out;
}

//  --- file history: graflog_file ----------------------------------------
//  DAGAncestors closure + topo-sort (parents-before-children), keep a commit
//  iff its leaf sha at `path` DIFFERS from EVERY parent's (presence-flips
//  count; a root is kept iff present).  Emit newest-first, bounded by `#N`.
function fileHistory(k, tip, path, cap) {
  const ancSet = dag.ancestors(k, tip);
  if (!ancSet.size) return [];
  const order = dag.topoSort(k, ancSet);      // parents-before-children
  //  Memoised leaf sha at `path` per commit (its TREE descent).
  const leafCache = new Map();
  function leafAt(sha) {
    if (leafCache.has(sha)) return leafCache.get(sha);
    const treeSha = k.commitTree(sha);
    const leaf = treeSha ? leafSha(k, treeSha, path) : undefined;
    leafCache.set(sha, leaf);
    return leaf;
  }
  const kept = [];
  for (const sha of order) {
    const mine = leafAt(sha);
    const parents = (k.commitParents(sha) || []).filter(function (p) {
      return ancSet.has(p);
    });
    let keep;
    if (!parents.length) {
      keep = mine !== undefined;               // root: kept iff present
    } else {
      //  Default git-log simplification: kept iff DIFFERS from EVERY parent.
      keep = parents.every(function (p) { return leafAt(p) !== mine; });
    }
    if (keep) kept.push(sha);
  }
  //  Emit newest-first.  topoSort already gives parents-before-children
  //  (oldest-first), so a reverse is newest-first AND topologically valid; a
  //  final commit-ts sort (newest-first, the same key dag.aheadBehind uses)
  //  pins sibling order to `be`'s time order regardless of DFS discovery.
  kept.reverse();
  kept.sort(function (a, b) {
    const ta = dag.commitTs(k, a), tb = dag.commitTs(k, b);
    return ta === tb ? 0 : (ta > tb ? -1 : 1);
  });
  return cap ? kept.slice(0, cap) : kept;
}

//  The blob/leaf sha for `path` inside a tree (descend by path segments) —
//  the store.js readTree walk to the leaf.  undefined when the path is absent.
function leafSha(k, treeSha, path) {
  const segs = path.split("/");
  let cur = treeSha;
  for (let i = 0; i < segs.length; i++) {
    const ents = k.readTree(cur);
    if (!ents) return undefined;
    let hit;
    for (const e of ents) if (e.name === segs[i]) { hit = e; break; }
    if (!hit) return undefined;
    if (i === segs.length - 1) return hit.sha;
    cur = hit.sha;
  }
  return undefined;
}

//  --- row render (graflog_render_commit + graflog_pack) -----------------
//  Append one commit's row bytes + its tok32 spans to the accumulators.
//  Row = sha8 + " " + date7 + " " + summary + " (" + author + ")" + "\n".
//  Columns (VERIFIED byte-for-byte vs `be log:`): sha8 (8) + a SEP space +
//  date7 (the 7-col ron.date, its own leading/trailing space) + a SEP space +
//  the first-line summary + " (" + author-name + ")".
//  Spans (--color, VERIFIED vs `be log: --color`): sha8=L(cyan), sep=G(green),
//  date7=L(cyan), sep=G(green), summary=S(default), " (author)"=D(gray).  A
//  final S-tagged span covers the row's "\n" so the next row's cyan sha8 span
//  does NOT bleed onto this line's terminator (the renderHunkLog binding emits
//  a default-fg reset at a default-tag span; the C resets fully to ESC[0m at
//  each line — a residual binding-level colour delta on the line terminator,
//  shared by every content view, see the report).
function appendRow(sha, k, textParts, spans, baseOff) {
  const sha8 = sha.slice(0, 8);
  const date7 = ron.date(dag.commitTs(k, sha));        // 7-col; ts<=0 → "   ?   "
  const pc = k.parseCommit(sha);
  const summary = firstLine(pc ? pc.body : "");
  const author = authorName(pc ? pc.author : "");
  const authTail = " (" + author + ")";
  const line = sha8 + " " + date7 + " " + summary + authTail + "\n";
  const bytes = utf8.Encode(line);
  textParts.push(bytes);

  //  Byte ends of each column (ASCII sha8/date7/sep; summary/author may be
  //  multibyte — measure with utf8.Encode).  Shift every end by baseOff so the
  //  spans address the WHOLE accumulated hunk body.
  const eSha8 = 8;                                      // [0,8)   sha8
  const eSep1 = eSha8 + 1;                              // [8,9)   sep " "
  const eDate = eSep1 + utf8.Encode(date7).length;      // [9,16)  date7 (7 cols)
  const eSep2 = eDate + 1;                              // [16,17) sep " "
  const eSumm = eSep2 + utf8.Encode(summary).length;    // summary
  const eAuth = eSumm + utf8.Encode(authTail).length;   // " (author)"
  const eNL   = bytes.length;                           // incl the "\n"
  spans.push([TAG_L, baseOff + eSha8]);                 // sha8
  spans.push([TAG_G, baseOff + eSep1]);                 // sep
  spans.push([TAG_L, baseOff + eDate]);                 // date7
  spans.push([TAG_G, baseOff + eSep2]);                 // sep
  spans.push([TAG_S, baseOff + eSumm]);                 // summary
  spans.push([TAG_D, baseOff + eAuth]);                 // " (author)"
  spans.push([TAG_S, baseOff + eNL]);                   // "\n" → no colour bleed
  return bytes.length;
}

//  --- the handler -------------------------------------------------------
module.exports = function handle(row, ctx) {
  const sink = ctx && ctx.sink;
  if (!sink) return;
  const repo = (ctx && ctx.repo) || null;
  if (!repo) return;

  //  The full `log:<uri>` rides ctx.args[0] (the one-shot scheme:uri lowering);
  //  a fragment-only seed can't survive the queue, so never trust row.uri.
  const rawArgs = (ctx && ctx.args && ctx.args.length) ? ctx.args : [row.uri];
  let first = String(rawArgs[0] || "");
  if (first.indexOf("log:") !== 0) first = "log:" + first;
  const parsed = parseArg(first);

  const k = store.open(repo.storePath, repo.project);
  const cap = countFromFrag(parsed.frag);
  const tip = resolveTip(k, repo, parsed);

  //  UNRESOLVABLE tip (a missing `?<ref>`, a `#0`/`#L0`/`#<bad-hashlet>` that
  //  resolves nothing) → GRAFResolveTip fails → NO hunk at all (not even the
  //  banner).  VERIFIED: `be log:?nonexistent` / `be log:#0` emit ZERO bytes.
  //  A RESOLVED tip with an empty walk (e.g. `log:./absent` file history) still
  //  emits the banner + the single trailing blank line.
  if (!tip || !isFullSha(tip)) return;

  //  The banner uri: `log:` + path + `?query` (the GRAFLog title shape).  The
  //  fragment (#N / #hashlet) is NOT part of the title.
  let bannerUri = "log:" + parsed.path;
  if (parsed.query) bannerUri += "?" + parsed.query;

  //  The history walk (newest-first, bounded by `#N`).
  const shas = parsed.path ? fileHistory(k, tip, parsed.path, cap)
                           : branchHistory(k, tip, cap);

  //  Accumulate every row into ONE hunk body + its tok32 spans.
  const textParts = [];
  const spans = [];
  let off = 0;
  for (const sha of shas) off += appendRow(sha, k, textParts, spans, off);

  //  Concatenate the row bytes.
  const body = concat(textParts, off);
  const toks = new Uint32Array(spans.length);
  for (let i = 0; i < spans.length; i++) toks[i] = tok(spans[i][0], spans[i][1]);

  //  ONE feed: verb "" → the bare `log:<path>?<ref>` banner (no verb word),
  //  the accumulated rows, the per-column tok32 spans.  HUNKu8sFeedText adds
  //  the SINGLE trailing blank line for the whole log.
  sink.feed(bannerUri, body, toks, "", 0n);
};

//  Concatenate a list of Uint8Array chunks into one buffer of length `total`.
function concat(parts, total) {
  const all = new Uint8Array(total);
  let off = 0;
  for (const p of parts) { all.set(p, off); off += p.length; }
  return all;
}
