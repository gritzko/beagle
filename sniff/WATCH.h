#ifndef SNIFF_WATCH_H
#define SNIFF_WATCH_H

//  WATCH — inotify daemon that emits one `mod <dir/>` ULOG row per
//  directory containing dirty files (mtime ∉ stamp-set) since the
//  most recent baseline (get/post/patch).
//
//  Coarse-grained by design: POST does its own wt scan; the row is
//  just an advisory "something changed in this area" signal for
//  external tools.  Per-baseline dedup: a directory whose `mod` row
//  already exists since the baseline is skipped.

#include "SNIFF.h"

//  Fork the watch daemon, write `<reporoot>/.be/sniff.pid`, run until
//  SIGTERM/SIGINT.  Parent returns immediately (prints child pid on
//  stderr); the daemon's own exit is _exit(0).
ok64 SNIFFWatch(u8cs reporoot);

//  Read `<reporoot>/.be/sniff.pid`, SIGTERM the recorded pid, unlink
//  the pidfile.  No-op (returns OK) when no pidfile is present.
ok64 SNIFFWatchStop(u8cs reporoot);

#endif
