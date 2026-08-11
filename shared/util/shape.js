//  shape.js — hidden-class pinning for hot object literals (BE-066).  The
//  engines key an object's layout on the SEQUENCE of keys it was built from.
//
//  quickjs charges one shape CLONE per multi-key object built at a hot site:
//  the first key clones the shared empty shape, the rest mutate that private
//  shape in place, so the transition is never cached and the NEXT object pays
//  again (measured: 519,992 clones in one pty `todo`, 29% of engine samples).
//
//  Holding one live object per PREFIX of the key sequence keeps every
//  transition in the engine's shape table, and the literals then run clone-free
//  AND share one shape (JSC reads them through one inline cache too).
//
//    const PIN_ROW = shape.pin(["bucket", "path", "ts"]);   // module level
//    rows.push({ bucket: b, path: p, ts: t });              // same ORDER

"use strict";

//  The pins live HERE, for the whole process: a caller's module-scope const is
//  dropped unless some inner function reads it, and `pin` below reads KEPT.
const KEPT = [];

//  pin(keys) → the prefix chain (also KEPT).  The key order MUST match the
//  literal's, key for key.
function pin(keys) {
  const chain = [];
  KEPT.push(chain);
  for (let n = 1; n <= keys.length; n++) {
    const o = {};
    for (let i = 0; i < n; i++) o[keys[i]] = undefined;
    chain.push(o);
  }
  return chain;
}

module.exports = { pin: pin };
