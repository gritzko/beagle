//  mark — the publication verb: render a StrictMark page to standalone HTML.
//
//    jab mark wiki/StrictMark.mkd      (anywhere in the project) -> html/wiki/StrictMark.html
//    jab mark //BE-123/wiki/Page.mkd   same page, read from the BE-123 worktree
//
//  The PROJECT root (shared/project.js: the ancestor with .be/.git + meta/)
//  fixes the layout ([/wiki/Project]): sources count from that root (an
//  authority names a `work/` worktree), the render mirrors the tree-relative path
//  into `<root>/html/`, rewriting .mkd->.html.  The <head>/<body>/footer
//  injects are html/'s `head.html` / `banner.html` / `footer.html` — the
//  published site's chrome, read from where we write.  Asset links
//  (stylesheet, images) are probed under html/ and any missing one is WARNED
//  (never fatal).  Pure JS: the render lives in ./render.js.  See [MARK], BE-029.
"use strict";

const render = require("./render.js");
const project = require("../../shared/project.js");

function readFile(p) { return utf8.Decode(io.mmap(p, "r").data()); }
function tryRead(p) { try { return readFile(p); } catch (e) { return ""; } }
function baseName(s) { const i = s.lastIndexOf("/"); return i < 0 ? s : s.slice(i + 1); }
function dirName(s) { const i = s.lastIndexOf("/"); return i <= 0 ? "" : s.slice(0, i); }
function stemOf(s) { const m = /^(.*)\.(mkd|md)$/.exec(s); return m ? m[1] : s; }

function writeFile(p, text) {
  const dir = dirName(p);
  if (dir) io.mkdir(dir);                       // FILEMakeDirP: parents, idempotent
  const bytes = utf8.Encode(text);
  const fd = io.open(p, "c");
  try { const b = io.buf(bytes.length + 8); b.feed(bytes); io.writeAll(fd, b); }
  finally { io.close(fd); }
}

//  Warn (never fail) on a referenced site-absolute asset (`/assets/...`) that is
//  absent under html/ — the published site root.  Covers the stylesheet + images.
function checkAssets(html, base) {
  const seen = {};
  const re = /(?:href|src)="(\/[^"]*)"/g;
  let m;
  while ((m = re.exec(html)) !== null) {
    const rel = m[1];
    if (rel in seen) continue; seen[rel] = 1;
    if (/^\/\//.test(rel)) continue;            // //host — external, skip
    try { io.stat(base + rel); }
    catch (e) { io.log("mark: warning: asset not found: " + rel + "\n"); }
  }
}

function renderOne(arg) {
  //  Fixed layout: the arg counts from the project root — `//name/…` reads the
  //  work/<name> worktree, a bare path the main tree; p.tree anchors link probing.
  const p = project.resolve(String(arg));
  const rel = p.rel;                            // wiki/StrictMark.mkd
  if (!rel || !/\.(mkd|md)$/.test(rel)) {
    io.log("mark: needs a .mkd/.md path\n  try: mark wiki/page.mkd | mark //WT/dir/page.mkd\n");
    throw "MARKARG";
  }
  const root = p.tree;
  const srcPath = p.abs;
  let src;
  try { src = readFile(srcPath); }
  catch (e) { io.log("mark: cannot read " + srcPath + "\n"); throw "MARKARG"; }

  //  head/banner/footer chrome is read from <root>/html (the site root we write
  //  into), not the source tree; link .mkd->.html resolution anchors to `root`.
  const base = p.root + "/html";
  const opts = {
    head: tryRead(base + "/head.html"),
    body: tryRead(base + "/banner.html"),
    foot: tryRead(base + "/footer.html"),
    root: root,
    exists: function (r) { try { return !!io.stat(root + "/" + r); } catch (e) { return false; } },
  };
  const html = render.renderDoc(src, stemOf(baseName(rel)), opts);

  const outRel = rel.replace(/\.(mkd|md)$/, ".html");
  writeFile(base + "/" + outRel, html);
  io.log("mark: wrote html/" + outRel + "\n");
  checkAssets(html, base);
}

function mark() {
  const args = Array.prototype.slice.call(arguments).filter(function (a) {
    return a != null && String(a).length && String(a)[0] !== "-";
  });
  if (args.length === 0) { io.log("usage: mark dir/page.mkd | mark //WT/dir/page.mkd ...\n"); throw "MARKARG"; }
  for (let i = 0; i < args.length; i++) renderOne(args[i]);
}

mark.jab = "args";
module.exports = mark;
