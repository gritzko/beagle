#!/bin/sh
#  get/14-sub-dirty-conflict-aggregate — `be get` refuses when a
#  submodule's wt has dirty bytes that conflict with the new pin
#  the parent's switch would force.
#
#  Status: WILL_FAIL.  Per SUBS.plan.md the design calls for a
#  compiler-style dry-run that collects all conflicts across the
#  forest and refuses atomically before any mutation lands.  The
#  current sniff GET does NOT have a dry-run mode; instead it runs
#  WEAVE merge inline, which silently merges most dirty-vs-target
#  divergences (visible as `sniff: weave-merged N file(s)` on
#  stderr).  A real refusal here would require either:
#    (a) sub-side conflicts the weave can't auto-resolve (the test
#        framework can't reliably synthesise this on small fixtures),
#    (b) a `be get --dry-run` flag that walks the forest, calls each
#        sub's pre-flight classifier, aggregates issues, exits
#        non-zero without mutating.
#  Flip off WILL_FAIL once (b) lands.
#
#  Inline-extends the 2-level fixture with a `bump` branch on
#  parent.git whose tree advances the vendor/sub gitlink from
#  SUB_C2 → SUB_C3.
#
#  Test sequence:
#    1. `be get $PARENT_URL?master` — mounts vendor/sub at SUB_C2.
#    2. Edit vendor/sub/core.c (dirty bytes that DIFFER from SUB_C3's
#       content, so a noop-overlay can't paper over them).
#    3. `be get $PARENT_URL?bump` — parent switch wants sub at SUB_C3.
#       Sub's recursive `be get` sees dirty core.c whose bytes differ
#       from both the baseline (SUB_C2) and the target (SUB_C3).
#       Sniff refuses; beagle aggregates worst exit.
#
#  Assertions:
#    * `be get` exits non-zero.
#    * stderr surfaces the sub conflict (some form of dirty/refused
#       message — exact form depends on which classifier branch trips).

. "$(dirname "$0")/../../lib/submodules.sh"

#  --- Push a `bump` branch on parent.git with gitlink at SUB_C3. ----
BUMP_SEED="$ETMP/parent-bump-seed"
rm -rf "$BUMP_SEED"
git clone -q "$PARENT_BARE" "$BUMP_SEED"
git -C "$BUMP_SEED" config user.email t@t
git -C "$BUMP_SEED" config user.name  T
git -C "$BUMP_SEED" checkout -q master
git -C "$BUMP_SEED" update-index --add --cacheinfo \
    160000,"$SUB_C3",vendor/sub
git -C "$BUMP_SEED" commit -qm 'parent: bump vendor/sub to C3'
git -C "$BUMP_SEED" push -q "$PARENT_BARE" master:refs/heads/bump

mkdir wt && cd wt

# --- Step 1: clone (sub mounts at SUB_C2). ---------------------------
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
[ -f vendor/sub/core.c ] || fail "step-1: sub did not mount"

# --- Step 2: dirty the sub.  Use content that won't match SUB_C3. ---
echo '/* get/14 sentinel — conflicts with SUB_C3 checkout */' \
    >> vendor/sub/core.c

# --- Step 3: switch parent to bump (sub gitlink advances). -----------
rc=0
"$BE" get "$PARENT_URL?bump" >02.get.got.out 2>02.get.got.err || rc=$?
[ "$rc" != 0 ] || fail "expected non-zero exit on dirty sub conflict;
got $rc; stderr:
$(cat 02.get.got.err)"

# Sub conflict surfaces.  Multiple textual forms are acceptable —
# sniff's pre-flight prints "dirty overlay <path>" lines for paths
# that conflict, and "GET refused" with a count if --force absent.
grep -qiE 'dirty|refused|overlap|conflict' 02.get.got.err \
    || fail "no dirty/refusal indication on stderr:
$(cat 02.get.got.err)"

# Sentinel byte survived (parent's force-no checkout did not
# clobber the dirty content of the sub).
grep -q 'get/14 sentinel' vendor/sub/core.c \
    || fail "sentinel disappeared — sub bytes overwritten"

note "get/14-sub-dirty-conflict-aggregate: sub conflict refused, sentinel survived (exit $rc)"
