#!/bin/sh
#  post/13-sub-dryrun-all-issues — `be post --dry-run` recurses the
#  forest reporting per-project dirty paths without committing.  Plan
#  §POST "Dry-run pass": fork `be post --dry-run` per sub; each level
#  reports its own state; aggregate; never commit, never bump.
#
#  Test:
#    1. `be get $PARENT_URL?master` — mount.
#    2. Edit a file in outer AND in sub.
#    3. Snapshot wtlog tails.
#    4. `be post --dry-run` — recursive status walk.
#    5. Exit code 0 (informational).
#    6. wtlog tails unchanged (no commits anywhere, no `put <sub>#`
#       bump in the parent).
#    7. stderr surfaces both levels' dirty paths.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "be get exited $rc; stderr:
$(cat 01.get.got.err)"

# Snapshot tips.
outer_pre=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                         END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
            .be/wtlog)
sub_pre=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                       END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
          vendor/sub/.be)
outer_log_pre=$(wc -c < .be/wtlog)
sub_log_pre=$(wc -c < vendor/sub/.be)

# Dirty both sides.
sleep 0.02
cat >> util.c <<'EOF'

int util_dry(void) { return 13; }
EOF
cat >> vendor/sub/core.c <<'EOF'

void sub_dry(void) { sub_counter--; }
EOF

# Dry-run.
"$BE" post --dry-run >02.post.got.out 2>02.post.got.err
rc=$?
[ "$rc" = 0 ] || fail "be post --dry-run exited $rc; stderr:
$(cat 02.post.got.err)"

# Tips unchanged (no commits anywhere).
outer_post=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                          END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
             .be/wtlog)
sub_post=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                        END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
           vendor/sub/.be)
[ "$outer_post" = "$outer_pre" ] || fail "outer tip moved during --dry-run ($outer_pre -> $outer_post)"
[ "$sub_post"   = "$sub_pre"   ] || fail "sub tip moved during --dry-run ($sub_pre -> $sub_post)"

# wtlog sizes unchanged — no put-rows or post-rows landed.
outer_log_post=$(wc -c < .be/wtlog)
sub_log_post=$(wc -c < vendor/sub/.be)
[ "$outer_log_pre" = "$outer_log_post" ] \
    || fail "outer wtlog grew during --dry-run ($outer_log_pre -> $outer_log_post)"
[ "$sub_log_pre" = "$sub_log_post" ] \
    || fail "sub wtlog grew during --dry-run ($sub_log_pre -> $sub_log_post)"

# POST-020: the per-level change-count is now a `post: <N> change(s)`
# ROWS summary on STDOUT (was a bare `sniff: N change(s)` stderr line —
# BE-005), the sub's riding its relayed module hunk.  Recursion into BOTH
# levels is proven by ≥2 such lines (one per project); STDERR stays clean.
nchg=$(grep -c 'post:.*change(s)' 02.post.got.out 2>/dev/null || echo 0)
[ "$nchg" -ge 2 ] \
    || fail "expected ≥2 'post: N change(s)' lines (per-level); got $nchg; stdout:
$(cat 02.post.got.out)
stderr:
$(cat 02.post.got.err)"
# STDERR must carry no bare sniff:/keeper: change-count or notice echo.
! grep -qE 'sniff:.*change\(s\)|keeper: post:' 02.post.got.err \
    || fail "bare sniff:/keeper: stderr echo leaked (POST-020):
$(cat 02.post.got.err)"

note "post/13-sub-dryrun-all-issues: both levels surfaced; no tips moved"
