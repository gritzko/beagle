//  rss — the syndication verb: render a StrictMark page to HTML (like `mark`)
//  AND upsert the page as an item in the static RSS 2.0 feed html/feed.rss.
//
//    jab rss blog/away.mkd     (anywhere in the project) -> html/blog/away.html
//                              + an <item> in html/feed.rss
//
//  Mirrors mark's render (see ../mark/mark.js): the PROJECT root
//  (shared/project.js) fixes the layout, the tree-relative path is mirrored
//  into `<root>/html/` with .mkd->.html.  Then the item is upserted (matched
//  by <guid>, newest-first) into html/feed.rss — WARN + create if absent.
//  pubDate is the source file's mtime; description is the post's intro
//  paragraph.  Pure JS; the render lives in ../mark/render.js.  See [MARK].
"use strict";

const render = require("../mark/render.js");
const project = require("../../shared/project.js");

function readFile(p) { return utf8.Decode(io.mmap(p, "r").data()); }
function tryRead(p) { try { return readFile(p); } catch (e) { return ""; } }
function baseName(s) { const i = s.lastIndexOf("/"); return i < 0 ? s : s.slice(i + 1); }
function dirName(s) { const i = s.lastIndexOf("/"); return i <= 0 ? "" : s.slice(0, i); }
function stemOf(s) { const m = /^(.*)\.(mkd|md)$/.exec(s); return m ? m[1] : s; }

function writeFile(p, text) {
  const dir = dirName(p);
  if (dir) io.mkdir(dir);                         // FILEMakeDirP: parents, idempotent
  const bytes = utf8.Encode(text);
  const fd = io.open(p, "c");
  try { const b = io.buf(bytes.length + 8); b.feed(bytes); io.writeAll(fd, b); }
  finally { io.close(fd); }
}

//  ron60 BigInt (io.stat mtime, JS-042) -> ms since epoch.  Pure inverse of the
//  RON calendar packing (port of ulog.js ronToMs): 6-bit base64 fields.
function ronToMs(r) {
  r = BigInt(r);
  const d = (k) => Number((r >> BigInt(k * 6)) & 63n);
  const yy = d(9) * 10 + d(8);
  const mon = d(7), day = d(6) * 10 + d(5);
  const hh = d(4), mm = d(3), ss = d(2);
  const ms = d(1) * 64 + d(0);
  return Date.UTC(2000 + yy, mon - 1, day, hh, mm, ss, ms);
}

//  RFC-822 pubDate from the source file's mtime; wall clock on any failure.
function pubDate(srcPath) {
  try { return new Date(ronToMs(io.stat(srcPath).mtime)).toUTCString(); }
  catch (e) { return new Date(ronToMs(ron.now())).toUTCString(); }
}

//  Title = the H1 opener text; intro = the first prose paragraph, StrictMark
//  markup stripped to plain text (image-only and reference-def lines skipped).
function metaOf(src, fallback) {
  const lines = src.split(/\r?\n/);
  let title = fallback, i = 0;
  for (; i < lines.length; i++) {
    const m = /^#\s+(.*\S)\s*$/.exec(lines[i]);
    if (m) { title = plain(m[1]); i++; break; }
  }
  let intro = "";
  for (; i < lines.length; i++) {
    const ln = lines[i];
    if (/^\s*$/.test(ln)) { if (intro) break; else continue; }
    if (/^!\[/.test(ln)) continue;                // image-only line
    if (/^\s*\[[^\]]+\]:\s/.test(ln)) continue;   // reference definition
    intro += (intro ? " " : "") + ln.trim();
  }
  return { title: title, intro: plain(intro) };
}

//  Strip inline StrictMark to plain text for the feed description/title.
function plain(s) {
  return s
    .replace(/!\[[^\]]*\](\[[^\]]*\]|\([^)]*\))?/g, "")   // images
    .replace(/\[([^\]]+)\](\[[^\]]*\]|\([^)]*\))/g, "$1")  // [text][l] / [text](url)
    .replace(/\[([^\]]+)\]/g, "$1")                        // shortcut [Page]
    .replace(/~~([^~]+)~~/g, "$1")
    .replace(/[*_`]/g, "")
    .replace(/\s+/g, " ").trim();
}

//  Site base URL, e.g. https://replicated.live — from html/CNAME, else "".
function siteBase(base) {
  const cname = tryRead(base + "/CNAME").trim().split(/\s+/)[0];
  return cname ? "https://" + cname : "";
}

//  A fresh empty RSS 2.0 skeleton.  `<!--items-->` marks where items go; the
//  span from there to </channel> is the machine-editable item list.
function emptyFeed(base) {
  const home = (siteBase(base) || "") + "/";
  return '<?xml version="1.0" encoding="UTF-8"?>\n' +
    '<rss version="2.0">\n<channel>\n' +
    "<title>" + render.esc(siteBase(base).replace(/^https?:\/\//, "") || "feed") + "</title>\n" +
    "<link>" + render.esc(home) + "</link>\n" +
    "<description>Syndicated StrictMark pages</description>\n" +
    "<!--items-->\n</channel>\n</rss>\n";
}

//  Upsert `item` (an <item>...</item> string) into feed XML, matched by guid,
//  newest-first.  Returns the new feed text.
function upsert(feed, item, guid) {
  const head = feed.slice(0, feed.indexOf("<!--items-->") + "<!--items-->".length);
  const tail = feed.slice(feed.indexOf("</channel>"));
  const body = feed.slice(head.length, feed.length - tail.length);
  const items = (body.match(/<item>[\s\S]*?<\/item>/g) || [])
    .filter((it) => it.indexOf("<guid>" + guid + "</guid>") < 0);
  items.unshift(item);
  return head + "\n" + items.join("\n") + "\n" + tail;
}

function syndicate(arg) {
  //  Fixed layout: the arg counts from the project root — `//name/…` reads the
  //  work/<name> worktree, a bare path the main tree; p.tree anchors link probing.
  const p = project.resolve(String(arg));
  const rel = p.rel;
  if (!rel || !/\.(mkd|md)$/.test(rel)) {
    io.log("rss: needs a .mkd/.md path\n  try: rss blog/page.mkd | rss //WT/blog/page.mkd\n");
    throw "RSSARG";
  }
  const root = p.tree;
  const srcPath = p.abs;
  let src;
  try { src = readFile(srcPath); }
  catch (e) { io.log("rss: cannot read " + srcPath + "\n"); throw "RSSARG"; }

  //  1) render the page, exactly like mark, into <root>/html.  head/banner/
  //  footer chrome is read from html/ (where we write); links anchor to `root`.
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
  io.log("rss: wrote html/" + outRel + "\n");

  //  2) upsert the feed item.  WARN + create html/feed.rss if it is absent.
  const feedPath = base + "/feed.rss";
  let feed;
  try { feed = readFile(feedPath); }
  catch (e) {
    io.log("rss: warning: html/feed.rss not found — creating it\n");
    feed = emptyFeed(base);
  }

  const meta = metaOf(src, stemOf(baseName(rel)));
  const link = (siteBase(base) || "") + "/" + outRel;
  const guid = link || outRel;
  const item =
    "<item>\n" +
    "<title>" + render.esc(meta.title) + "</title>\n" +
    "<link>" + render.esc(link) + "</link>\n" +
    "<guid>" + render.esc(guid) + "</guid>\n" +
    "<pubDate>" + render.esc(pubDate(srcPath)) + "</pubDate>\n" +
    "<description>" + render.esc(meta.intro) + "</description>\n" +
    "</item>";

  writeFile(feedPath, upsert(feed, item, render.esc(guid)));
  io.log("rss: feed.rss <- " + guid + "\n");
}

function rss() {
  const args = Array.prototype.slice.call(arguments).filter(function (a) {
    return a != null && String(a).length && String(a)[0] !== "-";
  });
  if (args.length === 0) { io.log("usage: rss blog/page.mkd | rss //WT/blog/page.mkd ...\n"); throw "RSSARG"; }
  for (let i = 0; i < args.length; i++) syndicate(args[i]);
}

rss.jab = "args";
module.exports = rss;
