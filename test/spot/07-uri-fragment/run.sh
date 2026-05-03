#!/bin/sh
. "$(dirname "$0")/../../lib/spot-case.sh"

# 01: stage one .c file.
cp "$CASE/u.c" u.c
"$BE" put u.c     > /dev/null 2>&1
"$BE" post 'init msg' > /dev/null 2>&1

# 02: `be '#text.ext'` — bare URI with a search fragment routes to
#     spot via BE.cli's verb-less search dispatch.
"$BE" '#bar.c' > 03.uri.got.out 2> 03.uri.got.err
match "$CASE/03.uri.want.out" 03.uri.got.out
empty 03.uri.got.err
