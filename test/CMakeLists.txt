# test/ — data-driven `be` integration cases.
#
# Each <verb>/<case>/ subdir gets its own ctest target named
# be-<verb>-<case>.  Discovery is via file(GLOB ...) so adding a new
# case dir does not require a CMake edit (re-run cmake to refresh).

# IMPORTANT: TMP must live OUTSIDE the dogs source tree.  `be` walks
# upward from the cwd looking for an existing `.be/` and would
# otherwise glue the test scratch onto the project's own repo
# (`/.../dogs/.be/`).  Use the timestamped run dir already created
# by the top-level CMakeLists.txt for the existing toy-repo tests.
set(_BE_TEST_TMP "${DOG_TEST_TMP_ROOT}/be-test")
file(MAKE_DIRECTORY "${_BE_TEST_TMP}")

# Glob <verb>/<case>/run.sh under this dir.  CONFIGURE_DEPENDS asks
# CMake to re-glob on rebuild, so newly added case dirs get picked up
# without a manual `cmake ..`.
file(GLOB _be_case_runs CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/*/*/run.sh")

# Tests in this list are drafted alongside spec rewrites, expected
# to FAIL until the implementation catches up.  Flip-back is part
# of the relevant impl PR.  See VERBS.todo.md for tracking.
set(_BE_WILL_FAIL_CASES
    # SUBS.plan.md get/14 — needs `be get --dry-run` + compiler-style
    # aggregation, neither of which exist yet.  Sniff GET currently
    # auto-merges dirty-vs-target divergence via WEAVE, so the
    # refusal the test expects doesn't fire.  See test header.
    "be-get-14-sub-dirty-conflict-aggregate"
    # Four-shape URI-ref probe.  Absolute trunk (`?/proj`) + bare `?`
    # work; absolute-with-branch (`?/proj/feat`) and project-relative
    # (`?feat`) fail because keeper upload-pack serves only the
    # currently-open shard's packs (TRIANGLE.todo.md gap #2b — multi-
    # branch fan-out is "Phase 1c: dir chain is always [trunk
    # shard 0]" per keeper/WIRE.c::WIREBuildSegments).
    "be-get-24-uri-refs"
    # SUBS audit (2026-06-05) bug repros — expected to FAIL until the named
    # ticket lands.  See ~/src/replicated.wiki/todo/SUBS.mkd.
    # SUBS-001 (post/26, post/27) fixed — removed from this list so they
    # must now pass normally (sniff/POST.c post_pd_cb).
    # SUBS-002 fixed (patch/28): sniff PATCH skips 160000 gitlinks + the be
    # entry-point resolver hands BEActSubsPatch a context-free pin.
    # SUBS-004 (put/09) + SUBS-005 (delete/08) landed via the dogs merge
    # (5fb56d6f / 9aabd863) — off WILL_FAIL so they must pass normally.
    # SUBS-001/002/007 capstone (be-all-01-subs-all-verbs) — all three landed
    # (dogs merge 7d37f90f) — off WILL_FAIL; every verb's sub-recursion passes.
    # SUBS-011 (be-spot-10-sub-recurse-search) FIXED: BEProjector now
    # fans search projectors into mounted subs (BEProjectorSubs) and
    # be get indexes each sub on mount (BEIndexMount) — off WILL_FAIL.
    # POST-005 (post/28) fixed — absorbed-tree (theirs) ULOG cursor gates the
    # untracked-file ADD; off WILL_FAIL so it must pass normally.
)

# Tests that need passwordless ssh to localhost (mirror what
# beagle/test/CMakeLists.txt does for roundtrip.sh).  Skipped when
# WITH_SSH is OFF so non-ssh dev environments stay green.  TMP must
# live under $HOME because keeper resolves `//host/path` URIs as
# remote-HOME-relative — case.sh's $SCRATCH already does (it sits
# under $HOME/tmp/run-<ts>/be-test/$$/<verb>/<name>).
set(_BE_NEEDS_SSH_CASES
    "be-post-05-non-ff-refused"
    "be-post-06-fetch-incremental"
    "be-get-05-submodule-from-ssh"
    "be-get-07-submodule-nosub"
    # SUBS.plan.md fixture (test/lib/submodules.sh) uses
    # ssh://localhost just like get/05.
    "be-head-03-sub-clean-flat"
    "be-head-04-sub-dirty"
    "be-head-05-sub-parent-ahead"
    "be-head-08-sub-declared-not-mounted"
    "be-get-11-sub-clone-recursive"
    "be-get-12-sub-add"
    "be-get-15-sub-force-recursive"
    "be-get-16-sub-prune-recursive"
    "be-head-06-sub-depth3"
    "be-head-07-sub-fetch-recursive"
    "be-head-18-sub-leaf-unreachable"
    "be-get-13-sub-remove"
    "be-get-14-sub-dirty-conflict-aggregate"
    "be-get-17-sub-rename"
    "be-get-18-sub-leaf-unreachable"
    "be-post-09-sub-flat-both-dirty"
    "be-post-10-sub-depth3-cascading"
    "be-post-11-sub-only-sub-dirty"
    "be-post-12-sub-only-parent-dirty"
    "be-post-13-sub-dryrun-all-issues"
    "be-post-15-sub-add-then-post"
    "be-post-14-sub-msg-override"
    "be-post-16-sub-push-postorder"
    "be-post-17-sub-msg-resolution-per-level"
    "be-get-20-sub-already-mounted"
    "be-get-32-sub-same-project-twice"
    # Triangular tests (be→be, be→git, git→be peer ring).
    "be-get-23-triangle"
    "be-post-18-triangle"
    "be-put-06-triangle"
    # SUBS-004 repro: bare `be put`, parent clean + sub dirty → exit 0.
    "be-put-09-sub-only-dirty-exit"
    # SUBS-009 repro: `be put <subpath>` on a declared-but-unmounted
    # sub-add candidate gives an actionable refusal, not "does not exist".
    "be-put-11-sub-add-refusal"
    # SUBS-005 repro: path-scoped delete of a sub / sub-interior file.
    "be-delete-08-sub-path-scoped-deldirty"
    "be-get-24-uri-refs"
    # Bug repro cases — all need an ssh-cloneable origin.
    "be-get-26-cached-no-wire"
    "be-get-26-remote-shard-refs"
    "be-get-27-ff-refuse"
    "be-get-28-linear-ahead-preserved"
    "be-post-19-pure-push-no-commit"
    # DIS-027: a refused push must surface the peer's reason, not WIRECLFL.
    "be-post-30-push-refused-reason"
    # DIS-026: be post //git-remote?<branch> pushes cur onto the peer's branch.
    "be-post-31-remote-branch-push"
    # DIS-028: explicit ?main resolves the peer's literal refs/heads/main
    # (like ?master), not WIRECLFL; bare-trunk still maps to refs/heads/main.
    "be-get-38-remote-main-branch"
    "be-post-20-push-prints-diff"
    "be-post-21-foster-ff-push"
    "be-post-22-recv-advances-wt"
    "be-patch-18-noop-already-absorbed"
    "be-patch-20-divergent-merge"
    "be-patch-21-rebase-one-remote"
    # SUBS audit (2026-06-05): per-verb submodule recursion cases.  All use
    # the test/lib/submodules.sh ssh://localhost fixture.
    "be-put-08-sub-stage-all-both-dirty"
    "be-put-09-sub-only-dirty-exit"
    "be-put-10-sub-nosub-suppress"
    # PUT-001 repro: path-scoped `be put <sub>/<file>` relays into the
    # mounted sub's own staging (like POST/GET/DELETE); --nosub opts out.
    "be-put-12-sub-path-scoped"
    "be-delete-07-sub-delete-all-missing"
    "be-delete-08-sub-path-scoped-deldirty"
    "be-delete-09-sub-nosub-suppress"
    "be-patch-28-sub-gitlink-bump-reget"
    "be-patch-29-sub-unchanged-gitlink-ok"
    # PATCH-001: a behind source whose only forward change is a gitlink
    # bump must stage the parent gitlink put-row (take-theirs), so a plain
    # be post records the pin with no manual `be put <sub>`.
    "be-patch-35-sub-gitlink-bump-staged"
    "be-post-26-sub-both-dirty-keeps-parent-edit"
    "be-post-27-sub-only-parent-dirty-commits"
    # SUBS-019: a gitlink-bump put-row after a sub re-mount must record
    # the real sub tip, never the empty-blob sha (mode 160000 not 100644).
    "be-post-39-sub-gitlink-bump-never-empty"
    # SUBS-006 repro: clean nested sub must no-op a git-peer push, not
    # abort it with NOBRANCH.
    "be-post-29-sub-clean-nested-remote-push"
    "be-head-21-sub-dirty-reported"
    "be-all-01-subs-all-verbs"
    "be-spot-10-sub-recurse-search"
    # STATUS-001: `be status!` extended (branch+remote rows) and
    # `be status:<sub>/` sub-scope.  Uses the submodules.sh ssh fixture.
    "be-status-01-extended-and-subscope"
    # SUBS-012: projector sub-recursion (diff fan-out + object routing).
    "be-diff-06-sub-change"
    "be-tree-01-sub-object-routing"
    # SUBS-020 POST mirror CASE 3: non-`.git` git parent push routes each
    # sub to the parent-relative url (needs ssh for a non-`.git` git
    # remote; the `.git` CASE 2 sibling test post/43 is local file://).
    "be-post-44-sub-push-git-parent-relative"
)

foreach(_run ${_be_case_runs})
    # _run = "<verb>/<case>/run.sh"  →  verb / case
    get_filename_component(_case_dir "${_run}"     DIRECTORY)
    get_filename_component(_case_name "${_case_dir}" NAME)
    get_filename_component(_verb_dir  "${_case_dir}" DIRECTORY)
    get_filename_component(_verb_name "${_verb_dir}" NAME)

    # skip lib/ if it ever sprouts a run.sh by accident
    if (_verb_name STREQUAL "lib")
        continue()
    endif()

    set(_test_name "be-${_verb_name}-${_case_name}")
    if (_test_name IN_LIST _BE_NEEDS_SSH_CASES AND NOT WITH_SSH)
        continue()
    endif()

    add_test(NAME ${_test_name}
        COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/${_run}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
    set_tests_properties(${_test_name} PROPERTIES
        ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${_BE_TEST_TMP}")
    if (_test_name IN_LIST _BE_WILL_FAIL_CASES)
        set_tests_properties(${_test_name} PROPERTIES WILL_FAIL TRUE)
    endif()
endforeach()
