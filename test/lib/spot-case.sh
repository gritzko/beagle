# test/lib/spot-case.sh — sourced by spot/<case>/run.sh.
#
# Adds $SPOT (the spot binary) on top of the generic case helpers.
# spot lives next to be in $BIN; the same fall-back chain applies.

. "$(dirname "$0")/../../lib/case.sh"

SPOT=${SPOT:-${BIN:+$BIN/spot}}
SPOT=${SPOT:-$(command -v spot || true)}
[ -n "$SPOT" ] && [ -x "$SPOT" ] || {
    echo "spot-case.sh: cannot locate \`spot\` (set BIN=... or SPOT=...)" >&2
    exit 2
}
export SPOT
