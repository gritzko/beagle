#!/bin/sh
#  get/24-uri-refs — `be get` URI-ref resolution probe.  Per https://replicated.wiki/html/wiki/Verbs.html
#  §"Ref resolution" the query slot has four shapes:
#
#    Shape          | Form                | Resolves to
#    ---------------|---------------------|------------------------------
#    absolute       | `?/<project>/<br>`  | <store>/<project>/<br>/
#    absolute trunk | `?/<project>`       | <store>/<project>/ (trunk)
#    project-rel    | `?<branch>`         | <store>/<curproject>/<br>/
#    cur trunk      | `?`                 | cur project's trunk
#
#  This case spins up a be peer with one project (`proj1`) carrying
#  trunk + a sibling branch `feat`, then probes each shape from an
#  empty local wt.  Project-relative `?feat` requires the local wt to
#  be already anchored to a project (no implicit default project
#  exists today — https://replicated.wiki/html/wiki/Verbs.html spec, also `be put ?/<project>` is the
#  only way to anchor); the test exercises both the un-anchored
#  failure mode AND the post-anchor success path.
#
#  Each probe checks out into its own subdir (so wts don't collide)
#  and asserts the resulting `hello.c` matches the expected branch
#  content.
#
#  Gated on WITH_SSH because every probe round-trips through
#  `keeper upload-pack` over ssh://localhost.

. "$(dirname "$0")/../../lib/case.sh"

export GIT_CONFIG_GLOBAL=/dev/null

[ -n "${HOME:-}" ] || { echo "get/24: \$HOME unset" >&2; exit 1; }
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) echo "get/24: SCRATCH=$SCRATCH not under \$HOME=$HOME" >&2; exit 1;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

rm -rf "$SCRATCH/.be"

# ====================================================================
# Upstream U setup: one project `proj1` with trunk + `feat` branch.
# Trunk holds `hello.c = trunk content`; `feat` overwrites it with
# the feat-branch payload.  Both branches end up reachable via the
# wire on a single `keeper upload-pack <U>` invocation.
# ====================================================================
U="$SCRATCH/proj1"          # dir basename = project name
mkdir -p "$U/.be"
( cd "$U"
  cp "$CASE/00.trunk.c" hello.c
  sleep 0.02; "$BE" put hello.c   >setup.put.out 2>setup.put.err
  sleep 0.02; "$BE" post 'trunk seed'  >setup.post.out 2>setup.post.err
  sleep 0.02; "$BE" put '?feat'        >setup.mkbr.out 2>setup.mkbr.err
  sleep 0.02; "$BE" get '?feat'        >setup.cd.out   2>setup.cd.err
  sleep 0.02; cp "$CASE/01.feat.c" hello.c
  sleep 0.02; "$BE" post 'feat commit' >setup.feat.out 2>setup.feat.err
)
REL_U="$REL_SCRATCH/proj1"

# Helper: clone into a fresh subdir using one URI shape, assert
# `hello.c` matches `$2` (one of the case data files).
probe() {
    _dst="$1"; _want="$2"; _uri="$3"
    mkdir -p "$SCRATCH/$_dst/.be"
    ( cd "$SCRATCH/$_dst"
      sleep 0.02
      "$BE" get "$_uri" >got.out 2>got.err
    )
    [ -f "$SCRATCH/$_dst/hello.c" ] || {
        echo "FAIL $_dst (URI=$_uri): hello.c missing" >&2
        cat "$SCRATCH/$_dst/got.err" >&2 | head -10
        exit 1
    }
    match "$CASE/$_want" "$SCRATCH/$_dst/hello.c"
}

# ====================================================================
# Probe 1 — absolute trunk: `?/proj1`
# ====================================================================
probe L1 00.trunk.c "be://localhost/$REL_U?/proj1"

# ====================================================================
# Probe 2 — absolute with branch: `?/proj1/feat`
# ====================================================================
probe L2 01.feat.c "be://localhost/$REL_U?/proj1/feat"

# ====================================================================
# Probe 3 — project-relative: `?feat`
#   Requires the wt to be anchored to a project FIRST.  Two passes:
#     3a. Bare `?feat` in a fresh wt → expected to FAIL (no anchor,
#         no default project).
#     3b. After anchoring via `?/proj1` (Probe 1's success), a
#         project-relative `?feat` resolves against cur project.
# ====================================================================
# 3a: fresh wt, bare ?feat — must fail
mkdir -p "$SCRATCH/L3a/.be"
( cd "$SCRATCH/L3a"
  sleep 0.02
  "$BE" get "be://localhost/$REL_U?feat" >got.out 2>got.err
) && {
    echo "FAIL L3a (?feat on fresh wt): expected non-zero, got success" >&2
    exit 1
} || true
[ ! -f "$SCRATCH/L3a/hello.c" ] || {
    echo "FAIL L3a (?feat on fresh wt): hello.c should not exist" >&2
    exit 1
}

# 3b: L1 is already anchored to proj1; ?feat should resolve.
( cd "$SCRATCH/L1"
  sleep 0.02
  "$BE" get "be://localhost/$REL_U?feat" >l3b.got.out 2>l3b.got.err
)
match "$CASE/01.feat.c" "$SCRATCH/L1/hello.c"

# ====================================================================
# Probe 4 — cur trunk: `?` (empty query slot).
#   L1 was just switched to feat by Probe 3b.  Bare `?` should bring
#   it back to cur project's trunk.
# ====================================================================
( cd "$SCRATCH/L1"
  sleep 0.02
  "$BE" get "be://localhost/$REL_U?" >l4.got.out 2>l4.got.err
)
match "$CASE/00.trunk.c" "$SCRATCH/L1/hello.c"
