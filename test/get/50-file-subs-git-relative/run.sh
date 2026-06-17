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
#  Fully local (file://), no ssh.  PROOF is byte-exact checkout in
#  layouts that ONLY the correct resolution can satisfy: 3a clones into a
#  DEEP non-sibling dir where a cwd-relative `../sub` misses, so a checked-
#  out sub proves parent-URI resolution; 3b moves the parent-relative
#  target away leaving only the literal-url copy, so a checked-out sub
#  proves the fallback fired.  The `sub fetch try=` debug trace is gone
#  (GET-026 cleaned default output).

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
         exit 1; }

#  Deep non-sibling dest: a cwd-relative `../sub` would resolve to
#  `c3a/x/sub` (absent), so a checked-out, byte-exact sub PROVES the
#  fetch used the parent-URI-resolved absolute `file://…/sub`, not the
#  raw literal `../sub`.
[ -f c3a/x/y/sub/s.txt ] \
    || { echo "FAIL(3a): sub not checked out (parent-relative url not resolved)" >&2
         exit 1; }
match sub/s.txt c3a/x/y/sub/s.txt

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
         mv "$SCRATCH/sub-hidden" "$SCRATCH/sub"; exit 1; }
mv "$SCRATCH/sub-hidden" "$SCRATCH/sub"

#  The parent-resolved target was moved to `sub-hidden` (computed-relative
#  MISSES); only the literal `../sub` reaches the cwd-relative copy at
#  `c3b/sub`.  A checked-out, byte-exact sub therefore PROVES the fallback
#  to the raw `.gitmodules` url fired after the computed-relative miss.
[ -f c3b/dest/sub/s.txt ] \
    || { echo "FAIL(3b): sub not checked out via fallback url" >&2; exit 1; }
match c3b/sub/s.txt c3b/dest/sub/s.txt

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
    || { cat c2.err >&2; echo "FAIL(2): clone .git git parent" >&2; exit 1; }

#  A `.git` parent uses the official absolute `.gitmodules` url directly
#  (no path computation); a byte-exact checkout proves it resolved.
[ -f c2/sub/s.txt ] \
    || { echo "FAIL(2): sub not checked out" >&2; exit 1; }
match sub/s.txt c2/sub/s.txt

echo "get/50-file-subs-git-relative: OK"
