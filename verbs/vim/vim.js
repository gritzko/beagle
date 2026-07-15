//  verbs/vim/vim.js — BE-047: `vim`/`nvim` — open the editor NAMED BY THE VERB
//  on a context-resolved file; ONE implementation, verbs/nvim/nvim.js aliases it.
//
//  The file arg rides the STANDARD context machinery (BE-030/BE-032): a `//X`
//  authority on arg 0 was already hoisted by the loop (authorityRepo); the
//  relative arg resolves via discover.argRel (context-dir aware, NAVESCAPE on a
//  `..` climb) + wtpath (resolve-backed, tree-confined).  A BARE call (`:vim`
//  in the pager) edits the NAV CONTEXT's own path — the currently open file —
//  via discover.resolve(context, "") (be.context, threaded by loop.cli).
//
//  TTY handover: io.spawnFds hands the CONTROLLING terminal (/dev/tty, the
//  bro.js pattern — stdin/stdout may be pipes) to the child as fd 0/1, else -1
//  inherits; io.reap WAITS for the editor to exit.  The pager suspends its raw
//  mode around this verb (fn.tty marker → pager._editSpell) and re-drives the
//  view after, so the edit shows.
"use strict";

const discover = require("../../core/discover.js");
const pathlib = require("../../shared/util/path.js");

//  BE-047: ONE uniform miss line + throw (the done.js idiom).
function miss(name, why, code) { io.log(name + ": " + why + "\n"); throw code; }

//  BE-047: the absolute fs path of the file to edit — bare → the context's own
//  path (resolve(context, "")); one arg → argRel/wtpath under the scoped repo.
function target(name, args) {
  if (args.length > 1) miss(name, "one file at a time", "VIMARGS");
  if (args.length === 1 && String(args[0]) !== "") {
    const raw = String(args[0]);
    const repo = (typeof be !== "undefined" && be.repo) || null;
    if (repo) return discover.wtpath(repo.wt, discover.argRel(repo, raw));
    return pathlib.wtJoin(io.cwd(), raw);      // repo-less CLI: cwd-confined
  }
  //  DIS-061: no file operand → the pager's stashed CURRENT file (be.prev_uri,
  //  normalized to a typed-arg-shaped URI for a single-hunk FILE view); an empty
  //  stash falls back to today's behaviour — the nav CONTEXT's own path.  The
  //  file-focus lives HERE (the pager welds nothing); both resolve identically.
  const prev = (typeof be !== "undefined" && be && be.prev_uri) || "";
  const ctx = prev || ((typeof be !== "undefined" && be.context) || "");
  let u = null; try { u = uri._parse(ctx); } catch (e) { u = null; }
  if (!u || (u.authority === undefined && !u.path))
    miss(name, "no file (no arg, no context path)", "VIMNONE");
  return discover.resolve(u, "");              // the currently open file/dir
}

function vim() {
  //  BE-047: the editor binary IS the verb's own name (vim → vim, nvim → nvim).
  const name = (typeof be !== "undefined" && be.verb) || "vim";
  const file = target(name, Array.prototype.slice.call(arguments));
  //  Hand the controlling terminal over; no /dev/tty (ctest) → -1 = inherit.
  let fd = null;
  try { fd = io.open("/dev/tty", "rw"); } catch (e) { fd = null; }
  const pass = fd === null ? -1 : fd;
  let pid;
  try { pid = io.spawnFds(name, [name, file], pass, pass); }
  catch (e) {
    if (fd !== null) { try { io.close(fd); } catch (e2) {} }
    miss(name, "cannot spawn '" + name + "' (" + e + ")", "VIMSPAWN");
  }
  let r;
  try { r = io.reap(pid); }                    // WAIT for the editor to exit
  finally { if (fd !== null) { try { io.close(fd); } catch (e) {} } }
  if (r.signal !== undefined) miss(name, "killed by signal " + r.signal, "VIMDIED");
  if (r.code !== 0) miss(name, "exit " + r.code, "VIMEXIT");
}

vim.jab = "args";
//  BE-047: the TERMINAL marker — the pager must suspend raw mode around this
//  verb (loop._isTty → pager isTty → _editSpell's cook/drive/raw/refresh).
vim.tty = true;
module.exports = vim;
