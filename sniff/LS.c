//  LS — `ls:` projector.  See LS.h.
//
//  Mirrors bare-`be`'s status pass: SNIFFClassify fans one step per
//  distinct path; each step is lifted into a HUNK_TLV record carrying
//  the status verb (V), ts (T), navigation URI (U), and display name
//  (X).  Always emits TLV — dog/HUNK picks plain/color/TLV via the
//  global HUNKMode.

#include "LS.h"

#include <string.h>

#include "abc/B.h"
#include "abc/BUF.h"
#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PRO.h"

#include "dog/HUNK.h"
#include "dog/ULOG.h"

#include "AT.h"
#include "CLASS.h"
#include "SNIFF.h"

// --- Local verb cache --------------------------------------------------
//
//  `status_verbs` is private to SNIFF.exe.c (and rightly so — it's tied
//  to its own status_buckets layout).  We cache the same nine ron60s
//  locally; the verb literals are stable so there's no risk of drift.

typedef struct {
    ron60 v_put, v_new, v_mov, v_mod, v_del, v_mis, v_unk, v_eq;
} ls_verbs;

// =====================================================================
//  Per-row hunk emit
// =====================================================================

typedef struct {
    u8cs    prefix;            // listing scope ("<path>/" or empty)
    ls_verbs v;                // put/new/mov/mod/del/mis/unk/eq ron60s
} ls_ctx;

//  Push one status hunk — bare-`be` shape via `ULOGPrintStatusLine`:
//  empty text, ts/verb set, URI = path.  Bare-path URIs let bro
//  click-handling stay in-process via `BROOpenFile` (repo-aware via
//  `HOMEOpen`); routing through a `cat:` scheme would fork sub-be
//  and re-enter the `--at` chain, which has a colocated-wt cwd bug
//  on non-root subdirs.  Mov rows carry `mov_dst` in `.fragment`.
static void ls_push(ls_ctx *c, u8cs path, u8cs mov_dst, ron60 ts,
                    ron60 verb) {
    (void)c;
    uri u = {};
    u.path[0] = path[0];
    u.path[1] = path[1];
    if (!u8csEmpty(mov_dst)) {
        u.fragment[0] = mov_dst[0];
        u.fragment[1] = mov_dst[1];
    }
    ulogrec rec = {.ts = ts, .verb = verb, .uri = u};
    (void)ULOGPrintStatusLine(&rec);
}

static b8 ls_path_in_scope(u8cs path, u8cs prefix) {
    if (u8csEmpty(prefix)) return YES;
    if ((size_t)$len(path) < (size_t)$len(prefix)) return NO;
    return memcmp(path[0], prefix[0], (size_t)$len(prefix)) == 0;
}

//  CLASS callback — mirrors sniff/SNIFF.exe.c::status_step, but emits
//  one HUNK per row instead of staging into a row buffer.  The bucket
//  decisions are identical so the output set matches bare `be` exactly.
static ok64 ls_step(class_step const *step, void *ctx_) {
    ls_ctx *c = (ls_ctx *)ctx_;
    u8cs path = {step->path[0], step->path[1]};
    if (!ls_path_in_scope(path, c->prefix)) return OK;

    u8cs empty = {NULL, NULL};

    //  Staged groups take precedence — same priority bare-be uses.
    if (step->del_rec != NULL) {
        ls_push(c, path, empty, step->del_rec->ts, c->v.v_del);
        return OK;
    }
    if (step->put_rec != NULL) {
        u8cs frag = {step->put_rec->uri.fragment[0],
                     step->put_rec->uri.fragment[1]};
        ron60 ts = step->put_rec->ts;
        b8 is_bump = (u8csLen(frag) == 40 && HEXu8sValid(frag));
        if (!u8csEmpty(frag) && !is_bump) {
            ls_push(c, path, frag, ts, c->v.v_mov);
            return OK;
        }
        ron60 verb = (step->kind == CLASS_BOTH ||
                      step->kind == CLASS_BASE_ONLY)
                     ? c->v.v_put : c->v.v_new;
        ls_push(c, path, empty, ts, verb);
        return OK;
    }

    switch (step->kind) {
        case CLASS_WT_ONLY:
            //  PATCH-stamped path → surface as new; raw untracked → unk.
            if (step->wt_rec && SNIFFAtKnown(step->wt_rec->ts))
                ls_push(c, path, empty, step->wt_rec->ts, c->v.v_new);
            else
                ls_push(c, path, empty,
                        step->wt_rec ? step->wt_rec->ts : 0,
                        c->v.v_unk);
            break;
        case CLASS_BASE_ONLY:
            ls_push(c, path, empty, 0, c->v.v_mis);
            break;
        case CLASS_BOTH:
            //  mtime fast-path: stamped → unchanged (`eq`); otherwise
            //  edited since the last tracked op (`mod`).  Both get a row.
            if (step->wt_rec && SNIFFAtKnown(step->wt_rec->ts))
                ls_push(c, path, empty, step->wt_rec->ts, c->v.v_eq);
            else
                ls_push(c, path, empty, step->wt_rec->ts, c->v.v_mod);
            break;
    }
    return OK;
}

// =====================================================================
//  Entry point
// =====================================================================

ok64 SNIFFLs(u8cs reporoot, uri const *u) {
    sane(u);
    (void)reporoot;

    ls_ctx c = {};
    #define LSV(field, lit) do {                       \
        a_cstr(_s, lit); a_dup(u8c, _d, _s);           \
        c.v.field = SNIFFAtVerbOf(_d);                  \
    } while (0)
    LSV(v_put, "put");
    LSV(v_new, "new");
    LSV(v_mov, "mov");
    LSV(v_mod, "mod");
    LSV(v_del, "del");
    LSV(v_mis, "mis");
    LSV(v_unk, "unk");
    LSV(v_eq,  "eq");
    #undef LSV

    u8csMv(c.prefix, u->path);

    //  TODO: `?ref` baseline override.  CLASS today resolves baseline
    //  via SNIFFAtCurTip; for `ls:?ref` we'd want SNIFFClassifyAt
    //  (sha1cp base_tree, class_cb, void *ctx).  Default-baseline path
    //  works today and matches bare `be` output verbatim.
    return SNIFFClassify(ls_step, &c);
}
