//  verbs/spot/match.js — the per-MODE matcher behind the shared search VIEW
//  (JAB-021).  `make(mode, body, ext)` returns a matcher with:
//     needsToks : YES iff the mode rides the tok cursor (spot only)
//     run(src, htoks) -> [{lo, hi}]   ascending per-match BYTE spans
//  so the scaffold (search.js) is mode-agnostic: one flag + one fn per mode.
//
//  grep  = literal substring over the raw source bytes (incl. comments).
//  regex = native JS RegExp over the decoded source text.
//  spot  = the structural flat-token matcher — a PURE-JS re-roll of
//          spot/SPOT.c's spot_match_flat + SPOTNext over a TokStream cursor.
//          lowercase a-z bind ONE token; uppercase A-Z bind a token BLOCK
//          (incl. balanced brackets); two spaces = a skip gap.  Faithful
//          dedup/coalesce is the scaffold's job (capo_spot_file).
"use strict";

const TokStream = tok.TokStream;

//  --- grep: literal substring (CAPO GREP.c capo_grep_file_cb) --------------
function grepMatcher(body) {
  const ndl = utf8.Encode(body);
  return {
    needsToks: false,
    run: function (src) {
      const out = [];
      const n = src.length, m = ndl.length;
      if (m === 0 || n < m) return out;
      for (let i = 0; i + m <= n; i++) {
        let k = 0;
        while (k < m && src[i + k] === ndl[k]) k++;
        if (k === m) out.push({ lo: i, hi: i + m });   // every occurrence
      }
      return out;
    }
  };
}

//  --- regex: native JS RegExp (replaces the Thompson NFA) ------------------
//  The C dog walks LINE BY LINE (NFAu8Search) and highlights the shortest
//  accepted sub-span (grep_match_span).  We mirror that: per source line, the
//  first RegExp match's [start, end) byte span (line-relative → absolute).
function regexMatcher(body) {
  //  C-NFA `{,m}` reads as `{0,m}` (NFAu8RCounted); JS treats `{,m}` as a
  //  literal.  Rewrite that one form so the dialect stays a JS-RegExp subset
  //  (JAB-023).
  const jsBody = body.replace(/(^|[^\\])\{,(\d+)\}/g, "$1{0,$2}");
  let re;
  try { re = new RegExp(jsBody); } catch (e) { return null; }
  return {
    needsToks: false,
    run: function (src) {
      const out = [];
      //  Walk source line by line (NFAu8Search is per-line); the first match's
      //  byte span per line (line-relative char offset → absolute byte offset).
      const lines = splitLinesBytes(src);
      for (const ln of lines) {
        const lineText = utf8.Decode(src.subarray(ln.lo, ln.hi));
        re.lastIndex = 0;
        const mm = re.exec(lineText);
        if (mm) {
          //  Map char offsets in the line to byte offsets (utf8-safe).
          const preBytes = utf8.Encode(lineText.slice(0, mm.index)).length;
          const matBytes = utf8.Encode(mm[0]).length;
          const lo = ln.lo + preBytes;
          out.push({ lo: lo, hi: lo + matBytes });
        }
      }
      return out;
    }
  };
}

//  Split source bytes into line ranges [lo, hi) (hi excludes the '\n').
function splitLinesBytes(src) {
  const out = [];
  let lo = 0;
  for (let i = 0; i < src.length; i++) {
    if (src[i] === 10) { out.push({ lo: lo, hi: i }); lo = i + 1; }
  }
  if (lo < src.length) out.push({ lo: lo, hi: src.length });
  return out;
}

//  --- spot: the structural flat-token matcher -----------------------------
//  A faithful JS re-roll of spot/SPOT.c.  Token tags come from the TokStream
//  cursor (tag/start/end); the needle is tokenized via tok.parse with the same
//  ext.  Placeholder rules (spot_is_placeholder / spot_bind_index):
//    a-z bind ONE non-punct leaf token; A-Z bind a balanced token BLOCK; a
//    needle whitespace token of >=2 spaces sets a skip gap before the next.
function isPlaceholder(s) {
  if (s.length !== 1) return false;
  const c = s.charCodeAt(0);
  return (c >= 97 && c <= 122) || (c >= 65 && c <= 90);   // a-z | A-Z
}
function bindIndex(c) {
  if (c >= 97 && c <= 122) return c - 97;        // a-z -> 0..25
  if (c >= 65 && c <= 90) return 26 + (c - 65);  // A-Z -> 26..51
  return -1;
}
function isLower(c) { return c >= 97 && c <= 122; }
function countSpaces(s) { let n = 0; for (let i = 0; i < s.length; i++) { if (s[i] === " ") n++; else break; } return n; }
function bracketDir(s) {
  if (s.length !== 1) return 0;
  const c = s[0];
  if (c === "{" || c === "(" || c === "[") return 1;
  if (c === "}" || c === ")" || c === "]") return -1;
  return 0;
}

//  Decode a tok32 array into a flat [{tag, lo, hi, text}] list over `src`.
function decodeToks(t32, src) {
  const out = [];
  const ts = new TokStream(t32, src);
  for (let i = 0; i < t32.length; i++) {
    ts.seek(i);
    out.push({ tag: ts.tag, lo: ts.start, hi: ts.end });
  }
  return out;
}
function tokText(toks, src, i) { return utf8.Decode(src.subarray(toks[i].lo, toks[i].hi)); }

//  Flatten the needle tokens into placeholders/literals + skip flags
//  (spot_flatten_needle, SPOT.c:112).
function flattenNeedle(ntoks, nsrc) {
  const flat = [];
  let pendingSkip = false;
  for (let i = 0; i < ntoks.length; i++) {
    const tag = ntoks[i].tag;
    const val = utf8.Decode(nsrc.subarray(ntoks[i].lo, ntoks[i].hi));
    if (tag === "D") continue;                  // comment
    if (tag === "W") { if (countSpaces(val) >= 2) pendingSkip = true; continue; }
    if (val.length === 0) continue;
    flat.push({ tag: tag, val: val, skip: flat.length > 0 ? pendingSkip : false });
    pendingSkip = false;
  }
  return flat;
}

//  Skip comment(D)/whitespace(W) haystack tokens from pos.
function skipWs(htoks, pos) {
  const n = htoks.length;
  while (pos < n) { const t = htoks[pos].tag; if (t === "D" || t === "W") { pos++; continue; } break; }
  return pos;
}

//  Next literal (non-placeholder) needle anchor after `from`, before a skip
//  boundary (spot_find_anchor, SPOT.c:170).
function findAnchor(flat, from) {
  for (let k = from + 1; k < flat.length; k++) {
    if (flat[k].skip) return -1;
    if (!isPlaceholder(flat[k].val)) return k;
  }
  return -1;
}

//  The recursive flat matcher.  `b` carries binds + the matched-token segment
//  list (subs).  Returns true on full match; mutates b.pos.  A faithful port
//  of spot_match_flat (SPOT.c:184-533).  src = the haystack bytes.
function matchFlat(b, flat, from, htoks, src, brace) {
  const hlen = htoks.length;
  if (from >= flat.length) return true;
  const cur = flat[from];

  function tokVal(pos) { return utf8.Decode(src.subarray(htoks[pos].lo, htoks[pos].hi)); }
  function leafLo(pos) { return pos > 0 ? htoks[pos - 1].hi : 0; }
  function leafHi(pos) { return htoks[pos].hi; }

  if (!cur.skip) {
    let pos = skipWs(htoks, b.pos);
    if (pos >= hlen) return false;
    const hv = tokVal(pos);
    const lLo = leafLo(pos), lHi = leafHi(pos);
    b.pos = pos + 1;

    if (from === 0) b.subs.push({ lo: lLo, hi: lHi });
    if (b.subs.length > 0) b.subs[b.subs.length - 1].hi = lHi;

    if (isPlaceholder(cur.val)) {
      const c = cur.val.charCodeAt(0);
      const idx = bindIndex(c);
      if (idx < 0) return false;
      const bit = 1n << BigInt(idx);

      if (isLower(c)) {
        if (htoks[pos].tag === "P") return false;       // punct can't bind a-z
        const capHi = lHi;
        if (b.subs.length > 0) b.subs[b.subs.length - 1].hi = capHi;
        if (b.bound & bit) {
          const bm = b.binds[idx];
          if (!byteEq(src, bm.lo, bm.hi, lLo, capHi)) return false;
        } else {
          b.bound |= bit;
          b.binds[idx] = { lo: lLo, hi: capHi };
        }
      } else {
        //  Uppercase: token-block bind.
        if (b.bound & bit) {
          const bm = b.binds[idx];
          const boundLen = bm.hi - bm.lo;
          const capEnd = lLo + boundLen;
          let consumed = lHi;
          while (consumed < capEnd) {
            const p2 = skipWs(htoks, b.pos);
            if (p2 >= hlen) return false;
            b.pos = p2 + 1;
            consumed = htoks[p2].hi;
          }
          if (consumed !== capEnd) return false;
          if (!byteEq(src, bm.lo, bm.hi, lLo, capEnd)) return false;
          if (b.subs.length > 0) b.subs[b.subs.length - 1].hi = consumed;
        } else {
          if (bracketDir(hv) < 0) return false;          // can't start on close
          const anchorIdx = findAnchor(flat, from);
          const gap = anchorIdx >= 0 ? anchorIdx - from - 1 : -1;
          let capHi = lHi;
          b.bound |= bit;
          b.binds[idx] = { lo: lLo, hi: capHi };
          let capBrace = bracketDir(hv);

          if (gap < 0) {
            if (b.subs.length > 0) b.subs[b.subs.length - 1].hi = capHi;
            const sv = save(b);
            if (matchFlat(b, flat, from + 1, htoks, src, brace)) return true;
            restore(b, sv);
            return false;
          }

          //  Anchor-guided lookahead window of gap+1 positions.
          let ahead = [], laScan = b.pos;
          for (let g = 0; g <= gap; g++) {
            const p = skipWs(htoks, laScan);
            if (p >= hlen) break;
            ahead.push(p); laScan = p + 1;
          }
          let atBoundary = false;
          for (;;) {
            if (ahead.length > gap) {
              const av = tokVal(ahead[gap]);
              if (capBrace === 0 && av === flat[anchorIdx].val) {
                if (b.subs.length > 0) b.subs[b.subs.length - 1].hi = capHi;
                b.binds[idx].hi = capHi;
                const sv = save(b);
                b.pos = ahead[0];
                if (matchFlat(b, flat, from + 1, htoks, src, brace)) return true;
                restore(b, sv);
              }
            }
            if (atBoundary) return false;
            if (ahead.length === 0) return false;
            const cv = tokVal(ahead[0]);
            capBrace += bracketDir(cv);
            if (capBrace < 0) return false;
            if (brace + capBrace === 0) {
              if (bracketDir(cv) < 0) atBoundary = true;
              if (cv.length === 1 && cv === ";") atBoundary = true;
            }
            capHi = htoks[ahead[0]].hi;
            b.pos = ahead[0] + 1;
            ahead.shift();
            const p = skipWs(htoks, laScan);
            if (p < hlen) { ahead.push(p); laScan = p + 1; }
          }
        }
      }
    } else {
      //  Literal: value must match.
      if (cur.val !== hv) return false;
      brace += bracketDir(cur.val);
    }
    return matchFlat(b, flat, from + 1, htoks, src, brace);
  }

  //  SKIP: scan forward, try each position with backtracking.
  {
    const savedBinds = save(b);
    let scanBrace = 0;
    let isClosePh = false, ndlBdir = 0;
    if (!isPlaceholder(cur.val)) { ndlBdir = bracketDir(cur.val); isClosePh = ndlBdir < 0; }
    for (;;) {
      let pos = skipWs(htoks, b.pos);
      if (pos >= hlen) return false;
      const hv = tokVal(pos);
      const lLo = leafLo(pos), lHi = leafHi(pos);
      b.pos = pos + 1;
      const bd = bracketDir(hv);
      scanBrace += bd;
      if (isClosePh) { if (scanBrace < -brace) return false; }
      else { if (scanBrace < 0) return false; }
      const postPos = b.pos;

      b.subs.push({ lo: lLo, hi: lHi });

      const isPh = isPlaceholder(cur.val);
      const phC = isPh ? cur.val.charCodeAt(0) : 0;
      const phIdx = isPh ? bindIndex(phC) : -1;
      const isUpper = isPh && !isLower(phC);

      if (isUpper && phIdx >= 0) {
        const bit = 1n << BigInt(phIdx);
        if (b.bound & bit) { restore(b, savedBinds); continue; }
        if (bracketDir(hv) < 0) { restore(b, savedBinds); continue; }
        b.bound |= bit;
        b.binds[phIdx] = { lo: lLo, hi: lHi };
        const anchorIdx = findAnchor(flat, from);
        const gap = anchorIdx >= 0 ? anchorIdx - from - 1 : -1;
        let capHi = lHi;
        let capBrace2 = bracketDir(hv);

        if (gap < 0) {
          if (b.subs.length > 0) b.subs[b.subs.length - 1].hi = capHi;
          const sv = save(b);
          if (matchFlat(b, flat, from + 1, htoks, src, brace + scanBrace)) return true;
          restore(b, sv);
        } else {
          let ahead = [], laScan = b.pos;
          for (let g = 0; g <= gap; g++) {
            const p = skipWs(htoks, laScan);
            if (p >= hlen) break;
            ahead.push(p); laScan = p + 1;
          }
          let matched = false, atBoundary2 = false;
          for (;;) {
            if (ahead.length > gap) {
              const av = tokVal(ahead[gap]);
              if (capBrace2 === 0 && av === flat[anchorIdx].val) {
                if (b.subs.length > 0) b.subs[b.subs.length - 1].hi = capHi;
                b.binds[phIdx].hi = capHi;
                const sv = save(b);
                b.pos = ahead[0];
                if (matchFlat(b, flat, from + 1, htoks, src, brace + scanBrace)) { matched = true; break; }
                restore(b, sv);
              }
            }
            if (atBoundary2) break;
            if (ahead.length === 0) break;
            const cv = tokVal(ahead[0]);
            capBrace2 += bracketDir(cv);
            if (capBrace2 < 0) break;
            if (brace + scanBrace + capBrace2 === 0) {
              if (bracketDir(cv) < 0) atBoundary2 = true;
              if (cv.length === 1 && cv === ";") atBoundary2 = true;
            }
            capHi = htoks[ahead[0]].hi;
            b.pos = ahead[0] + 1;
            ahead.shift();
            const p = skipWs(htoks, laScan);
            if (p < hlen) { ahead.push(p); laScan = p + 1; }
          }
          if (matched) return true;
        }
        b.pos = postPos;
        restore(b, savedBinds);
        continue;
      }

      let tokMatch = false;
      if (isPh && phIdx >= 0 && isLower(phC)) {
        const bit = 1n << BigInt(phIdx);
        if (htoks[pos].tag === "P") { restore(b, savedBinds); continue; }
        const capHi = lHi;
        if (b.subs.length > 0) b.subs[b.subs.length - 1].hi = capHi;
        if (b.bound & bit) {
          const bm = b.binds[phIdx];
          tokMatch = byteEq(src, bm.lo, bm.hi, lLo, capHi);
        } else {
          b.bound |= bit;
          b.binds[phIdx] = { lo: lLo, hi: capHi };
          tokMatch = true;
        }
      } else if (!isPh) {
        tokMatch = (cur.val === hv);
        if (tokMatch && isClosePh && scanBrace !== -brace) tokMatch = false;
      }

      if (tokMatch) {
        const mr = save(b);
        if (matchFlat(b, flat, from + 1, htoks, src, brace + scanBrace)) return true;
        b.pos = postPos;
        restore(b, mr);
      }
      restore(b, savedBinds);
    }
  }
}

//  byte-equal two src ranges (u8csEq).
function byteEq(src, aLo, aHi, bLo, bHi) {
  if (aHi - aLo !== bHi - bLo) return false;
  for (let i = 0; i < aHi - aLo; i++) if (src[aLo + i] !== src[bLo + i]) return false;
  return true;
}

//  Snapshot / restore the backtracking state (SPOTsave).
function save(b) {
  return { pos: b.pos, bound: b.bound,
           binds: b.binds.map(function (x) { return x ? { lo: x.lo, hi: x.hi } : null; }),
           nsubs: b.subs.length,
           subs: b.subs.map(function (s) { return { lo: s.lo, hi: s.hi }; }) };
}
function restore(b, sv) {
  b.pos = sv.pos; b.bound = sv.bound;
  b.binds = sv.binds.map(function (x) { return x ? { lo: x.lo, hi: x.hi } : null; });
  b.subs = sv.subs.map(function (s) { return { lo: s.lo, hi: s.hi }; });
}

//  spotMatcher: SPOTInit/SPOTNext over the cursor — emit each match's src_rng
//  (the [first-sub.lo, last-sub.hi) byte span).
function spotMatcher(body, ext) {
  return {
    needsToks: true,
    run: function (src, htoks) {
      if (!htoks || htoks.length === 0) return [];
      const lang = ext.replace(/^\./, "");
      let n32;
      try { n32 = tok.parse(utf8.Encode(body), lang); } catch (e) { return []; }
      const nsrc = utf8.Encode(body);
      const ntoks = decodeToks(n32, nsrc);
      const flat = flattenNeedle(ntoks, nsrc);
      if (flat.length === 0) return [];

      const hToks = decodeToks(htoks, src);
      const hlen = hToks.length;
      const out = [];
      let hpos = 0;
      for (;;) {
        const b = { pos: hpos, bound: 0n, binds: new Array(52).fill(null), subs: [] };
        if (matchFlat(b, flat, 0, hToks, src, 0)) {
          if (b.subs.length > 0) {
            const lo = b.subs[0].lo, hi = b.subs[b.subs.length - 1].hi;
            if (hi > lo && hi <= src.length) out.push({ lo: lo, hi: hi });
          }
          //  Advance the main cursor by one meaningful token (SPOTNext).
          const adv = skipWs(hToks, hpos);
          if (adv < hlen) hpos = adv + 1; else break;
        } else {
          const pos = skipWs(hToks, hpos);
          if (pos >= hlen) break;
          hpos = pos + 1;
        }
      }
      //  Dedup wider-then-narrower duplicate spans the engine may report
      //  (capo_spot_file keeps the narrowest); the scaffold also coalesces,
      //  but ascending uniqueness keeps the span list clean.
      return dedupSpans(out);
    }
  };
}

//  Keep spans ascending by lo; when a later span is contained in the prior
//  (same/inner), keep the narrower — mirrors capo_spot_file's contain-replace.
function dedupSpans(spans) {
  const out = [];
  for (const s of spans) {
    const prev = out[out.length - 1];
    if (prev && s.lo >= prev.lo && s.hi <= prev.hi) { out[out.length - 1] = s; continue; }
    if (prev && s.lo === prev.lo && s.hi === prev.hi) continue;
    out.push(s);
  }
  return out;
}

//  --- mode factory ---------------------------------------------------------
function make(mode, body, ext) {
  if (mode === "grep")  return grepMatcher(body);
  if (mode === "regex") return regexMatcher(body);
  if (mode === "spot")  return spotMatcher(body, ext);
  return null;
}

module.exports = { make: make };
