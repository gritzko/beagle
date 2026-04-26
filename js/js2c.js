#!/usr/bin/node

const fs = require("fs");
const console = require("console");
const process = require("process");

var file = process.argv[2];

str = fs.readFileSync(file, {encoding:"utf8"})

str = str.replace(/"/g, '\\"')
        .replace(/\n/g, '\\n"\n"');

var varn = file.replace(/.*\//g, "").replaceAll('.', '_');

var code = "const char* " + varn + " = \"" + str + "\";";

console.log(code);


