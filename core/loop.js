//  JSQUE-002: the resident dispatch loop.  ONE long-running process pulls
//  `<verb> <uri>` rows off an IN-MEMORY work queue and dispatches each to a
//  resident handler via O(1) registry lookup; a handler may enqueue child
//  rows (fan-out, consume-while-append).  Replaces fork-per-verb: the JSC
//  arena + require cache are paid ONCE for the whole run.  See JSQUE-001/003.
"use strict";

const registry = require("core/registry.js");
//  JAB-004: the shared tokenizer — cli() coerces each already-split CLI argv
//  token via argline.scalar() (the shape-2 rule) before PLAIN dispatch.
const argline = require("core/../shared/argline.js");
//  BE-032: dirname for the context-dir climb (a FILE context → its dir).
const pathlib = require("core/../shared/util/path.js");
//  JSQUE-008: the integration seam — the real seed (resolution-at-entry) and
//  emit sink (output-as-ULog) replace the JSQUE-002 stubs in the CLI entry.
//  JSQUE-016: the entry shim (be/main.js) requires this, so argv[1] is the
//  shim path; _here is the be/ ROOT (where core/ + shared/ live).
const _self = process.argv[1];
const _here = _self.slice(0, _self.lastIndexOf("/"));
const emit = require("core/emit.js");
//  JAB-004: the discover module stays intact + required; its API is folded onto
//  the unified global `be` at loop entry (mintBe), and loop.js reads that global.
const discover = require(_here + "/core/discover.js");
//  DIS-060: the shape-(2) `<scheme>:uri` gate consults THIS projector+transport
//  allowlist, NOT _isVerb — a VERB is not a SCHEME ([Nav]); mutation verbs absent.
const SCHEME_ALLOW = (function () {
  //  DIS-060: transports + the two viewers (bro/help) that help.js's SCHEMES
  //  table omits but which ARE dispatchable schemes.
  const s = new Set(["ssh", "https", "http", "git", "be", "file", "keeper",
                     "bro", "help"]);
  const SCHEMES = require(_here + "/views/help/help.js").SCHEMES || [];
  for (const p of SCHEMES) {
    const k = String(p[0]);
    const c = k.indexOf(":");
    if (c > 0) s.add(k.slice(0, c));       // "commit:<rev>" -> "commit"
  }
  return s;
})();
//  JAB-029: the edge render — cli() turns the collected HUNK sink into fd-1
//  bytes via bro.renderHunkLog (the ONE place mode plain/color/tlv is applied).
const bro = require(_here + "/view/bro.js");
//  JAB-030: the universal-pager edge — on a tty the run's hunk stream opens the
//  bro Pager (hunksFromLog), instead of the plain renderHunkLog dump.
const pager = require(_here + "/views/bro/pager.js");

//  run(opts): seed -> build registry -> consume-while-append dispatch loop.
//    opts.seedRows : [{verb, uri}]   the seed job list (argv lowered; JSQUE-004
//                    delivers the real resolution-at-entry seed — here a stub
//                    just forwards the rows).
//    opts.repo     : the opened repo handle (forwarded in ctx; loop is agnostic).
//    opts.out      : the emit sink (JSQUE-005); a no-op stub is used if absent.
//    opts.require  : the shard-relative require of the caller (so the registry's
//                    require(verb) scans the right jsrc/ shard); default global.
//  Returns { dispatched, order } — dispatched count + the verb-dispatch order
//  (the proof the loop drove the queue; the real run cares only about effects).
function run(opts) {
  opts = opts || {};
  const out = opts.out || _nullSink();
  //  JAB-004: PLAIN dispatch is the ONLY dispatch — a converted verb runs ONCE
  //  as fn(...args) reading `be` (the legacy resolve.seed→queue→handle(row,ctx)
  //  path is retired).  ONE edge-catch flushes the partial banner on a handler
  //  throw (DELDIRTY/PUTNONE/…) then re-propagates (jab maps it to the non-zero
  //  exit; no process.exit in handlers — JS-026).
  try { opts.plain.fn.apply(null, opts.plain.args); }
  catch (e) { if (out.flush) out.flush(null); throw e; }
  return { dispatched: 1, order: [opts.plain.verb], outSort: null };
}

//  JAB-029: the in-memory HUNK sink — a grow-on-full HUNK ram log a content
//  view feeds (feed(uri, body, toks, verb, ts)).  abc.ram is fixed-size, so on
//  the binding's `out full` throw we double the cap and replay the held records
//  into a fresh log (atomic feed: a failed record is not half-written).  `.log`
//  is the HUNK log renderHunkLog reads at the edge; `.empty` gates the flush.
function _hunkSink(initial) {
  let cap = initial || (1 << 16);
  let log = abc.ram("HUNK", cap);
  const recs = [];
  function replay() { log = abc.ram("HUNK", cap); for (const r of recs) log.feed(r.uri, r.body, r.toks, r.verb, r.ts); }
  return {
    feed: function (uri, body, toks, verb, ts) {
      const r = { uri: uri, body: body, toks: toks, verb: verb, ts: ts };
      for (let t = 0; t < 40; t++) {
        try { log.feed(uri, body, toks, verb, ts); recs.push(r); return; }
        catch (e) {
          if (!("" + e).includes("full")) throw e;   // only grow on `out full`
          cap = cap * 2 + uri.length + (body ? body.length : 0) +
                (toks ? toks.length * 4 : 0) + 256;
          replay();
        }
      }
      throw "loop: HUNK sink grow exhausted";
    },
    get log() { return log; },
    get empty() { return recs.length === 0; },
  };
}

//  A no-op emit sink so the loop is provable without JSQUE-005 wired in.  Same
//  surface as core/emit.js (create/banner/row/render) — the real sink drops in.
function _nullSink() {
  const rows = [];
  return {
    banner: function () {},
    row: function (path, verb, ts, extra) { rows.push({ path: path, verb: verb }); },
    render: function () { return new Uint8Array(0); },
    rows: rows,
  };
}

//  JAB-030: the verb-vs-URI gate (Design decision b).  A word is a VERB iff a
//  handler file exists at verbs/<w>/<w>.js OR views/<w>/<w>.js under the jsrc/ root
//  (_here).  The [a-zA-Z0-9]+ shape test is the caller's; this is the file probe.
//  BE-029: the probe now BE-CLIMBS from cwd (registry.verbFile), so a nested
//  jsrc/ shard's verbs are seen while core/shared still load from `_here` (the
//  launched be/ up-tree).  `here` is ignored (kept for call-site compat).
function _isVerb(w, here) {
  return registry.verbFile(w) !== null;
}

//  BE-041: a word is a MUTATION verb iff its handler lives in the verbs/ tree
//  (put/post/delete/get/patch/done…), never views/.  The pager's act-button
//  gate: an O-spell click on a mutation runs it and REFRESHES the current view
//  in place (no result screen); a view spell keeps the push-nav behaviour.
function _isMutation(w) {
  return registry.verbFile(w, undefined, ["verbs"]) !== null;
}

//  JAB-030: the bare-URI view default (Design decision: dir -> ls:, file ->
//  blob:/cat:).  `blob:` has no landed handler; a file falls back to the landed
//  `cat:` syntax-hili view (gritzko: "blob: or cat: whatever"); a dir (or an
//  unstattable path) defaults to `ls`.  Returns the VERB; args[0] holds the URI.
function _viewDefault(token, here) {
  //  Strip a leading `<scheme>:` to the bare fs path to stat (`cat:foo` -> `foo`).
  //  The URI-class scheme-drop (remake sans scheme) is byte-identical to the old
  //  slice for every shape — authority/query/fragment ride through untouched.
  let path = token;
  try { const u = uri._parse(token);
        const r = uri._make(undefined, u.authority, u.path, u.query, u.fragment);
        if (r !== undefined && r !== null) path = r; } catch (e) {}
  let st = null;
  try { st = io.stat(path); } catch (e) {}
  if (st && st.kind !== "dir") return _isVerb("blob", here) ? "blob" : "cat";
  return "ls";                              // a dir, or an unresolvable path
}

//  JAB-004: mint/refresh the unified global `be`.  Folds the (intact) discover
//  module's API onto globalThis.be, then overlays this run's ambient fields.
//  BRO-014: seed the session-scoped per-view-TYPE wrap defaults (verb → boolean;
//  true = soft-wrap, false = no-wrap) once; the pager reads/`W`-writes be.wrap.
function mintBe(ambient) {
  const g = globalThis.be || (globalThis.be = {});
  if (!g.wrap) g.wrap = { 
	  log: false, 
	  todo: false,
	  cat: true, 
	  diff: true, 
	  status: true, 
	  ls: false, 
	  list: false 
  };
  return Object.assign(g, discover, ambient);
}

//  URI-011/[Nav]: resolve a scheme-less `//NAME` nav authority in the spell's
//  args to a repo, via `be.wtdir`: `//name[/sub]` → the hive cell at
//  `<srcRoot>/name/sub`, `//`/`//.` → the LAUNCH tree.  A schemed transport is
//  left to the wire; BE-033: a scheme-less `//X` is ALWAYS a worktree (remotes
//  always carry a scheme), so a wtdir miss is NAVNONE, never the wire.  The
//  `//authority` is STRIPPED to the repo-relative sub-path so an UNCONVERTED verb
//  sees a clean arg; the authority itself rides `be.authority` for CONVERTED verbs
//  (full-URI hunks — the launch tree is "here", so its authority is "").  Returns
//  { repo, authority } or null when no arg carries a nav authority.

//  URI-011: TRANSPORT schemes go to the wire and are NOT nav-scoped; a PROJECTOR
//  scheme (`commit:`/`status:`/`diff:`/`log:`/…) with a `//authority` IS a local
//  nav target that must scope (the DIS-060 leak: a clicked `commit://ULOG?sha`).
const TRANSPORT = { ssh: 1, https: 1, http: 1, git: 1, be: 1, file: 1, keeper: 1 };

//  BE-032: `limit` caps the scan — a MUTATION verb takes the authority on arg 0
//  ONLY (rest args are context-relative paths; the verb refuses a `//` rest arg),
//  so a later `//X` operand can no longer silently rescope the whole run.
function authorityRepo(args, limit) {
  const n = limit == null ? args.length : Math.min(limit, args.length);
  for (let i = 0; i < n; i++) {
    const a = String(args[i] || "");
    if (a.indexOf("//") < 0) continue;
    let u; try { u = uri._parse(a); } catch (e) { continue; }
    if (u.authority === undefined) continue;               // no `//` slot
    if (u.scheme && TRANSPORT[u.scheme]) continue;         // ssh:/be:/… → the wire
    const host = u.host || "";
    //  A projector-schemed nav (`commit://ULOG?sha`) scopes its authority too —
    //  resolve via the SCHEME-LESS form, since be.wtdir rejects a scheme.
    const navStr = uri._make(undefined, u.authority, u.path, undefined, undefined) || ("//" + host);
    const dir = be.wtdir(navStr);
    //  BE-033: a scheme-less //authority is ALWAYS a worktree — a wtdir miss is
    //  NAVNONE (any host shape).
    //  BE-037: an empty-host miss (repo-less cwd) is as LOUD as any other — the
    //  raw `///path` must never reach the verb verbatim (silent whole-wt degrade).
    if (!dir) throw "NAVNONE: no worktree //" + host;
    let repo; try { repo = be.find(dir); } catch (e) { continue; }
    if (!repo) continue;
    //  strip the `//authority` → the repo-relative sub-path (+ ?ref#frag), but
    //  KEEP a projector scheme so the verb still dispatches (`commit://ULOG?sha`
    //  → `commit:?sha`, resolved against the now-scoped be.repo).
    const rel = dir.length > repo.wt.length ? dir.slice(repo.wt.length + 1) : "";
    args[i] = uri._make(u.scheme, undefined, rel || undefined, u.query, u.fragment) || "";
    return { repo: repo, authority: (host === "" || host === ".") ? "" : "//" + host };
  }
  return null;
}

//  BE-039: resolve a threaded NAV CONTEXT (`//name[/sub]`, opts2.context from the pager
//  reentry) to its anchored repo + authority — the SAME wtdir→find scope authorityRepo
//  does for an explicit operand, but read from the CONTEXT (never an arg) and NON-FATAL
//  on a miss (an ambient scope → fall back to cwd).  The context has NO arg path baked
//  in, so find lands on the tree TOP (or nav'd sub); each RAW arg descends a mount ITSELF.
function contextRepo(ctxUri) {
  let u; try { u = uri._parse(ctxUri || ""); } catch (e) { return null; }
  if (u.authority === undefined) return null;            // not a `//` nav scope
  if (u.scheme && TRANSPORT[u.scheme]) return null;      // a transport, not nav
  const host = u.host || "";
  const navStr = uri._make(undefined, u.authority, u.path, undefined, undefined) || ("//" + host);
  let dir; try { dir = be.wtdir(navStr); } catch (e) { return null; }   // NAVESCAPE → miss
  if (!dir) return null;
  let repo; try { repo = be.find(dir); } catch (e) { return null; }
  if (!repo) return null;
  //  BE-032: find() climbed to the anchor, but the context's PLAIN sub-dir must
  //  survive as the arg-resolution base (be.ctxDir) — a FILE context (a cat view)
  //  falls back to its dir, never above the anchored wt root.
  let d = dir;
  while (d.length > repo.wt.length && _statKind(d) !== "dir") d = pathlib.dirname(d);
  return { repo: repo, authority: (host === "" || host === ".") ? "" : "//" + host,
           dir: d };
}

function _statKind(p) { try { return io.stat(p).kind; } catch (e) { return undefined; } }

//  --- JSQUE-008: the canonical CLI entry (argv -> seed -> run -> flush) ---
//  The SHARED integrated entry every later verb reuses: argv lowers to a verb +
//  positional args + flags; the repo + its ambient coordinates are pinned ONCE
//  via resolve.seedCtx (resolution-at-entry); resolve.seed turns the positional
//  args into branch-free seed rows; run() drives the loop with the REAL emit
//  sink; ONE out.flush(sort) at the edge renders the collected rows.  Replaces
//  the 002 `_nullSink`/`opts.seedRows` stubs.
//  opts2 (optional): an in-process re-entry marker (JAB-028 / JSQUE-020).  bro's
//  address bar re-enters cli() to drive a spell; each cli() now gets its OWN
//  in-memory queue (run() builds one per call), so no queue path is threaded —
//  opts2.reentry only gates the pager (a re-entry must NEVER open a nested one).
function cli(argv, opts2) {
  opts2 = opts2 || {};
  //  GIT-020: `jab upload-pack <sel>` is a RAW-WIRE serve entry (what be:/ssh
  //  local-exec spawns) — intercept BEFORE the verb/banner/pager machinery and
  //  emit ONLY wire bytes on fd 1 (the hyphen also fails the verb gate anyway).
  if (argv[2] === "upload-pack") {
    require(_here + "/shared/serve.js").uploadPack(argv[3] || "", 0, 1);
    return;
  }
  //  JAB-004: a driveSpell re-entry overlays its ambient onto the shared `be`;
  //  snapshot the outer run's fields so the finally restores them (verbs read be.*).
  const beSaved = opts2.reentry ? _snapBe() : null;
  try {
  return _cli(argv, opts2);
  } finally { if (beSaved) _restoreBe(beSaved); }
}

//  JAB-004: snapshot/restore the `be` ambient (repo/sink/format/force/flags) —
//  the fields the re-entrant driveSpell run overwrites on the shared global.
function _snapBe() {
  const b = globalThis.be || {};
  return { repo: b.repo, sink: b.sink, format: b.format, force: b.force, flags: b.flags,
           ctxDir: b.ctxDir };
}
function _restoreBe(s) { Object.assign(globalThis.be || (globalThis.be = {}), s); }

function _cli(argv, opts2) {
  //  JAB-004: mint the `be` API up front so repo discovery's be.find (loop's +
  //  every alias file's) resolves against the global; ambient overlaid below.
  mintBe({});
  //  Split flags (a leading '-') from the positional args — flags are seed
  //  globals (pinned in ctx), positionals become seed rows.  The first
  //  positional is the parse SUBJECT (verb-or-URI); the rest are its args.
  const flags = [], args = [];
  for (const a of argv.slice(2)) (a[0] === "-" ? flags : args).push(a);

  //  JAB-030: the THREE-shape arg parse + the verb-vs-URI gate, replacing the
  //  bare-verb/':'-split patchwork.  The first positional decides the shape:
  //   (1) verb URI+  — first word is a VERB (matches [a-zA-Z0-9]+ AND a handler
  //       file verbs/<w>/<w>.js | views/<w>/<w>.js exists) → run those records.
  //   (2) URI        — a <scheme>:<uri> token, OR a bare path → a VIEW: a dir
  //       defaults to ls:, a file to blob: (BLOCKED — no blob handler; see the
  //       FLAG below, the practical fall-back is the landed `bro` viewer).
  //   (3) bare jab   — no positionals → ls:. (the cwd listing).
  //  NB the C `require.cpp __main` only routes a BAREWORD or a `scheme:` token to
  //  the loop; a bare FILE/DIR path (with '.'/'/' and no scheme) and the no-arg
  //  case never reach cli() — they need the deferred C routing change (FLAG).
  let verb = args.length ? args[0] : null;
  //  GET.mkd "Forceful execution": a trailing `!` on the VERB token (`get!`)
  //  is the force modifier — shed it and raise --force so the handler discards
  //  local changes (DIS-055 D6).  Only a bareword+`!` (not a `scheme:`/path).
  if (verb && verb.length > 1 && verb[verb.length - 1] === "!" &&
      /^[a-zA-Z0-9]+$/.test(verb.slice(0, -1)) && _isVerb(verb.slice(0, -1), _here)) {
    verb = verb.slice(0, -1); args[0] = verb;
    if (flags.indexOf("--force") < 0) flags.push("--force");
  }
  if (verb == null) {                       // shape (3): bare jab -> ls:.
    verb = "ls"; args.push(".");
  } else if (/^[a-zA-Z0-9]+$/.test(verb) && _isVerb(verb, _here)) {
    args.shift();                           // shape (1): verb URI+
  } else {                                  // shape (2): a URI view
    let scheme = ""; try { scheme = uri._parse(verb).scheme || ""; } catch (e) {}
    //  DIS-060: route `<scheme>:uri` only when scheme is on the allowlist, not any
    //  _isVerb module — a phantom `<verb>:` is absent, so it can't round-trip.
    if (scheme && /^[a-zA-Z0-9]+$/.test(scheme) && SCHEME_ALLOW.has(scheme)) {
      verb = scheme;                        //  scheme:uri — keep the whole token
    } else {                                //  a bare path — default the view
      verb = _viewDefault(verb, _here);     //  dir -> ls, file -> blob/bro
    }
    //  A view's whole URI rides ctx.args (a fragment-only URI can't survive a
    //  queue row); leave args[0] = the full token for the handler to re-parse.
  }

  //  JAB-025: the colour gate for the emit sink.  `--color` forces SGR,
  //  `--plain` forces the plain bytes; otherwise default to whether stdout
  //  (fd 1) is a terminal.  The piped/`--plain` path is byte-parity plain (the
  //  SUT=loop harnesses redirect stdout, so they land here OFF); a tty (or
  //  `--color`) renders the columnar rows through the C THEME.
  //  The SHARED output-mode gate — ONE source for every view.  An explicit flag
  //  wins; with none, a tty defaults to colour.  color = dog/THEME SGR, tlv =
  //  the raw HUNK 'H' records (on-wire), plain = HUNKu8sFeedText bytes.
  //  --color/tty → color; --tlv → tlv; --plain/else → plain.
  const mode = flags.indexOf("--color") >= 0 ? "color"
             : flags.indexOf("--tlv")   >= 0 ? "tlv"
             : flags.indexOf("--plain") >= 0 ? "plain"
             : io.isatty(1)                  ? "color"
             : "plain";
  const color = mode === "color";
  //  JAB-004: the boolean force flag → its own be field (the get!/--force sugar).
  const force = flags.indexOf("--force") >= 0;

  //  JAB-004: the DUAL-CONVENTION fork — a CONVERTED verb (`{jab:"args"}`) runs
  //  the PLAIN path; a LEGACY verb keeps resolve.seed→queue; BOTH share the edge.
  //  BE-029: resolve via the cwd-climb (a nested jsrc/ shard wins), not a single
  //  `_here` root; require the resolved abs path with loop.js's own require.
  const conv = registry.resolveVerb(verb, undefined, require);
  const out = emit.create({ color: color });   // JAB-025: tty/--color gate
  //  JAB-029: the content-view HUNK sink (cat/grep/spot/regex feed it, no fd 1);
  //  cli OWNS the one renderHunkLog(sink.log, mode) -> fd 1 edge write below.
  const sink = _hunkSink();
  let repo = null, res;

  //  JAB-004: EVERY verb is a plain-args handler now — the legacy resolve.seed →
  //  queue → handle(row,ctx) path is retired.  Open the repo (repo-less on throw,
  //  e.g. a fresh GET clone or bro file view), mint `be`, coerce each split CLI
  //  token via scalar(), and run() calls fn(...args) ONCE reading `be`.
  if (!(conv && conv.jab === "args"))
    throw "loop: verb '" + verb + "' has no plain-args handler";
  //  URI-011/[Nav]: a `//name` nav authority (via `be.wtdir`, a hive cell)
  //  scopes the WHOLE spell to that tree + exposes `be.authority` (the `//name`)
  //  so converted verbs emit full-URI hunks; any other arg keeps the cwd repo.
  //  A repo-less cwd is swallowed (be.repo=null).
  //  BE-032: a MUTATION verb takes the authority on arg 0 only (rest args are
  //  context-relative paths); views keep the whole-spell scan.
  let nav = authorityRepo(args, _isMutation(verb) ? 1 : args.length);
  let authority = "", ctxDir = null;
  //  BE-032: `be.ctxDir` is the CONTEXT DIR relative args resolve against
  //  (discover.argRel): an explicit `//X` operand pins the tree ROOT; a pager
  //  context pins its nav'd sub-dir; a plain CLI run pins the invocation cwd; a
  //  context-less reentry (slot-edit/click, args pre-merged root-relative) the root.
  if (nav) { repo = nav.repo; authority = nav.authority; ctxDir = repo.wt; }
  //  BE-039: no explicit `//authority` operand — scope the run to the NAV CONTEXT
  //  threaded from the pager (opts2.context) so RAW args resolve relative to it
  //  (put/delete descend a mount per-arg); a miss / plain CLI falls to cwd — unchanged.
  else if (opts2.context && (nav = contextRepo(opts2.context))) { repo = nav.repo; authority = nav.authority; ctxDir = nav.dir; }
  else try { repo = be.find(); ctxDir = opts2.reentry ? repo.wt : io.cwd(); }
       catch (e) { /* repo-less verb reads be.repo=null */ }
  mintBe({ repo: repo, sink: sink, out: out, format: mode, force: force, flags: flags,
           verb: verb, authority: authority, ctxDir: ctxDir });
  const pargs = args.map(function (t) { return argline.scalar(t); });
  res = run({
    repo: repo, require: require, out: out, sink: sink, flags: flags,
    mode: mode, args: args,
    plain: { verb: verb, fn: conv.fn, args: pargs },
  });
  //  JAB-030: the UNIVERSAL pager edge.  ONE output gate picks the render by the
  //  tty: on a TTY the interactive bro Pager over the run's hunk stream; on a
  //  PIPE/redirect (or --plain/--tlv, or an in-process re-entry) the plain dump.
  //  An in-process re-entry (opts2.reentry — bro's driveSpell capturing --tlv)
  //  must NEVER open a nested pager: it stays the plain/tlv dump it captures.
  const wantPager = io.isatty(1) && mode !== "tlv" && !opts2.reentry &&
                    flags.indexOf("--plain") < 0;
  if (wantPager) {
    //  Gather the run's hunks for the viewport: the content-view sink (cat/grep/
    //  spot/regex feed it) PLUS, if a columnar view (ls/status/refs) emitted
    //  rows, that rendered text wrapped as ONE plain hunk (DEFER: ls/status
    //  feeding the sink directly — JAB-030 TODOs).  A self-paging verb (bro on a
    //  tty) leaves both empty, so the edge opens NO second pager.
    let hunks = sink.empty ? [] : pager.hunksFromLog(sink.log);
    //  Colour the columnar (ls/status/refs) hunk too: on a tty the emit sink's
    //  COLOUR render (THEME date/verb SGR) — not the plain columniser — so the
    //  pager shows it coloured like the content views (gritzko: pager colored).
    const colBytes = out.renderColor ? out.renderColor(res.outSort || null)
                                     : out.render(res.outSort || null);
    //  DIS-060: banner URI = the projector's own `<verb>:` scheme, but a bare
    //  (schemeless) uri for an off-allowlist mutation verb (never a phantom).
    if (colBytes && colBytes.length)
      hunks = hunks.concat([{ uri: SCHEME_ALLOW.has(verb) ? (URI.make(verb) || verb + ":") : verb,
                              verb: "hunk", text: colBytes,
                              toks: new Uint32Array(0), kind: "file" }]);
    if (hunks.length) { _openPager(hunks); return res; }
    //  Nothing to page (a self-paging verb already ran, or no output): done.
    return res;
  }
  //  Non-tty (pipe / --plain / --tlv / re-entry): the byte-parity plain dump.
  //  ONE columnar flush (ls/status/refs) + ONE hunk-sink render to fd 1.
  out.flush(res.outSort || null);   // JSQUE-009: GET owns its edge sort order
  if (!sink.empty) {
    const bytes = bro.renderHunkLog(sink.log, mode);
    if (bytes.length) { const b = io.buf(bytes.length + 8); b.feed(bytes); io.writeAll(1, b); }
  }
  return res;
}

//  JAB-030: open the interactive bro Pager over a hunk array (the universal-tty
//  edge).  Keystrokes come from the controlling terminal (/dev/tty so input
//  still works when stdin is a data pipe — the bro.js pattern); a typed `:`
//  spell re-runs the loop via bro's driveSpell (its OWN capture sink + queue).
function _openPager(hunks) {
  let fd = null, own = false;
  try { fd = io.open("/dev/tty", "rw"); own = true; } catch (e) { fd = null; }
  if (fd === null && io.isatty(0)) fd = 0;
  if (fd === null) fd = 1;
  try {
    const broh = require(_here + "/views/bro/bro.js");
    const p = new pager.Pager(fd, { color: true, driveSpell: broh.driveSpell,
                                    isVerb: function (w) { return _isVerb(w, _here); },
                                    isMutation: _isMutation });
    p.setHunks(hunks);
    p.run();
  } finally { if (own) { try { io.close(fd); } catch (e) {} } }
}

//  JSQUE-016: always required (via the be/main.js entry shim), so export
//  run/cli; the shim self-runs cli() when invoked directly.
if (typeof module !== "undefined")
  //  isVerb: the pager's composer peels a leading token as a verb ONLY if it is a
  //  real handler (else it's a wt-relative path) — the SAME probe cli() dispatches on.
  module.exports = { run: run, cli: cli,
                     isVerb: function (w) { return _isVerb(w, _here); },
                     isMutation: _isMutation };
else cli(process.argv);
