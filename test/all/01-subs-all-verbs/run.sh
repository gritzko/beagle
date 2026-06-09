#!/bin/sh
#  all/01-subs-all-verbs — capstone integration test: exercise EVERY verb's
#  submodule recursion on a parent+vendor/sub fixture and report a per-verb
#  verdict.  Each verb runs in its own fresh worktree so a failure in one does
#  not mask another.  The script exits non-zero while ANY verb's recursion is
#  broken, and exits 0 only when all are correct — hence it is registered
#  WILL_FAIL until SUBS-001 (post), SUBS-002 (patch) and SUBS-007 (head) land.
#
#  Verdict matrix (2026-06-05 audit):
#    get    works     (mount + checkout recurse)
#    put    works     (stage-all relays into sub; both-dirty)
#    delete works     (delete-all-missing relays into sub)
#    post   BROKEN    SUBS-001 (parent own edit dropped when sub also dirty)
#    head   BROKEN    SUBS-007 (sub working-tree dirty state not reported)
#    patch  BROKEN    SUBS-002 (gitlink aborts patch before sub recursion)

. "$(dirname "$0")/../../lib/submodules.sh"

FAILED=0
broke() { printf 'all/01 BROKEN [%s]: %s\n' "$1" "$2" >&2; FAILED=$((FAILED+1)); }
ok()    { printf 'all/01 ok      [%s]: %s\n' "$1" "$2"; }

# ---------------------------------------------------------------- GET --------
cd "$SCRATCH"; mkdir -p g/.be && cd g
if "$BE" get "$PARENT_URL?master" >get.out 2>get.err && \
   [ -f vendor/sub/.be ] && [ -f vendor/sub/core.c ]; then
    ok get "parent cloned, vendor/sub mounted + checked out"
else
    broke get "sub not mounted/checked out; err: $(cat get.err)"
fi

# ---------------------------------------------------------------- PUT ---------
cd "$SCRATCH"; mkdir -p p/.be && cd p
"$BE" get "$PARENT_URL?master" >get.out 2>get.err || broke put "get failed"
printf '\n// put parent\n' >> main.c
printf '\n// put sub\n'     >> vendor/sub/core.c
if "$BE" put >put.out 2>put.err && \
   grep -qE 'put[[:space:]]+main\.c' .be/wtlog && \
   grep -qE 'put[[:space:]]+core\.c' vendor/sub/.be; then
    ok put "both parent + sub staged, exit 0"
else
    broke put "stage-all did not recurse cleanly; err: $(cat put.err)"
fi

# ------------------------------------------------------------- DELETE --------
cd "$SCRATCH"; mkdir -p d/.be && cd d
"$BE" get "$PARENT_URL?master" >get.out 2>get.err || broke delete "get failed"
rm vendor/sub/helper.c
if "$BE" delete >del.out 2>del.err && \
   grep -qE 'delete[[:space:]]+helper\.c' vendor/sub/.be; then
    ok delete "delete-all-missing relayed into sub, exit 0"
else
    broke delete "delete did not recurse cleanly; err: $(cat del.err)"
fi

# ---------------------------------------------------------------- POST -------
#  SUBS-001: with both sides dirty, the parent's own edit must be committed.
cd "$SCRATCH"; mkdir -p o/.be && cd o
"$BE" get "$PARENT_URL?master" >get.out 2>get.err || broke post "get failed"
printf '\nint mark_all01(void){return 1;}\n' >> main.c
printf '\n// post sub\n'                      >> vendor/sub/core.c
if "$BE" post 'all01 both dirty' >post.out 2>post.err && \
   "$BE" blob:main.c?master >blob.out 2>blob.err && \
   grep -q 'mark_all01' blob.out; then
    ok post "parent own edit + gitlink bump both committed"
else
    broke post "SUBS-001: parent own edit dropped; post err: $(cat post.err)"
fi

# ---------------------------------------------------------------- HEAD -------
#  SUBS-007: dirtying a sub file must change the head report.
cd "$SCRATCH"; mkdir -p h/.be && cd h
"$BE" get "$PARENT_URL?master" >get.out 2>get.err || broke head "get failed"
"$BE" head '?' >head.clean 2>/dev/null
printf '\n// head dirty\n' >> vendor/sub/core.c
"$BE" head '?' >head.dirty 2>/dev/null
if diff -q head.clean head.dirty >/dev/null 2>&1; then
    broke head "SUBS-007: head report identical with/without dirty sub"
else
    ok head "sub working-tree dirty state reflected in head report"
fi

# ---------------------------------------------------------------- PATCH ------
#  SUBS-002: a patch whose source bumped the gitlink must re-get the sub.
cd "$SCRATCH"; mkdir -p a/.be && cd a
"$BE" get "$PARENT_URL?master" >get.out 2>get.err || broke patch "get failed"
"$BE" put ?./bumpsub >/dev/null 2>&1
"$BE" get ?./bumpsub >/dev/null 2>&1
printf '\n// ALL01 SUB BUMP\n' >> vendor/sub/core.c
"$BE" post '#bump' >bump.out 2>bump.err
"$BE" get ?.. >/dev/null 2>&1
if "$BE" patch ?./bumpsub >patch.out 2>patch.err && \
   grep -q 'ALL01 SUB BUMP' vendor/sub/core.c; then
    ok patch "gitlink-bumping patch re-got the sub"
else
    broke patch "SUBS-002: patch aborted / sub not re-got; err: $(cat patch.err)"
fi

# ---------------------------------------------------------------- verdict ----
cd "$SCRATCH"
echo "all/01: $FAILED verb(s) with broken submodule recursion"
[ "$FAILED" = 0 ] || exit 1
note "all/01 ok: every verb recurses into submodules correctly"
