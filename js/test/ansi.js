"use strict";
// ansi colour helper (pure JS) + io.isatty gate.
function fail(m) { throw "FAIL " + m; }
function eq(a, b, m) { if (a !== b) fail(m + ": " + JSON.stringify(a) + " !== " + JSON.stringify(b)); }

eq(ansi.green("ok"), "\x1b[32mok\x1b[0m", "green");
eq(ansi.bold("x"), "\x1b[1mx\x1b[0m", "bold");
eq(ansi.grey("y"), "\x1b[90my\x1b[0m", "grey");
eq(ansi.sgr(31), "\x1b[31m", "raw sgr");
eq(ansi.reset, "\x1b[0m", "reset");

// io.isatty returns a boolean (false under ctest's pipe)
if (typeof io.isatty(io.stdout) !== "boolean") fail("isatty type");
if (typeof io.isatty(io.stderr) !== "boolean") fail("isatty stderr type");

io.log("ansi.js OK");
