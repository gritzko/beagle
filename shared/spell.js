//  spell.js — URI-011: the `word(context_uri, …rest)` spell composer, SHARED by
//  the pager address bar (views/bro/pager.js) and the CLI (core/loop.js) so both
//  classify tokens IDENTICALLY.  The FIRST URI-shaping token (`./x` path, `//WT`
//  auth, `?x` ref, `#x` frag, `scheme:…`) updates the context URI (one component
//  MERGES, ≥2 of {scheme,auth,path} RESETS, `?ref`/`#frag` always merge); every
//  other token is REST — the verb's natural slot — handed through RAW.  Pure over
//  the native `URI` global + shared/argline.
"use strict";

const argline = require("shared/argline.js");

//  Total URI parse (never throws — an empty URI on malformed input).
function parse(s) { try { return new URI(s || ""); } catch (e) { return new URI(""); } }

//  Join a relative path onto a base treated as a DIRECTORY (`.` skip, `..` pop).
function joinPath(base, rel) {
  const segs = base ? base.split("/") : [];
  for (const seg of rel.split("/")) {
    if (seg === "" || seg === ".") continue;
    if (seg === "..") { if (segs.length) segs.pop(); }
    else segs.push(seg);
  }
  return segs.join("/");
}

//  Does a token SHAPE the context URI (a leading `.` `/` `?` `#`, or a `scheme:`
//  prefix)?  A bareword or embedded-slash path (`a.txt`, `b/c.txt`) is REST.
function uriShaping(tok) {
  if (!tok) return false;
  const c = tok[0];
  if (c === "/" || c === "." || c === "?" || c === "#") return true;
  const ci = tok.indexOf(":");
  return ci > 0 && /^[a-zA-Z][a-zA-Z0-9+.-]*$/.test(tok.slice(0, ci));
}

//  The context URI with its scheme DROPPED — the scheme was the VIEW name
//  (`status:`/`cat:`), not part of the address; arg 0 is a bare `//wt/path`.
function bareCtx(ctx) {
  return URI.make(undefined, ctx.authority, ctx.path, ctx.query, ctx.fragment) || "";
}

//  Merge one URI-shaping token `t` onto the context `ctx`.  ≥2 of {scheme,auth,
//  path} = a fully-qualified address → RESET (the token IS arg 0).  Else update
//  only the supplied slot(s): a relative path joins the context AS A DIR, an
//  absolute path replaces; `?ref`/`#frag` NEVER count toward the reset.  A path
//  move drops a now-stale `#fragment` (line anchor).
function mergeUri(ctx, t) {
  //  A SCHEME makes the token a complete address (`size:#sha`, `ssh://h`, `cat:f`)
  //  → RESET; else ≥2 addressing slots {auth,path} (`//OTHER/`) also reset.  A lone
  //  `//WT` / `./x` / `?ref` / `#frag` updates just that slot of the context.
  const ap = (t.authority !== undefined ? 1 : 0) + (t.path ? 1 : 0);
  if (t.scheme || ap >= 2) return t.toString();
  const scheme = undefined;
  const auth = t.authority !== undefined ? t.authority : ctx.authority;
  let path = ctx.path, frag = ctx.fragment;
  if (t.path) {
    //  URI-011b: a BARE or `/`-led path is WT-RELATIVE (rootless, replaces the
    //  context path); only `./`/`../` is context-relative (joins it AS A DIR).
    path = t.path[0] === "." ? joinPath(ctx.path || "", t.path)
                             : t.path.replace(/^\/+/, "");
    if (auth !== undefined && path && path[0] !== "/") path = "/" + path;
    if (t.fragment === undefined) frag = undefined;     // path moved → drop #anchor
  }
  const query = t.query !== undefined ? t.query : ctx.query;
  if (t.fragment !== undefined) frag = t.fragment;
  return URI.make(scheme, auth, path, query, frag) || "";
}

//  shapeArg0 → { arg0, rest }.  A leading `-` = default context + raw REST; else the
//  first unquoted token shapes arg 0 (bareIsUri: a bareword is a wt-relative path).
//  [Nav]/URI-011: REST tokens stay RAW wt-relative — arg 0 carries the `//authority`
//  (authorityRepo hoists+strips it to scope the repo), and the REST are already the
//  wt-relative paths the verb stages, so they must NOT gain a `//authority` (only
//  arg 0's is stripped downstream; an authority left on a rest arg stages a bogus
//  `//WT/path` row).  Retiring put/delete bindRest (arg0-dir rebasing) is what fixes
//  the cross-dir `:put core/x test/y` → `core/test/y` mis-scope.
function shapeArg0(ctxUri, items, bareIsUri) {
  const ctx = parse(ctxUri);
  if (items.length && !items[0].q && items[0].tok === "-") {
    items.shift();                                   // `:word - a b` → ctx + raw REST
    return { arg0: bareCtx(ctx), rest: items.map(function (it) { return it.tok; }) };
  }
  let arg0;
  if (items.length && !items[0].q && (bareIsUri || uriShaping(items[0].tok)))
    arg0 = mergeUri(ctx, parse(items.shift().tok));
  else
    arg0 = bareCtx(ctx);
  return { arg0: arg0, rest: items.map(function (it) { return it.tok; }) };
}

//  compose from a raw spell STRING (the pager address bar): shell-split, keep each
//  token's quote flag, PEEL a leading bareword verb, then shape arg 0 + rest.
function compose(ctxUri, verbFallback, spell, isVerb) {
  const sp = argline.shellSplit(spell || "");
  const items = sp.toks.map(function (t, i) { return { tok: t, q: !!sp.split[i] }; });
  let verb = verbFallback || "", verbCall = false;
  //  Peel a leading bareword verb — but with an isVerb probe, only a REAL verb
  //  (else `verbs` in an `ls` view shadows the path retarget → a stray 2nd hunk).
  if (items.length && !items[0].q && /^[a-zA-Z][a-zA-Z0-9]*$/.test(items[0].tok) &&
      (!isVerb || isVerb(items[0].tok)))
    { verb = items.shift().tok; verbCall = true; }
  //  BE-039: a VERB word-call keeps args RAW (no arg0 ctx-merge) — the context travels
  //  as CONTEXT (threaded via driveSpell); a slot-edit (no verb) keeps the merge (nav).
  if (verbCall) {
    const rest = items.map(function (it) { return it.tok; });
    return { verb: verb, arg0: rest.length ? rest.shift() : "", rest: rest, context: ctxUri };
  }
  const s = shapeArg0(ctxUri, items, true);   // pager: a bareword IS a URI part
  return { verb: verb, arg0: s.arg0, rest: s.rest, context: "" };
}

//  compose from pre-split CLI tokens with the verb ALREADY known (loop.cli split
//  it off): no verb peel, no quote flags (the OS shell resolved quoting; a token
//  with a space is inherently non-URI-shaping).  arg 0 = the cwd/`//WT` context.
function composeArgv(ctxUri, verb, tokens) {
  const items = tokens.map(function (t) { return { tok: String(t), q: false }; });
  const s = shapeArg0(ctxUri, items);
  return { verb: verb, arg0: s.arg0, rest: s.rest };
}

//  The directory of a path (drop the last segment); "" when segment-less.
function dirOfPath(p) { p = p || ""; const i = p.lastIndexOf("/"); return i >= 0 ? p.slice(0, i) : ""; }

//  bindRest(argv, isDir) → argv with the REST paths bound under arg 0's context
//  (URI-011, for put/delete inside a nav context).  arg 0 is the context: a DIR
//  arg 0 is the base — rest joins under it and arg 0 itself is dropped; a FILE
//  arg 0 is a target AND rest joins its parent (so `["//T/dir/a.txt","b/c.txt"]`
//  ≡ `["//T/dir","a.txt","b/c.txt"]`).  `isDir(path)` is the verb's file-vs-dir
//  oracle (stats the wt); a ref/flag rest arg (`?`/`#`/`-`) passes through raw.
function bindRest(argv, isDir) {
  if (argv.length <= 1) return argv.slice();
  const base = argv[0], rest = argv.slice(1);
  const baseIsDir = isDir(base);
  const baseDir = baseIsDir ? base : dirOfPath(base);
  const under = rest.map(function (p) {
    if (!p || p[0] === "?" || p[0] === "#" || p[0] === "-") return p;   // ref/flag: raw
    return baseDir ? joinPath(baseDir, p) : p;
  });
  return baseIsDir ? under : [base].concat(under);
}

//  A call { verb, arg0, rest } → a drivable spell string.  No rest: a scheme'd
//  arg 0 with no verb drives ITSELF (the scheme is the view), else `verb arg0`.
//  With rest: the shape-3 eval form `verb(arg0,"r1",…)` so a spaced message stays
//  ONE argument (argline evals it back to real values).
function buildSpell(c) {
  //  BE-039: arg0 "" (context now rides `context`) → the lone verb; a SPACED arg0
  //  (a quoted message) rides the eval form so `verb arg0` is not re-split on it.
  if (!c.rest.length) {
    if (!c.verb) return c.arg0;
    if (!c.arg0) return c.verb;
    return /\s/.test(c.arg0) ? c.verb + "(" + JSON.stringify(String(c.arg0)) + ")"
                             : c.verb + " " + c.arg0;
  }
  const a = [c.arg0].concat(c.rest).map(function (x) { return JSON.stringify(String(x)); });
  return (c.verb || "") + "(" + a.join(",") + ")";
}

module.exports = { uriShaping: uriShaping, bareCtx: bareCtx, mergeUri: mergeUri,
                   compose: compose, composeArgv: composeArgv, buildSpell: buildSpell,
                   bindRest: bindRest, joinPath: joinPath };
