//  verbs/lsr/lsr.js — `lsr:` recursive worktree listing (JAB-019).  lsr IS ls
//  with recursion ON: the handler is verbs/ls/ls.js, which reads `row.verb`
//  ("lsr") to fan out an `lsr:<child>` row per immediate subdir / mount.  ONE
//  line so the two views can NEVER drift — same code, the verb is the parameter.
"use strict";
module.exports = require("../ls/ls.js");
