//  IGNO: hierarchical .gitignore parser and matcher using cc/PATH
//
//  STATUS-002: IGNOLoad anchors at a worktree dir and walks UP to
//  $HOME (or `/`), stacking every `.gitignore` found.  IGNOMatch
//  consults the whole stack with git precedence (a nearer/deeper file
//  overrides a shallower one; `!` negation honored), crossing
//  submodule boundaries.  This is why `be status:<sub>/` (relayed into
//  a sub whose own `.gitignore` is absent) still honors the parent
//  repo's ignores — fixing the `abc/build-debug/...` flood.
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

//  Parse the mmap'd `.gitignore` bytes in `set->buf` into `set->patterns`.
//  Extracted from the old inline IGNOLoad body; one set per file.
static void igno_parse_set(igno_set *set) {
    set->count = 0;
    if (!set->buf || !set->buf[0]) return;

    u8cs data;
    u8csDup(data, u8bDataC(set->buf));

    while (!u8csEmpty(data) && set->count < IGNO_MAX_PATTERNS) {
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
        igno_pat *pat = &set->patterns[set->count];
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

        set->count++;
    }
}

//  Load one `.gitignore` from `dir` into `set->buf` (or leave NULL when
//  absent / unreadable), then parse it.  `dir` is the directory path
//  slice.  Absent file is not an error.
static ok64 igno_load_set(igno_set *set, u8cs dir) {
    sane(set && u8csOK(dir));
    a_path(gi_path);
    call(u8sFeed, u8bIdle(gi_path), dir);
    call(PATHu8bTerm, gi_path);
    call(PATHu8bPush, gi_path, GITIGNORE_NAME);

    ok64 o = FILEMapRO(&set->buf, $path(gi_path));
    if (o != OK) { set->buf = NULL; done; }   // no .gitignore here
    igno_parse_set(set);
    done;
}

ok64 IGNOLoad(ignop out, u8cs dir_path) {
    sane(out && u8csOK(dir_path));
    zerop(out);

    //  Normalize the anchor dir into a heap copy: per-set prefix slices
    //  point into this, so it must outlive the load.  Capacity is the
    //  path length + room for a NUL.
    u64 dlen = u8csLen(dir_path);
    call(u8bAllocate, out->prefixbuf, dlen + 2);
    ok64 no = PATHu8bNorm(out->prefixbuf, dir_path);
    if (no != OK) { u8bFree(out->prefixbuf); return no; }
    a_dup(u8c, anchor, u8bDataC(out->prefixbuf));

    //  $HOME terminates the upward walk (or `/` when the anchor is not
    //  under $HOME).  Compare as a normalized prefix.
    a_pad(u8, homebuf, 256);
    u8cs home_env = {};
    FILEGetEnv("HOME", home_env);
    u8cs home = {};
    if (!u8csEmpty(home_env)) {
        if (PATHu8bNorm(homebuf, home_env) == OK)
            u8csMv(home, u8bDataC(homebuf));
    }

    //  Walk the anchor dir up to (and including) $HOME / `/`.  cur is a
    //  consumable path buffer seeded with the anchor; each step loads
    //  the set then pops one segment.  set[0] is the deepest.
    a_path(cur);
    call(PATHu8bFeed, cur, anchor);

    for (out->count = 0; out->count < IGNO_MAX_CHAIN; ) {
        igno_set *set = &out->set[out->count];
        a_dup(u8c, curdir, u8bDataC(cur));

        //  Prefix = anchor path relative to curdir (empty for the
        //  deepest set).  Both are normalized; curdir is a prefix of
        //  anchor by construction, so slice the anchor tail.
        u8cs pfx = {};
        if (u8csLen(anchor) > u8csLen(curdir) &&
            u8csHasPrefix(anchor, curdir)) {
            u8cs tail = {$atp(anchor, u8csLen(curdir)), anchor[1]};
            while (!u8csEmpty(tail) && *u8csHead(tail) == '/') u8csUsed1(tail);
            u8csMv(pfx, tail);
        }
        u8csMv(set->prefix, pfx);

        ok64 lo = igno_load_set(set, curdir);
        if (lo != OK) { IGNOFree(out); return lo; }
        out->count++;

        //  Stop once we just processed $HOME (or the filesystem root).
        if (!u8csEmpty(home) && u8csEq(curdir, home)) break;
        if (u8csLen(curdir) <= 1) break;          // "/" — root reached

        //  Pop one segment; stop if nothing was removed.
        u64 before = u8bDataLen(cur);
        (void)PATHu8bPop(cur);
        if (u8bDataLen(cur) >= before) break;     // no progress
        if (u8bDataLen(cur) == 0) {
            //  Popped to empty: synthesize root "/" for one last set.
            call(PATHu8bFeed, cur, ((u8cs)u8slit("/")));
        }
    }

    done;
}

void IGNOFree(ignop ig) {
    if (!ig) return;
    for (u64 i = 0; i < ig->count; i++) {
        igno_set *set = &ig->set[i];
        if (set->buf && set->buf[0]) u8bUnMap(set->buf);
    }
    if (!BNULL(ig->prefixbuf)) u8bFree(ig->prefixbuf);
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

//  Apply every pattern (last-match-wins) of one set to a single
//  path/is_dir pair.  Returns -1 (no pattern matched), 0 (last match
//  was a negation → un-ignored), or 1 (last match ignores).  Caller
//  walks the chain shallow→deep and lets a definite (>=0) result from
//  a deeper set override.
static int igno_set_decide(igno_set const *set, u8cs path, b8 is_dir) {
    int decision = -1;
    for (u64 i = 0; i < set->count; i++) {
        igno_pat const *pat = &set->patterns[i];
        if (TryMatch(pat, path, is_dir)) decision = pat->negated ? 0 : 1;
    }
    return decision;
}

//  Decide one set for `path` including git's dir-prefix rule: a `dir/`
//  pattern matching any parent of `path` ignores everything beneath.
//  The path itself is tested first; then each prefix `a`, `a/b`, … with
//  is_dir=YES.  A parent-dir ignore is definitive (1); we don't let a
//  parent negation un-ignore a descendant (the shortcut git takes).
static int igno_set_decide_deep(igno_set const *set, u8cs path, b8 is_dir) {
    int d = igno_set_decide(set, path, is_dir);
    for (u8cp p = path[0]; p < path[1]; p++) {
        if (*p != '/') continue;
        u8cs prefix = {path[0], p};
        if (igno_set_decide(set, prefix, YES) == 1) return 1;
    }
    return d;
}

b8 IGNOMatch(ignocp ig, u8cs rel_path, b8 is_dir) {
    if ($empty(rel_path)) return NO;
    if (igno_is_meta(rel_path)) return YES;
    if (!ig || ig->count == 0) return NO;

    //  Walk the chain SHALLOW→DEEP (set[count-1] is $HOME, set[0] is
    //  the anchor) so a deeper file's decision overrides a shallower
    //  one — git precedence.  Each set's patterns are relative to its
    //  own dir, so prepend the set's prefix (anchor path relative to
    //  that dir) to the anchor-relative `rel_path`.
    a_pad(u8, full, 4096);
    b8 ignored = NO;
    for (u64 k = ig->count; k-- > 0; ) {
        igno_set const *set = &ig->set[k];
        if (set->count == 0) continue;

        u8bReset(full);
        u8cs effective = {};
        if (u8csEmpty(set->prefix)) {
            u8csMv(effective, rel_path);
        } else {
            if (PATHu8bFeed(full, set->prefix) != OK) continue;
            if (PATHu8bAdd(full, rel_path) != OK) continue;
            u8csMv(effective, u8bDataC(full));
        }

        int d = igno_set_decide_deep(set, effective, is_dir);
        if (d >= 0) ignored = (d == 1);
    }
    return ignored;
}
