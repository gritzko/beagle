#!/bin/sh
#  status/01-extended-and-subscope — STATUS-001 repro.
#
#  Two status refinements on the shared `status:` ROWS banner (BE-005):
#
#    1. `be status!` (verb-bang, ≡ `be status --force`) is EXTENDED
#       status: the per-file rows PLUS the state of every tracked branch
#       tip and remote-tracking ref (uri -> hash) appended as rows to the
#       SAME `status:` table.  Plain `be status` stays file-only — no
#       branch / remote rows.  (Rehomes the old SNIFFGetSummary body,
#       orphaned by GET-026.)  Asserted in --color (--plain here) AND
#       --tlv.
#
#    2. `be status:<path>/` where `<path>` is a mounted submodule scopes
#       the report to ONLY that sub (relayed from the sub mount), never
#       the parent worktree's own rows.
#
#  Fixture: the ssh://localhost parent+sub from test/lib/submodules.sh.
#  After `be get <parent>?master` the parent shard REFS holds a local
#  `?master#<sha>` tip AND a remote `ssh://...parent.git?master#<sha>`
#  ref — exactly the two row classes `be status!` must surface.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "be get exited $rc; stderr:
$(cat 01.get.got.err)"
[ -f vendor/sub/core.c ] || fail "vendor/sub/core.c missing after get"

#  The remote-tracking ref key carries the upstream URL host; the local
#  tip is the bare `?master`.  Both must appear in `be status!`.
PARENT_HOST="ssh://localhost"

# --- 1. plain `be status` is FILE-ONLY (no branch / remote rows) ------
echo "=== 1. plain be status: file rows only ==="
"$BE" status --plain >10.plain.out 2>10.plain.err \
    || fail "be status exited nonzero: $(cat 10.plain.err)"
sed 's/^/  | /' 10.plain.out
if grep -q "$PARENT_HOST" 10.plain.out; then
    fail "plain be status leaked a remote uri row ($PARENT_HOST):
$(cat 10.plain.out)"
fi
#  A `?master#<40hex>` branch-tip row is the extended form; plain status
#  must not carry it.
if grep -Eq '\?master#[0-9a-f]{40}' 10.plain.out; then
    fail "plain be status leaked a branch-tip uri->hash row:
$(cat 10.plain.out)"
fi
note "plain be status: file-only (no branch/remote rows)"

# --- 2. `be status!` is EXTENDED (branch + remote uri->hash rows) -----
echo "=== 2. be status! --plain: branch + remote rows appended ==="
"$BE" status! --plain >20.bang.out 2>20.bang.err \
    || fail "be status! exited nonzero: $(cat 20.bang.err)"
sed 's/^/  | /' 20.bang.out
#  Still inside the SAME `status:` table (one banner).
grep -q '^status:' 20.bang.out \
    || fail "be status! lost the status: banner:
$(cat 20.bang.out)"
#  Local branch tip row: `?master#<sha>`.
grep -Eq '\?master#[0-9a-f]{40}' 20.bang.out \
    || fail "be status! missing the ?master branch-tip uri->hash row:
$(cat 20.bang.out)"
#  Remote-tracking ref row: carries the upstream URL host.
grep -q "$PARENT_HOST" 20.bang.out \
    || fail "be status! missing the remote uri->hash row ($PARENT_HOST):
$(cat 20.bang.out)"
note "be status! --plain: branch + remote uri->hash rows present"

# --- 3. `be status!` in --tlv carries the same rows -------------------
echo "=== 3. be status! --tlv: same rows in the machine stream ==="
"$BE" status! --tlv >30.bang.tlv 2>30.bang.tlv.err \
    || fail "be status! --tlv exited nonzero: $(cat 30.bang.tlv.err)"
strings 30.bang.tlv | sed 's/^/  T| /'
strings 30.bang.tlv | grep -Eq '\?master#[0-9a-f]{40}' \
    || fail "be status! --tlv missing the ?master branch-tip row"
strings 30.bang.tlv | grep -q "$PARENT_HOST" \
    || fail "be status! --tlv missing the remote uri->hash row"
#  And plain --tlv (no bang) must NOT carry them.
"$BE" status --tlv >31.plain.tlv 2>/dev/null || true
if strings 31.plain.tlv | grep -q "$PARENT_HOST"; then
    fail "plain be status --tlv leaked a remote uri row"
fi
note "be status! --tlv: rows present; plain --tlv clean"

# --- 4. `be status:<sub>/` scopes to the sub only --------------------
echo "=== 4. be status:vendor/sub/ — sub-scoped report ==="
"$BE" status:vendor/sub/ --plain >40.sub.out 2>40.sub.err \
    || fail "be status:vendor/sub/ exited nonzero: $(cat 40.sub.err)"
sed 's/^/  | /' 40.sub.out
#  The hunk address is the sub mount, not the parent worktree.
grep -q '^status:vendor/sub' 40.sub.out \
    || fail "be status:vendor/sub/ banner not scoped to the sub:
$(cat 40.sub.out)"
#  Parent-only tracked files (main.c / util.c live at the wt root, never
#  under vendor/sub) must NOT appear — the report is the sub's alone.
if grep -Eq '(^| )main\.c( |$)|(^| )util\.c( |$)' 40.sub.out; then
    fail "be status:vendor/sub/ leaked parent worktree files:
$(cat 40.sub.out)"
fi
note "be status:vendor/sub/ reports only the sub"

# --- 5. STATUS-002: hierarchical .gitignore crosses the sub boundary --
#  The sub (vendor/sub) has NO .gitignore of its own.  A PARENT-only
#  .gitignore at the wt root must still govern the sub's paths when
#  `be status:vendor/sub/` relays into it — otherwise build artifacts
#  flood the report with `unk`.  Also locks `!`-negation precedence.
echo "=== 5. STATUS-002: parent .gitignore governs sub paths ==="
#  Parent-only ignore: build-*/ and *.o, but un-ignore keep.o.
printf 'build-*/\n*.o\n!keep.o\n' >.gitignore
#  Build artifacts + a negated file inside the SUB (no .gitignore there).
mkdir -p vendor/sub/build-debug
: >vendor/sub/build-debug/CMakeCache.txt
: >vendor/sub/build-debug/obj.o
: >vendor/sub/keep.o
: >vendor/sub/scratch.o
"$BE" status:vendor/sub/ --plain >50.sub.out 2>50.sub.err \
    || fail "be status:vendor/sub/ exited nonzero: $(cat 50.sub.err)"
sed 's/^/  | /' 50.sub.out
#  The parent rule must hide build-debug/* and *.o crossing the sub
#  boundary — no `unk` rows for them.
if grep -Eq 'build-debug|obj\.o|scratch\.o' 50.sub.out; then
    fail "STATUS-002: parent .gitignore not applied to sub paths (flood):
$(cat 50.sub.out)"
fi
#  `!keep.o` un-ignores keep.o → it MUST surface as unk (precedence).
grep -q 'keep\.o' 50.sub.out \
    || fail "STATUS-002: !keep.o negation lost — keep.o should be unk:
$(cat 50.sub.out)"
note "STATUS-002: parent .gitignore governs sub paths; !keep.o negation honored"

note "status/01: extended status! + sub-scoped status: OK"
