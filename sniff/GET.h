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

#endif
