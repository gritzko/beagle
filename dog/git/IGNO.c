//  IGNO: .gitignore parser and matcher using cc/PATH
//
#include "IGNO.h"

#include <string.h>

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "DOG.h"

static a_cstr(GITIGNORE_NAME, ".gitignore");

// Gitignore-style glob: * matches non-/, ** matches everything, ? matches one.
// Both arguments are consumed locally via a_dup; recursion runs on sub-slices.
static b8 IGNOGlob(u8csc pat_in, u8csc str_in) {
    a_dup(u8c, pat, pat_in);
    a_dup(u8c, str, str_in);
    while (!u8csEmpty(pat) && !u8csEmpty(str)) {
        u8 pc = *u8csHead(pat);
        if (pc == '*') {
            b8 dbl = u8csLen(pat) >= 2 && *u8csAtP(pat, 1) == '*';
            if (dbl) {
                u8csUsed(pat, 2);
                if (!u8csEmpty(pat) && *u8csHead(pat) == '/') u8csUsed1(pat);
                // ** matches everything including '/': try every suffix.
                a_dup(u8c, tail, str);
                for (;;) {
                    if (IGNOGlob(pat, tail)) return YES;
                    if (u8csEmpty(tail)) break;
                    u8csUsed1(tail);
                }
                return NO;
            }
            u8csUsed1(pat);
            // * matches anything except '/': try suffixes, stop past a '/'.
            a_dup(u8c, tail, str);
            for (;;) {
                if (IGNOGlob(pat, tail)) return YES;
                if (u8csEmpty(tail)) break;
                if (*u8csHead(tail) == '/') break;
                u8csUsed1(tail);
            }
            return NO;
        }
        if (pc == '?') {
            if (*u8csHead(str) == '/') return NO;
            u8csUsed1(pat);
            u8csUsed1(str);
            continue;
        }
        if (pc != *u8csHead(str)) return NO;
        u8csUsed1(pat);
        u8csUsed1(str);
    }
    while (!u8csEmpty(pat) && *u8csHead(pat) == '*') u8csUsed1(pat);
    return u8csEmpty(pat) && u8csEmpty(str);
}

// Try to match pattern against path
static b8 TryMatch(igno_pat const *pat, u8cs path, b8 is_dir) {
    // Directory-only patterns don't match files
    if (pat->dir_only && !is_dir) return NO;

    u8cs pattern;
    u8csDup(pattern, pat->pattern);
    if ($empty(pattern)) return NO;

    // Strip leading / from pattern for matching (we handle anchoring separately)
    u8cs match_pattern;
    u8csDup(match_pattern, pattern);
    if (!u8csEmpty(match_pattern) && *u8csHead(match_pattern) == '/') {
        u8csUsed1(match_pattern);
    }

    // If pattern has no slash and is not anchored, match against basename only
    // (gitignore rule: patterns without / match anywhere in the tree)
    if (!pat->has_slash && !pat->anchored) {
        u8cs basename = {};
        PATHu8sBase(basename, (path8s){path[0], path[1]});
        return IGNOGlob(match_pattern, basename);
    }

    // Pattern has slash or is anchored - match against full path
    // For anchored patterns, path must match from root
    // For unanchored patterns with slash, pattern can match at any level

    u8cs match_path;
    u8csDup(match_path, path);

    // Skip leading / in path for comparison
    if (!u8csEmpty(match_path) && *u8csHead(match_path) == '/') {
        u8csUsed1(match_path);
    }

    if (pat->anchored) {
        // Anchored: must match from root
        return IGNOGlob(match_pattern, match_path);
    }

    // Unanchored with slash: try matching at each directory level
    // e.g., "foo/bar" should match "x/foo/bar" or "y/z/foo/bar"
    u8cs try_path;
    u8csDup(try_path, match_path);

    while (!u8csEmpty(try_path)) {
        if (IGNOGlob(match_pattern, try_path)) {
            return YES;
        }
        // Move to next component: skip to the next '/' then past it.
        (void)u8csFind(try_path, '/');   // try_path[0] = '/' or term
        if (!u8csEmpty(try_path) && *u8csHead(try_path) == '/') {
            u8csUsed1(try_path);
        }
    }

    return NO;
}

ok64 IGNOLoad(ignop out, u8cs dir_path) {
    sane(out && u8csOK(dir_path));
    zerop(out);

    // Build path to .gitignore
    a_path(gi_path);
    call(u8sFeed, u8bIdle(gi_path), dir_path);
    call(PATHu8bTerm, gi_path);
    call(PATHu8bPush, gi_path, GITIGNORE_NAME);

    // Try to load file
    ok64 o = FILEMapRO(&out->buf, $path(gi_path));
    if (o != OK) {
        // No .gitignore - not an error
        return NONE;
    }

    // Save root directory
    u8csDup(out->root, dir_path);

    // Parse patterns
    u8cs data;
    u8csDup(data, u8bDataC(out->buf));

    while (!u8csEmpty(data) && out->count < IGNO_MAX_PATTERNS) {
        // Skip leading whitespace (but not newlines)
        while (!u8csEmpty(data) &&
               (*u8csHead(data) == ' ' || *u8csHead(data) == '\t')) {
            u8csUsed1(data);
        }

        // Carve out the line [line_start, line end): up to the first
        // '\n' / '\r'.  `line` is the working slice; `data` advances
        // past the terminating newline run afterwards.
        u8cs line = {};
        line[0] = data[0];
        while (!u8csEmpty(data) &&
               *u8csHead(data) != '\n' && *u8csHead(data) != '\r') {
            u8csUsed1(data);
        }
        line[1] = data[0];

        // Skip the newline run
        while (!u8csEmpty(data) &&
               (*u8csHead(data) == '\n' || *u8csHead(data) == '\r')) {
            u8csUsed1(data);
        }

        // Skip blank lines
        if (u8csEmpty(line)) continue;

        // Trim trailing whitespace
        while (!u8csEmpty(line) &&
               (*u8csLast(line) == ' ' || *u8csLast(line) == '\t')) {
            u8csShed1(line);
        }

        // Skip comment lines
        if (*u8csHead(line) == '#') continue;

        // Parse pattern
        igno_pat *pat = &out->patterns[out->count];
        zerop(pat);

        // Negation marker — consume the '!' from the matchable pattern.
        if (*u8csHead(line) == '!') {
            pat->negated = YES;
            u8csUsed1(line);
        }
        if (u8csEmpty(line)) continue;

        // Anchor: a leading '/' pins the pattern to the root.
        if (*u8csHead(line) == '/') {
            pat->anchored = YES;
        }

        // Trailing '/' → directory-only; trim it from the pattern.
        if (*u8csLast(line) == '/') {
            pat->dir_only = YES;
            u8csShed1(line);
        }

        // Pattern carries a '/' other than a leading-anchor slash → it
        // matches a full path, not just a basename.  Scan the tail past
        // the first byte for any '/'.
        if (u8csLen(line) > 1) {
            a_rest(u8c, tail, line, 1);
            if (u8csFind(tail, '/') == OK) pat->has_slash = YES;
        }

        // Store pattern
        u8csDup(pat->pattern, line);

        out->count++;
    }

    done;
}

void IGNOFree(ignop ig) {
    if (!ig) return;
    if (ig->buf && ig->buf[0]) {
        u8bUnMap(ig->buf);
    }
    zerop(ig);
}

//  Hardcoded metadata: .git and .be are always ignored, whether or
//  not a .gitignore is loaded.  `.be` covers both the primary-wt
//  store directory (`<wt>/.be/`, containing refs, wtlog, packs, …)
//  and the secondary-wt wtlog file (`<wt>/.be`).  Every dog's
//  wt-scan callback routes through IGNOMatch, so this one check
//  keeps internal state out of diff / status / post / patch passes.
//
//  Matches at ANY directory level — `.git/` at the wt root and a
//  submodule's nested `.git/` are equally ignored, so a submodule's
//  internal git-state never surfaces as untracked.
static b8 igno_is_meta(u8cs rel) {
    if (u8csEmpty(rel)) return NO;
    a_cstr(m_git, ".git");
    DOGa_be(m_be);
    //  Secondary-wt sidecar: `<wt>/.be` is a file, its ULOG idx is at
    //  `<wt>/..be.idx`.  Primary-wt's idx (`<wt>/.be/.wtlog.idx`) is
    //  inside `.be/` and is already covered by the segment check.
    a_cstr(m_be_idx, "..be.idx");
    $eachseg(seg, rel) {
        if (u8csEq(seg, m_git))    return YES;
        if (u8csEq(seg, m_be))     return YES;
        if (u8csEq(seg, m_be_idx)) return YES;
    }
    return NO;
}

//  Apply every pattern (last-match-wins) to a single path/is_dir pair.
//  Caller controls iteration over the path's parent prefixes — see
//  IGNOMatch.  Splitting this out avoids re-checking already-tested
//  prefixes when we walk up the tree.
static b8 igno_match_one(ignocp ig, u8cs path, b8 is_dir) {
    b8 ignored = NO;
    for (u64 i = 0; i < ig->count; i++) {
        igno_pat const *pat = &ig->patterns[i];
        if (TryMatch(pat, path, is_dir)) ignored = !pat->negated;
    }
    return ignored;
}

b8 IGNOMatch(ignocp ig, u8cs rel_path, b8 is_dir) {
    if ($empty(rel_path)) return NO;
    if (igno_is_meta(rel_path)) return YES;
    if (!ig || ig->count == 0) return NO;

    //  First, the path itself — covers file patterns and dir patterns
    //  applied to the dir entry.
    if (igno_match_one(ig, rel_path, is_dir)) return YES;

    //  Git semantics: a `dir/` pattern that matches any parent of this
    //  path also ignores everything beneath, including files.  Walk
    //  every prefix `a`, `a/b`, … with is_dir=YES so directory-only
    //  patterns can fire.  Stops short of the full path (already
    //  tested above).  Negations on parents intentionally don't
    //  un-ignore descendants here — that's the same shortcut git takes.
    for (u8cp p = rel_path[0]; p < rel_path[1]; p++) {
        if (*p != '/') continue;
        u8cs prefix = {rel_path[0], p};
        if (igno_match_one(ig, prefix, YES)) return YES;
    }
    return NO;
}
