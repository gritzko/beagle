#!/bin/sh
#  put/12-sub-path-scoped — PUT-001 repro.  A path-scoped `be put
#  <sub>/<file>` that names a file living INSIDE a mounted submodule
#  must relay into the sub's own staging, mirroring the way POST / GET
#  / DELETE descend into mounted subs (BEActSubsRelay).
#
#  Before the fix PUT resolved paths only against the PARENT tree and
#  never relayed a `<sub>/<path>` into the sub, so a file that plainly
#  exists in the mount (it shows under `be status:vendor/sub` as `unk`)
#  was reported "does not exist — skipped" → "no eligible paths" →
#  PUTNONE (exit 206).  This case asserts:
#    A. `be put vendor/sub/<file>` stages the file IN THE SUB — the
#       sub's wtlog gains a real `put <file>` row and the command
#       exits 0 (not PUTNONE).
#    B. `be put --nosub vendor/sub/<file>` does NOT relay — it stays
#       parent-bound and surfaces PUTNONE (the documented opt-out).
#
#  Gated on WITH_SSH (submodules.sh ssh://localhost fixture).

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)

# --- Clone parent at master (C3) — sub mounts at vendor/sub. ----------
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "clone be get exited $rc; stderr:
$(cat 01.get.got.err)"
[ -f vendor/sub/.be ]    || fail "sub did not mount (no vendor/sub/.be)"
[ -f vendor/sub/core.c ] || fail "sub content missing (no vendor/sub/core.c)"

# --- Create an UNTRACKED file inside the mounted sub. ---------------
#  Untracked in both the parent (only the gitlink is recorded) and the
#  sub (never committed), so the parent tree has no entry for it — the
#  exact shape the ticket reports as `status:vendor/sub` → `unk`.
echo '/* new untracked sub file */' > vendor/sub/newfile.c

#  Baseline sub wtlog length, so we can prove a put row was appended.
sub_rows_before=$(wc -l < vendor/sub/.be)

# --- Probe A: path-scoped put of the sub-interior file. -------------
#  Must relay into the sub and exit 0 (NOT PUTNONE/206).
"$BE" put vendor/sub/newfile.c >02.put.got.out 2>02.put.got.err
rc=$?
[ "$rc" = 0 ] || fail "be put vendor/sub/newfile.c exited $rc, want 0; stderr:
$(cat 02.put.got.err)"

#  The sub's own wtlog gained a real `put newfile.c` row (the path is
#  mount-relative inside the sub).
sub_rows_after=$(wc -l < vendor/sub/.be)
[ "$sub_rows_after" -gt "$sub_rows_before" ] \
    || fail "sub wtlog did not grow ($sub_rows_before -> $sub_rows_after); \
the put was not relayed into the sub. parent stderr:
$(cat 02.put.got.err)"
tail -1 vendor/sub/.be | grep -q "put	newfile.c" \
    || fail "sub wtlog tail is not a 'put newfile.c' row:
$(tail -1 vendor/sub/.be)"

# --- Probe B: --nosub must NOT relay (documented opt-out). ----------
#  Re-create on a fresh clone so the sub is clean and the parent has no
#  staged row; with --nosub the path stays parent-bound → PUTNONE.
cd ..
mkdir wt2 wt2/.be && cd wt2
"$BE" get "$PARENT_URL?master" >04.get.got.out 2>04.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "second be get exited $rc; stderr:
$(cat 04.get.got.err)"
echo '/* new untracked sub file */' > vendor/sub/newfile.c
sub_rows_before2=$(wc -l < vendor/sub/.be)

rc=0
"$BE" put --nosub vendor/sub/newfile.c >05.put.got.out 2>05.put.got.err || rc=$?
[ "$rc" != 0 ] \
    || fail "be put --nosub vendor/sub/newfile.c unexpectedly exited 0; \
--nosub must keep the path parent-bound (PUTNONE); stdout:
$(cat 05.put.got.out)"
sub_rows_after2=$(wc -l < vendor/sub/.be)
[ "$sub_rows_after2" = "$sub_rows_before2" ] \
    || fail "--nosub still relayed into the sub (wtlog grew \
$sub_rows_before2 -> $sub_rows_after2)"

note "put/12: path-scoped put of a sub-interior file relays into the sub; \
--nosub stays parent-bound (PUTNONE)"
