//  mark/render.js — StrictMark → HTML, a pure-JS port of beagle/mark (MARK.c)
//  + the dog/tok MKDT block/inline grammars.  No dogenizer: StrictMark's inline
//  layer IS regex markers and its block layer is a regular language, so the
//  whole renderer is regexes + a container stack.  Output tracked the C `mark`
//  renderer byte-for-byte (the golden html/wiki/StrictMark.C.html) until
//  captioned figures: a paragraph-leading `![alt][l]` now renders as a
//  <figure>/<figcaption> pair, where C still emits a bare inline <img>.  Every
//  other construct stays byte-identical, so the golden holds for pages without
//  a leading image; beagle/mark/MARK.c has yet to grow the same rule.
//
//  renderDoc(src, title, opts) -> full HTML document string.
//    opts.head / opts.body : raw HTML injected before </head> / after <body>.
//    opts.root             : site root for `[/...]` existence probing (else none).
//    opts.exists(relPath)  : optional fs probe (root-relative) → bool.
"use strict";

//  the head-meta matcher pageMeta skips lives in ONE place, with the board's
//  own head read (shared/ticketpage.js) — never a second pair grammar here.
const page = require("../../shared/ticketpage.js");

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
  { re: /\\[^ \t\r\n\f\v]/y,                    tag: "E" },   // \<char> escape
  { re: new RegExp("\\*[" + NWS + "*](?:\\\\.|[^*\\n])*\\*", "y"), tag: "G" }, // strong
  { re: new RegExp("_[" + NWS + "_](?:\\\\.|[^_\\n])*_", "y"),        tag: "G" }, // emph
  { re: new RegExp("~[" + NWS + "~](?:\\\\.|[^~\\n])*~", "y"),        tag: "G" }, // strike
  { re: /\[(?:\\.|[^\]\n])+\]\[[0-9A-Za-z]\]/y,     tag: "G" },   // reflink
  { re: /!\[(?:\\.|[^\]\n])+\]\[[0-9A-Za-z]\]/y,    tag: "G" },   // image
  { re: /\[(?:\\.|[^\]\n])+\]/y,                    tag: "G" },   // shortcut
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
];

//  DOG-024: a span is markup + text, not one blob — the delimiters come back
//  as their own 'G' tokens and the body is re-scanned one level down, exactly
//  as `MKDTEmitSpan` splits it in C (dog/tok/MKDT.c).  A code body is verbatim.
var MAX_DEPTH = 16;

function scanInline(text, depth) {
  depth = depth || 0;
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
    if (best.tag === "H" && best.text.length >= 3) {
      toks.push({ tag: "G", text: best.text.slice(0, 1) });
      toks.push({ tag: "H", text: best.text.slice(1, -1) });
      toks.push({ tag: "G", text: best.text.slice(-1) });
    } else if (best.tag === "G" && depth < MAX_DEPTH) {
      var g = decompose(best.text);
      var cut = g.kind === "M" ? 2 : (g.kind ? 1 : -1);   // "![" | one char
      if (cut < 0) {
        toks.push(best);
      } else {
        toks.push({ tag: "G", text: best.text.slice(0, cut) });
        var inner = scanInline(g.text, depth + 1);
        for (var k = 0; k < inner.length; k++) toks.push(inner[k]);
        toks.push({ tag: "G", text: best.text.slice(cut + g.text.length) });
      }
    } else {
      toks.push(best);
    }
    i += bestLen;
  }
  return toks;
}

//  ---- span decomposition (MKDTDecomposeSpan, mkdtg) ----
//  A 'G' token -> {kind, text, label}; kind B strong / I emph / D del /
//  A link / M image; for a shortcut, label == text.
function decompose(tok) {
  var m;
  if ((m = /^\*((?:\\.|[^*\n])*)\*$/.exec(tok)))  return { kind: "B", text: m[1] };
  if ((m = /^_((?:\\.|[^_\n])*)_$/.exec(tok)))    return { kind: "I", text: m[1] };
  if ((m = /^~((?:\\.|[^~\n])*)~$/.exec(tok)))    return { kind: "D", text: m[1] };
  if ((m = /^!\[((?:\\.|[^\]\n])*)\]\[([0-9A-Za-z])\]$/.exec(tok)))
    return { kind: "M", text: m[1], label: m[2] };
  if ((m = /^\[((?:\\.|[^\]\n])*)\]\[([0-9A-Za-z])\]$/.exec(tok)))
    return { kind: "A", text: m[1], label: m[2] };
  if ((m = /^\[((?:\\.|[^\]\n])*)\]$/.exec(tok)))
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

//  A figure's layout rides in the image URL fragment, the same styling channel
//  the bare <img> uses (assets/css/style.css): `pic.jpg#rightw40` -> the
//  classes `right w40`, which the stylesheet floats and widths.  The fragment
//  stays in src (the browser drops it when fetching); the classes just move the
//  layout onto the figure, so the caption travels with the image.
function figureClass(url) {
  var hash = url.indexOf("#");
  var frag = hash < 0 ? "" : url.slice(hash + 1);
  var cls = [];
  var align = /left|right|center/.exec(frag);
  if (align) cls.push(align[0]);
  var width = /w[0-9]+/.exec(frag);
  if (width) cls.push(width[0]);
  return cls.join(" ");
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

//  emit `![text][label]` as a captioned <figure>: the bracket text is both the
//  alt and the visible caption.  Only paragraph-leading images come here (see
//  paraFlush); a mid-sentence image stays a bare inline <img> via emitLink.
Renderer.prototype.emitFigure = function (text, label) {
  var url = this.refs[label];
  var cls = url === undefined ? "" : figureClass(url);
  this.lit("<figure");
  if (cls) { this.lit(' class="'); this.escf(cls); this.lit('"'); }
  this.lit(">\n<img src=\"");
  if (url !== undefined) this.lit(emitUrl(url));
  this.lit('" alt="'); this.escf(text); this.lit('">\n');
  this.lit("<figcaption>"); this.escf(text); this.lit("</figcaption>\n");
  this.lit("</figure>\n");
};

//  inline render (mark_inline + mark_inline_cb): tokenize `text`, emit HTML.
//  inline render: the scanner hands over markup and text separately (DOG-024),
//  so pair the delimiters with a stack instead of recursing.  The opener names
//  the kind; a link's closer carries its label.  Emphasis wraps whatever HTML
//  was emitted in between; a link's body is raw text (its markup is literal,
//  as it always was), so a link frame buffers instead of emitting.
var SPAN_OPEN = { "*": "B", "_": "I", "~": "D", "[": "A", "![": "M", "`": "C" };
var SPAN_TAG = { B: "strong", I: "em", D: "del" };

function spanCloses(frame, tok) {
  if (frame.kind === "A" || frame.kind === "M")
    return /^\](\[[0-9A-Za-z]\])?$/.test(tok);
  if (frame.kind === "C") return tok === "`";
  return tok === frame.open;
}

Renderer.prototype.inline = function (text) {
  var toks = scanInline(text);
  var stack = [];
  for (var i = 0; i < toks.length; i++) {
    var t = toks[i];
    var top = stack.length ? stack[stack.length - 1] : null;
    if (t.tag === "G") {
      if (top && spanCloses(top, t.text)) {
        stack.pop();
        if (top.kind === "C") { this.lit("</code>"); continue; }
        if (top.kind === "A" || top.kind === "M") {
          var m = /^\]\[([0-9A-Za-z])\]$/.exec(t.text);
          var g = { kind: top.kind, text: top.raw,
                    label: m ? m[1] : top.raw };
          this.emitLink(g, top.kind === "M");
          continue;
        }
        this.out.splice(top.mark, 0, "<" + SPAN_TAG[top.kind] + ">");
        this.lit("</" + SPAN_TAG[top.kind] + ">");
        continue;
      }
      var kind = SPAN_OPEN[t.text];
      if (kind === undefined) { this.escf(t.text); continue; }
      if (top && (top.kind === "A" || top.kind === "M")) { top.raw += t.text; continue; }
      if (kind === "C") this.lit("<code>");
      stack.push({ kind: kind, open: t.text, mark: this.out.length, raw: "" });
      continue;
    }
    var body = t.tag === "E" ? t.text.slice(1) : t.text;   // escape: drop the \
    if (top && (top.kind === "A" || top.kind === "M")) { top.raw += body; continue; }
    this.escf(body);
  }
  while (stack.length) {                       // unmatched: emit what is buffered
    var f = stack.pop();
    if (f.kind === "C") this.lit("</code>");
    else if (f.kind === "A" || f.kind === "M") this.escf(f.open + f.raw);
    else this.out.splice(f.mark, 0, f.open);
  }
};

//  ---- page metadata (MARK-011: OG cards) --------------------------------
//  Strip inline StrictMark to plain text for a <title>/description/og value.
function plain(s) {
  return s
    .replace(/!\[[^\]]*\](\[[^\]]*\]|\([^)]*\))?/g, "")    // images
    .replace(/\[([^\]]+)\](\[[^\]]*\]|\([^)]*\))/g, "$1")  // [text][l] / [text](url)
    .replace(/\[([^\]]+)\]/g, "$1")                        // shortcut [Page]
    .replace(/~~([^~]+)~~/g, "$1")
    .replace(/[*_`]/g, "")
    .replace(/\s+/g, " ").trim();
}

//  pageMeta(src, fallback) -> { title, intro, image } — the ONE extractor both
//  `mark` and `rss` share.  title = the FIRST heading opener (`#`..`######`),
//  markup stripped (fallback when there is none); intro = the first prose
//  paragraph (blank/heading/image-only/reference-def lines skipped), stripped;
//  image = the first post image's url — reference-style labels resolved through
//  the same `[key]: url` definitions, the layout `#fragment` stripped, still
//  page-relative ("" when the post carries no image).  No fs, no absolute url:
//  the caller turns `image` absolute via siteBase + absImage.
function pageMeta(src, fallback) {
  var lines = src.split(/\r?\n/);
  //  reference definitions ([key]: url), same scan as collectRefs.
  var refs = {};
  for (var i = 0; i < lines.length; i++) {
    var rm = /^ *\[([^\]]+)\]:[ \t]*([^ \t"\r]+)/.exec(lines[i]);
    if (rm && !(rm[1] in refs)) refs[rm[1]] = rm[2];
  }
  //  title: the first heading opener at any level (`#` through `######`).
  var title = fallback || "";
  for (i = 0; i < lines.length; i++) {
    var hm = /^#{1,6}\s+(.*\S)\s*$/.exec(lines[i]);
    if (hm) { title = plain(hm[1]); break; }
  }
  //  intro: the first prose paragraph — independent of whether a heading was
  //  found (the old rss `metaOf` started its scan at the title loop's index,
  //  so a missed `##` opener ran it to EOF and collected nothing).
  var intro = "";
  for (i = 0; i < lines.length; i++) {
    var ln = lines[i];
    if (/^\s*$/.test(ln)) { if (intro) break; else continue; }
    if (/^#{1,6}\s/.test(ln)) continue;              // heading
    if (/^\s*!\[/.test(ln)) continue;                // image-only line
    if (/^\s*\[[^\]]+\]:\s/.test(ln)) continue;      // reference definition
    //  the HEAD block is not prose: a ticket's `Now:`/`Rep:` pairs (the ONE
    //  matcher, shared/ticketpage.js) and a page's `-- Author` byline.
    if (page.isMetaPair(ln)) continue;
    if (/^\s*--\s+\S/.test(ln)) continue;            // byline
    intro += (intro ? " " : "") + ln.trim();
  }
  //  first image: reference `![alt][label]` or inline `![alt](url)`.
  var image = "";
  var im = /!\[[^\]\n]*\](?:\[([0-9A-Za-z])\]|\(([^)\s]+)[^)]*\))/.exec(src);
  if (im) {
    var url = im[1] !== undefined ? refs[im[1]] : im[2];
    if (url) image = url.split("#")[0];              // drop the layout #fragment
  }
  return { title: title, intro: plain(intro), image: image };
}

//  absImage(site, pageRel, rel) -> an ABSOLUTE image url, or "" when there is
//  no site base or no image.  `rel` is resolved against the page's directory
//  (a leading-`/` rel is already root-absolute); OG scrapers require absolute.
function absImage(site, pageRel, rel) {
  if (!site || !rel) return "";
  if (rel[0] === "/") return site + normPath(rel);
  var i = pageRel.lastIndexOf("/");
  var dir = i < 0 ? "" : pageRel.slice(0, i);
  return site + "/" + normPath((dir ? dir + "/" : "") + rel);
}

//  headMeta(meta, site, outRel) -> the head annotation values renderDoc emits.
//  `site` is siteBase(html) ("" when html/CNAME is absent — then og:url and
//  og:image are dropped, but title/og:title/og:description still ship).
function headMeta(meta, site, outRel) {
  return {
    title: meta.title,
    intro: meta.intro,
    url:   site ? site + "/" + outRel : "",
    image: absImage(site, outRel, meta.image),
  };
}

module.exports = { renderDoc: renderDoc, esc: esc, scanInline: scanInline,
                   Renderer: Renderer, pageMeta: pageMeta, plain: plain,
                   absImage: absImage, headMeta: headMeta };

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
  //  4-char markers in the group after the indents (padded, any column):
  //  heading `#`, quote `>`, todo `-[x]`, ulist `-`, olist `N.`.
  var slot = line.substr(i, 4);
  //  heading (DOG-024): a run of 1-4 '#' padded with spaces to fill the quad,
  //  in any column, exactly like quote/bullet; the level is the count of '#'.
  //  A full quad self-delimits, so `####` needs no gap space and `  # ` is h1.
  var head = slot.length === 4 ? /^ *(#{1,4}) *$/.exec(slot) : null;
  if (head) { b.heading = head[1].length; b.content = i + 4; return b; }
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
//  `![alt][l]` opening a paragraph, plus the space that joined the next line in.
var LEAD_IMAGE = /^!\[([^\]\n]+)\]\[([0-9A-Za-z])\]\s*/;

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
      //  An image opening the paragraph becomes a captioned <figure>.  <figure>
      //  is flow content, so it cannot nest in <p> — hoist it out ahead of the
      //  paragraph; a floated figure escapes the <p> box anyway, and an image
      //  alone in its paragraph then needs no <p> at all.
      var body = st.para, lead = LEAD_IMAGE.exec(body);
      if (lead) { rd.emitFigure(lead[1], lead[2]); body = body.slice(lead[0].length); }
      if (/\S/.test(body)) { rd.lit("<p>\n"); rd.inline(body); rd.lit("\n</p>\n"); }
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
      //  DOG-024: a full marker quad self-delimits, so a following gap
      //  space belongs to the content — trim it before rendering.
      var hc = linec.slice(b.content).replace(/^ +/, "");
      if (b.heading === 1) { st.h1Seen = true; st.opener = true; }
      var tag = "h" + b.heading;
      rd.lit("<" + tag + ">"); rd.inline(hc); rd.lit("</" + tag + ">\n");
      continue;
    }
    if (b.marker === "ulist" || b.marker === "olist") {
      enterList(b.depth, b.marker === "olist", linec.slice(b.content).replace(/^ +/, ""));
      st.opener = false; continue;
    }
    if (b.marker === "quote") { enterQuote(b.depth, linec.slice(b.content).replace(/^ +/, "")); st.opener = false; continue; }
    if (b.marker === "todo") { enterTodo(b.depth, b.todo, linec.slice(b.content).replace(/^ +/, "")); st.opener = false; continue; }

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
  //  MARK-011: the head <title> and the OG card come from opts.meta (headMeta);
  //  `title` is the filename-stem fallback for a post with no heading.
  var meta = (opts && opts.meta) || {};
  var docTitle = (meta.title && meta.title.length) ? meta.title
               : (title && title.length ? title : "wiki");
  rd.lit('<!DOCTYPE html>\n<html lang="en">\n<head>\n');
  rd.lit('<meta charset="utf-8">\n');
  rd.lit('<meta name="viewport" content="width=device-width,initial-scale=1">\n');
  rd.lit("<title>");
  rd.escf(docTitle);
  rd.lit("</title>\n");
  //  Open Graph annotation block (Bluesky/Slack/Mastodon link cards).  og:url
  //  and og:image need an ABSOLUTE base — omitted when html/CNAME is absent.
  rd.lit('<meta property="og:title" content="'); rd.escf(docTitle); rd.lit('">\n');
  if (meta.intro && meta.intro.length) {
    rd.lit('<meta property="og:description" content="'); rd.escf(meta.intro); rd.lit('">\n');
  }
  rd.lit('<meta property="og:type" content="article">\n');
  if (meta.url) { rd.lit('<meta property="og:url" content="'); rd.escf(meta.url); rd.lit('">\n'); }
  if (meta.image) { rd.lit('<meta property="og:image" content="'); rd.escf(meta.image); rd.lit('">\n'); }
  if (opts && opts.head) rd.lit(opts.head);
  rd.lit("</head>\n<body>\n");
  if (opts && opts.body) rd.lit(opts.body);
  rd.collectRefs(src);
  renderBlocks(rd, src);
  if (opts && opts.foot) rd.lit(opts.foot);      // footer.html, before </body>
  rd.lit("</body>\n</html>\n");
  return rd.out.join("");
}
