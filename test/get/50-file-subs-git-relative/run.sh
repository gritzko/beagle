#!/bin/sh
#  get/50-file-subs-git-relative — SUBS-020.  `be get` from a GIT parent
#  resolves WHERE to fetch each submodule by parent-URI kind, validating
#  by gitlink-pin presence and falling back to the official
#  `.gitmodules` URL:
#
#    Case 2  — parent URI ENDS in `.git`: go straight to the official URL
#              from `.gitmodules` (NO path computation).
#    Case 3a — parent URI does NOT end in `.git`, relative sub URL
#              (`../sub`): compute the sub URI by resolving its path
#              relative to the parent URI (git-style: parent URL treated
#              as a directory), try THAT first.
#    Case 3b — same as 3a but the computed-relative URI MISSES; fall back
#              to the raw official `.gitmodules` URL.
#
#  The previous behavior passed the declared `url` verbatim to the
#  fetch, so a relative `../sub` only resolved by accident of the child
#  process cwd — it broke whenever the clone destination was not a
#  filesystem sibling of the parent.  SUBS-020 resolves it against the
#  parent SOURCE URI instead.
#
#  Fully local (file://), no ssh.  PROOF is the `sub fetch try=… => …`
#  trace ordering plus byte-exact checkout.

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null

#  case.sh shielded $SCRATCH/.be; drop it so the dirs below bootstrap
#  as their own fresh stores.
rm -rf "$SCRATCH/.be"

command -v git >/dev/null 2>&1 || { echo "SKIP: git not found" >&2; exit 0; }

mkg() {  # $1 = repo dir — quiet init with file:// submodules allowed
    git init -q -b master "$1" >/dev/null 2>&1 || return 1
    git -C "$1" config user.email t@t
    git -C "$1" config user.name  T
    git -C "$1" config protocol.file.allow always
}

# ---------------------------------------------------------------------
# 0. one shared sub upstream `sub` (the gitlink target for every parent).
# ---------------------------------------------------------------------
mkg sub  || { echo "FAIL(setup): git init sub";  exit 1; }
printf 'sub-body\n' > sub/s.txt; git -C sub add -A; git -C sub commit -qm sub

# =====================================================================
# CASE 3a — non-`.git` git parent, RELATIVE sub url `../sub`.  The
# computed-relative candidate (parent-URI dir + `../sub`) must be the
# source that lands the pin, even from a non-sibling clone destination.
# =====================================================================
mkg par3 || { echo "FAIL(setup): git init par3"; exit 1; }
printf 'par3\n' > par3/p.txt
git -C par3 -c protocol.file.allow=always submodule add -q "$SCRATCH/sub" sub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add sub → par3"; exit 1; }
#  Rewrite to a RELATIVE url so resolution must happen against the parent.
printf '[submodule "sub"]\n\tpath = sub\n\turl = ../sub\n' > par3/.gitmodules
git -C par3 add -A; git -C par3 commit -qm par3

#  Clone the non-`.git` parent into a DEEP, non-sibling destination so a
#  cwd-relative `../sub` would resolve to the wrong place — only true
#  parent-URI resolution finds it.
mkdir -p c3a/x/y/.be
( cd c3a/x/y && "$BE" get "file:$SCRATCH/par3" >../../../c3a.out 2>../../../c3a.err ) \
    || { cat c3a.err >&2
         echo "FAIL(3a): clone non-.git git parent w/ relative sub url" >&2
         grep -i 'sub fetch try\|submodule fetch failed' c3a.err >&2 || true
         exit 1; }

[ -f c3a/x/y/sub/s.txt ] \
    || { echo "FAIL(3a): sub not checked out (relative url not resolved)" >&2
         grep 'sub fetch try=' c3a.err >&2 || true; exit 1; }
match sub/s.txt c3a/x/y/sub/s.txt

#  The PARENT-RESOLVED URI (file://…/subs/sub, absolute) — NOT the raw
#  literal `../sub` — must be the source that lands the pin.
grep -q "sub fetch try=file://.*/sub => OK" c3a.err \
    || { echo "FAIL(3a): sub not sourced from the parent-resolved URI" >&2
         grep 'sub fetch try=' c3a.err >&2; exit 1; }

# =====================================================================
# CASE 3b — same non-`.git` parent kind, but the computed-relative URI
# MISSES; the raw official `.gitmodules` url is the fallback that lands
# the pin.  We force the miss by moving the parent-relative target away
# while leaving a cwd-relative copy the literal `../sub` can still reach.
# =====================================================================
mv "$SCRATCH/sub" "$SCRATCH/sub-hidden"
mkdir -p c3b && cp -r "$SCRATCH/sub-hidden" c3b/sub && mkdir -p c3b/dest/.be
( cd c3b/dest && "$BE" get "file:$SCRATCH/par3" >../../c3b.out 2>../../c3b.err ) \
    || { cat c3b.err >&2
         echo "FAIL(3b): clone w/ computed-rel miss did not fall back" >&2
         grep 'sub fetch try=' c3b.err >&2 || true
         mv "$SCRATCH/sub-hidden" "$SCRATCH/sub"; exit 1; }
mv "$SCRATCH/sub-hidden" "$SCRATCH/sub"

[ -f c3b/dest/sub/s.txt ] \
    || { echo "FAIL(3b): sub not checked out via fallback url" >&2
         grep 'sub fetch try=' c3b.err >&2; exit 1; }

#  Trace must show the computed-relative URI tried FIRST (and missing),
#  then the literal `../sub` fallback landing the pin.
grep -q "sub fetch try=file://.*/sub => " c3b.err \
    || { echo "FAIL(3b): computed-relative candidate was not tried first" >&2
         grep 'sub fetch try=' c3b.err >&2; exit 1; }
grep -q "sub fetch try=../sub => OK" c3b.err \
    || { echo "FAIL(3b): literal-url fallback did not land the pin" >&2
         grep 'sub fetch try=' c3b.err >&2; exit 1; }

# =====================================================================
# CASE 2 — parent URI ENDS in `.git`: NO path computation; the official
# (absolute, reachable) `.gitmodules` url is used directly.  Only ONE
# fetch candidate is tried.
# =====================================================================
git init --bare -q "$SCRATCH/par2.git"
mkg p2seed || { echo "FAIL(setup): git init p2seed"; exit 1; }
printf 'p2\n' > p2seed/p.txt
git -C p2seed -c protocol.file.allow=always submodule add -q "$SCRATCH/sub" sub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add sub → p2seed"; exit 1; }
git -C p2seed add -A; git -C p2seed commit -qm p2
git -C p2seed push -q "$SCRATCH/par2.git" master:master

mkdir -p c2/.be
( cd c2 && "$BE" get "file:$SCRATCH/par2.git" >../c2.out 2>../c2.err ) \
    || { cat c2.err >&2; echo "FAIL(2): clone .git git parent" >&2
         grep 'sub fetch try=' c2.err >&2 || true; exit 1; }

[ -f c2/sub/s.txt ] \
    || { echo "FAIL(2): sub not checked out" >&2
         grep 'sub fetch try=' c2.err >&2; exit 1; }
match sub/s.txt c2/sub/s.txt

#  A `.git` parent does NO relative computation: exactly one fetch try,
#  the official absolute url.
_n=$(grep -c 'sub fetch try=' c2.err)
[ "$_n" -eq 1 ] \
    || { echo "FAIL(2): expected 1 fetch try (no path calc), got $_n" >&2
         grep 'sub fetch try=' c2.err >&2; exit 1; }
grep -q "sub fetch try=$SCRATCH/sub => OK" c2.err \
    || { echo "FAIL(2): official url was not the (only) source tried" >&2
         grep 'sub fetch try=' c2.err >&2; exit 1; }

echo "get/50-file-subs-git-relative: OK"
