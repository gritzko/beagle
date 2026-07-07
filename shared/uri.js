//  uri.js — the ONE total arg classifier shared by verbs + bro (JAB-005):
//  parse(arg) returns a URI when arg lexes, else the raw string (swallows the
//  native `malformed` throw), so verbs parse on entry and branch on the TYPE.
"use strict";

//  URI is the native (js/uri.cpp) global constructor.  A URI result => use its
//  slots; a string result => free-form text (a message / search prose).
function parse(arg) {
  try { return new URI(arg); } catch (e) { return arg; }
}

//  URI-015: git scp-form remote (`user@host:path`, git's colon-before-slash
//  rule) is NOT a URI — recompose as ssh:// via URI.make; else pass verbatim.
function fromGit(s) {
  s = String(s === undefined || s === null ? "" : s);
  if (s.indexOf(" ") >= 0 || s.indexOf("\t") >= 0) return s;  // prose, not a remote
  const colon = s.indexOf(":"), at = s.indexOf("@"), slash = s.indexOf("/");
  if (colon < 1 || at < 1 || at > colon) return s;            // no `user@host:` head
  if (slash >= 0 && slash < colon) return s;                  // `/` before `:` → not scp
  //  The tail is lexed as a RELATIVE URI so `?branch`/`#sha` land in their
  //  slots; a tail with its own scheme/authority is not a git path — bail.
  let t; try { t = new URI(s.slice(colon + 1)); } catch (e) { return s; }
  if (t.scheme !== undefined || t.authority !== undefined) return s;
  const path = (t.path || "")[0] === "/" ? t.path : "/" + (t.path || "");
  const r = URI.make("ssh", "//" + s.slice(0, colon), path, t.query, t.fragment);
  if (!r) return s;
  try { new URI(r); } catch (e) { return s; }                 // relex guard
  return r;
}

module.exports = { parse: parse, fromGit: fromGit };
