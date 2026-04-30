# get/ — `be get` (checkout) integration cases

* `01-checkout/` — put + post + `be get '?'` on a fresh repo with one
  file (greet.txt = "hello world\n"); pins down post stderr shape and
  that GET succeeds against the just-committed branch tip.
