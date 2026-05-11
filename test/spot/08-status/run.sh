#!/bin/sh
. "$(dirname "$0")/../../lib/spot-case.sh"

# 01: bring up an empty workspace (no commits — just .be/).
sleep 0.02; cp "$CASE/empty.c" empty.c
"$BE" put empty.c  > /dev/null 2>&1
"$BE" post 'init msg'  > /dev/null 2>&1

# 02: `spot status` — sniff drove SPOTUpdate during the post above,
#     so the index now has one .spot.idx run with a non-zero entry
#     count (single tokenized .c blob).  stdout is empty; stderr is
#     the info line.
"$SPOT" status > 02.status.got.out 2> 02.status.got.err
empty    02.status.got.out
match_re "$CASE/02.status.err.txt" 02.status.got.err
