#!/bin/sh
#  post/41-sub-synth-no-sha-in-subproj — SUBS-021 repro.
#
#  A submodule committed onto its synthetic dot-branch sits on
#  `?/<subproj>/.<parent>` (Submodules.mkd §"Committing detached subs").
#  When something re-targets that sub with a query whose project slot is
#  the 40-hex PIN rather than the title — `?<sha>/.<parent>` — `be get`
#  used to BLINDLY treat the whole (non-hex) query as a checkout target.
#  The leading 40-hex prefix matched the pin object by prefix, so the
#  checkout "succeeded" and recorded a MALFORMED row whose query AND
#  fragment were both the literal `<sha>/.<parent>`:
#
#      get  ?<sha40>/.<parent>#<sha40>/.<parent>
#
#  i.e. a SHA leaked into the `<subproj>` slot and the `#fragment` was a
#  branch string, not a bare sha (SUBS-021, observed as the stray
#  `.be/abc/refs 266168WPCD` / `abc/.be` row in the dogfood store).
#
#  Fix (sniff/GET.c SNIFFGetURI raw-hex fallback): the fallback is now
#  gated on DOGIsHashlet(query) — a 6..40-hex sha prefix.  A non-hashlet
#  query (e.g. `?<sha40>/.par`: the `/.par` tail makes it non-hex /
#  over-length) falls through to a clean SNIFFFAIL instead of fabricating
#  the malformed ref.  (The first cut gated on DOGIsFullSha, which was
#  too strict — it rejected the documented sha-PREFIX get; see ASSERT 3.)
#
#  This table-driven case asserts, for each (query, expect) row:
#    * a well-formed synthetic ref `?/<sub>/.<parent>` with a BARE-sha
#      fragment is produced by a first post and survives untouched;
#    * a `?<sha>/.<parent>` get is REFUSED and writes NO row;
#    * NO row anywhere carries a sha in the project slot
#      (`?<40hex>/.` or `#<40hex>/.`).
#
#  Hermetic & fully local (file://, no ssh): one beagle peer store B1
#  holds parent + sub as sibling shards; a working tree B2 mounts the
#  sub on its synthetic coordinate.

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null

#  case.sh shielded $SCRATCH/.be; drop it so the dirs below bootstrap as
#  their own fresh stores.
rm -rf "$SCRATCH/.be"

command -v git >/dev/null 2>&1 || { echo "SKIP: git not found" >&2; exit 0; }

mkg() {  # $1 = repo dir — quiet init with file:// submodules allowed
    git init -q -b master "$1" >/dev/null 2>&1 || return 1
    git -C "$1" config user.email t@t
    git -C "$1" config user.name  T
    git -C "$1" config protocol.file.allow always
}

# 1. git chain: sub (project "sub") ← par (gitlink vendor/sub) ----------
mkg sub || { echo "FAIL(setup): git init sub"; exit 1; }
printf 'sub payload v1\n' > sub/core.c
git -C sub add -A; git -C sub commit -qm sub

mkg par || { echo "FAIL(setup): git init par"; exit 1; }
printf 'int main(void) { return 0; }\n' > par/main.c
git -C par -c protocol.file.allow=always submodule add -q "$SCRATCH/sub" vendor/sub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add sub → par"; exit 1; }
git -C par add -A; git -C par commit -qm par

# 2. clone git `par` into a beagle PEER store B1 (sibling shards) -------
mkdir -p B1/.be
( cd B1 && "$BE" get "file:$SCRATCH/par" >../01.b1.out 2>../01.b1.err ) \
    || { cat 01.b1.err >&2; echo "FAIL(setup): clone git par into B1" >&2; exit 1; }
for p in par sub; do
    [ -f "B1/.be/$p/refs" ] \
        || { echo "FAIL(setup): B1 missing project shard '$p'" >&2; exit 1; }
done

# 3. clone the BEAGLE peer's `par` into a working tree B2; sub mounts ---
mkdir -p B2/.be
( cd B2 && "$BE" get "file://$SCRATCH/B1?/par" >../02.b2.out 2>../02.b2.err ) \
    || { cat 02.b2.err >&2; echo "FAIL(setup): beagle clone of par into B2" >&2; exit 1; }
[ -f B2/vendor/sub/core.c ] \
    || { echo "FAIL(setup): B2 sub not mounted" >&2; exit 1; }

# 4. dirty ONLY the sub; recursive commit → parks the sub on the
#    synthetic branch `?/sub/.par`. -------------------------------------
sleep 0.02
cat >> B2/vendor/sub/core.c <<'EOF'

void sub_extra(void) { }
EOF
( cd B2 && "$BE" post '#sync' >../03.post.out 2>../03.post.err ) \
    || { cat 03.post.err >&2; echo "FAIL: recursive local commit" >&2; exit 1; }

#  ASSERT 1: the first post recorded a WELL-FORMED synthetic ref —
#  project slot is the TITLE `sub` (not a sha) and the fragment is a
#  BARE 40-hex sha.
synth_row=$(grep -E '^[^	]+	post	\?/sub/\.par#[0-9a-f]{40}$' B2/vendor/sub/.be | tail -1)
[ -n "$synth_row" ] \
    || { echo "FAIL: first post did not record ?/sub/.par#<bare-sha>" >&2
         echo "  sub .be:" >&2; cat B2/vendor/sub/.be >&2; exit 1; }

#  The synthetic pin (bare 40-hex) recorded by that post.
PIN=$(printf '%s\n' "$synth_row" | sed -E 's/^.*#//')
case "$PIN" in
    *[!0-9a-f]* | "" ) echo "FAIL: synthetic pin not bare hex: '$PIN'" >&2; exit 1 ;;
esac
[ "${#PIN}" -eq 40 ] || { echo "FAIL: synthetic pin not 40 hex: '$PIN'" >&2; exit 1; }

# 5. TABLE-DRIVEN: drive `be get <query>` inside the sub mount and check
#    that every query whose project slot is a SHA (`?<40hex>/.<parent>`)
#    is REFUSED rather than checked out verbatim.  Pre-fix, GET's raw-hex
#    fallback accepted these and wrote a malformed `?<sha>/.<parent>#<sha>/
#    .<parent>` row (sha in subproj slot, branch-string fragment).
#
#    query                          | (all must be REFUSED)
#    -------------------------------+----------------------
#    ?<PIN>/.par                    | sha + the parent dot-branch
#    ?<PIN>/.par/feat               | sha + a deeper dot-branch
run_get_refused() {  # $1 = query
    _q=$1
    #  Capture rc WITHOUT tripping `set -e` (a refused get exits 157).
    _rc=0
    ( cd B2/vendor/sub && "$BE" get "$_q" ) >g.out 2>g.err || _rc=$?
    [ "$_rc" != 0 ] \
        || { echo "FAIL[$_q]: sha-in-subproj get accepted (rc=0), expected refusal" >&2
             echo "  sub .be tail:" >&2; tail -3 B2/vendor/sub/.be >&2; exit 1; }
}

run_get_refused "?$PIN/.par"
run_get_refused "?$PIN/.par/feat"

#  ASSERT 2: after the whole table, NO row anywhere (sub wtlog or either
#  peer shard's refs) leaked a sha into the project slot.
if grep -rEq '[?#][0-9a-f]{40}/\.' B2/vendor/sub/.be B2/.be/sub/refs B1/.be/sub/refs 2>/dev/null; then
    echo "FAIL: a recorded ref leaked a sha into the <subproj> slot (SUBS-021)" >&2
    grep -rEn '[?#][0-9a-f]{40}/\.' B2/vendor/sub/.be B2/.be/sub/refs B1/.be/sub/refs >&2
    exit 1
fi

#  ASSERT 3 (SUBS-021 follow-up): the refusal above must NOT regress the
#  documented sha-PREFIX get (`be get '?abc1234'`, README §"sha-prefix
#  lookup").  The GET raw-hex fallback (sniff/GET.c SNIFFGetURI ~:2225)
#  is reached only when no branch-keyed REFS row matches the query — a
#  worktree whose wtlog rows are keyed by a branch name (`?master`), not
#  a full sha.  There, a query that is a SHA PREFIX falls through to the
#  fallback and is resolved by keeper's prefix lookup (hexlen capped at
#  15).  The first SUBS-021 fix gated that fallback on DOGIsFullSha,
#  which requires EXACTLY 40/64 hex and so REJECTED every legitimate
#  sha-prefix get (SNIFFFAIL).  The corrected gate is DOGIsHashlet (6..40
#  hex): it still rejects the malformed `?<sha40>/.par` table above (the
#  `/.par` tail is non-hex / over-length → not a hashlet) while letting a
#  clean sha-prefix through.  This block proves the prefix get SUCCEEDS.
#
#  Fresh git project G with two commits, cloned into a beagle peer store
#  PG (sibling shard `g`) and a working tree WG that mounts `g` — WG's
#  wtlog is branch-keyed (`?master#<tip>`), so a prefix get takes the
#  gated fallback (verified by trace during development).
mkg g || { echo "FAIL(setup): git init g"; exit 1; }
printf 'g payload v1\n' > g/g.c
git -C g add -A; git -C g commit -qm g1
printf 'g payload v2\n' > g/g.c
git -C g add -A; git -C g commit -qm g2

mkdir -p PG/.be
( cd PG && "$BE" get "file:$SCRATCH/g" >../pg.out 2>../pg.err ) \
    || { cat pg.err >&2; echo "FAIL(setup): clone git g into PG" >&2; exit 1; }
[ -f PG/.be/g/refs ] || { echo "FAIL(setup): PG missing shard 'g'" >&2; exit 1; }

mkdir -p WG/.be
( cd WG && "$BE" get "file://$SCRATCH/PG?/g" >../wg.out 2>../wg.err ) \
    || { cat wg.err >&2; echo "FAIL(setup): beagle clone of g into WG" >&2; exit 1; }
[ -f WG/g.c ] || { echo "FAIL(setup): WG g not mounted" >&2; exit 1; }

#  The two commits' shas, from the cloned shard's git tip + its parent.
GTIP=$(git -C g rev-parse HEAD)
GPAR=$(git -C g rev-parse HEAD~1)
case "$GTIP$GPAR" in *[!0-9a-f]*|"") echo "FAIL: bad g shas" >&2; exit 1;; esac

run_prefix_get_ok() {  # $1 = full sha, $2 = prefix length
    _full=$1; _n=$2
    _pre=$(printf '%s' "$_full" | cut -c1-"$_n")
    #  Under the OLD DOGIsFullSha gate this exact get returned SNIFFFAIL
    #  (the prefix is <40 hex); under DOGIsHashlet it checks out (rc 0).
    _rc=0
    ( cd WG && "$BE" get "?$_pre" ) >wg.g.out 2>wg.g.err || _rc=$?
    [ "$_rc" = 0 ] \
        || { echo "FAIL[?$_pre]: legitimate sha-prefix get refused (rc=$_rc)" >&2
             echo "  (regressed — DOGIsHashlet must accept a 6..40-hex prefix)" >&2
             tail -3 wg.g.err >&2; exit 1; }
}

#    full sha | prefix len  (each must SUCCEED)
#    ---------+-----------
#    tip      | 10
#    tip      | 8
#    parent   | 12   (non-tip commit, present in pack, not a ref tip)
run_prefix_get_ok "$GTIP" 10
run_prefix_get_ok "$GTIP" 8
run_prefix_get_ok "$GPAR" 12

echo "post/41-sub-synth-no-sha-in-subproj: no sha leaked into the subproj slot; sha-prefix get OK"
