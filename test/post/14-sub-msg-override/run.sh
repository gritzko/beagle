#!/bin/sh
#  post/14-sub-msg-override — `--sub-msg <subpath>=<alt>` overrides
#  the default `<parent-msg> [<subpath>]` decoration when the BEPost
#  wrapper forks into a sub.
#
#  Two-arc test:
#    Arc A — no override.  `be post '#round'`: sub's commit body
#            contains `round [vendor/sub]` (the default decoration).
#    Arc B — with override.  Reset wt to baseline (be get); dirty
#            again; `be post --sub-msg vendor/sub=alt-msg '#round2'`:
#            sub's commit body contains `alt-msg` exactly, and
#            does NOT contain `round2 [vendor/sub]`.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt && cd wt
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "be get exited $rc; stderr:
$(cat 01.get.got.err)"

# ====================================================================
# Arc A — no override; default decoration `round [vendor/sub]`.
# ====================================================================
sleep 0.02
cat >> util.c <<'EOF'

int util_a(void) { return 1; }
EOF
cat >> vendor/sub/core.c <<'EOF'

void sub_a(void) { sub_counter++; }
EOF

"$BE" post '#round' >02.post.got.out 2>02.post.got.err
rc=$?
[ "$rc" = 0 ] || fail "Arc A: be post exited $rc; stderr:
$(cat 02.post.got.err)"

sub_a_sha=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                         END { h=last; sub(/^[^#]*#/, "", h); print h }' \
            vendor/sub/.be)

(cd vendor/sub && "$BE" "commit:?$sub_a_sha" >../../03.subcommit.out 2>../../03.subcommit.err)
grep -q 'round \[vendor/sub\]' 03.subcommit.out \
    || fail "Arc A: sub commit msg missing default decoration 'round [vendor/sub]'; commit body:
$(cat 03.subcommit.out)"

# ====================================================================
# Arc B — `--sub-msg vendor/sub=alt-msg` overrides decoration.
# Both outer and sub dirty again with fresh edits.
# ====================================================================
sleep 0.02
cat >> util.c <<'EOF'

int util_b(void) { return 2; }
EOF
cat >> vendor/sub/core.c <<'EOF'

void sub_b(void) { sub_counter--; }
EOF

"$BE" post --sub-msg vendor/sub=alt-msg '#round2' \
    >04.post.got.out 2>04.post.got.err
rc=$?
[ "$rc" = 0 ] || fail "Arc B: be post --sub-msg exited $rc; stderr:
$(cat 04.post.got.err)"

sub_b_sha=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                         END { h=last; sub(/^[^#]*#/, "", h); print h }' \
            vendor/sub/.be)

(cd vendor/sub && "$BE" "commit:?$sub_b_sha" >../../05.subcommit.out 2>../../05.subcommit.err)
grep -q 'alt-msg' 05.subcommit.out \
    || fail "Arc B: sub commit msg missing override 'alt-msg'; commit body:
$(cat 05.subcommit.out)"
if grep -q 'round2 \[vendor/sub\]' 05.subcommit.out; then
    fail "Arc B: sub commit unexpectedly carries default decoration; body:
$(cat 05.subcommit.out)"
fi

# Outer's commit uses the parent's `#round2` (no decoration).
outer_b_sha=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                           END { h=last; sub(/^[^#]*#/, "", h); print h }' \
              .be/wtlog)
"$BE" "commit:?$outer_b_sha" >06.outercommit.out 2>06.outercommit.err
grep -q 'round2' 06.outercommit.out \
    || fail "Arc B: outer commit msg missing 'round2'; body:
$(cat 06.outercommit.out)"

note "post/14-sub-msg-override: default decoration AND override work per level"
