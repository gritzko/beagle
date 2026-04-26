#!/bin/sh
#  squash.sh — squash a sub-branch with multiple commits into one
#  linear commit on the parent branch.
#
#  Workflow exercised:
#    1. Fresh repo; initial commit on heads/master touching a+b.
#    2. Label that tip as heads/master/feat (sub-branch ref).
#    3. Switch to feat via `sniff get ?heads/master/feat`;
#       make two "turbulent" commits (modify a, add c).
#    4. Switch back to master; make a concurrent commit (modify b).
#    5. From master: `sniff patch ?heads/master/feat` does a 3-way
#       merge (ours=master tip, theirs=feat tip, lca=initial).
#    6. `sniff post -m "squash feat"` lands the merged wt as ONE new
#       commit on master whose parent is master's concurrent tip.
#
#  Verifies:
#    * The squash commit includes every expected file (a modified by
#      feat, b modified by trunk, c added by feat).
#    * The squash commit's parent is master's concurrent tip, not the
#      base — i.e. trunk history stays linear and we haven't lost the
#      concurrent work.
#    * Master's REFS advances to the new squash commit.
#
#  Run: BIN=build-debug/bin sh sniff/test/squash.sh
set -eu

BIN=${BIN:-$(dirname "$0")/../../build-debug/bin}
export PATH="$BIN:$PATH"
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"

TMP=${TMP:-$HOME/tmp}
TEST_ID=${TEST_ID:-SNIFFsquash}
TMP=$TMP/$$/$TEST_ID
trap 'rm -rf "$TMP"' EXIT INT TERM
mkdir -p "$TMP"

fail() { echo "FAIL: $*" >&2; exit 1; }
note() { echo "  - $*"; }

WT="$TMP/wt"
mkdir -p "$WT"
cd "$WT"

#  Last sha recorded in .sniff — walk the query of the most recent
#  get/post/patch row (dog/QURY: `&`-separated ref/SHA specs) and
#  pick the first 40-hex SHA segment.  For a multi-SHA merge state
#  that's the "ours" sha, matching resolve_ours / post_load_baseline.
head_hex() {
    awk -F'\t' '$2=="post" || $2=="get" || $2=="patch" { last=$3 }
                END {
                    q = last
                    sub(/^[^?]*\?/, "", q)
                    sub(/#.*$/, "", q)
                    n = split(q, parts, "&")
                    for (i = 1; i <= n; i++) {
                        if (length(parts[i]) == 40 &&
                            parts[i] ~ /^[0-9a-f]+$/) {
                            print parts[i]; exit
                        }
                    }
                }' .sniff
}

#  Parent sha of a given commit, via `keeper get .#<hex>`.
parent_of() {
    hex=$1
    keeper get ".#$hex" 2>/dev/null \
        | awk '/^parent / { print $2; exit }'
}

#  Tip of a local ref via keeper refs.  Output lines look like
#  `  <key>\t→ ?<40-hex>`; grep the line, take the last token, strip
#  the leading `?`.
ref_tip() {
    key=$1
    keeper refs 2>/dev/null | awk -v k="$key" '
        {
            sub(/^[[:space:]]+/, "")
            tab = index($0, "\t")
            if (tab == 0) next
            kf = substr($0, 1, tab - 1)
            if (kf != k) next
            n = split($0, toks, /[[:space:]]+/)
            v = toks[n]
            sub(/^\?/, "", v)
            print v
            exit
        }'
}

# --- step 1: base commit on heads/master ------------------------------
echo "=== 1. base commit on master ==="
echo "a line 1" > a.txt
echo "b line 1" > b.txt
sniff post -m "base" >/dev/null
BASE=$(head_hex)
[ -n "$BASE" ] || fail "no base sha"
note "master BASE=$BASE"

# --- step 2: label same tip as heads/master/feat ----------------------
echo "=== 2. label heads/master/feat at BASE ==="
sniff post "?heads/master/feat" >/dev/null
FEAT_REF=$(ref_tip "?heads/master/feat")
[ "$FEAT_REF" = "$BASE" ] || fail "feat ref not at BASE (got=$FEAT_REF)"
note "heads/master/feat -> $FEAT_REF"

# --- step 3: switch to feat, two turbulent commits --------------------
echo "=== 3. feat branch: modify a, then add c ==="
sniff get "?heads/master/feat" >/dev/null
sleep 1
echo "a line 1 (feat mod)" > a.txt
sniff post -m "feat: rewrite a" >/dev/null
FEAT1=$(head_hex)
note "feat tip after rewrite=$FEAT1"

sleep 1
echo "c line 1" > c.txt
#  c.txt is untracked relative to FEAT1's baseline; implicit `sniff
#  post -m` no longer auto-stages strangers (post_decide rule:
#  "with no put/delete, post commits only tracked files").  Name the
#  new path explicitly.
sniff put c.txt >/dev/null
sniff post -m "feat: add c" >/dev/null
FEAT2=$(head_hex)
[ "$FEAT2" != "$FEAT1" ] || fail "feat tip didn't advance"
note "feat tip after add c=$FEAT2"

# --- step 4: switch back to master, concurrent commit -----------------
echo "=== 4. master: concurrent modify b ==="
sniff get "?heads/master" >/dev/null
#  After this get the wt reflects master tip (a=base, b=base, c=gone).
[ -f a.txt ] || fail "a.txt missing after switch to master"
[ ! -f c.txt ] || fail "c.txt should be pruned on switch to master"
grep -qF 'a line 1' a.txt || fail "a.txt not base content on master"

sleep 1
echo "b line 1 (trunk mod)" > b.txt
sniff post -m "trunk: rewrite b" >/dev/null
MASTER1=$(head_hex)
[ "$MASTER1" != "$BASE" ] || fail "master tip didn't advance"
note "master tip after rewrite b=$MASTER1"

# --- step 5: squash feat into master ----------------------------------
echo "=== 5. patch feat into master wt ==="
sniff patch "?heads/master/feat" 2>&1 | sed 's/^/  | /'
[ -f c.txt ] || fail "patch did not bring in c.txt from feat"
grep -qF '(feat mod)'  a.txt || fail "patch did not merge feat's a"
grep -qF '(trunk mod)' b.txt || fail "patch clobbered trunk's b"
grep -qF 'c line 1'    c.txt || fail "patch's c.txt has wrong content"
! grep -qF '<<<<' a.txt b.txt c.txt \
    || fail "unexpected conflict markers on disjoint edits"
note "merged wt: a(feat), b(trunk), c(feat)"

echo "=== 6. squash commit on master ==="
sniff post -m "squash feat into master" >/dev/null
SQUASH=$(head_hex)
[ "$SQUASH" != "$MASTER1" ] || fail "no new commit after squash post"
note "squash commit=$SQUASH"

#  `?heads/master` canonicalises to bare `?` (trunk).
MASTER_REF=$(ref_tip "?")
[ "$MASTER_REF" = "$SQUASH" ] \
    || fail "heads/master not advanced to squash ($MASTER_REF vs $SQUASH)"
note "heads/master -> $SQUASH"

#  History check: squash's parent must be master's concurrent tip.
SQUASH_PARENT=$(parent_of "$SQUASH")
[ "$SQUASH_PARENT" = "$MASTER1" ] \
    || fail "squash parent is $SQUASH_PARENT, expected MASTER1=$MASTER1"
note "squash parent = MASTER1 (linear history on master)"

#  A fresh checkout of the squash tip should materialise the merged
#  tree (a=feat, b=trunk, c=feat).
echo "=== 7. fresh checkout verifies squash tree ==="
WT2="$TMP/wt2"
mkdir -p "$WT2"
cp -r "$WT/.dogs" "$WT2/"
cd "$WT2"
sniff get "$SQUASH" >/dev/null
grep -qF '(feat mod)'  a.txt || fail "checkout: a.txt not feat-modified"
grep -qF '(trunk mod)' b.txt || fail "checkout: b.txt not trunk-modified"
grep -qF 'c line 1'    c.txt || fail "checkout: c.txt missing"
note "fresh wt reflects squashed tree"

echo
echo "=== sniff squash: OK ==="
