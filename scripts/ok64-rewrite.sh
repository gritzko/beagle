#!/bin/sh
# ok64-rewrite: in-place rewrite every `con ok64 NAME = 0xOLD;` whose
# OLD doesn't match `abc/ok64 NAME` to use the canonical encoding.
# Idempotent: re-running on a clean tree changes nothing.
#
# Usage: scripts/ok64-rewrite.sh [src_root] [ok64_binary]

set -u

ROOT="${1:-$(cd "$(dirname "$0")/.." && pwd)}"
OK64="${2:-$ROOT/build-debug/abc/ok64}"

if [ ! -x "$OK64" ]; then
    echo "ok64-rewrite: $OK64 not executable" >&2
    exit 1
fi

TMP=$(mktemp -d -t ok64-rewrite-XXXXXX)
trap 'rm -rf "$TMP"' EXIT

DECL_RAW="$TMP/decl.raw"

# Same harvester as ok64-audit.
find "$ROOT" -type d \( -name build -o -name 'build-*' -o -name run-fuzz \
                       -o -name Corpus -o -name '.git' \) -prune -o \
            -type f \( -name '*.c' -o -name '*.h' \) -print \
  | xargs grep -EHn '^[[:space:]]*con[[:space:]]+ok64[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*=[[:space:]]*0x[0-9a-fA-F]+' \
  | sed -E 's/[[:space:]]*\/\/.*$//; s/[[:space:]]*\/\*.*$//' \
  | awk -F: '
      {
          file=$1; line=$2;
          rest=$0; sub(/^[^:]+:[^:]+:/, "", rest);
          if (match(rest, /con[[:space:]]+ok64[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*=[[:space:]]*0x[0-9a-fA-F]+/)) {
              decl=substr(rest, RSTART, RLENGTH);
              n=split(decl, parts, /[[:space:]]+|=[[:space:]]*/);
              name=""; hex="";
              for (i=1; i<=n; i++) {
                  if (parts[i]=="ok64" && i+1<=n) name=parts[i+1];
                  if (parts[i] ~ /^0x/) hex=parts[i];
              }
              if (name!="" && hex!="") printf "%s\t%s\t%s\t%s\n", name, hex, file, line;
          }
      }' \
  | sort -u > "$DECL_RAW"

CHANGED=0
SKIPPED=0
FAILED=0

while IFS=$(printf '\t') read -r NAME HEX FILE LINE; do
    EXPECT=$("$OK64" "$NAME" 2>/dev/null | awk -F'= 0x' '{print $2}' | tr -d ' ;')
    if [ -z "$EXPECT" ]; then
        echo "  SKIP $NAME ($FILE:$LINE) — abc/ok64 returned nothing" >&2
        SKIPPED=$((SKIPPED+1))
        continue
    fi
    DECL_NORM=$(printf '%s' "$HEX" | sed 's/^0x//' | tr 'A-F' 'a-f')
    EXPECT_NORM=$(printf '%s' "$EXPECT" | tr 'A-F' 'a-f')
    if [ "$DECL_NORM" = "$EXPECT_NORM" ]; then
        continue
    fi
    # Replace ONLY the hex on the declaration line. Anchor on the name
    # too so we don't accidentally touch a different literal that
    # happens to share the bytes.
    PATTERN="\\(con[[:space:]]\\{1,\\}ok64[[:space:]]\\{1,\\}${NAME}[[:space:]]*=[[:space:]]*\\)0x${DECL_NORM}"
    REPLACE="\\10x${EXPECT_NORM}"
    if sed -i "${LINE}s/${PATTERN}/${REPLACE}/" "$FILE" 2>/dev/null; then
        # Verify the substitution actually happened (sed -i never errors on no-match)
        if grep -q "0x${EXPECT_NORM}" "$FILE"; then
            echo "  $NAME: 0x$DECL_NORM -> 0x$EXPECT_NORM    $FILE:$LINE"
            CHANGED=$((CHANGED+1))
        else
            echo "  FAIL $NAME ($FILE:$LINE) — sed pattern did not match" >&2
            FAILED=$((FAILED+1))
        fi
    else
        echo "  FAIL $NAME ($FILE:$LINE) — sed exit $?" >&2
        FAILED=$((FAILED+1))
    fi
done < "$DECL_RAW"

echo
echo "=== ok64-rewrite summary ==="
echo "  rewritten: $CHANGED"
echo "  skipped:   $SKIPPED"
echo "  failed:    $FAILED"

[ "$FAILED" -eq 0 ]
