//  shared/nav.js — URI-011: the nav-authority helper.  Every projector runs its
//  hunk banner + click-target URIs through navUri() so they carry the FULL nav
//  address (`//name/path`) — the pager stays scoped and answers "where am I".
"use strict";

//  The current nav authority (`//name` of the scoped tree) off the `be` global;
//  "" for the launch tree / no nav → navUri is byte-identical to `scheme:path`.
function authority() { return (typeof be !== "undefined" && be.authority) || ""; }

//  Compose `<scheme>:<auth><path>` — `path` is repo-relative (no leading '/').
function navUri(scheme, path) {
  const a = authority();
  return scheme + ":" + a + (a && path ? "/" : "") + (path || "");
}

//  URI-011: inject the current nav authority into a BAKED `scheme:<path>?…` hunk
//  URI — the C weave/graf bakes `diff:`/`cat:` click-targets with NO authority, so
//  a click from a `//ULOG` view loses the scope (empty output).  `be.authority`
//  set → `diff:file?a..b` becomes `diff://ULOG/file?a..b`; no authority (launch
//  tree) → UNCHANGED (byte-parity); an already-authoritative `scheme://…` is left.
function navAuthorize(bakedUri) {
  const a = authority();
  if (!a || !bakedUri) return bakedUri;
  const c = bakedUri.indexOf(":");
  if (c < 0) return bakedUri;
  const rest = bakedUri.slice(c + 1);
  if (rest.slice(0, 2) === "//") return bakedUri;       // already authoritative
  const sep = (rest && rest[0] !== "?" && rest[0] !== "#") ? "/" : "";
  return bakedUri.slice(0, c + 1) + a + sep + rest;
}

module.exports = { authority: authority, navUri: navUri, navAuthorize: navAuthorize };
