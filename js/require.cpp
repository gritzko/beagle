#include "JABC.hpp"

//  Synchronous CommonJS require() — built entirely on the existing bindings
//  (io.mmap to read the source, utf8.Decode, io.stat to probe).  No engine
//  module loader, no promises: resolve -> mmap -> wrap -> eval -> cache.
//  Each module gets its own require bound to its directory (relative
//  resolution); the cache is keyed by resolved absolute path and the module
//  is inserted BEFORE eval so cycles see a partial exports (CommonJS rule).
static const char* JABC_REQUIRE_JS = R"JS(
(function (g) {
  "use strict";
  const io = g.io, utf8 = g.utf8;
  const cache = {};

  function normalize(p) {
    const abs = p[0] === "/";
    const out = [];
    for (const s of p.split("/")) {
      if (s === "" || s === ".") continue;
      if (s === "..") {
        if (out.length && out[out.length - 1] !== "..") out.pop();
        else if (!abs) out.push("..");
      } else out.push(s);
    }
    return (abs ? "/" : "") + out.join("/");
  }
  function dirname(p) {
    const i = p.lastIndexOf("/");
    return i < 0 ? "." : (i === 0 ? "/" : p.slice(0, i));
  }
  function isFile(p) {
    try { return io.stat(p).kind === "reg"; } catch (e) { return false; }
  }
  function resolve(spec, baseDir) {
    let base = normalize(spec[0] === "/" ? spec : baseDir + "/" + spec);
    for (const c of [base, base + ".js", base + "/index.js"])
      if (isFile(c)) return c;
    throw "require: cannot find '" + spec + "' from '" + baseDir + "'";
  }
  function makeRequire(baseDir) {
    const req = (spec) => load(spec, baseDir);
    req.resolve = (spec) => resolve(spec, baseDir);
    req.cache = cache;
    return req;
  }
  function load(spec, baseDir) {
    const abs = resolve(spec, baseDir);
    if (cache[abs]) return cache[abs].exports;
    const src = utf8.Decode(io.mmap(abs, "r").data());
    const module = { exports: {}, id: abs, filename: abs };
    cache[abs] = module;                       // before eval: cycle-safe
    const dir = dirname(abs);
    const fn = new Function("module", "exports", "require",
                            "__filename", "__dirname", src);
    fn(module, module.exports, makeRequire(dir), abs, dir);
    return module.exports;
  }

  g.require = makeRequire(".");                 // top-level: resolve vs cwd
})(this);
)JS";

ok64 JABCRequireInstall() {
  JABCExecute(JABC_REQUIRE_JS);
  return OK;
}
