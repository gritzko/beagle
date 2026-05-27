#ifndef KEEPER_PROJ_H
#define KEEPER_PROJ_H

//  PROJ: keeper-owned view projectors (VERBS.md §"View projectors").
//
//  Each handler takes a pre-parsed URI whose `scheme` matches the
//  projector and emits a formatted view to stdout.  When `tlv` is YES
//  the bytes go through dog/HUNK as a TLV record (consumed by `bro`);
//  otherwise plain text / raw bytes are written.
//
//  Wired through KEEPProjDispatch, which KEEP.exe.c invokes when the
//  CLI parsed a verb-less arg and DOG_PROJECTORS routes its scheme to
//  "keeper".

#include "KEEP.h"

con ok64 PROJFAIL = 0x65b6133ca495;
con ok64 PROJNONE = 0x65b6135d85ce;

//  tree:[<path>]?<ref|sha>  — list one directory's entries
//  (mode, type, sha, name).  Non-recursive.
ok64 KEEPProjTree(uricp u, b8 tlv);

//  commit:?<ref|sha>  — render a commit object: header lines
//  (commit/tree/parents/author/committer) + message body.
ok64 KEEPProjCommit(uricp u, b8 tlv);

//  blob:[<path>]?<ref|sha>  — emit blob bytes.  In TLV mode, the bytes
//  are tokenized via dog/TOK using the URI path's extension and packed
//  into a hunk so `bro` can render syntax highlighting.
ok64 KEEPProjBlob(uricp u, b8 tlv);

//  sha1:[<path>]?<ref|sha>  — emit 40-hex SHA-1 of the resource +
//  newline.  Resource shape:
//    sha1:?<ref>          tip sha of <ref> (commit on the branch)
//    sha1:?#<hex>         resolved sha for the (possibly short) hex
//    sha1:?               cur's tip (trunk if cur is trunk)
//    sha1:<path>?<ref>    blob sha of <path> at <ref>'s tree
//  Path-without-ref / empty-ref forms (sniff territory: wt on-disk
//  bytes vs tracked blob) are NOT handled here — keeper has no wt;
//  caller should route those through sniff.
ok64 KEEPProjSha1(uricp u, b8 tlv);

//  Dispatch on `u->scheme`.  Returns PROJNONE for schemes the keeper
//  table claims but no handler is wired (helpful diagnostic).
ok64 KEEPProjDispatch(uricp u, b8 tlv);

#endif
