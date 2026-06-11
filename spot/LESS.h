#ifndef SPOT_LESS_H
#define SPOT_LESS_H

#include "abc/INT.h"
#include "dog/HUNK.h"

// Producer-side staging: build hunks in `less_arena`, emit them to
// `spot_out_fd` via `HUNKu8sFeedOut` (TLV / Color / Plain dispatched
// off the module-global `HUNKMode`).  `be` is the only thing that
// forks bro for pagination.

// LESShunk kept as alias for hunk during transition.
typedef hunk LESShunk;

#define LESS_ARENA_SIZE (1UL << 24)   // 16MB
#define LESS_MAX_HUNKS  4              // tiny scratch ring
#define LESS_MAX_MAPS   1024

extern Bu8      less_arena;
extern LESShunk less_hunks[LESS_MAX_HUNKS];
extern u8bp     less_maps[LESS_MAX_MAPS];
extern Bu32     less_toks[LESS_MAX_MAPS];
extern u32      less_nhunks;
extern u32      less_nmaps;

// File descriptor for outgoing hunks.  STDOUT_FILENO under normal use;
// -1 = uninitialized.
extern int spot_out_fd;

ok64 LESSArenaInit(void);
void LESSArenaCleanup(void);
void LESSDefer(u8bp mapped, u32bp toks);

// Serialize less_hunks[less_nhunks] via spot_emit, write to spot_out_fd,
// rewind the staging arena.  Single-buffered: each emit reuses the slot.
// Propagates a serialize/write failure (e.g. EPIPE/EAGAIN on a broken
// output stream) so the caller can stop emission; the arena is rewound
// on both the success and the error path.
ok64 LESSHunkEmit(void);

// End-of-producer flush: cleans up deferred maps; the fd lifecycle
// (close + waitpid) is handled by the CLI.
ok64 LESSRun(LESShunk const *hunks, u32 nhunks);

#endif
