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
const pager = require("views/bro/pager.js");   // JAB-028: the raw-mode TUI
const argline = require("shared/argline.js");  // JAB-004: the shared tokenizer

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

//  Build the hunk OBJECTS (the renderer's model) for the positional args — the
//  same file/dir hunks plainHunk consumes, but kept whole for the TUI viewport.
//  A missing/unopenable arg is skipped (the pager just shows what opened).
function buildHunks(args) {
  const hunks = [];
  for (const arg of args) {
    const u = uri._parse(arg);
    const path = u.path || arg;
    const fp = bro.fsPath(path);
    let st;
    try { st = io.stat(fp); } catch (e) { continue; }
    let h = null;
    try { h = st.kind === "dir" ? bro.buildDirHunk(arg, fp) : bro.buildFileHunk(arg, fp); }
    catch (e) { continue; }
    if (h !== null) hunks.push(h);
  }
  return hunks;
}

//  JAB-004: address-bar line → argv via the SHARED argline splitter (pager `:`
//  splits like the CLI); verb null ⇒ URI/path, object/array arg ⇒ PARAMOBJ.
function spellCall(spell) {
  const r = argline.parse(spell);
  if (r.verb == null) return null;                // a URI/path — not a call
  if (r.args.length === 0) return [r.verb];       // bare verb
  const argv = [r.verb];
  for (const p of r.args) {
    const t = typeof p;
    if (p === null || t === "string" || t === "number" || t === "boolean")
      argv.push(String(p));
    else throw "PARAMOBJ";                         // object/array need the `be` global
  }
  return argv;
}

//  driveSpell(spell) -> hunks: the in-process address-bar drive (JAB-028 TODO#5).
//  Re-enter the resident loop for the typed spell in --tlv mode, capturing its
//  fd-1 'H'-record stream via a reversible io.writeAll hook (no dup2, no spawn,
//  no /proc — pure JS), then reparse the tlv into hunks.  A bare `path#Lnn` (no
//  scheme) lowers to bro's own file hunk; a `<verb>:<uri>` re-enters loop.cli.
//  The outer loop owns argv[1]=loop.js, so the re-entrant cli's require-scan and
//  queue path resolve correctly (sequential re-entry, JAB-004 recursion).
//  BE-039: `context` (optional, `//name`) is the nav scope for a VERB word-call — it
//  rides the reentry opts (loop.cli opts2.context), NOT an arg, so the verb resolves
//  each RAW arg against it; a slot-edit / bare-path drive passes none (cwd repo).
function driveSpell(spell, context) {
  //  JAB-003: a `verb param` / `verb(a,b)` CALL splits to a proper argv; null →
  //  a bare path / scheme:uri → the legacy single-token file/loop drive below.
  const call = spellCall(spell);
  if (!call) {
    const u = uri._parse(spell);
    //  No scheme + a plain path → bro's OWN file/dir hunk (no loop re-entry).
    if (!u.scheme) {
      const h = buildHunks([spell]);
      if (h.length) return h;
    }
  }
  //  Require core/loop.js DIRECTLY, never the be/loop.js ENTRY shim: the shim's
  //  self-run guard (`argv[1] ends /loop.js → cli(process.argv)`) re-fires on a
  //  fresh require, re-dispatching the OUTER `bro …` argv — infinite recursion
  //  (JAB-028).  core/loop.js as a module only exports; no self-run.
  const loop = require("core/loop.js");
  const orig = io.writeAll;
  const chunks = [];
  io.writeAll = function (fd, b) {
    if (fd === 1) { chunks.push(b.data().slice()); return; }
    return orig(fd, b);
  };
  //  JSQUE-020: each cli() now builds its OWN in-memory queue, so the re-entrant
  //  sub-run no longer needs a distinct queue path (the old file-queue shared-
  //  unlink crash is gone); opts2.reentry only marks it so no nested pager opens.
  const argv = call ? ["jab", "loop.js"].concat(call, ["--tlv"])
                    : ["jab", "loop.js", spell, "--tlv"];
  //  BE-039: thread the nav context as CONTEXT (opts2.context) so the verb scopes
  //  its RAW args to that tree — the pager never bakes it into an arg's spelling.
  try { loop.cli(argv, { reentry: true, context: context || undefined }); }
  finally { io.writeAll = orig; }
  let total = 0; for (const c of chunks) total += c.length;
  const tlv = new Uint8Array(total);
  let o = 0; for (const c of chunks) { tlv.set(c, o); o += c.length; }
  //  A hunk-stream view (cat/grep/spot/regex via renderHunkLog) emits 'H'
  //  records → rich hunks.  An emit-sink view (ls/lsr/status/refs, columnar)
  //  emits plain TEXT — wrap that whole output as ONE plain hunk so the pager
  //  still shows it (no toks → no syntax paint, but it browses).  [Until the
  //  mmap-buf sink lands so EVERY view feeds hunks directly — see JAB-029.]
  const hunks = pager.hunksFromTlv(tlv);
  if (hunks.length) return hunks;
  if (total > 0) return [{ uri: spell, verb: "hunk", text: tlv,
                           toks: new Uint32Array(0), kind: "file" }];
  return [];
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
    //  Keystrokes come from the controlling terminal (so input still works when
    //  stdin is a data pipe — API.md's /dev/tty pattern); else tty stdin, then 1.
    let fd = null, own = false;
    try { fd = io.open("/dev/tty", "rw"); own = true; } catch (e) { fd = null; }
    if (fd === null && io.isatty(0)) fd = 0;
    if (fd === null) fd = 1;
    try {
      //  isVerb lets the composer tell a real verb from a bareword path token.
      const loop = require("core/loop.js");
      const p = new pager.Pager(fd, { color: true, driveSpell: driveSpell,
                                      isVerb: loop.isVerb });
      p.setHunks(hunks);
      p.run();
    } finally { if (own) { try { io.close(fd); } catch (e) {} } }
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
//  address-bar spell drive — ONE spell path, no duplication.
module.exports.driveSpell = driveSpell;
//  JAB-004: expose the shared-tokenizer spell splitter for the phase-1 driver
//  (test/argline.js) to assert the pager `:` line splits like the CLI.
module.exports._spellCall = spellCall;
