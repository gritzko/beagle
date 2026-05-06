# head/ — `be head` (peek / dry-run) integration cases

* `01-ahead-behind/` — `be head ?..` and `be head ?` on a diverged
  feature branch surface ahead/behind log lines.  WILL_FAIL until
  the cur-vs-ref diff-summary entry point lands (see VERBS.todo.md).
* `02-msg-search/` — `be head '#wonderful'` walks cur's first-parent
  chain and prints the first commit whose message body contains the
  fragment as a substring.  Covers match, miss, and read-only
  invariant (cur tip unchanged after the call).
