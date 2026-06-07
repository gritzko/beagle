//  dog/test/TESTBE.h — the ONE shared hermetic-store setup for C tests.
//
//  Mirrors the shell repo-setup lib (test/lib/repo-setup.sh): every C
//  test that stands up a beagle store/worktree routes its scratch-dir
//  creation through TESTBEmkdtemp so isolation is uniform and a stray
//  `$HOME/.be` can never leak in.
//
//  WHY /tmp (not $HOME): the C tests open their store with HOMEOpenAt
//  on an EXPLICIT root, or chdir into the scratch dir and let HOMEOpen
//  walk UP.  The walk-up must hit no `.be` in any ancestor — and a
//  developer/CI box's `$HOME/.be` is a real multi-project store, so an
//  $HOME-rooted scratch would let discovery escape into it (the exact
//  HOMEtest/SNIFFnorepo "environmental" failure).  `/tmp`'s ancestor
//  chain up to `/` carries no `.be`, so the walk terminates cleanly.
//  C tests here only bootstrap a store in-place (no MAP_SHARED of a
//  pre-existing pack across the tmpfs boundary at a problematic size),
//  so tmpfs `/tmp` is safe for them — unlike the RW shell repo cases,
//  which root under $HOME ext4.
//
//  Usage:
//      char dir[256];
//      want(TESTBEmkdtemp(dir, sizeof dir) == OK);    // isolated root
//      ... HOMEOpenAt(&h, root, rw) / chdir(dir)+HOMEOpen ...
//      TESTBErmrf(dir);                               // teardown
//
#ifndef DOG_TEST_TESTBE_H
#define DOG_TEST_TESTBE_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "abc/01.h"

//  Create a fresh hermetic scratch dir under /tmp (clean `.be`-free
//  ancestry) and copy its path into `buf`.  Returns OK on success.
//
//  The harness OWNS the documented invariant that `/tmp` carries no
//  `.be` in the ancestry of its scratch dirs (see file header): the C
//  tests chdir into the scratch and let `HOMEOpen` walk UP, which must
//  hit no ancestor `.be` (else discovery escapes into the stray store —
//  the bootstrap lands in the wrong place and escaping rw ops corrupt
//  it).  A stray `/tmp/.be` (a leaked bare bootstrap) silently breaks
//  HOMEtest/SNIFFnorepo and pollutes whatever sits there.  Enforce the
//  invariant: if `/tmp/.be` exists, remove it loudly before handing back
//  a scratch dir.  `/tmp` is volatile and no legitimate store lives at
//  `/tmp/.be`, so this is harness hygiene, not data loss.
static inline void TESTBEShieldTmp(void) {
    struct stat st;
    if (stat("/tmp/.be", &st) != 0) return;   //  clean — nothing to do
    fprintf(stderr,
        "TESTBE: removing stray /tmp/.be (violates the .be-free /tmp "
        "test invariant — leaked bare bootstrap)\n");
    if (system("rm -rf /tmp/.be") != 0) { /* best-effort */ }
}

static inline ok64 TESTBEmkdtemp(char *buf, size_t cap) {
    if (buf == NULL || cap < sizeof("/tmp/be-test-XXXXXX")) return FAIL;
    TESTBEShieldTmp();
    snprintf(buf, cap, "/tmp/be-test-XXXXXX");
    return mkdtemp(buf) != NULL ? OK : FAIL;
}

//  Recursively remove a TESTBEmkdtemp scratch dir.
static inline void TESTBErmrf(char const *dir) {
    char cmd[300];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", dir);
    if (system(cmd) != 0) { /* best-effort teardown */ }
}

#endif
