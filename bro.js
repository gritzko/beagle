//  bro.js — the syntax-highlighting file/dir VIEWER as a repo-local JS
//  extension (JS-053 TODO#2).  Self-contained: URI args → io.mmap + tok.parse
//  (a file → one hunk) or io.readdir (a dir → one 'F' listing hunk) → the
//  render pipeline in bin/lib/bro.js → the plain sink.  No stdin TLV, no forked
//  C producers — bro.js owns its content.  Builds on the landed tty leaf
//  (tty.size); the interactive raw-mode TUI is TODO#3.
//
//  This increment is the NON-interactive renderer: output matches
//  `bro --plain` BYTE-FOR-BYTE — the banner `hunk <uri>` + the file text (or
//  the dir listing), per the BROPlain `!BRO_COLOR` path.
//
//  Usage:  bro.js [--plain] <URI...>          (jabc bin/bro.js file#42 dir/ …)
//          a file → cat; file#42 / file#Sym → cat (the frag rides the banner);
//          a dir/ → list.  Missing URIs print `bro: cannot open …` to stderr
//          and the process exits non-zero (parity with native bro's BE-002).

"use strict";

const self = process.argv[1];
const here = self.slice(0, self.lastIndexOf("/"));
const bro = require(here + "/lib/bro.js");

//  Width for soft-wrap / status bar: the tty cols, or 80 when not a tty
//  (the BROPlain default — the plain sink itself never wraps, so this only
//  matters for the colour/TUI path that builds on indexRows).
function termCols() {
  try { if (io.isatty(1)) return tty.size(1).cols || 80; } catch (e) {}
  return 80;
}

function writeStdout(bytes) {
  const b = io.buf(bytes.length + 8);
  b.feed(bytes);
  io.writeAll(1, b);
}

function writeStderr(str) {
  const bytes = utf8.Encode(str);
  const b = io.buf(bytes.length + 8);
  b.feed(bytes);
  io.writeAll(2, b);
}

function main() {
  //  --tlv/--color/--plain are the universal HUNK output flags; this viewer
  //  only emits the plain sink for now, so they're accepted + ignored (the
  //  colour TUI is TODO#3).  Everything non-flag is a URI arg.
  const args = [];
  for (const a of process.argv.slice(2)) {
    if (a === "--plain" || a === "--color" || a === "--tlv" || a === "--ansi" ||
        a === "--16" || a === "--dark" || a === "--light") continue;
    args.push(a);
  }

  if (args.length === 0) {
    writeStderr("Usage: bro.js [URI...]\n");
    process.exit(2);
  }

  void termCols();   // resolve width (status-bar/wrap scaffolding for TODO#3)

  let anyOpened = false;
  let lastErr = "";
  const out = [];

  //  Per URI: parse, stat the bare path (frag stripped), dispatch file vs dir.
  //  Mirrors BROExec's loop: a dir → BROListDir, a file → mmap+tokenize; a miss
  //  prints `bro: cannot open …` and sets the non-zero exit.
  for (const arg of args) {
    const u = uri._parse(arg);
    const path = u.path || arg;              // u->path is the fragment-less path
    const fp = bro.fsPath(path);

    let st;
    try { st = io.stat(fp); }
    catch (e) {
      writeStderr("bro: cannot open " + path + ": FILENONE\n");
      lastErr = "FILENONE";
      continue;
    }

    let hunk = null;
    try {
      if (st.kind === "dir") hunk = bro.buildDirHunk(arg, fp);
      else hunk = bro.buildFileHunk(arg, fp);
    } catch (e) {
      writeStderr("bro: cannot open " + path + ": " + e + "\n");
      lastErr = String(e);
      continue;
    }

    anyOpened = true;
    if (hunk !== null) out.push(bro.plainHunk(hunk));   // empty dir → no banner
  }

  //  Concatenate every hunk's plain rendering and write once.
  let total = 0;
  for (const b of out) total += b.length;
  const all = new Uint8Array(total);
  let off = 0;
  for (const b of out) { all.set(b, off); off += b.length; }
  if (all.length > 0) writeStdout(all);

  //  BE-002: at least one URI given but NONE opened → exit non-zero so callers
  //  see the failure (the per-URI `cannot open …` lines already explained).
  if (!anyOpened) process.exit(1);
}

main();
