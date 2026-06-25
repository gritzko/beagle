//  verbs/regex/regex.js — `regex:` native-RegExp search VIEW (JAB-023).  Rides
//  the SHARED scaffold verbs/spot/search.js; `row.verb` ("regex") selects the
//  native JS RegExp matcher (replaces the C Thompson NFA).  ONE line — the verb
//  is the mode parameter.  See verbs/spot/search.js for the scaffold + framing.
"use strict";
module.exports = require("../spot/search.js");
