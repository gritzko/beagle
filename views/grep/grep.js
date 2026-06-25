//  verbs/grep/grep.js — `grep:` literal-substring search VIEW (JAB-022).  Rides
//  the SHARED scaffold verbs/spot/search.js; `row.verb` ("grep") selects the
//  substring matcher (no lexer, no .ext required).  ONE line — the verb is the
//  mode parameter.  See verbs/spot/search.js for the scaffold + framing.
"use strict";
module.exports = require("../spot/search.js");
