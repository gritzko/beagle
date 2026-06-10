//  RECV: git receive-pack server (push direction).
//
//  See RECV.h for the contract and WIRE.md Phase 6 for the surrounding
//  plan.

#include "RECV.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "dog/DOG.h"
#include "dog/git/GIT.h"
#include "dog/git/PKT.h"
#include "keeper/REFS.h"
#include "keeper/WIRE.h"

// --- capability tokens (recognised on the first ref-update line) ---

static u8c const RECV_CAP_REPORT_STATUS_S[] = "report-status";
static u8c const RECV_CAP_SIDE_BAND_64K_S[] = "side-band-64k";
static u8c const RECV_CAP_OFS_DELTA_S[]     = "ofs-delta";
static u8c const RECV_CAP_QUIET_S[]         = "quiet";
static u8c const RECV_CAP_AGENT_PFX[]       = "agent=";

// --- small helpers ---

//  Token equality.
static b8 recv_token_eq(u8csc s, u8c const *tok, size_t tlen) {
    u8cs t = {(u8c *)tok, (u8c *)tok + tlen};
    return u8csEq((u8c **)s, t);
}

static b8 recv_starts_with(u8csc s, u8c const *pfx, size_t plen) {
    u8cs p = {(u8c *)pfx, (u8c *)pfx + plen};
    return u8csHasPrefix((u8c **)s, p);
}

//  Parse a space-separated capability list off the tail of the first
//  ref-update line.  Sets bits in *caps; unknown tokens are dropped.
static void recv_parse_caps(u32 *caps, u8csc tail) {
    u8cs scan = {tail[0], tail[1]};
    while (!u8csEmpty(scan)) {
        while (!u8csEmpty(scan) && (scan[0][0] == ' ' || scan[0][0] == '\t'))
            scan[0]++;
        if (u8csEmpty(scan)) break;
        u8c *tok_start = scan[0];
        while (!u8csEmpty(scan) &&
               scan[0][0] != ' ' && scan[0][0] != '\t' && scan[0][0] != '\n')
            scan[0]++;
        u8csc tok = {tok_start, scan[0]};
        if (u8csLen(tok) == 0) continue;
        if (recv_token_eq(tok, RECV_CAP_REPORT_STATUS_S,
                          sizeof(RECV_CAP_REPORT_STATUS_S) - 1)) {
            *caps |= RECV_CAP_REPORT_STATUS;
        } else if (recv_token_eq(tok, RECV_CAP_SIDE_BAND_64K_S,
                                 sizeof(RECV_CAP_SIDE_BAND_64K_S) - 1)) {
            *caps |= RECV_CAP_SIDE_BAND_64K;
        } else if (recv_token_eq(tok, RECV_CAP_OFS_DELTA_S,
                                 sizeof(RECV_CAP_OFS_DELTA_S) - 1)) {
            *caps |= RECV_CAP_OFS_DELTA;
        } else if (recv_token_eq(tok, RECV_CAP_QUIET_S,
                                 sizeof(RECV_CAP_QUIET_S) - 1)) {
            *caps |= RECV_CAP_QUIET;
        } else if (recv_starts_with(tok, RECV_CAP_AGENT_PFX,
                                    sizeof(RECV_CAP_AGENT_PFX) - 1)) {
            *caps |= RECV_CAP_AGENT;
        }
        if (!u8csEmpty(scan) && scan[0][0] == '\n') scan[0]++;
    }
}

//  Drain one pkt-line from buf, refilling via FILEDrain on NODATA.
//  Returns OK / PKTFLUSH / PKTDELIM / RECVFAIL on EOF/read error.
//  On refill, leftover bytes already in buf are preserved by feeding
//  past the current write head; `adv[1]` is reset to point at the new
//  buffer tail after each drain.
static ok64 recv_read_pkt(int in_fd, u8b buf, u8cs adv, u8csp line) {
    for (;;) {
        ok64 o = PKTu8sDrain(adv, line);
        if (o != NODATA) return o;
        if (!u8bHasRoom(buf)) return RECVFAIL;
        u8s fill;
        u8sFork(u8bIdle(buf), fill);
        ok64 fr = FILEDrain(in_fd, fill);
        if (fr == FILEEND) return RECVFAIL;
        if (fr != OK) return RECVFAIL;
        u8sJoin(u8bIdle(buf), fill);
        adv[1] = u8csTerm(u8bDataC(buf));
    }
}

// --- request reader ---

//  Caller (RECVServe) pre-acquires req->upds_b / req->arena / req->tail
//  from BASS so their pointers survive across the call() boundary into
//  this fn.  Here we just fill them and zero state.
ok64 RECVReadRequest(int in_fd, recv_reqp req) {
    sane(in_fd >= 0 && req);
    req->upds = (recv_update *)u8bDataHead(req->upds_b);
    req->count = 0;
    req->caps  = 0;

    a_carve(u8, buf, RECV_REQ_BUF);

    u8cs adv = {u8bDataHead(buf), u8bDataHead(buf)};
    ok64 rc = OK;

    for (;;) {
        u8cs line = {};
        ok64 d = recv_read_pkt(in_fd, buf, adv, line);
        if (d == PKTFLUSH) break;
        if (d == PKTDELIM) continue;
        if (d != OK) { rc = d; break; }

        //  Trim trailing '\n' if present.
        if (u8csLen(line) > 0 && line[1][-1] == '\n') line[1]--;

        wire_evt ev = {};
        if (WIREClassify(line, WIRE_RECEIVE, &ev) != OK ||
            ev.kind != WIRE_UPDATE) {
            rc = RECVBADREQ;
            break;
        }

        if (req->count >= RECV_MAX_UPDATES) {
            rc = RECVBADREQ;
            break;
        }
        recv_update *u = &req->upds[req->count];
        u->old_sha = ev.old_sha;
        u->new_sha = ev.sha;

        //  Copy refname into the request arena so the slice outlives `buf`.
        //  PATHu8bAren keeps a NUL byte after the name in PAST.
        u8csc refname = {ev.name[0], ev.name[1]};
        if (PATHu8bAren(req->arena, u->refname, refname) != OK) {
            rc = RECVFAIL;
            break;
        }

        //  Capabilities ride after a NUL on the first line only.
        if (req->count == 0 && !u8csEmpty(ev.caps))
            recv_parse_caps(&req->caps, (u8csc){ev.caps[0], ev.caps[1]});

        req->count++;
    }

    //  Preserve any bytes past `adv[0]` — they are the first bytes of
    //  the packfile the client streamed right after the flush.  Goes
    //  into req->tail (pre-acquired by caller); KEEPIngestFile reads
    //  it via {u8bDataHead(req.tail), u8bIdleHead(req.tail)}.
    if (rc == OK) {
        u8cs leftover = {adv[0], u8bIdleHead(buf)};
        if (!u8csEmpty(leftover)) u8bFeed(req->tail, leftover);
    }
    return rc;
}

//  RECVServe owns the buffer lifetime via BASS — this is now a
//  no-op kept for the API contract.  Callers that want the buffers
//  freed return from the procedure that acquired them.
void RECVCloseRequest(recv_reqp req) {
    if (!req) return;
    req->count = 0;
    req->caps  = 0;
}

// --- pack ingest ---

#define RECV_PACK_BUF (1u << 20)   // 1 MiB chunked drain

ok64 RECVIngestPack(int in_fd, u8csc tail) {
    sane(in_fd >= 0);
    keeper *k = &KEEP;

    Bu8 buf = {};
    call(u8bMap, buf, 1ULL << 30);  // up to 1 GiB packfile

    //  Consume any pre-buffered pack bytes first (see RECVReadRequest).
    if (!u8csEmpty(tail)) {
        if (u8bIdleLen(buf) < (size_t)u8csLen(tail)) {
            u8bUnMap(buf);
            return RECVFAIL;
        }
        u8bFeed(buf, tail);
    }

    //  Drain stdin to EOF.  Each iteration forks the buffer's idle
    //  slice, reads into it, joins back so DATA grows.  Per abc/Sx.h
    //  §sJoin, the fork+join contract requires `fill[1] == idle[1]`
    //  (sIs bounds check); a cap that rewrites `fill[1]` silently
    //  drops the just-read bytes because the join then fails its
    //  bounds check and the data border never advances.  read(2)
    //  already caps each call to whatever the kernel hands back —
    //  no need for our own ceiling here.
    for (;;) {
        if (!u8bHasRoom(buf)) {
            //  Pack larger than our cap — bail rather than truncate.
            u8bUnMap(buf);
            return RECVFAIL;
        }
        u8s fill;
        u8sFork(u8bIdle(buf), fill);
        ok64 fr = FILEDrain(in_fd, fill);
        if (fr == FILEEND) break;
        if (fr != OK) {
            u8bUnMap(buf);
            return RECVFAIL;
        }
        u8sJoin(u8bIdle(buf), fill);
    }

    u64 nbytes = u8bDataLen(buf);
    if (nbytes == 0) {
        //  Empty pack stream — delete-only updates etc.  Nothing to do.
        u8bUnMap(buf);
        done;
    }
    if (nbytes < 12) {
        u8bUnMap(buf);
        return RECVFAIL;
    }

    a_dup(u8c, bytes, u8bData(buf));
    ok64 io = KEEPIngestFile(bytes);
    u8bUnMap(buf);
    if (io != OK) return RECVFAIL;
    done;
}

// --- updates application ---

//  Build the local REFS key for a wire-side refname.  Maps:
//    refs/heads/main → `?`           (trunk; the only wire alias)
//    refs/heads/<X>  → `?<X>`        (literal local branch path)
//  Tags / remotes / OTHER kinds are not first-class locally yet —
//  reject with RECVBADREF.
static ok64 recv_build_key(u8b out, u8csc refname) {
    sane(u8bOK(out));
    gitref_kind k = GITREF_NONE;
    u8cs name = {};
    if (GITParseRef(refname, &k, name) != OK) return RECVBADREF;
    if (k != GITREF_BRANCH) return RECVBADREF;
    u8bFeed1(out, '?');
    if (u8csEq(name, GIT_MAIN_LIT)) done;  //  trunk: bare `?`
    u8bFeed(out, name);
    done;
}

//  Compose the to-URI value: bare 40-hex (canonical fragment form).
static ok64 recv_build_val(u8b out, sha1cp sha) {
    sane(u8bOK(out));
    call(SHA1u8sFeedHex, u8bIdle(out), sha);
    done;
}

//  Look up the current local tip for `refname` via the REFADV map.
//  Sets *have_tip = YES if found, NO otherwise.
static void recv_lookup_tip(refadvcp adv, u8csc refname,
                            sha1 *out_tip, b8 *have_tip) {
    *have_tip = NO;
    if (!adv) return;
    for (u32 i = 0; i < adv->count; i++) {
        if (!u8csEq(adv->ents[i].refname, (u8c **)refname)) continue;
        *out_tip   = adv->ents[i].tip;
        *have_tip  = YES;
        return;
    }
}

ok64 RECVApplyUpdates(refadvcp adv, recv_reqcp req,
                      recv_resultp out_results, u32 cap, u32p out_n) {
    sane(req && out_results && out_n);
    keeper *k = &KEEP;
    *out_n = 0;
    if (req->count > cap) return RECVFAIL;

    a_path(keepdir);
    call(HOMEBranchDir, k->h, keepdir, NULL);

    for (u32 i = 0; i < req->count; i++) {
        recv_update const *u = &req->upds[i];
        recv_result *r = &out_results[i];
        r->refname[0] = u->refname[0];
        r->refname[1] = u->refname[1];
        r->result = OK;

        b8 is_create = sha1empty(&u->old_sha);
        b8 is_delete = sha1empty(&u->new_sha);

        //  Phase 6 MVP: ref deletion (new_sha all-zeros) is not yet
        //  wired through REFS.  Refuse loudly with RECVBADREF rather
        //  than falling through to write a zero-sha tombstone — which
        //  also silently bypassed the fast-forward gate.
        if (is_delete) { r->result = RECVBADREF; continue; }

        //  Current local tip for this ref (captured pre-ingest in the
        //  advert).  Both the create-guard and the FF check need it.
        sha1 cur = {};
        b8 have_tip = NO;
        recv_lookup_tip(adv, u->refname, &cur, &have_tip);

        if (is_create) {
            //  A create (old_sha all-zeros) must name a ref that does
            //  not already exist.  An existing tip means this is an
            //  unguarded overwrite masquerading as a create — refuse
            //  as non-fast-forward.
            if (have_tip) { r->result = RECVNOTFF; continue; }
        } else {
            //  FF check: old_sha must equal the current tip.
            if (!have_tip || !sha1Eq(&cur, &u->old_sha)) {
                r->result = RECVNOTFF;
                continue;
            }
        }

        //  Build REFS key + val for this (create or fast-forward)
        //  update; new_sha is guaranteed non-zero here.
        a_pad(u8, kbuf, 512);
        ok64 ko = recv_build_key(kbuf, u->refname);
        if (ko != OK) { r->result = ko; continue; }
        a_pad(u8, vbuf, 64);
        ok64 vo = recv_build_val(vbuf, &u->new_sha);
        if (vo != OK) { r->result = RECVFAIL; continue; }

        a_dup(u8c, key, u8bData(kbuf));
        a_dup(u8c, val, u8bData(vbuf));
        ok64 ao = REFSAppend($path(keepdir), key, val);
        if (ao != OK) { r->result = RECVFAIL; continue; }
    }

    *out_n = req->count;
    done;
}

// --- response emit ---

ok64 RECVEmitResponse(int out_fd, ok64 unpack_status,
                      recv_resultcp results, u32 n) {
    sane(out_fd >= 0);

    u64 cap = 64;
    cap += 4 + 32;   // unpack line slack
    for (u32 i = 0; i < n; i++) {
        cap += 4 + 4 + (u64)u8csLen(results[i].refname) + 64;
    }

    a_carve(u8, frame, cap);

    //  unpack line.
    {
        a_pad(u8, line, 256);
        a_cstr(unpack_pfx, "unpack ");
        u8bFeed(line, unpack_pfx);
        if (unpack_status == OK) {
            a_cstr(ok_s, "ok");
            u8bFeed(line, ok_s);
        } else {
            a_cstr(err_s, "error");
            u8bFeed(line, err_s);
        }
        u8bFeed1(line, '\n');
        a_dup(u8c, payload, u8bData(line));
        call(PKTu8sFeed, u8bIdle(frame), payload);
    }

    //  Per-update lines.
    for (u32 i = 0; i < n; i++) {
        a_pad(u8, line, 512);
        if (results[i].result == OK && unpack_status == OK) {
            a_cstr(ok_pfx, "ok ");
            u8bFeed(line, ok_pfx);
            u8bFeed(line, results[i].refname);
            u8bFeed1(line, '\n');
        } else {
            a_cstr(ng_pfx, "ng ");
            u8bFeed(line, ng_pfx);
            u8bFeed(line, results[i].refname);
            u8bFeed1(line, ' ');
            char const *reason = "failed";
            if (unpack_status != OK) {
                reason = "unpacker failed";
            } else if (results[i].result == RECVNOTFF) {
                reason = "non-fast-forward";
            } else if (results[i].result == RECVBADREF) {
                reason = "bad ref";
            }
            u8csc rs = {(u8cp)reason, (u8cp)reason + strlen(reason)};
            u8bFeed(line, rs);
            u8bFeed1(line, '\n');
        }
        a_dup(u8c, payload, u8bData(line));
        call(PKTu8sFeed, u8bIdle(frame), payload);
    }

    call(PKTu8sFeedFlush, u8bIdle(frame));

    a_dup(u8c, fdata, u8bData(frame));
    ok64 wr = FILEFeedAll(out_fd, fdata);
    //  POST-010: a no-op push (`n == 0` — peer was already at our tip, so
    //  the client short-circuited, sent only a flush, and closed its read
    //  end) leaves no client reading this `unpack ok` reply.  Our write
    //  then races that early close and trips EPIPE → FILEFAIL, surfacing a
    //  stray `Error: FILEFAIL` even though the push reported success.  With
    //  zero ref updates to report there is nothing the client needs from
    //  this reply, so a broken pipe here is a benign no-op — return OK.  A
    //  real ref-update response (`n > 0`) failing to reach the client is a
    //  genuine error and still surfaces as FILEFAIL.
    if (wr != OK && n == 0 && errno == EPIPE) return OK;
    return wr;
}

// --- colocated primary-wt advance ---
//
//  After REFS moves on an FF push, a colocated primary wt — one whose
//  own `<wt>/.be/` directory IS the store we just received into — must
//  follow, otherwise the user's on-disk tree silently lags REFS.  We
//  fork+exec `be get ?` with cwd at the wt root; conflicts or dirty
//  files cause `be get` to refuse, which we warn about (the wire push
//  already committed; the wt-advance is a courtesy).
//
//  POST-014: the ONLY safe wt root is `h->wt` AND only when `h->wt` is
//  a genuine worktree (`<h->wt>/.be` is the store DIR) rather than the
//  store dir itself.  A CENTRAL store (`~/.be`) opened directly by its
//  path resolves `h->wt == h->root == <store>/.be` — the store dir, not
//  a worktree.  The old `dirname(dirname(<shard>))` derivation produced
//  `$HOME` there, and then a stat of `$HOME/.be/wtlog` (a stray or
//  legacy top-level wtlog) wrongly passed the "is a primary wt" probe —
//  the HOME-escape.  `be get ?` then ran with cwd=$HOME, advancing the
//  wrong tree (or failing 157 on a multi-project store), while the real
//  secondary worktree elsewhere was stranded.  There is no reverse map
//  from a central store to its secondary worktrees, so those are
//  correctly skipped: we only advance a wt whose own `.be` is this
//  store.
//
//  Split into two phases because `be get ?` needs the project store
//  lock that the receive-pack process is still holding mid-RECVServe:
//
//    1. RECVCaptureWtPath() — runs while KEEP is open; validates the
//       colocated layout off `k->h` and stashes the wt root into a
//       process-static slot.
//    2. RECVAdvanceColocatedWt() — runs from the receive-pack driver
//       AFTER KEEPClose releases the lock; consumes the slot and
//       fork+exec's `be get ?`.

static char recv_wt_path[1024];
static b8   recv_wt_path_set = NO;

//  YES iff `<wt>/.be` is the SAME directory as `<store>/.be/<proj>`'s
//  parent — i.e. the project shard lives directly under the worktree's
//  own `.be`.  This is the colocated invariant; a central store (whose
//  `h->wt` IS `<store>/.be`) fails it because its derived `<wt>` differs
//  from `h->wt`.  Compares NUL-terminated path strings byte-for-byte
//  (both come from the same PATH composer, so they are already
//  canonical — no symlink resolution needed for the same-process case).
static b8 recv_wt_is_colocated(path8s wt, path8s store_be_parent) {
    a_dup(u8c, a, wt);
    a_dup(u8c, b, store_be_parent);
    return u8csEq(a, b);
}

void RECVCaptureWtPath(void) {
    recv_wt_path_set = NO;
    keeper *k = &KEEP;
    if (k->h == NULL || u8bEmpty(k->h->wt)) return;

    a_path(keepdir);
    if (HOMEBranchDir(k->h, keepdir, NULL) != OK) return;

    //  keepdir = `<wt>/.be/<proj>/`.  Two pops → the derived wt root.
    if (PATHu8bPop(keepdir) != OK) return;   // → <wt>/.be
    if (PATHu8bPop(keepdir) != OK) return;   // → <wt>

    //  POST-014 gate: the derived wt root MUST equal `h->wt`, the home's
    //  own worktree root.  For a colocated push they coincide (`<wt>`);
    //  for a central store `h->wt` is the store dir (`<store>/.be`) while
    //  the two-pop derivation yields its parent — so they differ and we
    //  skip rather than escape to $HOME.  This is independent of any
    //  stray `<parent>/.be/wtlog`, which is what made the old probe lie.
    a_path(hwt);
    a_dup(u8c, hwt_s, u8bDataC(k->h->wt));
    if (PATHu8bFeed(hwt, hwt_s) != OK) return;
    if (!recv_wt_is_colocated($path(hwt), $path(keepdir))) return;

    //  Belt-and-suspenders: `<wt>/.be/wtlog` must be a regular file (the
    //  primary-wt anchor).  A dir there means a bare store, not a wt.
    a_path(anchor);
    if (PATHu8bFeed(anchor, $path(keepdir)) != OK) return;
    if (PATHu8bPush(anchor, DOG_BE_S) != OK) return;
    if (PATHu8bPush(anchor, DOG_WTLOG_S) != OK) return;
    struct stat st = {};
    if (stat((char const *)anchor[0], &st) != 0) return;
    if (!S_ISREG(st.st_mode)) return;

    //  Stash the wt root into our static slot for the post-KEEPClose
    //  phase.  Truncate (with a warning) rather than overrun.
    char const *src = (char const *)keepdir[0];
    size_t n = strlen(src);
    if (n >= sizeof(recv_wt_path)) {
        fprintf(stderr,
                "keeper: recv: wt path too long (%zu) for advance slot\n",
                n);
        return;
    }
    memcpy(recv_wt_path, src, n + 1);
    recv_wt_path_set = YES;
}

void RECVAdvanceColocatedWt(void) {
    if (!recv_wt_path_set) return;

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr,
                "keeper: recv: fork failed for wt-advance: %s\n",
                strerror(errno));
        return;
    }
    if (pid == 0) {
        if (chdir(recv_wt_path) != 0) {
            fprintf(stderr,
                    "keeper: recv: chdir(%s) failed: %s\n",
                    recv_wt_path, strerror(errno));
            _exit(127);
        }
        //  POST-014: route ALL three of the child's std streams to
        //  /dev/null.  A failing `be get ?` (dirty wt / conflict) prints
        //  raw `Error: SNIFFFAIL` / `Error: BEDOGEXIT` to stderr — which,
        //  inherited onto the push's stderr, masquerades as a fatal push
        //  failure even though the wire push already committed.  We
        //  instead emit ONE honest, prefixed deferral line from the
        //  parent (below) that affirms the push and points at the manual
        //  recovery.  The detailed conflict report is one `be get ?`
        //  away in the wt.
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) {
            dup2(dn, STDIN_FILENO);
            dup2(dn, STDOUT_FILENO);
            dup2(dn, STDERR_FILENO);
            if (dn > STDERR_FILENO) close(dn);
        }
        execlp("be", "be", "get", "?", (char *)NULL);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        //  Honest report: the push SUCCEEDED (REFS advanced); only the
        //  courtesy colocated wt-advance was deferred.  Phrase it as a
        //  deferral, not a failure, and never alter the push exit code —
        //  this fn is void and its caller's `done;` returns OK.
        fprintf(stderr,
                "keeper: recv: push OK — colocated wt-advance deferred "
                "(`be get ?` in %s returned %d; the wt lags REFS, run "
                "`be get ?` there to sync)\n",
                recv_wt_path,
                WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    }
}

// --- top-level orchestration ---

ok64 RECVServe(int in_fd, int out_fd, refadvcp adv) {
    sane(in_fd >= 0 && out_fd >= 0);
    FILEIgnoreSIGPIPE();  //  a client closing early must not kill us

    recv_req req = {};
    //  Pre-acquire the three req buffers from BASS in this scope.  Going
    //  through `call(u8bAcquire, …)` would snapshot+rewind BASS and
    //  immediately undo the acquire (see PRO.h's a_carve note); invoke
    //  u8bAcquire directly and propagate the error manually.
    __ = u8bAcquire(ABC_BASS, req.upds_b,
                    RECV_MAX_UPDATES * sizeof(recv_update));
    if (__ != OK) return __;
    __ = u8bAcquire(ABC_BASS, req.arena, RECV_ARENA_BYTES);
    if (__ != OK) return __;
    __ = u8bAcquire(ABC_BASS, req.tail, RECV_REQ_BUF);
    if (__ != OK) return __;

    ok64 rro = RECVReadRequest(in_fd, &req);
    if (rro != OK) {
        //  No request → no response; surface error to caller.
        return rro;
    }

    //  Drain the packfile (may be empty for delete-only requests, which
    //  we currently refuse — but git still sends a 32-byte empty pack
    //  for those, so consume it regardless).
    ok64 unpack_status = OK;
    if (req.count > 0) {
        u8csc rtail = {u8bDataHead(req.tail), u8bIdleHead(req.tail)};
        ok64 io = RECVIngestPack(in_fd, rtail);
        if (io != OK) unpack_status = io;
    }

    //  Apply updates regardless of pack status — RECVEmitResponse will
    //  override per-ref status with "unpacker failed" when the unpack
    //  itself failed.  Even on unpack failure, we still want to walk
    //  updates so each ref gets its `ng` line on the wire.
    recv_result results[RECV_MAX_UPDATES] = {};
    u32 nres = 0;
    if (req.count > 0) {
        ok64 ao = RECVApplyUpdates(adv, &req, results,
                                   RECV_MAX_UPDATES, &nres);
        if (ao != OK && unpack_status == OK) unpack_status = ao;
    }

    //  If at least one ref was accepted, capture the colocated wt
    //  path now (while KEEP is still open) so the receive-pack
    //  driver can advance the wt after closing the store lock.
    if (unpack_status == OK) {
        for (u32 i = 0; i < nres; i++) {
            if (results[i].result == OK) {
                RECVCaptureWtPath();
                break;
            }
        }
    }

    return RECVEmitResponse(out_fd, unpack_status, results, nres);
}

