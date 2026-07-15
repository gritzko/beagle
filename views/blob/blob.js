//  views/blob/blob.js — the `blob:` read-only VIEW (JAB-007).  The by-OBJECT-SHA
//  twin of the landed `cat:` view: where cat: reads a wt FILE by path, blob:
//  reads a tracked BLOB by the sha a URI resolves to, then emits its content as a
//  HUNK exactly the way cat: does.  Pure JS over the libabc/libdog bindings:
//  shared/store.js (object read + the canonical `resolveHexAny` sha/prefix→
//  full-sha resolver), core/resolve_hash.js (URI-016: THE URI->hash resolver —
//  the path-bearing form's ref/hashlet/cur-tip resolution + the path descent),
//  the URI binding (the structured scheme/path/query/frag split).  NO dog.
//
//  RULING (gritzko): blob: produces a HUNK like the other views (cat:/tree:/log:),
//  NOT a raw byte dump.  So this view is modelled CLOSELY on views/cat/cat.js:
//  it builds a HUNK (body + tok32) and feeds it through the SAME caller-owned
//  in-memory HUNK sink (`ctx.sink`); the loop EDGE renders that sink in the
//  active mode (plain/color/tlv) via view/bro.js renderHunkLog.  The handler
//  NEVER calls io.log/io.write/raw-stdout, NEVER uses a raw-bytes channel and
//  NEVER bypasses core/emit.js.  The hunk carries a banner naming the verb
//  (`blob <sha>#L<n>`, the analogue of cat's `cat <path>#L<n>`), so blob: PLAIN
//  looks like cat: PLAIN (banner + body + trailing newline) on the SAME bytes —
//  it does NOT byte-match the C blob:'s banner-less dump, and that is INTENDED.
//
//  Forms (KEEPGetByURI / the URI slots):
//    `?<hex>` / `?#<hex>` / `#<hex>`  bare object by a 1..40 hex sha-prefix;
//                                     the object MUST be a blob.
//    `<path>` / `<path>?<ref>`        the blob at `path` in the tree of `?ref`
//                                     (a branch, a sha-prefix, or empty → cur tip).
//    `#L42` (a non-hex fragment)      a bro line-anchor — IGNORED for resolution;
//                                     the FULL blob is always emitted.
//    `//host…` (a host-bearing URI)   remote NYI → fail (as C KEEPFAIL).
//
//  Error edges (NO hunk fed + a THROW → nonzero exit, matching native's
//  KEEPFAIL/KEEPNONE; the exact dog exit code/text is not reproduced):
//    missing path segment / bad ref / unresolvable sha   -> throw "BLOBNONE"
//    dir-as-blob / non-blob object / ambiguous prefix     -> throw "BLOBFAIL"
//    host-bearing URI (remote NYI)                        -> throw "BLOBFAIL"

"use strict";

const store = require("../../shared/store.js");
const ambient = require("../../shared/ambient.js");   // JAB-004: ctx→be bridge
const bro   = require("../../view/bro.js");
const discover = require("../../core/discover.js");   // URI-016: the nav context
const resolve_hash = require("../../core/resolve_hash.js").resolve_hash;
const isFullSha = require("../../shared/util/sha.js").isFullSha;
const navlib = require("../../shared/nav.js");        // URI-014: word-URI banner

//  JS-082: a FULL 40-hex sha passes through verbatim iff the object exists (its
//  presence is the resolution); resolveHexAny's {1,39} prefix scanner rejects
//  40, so short-circuit it.  Returns the sha, or undefined when absent — keeping
//  resolveHexAny's undefined-on-miss contract so callers branch identically.
function resolveHexOrFull(k, hex) {
  if (isFullSha(hex)) return k.getObject(hex) ? hex : undefined;
  return k.resolveHexAny(hex);
}

//  URI-016: THE resolver's record -> the blob sha, in blob:'s error dialect.
//  resolve_hash's refusals map onto this view's contract (see the header): a
//  missing segment / bad ref / unresolvable sha is BLOBNONE; a leaf that is a
//  dir (tree) or a gitlink (commit) is BLOBFAIL, never a readable blob.
function blobShaOf(bare) {
  const ctx = discover.navCwd(discover.ctxDir());   // URI-016: derived off be.context
  let r;
  try { r = resolve_hash(ctx, bare); }
  catch (e) { throw "BLOBNONE"; }
  if (r.otype !== "blob") throw "BLOBFAIL";
  return r.ohash;
}

const EMPTY32 = new Uint32Array(0);
const CAP = 1 << 20;   // 1 MiB/hunk cap; a bigger blob splits with a #L<n> rebanner

//  A 1..40 lowercase-hex string (a sha or sha-prefix).  Looser than
//  resolve.isHexish (which floors at 6) because the bare-object form takes an
//  ARBITRARY-length prefix (C WHIFFHexHashlet60 zero-pads any width).
function isHexPrefix(s) { return !!s && s.length <= 40 && /^[0-9a-f]+$/.test(s); }

//  URI-016: resolveRootTree/commitOrTree are GONE — the path-bearing form goes
//  through resolve_hash (blobShaOf).  They accepted a TREE sha as the root,
//  which [/wiki/URI] step 5 forbids: `chash` is a COMMIT in every case.

//  JAB-004: blob ONE arg — self-parse blob:<uri>, read be.repo/be.sink +
//  ambient.format(); `ctx` = direct-handler fallback (no global be).
function blobOne(arg, ctx) {
  const _be = (typeof be !== "undefined") ? be : null;
  const mode = ambient.format();
  const repo = (_be && _be.repo) || (ctx && ctx.repo) || null;
  if (!repo) return;

  //  URI-013: ONE structured parse of the whole `blob:<uri>` — the URI binding
  //  reads `.path`/`.query`/`.fragment`/`.authority` off the scheme'd form (no
  //  strip-then-reparse; the analogue of cat's collapse).
  const first = String(arg || "");
  const u = uri._parse(first);
  const path  = u.path || "";
  const query = u.query || "";
  const frag  = u.fragment || "";
  const auth  = u.authority || "";

  //  A host-bearing URI (`//remote…`) is a remote read — NYI, as native KEEPFAIL.
  if (auth) throw "BLOBFAIL";

  const k   = store.open(repo.storePath, repo.project);

  //  Resolve the URI to a BLOB sha (the by-object-sha resolution — the only part
  //  that differs from cat:'s by-path read).  `sha` names the blob; `bannerKey`
  //  is the banner stem (the full sha for a bare-object form, else `<path>` —
  //  the analogue of cat:'s `<path>` banner; cat: appends `#L<n>` per hunk).
  let sha, bannerKey;
  if (!path) {
    //  Bare object by a sha-prefix: `?<hex>`, `?#<hex>` or `#<hex>`.  A non-hex
    //  fragment (`#L42` bro line-anchor) is NOT an object id — fall through to
    //  the empty-path error (no full blob to name).  The object MUST be a blob.
    const hex = isHexPrefix(query) ? query : isHexPrefix(frag) ? frag : null;
    if (!hex) throw "BLOBNONE";                 // empty/`#label`-only → nothing
    sha = resolveHexOrFull(k, hex);             // JS-082: full-sha verbatim
    if (sha === null) throw "BLOBFAIL";         // ambiguous prefix
    if (!sha) throw "BLOBNONE";                 // no object with that prefix
    bannerKey = sha;                            // banner the FULL object sha
  } else {
    //  URI-016: the path-bearing form IS [/wiki/URI] §URI->hash steps 5+6 — it
    //  resolves through THE resolver, never a local twin.  `blob:` is this
    //  view's OWN scheme, so it is shed; the `[path][?ref][#frag]` left over is
    //  what the spec resolves.  A non-hex `#L42` is a bro line-anchor, not a
    //  hashlet — it is dropped so it cannot reach step 5.
    const rev = isHexPrefix(frag) ? frag : undefined;
    const bare = URI.make(undefined, undefined, path, u.query, rev) || path;
    sha = blobShaOf(bare);
    bannerKey = path;                           // banner the path (like cat:)
  }

  //  Fetch the blob bytes by sha (store.getObject — inflate + delta chase).
  const obj = k.getObject(sha);
  if (!obj) throw "BLOBNONE";                   // sha unreadable
  if (obj.type !== "blob") throw "BLOBFAIL";    // not a blob object
  const bytes = obj.bytes;

  //  An EMPTY blob (0 bytes) emits NOTHING — no banner — exactly like cat:'s
  //  empty-file case (and the C `KEEPProjBlob` 0-byte case).
  if (!bytes || bytes.length === 0) return;

  //  Feed the blob into the caller-owned in-memory HUNK sink (ctx.sink) — NO fd 1
  //  here; the loop edge (cli) renders sink.log to fd 1 in the mode (plain/color/
  //  tlv) via bro.renderHunkLog.  This is the SAME sink path cat:/grep/spot feed,
  //  and the SAME chunking/CAP cat: uses (1 MiB/hunk, backed up to a line bound so
  //  a line never splits; a #L<n> rebanner per chunk).  The banner verb is "blob"
  //  so a hunk reads `blob <bannerKey>#L<n>` — the cat: `cat <path>#L<n>` twin.
  const sink = (_be && _be.sink) || (ctx && ctx.sink) || null;
  if (!sink) return;
  const ext = bro.pathExt(bannerKey);           // "js"/"" — drives tok.parse (path form)
  let off = 0, line = 1;
  while (off < bytes.length) {
    //  1 MiB hunk, backed up to the last line boundary so a line never splits.
    let end = off + CAP < bytes.length ? off + CAP : bytes.length;
    if (end < bytes.length) {
      let nl = end; while (nl > off && bytes[nl - 1] !== 10) nl--;
      if (nl > off) end = nl;
    }
    const body = bytes.slice(off, end);
    let toks = EMPTY32;
    if (mode !== "plain" && ext) { try { toks = tok.parse(body, ext); } catch (e) { toks = EMPTY32; } }
    //  URI-014: word-URI banner — the verb "blob" rides the uri as its leading
    //  WORD (`blob <key>#L<n>`), NOT the RON60 verb slot (the pager can't read it).
    sink.feed(navlib.navLink("blob", bannerKey, undefined, "L" + line), body, toks, "", 0n);
    for (let i = off; i < end; i++) if (bytes[i] === 10) line++;
    off = end;
  }
  //  Read-only leaf: no fan-out (per-arg; the dispatcher fans out over args).
}

//  JAB-004: PLAIN verb (`.jab="args"`) loops its STRING args reading `be`.
function blob() {
  for (let i = 0; i < arguments.length; i++) blobOne(arguments[i]);
}
blob.jab = "args";
module.exports = blob;
