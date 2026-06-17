#include "JABC.hpp"

//  ANSI colour helper — pure JS (SGR codes are just bytes; no abc/ANSI, no
//  link).  Each helper wraps text in a `\x1b[<n>m … \x1b[0m` pair.  Gate on
//  io.isatty(fd) to fall back to plain.  For theme-consistent (bro-matched)
//  colours use the hunk renderer (h.color); this is for ad-hoc output.
static const char* JABC_ANSI_JS = R"JS(
(function (g) {
  "use strict";
  const E = "\x1b[", R = E + "0m";
  const wrap = (n) => (s) => E + n + "m" + s + R;
  g.ansi = {
    reset: R,
    bold: wrap(1), dim: wrap(2), italic: wrap(3), under: wrap(4), rev: wrap(7),
    black: wrap(30), red: wrap(31), green: wrap(32), yellow: wrap(33),
    blue: wrap(34), magenta: wrap(35), cyan: wrap(36), white: wrap(37),
    grey: wrap(90),
    sgr: (n) => E + n + "m",          // raw escape for any other code
  };
})(this);
)JS";

ok64 JABCAnsiInstall() {
  JABCExecute(JABC_ANSI_JS);
  return OK;
}
