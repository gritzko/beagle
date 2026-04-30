# test/ — data-driven `be` integration cases

Each `be` verb gets its own directory under `test/`.  Each case is a
numbered subdir whose `run.sh` performs a sequence of steps and matches
captured output against checked-in `*.want.*` files.  Per-case READMEs
are forbidden — case names + the per-verb `INDEX.md` carry the prose.

## Verb dirs

* [get/](get/INDEX.md)   — `be get`  (checkout) cases.
* [post/](post/INDEX.md) — `be post` (commit)   cases.
* [put/](put/INDEX.md)   — `be put`  (stage)    cases.
* [diff/](diff/INDEX.md) — `be diff` (`diff:` projector) cases.

(Existing beagle/test/ shell tests remain untouched — they are not
migrated.)

## Layout

    test/
    ├── INDEX.md
    ├── run.sh                        # suite runner
    ├── lib/
    │   ├── case.sh                   # sourced by every case run.sh
    │   └── verb.sh                   # sourced by every verb run.sh
    └── <verb>/
        ├── INDEX.md                  # per-verb case index
        ├── run.sh                    # `. ../lib/verb.sh; run_verb <verb>`
        └── NN-<name>/
            ├── run.sh                # the case driver
            ├── NN.<role>.txt         # input file for step NN
            ├── NN.<role>.want.out    # byte-exact expected stdout
            └── NN.<role>.want.err    # line-by-line regex expected stderr

## Filename roles

* `NN.<role>.txt` (or any non-`want.` / `got.` extension) — input.
* `NN.<role>.want.out` — byte-exact expected stdout.  Compared with `match`.
* `NN.<role>.want.err` — line-per-regex expected stderr.  Compared with
  `match_re`.  Empty file ⇒ stderr must be empty (use `empty` directly
  for clarity).
* `NN.<role>.got.out` / `.got.err` — written by the case driver during
  the run; deleted on success, kept on failure.

## case.sh API

Sourcing `lib/case.sh` (with `set -eu` already engaged) gives you:

* `$BE`     — absolute path to the `be` binary (resolved from `$BIN/be`,
  `$BE`, or `$PATH`).
* `$CASE`   — absolute path to the case dir (so you can `cp "$CASE/..."`).
* `$VERB`, `$NAME` — verb dir name and case dir name.
* `$SCRATCH` — per-pid scratch dir under `$TMP`; the cwd on entry.
* `match WANT GOT`    — byte-exact compare; unified diff to stderr on fail.
* `match_re WANT GOT` — every line of WANT (POSIX BRE) must match the
  corresponding line of GOT.  Empty WANT ⇒ GOT must also be empty.
* `empty GOT`         — assert `GOT` is zero-length; dump first 200
  bytes on failure.
* `must CMD ...`      — assert exit 0.
* `mustnt CMD ...`    — assert exit non-zero.

A trap deletes `$SCRATCH` on success and prints the path on failure
("scratch kept at ..."), so `ctest --output-on-failure` shows you
exactly where to poke around.

## verb.sh API

`run_verb VERB` discovers `<verb>/*/run.sh`, runs each in a child
`sh`, prints `OK` / `FAIL <verb>/<case>`, and exits 0 iff all pass.

## Adding a new case

1. Pick the verb dir (or create one with a one-line `run.sh` that
   sources `lib/verb.sh`, plus an `INDEX.md`).
2. Copy the simplest existing case dir, rename it `NN-<your-name>/`.
3. Edit `run.sh` and the `NN.<role>.*` data files.
4. No CMake edit needed — `file(GLOB ... CONFIGURE_DEPENDS)`
   auto-discovers the case on the next configure.
5. Append a one-line entry to the verb's `INDEX.md`.

## Running

    # via ctest (preferred)
    cd build-asan && ctest --output-on-failure -R '^be-'

    # by hand
    BIN=build-asan/bin sh test/run.sh
