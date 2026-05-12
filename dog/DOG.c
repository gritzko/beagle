#include "DOG.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "abc/B.h"
#include "abc/FILE.h"
#include "abc/PRO.h"

// kv32 buffer ops via Bx (instantiated in abc/KV.h).

// --- Canonical layout-name slices (extern'd in DOG.h) ----------------
//
// `u8cs`-shaped pointer pairs over the layout-name string literals,
// created via the standard `a_cstr` macro at file scope so callers
// can pass them where a slice is wanted (e.g.
// `a_path(p, root, DOG_BE_S)`).
a_cstr(DOG_BE_S,     DOG_BE_NAME);
a_cstr(DOG_REFS_S,   DOG_REFS_NAME);
a_cstr(DOG_WTLOG_S,  DOG_WTLOG_NAME);
a_cstr(DOG_CONFIG_S, DOG_CONFIG_NAME);

//  Known view-projector schemes (VERBS.md §"View projectors") with
//  the dog that implements each.  Shared source of truth: DOGParseURI
//  uses the scheme column to exempt these from the scheme→authority
//  promotion; BE (beagle/BE.cli.c) uses the dog column to dispatch
//  `be <proj>:<URI>` to the right dog.  Projectors are *never*
//  transports — the scheme selects what shape of bytes to emit.
static DOGProjRoute const DOG_PROJECTORS[] = {
    {"sha1",   "keeper"},
    {"blob",   "keeper"},
    {"tree",   "keeper"},
    {"commit", "keeper"},
    {"log",    "graf"},
    {"refs",   "keeper"},
    {"size",   "keeper"},
    {"type",   "keeper"},
    {"diff",   "graf"},
    {"blame",  "graf"},
    {"weave",  "graf"},
    {"map",    "graf"},
    {"ls",     "sniff"},
    //  Search projectors — read-only views (VERBS.md §"View projectors").
    //  Scheme picks the search backend; path-slot carries the body.
    //  Examples:
    //    be spot:'u8sFeed( a, b )'      structural
    //    be regex:'u\d+sFeed'           PCRE
    //    be grep:u8sFeed                literal
    {"spot",   "spot"},
    {"grep",   "spot"},
    {"regex",  "spot"},
    {NULL,     NULL}
};

static DOGProjRoute const *dog_proj_lookup(u8cs scheme) {
    if (u8csEmpty(scheme)) return NULL;
    for (DOGProjRoute const *p = DOG_PROJECTORS; p->scheme; p++) {
        a_cstr(p_s, p->scheme);
        if (u8csEq(scheme, p_s)) return p;
    }
    return NULL;
}

b8 DOGIsProjector(u8cs scheme) {
    return dog_proj_lookup(scheme) != NULL;
}

char const *DOGProjectorDog(u8cs scheme) {
    DOGProjRoute const *r = dog_proj_lookup(scheme);
    return r ? r->dog : NULL;
}

//  Known transport schemes (VERBS.md §"Transport schemes").  Kept as a
//  table next to DOG_PROJECTORS so adding a transport is a one-row
//  edit.  CLI.c uses this to disambiguate `word:` — known scheme = URI,
//  unknown = prose (e.g. a `cli:` conventional-commit prefix).
static char const *const DOG_TRANSPORTS[] = {
    "ssh", "https", "http", "git", "file", "be", NULL
};

b8 DOGIsTransport(u8cs scheme) {
    if (u8csEmpty(scheme)) return NO;
    for (char const *const *t = DOG_TRANSPORTS; *t; t++) {
        a_cstr(t_s, *t);
        if (u8csEq(scheme, t_s)) return YES;
    }
    return NO;
}

static b8 dog_is_projector(u8cs scheme) {
    return DOGIsProjector(scheme);
}

ok64 DOGParseURI(urip uri, u8csc text) {
    sane(uri != NULL);
    memset(uri, 0, sizeof(*uri));
    uri->data[0] = text[0];
    uri->data[1] = text[1];
    call(URILexer, uri);

    // Dog normalization: bare `host:path` with no `//` ends up
    // as scheme="host", path="path".  Promote scheme to authority
    // when authority is empty and the path has no leading slash
    // (proper RFC schemes have either an authority or a path
    // starting with '/').  Projector schemes are exempt — `tree:src/`,
    // `ls:subdir`, etc. keep `tree` / `ls` as the scheme.
    if (!$empty(uri->scheme) && $empty(uri->authority)
        && !dog_is_projector(uri->scheme)) {
        b8 rooted = !$empty(uri->path) && $at(uri->path, 0) == '/';
        if (!rooted) {
            u8csMv(uri->authority, uri->scheme);
            u8csMv(uri->host, uri->scheme);
            uri->scheme[0] = NULL;
            uri->scheme[1] = NULL;
        }
    }

    // Dog normalization: `user@host/path` (no `//`) parses as one big
    // path.  If path's head-segment contains `@`, promote it to
    // authority+host and leave `/rest` as path.  Handles the ergonomic
    // `gritzko@pm.me/dogs-sniff?ref` form without requiring `//`.
    if ($empty(uri->authority) && !$empty(uri->path)) {
        u8cp slash = NULL;
        u8cp at    = NULL;
        for (u8cp p = uri->path[0]; p < uri->path[1]; p++) {
            if (*p == '/' && slash == NULL) { slash = p; break; }
            if (*p == '@' && at == NULL)    at = p;
        }
        u8cp head_end = slash ? slash : uri->path[1];
        if (at != NULL && at < head_end) {
            u8cp auth_start = uri->path[0];
            uri->authority[0] = auth_start;
            uri->authority[1] = head_end;
            uri->user[0] = auth_start;
            uri->user[1] = at;
            uri->host[0] = at + 1;
            uri->host[1] = head_end;
            if (slash != NULL) {
                uri->path[0] = slash;      // keep leading '/'
            } else {
                uri->path[0] = NULL;
                uri->path[1] = NULL;
            }
        }
    }

    // Dog normalization: symbolic "ports" are overwhelmingly path
    // segments that got eaten by RFC 3986 (`ssh://host:src/dogs-sniff`
    // — `src` is not a port).  If the port slice has any non-digit
    // byte, glue it onto the front of the path and clear the port.
    if (!$empty(uri->port)) {
        b8 numeric = YES;
        $for(u8c, p, uri->port) {
            if (*p < '0' || *p > '9') { numeric = NO; break; }
        }
        if (!numeric) {
            // port and path slices are contiguous in the original
            // text (port ends exactly where path begins), so the
            // union slice is {port[0], old_path_end_or_port_end}.
            u8cp new_path_end = $empty(uri->path) ? uri->port[1]
                                                  : uri->path[1];
            uri->path[0] = uri->port[0];
            uri->path[1] = new_path_end;
            // Trim the ':' from authority/host by pointing their
            // end at port[0]-1 (one byte back over the colon).
            if (!$empty(uri->host))      uri->host[1]      = uri->port[0] - 1;
            if (!$empty(uri->authority)) uri->authority[1] = uri->port[0] - 1;
            uri->port[0] = NULL;
            uri->port[1] = NULL;
        }
    }

    done;
}

ok64 DOGNormalizeArg(urip u, u8csc arg) {
    sane(u != NULL);
    zerop(u);
    if (u8csEmpty(arg)) done;

    // If the arg contains `?` or `#` or `/` it's already URI-shaped;
    // if it contains whitespace it can't be a URI at all.  Classify
    // up front so we don't feed whitespace to URILexer.
    b8 has_ws     = NO;
    b8 has_mark   = NO;   // ? or #
    b8 has_slash  = NO;
    b8 has_colon  = NO;
    $for(u8c, p, arg) {
        u8 c = *p;
        if (c <= 0x20 || c == 0x7f) { has_ws = YES; continue; }
        if (c == '?' || c == '#')   { has_mark = YES; }
        if (c == '/')               { has_slash = YES; }
        if (c == ':')               { has_colon = YES; }
    }

    // URI vs free-text disambiguation for whitespace tokens.  A token
    // is URI-shaped only when it *starts* with one of the URI sigils:
    //   `?` query, `#` fragment, `/` (incl. `//`) authority/path,
    //   `<scheme>:` transport or projector.
    // Plain whitespace tokens — `fix the typo`, `URI/verb routing`,
    // `#wild msg` — get the bypass into the fragment slot.  This way
    // `spot:'u8sFeed( a, b )'` reaches URILexer, but `URI/verb routing`
    // doesn't get parsed as a path-form URI.
    b8 starts_uri = NO;
    if (!u8csEmpty(arg)) {
        u8 c0 = arg[0][0];
        if (c0 == '?' || c0 == '#' || c0 == '/') starts_uri = YES;
        else if (has_colon) {
            //  Scheme sits before the first `:` and is alpha (alpha|digit|+|-|.)*.
            //  If we hit any other char before `:`, this isn't a scheme.
            for (u8cp p = arg[0]; p < arg[1]; p++) {
                u8 c = *p;
                if (c == ':') { starts_uri = (p > arg[0]); break; }
                b8 alpha = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
                b8 digit = (c >= '0' && c <= '9');
                b8 ext   = (c == '+' || c == '-' || c == '.');
                if (p == arg[0]) { if (!alpha) break; }
                else { if (!alpha && !digit && !ext) break; }
            }
        }
    }

    if (starts_uri) {
        // Path may contain whitespace per RFC pchar's `unwise` set;
        // URILexer handles it.
        return DOGParseURI(u, arg);
    }

    if (has_ws) {
        // Free-text token (commit messages, search prose) → fragment.
        u->data[0]      = arg[0];
        u->data[1]      = arg[1];
        u->fragment[0]  = arg[0];
        u->fragment[1]  = arg[1];
        done;
    }

    // Structural char in a non-whitespace token → URI parser.
    if (has_mark || has_slash || has_colon) {
        return DOGParseURI(u, arg);
    }

    // Bare single token — RFC 3986 path-noscheme.  No charset-based
    // classification here: a bare `README`, `VERBS.md`, or 40-hex
    // string all parse as path.  Per-verb routing (POST → fragment,
    // GET/HEAD/PATCH → query, PUT/DELETE/verbless → path) is applied
    // post-parse via DOGPromoteBareword once the verb is known.
    u->data[0] = arg[0];
    u->data[1] = arg[1];
    u->path[0] = arg[0];
    u->path[1] = arg[1];
    done;
}

ok64 DOGPromoteBareword(urip u, u8 slot) {
    sane(u != NULL);
    if (slot != 'q' && slot != 'f') done;

    //  Bareword shape: path is the only populated component, and it
    //  has no '/' (a slash would have routed the arg through
    //  URILexer in DOGNormalizeArg, so the path is path-shaped, not
    //  a bare branch / message word).
    if (u->path[0] == NULL || u8csEmpty(u->path)) done;
    if (u->scheme[0]    != NULL) done;
    if (u->authority[0] != NULL) done;
    if (u->host[0]      != NULL) done;
    if (u->query[0]     != NULL) done;
    if (u->fragment[0]  != NULL) done;
    $for(u8c, p, u->path) {
        if (*p == '/') done;
    }

    if (slot == 'q') {
        u->query[0] = u->path[0];
        u->query[1] = u->path[1];
    } else {  //  'f'
        u->fragment[0] = u->path[0];
        u->fragment[1] = u->path[1];
    }
    u->path[0] = NULL;
    u->path[1] = NULL;
    done;
}

//  Presence vs emptiness for query / fragment slices:
//    s[0] == NULL                  → component absent (no `?` / no `#`)
//    s[0] != NULL, $empty(s)       → present-but-empty (bare `?` / `#`)
//    non-empty                     → component with text
//  Collapse/strip must preserve the present-but-empty state (point
//  both endpoints at the tail of the original text), not wipe it to
//  absent, or the row shape (`?#sha`, `?branch#`) is lost.
ok64 DOGCanonURI(urip u) {
    sane(u != NULL);

    //  Query is an opaque hierarchical local branch path.  Trunk =
    //  empty (`?`) or a lone `/` (`?/`) — the latter folds to the
    //  former so the on-disk key is unique.  No name-aware aliasing
    //  (no `refs/`, `heads/`, `main`/`master`/`trunk` collapsing) —
    //  git refname conventions live behind keeper/GIT.h's
    //  GITParseRef / GITFeedRef and apply only on the wire.
    if (!u8csEmpty(u->query)) {
        u8cs q = {u->query[0], u->query[1]};
        if ($len(q) == 1 && *q[0] == '/') {
            u->query[0] = q[1];
            u->query[1] = q[1];
        }
    }

    //  Fragment: drop a leading `?` so the value slot is a bare
    //  40-hex SHA (or empty = deletion).
    if (!u8csEmpty(u->fragment)) {
        u8cs f = {u->fragment[0], u->fragment[1]};
        if ($len(f) >= 1 && *f[0] == '?')
            u8csUsed1(f);
        if ($empty(f)) {
            u->fragment[0] = u->fragment[1];
        } else {
            u->fragment[0] = f[0];
            u->fragment[1] = f[1];
        }
    }

    done;
}

b8 DOGIsHashlet(u8cs s) {
    size_t n = u8csLen(s);
    if (n < 6 || n > 40) return NO;
    for (size_t i = 0; i < n; i++) {
        u8 c = s[0][i];
        if (!((c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) return NO;
    }
    return YES;
}

b8 DOGRefIsBranch(u8cs ref) {
    size_t n = u8csLen(ref);
    if (n == 0) return YES;  // trunk
    for (size_t i = 0; i < n; i++)
        if (ref[0][i] == '/') return YES;
    if (n == 1 && ref[0][0] == '.') return YES;
    if (n == 2 && ref[0][0] == '.' && ref[0][1] == '.') return YES;
    return NO;
}

void DOGRefSplitPin(u8cs query, u8csp branch_out, u8csp pin_out) {
    branch_out[0] = NULL; branch_out[1] = NULL;
    pin_out[0]    = NULL; pin_out[1]    = NULL;
    if (u8csLen(query) == 0) return;

    //  Find the last '/'-separated segment.  When query is just
    //  `abc1234` (no '/'), the whole slice is the candidate.
    u8cp head  = query[0];
    u8cp term  = query[1];
    u8cp slash = NULL;
    for (u8cp p = head; p < term; p++) if (*p == '/') slash = p;

    u8cs tail = {};
    if (slash) { tail[0] = slash + 1; tail[1] = term; }
    else       { tail[0] = head;      tail[1] = term; }

    if (DOGIsHashlet(tail)) {
        //  Trailing hashlet: split branch / pin at the last '/'.
        //  When no '/' is present the whole query is the pin and
        //  the branch is empty (trunk).
        if (slash) {
            branch_out[0] = head;
            branch_out[1] = slash;
        }
        pin_out[0] = tail[0];
        pin_out[1] = tail[1];
    } else {
        branch_out[0] = head;
        branch_out[1] = term;
    }
}

ok64 DOGCanonURIFeed(u8bp out, urip u) {
    sane(out != NULL && u != NULL);
    call(DOGCanonURI, u);

    //  Emit scheme verbatim (if any): cross-scheme identity is the
    //  job of the lookup-side host-substring matcher, not the
    //  storage key.  Keeping the scheme preserves access method info
    //  (ssh vs https vs file vs be://) for debugging and re-fetch.
    if (!u8csEmpty(u->scheme)) {
        u8bFeed(out, u->scheme);
        u8bFeed1(out, ':');
    }
    if (!u8csEmpty(u->authority) || !u8csEmpty(u->host)) {
        a_cstr(slashes, "//");
        u8bFeed(out, slashes);
        u8cs auth = {u->authority[0], u->authority[1]};
        if ($len(auth) >= 2 && auth[0][0] == '/' && auth[0][1] == '/')
            u8csUsed(auth, 2);
        u8bFeed(out, auth);
        //  `host`+`/path` stays as-is; `host`+`path` gets a `/`.
        //  `ssh://host:x` and `ssh://host/x` thus produce the same key.
        if (!u8csEmpty(u->path)) {
            if ($at(u->path, 0) != '/') u8bFeed1(out, '/');
            u8bFeed(out, u->path);
        }
    } else if (!u8csEmpty(u->path)) {
        u8bFeed(out, u->path);
    }
    //  Presence check is [0] != NULL (empty-but-present still emits
    //  the sigil so `?#sha` and `?branch#` round-trip).
    if (u->query[0] != NULL) {
        u8bFeed1(out, '?');
        if (!u8csEmpty(u->query)) u8bFeed(out, u->query);
    }
    if (u->fragment[0] != NULL) {
        u8bFeed1(out, '#');
        if (!u8csEmpty(u->fragment)) u8bFeed(out, u->fragment);
    }
    done;
}

// =============================================================
// --- Puppies: stack of <seqno>.<ext> files ---
// =============================================================

//  Compose `<dir>/<10-RON64-seqno><ext>` into `out` (reset first).
static ok64 dog_pup_path(path8b out, path8s dir, u32 seqno, u8cs ext) {
    sane(u8bOK(out));
    call(PATHu8bDup, out, dir);
    a_pad(u8, name, DOG_PUP_SEQNO_W + 32);
    call(RONu8sFeedPad, u8bIdle(name), (ok64)seqno, DOG_PUP_SEQNO_W);
    ((u8 **)name)[2] += DOG_PUP_SEQNO_W;
    call(u8bFeed, name, ext);
    call(PATHu8bPush, out, u8bDataC(name));
    done;
}

//  Parse "<10-RON64><ext>" filename → seqno; 0 on bad fmt.
static u32 dog_pup_parse_seqno(u8csc name, u8csc ext) {
    if (u8csLen(name) != DOG_PUP_SEQNO_W + u8csLen(ext)) return 0;
    a_dup(u8c, tail, name);
    u8csUsed(tail, DOG_PUP_SEQNO_W);
    if (!u8csEq(tail, ext)) return 0;
    u8cs seqno_s = {name[0], name[0] + DOG_PUP_SEQNO_W};
    ok64 v = 0;
    if (RONutf8sDrain(&v, seqno_s) != OK) return 0;
    return (u32)v;
}

ok64 DOGPupOpenAll(kv32b pups, path8sc dir, u8csc ext) {
    sane(pups != NULL && u8csOK(dir) && u8csOK(ext));

    //  Two-phase: walk directory with FILEIter to collect (seqno → name)
    //  pairs in a Bkv32, sort by key, then map each file in order.
    //  Keeping the iterator and FILEMapRO disjoint avoids any aliasing
    //  between the iterator's path buffer and FILEMapRO's bookkeeping.
    a_path(dpat, dir);
    fileit it = {};
    if (FILEIterOpen(&it, dpat) != OK) done;  // dir absent → empty stack

    Bkv32 seqnos = {};
    call(kv32bAllocate, seqnos, FILE_MAX_OPEN);
    //  names are <=21 bytes (10 seqno + 11 ext); 32 is plenty.
    a_pad(u8, namebuf, FILE_MAX_OPEN * 32);

    scan(FILENext, &it) {
        if (it.type != DT_REG) continue;
        u8cs base = {};
        PATHu8sBase(base, u8bDataC(it.path));
        u32 sq = dog_pup_parse_seqno(base, ext);
        if (sq == 0) continue;
        if ($len(u8bIdleC(namebuf)) < u8csLen(base) + 1) continue;
        u32 name_off = (u32)u8bDataLen(namebuf);
        u8bFeed(namebuf, base);
        u8bFeed1(namebuf, 0);
        kv32 kv = {.key = sq, .val = name_off};
        if (kv32bPush(seqnos, &kv) != OK) break;
    }
    seen(END);
    FILEIterClose(&it);

    a_dup(kv32, ks, kv32bData(seqnos));
    $kv32sort(ks);

    //  Skip seqnos already loaded — re-opens of shared ancestor dirs
    //  (cross-branch loads, idempotent re-scans) must not double-map.
    //  Scan PastData since duplicates can appear in either side.
    $for(kv32, kp, ks) {
        b8 dup = NO;
        {
            kv32s pd = {};
            kv32PastDataS(pups, pd);
            $for(kv32, q, pd) {
                if (q->key == kp->key) { dup = YES; break; }
            }
        }
        if (dup) continue;
        u8cp nm0 = u8bDataHead(namebuf) + kp->val;
        u8cs fn  = {nm0, nm0 + strlen((char const *)nm0)};
        a_path(fpath, dir, fn);
        u8bp buf = NULL;
        if (FILEMapRO(&buf, $path(fpath)) != OK) continue;
        int fd = FILEBookedFD(buf);
        if (fd < 0) { FILEUnMap(buf); continue; }
        kv32 kv = {.key = kp->key, .val = (u32)fd};
        call(kv32bPush, pups, &kv);
    }

    kv32bFree(seqnos);
    done;
}

ok64 DOGPupOpenAside(kv32b pups, path8sc dir, u8csc ext) {
    sane(pups != NULL && u8csOK(dir) && u8csOK(ext));
    //  Collapse the current DATA into PAST: the previously-active
    //  leaf becomes part of the read-only context.  pups[1] is the
    //  PAST/DATA boundary; advancing it to pups[2] (the DATA/IDLE
    //  boundary) leaves DATA empty and ready for the next branch's
    //  entries.
    if (kv32bDataLen(pups) > 0)
        ((kv32 **)pups)[1] = (kv32 *)pups[2];
    return DOGPupOpenAll(pups, dir, ext);
}

ok64 DOGPupCreateAt(kv32b pups, path8s dir, u8cs ext, u8cs bytes,
                    u32 seqno) {
    sane(pups != NULL && $ok(dir) && $ok(ext) && seqno > 0);

    //  Refuse on DATA-side seqno collision (caller is meant to
    //  DOGPupThinTail first when replacing the tail run).
    {
        kv32 const *db = (kv32 const *)kv32bDataHead(pups);
        kv32 const *de = (kv32 const *)kv32bIdleHead(pups);
        for (kv32 const *p = db; p < de; p++)
            if (p->key == seqno) return DOGPUPFAIL;
    }

    a_path(idxpath);
    call(dog_pup_path, idxpath, dir, seqno, ext);
    a_path(tmppath);
    call(dog_pup_path, tmppath, dir, seqno, ext);
    a_cstr(tmpsuf, ".tmp");
    call(u8bFeed, tmppath, tmpsuf);
    call(PATHu8bTerm, tmppath);

    int wfd = -1;
    call(FILECreate, &wfd, $path(tmppath));
    if (wfd >= 0) {
        FILEFeedAll(wfd, bytes);
        close(wfd);
    }
    call(FILERename, $path(tmppath), $path(idxpath));

    u8bp buf = NULL;
    call(FILEMapRO, &buf, $path(idxpath));
    int fd = FILEBookedFD(buf);
    if (fd < 0) { FILEUnMap(buf); return DOGPUPFAIL; }
    kv32 kv = {.key = seqno, .val = (u32)fd};
    call(kv32bPush, pups, &kv);
    done;
}

ok64 DOGPupCreate(kv32b pups, path8s dir, u8cs ext, u8cs bytes) {
    sane(pups != NULL);
    //  new_seqno = 1 + max(DATA seqno), default 1 for empty DATA.
    u32 new_seqno = 1;
    {
        kv32 const *db = (kv32 const *)kv32bDataHead(pups);
        kv32 const *de = (kv32 const *)kv32bIdleHead(pups);
        for (kv32 const *p = db; p < de; p++)
            if (p->key >= new_seqno) new_seqno = p->key + 1;
    }
    return DOGPupCreateAt(pups, dir, ext, bytes, new_seqno);
}

ok64 DOGPupThinTail(kv32b pups, path8s dir, u8cs ext, u32 m) {
    sane(pups != NULL && $ok(dir) && $ok(ext));
    u32 n = (u32)kv32bDataLen(pups);
    if (m > n) m = n;
    if (m == 0) done;
    kv32 *base = (kv32 *)kv32bDataHead(pups);
    for (u32 i = n - m; i < n; i++) {
        u32 fd = base[i].val;
        u32 sq = base[i].key;
        u8bp slot = FILE_WANT_BUFS[fd];
        if (slot && slot[0]) FILEUnMap(slot);
        a_path(ulpath);
        dog_pup_path(ulpath, dir, sq, ext);
        unlink((char *)u8bDataHead(ulpath));
    }
    //  Trim data end back by m records.  pups[2] is data end.
    ((kv32 **)pups)[2] = base + (n - m);
    done;
}

ok64 DOGPupClose(kv32b pups) {
    sane(pups != NULL);
    if (BNULL(pups)) done;
    kv32 *base = (kv32 *)kv32bDataHead(pups);
    u32 n = (u32)kv32bDataLen(pups);
    for (u32 i = 0; i < n; i++) {
        u32 fd = base[i].val;
        u8bp slot = FILE_WANT_BUFS[fd];
        if (slot && slot[0]) FILEUnMap(slot);
    }
    kv32bFree(pups);
    done;
}

// =============================================================
// --- Short human date formatter ---
// =============================================================

static char const *const DOG_MONS[12] = {
    "Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
};
static char const *const DOG_WDAYS[7] = {
    "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};

ok64 DOGutf8sFeedDate(u8s into, i64 ts, i64 now) {
    sane($ok(into));

    char buf[8];
    int n = 0;

    if (ts <= 0) {
        n = snprintf(buf, sizeof(buf), "?");
    } else {
        i64 diff = now - ts;
        time_t t  = (time_t)ts;
        time_t tn = (time_t)now;
        //  localtime so HH:MM and the day-boundary for today/weekday
        //  follow the user's wall clock.  Returns a pointer to a
        //  shared static buffer; copy each result before the next call
        //  clobbers it.
        struct tm tt = {}, tnn = {};
        b8 ok_t = NO, ok_n = NO;
        struct tm *p = localtime(&t);
        if (p) { tt = *p; ok_t = YES; }
        p = localtime(&tn);
        if (p) { tnn = *p; ok_n = YES; }

        b8 same_day  = ok_t && ok_n &&
                       tt.tm_year == tnn.tm_year &&
                       tt.tm_yday == tnn.tm_yday;
        b8 less_12h  = diff >= 0 && diff < 12 * 3600;
        b8 less_7d   = diff >= 0 && diff < 7 * 86400;
        //  ~6 months as 183 days — coarse threshold is fine, the
        //  same-year fallback catches the rest.
        b8 less_6mo  = diff >= 0 && diff < 183 * 86400;
        b8 same_year = ok_t && ok_n && tt.tm_year == tnn.tm_year;

        if (!ok_t) {
            n = snprintf(buf, sizeof(buf), "?");
        } else if (less_12h || same_day) {
            n = snprintf(buf, sizeof(buf), "%02d:%02d",
                         tt.tm_hour, tt.tm_min);
        } else if (less_7d) {
            n = snprintf(buf, sizeof(buf), "%s%02d",
                         DOG_WDAYS[tt.tm_wday], tt.tm_mday);
        } else if (less_6mo || same_year) {
            n = snprintf(buf, sizeof(buf), "%02d%s",
                         tt.tm_mday, DOG_MONS[tt.tm_mon]);
        } else {
            n = snprintf(buf, sizeof(buf), "%02d%s%02d",
                         tt.tm_mday, DOG_MONS[tt.tm_mon],
                         tt.tm_year % 100);
        }
    }

    if (n < 0) n = 0;
    if (n > (int)sizeof(buf) - 1) n = (int)sizeof(buf) - 1;

    //  Pad to exactly 7 columns, centred — so a column of dates lines
    //  up cleanly.  Widths emitted are 1 ("?"), 5 ("HH:MM" / "WdyDD" /
    //  "DDMon"), or 7 ("DDMonYY"); all share parity with 7, so centred
    //  padding is symmetric.
    int pad_total = 7 - n;
    if (pad_total < 0) pad_total = 0;
    int pad_left  = pad_total / 2;
    int pad_right = pad_total - pad_left;
    for (int i = 0; i < pad_left; i++) call(u8sFeed1, into, ' ');
    u8cs sl = {(u8 const *)buf, (u8 const *)buf + n};
    call(u8sFeed, into, sl);
    for (int i = 0; i < pad_right; i++) call(u8sFeed1, into, ' ');
    done;
}

void DOGPupData(u8csp out, kv32b pups, u32 i) {
    out[0] = NULL; out[1] = NULL;
    if (i >= kv32bDataLen(pups)) return;
    kv32 *base = (kv32 *)kv32bDataHead(pups);
    u32 fd = base[i].val;
    u8bp slot = FILE_WANT_BUFS[fd];
    if (!slot || !slot[0]) return;
    out[0] = slot[0];
    out[1] = slot[2];
}

void DOGPupDataAll(u8csp out, kv32b pups, u32 i) {
    out[0] = NULL; out[1] = NULL;
    kv32s all = {};
    kv32PastDataS(pups, all);
    size_t n = (size_t)(all[1] - all[0]);
    if ((size_t)i >= n) return;
    u32 fd = all[0][i].val;
    u8bp slot = FILE_WANT_BUFS[fd];
    if (!slot || !slot[0]) return;
    out[0] = slot[0];
    out[1] = slot[2];
}

ok64 DOGPupAllData(u8csb out, kv32b pups) {
    sane(out && pups);
    u8csbReset(out);
    u32 n = (u32)kv32bDataLen(pups);
    for (u32 i = 0; i < n; i++) {
        u8cs s = {};
        DOGPupData(s, pups, i);
        if (s[0] == NULL) continue;
        call(u8csbFeed1, out, s);
    }
    done;
}
