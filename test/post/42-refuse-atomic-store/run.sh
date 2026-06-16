#!/bin/sh
#  post/42-refuse-atomic-store — POST-017: a REFUSED `be post` must be
#  atomic.  When the conflict-marker scan (POSTCFLCT) refuses a commit,
#  the store reflog (`<shard>/refs`), the keeper pack + index segments,
#  and the wtlog must be BYTE-IDENTICAL to before the attempt — no stray
#  `post` row, no orphan keeper objects, no fresh idx run.  Before the
#  fix the conflict scan ran mid-pack-feed (sniff/POST.c, in the blob
#  loop), so trees + earlier blobs were already written and the partial
#  pack was closed (= indexed) before POSTCFLCT returned, leaving orphan
#  objects + a rewritten keeper.idx in the store.  The fix hoists the
#  scan to a pre-flight (step 5a) BEFORE any store mutation.
#
#  Table-driven: each row stages one file and posts.  REFUSE rows carry
#  a full WEAVE conflict triple (<<<< / |||| / >>>>) and must be refused
#  with the store unchanged; the ACCEPT row is a plain file (control)
#  and must commit.  A final FORCE row re-posts a refused marker file
#  with --force and must commit (the documented escape hatch).
#
#  `.refs.idx` / `.wtlog.idx` are LSM read-caches the keeper rewrites on
#  every OPEN (a plain `be head` churns them too), so they are excluded
#  from the byte-identity check — the contract is about written objects,
#  the reflog, and the index SEGMENTS, not the volatile cache files.

. "$(dirname "$0")/../../lib/case.sh"

#  Inline WEAVE conflict triple, assembled at runtime so this test file
#  itself doesn't trip the very scan it exercises (sniff would refuse to
#  commit a tracked test fixture that literally contains the triple).
O='<<<<'; M='||||'; C='>>>>'

#  Snapshot every store file EXCEPT the volatile read-cache idx files,
#  as `<relpath>  <md5>` lines, sorted.  Used to assert byte-identity
#  across a refused post.
snapshot() {
    _dst=$1
    ( cd .be && find . -type f | sort ) | while read -r _f; do
        case "$_f" in
            */.refs.idx|*/.wtlog.idx|./.refs.idx|./.wtlog.idx) continue ;;
        esac
        printf '%s  %s\n' "$_f" "$(md5sum ".be/$_f" | cut -d' ' -f1)"
    done > "$_dst"
}

# --- baseline commit so a shard + refs row exist ----------------------
printf 'hello\n' > greet.txt
must "$BE" put greet.txt
must "$BE" post '#initial'

#  Quiesce read-path idx churn before the first snapshot (a read settles
#  the LSM caches we exclude anyway, but this keeps intent clear).
"$BE" head >/dev/null 2>&1 || true

# --- TABLE ------------------------------------------------------------
#  name | filename | body            | expect   ( REFUSE / ACCEPT / FORCE )
#  body uses $O/$M/$C placeholders for the marker quads.
run_row() {
    _name=$1; _file=$2; _body=$3; _expect=$4

    #  Materialise the body (expand the marker placeholders).
    printf '%b' "$_body" > "$_file"
    must "$BE" put "$_file"

    if [ "$_expect" = REFUSE ]; then
        snapshot "$SCRATCH/snap.before.$_name"
        #  The post MUST fail (POSTCFLCT).  mustnt asserts nonzero exit.
        mustnt "$BE" post "#$_name"
        "$BE" head >/dev/null 2>&1 || true
        snapshot "$SCRATCH/snap.after.$_name"
        if ! diff -u "$SCRATCH/snap.before.$_name" \
                     "$SCRATCH/snap.after.$_name" >&2; then
            echo "post/42: REFUSED post mutated the store ($_name)" >&2
            exit 1
        fi
        #  The refs reflog text must have no new row either.
        if ! grep -q . ".be/$P/refs"; then
            echo "post/42: refs vanished ($_name)" >&2; exit 1
        fi
    elif [ "$_expect" = FORCE ]; then
        must "$BE" post --force "#$_name"
    else  # ACCEPT
        must "$BE" post "#$_name"
    fi
}

#  1. Plain file — control: commits cleanly.
run_row accept-plain  normal.txt "just a normal file\n"                ACCEPT

#  2. Inline triple in a // comment — the classic false positive; the
#     scan still refuses (the marker shape is indistinguishable), and
#     the refusal must be atomic.
run_row refuse-inline weave1.c   "// $O""theirs$M""ours$C\nint x;\n"   REFUSE

#  3. Line-block triple (real WEAVE output shape) — refused, atomic.
run_row refuse-block  weave2.c   "a\n$O\nt\n$M\no\n$C\nb\n"            REFUSE

#  4. Re-post the line-block file with --force — the documented escape
#     hatch commits it.
run_row force-block   weave2.c   "a\n$O\nt\n$M\no\n$C\nb\n"            FORCE

echo "post/42: refused posts left the store byte-identical; force committed"
