//  branch.js — SUBS-050: the ONE parsed-branch type + codec ([Submodules]
//  §Store layout).  A branch is ONE internal value `{ title, branch:[segs] }`;
//  text exists only at IO edges (ulog/wtlog/refs rows, wire refs, display
//  labels), parsed ONCE on entry and formatted ONCE on exit.  No ad-hoc
//  indexOf/slice/split surgery at the call sites — everything routes here,
//  which itself works over util/path.js split/merge segment arrays.
//
//    { title:"proj",   branch:[] }                        trunk
//    { title:"proj",   branch:["feature"] }               a primary-wt branch
//    { title:"libabc", branch:[".libdog",".jab","JS-101"] }  a mounted sub
//
//  Dots stay ON the segments (a `.parent` segment keeps its leading dot).
//  Branch paths appear in THREE recorded shapes; parse() accepts them all
//  (read-compat is forever — old rows on disk are never rewritten):
//    absolute  `/libabc/.libdog/.jab/JS-101`   (title head + dotted chain)
//    relative  `.libdog/.jab/JS-101`           (title-stripped legacy rows)
//    plain     `JS-101` / ""                    (primary wt / trunk)

"use strict";

const pathlib = require("./util/path.js");
const split = pathlib.split, merge = pathlib.merge;

//  parse(str, title) → Branch.  Accepts all three recorded shapes (+ a leading
//  `?`).  An ABSOLUTE `/head/...` takes its title from the head segment; a
//  relative-dotted or plain string is re-headed with the passed `title`.
function parse(str, title) {
  let q = str == null ? "" : String(str);
  if (q[0] === "?") q = q.slice(1);
  const t = title == null ? "" : String(title);
  if (q[0] === "/") {
    const segs = split(q);
    return { title: segs.length ? segs[0] : t, branch: segs.slice(1) };
  }
  return { title: t, branch: split(q) };
}

//  isTrunk(br) — YES for a trunk (no branch segments).
function isTrunk(br) {
  return !br || !br.branch || br.branch.length === 0;
}

//  format(br) → the ONE canonical display/record string: a plain `feature`
//  (segment join) for a primary-wt branch, an absolute `/<title>/<seg>/...`
//  when the first segment is a dotted chain head (a mounted sub's synthetic
//  branch), and "" for trunk.
function format(br) {
  const segs = (br && br.branch) || [];
  if (segs.length === 0) return "";
  if (segs[0][0] === ".") return "/" + merge([br.title].concat(segs));
  return merge(segs);
}

//  key(br) → the refs-log key: the title-STRIPPED relative form (a merge of the
//  segments, no title head).  Byte-identical to what today's rows key on
//  (DOGQueryStripProject's output), so existing refs rows still resolve.
function key(br) {
  return merge((br && br.branch) || []);
}

//  sub(parentBr, subTitle) → the child synthetic Branch the mounted sub tracks
//  ([Submodules] bullet 1): the parent's own title heads the dotted chain,
//  followed by the parent's whole branch tail.
function sub(parentBr, subTitle) {
  const pt = (parentBr && parentBr.title) || "parent";
  const pb = (parentBr && parentBr.branch) || [];
  return { title: subTitle, branch: ["." + pt].concat(pb) };
}

//  wireRef(br) → `refs/heads/<key>` (trunk → `refs/heads/main`).  Wire refs are
//  title-STRIPPED (the serve.js form).  GIT-015 defect A: an empty ref segment
//  (`refs/heads//…`, trailing `/`) is a bad target → POSTREF (never sent).
function wireRef(br) {
  const k = key(br);
  const name = "refs/heads/" + ((k && k !== "main") ? k : "main");
  if (/\/\//.test(name) || name[name.length - 1] === "/")
    throw { code: "POSTREF",
            msg: "POSTREF: empty ref segment in `" + name + "` — bad branch target" };
  return name;
}

//  fromWireRef(name, title) → Branch.  `refs/heads/main` → trunk; the head tail
//  re-parses (title-stripped, re-headed with `title` when known).
function fromWireRef(name, title) {
  let n = name == null ? "" : String(name);
  if (n.indexOf("refs/heads/") === 0) n = n.slice("refs/heads/".length);
  else if (n.indexOf("refs/") === 0) n = n.slice("refs/".length);
  if (n === "main") n = "";
  return parse(n, title);
}

//  display(br) → the status/label form `?<format>`.
function display(br) {
  return "?" + format(br);
}

module.exports = { parse: parse, format: format, key: key, sub: sub,
                   wireRef: wireRef, fromWireRef: fromWireRef,
                   isTrunk: isTrunk, display: display };
