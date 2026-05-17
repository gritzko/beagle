#ifndef XX_IGNO_H
#define XX_IGNO_H

//  IGNO: .gitignore parser and matcher
//
//  Parses .gitignore files and checks if paths should be ignored.
//  Uses cc/PATH.h glob matching for patterns like *.o, build/, etc.
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

// Single ignore pattern
typedef struct {
    u8cs pattern;       // The pattern text
    b8 negated;         // Pattern starts with !
    b8 anchored;        // Pattern starts with / (match from root only)
    b8 dir_only;        // Pattern ends with / (match directories only)
    b8 has_slash;       // Pattern contains / (match full path, not just name)
} igno_pat;

// Ignore state - loaded patterns from .gitignore
typedef struct {
    igno_pat patterns[IGNO_MAX_PATTERNS];
    u64 count;
    u8bp buf;            // Storage for pattern text
    u8cs root;          // Root directory (where .gitignore lives)
} igno;
typedef igno *ignop;
typedef igno const *ignocp;

// Load .gitignore from directory
// Returns OK if loaded, NONE if no .gitignore exists
ok64 IGNOLoad(ignop out, u8cs dir_path);

// Free resources
void IGNOFree(ignop ig);

// Check if path should be ignored
// path must be relative to .gitignore root
// is_dir should be YES if path is a directory
b8 IGNOMatch(ignocp ig, u8cs rel_path, b8 is_dir);

#endif
