# commit/ — `commit:` commit-object projector cases

`commit:?<ref|sha>` renders a commit object (headers + message) and,
per COMMIT-001, surfaces the commit's diff as a navigable link rather
than inlining it: keeper owns `commit:`, graf owns `diff:`, so the
emitter appends a `diff:?<sha>` U-token link (Bro follows it; graf's
`diff:?<sha>` commit-show form renders the commit-vs-first-parent
diff). The link is the QUERY form (`diff:?<sha>`), not the fragment
form — only the query form triggers graf's commit-show path.

* `01-diff-link/` — COMMIT-001.  Two commits of `foo.c`; asserts
  `commit:?v2` still carries the commit/author headers and the message,
  that the `--tlv` hunk stream carries a `diff:?<sha>` link for the
  resolved sha, and that the link target renders the commit's change.
