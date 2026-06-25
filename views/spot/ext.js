//  verbs/spot/ext.js — ext -> lexer-group resolution for the search VIEW's ext
//  gate (JAB-021).  A PURE-JS port of dog/tok/TOK.c's TOK_TABLE / TOK_NAME_TABLE
//  + TOKResolve/TOKKnownExt/TOKSameLexer — a STATIC DATA table (the same one the
//  C lexer dispatch uses), NOT a binding.  `known(ext)` and `sameLexer(a, b)`
//  drive the CAPOKnownExt + TOKSameLexer file-walk gate.
//
//  Each value is the C lexer's group NAME (its fn, e.g. "CT" for C/.h) so two
//  exts share a lexer iff their group strings are equal (TOKSameLexer ==).
"use strict";

//  ext (no dot) -> lexer group.  Verbatim from TOK_TABLE (TOK.c:110-211).
const EXT = {
  c:"CT", h:"CT",
  cpp:"CPPT", cc:"CPPT", cxx:"CPPT", hpp:"CPPT", hh:"CPPT", hxx:"CPPT",
  go:"GOT", py:"PYT",
  js:"JST", jsx:"JST", mjs:"JST",
  ts:"TST", tsx:"TST",
  rs:"RST", java:"JAT", kt:"KTT", kts:"KTT", scala:"SCLT", sc:"SCLT",
  cs:"CST", fs:"FSHT", fsi:"FSHT", fsx:"FSHT", swift:"SWFT", dart:"DARTT",
  d:"DT", zig:"ZIGT", html:"HTMT", htm:"HTMT", css:"CSST", scss:"SCSST",
  json:"JSONT", yml:"YMLT", yaml:"YMLT", toml:"TOMLT",
  sh:"SHT", bash:"SHT", rb:"RBT", lua:"LUAT", pl:"PRLT", pm:"PRLT",
  r:"RT", R:"RT", ex:"ELXT", exs:"ELXT", erl:"ERLT", hrl:"ERLT",
  hs:"HST", ml:"MLT", mli:"MLT", jl:"JLT", nim:"NIMT", nims:"NIMT",
  php:"PHPT", clj:"CLJT", cljs:"CLJT", cljc:"CLJT", edn:"CLJT",
  nix:"NIXT", sql:"SQLT", graphql:"GQLT", gql:"GQLT", proto:"PRTT",
  hcl:"HCLT", tf:"HCLT", tex:"LAXT", sty:"LAXT", cls:"LAXT", vim:"VIMT",
  cmake:"CMKT", dockerfile:"DKFT", mk:"MAKT",
  f90:"FORT", f95:"FORT", f03:"FORT", f08:"FORT",
  glsl:"GLST", vert:"GLST", frag:"GLST", geom:"GLST", comp:"GLST",
  gleam:"GLMT", odin:"ODNT", ps1:"PWST", psm1:"PWST", psd1:"PWST",
  sol:"SOLT", typ:"TYST", agda:"AGDT", v:"VERT", sv:"VERT", ll:"LLT",
  md:"MDT", markdown:"MDT", mkd:"MKDT", sm:"MKDT", txt:"TXTT", rst:"TXTT",
};

//  basename -> lexer group (TOK_NAME_TABLE, TOK.c:215-235).
const NAME = {
  "CMakeLists.txt":"CMKT", "Makefile":"MAKT", "makefile":"MAKT",
  "GNUmakefile":"MAKT", "Dockerfile":"DKFT", "Vagrantfile":"RBT",
  "Gemfile":"RBT", "Rakefile":"RBT", "Justfile":"MAKT",
  ".gitignore":"SHT", ".gitattributes":"SHT", ".gitmodules":"TOMLT",
  ".bashrc":"SHT", ".bash_profile":"SHT", ".profile":"SHT", ".zshrc":"SHT",
  ".vimrc":"VIMT", ".clang-format":"YMLT",
};

function base(path) {
  const i = path.lastIndexOf("/");
  return i < 0 ? path : path.slice(i + 1);
}

//  extOf: the trailing `.ext` of a path (with the dot), or "" — PATHu8sExt:
//  the last '.'-segment of the basename, NOT counting a leading-dot dotfile.
function extOf(path) {
  const b = base(path);
  const i = b.lastIndexOf(".");
  if (i <= 0) return "";            // no dot, or leading-dot dotfile
  return b.slice(i);
}

//  TOKResolve: resolve a path / filename / bare-or-dotted ext to a group, or
//  null.  Mirrors TOK.c:278-313.
function resolve(input) {
  if (!input) return null;
  const hasDot = input.indexOf(".") >= 0, hasSlash = input.indexOf("/") >= 0;
  if (!hasDot && !hasSlash) {
    if (EXT[input]) return EXT[input];        // bare ext "c"/"py"
    if (NAME[input]) return NAME[input];      // extensionless name
    return null;
  }
  if (input[0] === "." && !hasSlash && input.length > 1) {
    if (NAME[input]) return NAME[input];      // dotfile name (.gitignore)
    const stripped = input.slice(1);
    return EXT[stripped] || null;             // dotted ext ".c"
  }
  //  full path / filename: name table (basename), then ext table.
  const b = base(input);
  if (NAME[b]) return NAME[b];
  const e = extOf(input);
  return e ? (EXT[e.slice(1)] || null) : null;
}

function known(ext)        { return resolve(ext) !== null; }
function sameLexer(a, b)   { const fa = resolve(a); const fb = resolve(b); return fa !== null && fa === fb; }

module.exports = { known: known, sameLexer: sameLexer, extOf: extOf, resolve: resolve };
