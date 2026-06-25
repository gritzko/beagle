//  verbs/cat/cat.js — JAB-020: cat:<path>[?ref] — print a file's bytes with
//  SYNTAX highlighting, NO diff.  Ruling (gritzko): cat: shows the file's OWN
//  bytes; the C cat:'s baseline-diff is a misnomer — for a diff there is diff:.
//  So: --plain = verbatim bytes, --color = syntax-painted (dog/THEME via tok),
//  --tlv = HUNK records.  Reuses the SHARED binding render (view/bro.js
//  renderHunkLog) — the same sink grep/spot/regex feed.  Pure JS over
//  libabc+libdog; no dog binary, no /proc.  Banner names the verb: `cat
//  <path>#L<n>` (per the verb-titled-hunks convention).
"use strict";

const store = require("../../shared/store.js");
const join  = require("../../shared/util/path.js").join;
const bro   = require("../../view/bro.js");
const EMPTY32 = new Uint32Array(0);

const CAP = 1 << 20;   // 1 MiB/hunk cap; a bigger file splits with a #L<n> rebanner

//  Read a wt file's bytes (NUL-safe); absent/non-regular → null, empty → [].
function readFileBytes(full) {
  let st;
  try { st = io.lstat(full); } catch (e) { return null; }
  if (st.kind !== "reg") return null;
  if (st.size === 0) return new Uint8Array(0);
  let fd;
  try { fd = io.open(full, "r"); } catch (e) { return null; }
  try {
    const b = io.buf(st.size + 16);
    io.readAll(fd, b, st.size);
    return b.data().slice();
  } catch (e) { return null; }
  finally { try { io.close(fd); } catch (e) {} }
}

//  ?ref: resolve ref/branch/sha → commit → tree → the path's blob bytes
//  (mirrors the search view's walkRef; KEEPGetByURI's descend).
function readRefBytes(k, ref, path) {
  let sha = k.resolveRef(ref);
  if (!sha) { try { sha = require("../../core/resolve.js").resolveHex(k, ref); } catch (e) {} }
  if (!sha) return null;
  const treeSha = k.commitTree(sha) || sha;
  let found = null;
  k.readTreeRecursive(treeSha, function (leaf) {
    if (found) return;
    if ((leaf.kind === "f" || leaf.kind === "x") && leaf.path === path) {
      const o = k.getObject(leaf.sha);
      if (o && o.type === "blob") found = o.bytes;
    }
  });
  return found;
}

module.exports = function handle(row, ctx) {
  const mode = (ctx && ctx.mode) || "plain";
  const repo = (ctx && ctx.repo) || null;
  if (!repo) return;

  //  Parse cat:<path>[?ref] off the raw arg (ctx.args carries the whole URI;
  //  a fragment-only seed row lowers to a "." placeholder — see search.js).
  const rawArgs = (ctx && ctx.args && ctx.args.length) ? ctx.args : [row.uri];
  let first = String(rawArgs[0] || "");
  if (first.indexOf("cat:") === 0) first = first.slice(4);
  const u = new URI(first);
  const path = u.path || "";
  const ref  = (u.query && u.query.length) ? u.query : "";
  if (!path) { io.log("cat: needs a path\n  try: cat:<path>\n"); throw "CATNOPATH"; }

  //  Bytes: a `?ref` blob (historic) else the live wt file.  Absent/empty → no
  //  output (no banner), matching the empty-file case.
  const k = store.open(repo.storePath, repo.project);
  let bytes = ref ? readRefBytes(k, ref, path) : readFileBytes(join(repo.wt, path));
  if (bytes == null || bytes.length === 0) return;

  const ext = bro.pathExt(path);            // "js" / "" — drives tok.parse
  const chunks = [];
  let off = 0, line = 1;
  while (off < bytes.length) {
    //  1 MiB hunk, backed up to the last line boundary so a line never splits.
    let end = off + CAP < bytes.length ? off + CAP : bytes.length;
    if (end < bytes.length) {
      let nl = end; while (nl > off && bytes[nl - 1] !== 10) nl--;
      if (nl > off) end = nl;
    }
    const body = bytes.slice(off, end);
    let toks = EMPTY32;
    if (mode !== "plain" && ext) { try { toks = tok.parse(body, ext); } catch (e) { toks = EMPTY32; } }
    const uri = path + "#L" + line;
    const hlog = abc.ram("HUNK", body.length + toks.length * 4 + uri.length + 1024);
    hlog.feed(uri, body, toks, "cat", 0n);   // banner: `cat <path>#L<n>`
    chunks.push(bro.renderHunkLog(hlog, mode));
    for (let i = off; i < end; i++) if (bytes[i] === 10) line++;
    off = end;
  }

  let total = 0; for (const c of chunks) total += c.length;
  if (total) {
    const all = new Uint8Array(total);
    let o = 0; for (const c of chunks) { all.set(c, o); o += c.length; }
    const b = io.buf(total + 8); b.feed(all); io.writeAll(1, b);
  }
};
