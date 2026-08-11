#!/bin/sh
# .github/static-be.sh — build the fully static `be` (musl + static JSCOnly +
# embedded slim ICU data + embedded beagle jsrc pack).  Runs inside an
# alpine container; the be checkout is the cwd.  JAB-035.
#
#   static-be.sh <jsc-build-dir> <out-binary>
#
# The jsc-build-dir persists across releases via actions/cache — the WebKit
# compile (~40 min cold) is skipped when the cached archives are present.
set -eu
JSCDIR=$1
OUT=$2
WEBKIT=2.52.5
SRC=$PWD

apk add -q build-base cmake samurai ruby perl python3 linux-headers curl git \
    xz file curl-dev icu icu-dev icu-static icu-data-full \
    libsodium-dev libsodium-static zlib-dev zlib-static lz4-dev lz4-static
gem install -q getoptlong

# --- 1. static JSCOnly (cached) ---------------------------------------------
if [ ! -f "$JSCDIR/lib/libJavaScriptCore.a" ]; then
    curl -sfLO "https://webkitgtk.org/releases/webkitgtk-$WEBKIT.tar.xz"
    tar xf "webkitgtk-$WEBKIT.tar.xz"
    cmake -S "webkitgtk-$WEBKIT" -B "$JSCDIR" -GNinja -DPORT=JSCOnly \
        -DCMAKE_BUILD_TYPE=Release -DENABLE_STATIC_JSC=ON -DUSE_THIN_ARCHIVES=OFF
    # JSC unified sources take 1-3 GB each to compile: cap jobs by cores AND
    # by available RAM (2.5 GB per job) — FTLLowerDFGToB3.cpp OOMs otherwise.
    J=$(nproc); [ "$J" -gt 4 ] && J=4
    M=$(awk '/MemAvailable/{print int($2/2500000)}' /proc/meminfo); [ "$M" -lt 1 ] && M=1
    [ "$M" -lt "$J" ] && J=$M
    ninja -C "$JSCDIR" -j"$J" jsc
fi

# --- 2. slim ICU data: drop locale bundles, keep root+en + supplemental -----
# Removing only true locale IDs (xx, xx_YY, xx_Xxxx_YY) keeps res_index,
# zoneinfo64, supplementalData etc — without those ICU SEGFAULTS at runtime.
mkdir -p icu-slim && cd icu-slim
cp /usr/share/icu/*/icudt*l.dat icudt.dat
V=$(ls /usr/share/icu/ | head -1 | cut -d. -f1)
mv icudt.dat "icudt${V}l.dat"
icupkg -l "icudt${V}l.dat" > items.txt
LOC='([a-z]{2,3})(_[A-Z][a-z]{3})?(_([A-Z]{2}|[0-9]{3}))?'
grep -E "^((curr|zone|region|lang|unit|coll|rbnf|translit|brkitr)/)?$LOC\.res\$" items.txt \
    | grep -vE '(^|/)(root|en|en_US|en_GB|en_001|pool)\.res$' > remove.txt
# legacy variant stragglers the pattern cannot see
for x in coll/de_.res coll/de__PHONEBOOK.res coll/es_.res coll/es__TRADITIONAL.res \
         curr/no_NO_NY.res ja_JP_TRADITIONAL.res lang/no_NO_NY.res no_NO_NY.res \
         region/no_NO_NY.res th_TH_TRADITIONAL.res unit/no_NO_NY.res zone/no_NO_NY.res; do
    grep -qxF "$x" items.txt && echo "$x" >> remove.txt
done
icupkg -r remove.txt "icudt${V}l.dat"
genccode -a gcc -e "icudt${V}" "icudt${V}l.dat"
gcc -c "icudt${V}l_dat.S" -o icudt_dat.o
ar rcs libicudata-slim.a icudt_dat.o
cd "$SRC"

# --- 3. jab, static, with the be pack ---------------------------------------
git clone -q --recurse-submodules --depth 1 https://github.com/gritzko/jab.git jab-src
mkdir -p be-jsrc
git archive HEAD | tar -x -C be-jsrc
cmake -S jab-src -B jab-build -GNinja -DCMAKE_BUILD_TYPE=Release \
    "-DJSC_INCLUDE_DIR=$JSCDIR/JavaScriptCore/Headers" \
    "-DJSC_LIB=$JSCDIR/lib/libJavaScriptCore.a;$JSCDIR/lib/libWTF.a;$JSCDIR/lib/libbmalloc.a;/usr/lib/libicui18n.a;/usr/lib/libicuuc.a;$SRC/icu-slim/libicudata-slim.a;/usr/lib/libatomic.a" \
    -Dsodium=/usr/lib/libsodium.a \
    -DZLIB_LIBRARY_RELEASE=/usr/lib/libz.a \
    -DCMAKE_EXE_LINKER_FLAGS=-static \
    "-DJAB_JSRC=$SRC/be-jsrc"
# the full target set links curl-using abc tests, impossible under -static:
# build the jab binary only.
ninja -C jab-build -j"$(nproc)" jab
strip jab-build/bin/jab
cp jab-build/bin/jab "$OUT"

# --- 4. smoke: static, floor extracts, Intl survives without host ICU -------
file "$OUT" | grep -q 'static' || { echo "FAIL: not static" >&2; exit 1; }
S=$(mktemp -d); W=$(mktemp -d)
( cd "$W" && HOME="$S" XDG_CACHE_HOME= ICU_DATA=/nonexistent "$OUT" status 2>&1 | head -3 ) || true
find "$S/.cache/jsrcs" -mindepth 1 -maxdepth 1 -type d | grep -q . \
    || { echo "FAIL: jsrc floor did not extract" >&2; exit 1; }
echo 'try { new Intl.DateTimeFormat("en",{month:"long"}).format(new Date(0)); console.log("intl ok") } catch (e) { console.log("intl FAIL " + e) }' > "$W/p.js"
ICU_DATA=/nonexistent "$OUT" "$W/p.js" | grep -q 'intl ok' \
    || { echo "FAIL: embedded ICU data broken" >&2; exit 1; }
echo "static-be: OK, $(du -h "$OUT" | cut -f1)"
