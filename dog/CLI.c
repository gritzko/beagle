#include "CLI.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "DOG.h"
#include "abc/ANSI.h"
#include "abc/FILE.h"
#include "abc/PRO.h"

// Check if flag appears in the val_flags string (NUL-separated list).
// e.g. val_flags = "-g\0-C\0-r\0" (triple-NUL terminated).
static b8 cli_takes_val(char const *val_flags, u8csc flag) {
    if (val_flags == NULL) return NO;
    char const *p = val_flags;
    while (*p) {
        a_cstr(vf, p);
        if ($eq(flag, vf)) return YES;
        p += strlen(p) + 1;
    }
    return NO;
}

ok64 CLIParse(cli *c, char const *const *verb_names,
              char const *val_flags) {
    //  Caller must PATHu8bAlloc(c->repo) + u8csbAlloc(c->flags) +
    //  uribAlloc(c->uris) before invoking CLIParse (CLAUDE.md §5:
    //  alloc at the top of the call chain).
    sane(c != NULL && c->repo[0] != NULL
         && c->flags[0] != NULL && c->uris[0] != NULL);

    // Worktree root — feed into the caller-owned path buffer.  Borrow
    // a temporary `home` just to walk up to the anchor; caller may
    // open its own home later from $path(c->repo).
    //
    // c->repo carries the *worktree* path (h->wt), not the store path
    // (h->root) — for secondary worktrees these differ.  sniff /
    // beagle layer `<reporoot>/<rel>` to compose absolute paths to
    // the user's files, which live in the wt.
    {
        home rh = {};
        uri none = {};
        if (HOMEOpen(&rh, &none, NO) == OK) {
            (void)PATHu8bFeed(c->repo, u8bDataC(rh.wt));
        }
        HOMEClose(&rh);
    }

    int argn = (int)$arglen;
    int ai = 1;  // skip argv[0]

    // Verb: first arg that matches a known verb name.  Argv order is
    // fixed: `be [verb] [flags] [URIs]` (VERBS.md).  No verb scan past
    // leading flags — flags before the verb don't classify here.
    if (ai < argn && verb_names != NULL) {
        a$rg(a, ai);
        for (char const *const *vn = verb_names; *vn != NULL; vn++) {
            a_cstr(vs, *vn);
            if ($eq(a, vs)) {
                $mv(c->verb, a);
                ai++;
                break;
            }
        }
    }

    // Remaining args: flags or URIs.  Every non-flag arg becomes one
    // URI via DOGNormalizeArg — bare tokens land in u->path
    // (RFC 3986 path-noscheme), whitespace-bearing args land in
    // u->fragment, and properly-shaped tokens go through the URI
    // lexer.  Verbs interpret the resulting slices: post/patch
    // demote path → ref; put/delete/bro use path as-is.  Free-form
    // text (commit messages, search strings) must be a single
    // whitespace-bearing arg (`be post "fix the typo"`) or carry
    // an explicit `#` (`be post '#fix'`); legacy `-m <msg>` still
    // works.
    a_cstr(empty_val, "");                  // non-NULL empty sentinel
    while (ai < argn) {
        a$rg(a, ai);
        ai++;

        if ($len(a) >= 1 && a[0][0] == '-') {
            //  Flag pair (flag + val).  u8csbFeed1 returns SNOROOM
            //  when the alloc'd cap is hit; skip the pair in that
            //  case to preserve the existing "drop overflow" shape.

            // Check for --flag=value
            u8cs flag_part = {};
            u8cs val_part = {};
            $mv(flag_part, a);
            $for(u8c, ep, a) {
                if (*ep == '=') {
                    flag_part[1] = ep;
                    val_part[0] = ep + 1;
                    val_part[1] = a[1];
                    break;
                }
            }

            if (!$empty(val_part)) {
                // --flag=value
                if (u8csbFeed1(c->flags, flag_part) != OK) continue;
                if (u8csbFeed1(c->flags, val_part)  != OK) continue;
            } else if (cli_takes_val(val_flags, a)) {
                // -f value (separate args) or -fN (attached)
                // Check for short flag with attached value: -XY where
                // -X takes a value and Y... is the value
                b8 attached = NO;
                if (a[0][1] != '-' && $len(a) > 2) {
                    // Short flag: -X is first 2 chars, rest is value
                    u8cs sf = {a[0], a[0] + 2};
                    if (cli_takes_val(val_flags, sf)) {
                        if (u8csbFeed1(c->flags, sf) != OK) continue;
                        u8cs sv = {a[0] + 2, a[1]};
                        if (u8csbFeed1(c->flags, sv) != OK) continue;
                        attached = YES;
                    }
                }
                if (!attached) {
                    if (u8csbFeed1(c->flags, a) != OK) continue;
                    u8cs v = {};
                    if (ai < argn) {
                        a$rg(v_local, ai);
                        ai++;
                        $mv(v, v_local);
                    } else {
                        $mv(v, empty_val);
                    }
                    if (u8csbFeed1(c->flags, v) != OK) continue;
                }
            } else {
                // Boolean flag
                if (u8csbFeed1(c->flags, a)         != OK) continue;
                if (u8csbFeed1(c->flags, empty_val) != OK) continue;
            }
        } else {
            // Non-flag arg → URI via DOGNormalizeArg.  Fill the
            // buffer's idle slot in place (uri is ~80B; avoids a
            // value copy through uribFeed1), then commit with
            // uribFed1.  Overflow → SNOROOM → drop with a hint.
            uri *u = uribIdleHead(c->uris);
            if (u == NULL || uribIdleLen(c->uris) == 0) {
                fprintf(stderr,
                    "cli: too many URIs (cap %u) — dropping %.*s\n",
                    (unsigned)CLI_MAX_URIS,
                    (int)$len(a), (char *)a[0]);
                continue;
            }
            zerop(u);
            DOGNormalizeArg(u, a);
            $mv(u->data, a);    // restore original data slice
            (void)uribFed1(c->uris);
        }
    }
    done;
}

void CLISetHUNKMode(cli const *c) {
    //  --ansi accepted as a legacy alias for --color; the documented
    //  rule is the three-flag set (--tlv / --color / --plain).
    if      (c && CLIHas(c, "--tlv"))   HUNKMode = HUNKOutTLV;
    else if (c && (CLIHas(c, "--color") || CLIHas(c, "--ansi")))
                                        HUNKMode = HUNKOutColor;
    else if (c && CLIHas(c, "--plain")) HUNKMode = HUNKOutPlain;
    else HUNKMode = ANSIIsTTY() ? HUNKOutColor : HUNKOutPlain;
}
