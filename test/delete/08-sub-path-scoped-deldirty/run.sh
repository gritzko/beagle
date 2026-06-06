#!/bin/sh
#  delete/08-sub-path-scoped-deldirty — path-scoped `be delete` of a
#  submodule mount, or a byte-clean file inside one, must NOT
#  false-positive DELDIRTY (exit 157).  SUBS-005.
#
#  Setup uses the 2-level fixture (submodules.sh): parent C3 carries a
#  `160000 vendor/sub` gitlink + `.gitmodules`; the sub mounts at
#  vendor/sub on clone.  A freshly-mounted sub is byte-clean — every
#  file's mtime is sniff-stamped by GET.
#
#  Two probes, both currently aborting DELDIRTY/157:
#    1. `be delete vendor/sub/core.c` — a clean file INSIDE the mount.
#       The parent tree has no baseline entry for it (only the gitlink),
#       so DELETE's dirty check (mtime ∉ stamp-set ⇒ refuse) fires.
#       Expected: clean (skip / recurse), never DELDIRTY.
#    2. `be delete vendor/sub` — the gitlink dir itself.  Its on-disk
#       form is a directory with a `.be` mount, never hashing to the
#       `160000` pin → spurious DELDIRTY.
#       Expected: sub-aware removal or skip, never DELDIRTY.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)

# --- Clone parent at master (C3) — sub mounts at vendor/sub. ----------
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "clone be get exited $rc; stderr:
$(cat 01.get.got.err)"
[ -f vendor/sub/.be ]    || fail "sub did not mount (no vendor/sub/.be)"
[ -f vendor/sub/core.c ] || fail "sub content missing (no vendor/sub/core.c)"

# --- Probe 1: delete a byte-clean file inside the mount. -------------
#  Must NOT abort DELDIRTY (157).  core.c was just written by GET, so
#  it is NOT a user edit — it has no business being called "dirty".
"$REAL_BE" delete vendor/sub/core.c >02.del.got.out 2>02.del.got.err
rc=$?
if [ "$rc" = 157 ]; then
    fail "probe-1 (delete vendor/sub/core.c) false-positived DELDIRTY/157:
$(cat 02.del.got.err)"
fi
[ "$rc" = 0 ] || fail "probe-1 (delete vendor/sub/core.c) exited $rc; stderr:
$(cat 02.del.got.err)"

# --- Probe 2: delete the gitlink dir itself. ------------------------
#  Must NOT abort DELDIRTY (157).
"$REAL_BE" delete vendor/sub >03.del.got.out 2>03.del.got.err
rc=$?
if [ "$rc" = 157 ]; then
    fail "probe-2 (delete vendor/sub) false-positived DELDIRTY/157:
$(cat 03.del.got.err)"
fi
[ "$rc" = 0 ] || fail "probe-2 (delete vendor/sub) exited $rc; stderr:
$(cat 03.del.got.err)"

note "delete/08: path-scoped sub delete did not false-positive DELDIRTY"
