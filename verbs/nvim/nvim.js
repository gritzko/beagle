//  verbs/nvim/nvim.js — BE-047: `nvim` is the SAME editor verb as vim (one
//  shared implementation); the binary name comes from be.verb (= "nvim" here).
"use strict";
module.exports = require("../vim/vim.js");
