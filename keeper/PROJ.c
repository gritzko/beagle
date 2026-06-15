//  PROJ: keeper view projectors — tree:, commit:, blob:.
//
#include "PROJ.h"
#include "dog/git/GIT.h"
#include "REFS.h"
#include "WALK.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "abc/B.h"
#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"

#include "dog/DOG.h"
#include "dog/HUNK.h"
#include "dog/tok/TOK.h"

// =====================================================================
//  Helpers
// =====================================================================

//  20-byte SHA → 40 hex bytes appended via streaming cursor `out`.
static void proj_feed_sha_hex(u8s out, sha1cp s) {
    (void)SHA1u8sFeedHex(out, s);
}

//  Append a NUL-terminated literal to streaming cursor `out`.
static void proj_feed_lit(u8s out, char const *s) {
    a$str(sl, s);
    (void)u8sFeed(out, sl);
}

//  Resolve `u` (#hex or ?ref) to a binary commit/tree SHA.  Mirrors
//  the early half of KEEPResolveTree but stops at the commit (or
//  whatever object the URI points at — caller filters by type).
//  On success returns OK; *out is the 20-byte SHA-1.
static ok64 proj_resolve_object_sha(keeper *k, uricp u, sha1 *out) {
    sane(k && u && out);
    a_path(keepdir);
    call(HOMEBranchDir, keepdir, NULL);

    //  #hex — convert prefix to a full SHA via KEEPGet (which scans
    //  hashlet matches and verifies).  We don't need the body here,
    //  so use a scratch buffer and discard.
    if (!u8csEmpty(u->fragment)) {
        if (u8csLen(u->fragment) >= 40) {
            u8s sb = {out->data, out->data + 20};
            u8cs hx = {u->fragment[0], u->fragment[0] + 40};
            return HEXu8sDrainSome(sb, hx);
        }
        //  Short prefix: fetch object to confirm and recompute its sha.
        a_carve(u8, tmp, 1UL << 20);
        u64 hashlet = WHIFFHexHashlet60(u->fragment);
        u8 type = 0;
        ok64 go = KEEPGet(hashlet, u8csLen(u->fragment), tmp, &type);
        if (go == OK) {
            a_dup(u8c, body, u8bData(tmp));
            KEEPObjSha(out, type, body);
        }
        return go;
    }

    if (u8csEmpty(u->query)) fail(KEEPFAIL);

    //  REFSResolve handles `?ref`, full URIs, alias chains.
    a_pad(u8, arena_buf, 1024);
    uri resolved = {};
    a_dup(u8c, in_uri, u->data);
    ok64 ro = REFSResolve(&resolved, arena_buf, $path(keepdir), in_uri);
    if (ro == OK && u8csLen(resolved.query) >= 40) {
        u8s sb = {out->data, out->data + 20};
        u8cs hx = {resolved.query[0], resolved.query[0] + 40};
        return HEXu8sDrainSome(sb, hx);
    }
    fail(PROJNONE);
}

//  Descend a '/'-separated subpath inside a tree.  Returns the final
//  entry's sha + kind in *out_*.  Empty / "." / "./" subpath returns
//  the input tree as a DIR.  The segment walk itself is the shared
//  `KEEPTreeDescend` (WALK.h, CODE-005); here we only collapse the
//  "this-directory" shorthand to an empty subpath first, then map a
//  not-found segment to PROJNONE.
static ok64 proj_descend(keeper *k, sha1cp root_tree, u8cs subpath,
                          sha1 *out_sha, u8 *out_kind) {
    sane(k && root_tree && out_sha && out_kind);
    (void)k;

    //  Tolerate "." / "./" for "this directory" — both shapes come
    //  from the bareword promotion in dog/DOG.c (`be tree:./` vs
    //  `be tree:.`) and collapse to the empty-path root walk.
    u8cs sub = {};
    u8csMv(sub, subpath);
    if (u8csLen(sub) == 1 && sub[0][0] == '.') sub[0] = sub[1];
    else if (u8csLen(sub) == 2 && sub[0][0] == '.' && sub[0][1] == '/')
        sub[0] = sub[1];

    a_carve(u8, tbuf, 1UL << 20);
    call(KEEPTreeDescend, root_tree, sub, NULL, out_sha, out_kind,
         tbuf, PROJNONE);
    done;
}

static b8 proj_is_hex_prefix(u8cs s);

//  Title for the hunk header bar: "<scheme>:<path>?<query>" or
//  "<scheme>:#<frag>".  Display label only — not a protocol URI.
//  A bare hex-sha fragment (the shape KEEPProjDispatch leaves behind
//  after promoting a `?<sha>` query into the fragment for resolution)
//  is rendered back in the query slot: the general rule is that a hash
//  always lives in `?`, never `#` (the fragment is only ever a line or
//  a message).
static void proj_feed_title(u8s out, uricp u) {
    if (!u8csEmpty(u->scheme)) {
        (void)u8sFeed(out, u->scheme);
        (void)u8sFeed1(out, ':');
    }
    if (!u8csEmpty(u->path)) (void)u8sFeed(out, u->path);
    u8cs frag = {u->fragment[0], u->fragment[1]};
    b8 frag_is_sha = u8csEmpty(u->query) && proj_is_hex_prefix(frag);
    if (!u8csEmpty(u->query) || frag_is_sha) {
        (void)u8sFeed1(out, '?');
        (void)u8sFeed(out, u8csEmpty(u->query) ? u->fragment : u->query);
    }
    if (!u8csEmpty(u->fragment) && !frag_is_sha) {
        (void)u8sFeed1(out, '#');
        (void)u8sFeed(out, u->fragment);
    }
}

//  Emit `text` as a hunk: TLV via HUNKu8sFeed (bro will render), or
//  raw bytes otherwise.  Used by tree: and commit: where there's no
//  tokenization.  `toks` may be empty.
static ok64 proj_emit_hunk(uricp u, Bu8 text, tok32cs toks, b8 tlv) {
    sane(u);
    if (!tlv) {
        //  Plain CLI output: walk toks and elide any 'U'-tagged spans
        //  (invisible URI metadata).  Sans toks (or no U tokens) this
        //  is a straight fwrite of the entire buffer.
        u8c *base = u8bDataHead(text);
        u32  tlen = (u32)u8bDataLen(text);
        int  n    = toks != NULL ? (int)$len(toks) : 0;
        u32  prev = 0, emit_lo = 0;
        for (int i = 0; i < n; i++) {
            u32 end = tok32Offset(toks[0][i]);
            if (tok32Tag(toks[0][i]) == 'U') {
                if (emit_lo < prev) {
                    fwrite(base + emit_lo, 1, prev - emit_lo, stdout);
                }
                emit_lo = end;
            }
            prev = end;
        }
        if (emit_lo < tlen) {
            fwrite(base + emit_lo, 1, tlen - emit_lo, stdout);
        }
        fflush(stdout);
        done;
    }
    a_pad(u8, title, 512);
    proj_feed_title(u8bIdle(title), u);

    hunk hk = {};
    hk.uri[0]  = u8bDataHead(title);
    hk.uri[1]  = u8bIdleHead(title);
    hk.text[0] = u8bDataHead(text);
    hk.text[1] = u8bIdleHead(text);
    if (toks != NULL) {
        hk.toks[0] = toks[0];
        hk.toks[1] = toks[1];
    }

    size_t tlen = u8bDataLen(text);
    a_carve(u8, outbuf, tlen + (1UL << 16));
    ok64 fo = HUNKu8sFeed(u8bIdle(outbuf), &hk);
    if (fo == OK) {
        fwrite(u8bDataHead(outbuf), 1, u8bDataLen(outbuf), stdout);
        fflush(stdout);
    }
    return fo;
}

// =====================================================================
//  tree:
// =====================================================================

//  Push one tok at the current text end-offset.  Tokens carry end
//  offsets (24-bit), so the caller feeds bytes first and stamps the
//  token right after.  Returns the new end offset for convenience.
static u32 proj_push_tok(u8b text, Bu32 toks, u8 tag) {
    u32 end = (u32)u8bDataLen(text);
    (void)u32bFeed1(toks, tok32Pack(tag, end));
    return end;
}

//  Parent path for the `..` row.  Empty / single-segment / trailing-`/`
//  inputs collapse to the empty slice (= tree root).
static void proj_tree_parent_path(u8csp out, u8cs path) {
    out[0] = out[1] = NULL;
    if ($empty(path)) return;
    //  Strip trailing '/' so "src/" and "src" behave identically.
    u8cp end = path[1];
    while (end > path[0] && *(end - 1) == '/') end--;
    if (end == path[0]) return;
    //  Walk back to the last '/' separator.
    u8cp p = end;
    while (p > path[0] && *(p - 1) != '/') p--;
    if (p == path[0]) return;  //  single segment → root
    out[0] = path[0];
    out[1] = p - 1;            //  drop the separator itself
}

//  Emit a `<scheme>:<path>?<query>` or `<scheme>:<path>#<frag>` URI
//  composed from the source URI's rev slot and an explicit `path`.
//  After KEEPProjDispatch normalises a hex-prefix query to fragment,
//  query and fragment are mutually exclusive on the input URI — so we
//  carry whichever is set verbatim.
static void proj_feed_link_uri(u8s out, char const *scheme, u8cs path,
                                uricp src) {
    proj_feed_lit(out, scheme);
    (void)u8sFeed1(out, ':');
    if (!$empty(path)) (void)u8sFeed(out, path);
    if (!u8csEmpty(src->query)) {
        (void)u8sFeed1(out, '?');
        (void)u8sFeed(out, src->query);
    } else if (!u8csEmpty(src->fragment)) {
        (void)u8sFeed1(out, '#');
        (void)u8sFeed(out, src->fragment);
    }
}

//  Emit one entry's anchor (name [+/]) + invisible U-tagged URI bytes
//  + '\n'.  Tokens: anchor 'F' at end-of-name, U at end-of-URI, W at NL.
//  The mode/type/sha prefix is emitted by the caller (with its own 'P'
//  tok at start-of-name).  `base_path` is the directory the listing is
//  rooted in (the URI's resolved path); it composes with `name` to
//  produce the full link path inside the rev.
static ok64 proj_tree_emit_entry(u8b text, Bu32 toks,
                                 u8cs base_path, u8cs name, b8 is_dir,
                                 uricp src) {
    sane(1);
    //  Visible anchor: just the name (+ '/' for dirs).
    (void)u8bFeed(text, name);
    if (is_dir) (void)u8bFeed1(text, '/');
    proj_push_tok(text, toks, 'F');

    //  Build the full link path in a scratch buffer.
    a_pad(u8, lp, 4096);
    if (!$empty(base_path)) {
        (void)u8bFeed(lp, base_path);
        if (*$last(base_path) != '/') (void)u8bFeed1(lp, '/');
    }
    (void)u8bFeed(lp, name);
    if (is_dir) (void)u8bFeed1(lp, '/');
    a_dup(u8c, link_path, u8bData(lp));

    //  Invisible URI bytes — bro hides U-tagged spans, clicking the
    //  preceding 'F' anchor opens this URI.
    proj_feed_link_uri(u8bIdle(text), is_dir ? "tree" : "blob", link_path, src);
    proj_push_tok(text, toks, 'U');

    (void)u8bFeed1(text, '\n');
    proj_push_tok(text, toks, 'W');
    done;
}

//  git mode prefix → display columns ("100644 blob", "040000 tree", …).
//  Wide-enough fixed columns so names line up.
static void proj_tree_mode_type(u8s out, u8 kind) {
    char const *mode = "??????";
    char const *type = "?????";
    switch (kind) {
        case WALK_KIND_DIR: mode = "040000"; type = "tree";   break;
        case WALK_KIND_REG: mode = "100644"; type = "blob";   break;
        case WALK_KIND_EXE: mode = "100755"; type = "blob";   break;
        case WALK_KIND_LNK: mode = "120000"; type = "blob";   break;
        case WALK_KIND_SUB: mode = "160000"; type = "commit"; break;
        default: break;
    }
    proj_feed_lit(out, mode);
    (void)u8sFeed1(out, ' ');
    proj_feed_lit(out, type);
    //  Pad type to a stable 6-char column ("commit" is the widest).
    size_t tn = strlen(type);
    while (tn++ < 6) (void)u8sFeed1(out, ' ');
}

ok64 KEEPProjTree(uricp u, b8 tlv) {
    sane(u);
    keeper *k = &KEEP;

    //  Resolve URI → root tree SHA.  KEEPResolveTree handles ?ref,
    //  #hex, and commit→tree dereference.
    sha1 root_tree = {};
    call(KEEPResolveTree, u, &root_tree);

    //  Descend the URI's path inside that tree.  Empty path stays at
    //  the root; trailing '/' is fine.
    sha1 target = {};
    u8 target_kind = 0;
    u8cs sub = {};
    u8csMv(sub, u->path);
    call(proj_descend, k, &root_tree, sub, &target, &target_kind);
    if (target_kind != WALK_KIND_DIR) fail(PROJFAIL);

    //  All three buffers acquired from BASS; auto-rewound at procedure
    //  return via caller's call().
    a_carve(u8, tbuf, 1UL << 20);
    u8 otype = 0;
    ok64 go = KEEPGetExact(&target, tbuf, &otype);
    if (go != OK || otype != DOG_OBJ_TREE)
        return go == OK ? PROJFAIL : go;

    //  Format each entry into `text`, with a parallel tok stream so
    //  every entry's name becomes a clickable anchor (next-token-U
    //  convention; see dog/tok/TOK.h).
    a_carve(u8, text, 1UL << 20);
    a_carve(u32, toks, 1UL << 16);

    //  `..` row when we're below the tree root.  No mode/type/sha
    //  prefix — just `..\n` with a tree:<parent>?<rev> U-link.
    u8cs base_path = {};
    u8csMv(base_path, u->path);
    u8cs parent_path = {};
    proj_tree_parent_path(parent_path, base_path);
    b8 below_root = NO;
    {
        //  "below root" iff base_path has any byte that isn't '/'.
        $for(u8c, p, base_path) { if (*p != '/') { below_root = YES; break; } }
    }
    if (below_root) {
        a_cstr(dd, "..");
        a_dup(u8c, ddv, dd);
        (void)u8bFeed(text, ddv);
        proj_push_tok(text, toks, 'F');
        //  Trailing '/' on the parent's link path keeps the tree URI
        //  shape stable (`tree:foo/?ref` vs `tree:foo?ref`).
        a_pad(u8, lp, 4096);
        if (!$empty(parent_path)) {
            (void)u8bFeed(lp, parent_path);
            if (*$last(parent_path) != '/') (void)u8bFeed1(lp, '/');
        }
        a_dup(u8c, lpath, u8bData(lp));
        proj_feed_link_uri(u8bIdle(text), "tree", lpath, u);
        proj_push_tok(text, toks, 'U');
        (void)u8bFeed1(text, '\n');
        proj_push_tok(text, toks, 'W');
    }

    u8cs scan = {u8bDataHead(tbuf), u8bIdleHead(tbuf)};
    u8cs file = {}, esha = {};
    while (GITu8sDrainTree(scan, file, esha, NULL) == OK) {
        u8cs mode_s = {}, name_s = {};
        if (GITu8sFileSplit(file, mode_s, name_s) != OK) continue;
        u8 kind = WALKu8sModeKind(mode_s);
        if (kind == 0) continue;

        //  Prefix: <mode> <type>  <sha40>\t  — tagged 'P' (gray punct).
        proj_tree_mode_type(u8bIdle(text), kind);
        (void)u8bFeed1(text, ' ');
        sha1 esh = {};
        (void)sha1Drain(esha, &esh);
        proj_feed_sha_hex(u8bIdle(text), &esh);
        (void)u8bFeed1(text, '\t');
        proj_push_tok(text, toks, 'P');

        //  Anchor name + U-URI + '\n'.
        b8 is_dir = (kind == WALK_KIND_DIR);
        ok64 ee = proj_tree_emit_entry(text, toks, base_path,
                                       name_s, is_dir, u);
        if (ee != OK) return ee;
    }

    tok32cs toks_view = {(u32 const *)u32bDataHead(toks),
                        (u32 const *)u32bIdleHead(toks)};
    return proj_emit_hunk(u, text, toks_view, tlv);
}

// =====================================================================
//  commit:
// =====================================================================

ok64 KEEPProjCommit(uricp u, b8 tlv) {
    sane(u);
    keeper *k = &KEEP;

    sha1 csha = {};
    call(proj_resolve_object_sha, k, u, &csha);

    a_carve(u8, obj, 1UL << 20);
    u8 otype = 0;
    call(KEEPGetExact, &csha, obj, &otype);

    //  Tag → dereference once to its target object.
    if (otype == DOG_OBJ_TAG) {
        a_dup(u8c, tbody, u8bData(obj));
        u8cs tf = {}, tv = {};
        sha1 tgt = {};
        b8 found = NO;
        while (GITu8sDrainCommit(tbody, tf, tv) == OK) {
            if (u8csEmpty(tf)) break;
            if (u8csLen(tf) == 6 && memcmp(tf[0], "object", 6) == 0 &&
                u8csLen(tv) >= 40) {
                u8s sb = {tgt.data, tgt.data + 20};
                u8cs hx = {tv[0], tv[0] + 40};
                if (HEXu8sDrainSome(sb, hx) == OK) found = YES;
                break;
            }
        }
        if (found) {
            csha = tgt;
            u8bReset(obj);
            otype = 0;
            call(KEEPGetExact, &csha, obj, &otype);
        }
    }

    if (otype != DOG_OBJ_COMMIT) {
        fprintf(stderr, "keeper: commit: object is not a commit (type=%u)\n",
                (unsigned)otype);
        fail(PROJFAIL);
    }

    a_carve(u8, text, 1UL << 20);
    a_carve(u32, toks, 1UL << 14);

    //  Header: "commit <sha40>\n" — no link, this object is the page
    //  itself.  Subsequent `tree <sha>` / `parent <sha>` headers get
    //  U-tagged links so clicks open the referenced object; other
    //  headers (author, committer, message) flow through without toks.
    proj_feed_lit(u8bIdle(text), "commit ");
    proj_push_tok(text, toks, 'R');
    proj_feed_sha_hex(u8bIdle(text), &csha);
    proj_push_tok(text, toks, 'L');
    (void)u8bFeed1(text, '\n');
    proj_push_tok(text, toks, 'W');

    a_cstr(s_tree,   "tree");
    a_cstr(s_parent, "parent");
    a_dup(u8c, body, u8bData(obj));
    u8cs field = {}, value = {};
    while (GITu8sDrainCommit(body, field, value) == OK) {
        if (u8csEmpty(field)) {
            //  Blank-line separator — `value` is the message body.  The
            //  first line (subject) renders BOLD ('N'); the rest plain
            //  ('W'), so the subject stands apart from the diff below.
            (void)u8bFeed1(text, '\n');
            proj_push_tok(text, toks, 'W');
            if (!u8csEmpty(value)) {
                a_dup(u8c, msc, value);
                u8cp send = (u8csFind(msc, '\n') == OK) ? msc[0] : value[1];
                u8cs subj = {value[0], send};
                (void)u8bFeed(text, subj);
                proj_push_tok(text, toks, 'N');
                if (send < value[1]) {
                    u8cs rest = {send, value[1]};
                    (void)u8bFeed(text, rest);
                    proj_push_tok(text, toks, 'W');
                }
            }
            //  Ensure a trailing newline so the message terminates
            //  cleanly even when the commit body lacks one.
            if (!u8csEmpty(value) && *(value[1] - 1) != '\n')
                (void)u8bFeed1(text, '\n');
            break;
        }

        b8 is_tree   = $eq(field, s_tree)   && u8csLen(value) >= 40;
        b8 is_parent = $eq(field, s_parent) && u8csLen(value) >= 40;
        char const *link_scheme = is_tree ? "tree" : (is_parent ? "commit" : NULL);

        (void)u8bFeed(text, field);
        (void)u8bFeed1(text, ' ');
        proj_push_tok(text, toks, 'R');

        if (link_scheme != NULL) {
            //  Anchor: just the sha40 (the clickable bit).
            u8cs sha_hex = {value[0], value[0] + 40};
            (void)u8bFeed(text, sha_hex);
            proj_push_tok(text, toks, 'L');

            //  Invisible URI bytes: <scheme>:?<sha40>.  Hashes live in
            //  the query slot (the general rule — the `#` fragment is
            //  only ever a line or message, never a sha); full 40-hex
            //  here since a commit-view header carries a single hash
            //  (hashlets are reserved for diff's two-hash range).
            //  KEEPProjDispatch promotes this hex query back to the
            //  fragment internally before resolution.
            proj_feed_lit(u8bIdle(text), link_scheme);
            (void)u8bFeed1(text, ':');
            (void)u8bFeed1(text, '?');
            (void)u8bFeed(text, sha_hex);
            proj_push_tok(text, toks, 'U');

            //  Any trailing bytes on the value line (none for git tree/
            //  parent headers, but be tolerant) ride along as default-
            //  tagged.
            if (u8csLen(value) > 40) {
                u8cs rest = {value[0] + 40, value[1]};
                (void)u8bFeed(text, rest);
            }
        } else {
            (void)u8bFeed(text, value);
        }
        (void)u8bFeed1(text, '\n');
        proj_push_tok(text, toks, 'W');
    }

    //  COMMIT-002: keeper emits ONLY the commit-metadata hunk (headers +
    //  message).  The full diff is no longer a navigable link here — the
    //  `be` dispatcher (BEProjector) runs graf's `diff:?<sha>` for the
    //  SAME resolved sha and RELAYS its hunk stream right after this one,
    //  so `commit:?<sha>` reads like `git show` in every render mode.  The
    //  dog boundary stays intact: keeper owns no diff logic (COMMIT-001's
    //  link is superseded; the inline diff is navigable on its own).

    tok32cs toks_view = {(u32 const *)u32bDataHead(toks),
                        (u32 const *)u32bIdleHead(toks)};
    return proj_emit_hunk(u, text, toks_view, tlv);
}

// =====================================================================
//  blob:
// =====================================================================

ok64 KEEPProjBlob(uricp u, b8 tlv) {
    sane(u);
    keeper *k = &KEEP;

    //  KEEPGetByURI handles both `?<sha>` (bare blob) and
    //  `<path>?<ref|sha>` (path-in-tree), so we don't duplicate
    //  resolution here.  Up to 64 MiB blob + parallel tok stream
    //  both carved from BASS (1 GiB default; sized to fit Blob
    //  comfortably).
    a_carve(u8, text, 64UL << 20);
    call(KEEPGetByURI, u, text);

    if (!tlv) {
        a_dup(u8c, data, u8bData(text));
        write(STDOUT_FILENO, data[0], u8csLen(data));
        done;
    }

    //  TLV: tokenize via dog/TOK using the path's extension so bro
    //  can render syntax highlighting.  Without an extension (bare
    //  `blob:?<sha>` or unknown ext) there's no language hint and we
    //  ship the bytes unhighlighted — bro still pages them via HUNK.
    a_carve(u32, tok_arena, (size_t)(u8bDataLen(text) + 16));

    tok32cs toks_slice = {NULL, NULL};
    if (!u8csEmpty(u->path)) {
        u8cs ext = {};
        a_dup(u8c, ps, u->path);
        PATHu8sExt(ext, ps);
        if (!$empty(ext) && TOKKnownExt(ext)) {
            u8cs source = {u8bDataHead(text), u8bIdleHead(text)};
            u32 *begin = u32bIdleHead(tok_arena);
            ok64 to = HUNKu32bTokenize(tok_arena, source, ext);
            if (to == OK) {
                u32 *end = u32bIdleHead(tok_arena);
                toks_slice[0] = (u32 const *)begin;
                toks_slice[1] = (u32 const *)end;
            }
        }
    }

    return proj_emit_hunk(u, text, toks_slice, YES);
}

//  sha1: — emit 40-hex SHA-1 of the resource + newline.
ok64 KEEPProjSha1(uricp u, b8 tlv) {
    sane(u);
    keeper *k = &KEEP;

    sha1 target = {};

    if (!u8csEmpty(u->path)) {
        //  Path-bearing: resolve to the containing tree, descend the
        //  path, target is whatever sits there (blob or subtree sha).
        sha1 root_tree = {};
        call(KEEPResolveTree, u, &root_tree);
        u8 target_kind = 0;
        u8cs sub = {};
        u8csMv(sub, u->path);
        call(proj_descend, k, &root_tree, sub, &target, &target_kind);
    } else if (!u8csEmpty(u->query) || !u8csEmpty(u->fragment)) {
        //  Ref/hex form: tip sha of the named ref, or resolved sha
        //  for the (possibly short) hex prefix.
        call(proj_resolve_object_sha, k, u, &target);
    } else {
        //  Bare `sha1:` — cur's tip.  REFSResolve with the trunk
        //  key (`?`) goes through the same path as `?<branch>` but
        //  for an empty branch name (= trunk per the recv_build_key
        //  convention).
        a_path(keepdir);
        call(HOMEBranchDir, keepdir, NULL);
        a_pad(u8, arena_buf, 1024);
        uri resolved = {};
        a_cstr(trunk_uri, "?");
        ok64 ro = REFSResolve(&resolved, arena_buf,
                              $path(keepdir), trunk_uri);
        if (ro != OK || u8csLen(resolved.query) < 40) fail(PROJNONE);
        u8s sb = {target.data, target.data + 20};
        u8cs hx = {resolved.query[0], resolved.query[0] + 40};
        call(HEXu8sDrainSome, sb, hx);
    }

    //  Emit 40 hex + newline.  41 bytes total — small enough to skip
    //  TLV/hunk framing entirely; bare-sha output is a shell-friendly
    //  one-liner regardless of tty.  (tlv arg accepted for ABI
    //  uniformity with the other projectors.)
    (void)tlv;
    a_pad(u8, line, 64);
    proj_feed_sha_hex(u8bIdle(line), &target);
    (void)u8bFeed1(line, '\n');
    a_dup(u8c, payload, u8bData(line));
    write(STDOUT_FILENO, payload[0], u8csLen(payload));
    done;
}

// =====================================================================
//  Dispatch
// =====================================================================

//  YES iff `s` is hex-only and at least HASH_MIN_HEX, at most 40 chars.
//  Used to spot a sha-prefix query (e.g. `?abc1234`) so it can be
//  routed through keeper's hashlet index — which accepts any prefix
//  ≥ HASH_MIN_HEX — by promoting it into the fragment slot.
static b8 proj_is_hex_prefix(u8cs s) {
    size_t n = (size_t)u8csLen(s);
    if (n < HASH_MIN_HEX || n > 40) return NO;
    $for(u8c, p, s) {
        u8 c = *p;
        if (!((c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) return NO;
    }
    return YES;
}

ok64 KEEPProjDispatch(uricp u, b8 tlv) {
    sane(u);
    if (u8csEmpty(u->scheme)) fail(PROJFAIL);

    a_cstr(s_tree,   "tree");
    a_cstr(s_commit, "commit");
    a_cstr(s_blob,   "blob");
    a_cstr(s_sha1,   "sha1");

    //  Per https://replicated.wiki/html/wiki/URI.html §"Ref resolution", `?<hex-prefix>` is a sha
    //  lookup.  KEEPResolveTree (used by tree:/commit: and the
    //  path-bearing blob: form) only special-cases the fragment slot;
    //  mirror the user's intent into the fragment so any prefix from
    //  HASH_MIN_HEX through 40 chars routes through KEEPGet's hashlet
    //  index.  `blob:?<hex>` (bare blob, empty path) stays untouched —
    //  KEEPGetByURI has its own hashlet branch for that shape.
    //
    //  When the query carries the hex prefix it is the authoritative
    //  address; any fragment present is a human message LABEL (e.g.
    //  blame's `commit:?<8hex>#<subject>`), not an object id, so the
    //  promotion overwrites it — resolution must follow the hex, never
    //  try to parse the message as a sha.
    uri local = *u;
    b8 bare_blob = $eq(local.scheme, s_blob) && u8csEmpty(local.path);
    if (!bare_blob && proj_is_hex_prefix(local.query)) {
        u8csMv(local.fragment, local.query);
        local.query[0] = NULL;
        local.query[1] = NULL;
    }
    uricp un = (uricp)&local;

    if ($eq(un->scheme, s_tree))   return KEEPProjTree(un, tlv);
    if ($eq(un->scheme, s_commit)) return KEEPProjCommit(un, tlv);
    if ($eq(un->scheme, s_blob))   return KEEPProjBlob(un, tlv);
    if ($eq(un->scheme, s_sha1))   return KEEPProjSha1(un, tlv);

    //  DOG_PROJECTORS routed this scheme to keeper but no handler is
    //  wired here.  Surface that explicitly so the gap is obvious.
    fprintf(stderr, "keeper: projector '%.*s:' not implemented\n",
            (int)$len(un->scheme), (char *)un->scheme[0]);
    fail(PROJNONE);
}
