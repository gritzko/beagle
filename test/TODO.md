# `be` verb × URI-shape coverage

5 verbs (GET, POST, PUT, PATCH, DELETE) × 5 URI parts
(`scheme:`, `//auth`, `path`, `?ref`, `#frag`) = 5 × 2^5 = 160 shapes.
Meanings of URI parts generally follow the URI logic:

 1. scheme is access protocol/mode,
 2. authority is a remote host (or a user email),
 3. path is project-relative,
 4. query conveys the versioning: branch, hash, etc
 5. fragment is text: commit message, search term, etc

Missing fragments can be implied from the context in some cases,
but generally missing scheme/authority is "no remote component",
missing path is whole-tree operations, no query is default/current
branch/version, no fragment is no message.

Mask `[SAPRF]`: bit set ⇒ that URI part is present.
URI samples — `S=ssh:`/`file:`, `A=//host` (or `//origin` w/o scheme),
`P=file.c`/`src/`, `R=?feat`, `F=#msg`/`#abc1234`.

## GET — repo → worktree

`#frag` = lookup expression (sha-prefix OR commit-msg substring).
Missing `//auth` resolves via branch↔remote assoc (cur or named
ref's tracking entry).

- [ ] `[-----]` `be get` — branch, remote status
- [ ] `[S----]` `be get ssh:` — fetch cur from cur's assoc via ssh, restore wt
- [ ] `[-A---]` `be get //origin` — ff cur from origin's counterpart
- [ ] `[SA---]` `be get ssh://host` — fetch from URL (registers alias), restore wt to cur
- [ ] `[--P--]` `be get file.c` — file restore from cur
- [x] `[S-P--]` `be get file:<path>` — wire local worktree (no ref) — branches/10
- [ ] `[-AP--]` `be get //origin/path` — fetch origin, restore path/ from cur-counterpart
- [ ] `[SAP--]` `be get ssh://host/path` — fetch URL, restore path/ from URL's cur
- [x] `[---R-]` `be get ?feat` — switch wt to branch `feat` — branches/01,04,07,09; get/01; diff/*
- [ ] `[S--R-]` `be get ssh:?feat` — fetch feat from feat's assoc via ssh, restore wt
- [ ] `[-A-R-]` `be get //origin?feat` — lazy fetch `feat` from origin
- [ ] `[SA-R-]` `be get ssh://host?feat` — fetch URL, restore wt to feat
- [ ] `[--PR-]` `be get file.c?feat` — overwrite one wt file from `feat` tip
- [ ] `[S-PR-]` `be get ssh:file.c?feat` — fetch one file from feat's assoc via ssh
- [ ] `[-APR-]` `be get //origin/path?feat` — fetch origin, restore path/ from feat
- [x] `[SAPR-]` `be get file://<path>?feat` — worktree-clone of `feat` (registers alias) — post/04
- [ ] `[----F]` `be get #tricky` — search cur's history (sha-prefix or msg), detach on match
- [ ] `[S---F]` `be get ssh:#frag` — fetch cur's assoc via ssh, search cur, detach on match
- [ ] `[-A--F]` `be get //origin#frag` — fetch origin, search cur, detach on match
- [ ] `[SA--F]` `be get ssh://host#frag` — fetch URL, search cur, detach on match
- [ ] `[--P-F]` `be get file.c#frag` — search cur's history, restore file.c at match
- [ ] `[S-P-F]` `be get ssh:file.c#frag` — fetch one file from cur's assoc via ssh, restore at match
- [ ] `[-AP-F]` `be get //origin/path#frag` — fetch origin, restore path/ at match
- [ ] `[SAP-F]` `be get ssh://host/path#frag` — fetch URL, restore path/ at match
- [ ] `[---RF]` `be get ?feat#frag` — search feat's history, detach on match
- [ ] `[S--RF]` `be get ssh:?feat#frag` — fetch feat from feat's assoc, search, detach
- [ ] `[-A-RF]` `be get //origin?feat#frag` — fetch origin, search feat, detach
- [ ] `[SA-RF]` `be get ssh://host?feat#frag` — fetch URL, search feat, detach
- [ ] `[--PRF]` `be get file.c?feat#frag` — search feat, restore file.c at match
- [ ] `[S-PRF]` `be get ssh:file.c?feat#frag` — fetch one file from feat's assoc, restore at match
- [ ] `[-APRF]` `be get //origin/path?feat#frag` — fetch origin, restore path/ from feat at match
- [ ] `[SAPRF]` `be get ssh://host/path?feat#frag` — fetch URL, restore path/ from feat at match

## POST — advance / commit

`#frag` = commit message.  Path scopes the commit/advance to that
path (partial commit, like `git add path && git commit`).
Missing `//auth` resolves via branch↔remote assoc.

- [ ] `[-----]` `be post` — no-op / dry-run status
- [ ] `[S----]` `be post ssh:` — push cur via ssh to cur's assoc
- [ ] `[-A---]` `be post //origin` — push cur to origin
- [ ] `[SA---]` `be post ssh://host` — push cur to explicit URL (registers alias)
- [ ] `[--P--]` `be post file.c` — partial commit: scope to file.c only
- [ ] `[S-P--]` `be post ssh:file.c` — partial commit + push via ssh to cur's assoc
- [ ] `[-AP--]` `be post //origin/path` — partial commit + push to origin
- [ ] `[SAP--]` `be post ssh://host/path` — partial commit + push to URL
- [x] `[---R-]` `be post ?feat` — advance `feat` to cur.tip (ff or rebase); cur untouched — branches/06,09; post/02,03
- [ ] `[S--R-]` `be post ssh:?feat` — push feat via ssh to feat's assoc
- [ ] `[-A-R-]` `be post //origin?feat` — advance `feat` locally + push to origin
- [ ] `[SA-R-]` `be post ssh://host?feat` — advance feat + push to explicit URL
- [ ] `[--PR-]` `be post file.c?feat` — partial advance: scope feat-advance to file.c only
- [ ] `[S-PR-]` `be post ssh:file.c?feat` — partial advance feat + push via ssh to feat's assoc
- [ ] `[-APR-]` `be post //origin/path?feat` — partial advance feat + push to origin
- [ ] `[SAPR-]` `be post ssh://host/path?feat` — partial advance feat + push to URL
- [x] `[----F]` `be post '#fix the typo'` — commit on cur with msg, cur advances — branches/01–10; post/01,04; patch/*; put/*
- [ ] `[S---F]` `be post ssh:#msg` — commit on cur, push via ssh to cur's assoc
- [ ] `[-A--F]` `be post //origin#msg` — commit on cur, advance, push to origin
- [ ] `[SA--F]` `be post ssh://host#msg` — commit on cur, push to explicit URL
- [ ] `[--P-F]` `be post file.c#msg` — partial commit with msg
- [ ] `[S-P-F]` `be post ssh:file.c#msg` — partial commit with msg + push via ssh
- [ ] `[-AP-F]` `be post //origin/path#msg` — partial commit with msg + push to origin
- [ ] `[SAP-F]` `be post ssh://host/path#msg` — partial commit with msg + push to URL
- [x] `[---RF]` `be post -m vN '?tags/vN'` — new commit + advance via legacy `-m` flag — diff/01
- [ ] `[S--RF]` `be post ssh:?feat#msg` — commit, advance feat, push via ssh to feat's assoc
- [ ] `[-A-RF]` `be post //origin?feat#msg` — new commit, advance `feat`, push to origin
- [ ] `[SA-RF]` `be post ssh://host?feat#msg` — commit, advance feat, push to URL
- [ ] `[--PRF]` `be post file.c?feat#msg` — partial commit on feat with msg
- [ ] `[S-PRF]` `be post ssh:file.c?feat#msg` — partial commit on feat + push via ssh to feat's assoc
- [ ] `[-APRF]` `be post //origin/path?feat#msg` — partial commit on feat + push to origin
- [ ] `[SAPRF]` `be post ssh://host/path?feat#msg` — partial commit on feat + push to URL

## PUT — create / stage / register

PUT has **no `#frag` aspect** per https://replicated.wiki/html/wiki/Verbs.html.  Cross-branch staging
(`path?ref`) and remote-branch creation (`//auth?ref` w/o URL) are
also undefined.

- [ ] `[-----]` `be put` — stage every tracked-and-dirty file
- [ ] `[S----]` `be put ssh:` — n/a: PUT register needs an explicit URL
- [ ] `[-A---]` `be put //origin` — n/a: PUT registers explicit URL, not alias name
- [ ] `[SA---]` `be put ssh://host` — register URL as remote alias (name from host)
- [x] `[--P--]` `be put file.c` — stage one file (or `src/` for subtree) — branches/01,02,03,04,06,10; put/01,02; spot/*; patch/*
- [ ] `[S-P--]` `be put ssh:file.c` — n/a: PUT stages locally; remote-staging undefined
- [ ] `[-AP--]` `be put //origin/path` — n/a: remote-staging undefined
- [ ] `[SAP--]` `be put ssh://host/path` — register URL as remote alias (path is part of URL)
- [x] `[---R-]` `be put ?./fix` / `?feat/new` — create branch at cur.tip (label move, no commit) — branches/01–04,09
- [ ] `[S--R-]` `be put ssh:?feat` — n/a: PUT branch-create is local-only
- [ ] `[-A-R-]` `be put //origin?feat` — n/a: remote-branch create undefined for PUT
- [ ] `[SA-R-]` `be put ssh://host?feat` — n/a: remote-branch create undefined for PUT
- [ ] `[--PR-]` `be put file.c?feat` — n/a: cross-branch staging undefined
- [ ] `[S-PR-]` `be put ssh:file.c?feat` — n/a
- [ ] `[-APR-]` `be put //origin/path?feat` — n/a
- [ ] `[SAPR-]` `be put ssh://host/path?feat` — n/a
- [ ] `[----F]` `be put #frag` — n/a: PUT has no frag aspect
- [ ] `[S---F]` `be put ssh:#frag` — n/a: PUT has no frag aspect
- [ ] `[-A--F]` `be put //origin#frag` — n/a: PUT has no frag aspect
- [ ] `[SA--F]` `be put ssh://host#frag` — n/a: PUT has no frag aspect
- [ ] `[--P-F]` `be put file.c#frag` — n/a: PUT has no frag aspect
- [ ] `[S-P-F]` `be put ssh:file.c#frag` — n/a: PUT has no frag aspect
- [ ] `[-AP-F]` `be put //origin/path#frag` — n/a: PUT has no frag aspect
- [ ] `[SAP-F]` `be put ssh://host/path#frag` — n/a: PUT has no frag aspect
- [ ] `[---RF]` `be put ?feat#frag` — n/a: PUT has no frag aspect
- [ ] `[S--RF]` `be put ssh:?feat#frag` — n/a: PUT has no frag aspect
- [ ] `[-A-RF]` `be put //origin?feat#frag` — n/a: PUT has no frag aspect
- [ ] `[SA-RF]` `be put ssh://host?feat#frag` — n/a: PUT has no frag aspect
- [ ] `[--PRF]` `be put file.c?feat#frag` — n/a: PUT has no frag aspect
- [ ] `[S-PRF]` `be put ssh:file.c?feat#frag` — n/a: PUT has no frag aspect
- [ ] `[-APRF]` `be put //origin/path?feat#frag` — n/a: PUT has no frag aspect
- [ ] `[SAPRF]` `be put ssh://host/path?feat#frag` — n/a: PUT has no frag aspect

## DELETE — remove

DELETE has **no `#frag` aspect** per https://replicated.wiki/html/wiki/Verbs.html.  Cross-branch
delete (`path?ref`) is also undefined.  Missing `//auth` resolves
via branch↔remote assoc for push-delete forms.

- [ ] `[-----]` `be delete` — n/a: no resource named
- [ ] `[S----]` `be delete ssh:` — push-delete cur via ssh to cur's assoc
- [ ] `[-A---]` `be delete //origin` — drop the remote alias entry
- [ ] `[SA---]` `be delete ssh://host` — drop alias by URL bytes
- [x] `[--P--]` `be delete file.c` / `src/` — unlink + append `delete <path>` row — branches/06,09
- [ ] `[S-P--]` `be delete ssh:file.c` — n/a: per-file remote delete undefined
- [ ] `[-AP--]` `be delete //origin/path` — n/a: per-file remote delete undefined
- [ ] `[SAP--]` `be delete ssh://host/path` — n/a: per-file remote delete undefined
- [x] `[---R-]` `be delete ?feat` — drop branch dir (leaf-only; `-r` for recursive) — branches/01,09
- [ ] `[S--R-]` `be delete ssh:?feat` — push-delete feat via ssh to feat's assoc
- [ ] `[-A-R-]` `be delete //origin?feat` — push delete: `<old> 000…0 refs/heads/feat`
- [ ] `[SA-R-]` `be delete ssh://host?feat` — push-delete to explicit URL
- [ ] `[--PR-]` `be delete file.c?feat` — n/a: cross-branch delete undefined
- [ ] `[S-PR-]` `be delete ssh:file.c?feat` — n/a
- [ ] `[-APR-]` `be delete //origin/path?feat` — n/a
- [ ] `[SAPR-]` `be delete ssh://host/path?feat` — n/a
- [ ] `[----F]` `be delete #frag` — n/a: DELETE has no frag aspect
- [ ] `[S---F]` `be delete ssh:#frag` — n/a: DELETE has no frag aspect
- [ ] `[-A--F]` `be delete //origin#frag` — n/a: DELETE has no frag aspect
- [ ] `[SA--F]` `be delete ssh://host#frag` — n/a: DELETE has no frag aspect
- [ ] `[--P-F]` `be delete file.c#frag` — n/a: DELETE has no frag aspect
- [ ] `[S-P-F]` `be delete ssh:file.c#frag` — n/a: DELETE has no frag aspect
- [ ] `[-AP-F]` `be delete //origin/path#frag` — n/a: DELETE has no frag aspect
- [ ] `[SAP-F]` `be delete ssh://host/path#frag` — n/a: DELETE has no frag aspect
- [ ] `[---RF]` `be delete ?feat#frag` — n/a: DELETE has no frag aspect
- [ ] `[S--RF]` `be delete ssh:?feat#frag` — n/a: DELETE has no frag aspect
- [ ] `[-A-RF]` `be delete //origin?feat#frag` — n/a: DELETE has no frag aspect
- [ ] `[SA-RF]` `be delete ssh://host?feat#frag` — n/a: DELETE has no frag aspect
- [ ] `[--PRF]` `be delete file.c?feat#frag` — n/a: DELETE has no frag aspect
- [ ] `[S-PRF]` `be delete ssh:file.c?feat#frag` — n/a: DELETE has no frag aspect
- [ ] `[-APRF]` `be delete //origin/path?feat#frag` — n/a: DELETE has no frag aspect
- [ ] `[SAPRF]` `be delete ssh://host/path?feat#frag` — n/a: DELETE has no frag aspect

## PATCH — absorb (history erased)

`#frag` overloads two roles: (a) commit-msg search to pick a
specific source commit on read-side absorb, or (b) spot-rewrite
expression (`#'old'->'new'`) when there's no ref/auth context.
Missing `//auth` resolves via branch↔remote assoc.  Bare/path-only
forms with no source default to cur, which makes them no-ops.

- [ ] `[-----]` `be patch` — n/a: defaults to cur; absorbing cur is no-op
- [ ] `[S----]` `be patch ssh:` — fetch cur's assoc via ssh, absorb
- [ ] `[-A---]` `be patch //origin` — fetch origin, absorb origin's cur-counterpart (≈ sync from origin)
- [ ] `[SA---]` `be patch ssh://host` — fetch URL, absorb URL's cur-counterpart
- [ ] `[--P--]` `be patch file.c` — n/a: defaults to cur; no-op
- [ ] `[S-P--]` `be patch ssh:file.c` — fetch file.c via ssh from cur's assoc, absorb
- [ ] `[-AP--]` `be patch //origin/path` — fetch origin, absorb path/ from cur-counterpart
- [ ] `[SAP--]` `be patch ssh://host/path` — fetch URL, absorb path/ from URL's cur
- [x] `[---R-]` `be patch ?./fix1` — absorb branch into cur — branches/02,10; patch/01,02,03
- [ ] `[S--R-]` `be patch ssh:?feat` — fetch feat from feat's assoc, absorb
- [ ] `[-A-R-]` `be patch //origin?main` — fetch + absorb remote branch (≈ `git pull --squash`)
- [ ] `[SA-R-]` `be patch ssh://host?feat` — fetch URL, absorb feat
- [ ] `[--PR-]` `be patch file.c?feat` — absorb one file's version from `feat`
- [ ] `[S-PR-]` `be patch ssh:file.c?feat` — fetch one file from feat's assoc, absorb
- [ ] `[-APR-]` `be patch //origin/path?feat` — fetch origin, absorb path/ from feat
- [ ] `[SAPR-]` `be patch ssh://host/path?main` — explicit-URL fetch+absorb branch
- [ ] `[----F]` `be patch #'old'->'new'.c` — spot rewrite (delegated, structural)
- [ ] `[S---F]` `be patch ssh:#frag` — fetch cur's assoc, absorb cur at commit-by-msg-match
- [ ] `[-A--F]` `be patch //origin#frag` — fetch origin, absorb cur-counterpart at msg-match
- [ ] `[SA--F]` `be patch ssh://host#frag` — fetch URL, absorb cur-counterpart at msg-match
- [ ] `[--P-F]` `be patch file.c#'old'->'new'` — scoped spot rewrite (file.c only)
- [ ] `[S-P-F]` `be patch ssh:file.c#frag` — fetch file.c from cur's assoc, absorb at msg-match
- [ ] `[-AP-F]` `be patch //origin/path#frag` — fetch origin, absorb path/ at msg-match
- [ ] `[SAP-F]` `be patch ssh://host/path#frag` — fetch URL, absorb path/ at msg-match
- [ ] `[---RF]` `be patch ?feat#frag` — absorb feat at commit-by-msg-match (not its tip)
- [ ] `[S--RF]` `be patch ssh:?feat#frag` — fetch feat from assoc, absorb at msg-match
- [ ] `[-A-RF]` `be patch //origin?feat#frag` — fetch origin, absorb feat at msg-match
- [ ] `[SA-RF]` `be patch ssh://host?feat#frag` — fetch URL, absorb feat at msg-match
- [ ] `[--PRF]` `be patch file.c?feat#frag` — absorb one file from feat at msg-match
- [ ] `[S-PRF]` `be patch ssh:file.c?feat#frag` — fetch from feat's assoc, absorb file at msg-match
- [ ] `[-APRF]` `be patch //origin/path?feat#frag` — fetch origin, absorb path/ from feat at msg-match
- [ ] `[SAPRF]` `be patch ssh://host/path?feat#frag` — fetch URL, absorb path/ from feat at msg-match


