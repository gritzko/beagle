//  verbs/spot/spot.js — `spot:` structural search VIEW (JAB-021).  The handler
//  is the SHARED scaffold verbs/spot/search.js; `row.verb` ("spot") selects the
//  structural matcher.  ONE line so spot/grep/regex can never drift — the verb
//  is the mode parameter.  See search.js for the design + parity caveats.
"use strict";
module.exports = require("./search.js");
