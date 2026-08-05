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
//  Require core/loop.js DIRECTLY, never the be/loop.js ENTRY shim: the shim's
//  self-run guard (`argv[1] ends /loop.js -> cli(process.argv)`) re-fires on a
//  fresh require, re-dispatching the OUTER `bro ...` argv (JAB-028).
//  CODE-030: the spell drive + the pager edge live THERE now (loop dispatch),
//  so this edge is one-way; the two handles below keep every call site.
const loop = require("core/loop.js");
const driveSpell = loop.driveSpell, spellCall = loop.spellCall;

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

//  JAB-004: PLAIN verb (`.jab="args"`) — bro OWNS its whole batch in ONE call
//  reading `be` (repo-less file viewer; args ride `arguments`, flags off
//  be.flags).  Own `(row,ctx)` entry fallback removed; bro is now plain-args.
function bro_() {
  //  Plain path: args ride `arguments`, flags/repo/sink read off `be`.
  const _be = (typeof be !== "undefined") ? be : null;
  const flags = (_be && _be.flags) || [];
  broRun(Array.prototype.slice.call(arguments), flags, null);
}

//  JAB-004: the batch driver — args are SPELLs (pager) or file/dir URIs (plain).
//  `ctx` (legacy direct-handler) overrides the global; else read `be`.
function broRun(args, flags, ctx) {

  //  JAB-028: at a real terminal, enter the interactive raw-mode pager (a
  //  scrollable hunk viewport + `:` address bar) instead of the plain dump.
  //  OPEN FORK (a): explicit `jab bro` only — auto-enter for a tty view over
  //  one screen is the unresolved product call, surfaced for the gate.  Piped/
  //  --plain stays the byte-parity plain path below (every parity test intact).
  const wantPager = io.isatty(1) && flags.indexOf("--plain") < 0;
  if (wantPager) {
    //  Each arg is a spell: driveSpell runs a view → tlv hunks, a bare path →
    //  file hunk, or an emit-sink view → its text wrapped as one hunk.  No args
    //  → an empty viewport (NOT a usage error — type a `:` spell to fill it).
    let hunks = [];
    for (const arg of args) {
      try { const h = driveSpell(arg); if (h && h.length) hunks = hunks.concat(h); }
      catch (e) { /* a bad spell just contributes nothing */ }
    }
    //  CODE-030: THE universal-tty pager edge is loop.openPager — the same
    //  /dev/tty open + Pager wiring this block spelled a second time.
    loop.openPager(hunks);
    return;
  }

  //  Non-tty (piped/--plain): the plain dump.  No args here IS a usage error.
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
}

//  JAB-004: opt into the plain-args convention (registry routes fn(...args)).
bro_.jab = "args";
module.exports = bro_;
//  JAB-030: expose driveSpell on the exported fn (a fn IS an object) so the
//  universal-pager edge (core/loop.js _openPager) + the pager wire the SAME
//  address-bar spell drive — ONE spell path, no duplication.  CODE-030: it is
//  loop.js's now, re-exported here so bro's own callers are unchanged.
module.exports.driveSpell = driveSpell;
//  JAB-004: expose the shared-tokenizer spell splitter for the phase-1 driver
//  (test/argline.js) to assert the pager `:` line splits like the CLI.
module.exports._spellCall = spellCall;
