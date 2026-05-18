#ifndef KEEPER_REFADV_H
#define KEEPER_REFADV_H

//  REFADV: build + emit the git-protocol refs advertisement.
//
//  Walks every branch dir's REFS, decodes one (sha, refname) pair per
//  recognised local-tip key (`?heads/<name>` or `?tags/<name>`) with
//  a terminal `?<40-hex>` value, and remembers which branch dir each
//  tip came from so wire negotiators can resolve a `want sha` back to
//  a dir without rescanning REFS.
//
//  Phase 1c keeper has only the trunk shard open — REFS lives at
//  `<store>/REFS` (`<root>/.be/REFS`).  Sibling shards land in
//  Phase 2; REFADVOpen then fans out to each.
//
//  Emit format (git protocol v0/v1):
//
//      <40-hex-sha> SP <refname> NUL <capability-list>\n  -- first line
//      <40-hex-sha> SP <refname>\n                        -- subsequent
//      0000                                               -- flush
//
//  Each line is wrapped in a pkt-line (4 hex digits length prefix) per
//  keeper/PKT.h.  REFADVEmit writes the framed bytes to `out_fd`.

#include "abc/INT.h"
#include "abc/OK.h"
#include "abc/S.h"
#include "dog/git/SHA1.h"
#include "keeper/KEEP.h"

con ok64 REFADVFAIL = 0x6ce3ca35f3ca495;

//  One advertised ref: the resolved tip sha + the refname (e.g.
//  "refs/heads/main") + the dir slice (which dir's REFS this came
//  from, used for tip → dir lookup).  Refname and dir bytes are
//  owned by the parent `refadv`'s arena.
typedef struct {
    sha1  tip;
    u8cs  refname;     // "refs/heads/<name>" or "refs/tags/<name>"
    u8cs  dir;         // canonical branch dir slice ("" = trunk)
} refadv_entry;

typedef refadv_entry *refadv_entryp;
typedef refadv_entry const *refadv_entrycp;

//  In-memory advertisement.
typedef struct {
    refadv_entry *ents;       // calloc'd, `count` entries
    u32           count;
    u8b           arena;      // backing store for refname + dir slices
} refadv;

typedef refadv *refadvp;
typedef refadv const *refadvcp;

//  Walk the keeper's REFS, populate `out`.  Caller frees with
//  REFADVClose.  `out` is reset (zeroed) on entry.
ok64 REFADVOpen(refadv *out);

//  Release arena + entries array.  Safe on a zeroed `adv`.
void REFADVClose(refadv *adv);

//  Look up: which dir(s) hold this sha as a tip?
//  Writes up to `cap` matches into `out_dirs`, returns the match count.
//  "Topmost dir" picking (e.g. shortest dir slice) is the caller's job.
u32  REFADVTipDirs(refadv const *adv, sha1 const *tip,
                   u8cs *out_dirs, u32 cap);

//  Emit the pkt-line advertisement to `out_fd`:
//    first line includes capabilities after a NUL byte;
//    subsequent lines are bare "<sha> <refname>";
//    final flush (0000).
//
//  Capabilities advertised:
//    "multi_ack_detailed side-band-64k ofs-delta agent=dogs-keeper"
ok64 REFADVEmit(int out_fd, refadv const *adv);

#endif
