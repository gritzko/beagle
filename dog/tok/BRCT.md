#   Bracket detection for tokenized files

BRCT provides bracket matching and region detection on tokenized
source files. It works with packed u32 token arrays (from TOK.h)
and the original source bytes, inspecting only `P`-tagged
(punctuation) tokens so that brackets inside strings and comments
are ignored. Supports `{}`, `()`, and `[]`. All functions return
token indices; use `tok32Val()` to recover the byte slice.

##  API

| Function | Description |
|----------|-------------|
| `BRCTMatch(toks, data, at)` | Find matching bracket (forward or backward). Returns index or -1. |
| `BRCTInner(from, till, toks, data, at)` | Innermost enclosing bracket pair around `at`. |
| `BRCTOuter(from, till, toks, data, at)` | Outermost enclosing bracket pair around `at`. |
| `BRCTCheck(toks, data)` | Check all brackets are balanced. OK or BRCTBAD. |
| `BRCTDepth(toks, data, at)` | Nesting depth at token `at` (0 = top level). |

##  Usage

```c
#include "tok/BRCT.h"
#include "tok/CT.h"

// tokenize a C file, then:
i64 m = BRCTMatch(toks, data, open_brace_idx);
if (m >= 0) {
    u8cs val;
    tok32Val(val, toks, data[0], m);
    // val is the closing brace byte slice
}
```

See `test/BRCT.c` for more examples.
