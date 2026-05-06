#!/bin/sh
. "$(dirname "$0")/../../lib/spot-case.sh"

# 01: stage and commit one .c file.  sniff_indexer_fanout fires
# SPOTUpdate during the commit, so committed.c's basename hash lands
# in the trigram index keyed under each of needle_xyz's trigrams.
sleep 0.02; cp "$CASE/committed.c" committed.c
"$BE" put committed.c     > /dev/null 2>&1
"$BE" post 'init msg'     > /dev/null 2>&1

# 02: drop a second file into the worktree with the *same* search
# text — never put/post it.  CAPOScan's two ULOG streams cover both:
#   - committed.c arrives via KEEPTreeULog (CLASS_BOTH, clean mtime)
#     and passes the trigram filter (basename was indexed on commit).
#   - uncommitted.c arrives via SNIFFWtULog only (CLASS_WT_ONLY); the
#     filter is bypassed for wt-only/dirty rows so the new content is
#     still searched.  This is the proof that BOTH spines run: the
#     index path produces the first hit, the dirty path the second.
sleep 0.02; cp "$CASE/uncommitted.c" uncommitted.c

"$SPOT" -g needle_xyz > 03.grep.got.out 2> 03.grep.got.err
match "$CASE/03.grep.want.out" 03.grep.got.out
empty 03.grep.got.err
