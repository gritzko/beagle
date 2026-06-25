//  bro.js — `bro` as a loop HANDLER (JAB-bro).  The syntax-highlighting
//  file/dir VIEWER, relocated from the standalone entry `be/bro.js` into the
//  loop's by-verb layout so `jab bro <args>` routes through the resident loop
//  (the bare-verb resolver lowers `jab bro x.c` to `jab loop.js bro x.c`).
//  Reproduces native `bro --plain` BYTE-FOR-BYTE: per URI arg the banner
//  `hunk <uri>` + the file text (or the dir listing), via the render lib
//  view/bro.js's plain sink (BROPlain, the !BRO_COLOR path).
//
//  VIEW projection (not a staging verb): bro OWNS its content — it reads each
//  file/dir arg, renders the plain hunk, and writes the bytes to STDOUT
//  directly (io.writeAll(1, …)).  It does NOT push rows through ctx.out (the
//  emit sink is for the `<date> <verb> <uri>` log table; bro emits raw text).
//
//  LOOP SHAPE: the seed scatters one row per positional arg, so the loop calls
//  this handler N times — but bro folds the WHOLE batch in ONE pass (the DELETE
//  pattern: a ctx._broDone guard) so the multi-file banner order + the exit
//  code (BE-002: any-given-but-none-opened → non-zero) span the full arg list.
//
//  Exit (BE-002) is via THROW, not process.exit — a handler must not exit the
//  process (the loop's ONE edge-catch maps a thrown refusal to the non-zero
//  process exit + stderr diag).  No args → BROUSAGE; args but none opened →
//  BRONONE; otherwise return normally (exit 0).

"use strict";

//  The shared render lib lives at the be/ ROOT (view/bro.js).  Be-relative
//  require (NOT __dirname/argv[1] — the handler is require'd, never the entry):
//  the upward be/-scan resolves it against the be/ root via the self-loop, the
//  same way core/emit.js requires view/render.js.
const bro = require("view/bro.js");

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

//  `bro` as a loop HANDLER.  Folds the WHOLE batch on its FIRST row
//  (ctx._broDone guard) — the seed lowers each path arg to its own row, but
//  bro's multi-banner order + the BE-002 exit class span the full arg list, so
//  process every arg once and no-op on every later row.
module.exports = function handle(row, ctx) {
  if (ctx._broDone) return;
  ctx._broDone = true;

  //  The raw positional file args (flags already split off in cli()).
  const args = (ctx && ctx.args) || [];

  //  No URI args → usage + non-zero exit (native bro's BE-002 usage path).
  if (args.length === 0) {
    writeStderr("Usage: bro [URI...]\n");
    throw "BROUSAGE";
  }

  let anyOpened = false;
  const out = [];

  //  Per URI: parse, stat the bare path (frag stripped), dispatch file vs dir.
  //  Mirrors the OLD be/bro.js main() loop (BROExec's): a dir → BROListDir, a
  //  file → mmap+tokenize; a miss prints `bro: cannot open …` to STDERR and
  //  continues (no anyOpened bump → BE-002 if NONE open).
  for (const arg of args) {
    const u = uri._parse(arg);
    const path = u.path || arg;              // u->path is the fragment-less path
    const fp = bro.fsPath(path);

    let st;
    try { st = io.stat(fp); }
    catch (e) {
      writeStderr("bro: cannot open " + path + ": FILENONE\n");
      continue;
    }

    let hunk = null;
    try {
      if (st.kind === "dir") hunk = bro.buildDirHunk(arg, fp);
      else hunk = bro.buildFileHunk(arg, fp);
    } catch (e) {
      writeStderr("bro: cannot open " + path + ": " + e + "\n");
      continue;
    }

    anyOpened = true;
    if (hunk !== null) out.push(bro.plainHunk(hunk));   // empty dir → no banner
  }

  //  Concatenate every hunk's plain rendering and write once to stdout.
  let total = 0;
  for (const b of out) total += b.length;
  const all = new Uint8Array(total);
  let off = 0;
  for (const b of out) { all.set(b, off); off += b.length; }
  if (all.length > 0) writeStdout(all);

  //  BE-002: at least one URI given but NONE opened → non-zero exit so callers
  //  see the failure (the per-URI `cannot open …` lines already explained).
  //  THROW (not process.exit) — the loop edge maps it to the process exit code.
  if (!anyOpened) throw "BRONONE";
};
