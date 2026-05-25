#ifndef SNIFF_GET_H
#define SNIFF_GET_H

//  GET: checkout a commit tree from keeper into the worktree.
//
//  Skips unchanged files (hashlet match), protects dirty files
//  (worktree modified), creates symlinks for mode 120000,
//  skips submodules (mode 160000).  Records SNIFF_BLOB +
//  SNIFF_CHECKOUT for every written file, SNIFF_TREE for every dir.

#include "SNIFF.h"
#include "keeper/KEEP.h"

//  Checkout a commit from keeper into the worktree.
//  hex: commit SHA hex prefix (6-15 chars).
//  source: URI we checked out (recorded in keeper REFS as
//          file:///reporoot → source).  Empty to skip recording.
ok64 GETCheckout(u8cs reporoot, u8csc hex,
                 u8csc source);

//  URI dispatch: parses one `be get` URI shape into the right
//  checkout / overlay action.
//
//    path+query  (no authority)  — single-file or subtree overlay
//                                  from another branch's tip
//                                  (no `.be/wtlog` row).
//    path only                   — `be get <hex>` (commit checkout
//                                  by sha prefix).
//    query / authority           — REFSResolve over the canonicalised
//                                  URI; falls back to local trunk on
//                                  miss.  Resolves relative refs in
//                                  place; refuses on missing branch
//                                  (use `be post ?./X` to create).
ok64 SNIFFGetURI(u8cs reporoot, uri *u);

//  Bare `be get` (no URI args): print every local branch tip from
//  keeper REFS (current branch starred) plus every remote-tracking
//  ref.  No wt mutation.
ok64 SNIFFGetSummary(u8cs reporoot);

//  `be checkout <hex>` — thin wrapper over GETCheckout that builds
//  the source URI as `?<hex>`.
ok64 SNIFFCheckout(u8cs reporoot, u8cs hex);

#endif
