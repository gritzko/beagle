#!/bin/sh
#  diff/13-syntax-hili — a `diff:<file>` view must highlight SYNTAX, not
#  just the change.  The weave keeps no lexer tags (graf/WEAVE.h), so the
#  emit path (WEAVEEmitFull/Diff) re-tokenizes the assembled hunk text
#  for syntax and overlays it onto the per-token diff side — the same
#  thing blame does (graf/BLAME.c).  Regression guard for the period when
#  every diff token was emitted with the neutral 'S' tag, so the body
#  rendered flat (change-background only, no keyword/comment/string
#  colour) in BOTH color modes.
#
#  Theme-independent assertion: identical content + identical edit,
#  diffed once as a KNOWN extension (`.c` → a lexer exists → syntax
#  tags) and once as an UNKNOWN extension (`.zzz` → no lexer → neutral).
#  The `.c` colour diff must carry strictly MORE SGR escapes than the
#  `.zzz` one (the extra escapes ARE the syntax highlighting); the change
#  itself still highlights in both.

. "$(dirname "$0")/../../lib/case.sh"

#  A file with unmistakable C tokens (keywords, a string, a comment).
write_body() {
    cat > "$1" <<'EOF'
#include <stdio.h>
//  a comment that the C lexer tags distinctly
static int answer = 42;
int main(void) {
    char *greeting = "hello world";
    return answer;
}
EOF
}

count_sgr() {  # count_sgr FILE -> number of ESC[ sequences
    #  Each ANSI SGR starts with ESC '['; count them.
    LC_ALL=C tr -cd '\033' < "$1" | wc -c | tr -d ' '
}

#  --- baseline: same body under two extensions ----------------------
write_body code.c
write_body code.zzz
"$BE" put  code.c code.zzz   >/dev/null
"$BE" post -m base           >/dev/null

#  --- edit the same line in both ------------------------------------
sed 's/answer = 42/answer = 99/' code.c   > code.c.new   && mv code.c.new   code.c
sed 's/answer = 42/answer = 99/' code.zzz > code.zzz.new && mv code.zzz.new code.zzz

#  --- file-scope colour diffs ---------------------------------------
"$BE" 'diff:code.c#L3'   --ansi >c.color.out   2>c.color.err
"$BE" 'diff:code.zzz#L3' --ansi >z.color.out   2>z.color.err
empty c.color.err
empty z.color.err

#  Both must still show the change.  Token-level inline diffs interleave
#  ANSI escapes within the line, so strip first, then match the new value
#  (`99`) — present whether the renderer shows it inline or on a `+` row.
sed -E 's/\x1b\[[0-9;]*[a-zA-Z]//g' c.color.out >c.color.stripped
if ! grep -qF '99' c.color.stripped; then
    echo "FAIL: .c diff missing the changed value" >&2
    cat c.color.stripped >&2; exit 1
fi

C_SGR=$(count_sgr c.color.out)
Z_SGR=$(count_sgr z.color.out)

#  The known-ext (.c) diff must carry MORE colour than the unknown-ext
#  (.zzz) one — that surplus is the syntax highlighting the overlay adds.
if [ "$C_SGR" -le "$Z_SGR" ]; then
    echo "FAIL: .c diff has no extra syntax colour (.c SGR=$C_SGR, .zzz SGR=$Z_SGR)" >&2
    echo "--- .c color (stripped) ---" >&2
    sed -E 's/\x1b\[[0-9;]*[a-zA-Z]//g' c.color.out >&2
    exit 1
fi

#  Sanity: the unknown ext still produced *some* output (the change).
if [ "$Z_SGR" -eq 0 ]; then
    echo "FAIL: .zzz diff produced no SGR at all (expected change banner/hili)" >&2
    exit 1
fi

echo "OK 13-syntax-hili (.c SGR=$C_SGR > .zzz SGR=$Z_SGR)"
