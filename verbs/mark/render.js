//  mark/render.js — StrictMark → HTML, a pure-JS port of beagle/mark (MARK.c)
//  + the dog/tok MKDT block/inline grammars.  No dogenizer: StrictMark's inline
//  layer IS regex markers and its block layer is a regular language, so the
//  whole renderer is regexes + a container stack.  Output is byte-identical to
//  the C `mark` renderer (the golden html/wiki/StrictMark.C.html).
//
//  renderDoc(src, title, opts) -> full HTML document string.
//    opts.head / opts.body : raw HTML injected before </head> / after <body>.
//    opts.root             : site root for `[/...]` existence probing (else none).
//    opts.exists(relPath)  : optional fs probe (root-relative) → bool.
"use strict";

//  ---- escaping (MARKu8bFeedEsc: & < > " -> entities; & first) ----
function esc(s) {
  return s.replace(/&/g, "&amp;").replace(/</g, "&lt;")
          .replace(/>/g, "&gt;").replace(/"/g, "&quot;");
}

//  ---- inline tokenizer (MKDTInlineLexer, MKDT.c.rl `main`) ----
//  A ragel longest-match scanner: at each position every rule is tried and the
//  LONGEST match wins; ties go to the rule listed FIRST.  Tags mirror MKDT's
//  callbacks: H code, G span (emph/link/image), E escape, L number, F key,
//  S word, P punct, W space.
var NWS = "^ \\t\\r\\n\\f\\v";                 // nws = not whitespace
var INLINE_RULES = [
  { re: /`[^`\n]+`/y,                              tag: "H" },   // code
  { re: /\\\*[^*\n]*\*/y,                          tag: "E" },   // \*..* escape
  { re: /\\_[^_\n]*_/y,                            tag: "E" },   // \_.._ escape
  { re: new RegExp("\\\\[" + NWS + "*_]", "y"),    tag: "E" },   // \<char> escape
  { re: new RegExp("\\*[" + NWS + "*][^*\\n]*\\*", "y"), tag: "G" }, // strong
  { re: new RegExp("_[" + NWS + "_][^_\\n]*_", "y"),     tag: "G" }, // emph
  { re: new RegExp("~~[" + NWS + "~](?:[^~\\n]|~[^~\\n])*~~", "y"), tag: "G" }, // strike
  { re: /\[[^\]\n]+\]\[[0-9A-Za-z]\]/y,           tag: "G" },   // reflink
  { re: /!\[[^\]\n]+\]\[[0-9A-Za-z]\]/y,          tag: "G" },   // image
  { re: /\[[^\]\n]+\]/y,                           tag: "G" },   // shortcut
  { re: /0[xX][0-9a-fA-F]+/y,                       tag: "L" },   // hex
  { re: /[0-9]+\.[0-9]*/y,                          tag: "L" },   // float
  { re: /\.[0-9]+/y,                                tag: "L" },   // .frac
  { re: /[0-9]+/y,                                  tag: "L" },   // int
  { re: /~~/y,                                      tag: "P" },   // stray ~~
  { re: /[A-Z][A-Z0-9_]*-[0-9]+/y,                  tag: "F" },   // issue key
  { re: /[a-zA-Z_][a-zA-Z_0-9]*/y,                  tag: "S" },   // word
  { re: /[[\]()!*_~|\\:.\-+#&<>{}=,;/'"@^`]/y,      tag: "P" },   // punct
  { re: /\n/y,                                      tag: "W" },   // newline
  { re: /[ \t\r\f\v]+/y,                            tag: "W" },   // spaces
  { re: /[\x80-\xff][\x80-\xbf]*/y,                 tag: "S" },   // utf8 (byte view)
  { re: /[\s\S]/y,                                  tag: "P" },   // any other
];

function scanInline(text) {
  var toks = [];
  var i = 0;
  while (i < text.length) {
    var best = null, bestLen = 0;
    for (var r = 0; r < INLINE_RULES.length; r++) {
      var rule = INLINE_RULES[r];
      rule.re.lastIndex = i;
      var m = rule.re.exec(text);
      if (m && m.index === i && m[0].length > bestLen) {
        best = { tag: rule.tag, text: m[0] }; bestLen = m[0].length;
      }
    }
    if (best === null) { best = { tag: "P", text: text[i] }; bestLen = 1; }
    toks.push(best);
    i += bestLen;
  }
  return toks;
}

//  ---- span decomposition (MKDTDecomposeSpan, mkdtg) ----
//  A 'G' token -> {kind, text, label}; kind B strong / I emph / D del /
//  A link / M image; for a shortcut, label == text.
function decompose(tok) {
  var m;
  if ((m = /^\*([^*\n]*)\*$/.exec(tok)))  return { kind: "B", text: m[1] };
  if ((m = /^_([^_\n]*)_$/.exec(tok)))    return { kind: "I", text: m[1] };
  if ((m = /^~~((?:[^~\n]|~[^~\n])*)~~$/.exec(tok))) return { kind: "D", text: m[1] };
  if ((m = /^!\[([^\]\n]*)\]\[([0-9A-Za-z])\]$/.exec(tok)))
    return { kind: "M", text: m[1], label: m[2] };
  if ((m = /^\[([^\]\n]*)\]\[([0-9A-Za-z])\]$/.exec(tok)))
    return { kind: "A", text: m[1], label: m[2] };
  if ((m = /^\[([^\]\n]*)\]$/.exec(tok)))
    return { kind: "A", text: m[1], label: m[1] };
  return { kind: 0 };
}

//  ---- link helpers (MARK.c mark_emit_*) ----
function isPageExt(ext) { return ext === "mkd" || ext === "md"; }
function trimPageExt(s) {
  var m = /^(.*)\.(mkd|md)$/.exec(s);
  return m ? m[1] : s;
}
function baseName(s) { var i = s.lastIndexOf("/"); return i < 0 ? s : s.slice(i + 1); }
//  Normalize a path: collapse `.`/`..`/`//` segments (PATHu8bNorm-ish).
function normPath(p) {
  var abs = p[0] === "/";
  var parts = p.split("/"), out = [];
  for (var i = 0; i < parts.length; i++) {
    var seg = parts[i];
    if (seg === "" || seg === ".") continue;
    if (seg === "..") { if (out.length && out[out.length - 1] !== "..") out.pop();
                        else if (!abs) out.push(".."); }
    else out.push(seg);
  }
  return (abs ? "/" : "") + out.join("/");
}

function emitUrl(url) {                        // trailing .mkd -> .html, escaped
  if (/\.mkd$/.test(url)) return esc(url.slice(0, -4)) + ".html";
  return esc(url);
}

//  ---- the renderer ----
function Renderer(opts) {
  this.out = [];
  this.refs = {};                              // key -> url
  this.opts = opts || {};
}
Renderer.prototype.lit = function (s) { this.out.push(s); };
Renderer.prototype.escf = function (s) { this.out.push(esc(s)); };

//  pass 1: collect [key]: url reference definitions.
Renderer.prototype.collectRefs = function (src) {
  var lines = src.split("\n");
  for (var i = 0; i < lines.length; i++) {
    var m = /^ *\[([^\]]+)\]:[ \t]*([^ \t"\r]+)/.exec(lines[i]);
    if (m && !(m[1] in this.refs)) this.refs[m[1]] = m[2];
  }
};

//  root-relative page existence (opts.exists), for `[/x]` -> `/x.html`.
Renderer.prototype.pageExists = function (stem) {
  if (!this.opts.exists || !stem) return false;
  return this.opts.exists(stem + ".mkd") || this.opts.exists(stem + ".md");
};

//  emit an absolute `[/path]` shortcut (mark_emit_pathlink): root-absolute href;
//  page-ext (explicit, or extensionless whose source exists) -> .html, else verbatim.
Renderer.prototype.emitPathLink = function (bracket) {
  var tgt = normPath(bracket);
  var em = /\.([A-Za-z0-9]+)$/.exec(tgt);
  var ext = em ? em[1] : "";
  var topage = isPageExt(ext);
  if (!topage && ext === "") topage = this.pageExists(tgt.replace(/^\//, ""));
  var fin = topage ? trimPageExt(tgt) + ".html" : tgt;
  this.escf(fin);
};

//  emit a relative `[Page]` shortcut (mark_emit_implicitlink): `./stem.html`.
Renderer.prototype.emitImplicitLink = function (bracket) {
  var stem = trimPageExt(normPath(bracket));
  this.lit("./"); this.escf(stem + ".html");
};

Renderer.prototype.emitLink = function (g, image) {
  var url = this.refs[g.label];
  var found = url !== undefined;
  if (image) {
    this.lit('<img src="');
    if (found) this.lit(emitUrl(url));
    this.lit('" alt="'); this.escf(g.text); this.lit('">');
    return;
  }
  var shortcut = g.label === g.text;
  var absolute = g.text[0] === "/";
  var pathlink = !found && shortcut && absolute;
  var implicit = !found && shortcut && !absolute;
  this.lit('<a href="');
  if (found) this.lit(emitUrl(url));
  else if (pathlink) this.emitPathLink(g.text);
  else if (implicit) this.emitImplicitLink(g.text);
  this.lit('">');
  if (pathlink || implicit) this.escf(trimPageExt(baseName(g.text)));
  else this.escf(g.text);
  this.lit("</a>");
};

//  inline render (mark_inline + mark_inline_cb): tokenize `text`, emit HTML.
Renderer.prototype.inline = function (text) {
  var toks = scanInline(text);
  for (var i = 0; i < toks.length; i++) {
    var t = toks[i];
    if (t.tag === "H") {                       // `code`
      var inner = t.text.length >= 2 ? t.text.slice(1, -1) : t.text;
      this.lit("<code>"); this.escf(inner); this.lit("</code>");
    } else if (t.tag === "E") {                // escape: drop leading backslash
      this.escf(t.text.slice(1));
    } else if (t.tag === "G") {
      var g = decompose(t.text);
      if (g.kind === "B") { this.lit("<strong>"); this.inline(g.text); this.lit("</strong>"); }
      else if (g.kind === "I") { this.lit("<em>"); this.inline(g.text); this.lit("</em>"); }
      else if (g.kind === "D") { this.lit("<del>"); this.inline(g.text); this.lit("</del>"); }
      else if (g.kind === "A") { this.emitLink(g, false); }
      else if (g.kind === "M") { this.emitLink(g, true); }
      else this.escf(t.text);
    } else {                                    // S / P / L / W / F: literal
      this.escf(t.text);
    }
  }
};

module.exports = { renderDoc: renderDoc, esc: esc, scanInline: scanInline,
                   Renderer: Renderer };

//  ---- block classification (MKDTBlock) ----
//  line: one source line WITHOUT its trailing '\n'.
function classify(line) {
  var b = { depth: 0, marker: "none", todo: " ", heading: 0, fence: 0,
            fenceBlank: false, hrule: false, refdef: false, content: 0 };
  var i = 0;
  while (line.length - i >= 4 && line.substr(i, 4) === "    ") { i += 4; b.depth++; }
  var rest = line.slice(i);
  b.content = i;
  //  fence: a run of 3 or 4 backticks after the indents.
  var fm = /^`+/.exec(rest);
  if (fm && (fm[0].length === 3 || fm[0].length === 4)) {
    b.fence = fm[0].length;
    b.fenceBlank = /^\s*$/.test(rest.slice(fm[0].length));
    return b;
  }
  //  hrule: 3-4 dashes, blank rest.
  var hm = /^-{3,4}\s*$/.exec(rest);
  if (hm) { b.hrule = true; return b; }
  //  reference definition: [key]: ...
  if (/^\[[^\]]+\]:/.test(rest)) { b.refdef = true; return b; }
  //  heading: 1-4 '#' then a gap space (or EOL).
  var head = /^(#{1,4})(?= |$)/.exec(rest);
  if (head) {
    b.heading = head[1].length;
    var j = i + head[1].length;
    while (j < line.length && line[j] === " ") j++;
    b.content = j;
    return b;
  }
  //  4-char markers in the group after the indents (padded, any column):
  //  quote `>`, todo `-[x]`, ulist `-`, olist `N.`.
  var slot = line.substr(i, 4);
  if (/^-\[[ vVxX-]\]/.test(rest)) {
    b.marker = "todo"; b.todo = rest[2]; b.content = i + 4; return b;
  }
  if (/>/.test(slot) && /^ *> */.test(slot)) { b.marker = "quote"; b.content = i + 4; return b; }
  if (/^ *- +/.test(slot) || /^ *-  ?$/.test(slot) || slot === " -  " || slot === "-   ") {
    if (/^ *-/.test(slot)) { b.marker = "ulist"; b.content = i + 4; return b; }
  }
  //  numbered marker (MKDTB `number`): 1-3 digits then '.', spaces padding the
  //  rest of the EXACTLY-4-char slot — nothing wider or narrower is a marker.
  if (slot.length === 4 && /^ *[0-9]{1,3}\. *$/.test(slot)) {
    b.marker = "olist"; b.content = i + 4; return b;
  }
  return b;
}

function isBlank(line) { return /^[ \t\r]*$/.test(line); }

//  ---- block state machine (MARK.c mark_blocks) ----
function renderBlocks(rd, src) {
  var lines = [];
  { var s = src, nl;
    while (s.length) { nl = s.indexOf("\n");
      if (nl < 0) { lines.push({ c: s, nl: false }); break; }
      lines.push({ c: s.slice(0, nl), nl: true }); s = s.slice(nl + 1); } }

  var stk = [];                                // {kind:'div'|'ul'|'ol'|'quote'|'todo', li}
  var st = { inFence: false, fenceLen: 0, inPara: false, para: "",
             paraOpener: false, paraLi: false, paraDel: false,
             h1Seen: false, opener: false };

  function paraFlush() {
    if (!st.inPara) return;
    if (st.paraLi) {
      if (st.paraDel) rd.lit("<del>");
      rd.inline(st.para);
      if (st.paraDel) rd.lit("</del>");
    } else {
      rd.lit("<p>\n"); rd.inline(st.para); rd.lit("\n</p>\n");
    }
    st.para = ""; st.inPara = false; st.paraOpener = false;
    st.paraLi = false; st.paraDel = false;
  }
  function pop() {
    var f = stk.pop();
    if (f.kind === "div") rd.lit("</div>\n");
    else if (f.kind === "quote") rd.lit("</blockquote>\n");
    else { if (f.li) rd.lit("</li>\n"); rd.lit(f.kind === "ol" ? "</ol>\n" : "</ul>\n"); }
  }
  function unwind(n) { if (stk.length > n) paraFlush(); while (stk.length > n) pop(); }
  function growDivs(depth) {
    while (stk.length < depth) { rd.lit("<div>\n"); stk.push({ kind: "div", li: false }); }
  }
  function enterLeaf(depth) { unwind(depth); growDivs(depth); }
  function enterList(depth, ord, item) {
    var want = ord ? "ol" : "ul";
    paraFlush(); unwind(depth + 1);
    var reuse = stk.length === depth + 1 && stk[depth].kind === want;
    if (stk.length === depth + 1 && !reuse) unwind(depth);
    if (reuse) { if (stk[depth].li) rd.lit("</li>\n"); }
    else { growDivs(depth); rd.lit(want === "ol" ? "<ol>\n" : "<ul>\n");
           stk.push({ kind: want, li: false }); }
    rd.lit("<li>");
    st.para += item; st.paraLi = true; st.inPara = true;
    stk[depth].li = true;
  }
  function enterQuote(depth, content) {
    paraFlush(); unwind(depth + 1);
    var reuse = stk.length === depth + 1 && stk[depth].kind === "quote";
    if (stk.length === depth + 1 && !reuse) unwind(depth);
    if (!reuse) { growDivs(depth); rd.lit("<blockquote>\n");
                  stk.push({ kind: "quote", li: false }); }
    rd.inline(content); rd.lit("\n");
  }
  function enterTodo(depth, state, item) {
    paraFlush(); unwind(depth + 1);
    var reuse = stk.length === depth + 1 && stk[depth].kind === "todo";
    if (stk.length === depth + 1 && !reuse) unwind(depth);
    if (reuse) { if (stk[depth].li) rd.lit("</li>\n"); }
    else { growDivs(depth); rd.lit('<ul class="todo">\n');
           stk.push({ kind: "todo", li: false }); }
    var cls = "open", checked = false, del = false;
    if (state === "v" || state === "V") { cls = "done"; checked = true; }
    else if (state === "-") cls = "blocked";
    else if (state === "x" || state === "X") { cls = "wontfix"; del = true; }
    rd.lit('<li class="'); rd.lit(cls); rd.lit('"><input type="checkbox"');
    if (checked) rd.lit(" checked");
    rd.lit(" disabled> ");
    st.para += item; st.paraLi = true; st.paraDel = del; st.inPara = true;
    stk[depth].li = true;
  }

  for (var li = 0; li < lines.length; li++) {
    var linec = lines[li].c;
    var b = classify(linec);

    if (st.inFence) {
      if (b.fence >= st.fenceLen && b.fenceBlank) { st.inFence = false; rd.lit("</code></pre>\n"); }
      else { rd.escf(linec); rd.lit("\n"); }
      continue;
    }
    if (b.fence === 3 || b.fence === 4) {
      paraFlush(); enterLeaf(b.depth);
      st.inFence = true; st.fenceLen = b.fence; rd.lit("<pre><code>");
      continue;
    }
    if (isBlank(linec)) {
      paraFlush();
      while (stk.length && stk[stk.length - 1].kind !== "div") pop();
      continue;
    }
    if (b.hrule) { paraFlush(); enterLeaf(b.depth); rd.lit("<hr>\n"); continue; }
    if (b.refdef) continue;                    // collected in pass 1
    if (b.heading > 0) {
      paraFlush(); enterLeaf(b.depth);
      var hc = linec.slice(b.content);
      if (b.heading === 1) { st.h1Seen = true; st.opener = true; }
      var tag = "h" + b.heading;
      rd.lit("<" + tag + ">"); rd.inline(hc); rd.lit("</" + tag + ">\n");
      continue;
    }
    if (b.marker === "ulist" || b.marker === "olist") {
      enterList(b.depth, b.marker === "olist", linec.slice(b.content));
      st.opener = false; continue;
    }
    if (b.marker === "quote") { enterQuote(b.depth, linec.slice(b.content)); st.opener = false; continue; }
    if (b.marker === "todo") { enterTodo(b.depth, b.todo, linec.slice(b.content)); st.opener = false; continue; }

    //  paragraph / summary
    var pc = linec.slice(b.content);
    var cont = st.inPara && stk.length === b.depth;
    if (!cont) {
      paraFlush(); enterLeaf(b.depth);
      st.paraOpener = st.opener; st.opener = false;
    } else {
      pc = pc.replace(/^ +/, "");
      st.para += " ";
    }
    st.para += pc; st.inPara = true;
  }
  paraFlush(); unwind(0);
  if (st.inFence) rd.lit("</code></pre>\n");
}

//  ---- document (MARKRenderDoc) ----
function renderDoc(src, title, opts) {
  var rd = new Renderer(opts);
  rd.lit('<!DOCTYPE html>\n<html lang="en">\n<head>\n');
  rd.lit('<meta charset="utf-8">\n');
  rd.lit('<meta name="viewport" content="width=device-width,initial-scale=1">\n');
  rd.lit("<title>");
  rd.escf(title && title.length ? title : "wiki");
  rd.lit("</title>\n");
  if (opts && opts.head) rd.lit(opts.head);
  rd.lit("</head>\n<body>\n");
  if (opts && opts.body) rd.lit(opts.body);
  rd.collectRefs(src);
  renderBlocks(rd, src);
  if (opts && opts.foot) rd.lit(opts.foot);      // footer.html, before </body>
  rd.lit("</body>\n</html>\n");
  return rd.out.join("");
}
