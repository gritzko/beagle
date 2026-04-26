#!/bin/sh
#  verify-canonical-refs.sh — assert that a .dogs/refs file contains
#  only canonical URIs per dog/DOG.md's "Query mini-language" section.
#
#  Canonical row: `<ts>\t<verb>\t<uri>\n` where the URI must NOT
#  contain these unambiguous canonicaliser violations:
#
#    ?refs/heads/X, ?refs/tags/X   — `refs/` prefix leaks through
#    ?remotes/origin/*             — git-ism, not in model
#    #?<sha>                       — fragment with `?` prefix
#
#  Peer-observed refs (from wire operations like `keeper get`) preserve
#  the peer's ref name, so `?heads/master`-style rows for remote
#  tracking are accepted — the canonicaliser's trunk-alias collapse
#  applies to local CLI input, not to peer wire observations.
#
#  Usage: verify-canonical-refs.sh <repo-root>

set -eu

ROOT=${1:-.}
REFS="$ROOT/.dogs/refs"

if [ ! -f "$REFS" ]; then
    echo "verify-canonical-refs: missing $REFS" >&2
    exit 1
fi

#  Strip NUL padding (refs file is a FILEBook — trailing zeros).
BAD=$(tr -d '\0' < "$REFS" | awk -F'\t' '
    /\?refs\//          { print "refs/ prefix:", $0; exit 1 }
    /\?remotes\//       { print "remotes/ git-ism:", $0; exit 1 }
    /#\?/               { print "fragment leading ?:", $0; exit 1 }
    { print "OK:", $0 }
' || true)

case "$BAD" in
    *prefix:*|*git-ism:*|*leading*)
        echo "$BAD" | grep -v '^OK:' | head -5 >&2
        echo "FAIL: $REFS has non-canonical rows" >&2
        exit 1
        ;;
esac

NROWS=$(tr -d '\0' < "$REFS" | grep -c '' || true)
echo "verify-canonical-refs: $NROWS canonical row(s) in $REFS"
