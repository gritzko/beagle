#!/bin/sh
# ok64-audit: cross-check every `con ok64 NAME = 0x...;` in the tree
# against `abc/ok64 NAME`. Reports mismatches (name<->value drift),
# duplicate values (two names colliding on the same code), and a final
# total compared to what abc/ok64 round-trips successfully.
#
# Usage: scripts/ok64-audit.sh [src_root] [ok64_binary]
#
# Defaults: src_root = repo root, ok64_binary = build-debug/abc/ok64.

set -u

ROOT="${1:-$(cd "$(dirname "$0")/.." && pwd)}"
OK64="${2:-$ROOT/build/abc/ok64}"

if [ ! -x "$OK64" ]; then
    echo "ok64-audit: $OK64 not executable" >&2
    exit 1
fi

TMP=$(mktemp -d -t ok64-audit-XXXXXX)
trap 'rm -rf "$TMP"' EXIT

# 1. Harvest every declaration. Match `con ok64 NAME = 0xHEX;` allowing
#    tabs/spaces. Strip C comments first to avoid false positives.
DECL_RAW="$TMP/decl.raw"
DECL_OK="$TMP/decl.ok"
DECL_BAD="$TMP/decl.bad"
DUPES="$TMP/dupes"
NAME_DUPES="$TMP/namedupes"

find "$ROOT" -type d \( -name build -o -name 'build-*' -o -name run-fuzz \
                       -o -name Corpus -o -name '.git' \) -prune -o \
            -type f \( -name '*.c' -o -name '*.h' \) -print \
  | xargs grep -EHn '^[[:space:]]*con[[:space:]]+ok64[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*=[[:space:]]*0x[0-9a-fA-F]+' \
  | sed -E 's/[[:space:]]*\/\/.*$//; s/[[:space:]]*\/\*.*$//' \
  | awk -F: '
      {
          file=$1; line=$2;
          rest=$0; sub(/^[^:]+:[^:]+:/, "", rest);
          # rest = full source line; pull NAME and HEX out
          if (match(rest, /con[[:space:]]+ok64[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*=[[:space:]]*0x[0-9a-fA-F]+/)) {
              decl=substr(rest, RSTART, RLENGTH);
              n=split(decl, parts, /[[:space:]]+|=[[:space:]]*/);
              # parts: con ok64 NAME 0xHEX
              name=""; hex="";
              for (i=1; i<=n; i++) {
                  if (parts[i]=="ok64" && i+1<=n) name=parts[i+1];
                  if (parts[i] ~ /^0x/) hex=parts[i];
              }
              if (name!="" && hex!="") printf "%s\t%s\t%s:%s\n", name, hex, file, line;
          }
      }' \
  | sort -u > "$DECL_RAW"

TOTAL=$(wc -l < "$DECL_RAW")
echo "=== ok64-audit: found $TOTAL declaration(s) under $ROOT ==="
echo

# 2. Detect duplicate names (same NAME declared in multiple places).
awk -F'\t' '{print $1"\t"$2}' "$DECL_RAW" | sort -u \
  | awk -F'\t' '{print $1}' | sort | uniq -d > "$NAME_DUPES"
if [ -s "$NAME_DUPES" ]; then
    echo "--- DUPLICATE NAMES (same NAME, different locations or values) ---"
    while IFS= read -r name; do
        echo "  $name:"
        grep -E "^${name}	" "$DECL_RAW" | sed 's/^/    /'
    done < "$NAME_DUPES"
    echo
fi

# 3. Detect value collisions (same HEX, different NAMEs).
awk -F'\t' '{print $2"\t"$1}' "$DECL_RAW" | sort -u \
  | awk -F'\t' '{print $1}' | sort | uniq -d > "$DUPES"
if [ -s "$DUPES" ]; then
    echo "--- VALUE COLLISIONS (same HEX shared by multiple names) ---"
    while IFS= read -r hex; do
        echo "  $hex:"
        grep -E "	${hex}	" "$DECL_RAW" | sed 's/^/    /'
    done < "$DUPES"
    echo
fi

# 4. For each (NAME, HEX), call abc/ok64 with the NAME and confirm the
#    encoded value matches the declared HEX. Drift = name no longer
#    encodes to the declared bits (someone changed the name without
#    updating the literal).
OK_COUNT=0
BAD_COUNT=0
: > "$DECL_OK"
: > "$DECL_BAD"
while IFS=$(printf '\t') read -r NAME HEX LOC; do
    # ok64 NAME prints `con ok64 NAME	= 0xVALUE;`
    EXPECT=$("$OK64" "$NAME" 2>/dev/null | awk -F'= 0x' '{print $2}' | tr -d ' ;')
    if [ -z "$EXPECT" ]; then
        printf '%s\t%s\t%s\tNO_DECODE\n' "$NAME" "$HEX" "$LOC" >> "$DECL_BAD"
        BAD_COUNT=$((BAD_COUNT+1))
        continue
    fi
    # Normalise both sides (lowercase, strip leading 0x)
    DECL_NORM=$(printf '%s' "$HEX" | sed 's/^0x//' | tr 'A-F' 'a-f')
    EXPECT_NORM=$(printf '%s' "$EXPECT" | tr 'A-F' 'a-f')
    if [ "$DECL_NORM" = "$EXPECT_NORM" ]; then
        printf '%s\t0x%s\t%s\n' "$NAME" "$DECL_NORM" "$LOC" >> "$DECL_OK"
        OK_COUNT=$((OK_COUNT+1))
    else
        printf '%s\t0x%s\t0x%s\t%s\n' "$NAME" "$DECL_NORM" "$EXPECT_NORM" "$LOC" >> "$DECL_BAD"
        BAD_COUNT=$((BAD_COUNT+1))
    fi
done < "$DECL_RAW"

if [ -s "$DECL_BAD" ]; then
    echo "--- MISMATCHES (declared HEX != ok64-encoded NAME) ---"
    echo "  NAME    declared    expected    location"
    sed 's/^/  /' "$DECL_BAD"
    echo
fi

echo "=== summary ==="
echo "  total declarations:   $TOTAL"
echo "  matches abc/ok64:     $OK_COUNT"
echo "  mismatches:           $BAD_COUNT"
echo "  duplicate names:      $(wc -l < "$NAME_DUPES")"
echo "  value collisions:     $(wc -l < "$DUPES")"

# Non-zero exit if anything is wrong
[ "$BAD_COUNT" -eq 0 ] && [ ! -s "$DUPES" ] && [ ! -s "$NAME_DUPES" ]
