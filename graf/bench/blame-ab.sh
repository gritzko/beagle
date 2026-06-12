#!/bin/sh
#  blame-ab.sh — A/B wall-time for BLAME-002.  Builds ONE large fixture
#  (long history, deep path, sizeable blobs), then blames it with:
#    REF = $REF_BIN/graf   (keeper-inflate descent; BLAME-001 only)
#    NEW = $BIN/graf        (index-served descent; BLAME-002)
#  reporting BLAMESTATS (treeinfl) + median wall-time over K runs for each.
#  The store is indexed once by NEW so the (tree,name) edges exist; REF
#  ignores them and inflates trees, so the treeinfl + time gap is the win.
set -eu
BIN=${BIN:?set BIN=<build>/bin}
REF_BIN=${REF_BIN:-/home/gritzko/beagle/build/bin}
BE="$BIN/be"; GRAF="$BIN/graf"; REFGRAF="$REF_BIN/graf"
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"
for t in "$BE" "$GRAF" "$REFGRAF"; do
    [ -x "$t" ] || { echo "FAIL: $t not executable" >&2; exit 1; }
done

N=${N:-300}                # history length
DEPTH_PATH="a/b/c/d/e/f"   # depth 6 path
CHANGE_EVERY=${CHANGE_EVERY:-75}
BLOB_LINES=${BLOB_LINES:-300}   # sizeable blobs so inflate cost shows
K=${K:-5}

TEST_ID=${TEST_ID:-GRAFblameab}
. "$(dirname "$0")/../../test/lib/repo-setup.sh"
TMP=$(rs_repo_base); mkdir -p "$TMP"
trap '_rc=$?; rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null||true; rmdir "${TMP%/*/*}" 2>/dev/null||true; exit $_rc' EXIT INT TERM
fail(){ echo "FAIL: $*" >&2; exit 1; }

gen_blob(){ # $1=salt -> BLOB_LINES lines on stdout
    awk -v n="$BLOB_LINES" -v s="$1" 'BEGIN{for(i=1;i<=n;i++)print "f"i"_"s"(){return "i";}"}'
}

#  WIDE dirs: each directory on the descent path also holds many sibling
#  files, so each TREE object is large and inflating it is costly — this
#  is what makes the keeper-inflate descent's per-commit cost (treeinfl)
#  show up as wall-time, the way a real deep+wide repo does.
WIDTH=${WIDTH:-600}
seed_width(){ # populate sibling files in each dir along DEPTH_PATH
    d=""
    for seg in $(printf '%s\n' "$DEPTH_PATH" | tr '/' ' '); do
        d="${d:+$d/}$seg"
        j=1; while [ "$j" -le "$WIDTH" ]; do : > "$d/sib$j.txt"; j=$((j+1)); done
    done
}

R="$TMP/repo"; rs_wt_at "$R"
git init --quiet -b main .; git config user.email t@t; git config user.name t
mkdir -p "$DEPTH_PATH" other
seed_width
gen_blob 0 > "$DEPTH_PATH/deep.c"

i=1
while [ "$i" -le "$N" ]; do
    gen_blob "noise$i" > other/noise.c
    if [ $((i % CHANGE_EVERY)) -eq 0 ]; then gen_blob "$i" > "$DEPTH_PATH/deep.c"; fi
    touch -d "2026-04-20 12:00:$(printf '%02d' $((i % 60)))" \
        other/noise.c "$DEPTH_PATH/deep.c" 2>/dev/null || true
    "$BE" post -m "c$i" "?v0.0.$i" >/dev/null 2>&1
    i=$((i + 1))
done
"$GRAF" index >/dev/null 2>&1 || fail "graf index failed"

#  `date +%N` is 0 on musl/busybox; use /usr/bin/time -f %e (seconds,
#  centisecond precision) for robust wall-time and take the best (min,
#  least noisy) over K runs.
TIMECMD=$(command -v /usr/bin/time || true)
run_median(){ # $1=graf-binary -> echoes "stats | best_s"
    G="$1"
    GRAF_BLAME_STATS=1 "$G" --plain "blame:$DEPTH_PATH/deep.c" >/dev/null 2>&1 || true # warm
    stats=$(GRAF_BLAME_STATS=1 "$G" --plain "blame:$DEPTH_PATH/deep.c" 2>&1 >/dev/null | grep '^BLAMESTATS' || true)
    best=""
    if [ -n "$TIMECMD" ]; then
        k=1
        while [ "$k" -le "$K" ]; do
            "$TIMECMD" -f '%e' "$G" --plain "blame:$DEPTH_PATH/deep.c" \
                >/dev/null 2>"$TMP/t"; v=$(cat "$TMP/t")
            best=$(printf '%s\n%s\n' "$best" "$v" | grep -E '^[0-9]' | sort -g | head -1)
            k=$((k+1))
        done
    fi
    printf '%s | best_s=%s\n' "$stats" "${best:-n/a}"
}

echo "  REF (keeper-inflate descent): $(run_median "$REFGRAF")"
echo "  NEW (index-served  descent): $(run_median "$GRAF")"
