#ifndef LIBRDX_CAPO_H
#define LIBRDX_CAPO_H

#include <stdio.h>
#include <string.h>
#include "abc/INT.h"
#include "abc/KV.h"
#include "abc/PATH.h"
#include "abc/RON.h"
#include "abc/RAP.h"
#include "dog/DOG.h"
#include "dog/SHA1.h"
#include "dog/WHIFF.h"

con ok64 CAPONOROOM = 0x30a6585d86d8616;
con ok64 CAPONODIFF = 0x30a6585d83523cf;  // no usable saved commit → full reindex
//  Singleton-open return codes, matching keeper/sniff/graf convention.
con ok64 SPOTOPEN   = 0x71961d619397;
con ok64 SPOTOPENRO = 0x71961d6193976d8;
//  SPOTOpenBranch: branch outside the Phase-3-supported set (trunk only).
con ok64 SPOTNOBR   = 0x71961d5d82db;

extern b8 CAPO_COLOR;  // stdout is a terminal with color
extern b8 CAPO_TERM;   // stderr is a terminal

// Verbose call: prints step context on failure
#define vcall(step, f, ...)                                              \
    {                                                                    \
        __ = (f(__VA_ARGS__));                                           \
        if (__ != OK) {                                                  \
            fprintf(stderr, "spot: %s: %s (%s:%d)\n",                   \
                    step, ok64str(__), __func__, __LINE__);              \
            return __;                                                   \
        }                                                                \
    }

#define CAPO_DIR ".dogs"
#define CAPO_IDX_EXT ".spot.idx"
#define CAPO_LOCK_S  ".lock.spot"
#define CAPO_SEQNO_WIDTH 10
#define CAPO_MAX_LEVELS 24
#define SPOT_LEAF_BRANCH_MAX 1024
//  Missing prefix dir along the trunk → leaf branch path.
con ok64 SPOTNOPATH = 0x71961d5d864a751;
//  In-RAM sort-and-dedup scratch.  `CAPO_FLUSH_AT` is the trigger
//  size: once data in `s->entries` reaches it, we sort + dedup in
//  place, and — if the dedup leaves ≥ 50 % unique — flush to a new
//  `.idx` run.  If < 50 % unique (highly redundant input) we keep
//  the compacted scratch and let it refill.  Keeping the trigger
//  small (1 M entries / 8 MB) bounds each sort's working set to
//  stay cache-friendly; on dedup-heavy workloads (src/git ingest
//  hits ~20 % unique) this means many small sorts instead of a few
//  enormous ones.  `CAPO_SCRATCH_LEN` is the hard cap on scratch
//  size (anonymous mmap) — generously oversized vs the trigger.
#define CAPO_SCRATCH_LEN (1UL << 27)  // 128M u64 entries = 1GB
#define CAPO_FLUSH_AT    (1UL << 20)  // 1M entries (~8MB)
//  Path-hash propagation map (rw): 60-bit obj_hl → 64-bit path_hash.
//  ~16 B per entry × every tree+blob in the ingest closure.  src/git
//  scale (~1 M tree+blob objects across an 80 K-commit fetch) lands
//  near 16 MB; oversize the map for safety.
#define CAPO_OBJ_PATH_CAP   (1u << 22)        // 4 M slots → ~64 MB
//  Blob-to-extension map: only blobs with a known tokenizer ext
//  appear, so this is much smaller than obj_to_path_hash.
#define CAPO_BLOB_EXT_CAP   (1u << 20)        // 1 M slots
//  Per-session arena for ext strings ("c", "h", "py", …).  Offset 0
//  is reserved as a sentinel; ~50 distinct exts fit easily in 4 KB.
#define CAPO_EXT_ARENA_LEN  (1u << 12)        // 4 KB

#define CAPOTriChar(c) (RON64_REV[(u8)(c)] != 0xff)

// Pack 3 RON64 chars into upper 32 bits of u64 (18-bit trigram, zero-padded)
fun u64 CAPOTriPack(u8cs tri) {
    u64 t = ((u64)RON64_REV[tri[0][0]] << 12) |
            ((u64)RON64_REV[tri[0][1]] << 6) |
            ((u64)RON64_REV[tri[0][2]]);
    return t << 32;
}

// Extract triplet from packed u64 entry
fun u64 CAPOTriOf(u64 entry) { return entry & 0xFFFFFFFF00000000ULL; }

// Path hash: lower 32 bits of the chained DOGPathHash (root + each
// non-empty segment folded through DOGChildPathHash).  Must match
// the index-time stamping in SPOTUpdate(TREE), which builds the
// chain one tree-entry name at a time as the producer streams trees
// parent-first — see the helpers below (CAPOPathHashRoot,
// CAPOPathHashStep, CAPOPathHashKey).
fun u32 CAPOPathHash(u8csc path) {
    return (u32)DOGPathHash(path);
}

// Pack trigram + path hash into u64
fun u64 CAPOEntry(u8cs tri, u8cs path) {
    return CAPOTriPack(tri) | (u64)CAPOPathHash(path);
}

// --- Path hashing for streaming ingest --------------------------------
//
// Index entries are keyed by `(trigram, path_hash)`.  During pack
// ingest we want to compute path_hash *without* a second tree walk,
// so we propagate it through the producer's parent-first object
// stream:
//
//   SPOTUpdate(COMMIT, sha, body)
//       parse the `tree <hex>` header → root_tree_sha.
//       seed `obj_to_path_hash[hashlet(root_tree_sha)] = ROOT`.
//
//   SPOTUpdate(TREE, sha, body)
//       parent = obj_to_path_hash[hashlet(sha)]   (must be present)
//       for each (mode, name, child_sha) in the tree:
//           obj_to_path_hash[hashlet(child_sha)]
//               = CAPOPathHashStep(name, parent)
//
//   SPOTUpdate(BLOB, sha, bytes)
//       parent = obj_to_path_hash[hashlet(sha)]   (must be present;
//                if absent → warn: producer packed parent-last or
//                blob is orphan)
//       phash = CAPOPathHashKey(parent)
//       tokenise bytes inline using `phash`.
//
// The chain primitive is shared: dog/DOG.h's DOGChildPathHash, with
// root = dog/DOG.h's `ROOT` (= ron60("ROOT")).  The wrappers below
// just spell out spot's use of it so SPOTUpdate readers see the
// chain at a glance.

// Root path hash (matches dog/DOG.h's `ROOT`).
fun u64 CAPOPathHashRoot(void) { return ROOT; }

// One step down the tree: child of `parent` named `name` (leaf, no
// slashes).  Thin wrapper over `DOGChildPathHash` so spot indexing
// reads as a self-contained chain.
fun u64 CAPOPathHashStep(u8csc name, u64 parent) {
    return DOGChildPathHash(name, parent);
}

// Truncate the 64-bit chained value to spot's 32-bit posting key.
// Used at BLOB ingest time when feeding the trigram tokenizer.
fun u32 CAPOPathHashKey(u64 chained) { return (u32)chained; }

// 60-bit object-id key for `obj_to_path_hash`.  Same shape as keeper's
// WHIFFHashlet60; named here to make spot's SPOTUpdate dispatch
// readable on its own.
fun u64 CAPOObjHashlet(sha1 const *sha) { return WHIFFHashlet60(sha); }

// Index a streaming blob whose path hash was already propagated via
// SPOTUpdate(TREE).  Same body as the legacy CAPOIndexFile — only the
// signature differs (precomputed phash; no path slice needed).
ok64 CAPOIndexBlob(u64bp entries, u8csc source, u8csc ext, u32 path_hash);

// Index a single on-disk source file.  Computes path_hash from the
// path slice and delegates to CAPOIndexBlob.  Used by the search-time
// (re)tokenize path; ingest now goes through CAPOIndexBlob directly.
ok64 CAPOIndexFile(u64bp entries, u8csc source, u8csc ext, u8csc path);

// Load index stack as a typed view over SPOT.puppies (no fs scan,
// no per-call mmap; the puppy stack is owned by the singleton).
// `dir` is ignored — kept for API stability.  Each `<seqno>.spot.idx`
// along trunk → leaf appears as one run in `stack[0..nfiles)`.
ok64 CAPOStackOpen(u64css stack, u8bp *maps, u32p nfiles, u8csc dir);

// No-op: SPOT.puppies owns the mmaps now.
ok64 CAPOStackClose(u8bp *maps, u32 nfiles);

typedef struct spot_ spot;

// Compact the LSM stack at the leaf branch dir, unlink merged
// sources via DOGPupThinTail and write the merged run via
// DOGPupCreate.  Mirrors KEEPCompact / dag_compact.
ok64 CAPOCompact(spot *s);

// Flush in-memory postings (s->entries) as a new puppy and run
// CAPOCompact to keep the 1/8 invariant.  Called by SPOTUpdate(BLOB)
// when scratch exceeds CAPO_FLUSH_AT, and by SPOTClose at end of run.
ok64 CAPOFlushRun(spot *s);

// Next available sequence number (max existing + 1)
ok64 CAPONextSeqno(u64p seqno, u8csc dir);

#include "abc/URI.h"

// Structural code search: needle is a code fragment, ext is file extension.
// When replace is non-empty, matched regions are replaced and files rewritten.
// When ref is non-NULL, search historic blobs at that ref via keeper
// (replace is rejected in this mode — no on-disk path to rewrite).
ok64 CAPOSpot(u8csc needle, u8csc replace, u8csc ext, u8csc reporoot,
              u8css files, uri const *ref);

// Substring grep across all AST leaves (including comments).
// ext: optional language filter (empty = all parseable files).
// ctx_lines: max context lines above/below the match (like diff -C).
// ref: optional — when set, grep historic blobs at that ref via keeper.
ok64 CAPOGrep(u8csc substring, u8csc ext, u8csc reporoot, u32 ctx_lines,
              u8css files, uri const *ref);

// Regex grep using Thompson NFA (abc/NFA.h).
// pattern: regex string (supports . * + ? | () [] \d \w \s {n,m}).
// Extracts literal substrings for trigram index filtering, then NFA-matches
// candidate files line by line. Same output format as CAPOGrep.
// ref: optional — when set, regex-grep historic blobs at that ref.
ok64 CAPOPcreGrep(u8csc pattern, u8csc ext, u8csc reporoot, u32 ctx_lines,
                   u8css files, uri const *ref);

// Compact all .spot.idx files into a single run at the leaf branch
// dir.  Uses SPOT.puppies and writes to SPOT.leaf_branch.
ok64 CAPOCompactAll(spot *s);

// Resolve spot index dir from reporoot (<reporoot>/.dogs/spot)
ok64 CAPOResolveDir(path8b out, u8csc reporoot);

// Check if extension is known to tok/ tokenizers
b8 CAPOKnownExt(u8csc ext);

// --- Index entry kinds ---
//
// Every spot index entry is one u64 with the same shape:
//   [2 bits type | 30 bits key | 32 bits path_hash].
// The low 32 bits always carry CAPOPathHash(path) so all four kinds
// cluster together in the LSM by path.

typedef u64 idx64;    // index entry

#define IDX64_TRI  0  // text trigram
#define IDX64_MEN  1  // S token — symbol mention
#define IDX64_DEF  2  // N token — symbol definition
#define IDX64_PAIR 3  // (blob_hashlet30, path_hash) — already-tokenised marker

fun u64 idx64Type(idx64 e)     { return e >> 62; }
fun u32 idx64Key(idx64 e)      { return (u32)(e >> 32) & 0x3FFFFFFF; }
fun u32 idx64PathHash(idx64 e) { return (u32)e; }

// Pack 30-bit symbol name hash into key position [61:32]
fun u64 CAPOSymKey(u8cs name) {
    return ((u64)((u32)(RAPHash(name)) & 0x3FFFFFFF)) << 32;
}

fun idx64 CAPOSymEntry(u64 type, u8cs name, u8cs path) {
    return (type << 62) | CAPOSymKey(name) | (u64)CAPOPathHash(path);
}

// Low 30 bits of the SHA-1's first 8 bytes (matches keeper's hashlet60
// keyed lookups truncated by 30 bits).  Ambiguity at the prefix is
// rare and tolerated — the Close-pass treats a hit as "candidate seen".
fun u32 CAPOBlobHashlet30(sha1 const *sha) {
    u64 hl = 0;
    memcpy(&hl, sha->data, 8);
    return (u32)(hl & 0x3FFFFFFF);
}

// Tag-3 pair posting: same shape as TRI/MEN/DEF.
fun idx64 CAPOPairEntry(u32 blob_hashlet30, u32 path_hash) {
    return ((u64)IDX64_PAIR << 62) |
           ((u64)(blob_hashlet30 & 0x3FFFFFFF) << 32) |
           (u64)path_hash;
}

// --- DOG control struct (DOG.md rule 8) ---

#include "abc/FILE.h"
#include "dog/CLI.h"
#include "dog/HOME.h"
#include "dog/HUNK.h"
#include "spot/LESS.h"

struct spot_ {
    home    *h;                     // borrowed
    int      lock_fd;               // flock on leaf dir's .lock; -1 = ro

    Bu8      arena;
    hunk     hunks[LESS_MAX_HUNKS];
    u8bp     maps[LESS_MAX_MAPS];
    Bu32     toks[LESS_MAX_MAPS];
    u32      nhunks;
    u32      nmaps;

    int          out_fd;
    spot_emit_fn emit;

    //  Puppy stack: (seqno → fd) for every `<seqno>.spot.idx` along
    //  trunk → leaf.  Mmaps live in FILE_WANT_BUFS[fd].  Mirrors
    //  keeper's `k->puppies` and graf's `g->puppies`.  Reads fan out
    //  across the whole path; writes (DOGPupCreate) and compactions
    //  (DOGPupThinTail+DOGPupCreate) only land in the leaf dir.
    Bkv32    puppies;
    Bu8      leaf_branch;           // canonical leaf-branch path
                                    // (trailing '/'; empty for trunk).

    //  Ingestion scratch (rw only): postings accumulated by SPOTUpdate,
    //  flushed to a new puppy when len >= CAPO_FLUSH_AT or on close.
    Bu64     entries;

    //  Path-hash propagation map (rw only).  Populated greedily as the
    //  producer's parent-first object stream flows through SPOTUpdate
    //  (see header comment on CAPOPathHashStep).  Key = CAPOObjHashlet
    //  of a tree or blob; value = the 64-bit chained path hash.  Tree
    //  values are looked up at SPOTUpdate(TREE) to seed children; blob
    //  values are looked up at SPOTUpdate(BLOB) to tag postings.
    Bkv64    obj_to_path_hash;      // 60-bit obj_hl → 64-bit path_hash

    //  Per-blob extension lookup, only populated for blobs whose name
    //  in their containing tree resolves to a known tokenizer
    //  extension (CAPOKnownExt).  `val` is an offset into `ext_arena`
    //  pointing to a NUL-terminated extension string (".c", ".h",
    //  ".py", …).  Absent ⇒ blob is referenced from a tree but
    //  doesn't tokenize → SPOTUpdate(BLOB) silently skips it.
    Bkv64    blob_to_ext;           // 60-bit blob_hl → ext_off
    Bu8      ext_arena;             // NUL-separated ext strings,
                                    // offset 0 reserved as sentinel.

    b8 color;
    b8 term;
    b8 rw;
};

typedef spot *spotp;
typedef spot const *spotcp;

//  Singleton.  Zero-initialised; populated by SPOTOpen.
extern spot SPOT;

// --- Public API (singleton, same contract as KEEP/SNIFF/GRAF) ---

//  Open spot state rooted at `home` (repo root).  Returns:
//    OK         I opened; pair with SPOTClose.
//    SPOTOPEN   already open compatible; use &SPOT, don't close.
//    SPOTOPENRO already ro and caller asked for rw.
//    (other)    real error — propagate.
ok64 SPOTOpen(home *h, b8 rw);

//  Branch-aware Open (Phase 3 surface).  Normalizes `branch` via
//  DPATHBranchNormFeed and registers it on the home singleton via
//  HOMEOpenBranch before delegating to SPOTOpen.  Phase 3 accepts
//  only the trunk (canonical form = empty); other branches return
//  SPOTNOBR.  Mirrors `KEEPOpenBranch` / `GRAFOpenBranch`.
ok64 SPOTOpenBranch(home *h, u8cs branch, b8 rw);

//  Run one CLI invocation.
ok64 SPOTExec(cli *c);

//  Feed a single git object into spot during pack ingest.  Path
//  hashes propagate parent-first via the producer's pack order
//  (git/sniff both pack commits → trees parent-first → blobs):
//
//    COMMIT: parse `tree <hex>` header, seed obj_to_path_hash
//            for the root tree with CAPOPathHashRoot().
//    TREE:   for each (mode, name, child_sha) entry, stamp
//            obj_to_path_hash[hashlet(child_sha)] using the parent
//            tree's path hash + CAPOPathHashStep(name, parent).
//            For blob entries with a known tokenizer extension,
//            also record the ext in blob_to_ext / ext_arena.
//    BLOB:   look up the precomputed path_hash and ext, then call
//            CAPOIndexBlob inline — no second tree walk.  If the
//            path_hash is absent, fprintf(stderr, ...) a warning
//            (producer packed parent-last, or orphan blob).
//
//  `sha` is the caller's pre-computed git-object SHA-1.
ok64 SPOTUpdate(u8 obj_type, sha1 const *sha, u8cs blob);

void SPOTClose(void);

//  Verb + value-flag tables for CLIParse.
extern char const *const SPOT_CLI_VERBS[];
extern char const SPOT_CLI_VAL_FLAGS[];

#endif
