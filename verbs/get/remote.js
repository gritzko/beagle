//  remote.js — CODE-030: remote-source URI CLASSIFICATION, extracted verbatim
//  out of get.js.  parseRemote lexes a source URI into the GET.mkd 5-slot map;
//  resolveLocalSource follows a worktree anchor's row-0 redirect down to the
//  REAL store.  Neither is verb behaviour, and both are what `patch`'s fetchleg
//  reached back into get.js for — a leaf here, re-exported by get.js unchanged.
"use strict";

const pathlib = require("../../shared/util/path.js");
const branchlib = require("../../shared/branch.js");   // SUBS-050: the ONE branch codec
const ulog = require("../../shared/ulog.js");
const uriarg = require("../../shared/uri.js");         // URI-015: scp remote → ssh://
const join = pathlib.join, dirname = pathlib.dirname;

//  --- remote URI → { local, srcRoot, srcBe, proj, branch, pin } -----------
//  No hand-rolled parsing: the URI binding splits scheme/host/path/query.
//  GET.mkd 5-slot map: Scheme=transport, Host=remote, Query=branch/sha,
//  Fragment=exact-commit PIN (D1).  BE-033: a scheme-less `//host` is NEVER a
//  remote (a `//X` is always a worktree, nav-resolved); remotes carry a scheme.
function parseRemote(uri) {
  //  URI-015: scp-form remote → ssh:// before the lex; rem.raw records the
  //  recomposed URI (wtlog anchor / wire.fetch arg), never the scp string.
  uri = uriarg.fromGit(uri);
  const u = new URI(uri);
  //  URI-009: route on slot PRESENCE (undefined = absent), not string-emptiness.
  //  A `file:`/scheme-less `.be` path is a LOCAL store; any scheme is a wire
  //  transport.  u.host is the bare authority ("origin"), no leading `//`.
  const hasScheme = u.scheme !== undefined;
  const hasAuth   = u.authority !== undefined;
  const scheme = u.scheme || "";
  const host = u.host || "";
  const authority = u.authority || "";
  const path = u.path || "";
  const query = u.query || "";
  const frag = u.fragment || "";           // D1: the exact-commit pin (no `?`)
  //  SUBS-050: split a `?/<proj>/<branch>` selector via the ONE branch codec —
  //  the absolute head names the project, the tail (title-stripped) is the
  //  branch key; a plain `?<branch>` re-heads to no project.
  const br = branchlib.parse(query, "");
  const proj = (query && query[0] === "/") ? br.title : "";
  const branch = branchlib.key(br);
  //  A `file:`/scheme-less LOCAL store path (ends in `.be` or holds one).
  const hasStorePath = path.replace(/\/+$/, "").slice(-3) === ".be" ||
                       (path !== "" && !hasAuth);
  const localish = (scheme === "file" && hasStorePath) ||
                   (!hasScheme && !hasAuth && hasStorePath) ||
                   (scheme === "keeper" && (host === "" || host === "local" ||
                                            host === "localhost"));
  let srcBe = path, srcRoot = path;
  if (localish) {
    srcBe = path.replace(/\/+$/, "");
    srcRoot = srcBe.replace(/\/\.be$/, "");
    if (srcRoot === srcBe) srcRoot = dirname(srcBe);
  }
  return { local: localish, scheme, host, authority, srcRoot, srcBe,
           proj, branch, pin: frag, raw: uri };
}

//  GET-038: a local `file:` source may name a STORE (`<store>/.be`) OR a
//  WORKTREE (`<wt>` whose `.be` is a wtlog FILE redirecting to the real store).
//  A worktree is NOT a store: recording its path as the new wt's row-0 anchor
//  leaves `status`/`get` unable to read the baseline tree (the store has no
//  objects there) — every file then reads `unk`.  So resolve the source down to
//  the REAL store: when `<srcRoot>/.be` (or `<srcBe>` itself) is a FILE, follow
//  its row-0 `repo` redirect (be.repoFromBe / be.projectFromQuery, the same
//  DOGRepoFromBe split be.treeAt uses) to the store dir + project, and record THAT
//  — never the worktree path.  A plain store source resolves to itself unchanged.
//  Returns { storeRoot, storeBe, proj } where storeBe is the real `<store>/.be`.
function resolveLocalSource(rem) {
  //  The source `.be` to probe: the path itself when it ends `.be`, else
  //  `<path>/.be`.  A worktree anchor is a regular FILE; a store `.be` is a dir.
  const srcBe = rem.srcBe;                       // path with trailing `.be` shed
  const beFile = (srcBe.slice(-3) === ".be") ? srcBe : join(srcBe, ".be");
  let kind; try { kind = io.stat(beFile).kind; } catch (e) { kind = undefined; }
  if (kind !== "reg")                            // a store (dir) or absent → as-is
    return { storeRoot: rem.srcRoot, storeBe: rem.srcBe, proj: rem.proj };

  //  Worktree source: read row 0 (the `repo|<storepath>` redirect) and split it
  //  to the real store dir + project — the same resolution be.treeAt performs on a
  //  secondary wt anchor.
  let u0;
  ulog.each(beFile, function (log) { if (u0 === undefined) u0 = log.uri; });
  if (!u0)
    throw "be get: GETWTSRC worktree source " + srcBe +
          " has no store redirect — cannot resolve its store";
  const p = new URI(u0);
  const storeRoot = be.repoFromBe(p.path || "");
  const proj = rem.proj || be.projectFromQuery(p.query || "") ||
               be.projectFromPath(p.path || "");
  if (!storeRoot)
    throw "be get: GETWTSRC cannot resolve the store of worktree source " + srcBe;
  return { storeRoot: storeRoot, storeBe: join(storeRoot, ".be"), proj: proj };
}

module.exports = { parseRemote: parseRemote,
                   resolveLocalSource: resolveLocalSource };
