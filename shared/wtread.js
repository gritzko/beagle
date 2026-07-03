//  shared/wtread.js — CODE-020: the ONE reg-file wt read (open/readAll/close-
//  safe, NOT io.mmap which leaks).  Bytes copy on success, null on any error.
"use strict";

function readFileBytes(full, size) {
  let fd;
  try { fd = io.open(full, "r"); } catch (e) { return null; }
  try {
    const b = io.buf((size || 0) + 16);
    io.readAll(fd, b, size);
    const d = b.data().slice();
    io.close(fd);
    return d;
  } catch (e) { try { io.close(fd); } catch (e2) {} return null; }
}

module.exports = { readFileBytes: readFileBytes };
