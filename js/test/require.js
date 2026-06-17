"use strict";
// Synchronous CommonJS require(): write module files via io, require them.
function fail(m) { throw "FAIL " + m; }
function eq(a, b, m) { if (a !== b) fail(m + ": " + a + " !== " + b); }

function writeFile(path, text) {
  const fd = io.open(path, "c");
  const b = io.buf(text.length * 3 + 16);
  b.feedStr(text);
  io.writeAll(fd, b);
  io.close(fd);
}

// basic export + cache identity
{
  writeFile("/tmp/jabc_req_m.js",
    "module.exports = { add: (a, b) => a + b, name: 'm', dir: __dirname };");
  const m = require("/tmp/jabc_req_m.js");
  eq(m.add(2, 3), 5, "exported fn");
  eq(m.name, "m", "exported value");
  eq(m.dir, "/tmp", "__dirname");
  if (require("/tmp/jabc_req_m.js") !== m) fail("cache identity");
}

// nested + RELATIVE resolution (a requires ./b from its own dir)
{
  writeFile("/tmp/jabc_req_b.js", "module.exports = { v: 41 };");
  writeFile("/tmp/jabc_req_a.js",
    "module.exports = require('./jabc_req_b.js').v + 1;");
  eq(require("/tmp/jabc_req_a.js"), 42, "nested relative require");
}

// cycle: partial exports are visible (CommonJS semantics)
{
  writeFile("/tmp/jabc_req_c.js",
    "exports.early = 1; const d = require('./jabc_req_d.js'); exports.dGotEarly = d.sawEarly;");
  writeFile("/tmp/jabc_req_d.js",
    "const c = require('./jabc_req_c.js'); module.exports = { sawEarly: c.early };");
  const c = require("/tmp/jabc_req_c.js");
  eq(c.early, 1, "cycle own export");
  eq(c.dGotEarly, 1, "cycle partial exports visible");
}

// missing module throws
{
  let threw = false;
  try { require("/tmp/jabc_req_does_not_exist.js"); } catch (e) { threw = true; }
  if (!threw) fail("missing module not rejected");
}

io.log("require.js OK");
