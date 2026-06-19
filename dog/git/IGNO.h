#ifndef XX_IGNO_H
#define XX_IGNO_H

//  IGNO: hierarchical .gitignore parser and matcher
//
//  Parses .gitignore files and checks if paths should be ignored.
//  Uses cc/PATH.h glob matching for patterns like *.o, build/, etc.
//
//  STATUS-002: resolution is HIERARCHICAL.  Loading anchors at a
//  worktree dir and walks UP to $HOME (or `/`), stacking every
//  `.gitignore` found.  Matching consults the whole stack with git
//  precedence (a nearer/deeper file overrides a shallower one; `!`
//  negation honored).  The walk crosses submodule boundaries, so a
//  parent repo's `.gitignore` governs nested-sub paths — this is the
//  point: `be status:<sub>/` relays into the sub's wt, whose own
//  `.gitignore` may be absent, but the parent's rules still apply.
//
//  Gitignore pattern rules:
//    - blank lines and lines starting with # are ignored
//    - patterns ending with / match directories only
//    - patterns starting with / are anchored to root
//    - patterns starting with ! negate previous patterns
//    - * matches anything except /
//    - ** matches everything including /
//    - ? matches single character

#include "abc/INT.h"

con ok64 IGNOFAIL = 0x4905d83ca495;
con ok64 IGNONOMTCH = 0x4905d85d859d311;

// Maximum patterns per .gitignore
#define IGNO_MAX_PATTERNS 256

// Maximum .gitignore files in a single anchor→$HOME chain
#define IGNO_MAX_CHAIN 32

// Single ignore pattern
typedef struct {
    u8cs pattern;       // The pattern text
    b8 negated;         // Pattern starts with !
    b8 anchored;        // Pattern starts with / (match from root only)
    b8 dir_only;        // Pattern ends with / (match directories only)
    b8 has_slash;       // Pattern contains / (match full path, not just name)
} igno_pat;

// One .gitignore file's parsed patterns + storage.  `prefix` is the
// path from THIS file's directory down to the anchor dir IGNOLoad was
// called on (empty for the anchor's own .gitignore, `sub` for the
// parent one directory up, `a/b` two up, …).  Matching a path that is
// relative to the anchor dir against this set prepends `prefix/`.
typedef struct {
    igno_pat patterns[IGNO_MAX_PATTERNS];
    u64 count;
    u8bp buf;           // mmap of this .gitignore (NULL = none here)
    u8cs prefix;        // anchor-dir path relative to this file's dir
} igno_set;

// Ignore state - a stack of .gitignore files from the anchor dir up to
// $HOME (or /).  set[0] is the DEEPEST (anchor dir, prefix empty);
// later entries are shallower (parent dirs, longer prefixes).  Match
// precedence walks shallow→deep so deeper rules override.
typedef struct {
    igno_set set[IGNO_MAX_CHAIN];
    u64 count;          // number of loaded sets (>=1 even if all empty)
    Bu8 prefixbuf;      // heap copy of the anchor path; per-set prefix
                        // slices point into it (freed by IGNOFree)
} igno;
typedef igno *ignop;
typedef igno const *ignocp;

// Load the .gitignore chain anchored at `dir_path`, walking up to
// $HOME (or `/`).  Always succeeds (returns OK) even with no
// .gitignore in the chain — IGNOMatch still rejects .git/.be.  Returns
// a non-OK code only on an internal buffer/path failure.
ok64 IGNOLoad(ignop out, u8cs dir_path);

// Free resources
void IGNOFree(ignop ig);

// Check if path should be ignored.  `rel_path` is relative to the dir
// IGNOLoad anchored on (the worktree root).  Consults the whole chain
// up to $HOME with git precedence.  `is_dir` should be YES if path is
// a directory.
b8 IGNOMatch(ignocp ig, u8cs rel_path, b8 is_dir);

#endif
